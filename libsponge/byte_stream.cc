#include "byte_stream.hh"

#include <algorithm>

// implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

using namespace std;

ByteStream::ByteStream(const size_t capacity) {
    this->_capacity = capacity;
    this->_error = false;
    this->_read_pos = 0;
    this->_write_pos = 0;
    this->_total_read_count = 0;
    this->_total_write_count = 0;
    this->_buffer.resize(capacity);
    this->_input_ended = false;
}

size_t ByteStream::write(const string &data) {
    size_t i = 0;
    size_t writable = min(remaining_capacity(), data.length());
    while (i < writable) {
        data.copy(&this->_buffer[_write_pos], 1, i);
        _write_pos = (_write_pos + 1) % _capacity;
        _total_write_count = _total_write_count + 1;
        i++;
    }

    return writable;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    if (len == 0 or _total_read_count == _total_write_count) {
        return {};
    }
    size_t readable = min(_total_write_count - _total_read_count, len);
    string r(readable, 0);
    size_t i = 0;
    size_t pos = _read_pos;
    while (i < readable) {
        this->_buffer.copy(&r[i], 1, pos);
        pos = (pos + 1) % _capacity;
        i++;
    }
    return r;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) { read(len); }

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    if (len == 0 or _total_read_count == _total_write_count) {
        return {};
    }
    size_t readable = min(_total_write_count - _total_read_count, len);
    string r(readable, 0);
    size_t i = 0;
    while (i < readable) {
        this->_buffer.copy(&r[i], 1, _read_pos);
        _read_pos = (_read_pos + 1) % _capacity;
        _total_read_count = _total_read_count + 1;
        i++;
    }
    return r;
}

void ByteStream::end_input() { this->_input_ended = true; }

bool ByteStream::input_ended() const { return this->_input_ended; }

size_t ByteStream::buffer_size() const { return _total_write_count - _total_read_count; }

bool ByteStream::buffer_empty() const { return _total_read_count == _total_write_count; }

bool ByteStream::eof() const { return this->_input_ended and _total_read_count == _total_write_count; }

size_t ByteStream::bytes_written() const { return _total_write_count; }

size_t ByteStream::bytes_read() const { return _total_read_count; }

size_t ByteStream::remaining_capacity() const { return _capacity - labs(_total_write_count - _total_read_count); }
