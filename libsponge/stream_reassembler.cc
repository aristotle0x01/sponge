#include "stream_reassembler.hh"

#include <iostream>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _next_stream_index(0)
    , _ending_index(0)
    , _reassemble_marker(capacity)
    , _buffer(capacity)
    , _reassemble_count(0)
    , _ended(false)
    , _output(capacity)
    , _capacity(capacity) {}

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
    size_t i = 0;
    // data exceed capacity will be discarded, reserve earlier bytes first
    while (i < data.length() and (index + i) < (_next_stream_index + _capacity)) {
        if ((index + i) < _next_stream_index) {
            i++;
            continue;
        }
        // marker always starts with _next_stream_index
        int set_index = (index + i) - _next_stream_index;
        if (!_reassemble_marker[set_index]) {
            _reassemble_count++;
        }
        _reassemble_marker[set_index] = true;
        _buffer[set_index] = data[i];
        i++;
    }

    reassemble();

    if (_ended and _next_stream_index >= _ending_index) {
        _output.end_input();
    }
}

void StreamReassembler::reassemble() {
    size_t remaining_capacity = _output.remaining_capacity();

    size_t i = 0;
    string s;
    while (_reassemble_marker[i] and i < remaining_capacity) {
        s.push_back(_buffer[i]);
        _reassemble_marker[i] = false;
        i++;
        _reassemble_count--;
    }
    if (s.length() > 0) {
        _output.write(s);
        _next_stream_index = _next_stream_index + i;
        size_t j = i;
        for (; j < _capacity; j++) {
            // shift to make _reassemble_marker starts with _next_stream_index
            _reassemble_marker[j - i] = _reassemble_marker[j];
            _buffer[j - i] = _buffer[j];
            _reassemble_marker[j] = false;
        }
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _reassemble_count; }

bool StreamReassembler::empty() const { return _reassemble_count == 0; }
