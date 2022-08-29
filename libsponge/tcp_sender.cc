#include "tcp_sender.hh"

#include "tcp_config.hh"
#include "tcp_state.hh"

#include <algorithm>
#include <iostream>
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
    , _initial_retransmission_timeout{retx_timeout}
    , _retransmission_timeout{retx_timeout}
    , _ms_total_tick{0}
    , _timer{retx_timeout}
    , _consecutive_retransmissions{0}
    , _stream(capacity) {}

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
        _outstanding[_next_seqno] = seg;

        std::cout << "sent:" << _next_seqno << '-' << seg.length_in_sequence_space() << '-' << _window_size << '\n';

        _next_seqno = _next_seqno + seg.length_in_sequence_space();
        // if (_window_size >= seg.length_in_sequence_space()){
        //     _window_size = _window_size - seg.length_in_sequence_space();
        // } else {
        //     _window_size = 0;
        // }

        _timer.start(_ms_total_tick, _retransmission_timeout);
    } else if (state == TCPSenderStateSummary::SYN_ACKED) {
        bool fin = false;
        while (!_stream.buffer_empty() and _window_size > 0 and _next_seqno <= _last_wnd_no) {
            size_t gap = _last_wnd_no - _next_seqno + 1;
            size_t readable = min(TCPConfig::MAX_PAYLOAD_SIZE, static_cast<size_t>(_window_size));
            std::cout << "sent0:" << gap << '-' << _stream.buffer_size() << '-' << readable << '\n';
            readable = min(readable, gap);
            if (_stream.input_ended()) {
                if ((_next_seqno + _stream.buffer_size() - 1) < _last_wnd_no) {
                    fin = true;
                    // readable = readable - 1;
                }
            }
            std::string data = _stream.read(readable);
            TCPSegment seg = build_segment(data, false, fin, wrap(_next_seqno, _isn));
            _segments_out.push(seg);
            _outstanding[_next_seqno] = seg;

            std::cout << "sent:" << _next_seqno << '-' << gap << '-' << seg.length_in_sequence_space() << '-'
                      << _window_size << '\n';

            _next_seqno = _next_seqno + seg.length_in_sequence_space();
            if (_window_size >= seg.length_in_sequence_space()) {
                _window_size = _window_size - seg.length_in_sequence_space();
            } else {
                _window_size = 0;
            }

            _timer.start(_ms_total_tick, _retransmission_timeout);
        }
        if (fin == false and _stream.buffer_empty() and _stream.input_ended() and _window_size > 0 and
            _next_seqno <= _last_wnd_no) {
            TCPSegment seg = build_segment(std::string(), false, true, wrap(_next_seqno, _isn));
            _segments_out.push(seg);
            _outstanding[_next_seqno] = seg;

            std::cout << "sent:" << _next_seqno << '-' << seg.length_in_sequence_space() << '-' << _window_size << '\n';

            _next_seqno = _next_seqno + seg.length_in_sequence_space();
            if (_window_size >= seg.length_in_sequence_space()) {
                _window_size = _window_size - seg.length_in_sequence_space();
            } else {
                _window_size = 0;
            }

            _timer.start(_ms_total_tick, _retransmission_timeout);
        }
    } else {
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t abs_ack_no = unwrap(ackno, _isn, _check_point);
    if (abs_ack_no > _next_seqno) {
        // Impossible ackno (beyond next seqno) is ignored
        return;
    }
    // if (abs_ack_no <= _last_abs_ack_no){
    //     return;
    // }

    std::list<uint64_t> llist;
    for (auto it = _outstanding.begin(); it != _outstanding.end(); ++it) {
        if (it->first >= abs_ack_no) {
            break;
        }
        llist.push_back(it->first);
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

    _window_size = (0 == window_size ? 1 : window_size);
    _last_window_size = window_size;
    _last_ack_no = ackno;
    _last_abs_ack_no = unwrap(_last_ack_no, _isn, _check_point);
    _last_wnd_no = _last_abs_ack_no + _window_size - 1;
    if (_last_abs_ack_no > _check_point) {
        _check_point = _last_abs_ack_no - 1;
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
        std::cout << "resend:" << it->first << '\n';
        if (_last_window_size > 0) {
            _retransmission_timeout = _retransmission_timeout * 2;
            _consecutive_retransmissions++;
        }
        _timer.restart(_ms_total_tick, _retransmission_timeout);
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment seg = build_segment(std::string(), false, false, wrap(_next_seqno, _isn));
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