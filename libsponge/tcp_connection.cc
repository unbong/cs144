
#include <iostream>
#include "tcp_connection.hh"

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const {

    return _sender.stream_in().remaining_capacity();
}

size_t TCPConnection::bytes_in_flight() const {
    return _sender.bytes_in_flight();
}

size_t TCPConnection::unassembled_bytes() const {
    return _receiver.unassembled_bytes();
}

size_t TCPConnection::time_since_last_segment_received() const {
    // 返回累积时间
    return _since_last_received_tick ;
}

void TCPConnection::segment_received(const TCPSegment &seg) {

    // 当接收到段时，重置记录经过时间
    _since_last_received_tick = 0;
    // 如果收到rst，则关闭连接
    if(seg.header().rst){
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        _active = false;
        _linger_after_streams_finish = false;
        cerr<< "rst"<< endl;
        return;
    }
    // 接收段，（该操作会更新当前状态）
    _receiver.segment_received(seg);

    // 如果接收到ack 将ackno与window通知到发送者中
    if(seg.header().ack)
    {
        _sender.ack_received(seg.header().ackno,seg.header().win);
    }

    // 如果last cak状态 则关闭连接
    if(TCPState::state_summary(_sender ) == TCPSenderStateSummary::FIN_ACKED
        && TCPState::state_summary(_receiver ) == TCPReceiverStateSummary::FIN_RECV
        && !_linger_after_streams_finish
        && _active){
        _active = false;
        return ;
    }

    //  如果在listen状态下收到一个段后，还是处于listen状态，说明收到的段是非法的，因此返回一个空的ack信息
    if(TCPState::state_summary(_sender ) == TCPSenderStateSummary::CLOSED
        && TCPState::state_summary(_receiver ) == TCPReceiverStateSummary::LISTEN)
    {
        return ;
    }

    // syn rcv 发送ack
    // syn sent 发送ack
    // establish 发送ack
    // fin wait1 发送ack
    // fin wait2 发送ack

    // 被动关闭的一方不需要处于徘徊的状态
    // 进入到了close wait状态
    if(TCPState::state_summary(_sender ) == TCPSenderStateSummary::SYN_ACKED
        && TCPState::state_summary(_receiver ) == TCPReceiverStateSummary::FIN_RECV
        && _linger_after_streams_finish)
    {
        _linger_after_streams_finish = false;
    }

    // keep alive
    if(_receiver.ackno().has_value() && (seg.length_in_sequence_space() == 0 )
        && (seg.header().seqno == _receiver.ackno().value() -1))
    {
        _sender.send_empty_segment();
        send_data();
        return ;
    }

    _sender.fill_window();
    // 在send data上只要不是ack( 对方的确认段）就会发送ack
    if(seg.length_in_sequence_space() > 0 && _sender.segments_out().empty()){
        _sender.send_empty_segment();
    }
    send_data();
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    size_t len =_sender.stream_in().write(data);
    _sender.fill_window();
    send_data();
    return len;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {

    _sender.tick(ms_since_last_tick);
    _since_last_received_tick += ms_since_last_tick;

    // 如果重传的次数超过规定值，则认为发生了错误发送rst
    if(_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS)
    {
        // 终止连接 并向对方发送RST
        _sender.send_empty_segment();

        _since_last_received_tick =0 ;
        reset_connection();
        return ;
    }

    // time wait时如果超过 10 * timeoutdef 就认为对方已经接到主动关闭一方发出的承认。
    // 因此就关闭连接
    if(TCPState(_sender, _receiver, _active, _linger_after_streams_finish)
               == TCPState(TCPState::State::TIME_WAIT)
        &&
        _since_last_received_tick >= (10 * _cfg.rt_timeout)){

        _active = false;
        return;
    }

    // establis  主动断开连接 发送fin(ack)
    // close wait 应用程序关闭 发送fin(ack)

    // 因为stream终止时fill会设置fin
    if(_sender.stream_in().eof())
    {
        _sender.fill_window();
    }
    send_data();
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    send_data();
}

void TCPConnection::connect() {

    _sender.fill_window();
    send_data();
}
void TCPConnection::reset_connection()
{

    TCPSegment tmpSeg ;
    tmpSeg.header().rst = true;
    _segments_out.push(tmpSeg);
    _receiver.stream_out().set_error();
    _sender.stream_in().set_error();
    _active = false;
}

void TCPConnection::send_data() {
   TCPSegment tmpSeg;

   while (!_sender.segments_out().empty())
   {
        tmpSeg = _sender.segments_out().front();
        if(_receiver.ackno().has_value()){
            tmpSeg.header().ack = true;

            tmpSeg.header().ackno = _receiver.ackno().value();
            tmpSeg.header().win = _receiver.window_size();
        }

        _segments_out.push(tmpSeg);
        _sender.segments_out().pop();
   }
}



TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            // 关闭stream in和out
            _sender.stream_in().end_input();
            _receiver.stream_out().end_input();
            // Your code here: need to send a RST segment to the peer
            // 发送rst
            _sender.send_empty_segment();
            _sender.segments_out().front().header().rst= true;
            _active = false;

        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}


