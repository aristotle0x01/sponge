#include "stream_reassembler.hh"

#include <cstring>
#include <iostream>

// implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _next_stream_index(0)
    , _ending_index(0)
    , _reassemble_count(0)
    , _ended(false)
    , _output(capacity)
    , _capacity(capacity) {
    _reassemble_marker = new bool[capacity];
    memset(_reassemble_marker, false, capacity);
    _buffer = new char[capacity];
    memset(_buffer, 0, capacity);
}

StreamReassembler::~StreamReassembler() {
    delete[] _buffer;
    delete[] _reassemble_marker;
}
//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const uint64_t index, const bool eof) {
    // only when _next_stream_index surpassing _ending_index will stream be ended
    if (eof) {
        _ending_index = index + data.length();
        _ended = true;
    }

    // always put into reassemble buffer first, then reassemble to in-order bytes
    // data exceed capacity will be discarded, reserve earlier bytes first
    if (data.length() > 0) {
        size_t i = 0;
        uint64_t r_index = _next_stream_index >= index ? _next_stream_index : index;
        uint64_t write_start_index = r_index;
        uint64_t tail_stream_index = min(_next_stream_index + _output.remaining_capacity(), index + data.length());
        while (r_index < tail_stream_index) {
            i = r_index % _capacity;
            if (!_reassemble_marker[i]) {
                _reassemble_count++;
                _reassemble_marker[i] = true;
            }
            r_index++;
        }
        if (tail_stream_index > write_start_index) {
            size_t count = tail_stream_index - write_start_index;
            size_t d_index, begin_index;
            if (index >= write_start_index) {
                d_index = 0;
                begin_index = index % _capacity;
            } else {
                d_index = write_start_index - index;
                begin_index = write_start_index % _capacity;
            }
            if (count <= _capacity - begin_index) {
                memcpy(&_buffer[begin_index], &data[d_index], count);
            } else {
                size_t size_1 = _capacity - begin_index;
                memcpy(&_buffer[begin_index], &data[d_index], size_1);
                size_t size_2 = count - size_1;
                memcpy(&_buffer[(begin_index + size_1) % _capacity], &data[d_index + size_1], size_2);
            }
        }
    }

    reassemble();

    if (_ended and _next_stream_index >= _ending_index) {
        _output.end_input();
    }
}

void StreamReassembler::reassemble() {
    size_t remaining_capacity = _output.remaining_capacity();

    size_t count = 0;
    size_t r_index = (_next_stream_index + count) % _capacity;
    while (_reassemble_marker[r_index] and count < remaining_capacity) {
        _reassemble_marker[r_index] = false;
        count++;
        r_index++;
        if (r_index == _capacity) {
            r_index = 0;
        }
    }
    if (count > 0) {
        string r(count, 0);
        size_t begin_index = _next_stream_index % _capacity;
        if (count <= _capacity - begin_index) {
            memcpy(&r[0], &_buffer[begin_index], count);
        } else {
            size_t size_1 = _capacity - begin_index;
            memcpy(&r[0], &_buffer[begin_index], size_1);
            size_t size_2 = count - size_1;
            memcpy(&r[size_1], &_buffer[0], size_2);
        }

        _output.write(r);
        _next_stream_index = _next_stream_index + count;
        _reassemble_count = _reassemble_count - count;
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _reassemble_count; }

bool StreamReassembler::empty() const { return _reassemble_count == 0; }
