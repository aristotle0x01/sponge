#include "byte_stream.hh"

#include <algorithm>
#include <cstring>

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
    this->_avail = capacity;
}

size_t ByteStream::write(const string &data) {
    if (data.length() == 0)
        return 0;

    size_t capacity = _capacity;
    size_t bytes_to_write = std::min(remaining_capacity(), data.length());

    if (bytes_to_write <= capacity - _write_pos) {
        memcpy(&_buffer[_write_pos], data.c_str(), bytes_to_write);
        _write_pos = (_write_pos + bytes_to_write) % _capacity;
        _total_write_count = _total_write_count + bytes_to_write;
    } else {
        size_t size_1 = capacity - _write_pos;
        memcpy(&_buffer[_write_pos], data.c_str(), size_1);
        _write_pos = (_write_pos + size_1) % _capacity;
        _total_write_count = _total_write_count + size_1;
        size_t size_2 = bytes_to_write - size_1;
        memcpy(&_buffer[_write_pos], &data[size_1], size_2);
        _write_pos = (_write_pos + size_2) % _capacity;
        _total_write_count = _total_write_count + size_2;
    }
    this->_avail = this->_avail - bytes_to_write;

    return bytes_to_write;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    if (len == 0)
        return {};

    size_t capacity = _capacity;
    size_t bytes_to_read = std::min(_total_write_count - _total_read_count, len);

    string r(bytes_to_read, 0);
    if (bytes_to_read <= capacity - _read_pos) {
        memcpy(&r[0], &_buffer[_read_pos], bytes_to_read);
    } else {
        size_t read_pos = _read_pos;

        size_t size_1 = capacity - read_pos;
        memcpy(&r[0], &_buffer[read_pos], size_1);
        read_pos = (read_pos + size_1) % _capacity;
        size_t size_2 = bytes_to_read - size_1;
        memcpy(&r[size_1], &_buffer[read_pos], size_2);
        read_pos = (read_pos + size_2) % _capacity;
    }

    return r;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) { read(len); }

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    if (len == 0)
        return {};

    size_t capacity = _capacity;
    size_t bytes_to_read = std::min(_total_write_count - _total_read_count, len);

    string r(bytes_to_read, 0);
    if (bytes_to_read <= capacity - _read_pos) {
        memcpy(&r[0], &_buffer[_read_pos], bytes_to_read);
        _read_pos = (_read_pos + bytes_to_read) % _capacity;
        _total_read_count = _total_read_count + bytes_to_read;
    } else {
        size_t size_1 = capacity - _read_pos;
        memcpy(&r[0], &_buffer[_read_pos], size_1);
        _read_pos = (_read_pos + size_1) % _capacity;
        _total_read_count = _total_read_count + size_1;
        size_t size_2 = bytes_to_read - size_1;
        memcpy(&r[size_1], &_buffer[_read_pos], size_2);
        _read_pos = (_read_pos + size_2) % _capacity;
        _total_read_count = _total_read_count + size_2;
    }
    this->_avail = this->_avail + bytes_to_read;

    return r;
}

void ByteStream::end_input() { this->_input_ended = true; }

bool ByteStream::input_ended() const { return this->_input_ended; }

size_t ByteStream::buffer_size() const { return _total_write_count - _total_read_count; }

bool ByteStream::buffer_empty() const { return _total_read_count == _total_write_count; }

bool ByteStream::eof() const { return this->_input_ended and _total_read_count == _total_write_count; }

size_t ByteStream::bytes_written() const { return _total_write_count; }

size_t ByteStream::bytes_read() const { return _total_read_count; }

size_t ByteStream::remaining_capacity() const { return _avail; }
