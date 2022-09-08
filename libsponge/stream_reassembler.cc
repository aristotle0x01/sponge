#include "stream_reassembler.hh"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <list>
#include <sstream>

// implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

using namespace std;
using std::hex;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _next_stream_index(0)
    , _left_most_index(0)
    , _ending_index(0)
    , _reassemble_count(0)
    , _ended(false)
    , _capacity(capacity)
    , _reassemble_marker(capacity, false)
    , _output(capacity) {}

string StreamReassembler::tohex(const string &data) {
    std::stringstream ss;
    for (const auto &c : data)
        ss << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(static_cast<unsigned char>(c)) << ' ';
    return ss.str();
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

    if (!data.empty() and _next_stream_index > _left_most_index) {
        bool b1 = (index + data.length() - 1) >= (_left_most_index + _capacity);
        bool b2 = (index + data.length() - 1) >= (_next_stream_index + _output.remaining_capacity());
        if (b1 or b2) {
            for (size_t j = _next_stream_index; j < (_left_most_index + _capacity); j++) {
                // shift _reassemble_marker to start with _left_most_index
                _reassemble_marker[j - _next_stream_index] = _reassemble_marker[j - _left_most_index];
                _reassemble_marker[j - _left_most_index] = false;
            }
            _left_most_index = _next_stream_index;
        }
    }

    if (!data.empty()) {
        auto search = _buffer.find(index);
        if (search != _buffer.end()) {
            string old = search->second.second;
            if (data.length() > old.length()) {
                _buffer[index] = {index + data.length() - 1, data};
            }
        } else {
            _buffer[index] = {index + data.length() - 1, data};
        }

        // always put into reassemble buffer first, then reassemble to in-order bytes
        // data exceed capacity will be discarded, reserve earlier bytes first
        size_t i = 0;
        size_t avail_cap = _output.remaining_capacity();
        while (i < data.length() and (index + i) < (_left_most_index + _capacity) and
               (index + i) < (_next_stream_index + avail_cap)) {
            if ((index + i) >= _next_stream_index) {
                // marker always starts with _left_most_index
                int r_index = (index + i) - _left_most_index;
                if (!_reassemble_marker[r_index]) {
                    _reassemble_marker[r_index] = true;
                    _reassemble_count++;
                }
            }
            i++;
        }
    }

    reassemble();

    if (_ended and _next_stream_index >= _ending_index) {
        _output.end_input();
    }
}

void StreamReassembler::reassemble() {
    size_t avail_cap = _output.remaining_capacity();
    size_t reassembled = 0;
    for (size_t j = _next_stream_index; j < (_next_stream_index + avail_cap) and j < (_left_most_index + _capacity);
         j++) {
        if (not _reassemble_marker[j - _left_most_index]) {
            break;
        }
        _reassemble_marker[j - _left_most_index] = false;
        reassembled++;
    }
    if (0 == reassembled) {
        return;
    }

    std::list<uint64_t> list;
    string s(reassembled, '0');
    uint64_t end_index = _next_stream_index + reassembled - 1;
    for (auto it = _buffer.begin(); it != _buffer.end(); ++it) {
        auto left = it->first;
        auto right = it->second.first;
        string data = it->second.second;

        uint64_t stream_index = left;

        auto list_it = std::find(list.begin(), list.end(), left);
        if (list_it != list.end()) {
            continue;
        }
        if (_next_stream_index > right) {
            list.push_back(left);
            continue;
        }

        size_t tmp = 0;
        while (stream_index <= end_index and tmp < data.length()) {
            if (stream_index >= _next_stream_index) {
                s[stream_index - _next_stream_index] = data[stream_index - left];
            }
            stream_index++;
            tmp++;
        }

        if (tmp == data.length()) {
            list.push_back(left);
        } else {
            break;
        }
    }

    if (reassembled) {
        _output.write(s);
        _next_stream_index = _next_stream_index + reassembled;
        _reassemble_count = _reassemble_count - reassembled;
    }
    for (uint64_t n : list) {
        auto it = _buffer.find(n);
        if (it != _buffer.end()) {
            _buffer.erase(n);
        }
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _reassemble_count; }

bool StreamReassembler::empty() const { return _reassemble_count == 0; }
