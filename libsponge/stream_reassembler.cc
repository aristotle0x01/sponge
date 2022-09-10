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
        // [index|_next_stream_index, _next_stream_index + _output.remaining_capacity() | index + data.length()]
        uint64_t data_start_stream_index = index;
        if(_next_stream_index > index){
            data_start_stream_index = _next_stream_index;
        }
        uint64_t data_end_stream_index = min(_next_stream_index + _output.remaining_capacity(), index + data.length());
        if (data_end_stream_index <= data_start_stream_index){
            break;
        }

        auto it = _marker_map.find(data_start_stream_index);
        if (it != _marker_map.end()) {
            if(data_end_stream_index > _marker_map[data_start_stream_index]){
                _marker_map[data_start_stream_index] = data_end_stream_index;
            }
        } else {
            _marker_map[data_start_stream_index] = data_end_stream_index;
        }

        // std::cerr << "push_substring1: " << _next_stream_index << "-" << m_index  << " -> " << tail_stream_index << '\n';

        size_t count = data_end_stream_index - data_start_stream_index;
        size_t d_index = data_start_stream_index - index;
        size_t begin_index = data_start_stream_index % _capacity;
        // std::cerr << "push_substring2: " << begin_index << ": " << write_start_index << " -> " << count << '\n';
        if (count <= _capacity - begin_index) {
            memcpy(&_buffer[begin_index], &data[d_index], count);
        } else {
            size_t size_1 = _capacity - begin_index;
            memcpy(&_buffer[begin_index], &data[d_index], size_1);
            size_t size_2 = count - size_1;
            memcpy(&_buffer[(begin_index + size_1) % _capacity], &data[d_index + size_1], size_2);
        }

        recount();

        break;
    }

    reassemble();

    if (_ended and _next_stream_index >= _ending_index) {
        _output.end_input();
    }
}

void StreamReassembler::recount() {
    // std::cerr << "recount --------------------------" << '\n';

    // delete those out of capacity
    std::list<uint64_t> list;
    uint64_t valid_last = _next_stream_index + _output.remaining_capacity();
    auto it = _marker_map.begin();
    while (it != _marker_map.end()) {
        if (it->second > valid_last) {
            if (it->first >= valid_last){
                list.push_back(it->first);
            } else{
                _marker_map[it->first] = valid_last;
            }
        } 
        it++;
    }
    for (uint64_t n : list) {
        _marker_map.erase(n);
    }
    list.clear();

    // merge fist consecutive range
    uint64_t min_index = 0;
    uint64_t max_index = 0;
    it = _marker_map.begin();
    while (it != _marker_map.end()) {
        if (max_index == 0) {
            min_index = it->first;
            max_index = it->second;
            list.push_back(it->first);
        } else if (it->first > max_index) {
            break;
        } else if (it->first == max_index) {
            max_index = it->second;
            list.push_back(it->first);
        }else if (it->second > max_index) {
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
            _reassemble_count += it->second - (it->first > _next_stream_index ? it->first : _next_stream_index);
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
    // std::cerr << "recount: " <<_marker_map.size() << "-" << _next_stream_index << '\n';
    // while (it != _marker_map.end()) {
    //     std::cerr << "[" << it->first << "," << it->second << "], ";
    //     it++;
    // }
    // std::cerr <<'\n';

}

void StreamReassembler::reassemble() {
    size_t remaining_capacity = _output.remaining_capacity();
    size_t count = 0;
    std::list<uint64_t> list;
    auto it = _marker_map.begin();
    if (it != _marker_map.end() and it->first <= _next_stream_index and it->second > _next_stream_index) {
        count = min(remaining_capacity, static_cast<size_t>((it->second - _next_stream_index)));
        if (count == 0) {
            return;
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

        if ((_next_stream_index + count) == it->second) {
            _marker_map.erase(it->first);
        } else {
            // _marker_map.erase(it->first);
            // _marker_map[_next_stream_index + count] = it->second;
            // surpass the remaining capacity, delete
            _marker_map.clear();
        }
        _next_stream_index = _next_stream_index + count;

        recount();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _reassemble_count; }

bool StreamReassembler::empty() const { return _reassemble_count == 0; }
