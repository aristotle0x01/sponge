#include "stream_reassembler.hh"

#include <cstring>
#include <iostream>
#include <list>

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
        _buffer.resize(capacity);
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const uint64_t index, const bool eof) {
    // std::cerr << "push_substring: " << index << "-" << data.length() << "-" << _output.remaining_capacity() << '\n';
    // only when _next_stream_index surpassing _ending_index will stream be ended
    if (eof) {
        _ending_index = index + data.length();
        _ended = true;
    }

    // always put into reassemble buffer first, then reassemble to in-order bytes
    // data exceed capacity will be discarded, reserve earlier bytes first
    while (!data.empty() and _output.remaining_capacity()) {
        // bytes already reassembled
        if ((index + data.length() - 1) < _next_stream_index) {
            break;
        }
        uint64_t tail_stream_index = min(_next_stream_index + _output.remaining_capacity(), index + data.length());
        uint64_t write_start_index = _next_stream_index >= index ? _next_stream_index : index;
        // data out of wnd
        if (index >= tail_stream_index or write_start_index >= tail_stream_index) {
            break;
        }

        uint64_t m_index = index;
        if (m_index < _next_stream_index) {
            m_index = _next_stream_index;
        }
        auto it = _marker_map.find(m_index);
        if (it != _marker_map.end() and tail_stream_index > _marker_map[m_index]) {
            _marker_map[m_index] = tail_stream_index;
        } else {
            _marker_map[m_index] = tail_stream_index;
        }

        // std::cerr << "push_substring1: " << _next_stream_index << "-" << m_index  << " -> " << tail_stream_index << '\n';

        size_t count = tail_stream_index - write_start_index;
        size_t d_index = write_start_index - index;
        size_t begin_index = write_start_index % _capacity;
        // std::cerr << "push_substring2: " << begin_index << ": " << write_start_index << " -> " << count << '\n';
        if (count <= _capacity - begin_index) {
            memcpy(&_buffer[begin_index], &data[d_index], count);
        } else {
            size_t size_1 = _capacity - begin_index;
            memcpy(&_buffer[begin_index], &data[d_index], size_1);
            size_t size_2 = count - size_1;
            memcpy(&_buffer[(begin_index + size_1) % _capacity], &data[d_index + size_1], size_2);
        }

        break;
    }

    reassemble();

    if (_ended and _next_stream_index >= _ending_index) {
        _output.end_input();
    }
}

void StreamReassembler::recount() {
    // std::cerr << "recount --------------------------" << '\n';

    std::list<uint64_t> list;
    uint64_t min_index = _next_stream_index;
    uint64_t max_index = 0;
    auto it = _marker_map.begin();
    while (it != _marker_map.end()) {
        if (max_index == 0) {
            min_index = it->first <= _next_stream_index ? _next_stream_index : it->first;
            max_index = it->second;
            list.push_back(it->first);
        } else if (it->first > max_index) {
            break;
        } else if (it->second > max_index) {
            max_index = it->second;
            list.push_back(it->first);
        } else {
            list.push_back(it->first);
        }
        it++;
    }
    if (max_index) {
        for (uint64_t n : list) {
            _marker_map.erase(n);
        }
        _marker_map[min_index] = max_index;
    }

    _reassemble_count = 0;
    uint64_t last_right = 0;
    it = _marker_map.begin();
    while (it != _marker_map.end()) {
        if (last_right == 0) {
            _reassemble_count += it->second - it->first;
            last_right = it->second;
        } else {
            if (it->first >= last_right) {
                _reassemble_count += it->second - it->first;
                last_right = it->second;
            } else if (it->second > last_right) {
                _reassemble_count += it->second - last_right;
                last_right = it->second;
            }
        }
        it++;
    }

    // it = _marker_map.begin();
    // while (it != _marker_map.end()) {
    //     std::cerr << "recount: " << _next_stream_index << ": " << it->first << " -> " << it->second << '\n';
    //     it++;
    // }
}

void StreamReassembler::reassemble() {
    recount();
    uint64_t old = _next_stream_index;

    size_t remaining_capacity = _output.remaining_capacity();
    size_t count = 0;
    std::list<uint64_t> list;
    auto it = _marker_map.begin();
    if (it != _marker_map.end() and it->first <= _next_stream_index and it->second > _next_stream_index) {
        count = min(remaining_capacity, static_cast<size_t>((it->second - _next_stream_index)));
        if (count == 0) {
            return;
        }
        if ((_next_stream_index + count) == it->second) {
            _marker_map.erase(it->first);
        } else {
            _marker_map.erase(it->first);
            _marker_map[_next_stream_index + count] = it->second;
        }

        string r(count, 0);
        size_t begin_index = _next_stream_index % _capacity;
        if (count <= _capacity - begin_index) {
            memcpy(&r[0], &_buffer[begin_index], count);
        } else {
            size_t size_1 = _capacity - begin_index;
            memcpy(&r[0], &_buffer[begin_index], size_1);
            size_t size_2 = count - size_1;
            memcpy(&r[size_1], &_buffer[(begin_index + size_1) % _capacity], size_2);
        }

        _output.write(r);
        _next_stream_index = _next_stream_index + count;
    }

    if (old != _next_stream_index){
        recount();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _reassemble_count; }

bool StreamReassembler::empty() const { return _reassemble_count == 0; }
