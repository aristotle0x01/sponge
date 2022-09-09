#include "stream_reassembler.hh"

#include <iostream>

// implementation of a stream reassembler.

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
    while (i < data.length() and (index + i) < (_next_stream_index + _output.remaining_capacity())) {
        if ((index + i) < _next_stream_index) {
            i++;
            continue;
        }
        // marker always starts with _next_stream_index
        int r_index = (index + i) % _capacity;
        if (!_reassemble_marker[r_index]) {
            _reassemble_count++;
        }
        _reassemble_marker[r_index] = true;
        _buffer[r_index] = data[i];
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
    size_t r_index = (_next_stream_index + i) % _capacity;
    string s;
    while (_reassemble_marker[r_index] and i < remaining_capacity) {
        s.push_back(_buffer[r_index]);
        _reassemble_marker[r_index] = false;
        r_index = (_next_stream_index + (++i)) % _capacity;
    }
    if (s.length() > 0) {
        _output.write(s);
        _next_stream_index = _next_stream_index + i;
        _reassemble_count = _reassemble_count - i;
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _reassemble_count; }

bool StreamReassembler::empty() const { return _reassemble_count == 0; }
