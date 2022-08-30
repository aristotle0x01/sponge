#include "tcp_sender.hh"

#include "tcp_config.hh"
#include "tcp_state.hh"

#include <algorithm>
#include <list>
#include <random>

// implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _stream(capacity)
    , _timer{retx_timeout}
    , _initial_retransmission_timeout{retx_timeout}
    , _retransmission_timeout{retx_timeout}
    , _ms_total_tick{0}
    , _consecutive_retransmissions{0} {}

size_t TCPSender::bytes_in_flight() const {
    size_t bytes = 0;
    for (auto it = _outstanding.begin(); it != _outstanding.end(); ++it) {
        bytes = bytes + it->second.length_in_sequence_space();
    }
    return bytes;
}

void TCPSender::fill_window() {
    std::string state = TCPState::state_summary(*this);
    if (state == TCPSenderStateSummary::CLOSED) {
        TCPSegment seg = build_segment(std::string(), true, false, _isn);
        _segments_out.push(seg);
        _outstanding[_next_abs_seq_no] = seg;

        _next_abs_seq_no = _next_abs_seq_no + seg.length_in_sequence_space();
        _timer.start(_ms_total_tick, _retransmission_timeout);
    } else if (state == TCPSenderStateSummary::SYN_ACKED) {
        bool fin = false;
        while (!_stream.buffer_empty() and _next_abs_seq_no <= _wnd_right_abs_no) {
            size_t gap = _wnd_right_abs_no - _next_abs_seq_no + 1;
            size_t readable = std::min({TCPConfig::MAX_PAYLOAD_SIZE, gap, _stream.buffer_size()});
            if (_stream.input_ended() and (_next_abs_seq_no + readable) <= _wnd_right_abs_no) {
                fin = true;
            }
            std::string data = _stream.read(readable);
            TCPSegment seg = build_segment(data, false, fin, wrap(_next_abs_seq_no, _isn));
            _segments_out.push(seg);
            _outstanding[_next_abs_seq_no] = seg;

            _next_abs_seq_no = _next_abs_seq_no + seg.length_in_sequence_space();
            _timer.start(_ms_total_tick, _retransmission_timeout);
        }
        if (fin == false and _stream.buffer_empty() and _stream.input_ended() and _next_abs_seq_no <= _wnd_right_abs_no) {
            TCPSegment seg = build_segment(std::string(), false, true, wrap(_next_abs_seq_no, _isn));
            _segments_out.push(seg);
            _outstanding[_next_abs_seq_no] = seg;

            _next_abs_seq_no = _next_abs_seq_no + seg.length_in_sequence_space();
            _timer.start(_ms_total_tick, _retransmission_timeout);
        }
    } else {
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t abs_ack_no = unwrap(ackno, _isn, _check_point);
    if (abs_ack_no > _next_abs_seq_no or abs_ack_no < _wnd_left_abs_no) {
        // Impossible ackno (beyond next seqno) is ignored or repeated ack
        return;
    }

    std::list<uint64_t> llist;
    for (auto it = _outstanding.begin(); it != _outstanding.end(); ++it) {
        // What if an acknowledgment only partially acknowledges some outstanding segment?
        // in test case 17 there are repeated acks that may partially acknowledge
        if ((it->first + it->second.length_in_sequence_space()-1) < abs_ack_no) {
            llist.push_back(it->first);
        }
    }
    for (uint64_t n : llist) {
        _outstanding.erase(n);
    }
    if (_outstanding.empty()) {
        _timer.stop();
    }

    uint64_t last_abs_ack_no = unwrap(_last_ack_no, _isn, _check_point);
    if (abs_ack_no > last_abs_ack_no) {
        _retransmission_timeout = _initial_retransmission_timeout;
        _consecutive_retransmissions = 0;
        if (!_outstanding.empty()) {
            _timer.restart(_ms_total_tick, _retransmission_timeout);
        }
    }
    
    // What should I do if the window size is zero? If the receiver has announced a
    // window size of zero, the fill window method should act like the window size is one.
    // When filling window, treat a '0' window size as equal to '1' but don't back off RTO
    // so when _window_size == 0, then (_wnd_right_abs_no-_wnd_left_abs_no+1)==1
    _window_size = window_size;
    _last_ack_no = ackno;
    _wnd_left_abs_no = unwrap(_last_ack_no, _isn, _check_point);
    _wnd_right_abs_no = _wnd_left_abs_no + (_window_size == 0 ? 1:_window_size) - 1;
    if (_wnd_left_abs_no > _check_point and _window_size > 0) {
        _check_point = _wnd_left_abs_no - 1;
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _ms_total_tick = _ms_total_tick + ms_since_last_tick;

    bool expired = _timer.expire(_ms_total_tick);

    if (_outstanding.empty()) {
        _timer.stop();
    }

    if (expired and !_outstanding.empty()) {
        auto it = _outstanding.begin();
        _segments_out.push(it->second);
        if (_window_size > 0) {
            _retransmission_timeout = _retransmission_timeout * 2;
            _consecutive_retransmissions++;
        }
        _timer.restart(_ms_total_tick, _retransmission_timeout);
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment seg = build_segment(std::string(), false, false, wrap(_next_abs_seq_no, _isn));
    _segments_out.push(seg);
}

TCPSegment TCPSender::build_segment(const string &data, bool syn, bool fin, WrappingInt32 seqno) const {
    TCPSegment seg;
    seg.payload() = std::string(data);
    seg.header().fin = fin;
    seg.header().syn = syn;
    seg.header().seqno = seqno;
    return seg;
}