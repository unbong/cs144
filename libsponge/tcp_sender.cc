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

    if (_isFIN )
        return ;
    TCPSegment segment;
    TCPHeader header ;
    header.seqno = wrap(_next_seqno,_isn);
    uint64_t absSeq = unwrap(header.seqno, _isn, _checkPoint);

    // send syn
    if(absSeq == 0){
        header.syn = true;
        header.seqno = wrap(_next_seqno,_isn);
        segment.header() = header;
        _isSYN = true;
        send_segment( segment);
        return;
    }

    // send fin
    if (_stream.eof() && (_windowSize -(_next_seqno-_checkPoint) ) > 0)
    {
        header.fin  = true;
        _isFIN = true;
        header.seqno = wrap(_next_seqno,_isn);
        segment.header() = header;
        send_segment( segment);
        return;
    }

    while((_windowSize -(_next_seqno-_checkPoint) ) > 0 && !_stream.buffer_empty() )
    {

        // 查看windowsize和最大有效载荷大小，后取得大小较小的数据。
        uint32_t len = min(TCPConfig::MAX_PAYLOAD_SIZE, (_windowSize -(_next_seqno-_checkPoint) ));

        // syn 时没有有效载荷 但是占用一个字符

        Buffer str = _stream.read(len);
        segment.payload() = str;

        if(_stream.eof() && (_windowSize -(_next_seqno-_checkPoint) ) > 0)
        {
            header.fin  = true;
            _isFIN = true;
        }
        header.seqno = wrap(_next_seqno,_isn);
        segment.header() = header;

        // 放入数据队列
        send_segment( segment);
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {


    // 更新window size

    bool isPop = false;
    // 从重新发送队列中删除对应的数据端

    uint64_t absAckSeq = unwrap(ackno, _isn, _checkPoint);

    if(absAckSeq > _next_seqno)
        return ;

    _windowSize = window_size;
    if(_checkPoint < absAckSeq){
        _checkPoint = absAckSeq;
    }
    else
    {
        return ;
    }


    // 如集合中存在ackno小的 则把小的也一起在集合中删除
    while(!_segments_wait.empty())
    {
        TCPSegment tcp_segment = _segments_wait.front();
        if(unwrap(tcp_segment.header().seqno, _isn, _checkPoint) > absAckSeq)
            break ;

        _bytes_flight-=tcp_segment.length_in_sequence_space();
        isPop = true;

        _retransmission_timer = 0;
        _timeout = _initial_retransmission_timeout;
        _consecutiveRetransmissionsCount = 0;
        _segments_wait.pop();
    }
//    if(_next_seqno >= absAckSeq)
//    {
//        _bytes_flight-=_segments_wait.begin()->second.length_in_sequence_space();
//        isPop = true;
//        _retransmission_timer = 0;
//        _timeout = _initial_retransmission_timeout;
//        _consecutiveRetransmissionsCount = 0;
//        _segments_wait.erase(_segments_wait.begin(), _segments_wait.begin());
//
//    }

    if (isPop)
        fill_window();

    _isRetransmissionWorking = !_segments_wait.empty();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {

    if(!_isRetransmissionWorking)
        return ;
    // 超时了
    _retransmission_timer += ms_since_last_tick;
    if(_retransmission_timer  >= _timeout)
    {
        // 重传

        _segments_out.push(_segments_wait.front());

        _retransmission_timer = 0;
        _timeout = 2 *  _timeout;
        _consecutiveRetransmissionsCount++;
    }
    else
    {
        //fill_window();
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutiveRetransmissionsCount; }

void TCPSender::send_empty_segment() {
    TCPSegment segment;
    TCPHeader header = segment.header();
    header.ack = true;
    header.seqno = WrappingInt32(_isn.raw_value() + _stream.bytes_read());


    _segments_out.push(segment);
    _segments_wait.push( segment);
}

void TCPSender::send_segment( const TCPSegment &segment) {


    _segments_out.push(segment);

    _segments_wait.push(segment);
    _next_seqno +=  segment.length_in_sequence_space();

    _bytes_flight += segment.length_in_sequence_space();
//    if(_isSYN || _isFIN)
//    {
//        _next_seqno++;
//        _bytes_flight++;
//    }

    if(!_isRetransmissionWorking )
    {
        _retransmission_timer =0;
        _isRetransmissionWorking = true;
    }

}
