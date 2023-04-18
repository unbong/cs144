#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender


# define MAX_SEG_SIZE 1420
// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const {

    return _bytes_flight;

}

void TCPSender::fill_window() {

    // 如果时最后一个 发送fin
    // 其他正常发送 发送的大小 window size  byte stream eof?
    //
    //
    // 如果是第一次？发送syn
//    if(_stream.bytes_read() == 0){
//
//        // 生成syn segment
//        TCPSegment segment;
//        TCPHeader header = segment.header();
//        header.syn = true;
//        // 随机数生成
//        header.seqno = _isn;
//
//        uint64_t absSeq = 0;
//        // 放入输出端和需要承认的端
//        _segments_out.push(segment);
//        Segment_len seg(segment, _lastTickStamp);
//        _segments_copy.insert_or_assign(absSeq, seg);
//        _bytes_flight += 1;
//        _next_seqno = 1;
//        //
//
//    }
//    else if(_stream.eof())
//    {
//        TCPSegment segment;
//        TCPHeader header = segment.header();
//        header.fin = true;
//        header.seqno = WrappingInt32(_isn.raw_value() + _stream.bytes_read());
//        uint64_t absSeq = unwrap(header.seqno, _isn, _checkPoint);
//
//        _segments_out.push(segment);
//        Segment_len outSeq(segment, _lastTickStamp);
//        _segments_copy.insert_or_assign(absSeq, outSeq);
//        _bytes_flight += 1;
//        _next_seqno = _stream.bytes_read() + 2;
//    }
    //else


    if(_windowSize > 0  )
    {
        TCPSegment segment;
        TCPHeader header ;
        header.seqno = WrappingInt32(_isn+_next_seqno);
        uint64_t absSeq = unwrap(header.seqno, _isn, _checkPoint);

        if(absSeq == 0){
            header.syn = true;
            //header.seqno= WrappingInt32(0);
        }

        if(_stream.eof())
        {
            header.fin  = true;
        }
        segment.header() = header;

        // 查看windowsize和最大有效载荷大小，后取得大小较小的数据。
        uint32_t len = min(TCPConfig::MAX_PAYLOAD_SIZE, _windowSize);

//        if (_windowSize >= len)
//            _windowSize -= len;
//        else if(_windowSize < len || header.fin)
//            _windowSize  = 0;
//
//        if(!header.syn)
//        {
//            Buffer str = _stream.read(len);
//            segment.payload() = str;
//
//            if(str.size() !=0)
//            {
//                // 放入数据队列
//                _segments_out.push(segment);
//                Segment_len outSeq(segment, len);
//                _segments_copy.insert_or_assign(absSeq, outSeq);
//                _next_seqno +=  segment.length_in_sequence_space();
//                if(_stream.eof()) _next_seqno+=2;
//                _bytes_flight += segment.length_in_sequence_space();
//            }
//        }
//        else{
//            // 放入数据队列
//            _segments_out.push(segment);
//            Segment_len outSeq(segment, len);
//            _segments_copy.insert_or_assign(absSeq, outSeq);
//            _next_seqno +=  segment.length_in_sequence_space();
//            if(_stream.eof()) _next_seqno+=2;
//            _bytes_flight += segment.length_in_sequence_space();
//        }

        // syn 时没有有效载荷 但是占用一个字符
        if(! header.syn)
        {
            Buffer str = _stream.read(len);
            segment.payload() = str;
        }
        len = segment.length_in_sequence_space();
        _windowSize -= len;
        // 放入数据队列
        if(len> 0 || header.syn)
        _segments_out.push(segment);
        Segment_len outSeq(segment, len);
        _segments_copy.insert_or_assign(absSeq, outSeq);
        _next_seqno +=  segment.length_in_sequence_space();
        if(_stream.eof()) _next_seqno+=2;
        _bytes_flight += segment.length_in_sequence_space();
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {


    // 更新window size
     _windowSize = window_size;

    // 从重新发送队列中删除对应的数据端
    uint32_t seq = ackno.raw_value();
    uint64_t absSeq = unwrap(WrappingInt32(seq), _isn, _checkPoint);
    _checkPoint = absSeq;

    // 如集合中存在ackno小的 则把小的也一起在集合中删除
    for(auto iter = _segments_copy.begin(); iter != _segments_copy.lower_bound(absSeq); iter++ )
    {
        _bytes_flight-=iter->second.length;
    }
    _segments_copy.erase(_segments_copy.begin(), _segments_copy.lower_bound(absSeq));
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {

    // 超时了
    if(_timeout < ms_since_last_tick)
    {
        // 重传

        for(auto iter = _segments_copy.begin(); iter != _segments_copy.end(); iter++)
        {
            if (_windowSize > 0)
                _segments_out.push(iter->second.segment);
        }

        _timeout = _initial_retransmission_timeout;
    }
    else
    {
        fill_window();
        _timeout -= ms_since_last_tick;
    }

    _lastTickStamp += ms_since_last_tick;

}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutiveRetransmissionsCount; }

void TCPSender::send_empty_segment() {
    TCPSegment segment;
    TCPHeader header = segment.header();
    header.ack = true;
    header.seqno = WrappingInt32(_isn.raw_value() + _stream.bytes_read());

    uint64_t absSeq = unwrap(header.seqno, _isn, _checkPoint);
    _segments_out.push(segment);
    Segment_len outSeq(segment, _lastTickStamp);
    _segments_copy.insert_or_assign(absSeq, outSeq);
}
