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

    if(_windowSize > 0 && ! _isFinAcked  )
    {
        TCPSegment segment;
        TCPHeader header ;
        header.seqno = WrappingInt32(_isn+_next_seqno);
        uint64_t absSeq = unwrap(header.seqno, _isn, _checkPoint);

        if(absSeq == 0){
            header.syn = true;
            //header.seqno= WrappingInt32(0);
        }

        // 查看windowsize和最大有效载荷大小，后取得大小较小的数据。
        uint32_t len = min(TCPConfig::MAX_PAYLOAD_SIZE, _windowSize);

        // syn 时没有有效载荷 但是占用一个字符
        if(! header.syn)
        {
            Buffer str = _stream.read(len);
            segment.payload() = str;
            len = str.size();
        }
        if(_stream.eof())
        {
            header.fin  = true;
            len++;
            _isFinAcked = true;
        }
        segment.header() = header;
        _windowSize -= len;
        // 放入数据队列
        if(len> 0 || header.syn || header.fin)
        {
            _segments_out.push(segment);
            Segment_len outSeq(segment, len);
            _segments_wait.insert_or_assign(absSeq, outSeq);
            _next_seqno +=  len;
            //if(_stream.eof()) _next_seqno+=2;
            _bytes_flight += len;
            if(!_isRetransmissionWorking )
            {
                _retransmission_timer =0;
                _isRetransmissionWorking = true;
            }
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {


    // 更新window size
     _windowSize = window_size;
    bool isPop = false;
    // 从重新发送队列中删除对应的数据端
    uint32_t seq = ackno.raw_value();
    uint64_t absSeq = unwrap(WrappingInt32(seq), _isn, _checkPoint);
    _checkPoint = absSeq;

    // 如集合中存在ackno小的 则把小的也一起在集合中删除
    for(auto iter = _segments_wait.begin();
         iter != _segments_wait.lower_bound(absSeq)
         && _next_seqno >= absSeq; iter++ )
    {
        _bytes_flight-=iter->second.length;
        isPop = true;
        _retransmission_timer = 0;
        _timeout = _initial_retransmission_timeout;
        _consecutiveRetransmissionsCount = 0;
    }
    if(_next_seqno >= absSeq)
    {
        _segments_wait.erase(_segments_wait.begin(), _segments_wait.lower_bound(absSeq));
    }
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
        for(auto iter = _segments_wait.begin(); iter != _segments_wait.end()
                 && iter == _segments_wait.begin(); iter++)
        {
            _segments_out.push(iter->second.segment);
            _windowSize -= iter->second.length;
        }
        _retransmission_timer = 0;
        _timeout = 2 *  _timeout;
        _consecutiveRetransmissionsCount++;
    }
    else
    {
        fill_window();
    }
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
    _segments_wait.insert_or_assign(absSeq, outSeq);
}
