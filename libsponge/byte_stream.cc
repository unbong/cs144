#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) {
    // initialize datastruct
    _capacity = capacity;
    _data =  string(capacity, ' ') ;
}

size_t ByteStream::write(const string &data) {

    // check remain length if  <  data len error
    size_t len = data.length();
    if (len > remaining_capacity() ) {
        len = remaining_capacity();
    }

    for (size_t i = 0;  i < len ; i++) {
        _data[(_writeIndex++) %_capacity] = data.at(i);
    }

    _writeIndex = _writeIndex %_capacity;
    _writeBytesSize = _writeBytesSize +len;
    _bufSize+=len;
    return len;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {

    if (len > (_capacity-remaining_capacity())) throw unix_error("peek data len is too big");
    string result;
    for(size_t i = 0; i < len; i++)
    {
        result.push_back( _data.at((_readIndex+i) %_capacity));
    }
    return result;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {

    if (len > buffer_size()) throw unix_error("pop data len > buffer size");
    _readIndex = (_readIndex + len) % _capacity;
    _readBytesSize = _readBytesSize +len;
    _bufSize-=len;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    // check
    size_t bufLen =0;
    if (len > buffer_size())
        bufLen = buffer_size();

    string result;
    for(size_t i = 0; i < bufLen; i++)
    {
        result.push_back( _data.at((_readIndex++) % _capacity));
    }
    _readIndex = _readIndex %_capacity;
    _readBytesSize = _readBytesSize +bufLen;
    _bufSize -= bufLen;

    return result;
}

void ByteStream::end_input() {
    _isEnded = true;
}

bool ByteStream::input_ended() const { return _isEnded; }

size_t ByteStream::buffer_size() const {

    return _bufSize;
}

bool ByteStream::buffer_empty() const { return _bufSize == 0; }

bool ByteStream::eof() const {

    return buffer_empty() && input_ended();
}

size_t ByteStream::bytes_written() const { return _writeBytesSize; }

size_t ByteStream::bytes_read() const { return _readBytesSize; }

size_t ByteStream::remaining_capacity() const {

    return _capacity - buffer_size();
}
