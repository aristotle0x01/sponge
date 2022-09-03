#include "tcp_receiver.hh"

#include "tcp_segment.hh"

// implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    uint32_t seq_no = seg.header().seqno.raw_value();
    if (seg.header().syn) {
        _syn = std::make_tuple(seq_no, 0, true);
    }
    if (!std::get<2>(_syn)) {
        return;
    }

    uint64_t checkpoint = stream_out().bytes_written();
    uint64_t abs_seq_no = unwrap(seg.header().seqno, WrappingInt32(std::get<0>(_syn)), checkpoint);
    uint64_t next_valid_seq_no = ackno().has_value() ? unwrap(ackno().value(), WrappingInt32(std::get<0>(_syn)), checkpoint):0;
    size_t tw = window_size()==0 ? 0 : window_size()-1;
    // discard segments out of current wnd range
    if(abs_seq_no > (next_valid_seq_no+tw)){
        return;
    }

    bool fin = false;
    if (seg.header().fin) {
        fin = true;
        uint64_t t = (seq_no + seg.length_in_sequence_space() - 1) % (1ll << 32);
        uint32_t fin_seq = static_cast<uint32_t>(t);
        uint64_t fin_abs_seq = unwrap(WrappingInt32(fin_seq), WrappingInt32(std::get<0>(_syn)), checkpoint);
        _fin = std::make_tuple(fin_seq, fin_abs_seq, true);
    }

    string data = std::string();
    if (seg.payload().size() > 0) {
        data = std::string(seg.payload().str());
    }

    uint64_t stream_index;
    if (seg.header().syn) {
        stream_index = 0;
    } else if (fin and seg.payload().size() == 0) {
        if (0 == checkpoint) {
            stream_index = 0;
        } else {
            if (abs_seq_no < 2) {
                return;
            }
            stream_index = abs_seq_no - 2;
        }
    } else {
        // abs index here should't be zero
        if (0 == abs_seq_no) {
            return;
        }
        stream_index = abs_seq_no - 1;
    }

    if (seg.payload().size() > 0 or fin) {
        _reassembler.push_substring(data, stream_index, fin);
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!std::get<2>(_syn)) {
        return std::nullopt;
    }

    size_t next_stream_index = stream_out().bytes_written();
    uint64_t next_abs_index = next_stream_index + 1;
    if (std::get<2>(_fin) and std::get<1>(_fin) == (next_abs_index)) {
        return wrap(next_abs_index + 1, WrappingInt32(std::get<0>(_syn)));
    } else {
        return wrap(next_abs_index, WrappingInt32(std::get<0>(_syn)));
    }
}

size_t TCPReceiver::window_size() const { return stream_out().remaining_capacity(); }
