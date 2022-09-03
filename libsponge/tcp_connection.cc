#include "tcp_connection.hh"

#include <iostream>

// implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { 
    return _sender.stream_in().remaining_capacity(); 
}

size_t TCPConnection::bytes_in_flight() const { 
    return _sender.bytes_in_flight(); 
}

size_t TCPConnection::unassembled_bytes() const {
    return _receiver.unassembled_bytes();
}

size_t TCPConnection::time_since_last_segment_received() const {
    return _total_tick - _last_recv_seg_tick;
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    _last_recv_seg_tick = _total_tick;

    _receiver.segment_received(seg);
    if(seg.header().syn and 0 == _sender.next_seqno_absolute()){
        // listening side response
        write(std::string());
        _syn_sent_or_recv = true;

        return;
    }
    if(not _syn_sent_or_recv){
        return;
    }

    if(seg.header().rst){
        _active = false;
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
    }

    if(seg.header().fin){
        _fin_received = true;
        if (!_fin_sent){
            _linger_after_streams_finish = false;
        }
    }

    if(seg.header().ack){
        _sender.ack_received(seg.header().ackno, seg.header().win);
        write(std::string());
    }

    if(seg.length_in_sequence_space()){
        // send something in reply
        _sender.send_empty_segment();
        write(std::string());
    }
    if (_receiver.ackno().has_value() and (seg.length_in_sequence_space() == 0)
        and seg.header().seqno == _receiver.ackno().value() - 1) {
        _sender.send_empty_segment();
        write(std::string());
    }

    check_active();
}

void TCPConnection::send_reset() {
    _sender.send_empty_segment();
    TCPSegment& seg = _sender.segments_out().back();
    seg.header().rst = true;
    write(std::string());
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _active = false;
}

void TCPConnection::check_active(){
    if(not _active){
        return;
    }

    bool r = (0 == _receiver.unassembled_bytes()) and _receiver.stream_out().input_ended();
    bool s = _fin_sent and (TCPSenderStateSummary::FIN_ACKED == TCPState::state_summary(_sender));
    if (not (r and s)){
        return;
    }
    if (_linger_after_streams_finish){
        // active close
        if(time_since_last_segment_received() >= 10*_cfg.rt_timeout){
            _active = false;
        }
    } else{
        // passive close
        if (_fin_received){
            _active = false;
        }
    }
}

bool TCPConnection::active() const { 
    return _active;
}

size_t TCPConnection::write(const string &data) {
    if(!active()){
        return 0;
    }

    size_t written = _sender.stream_in().write(data);
    _sender.fill_window();
    while(!_sender.segments_out().empty()){
        TCPSegment& seg = _sender.segments_out().front();
        if(_receiver.ackno().has_value()){
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = _receiver.window_size();
        }
        _segments_out.push(seg);
        if (seg.header().fin){
            _fin_sent = true;
        }
        _sender.segments_out().pop();
    }

    check_active();

    return written;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    if (not active()){
        return;
    }

    if (_sender.consecutive_retransmissions() >= TCPConfig::MAX_RETX_ATTEMPTS){
        send_reset();
        return;
    }
    
    size_t l_old = _sender.segments_out().size();
    _total_tick = _total_tick + ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    size_t l_new = _sender.segments_out().size();
    // having segments expired
    if (l_new > l_old){
        write(std::string());
    }

    check_active();
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    write(std::string());
}

void TCPConnection::connect() {
    _sender.fill_window();
    while(!_sender.segments_out().empty()){
        TCPSegment& seg = _sender.segments_out().front();
        _segments_out.push(seg);
        _sender.segments_out().pop();
        _syn_sent_or_recv = true;
    }
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            send_reset();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
