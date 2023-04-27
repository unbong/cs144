#include "tcp_connection.hh"

#include <iostream>

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
    // tick的计数值 - 没次收到时的tick快照 * 超时时间 （有可能不对）
    return (_tick_count- _since_last_received_tick) * TCPConfig::TIMEOUT_DFLT;
}

void TCPConnection::segment_received(const TCPSegment &seg) {

//    std::cout << "\r\n msg:server receive start" << std::endl;
    _since_last_received_tick = _tick_count;
    // 如果收到rst 则关闭
    if(seg.header().rst)
    {
        _receiver.stream_out().set_error();
        _sender.stream_in().set_error();
        _isActive = false;
        _tcpState = TCPState::State::CLOSED;
        // todo close ?
        //_sender.send_empty_segment();

        // close(tmpSeg);
        return ;
    }
    else {
        _receiver.segment_received(seg);
    }

    // syn or fin
    if(seg.header().syn && !seg.header().ack  )
    {
        if(_tcpState == TCPState::State::LISTEN){
            _tcpState = TCPState::State::SYN_RCVD;
            _sender.fill_window();
            send_ack_data(TCPConnection::Type::SYN, _receiver);
        }
    }

    // 接收fin 发送ack
    if(seg.header().fin  && !seg.header().ack){

        _sender.send_empty_segment();
        send_ack_data(TCPConnection::Type::FIN, _receiver);
        if(_tcpState == TCPState::State::ESTABLISHED){
            _tcpState = TCPState::State::CLOSE_WAIT;
        }
        else if(_tcpState == TCPState::State::FIN_WAIT_2)
        {
            _tcpState = TCPState::State::TIME_WAIT;
        }
        else if(_tcpState == TCPState::State::FIN_WAIT_1)
        {
            _tcpState = TCPState::State::CLOSING;
        }
    }

    // todo _sender 发送fin的处理
    // ack
    if(seg.header().ack){
        // 接收ack和window
        _sender.ack_received(seg.header().ackno, seg.header().win);

        // 客户端收到syn ack 发送ack之后 进入到establish状态
        if(seg.header().syn){
            if(_tcpState == TCPState::State::SYN_SENT){
                _tcpState = TCPState::State::ESTABLISHED;
            }
            _sender.send_empty_segment();
            send_ack_data(TCPConnection::Type::ACK, _receiver);
        }
        // 收到 fin ack内容
        // 等待对方的fin
        else if(seg.header().fin )
        {
            if(_tcpState == TCPState::State::FIN_WAIT_1)
            {
                _tcpState = TCPState::State::FIN_WAIT_2;
            }
            else if(_tcpState == TCPState::State::CLOSING){
                _tcpState = TCPState::State::TIME_WAIT;
            }
            else if(_tcpState == TCPState::State::LAST_ACK){
                _tcpState = TCPState::State::CLOSED;
            }
        }else{
            // syn ack or ack 返回数据。
            _sender.fill_window();
            send_ack_data(TCPConnection::Type::ACK, _receiver);
        }

        // 如果发送端 入流结束 则发送fin
        if (_sender.stream_in().eof())
        {
            _sender.send_empty_segment();
            send_syn_fin_rst_data(TCPConnection::Type::FIN);
            _isFinSend = true;

            if(_tcpState == TCPState::State::ESTABLISHED){
                _tcpState = TCPState::State::FIN_WAIT_1;
            }
            else if(_tcpState == TCPState::State::CLOSE_WAIT)
            {
                _tcpState = TCPState::State::LAST_ACK;
            }
        }
    }

//    // 服务端收到了ack of syn时进入estabslish状态
//    if (_sender.next_seqno() ==  _receiver.ackno().value())
//    {
//        _isActive = true;
//    }

    // keep live
    if(_receiver.ackno().has_value() && (seg.length_in_sequence_space() == 0)
        && seg.header().seqno == _receiver.ackno().value()-1)
    {
        _sender.send_empty_segment();
        send_ack_data(TCPConnection::Type::ACK, _receiver);
    }
}

bool TCPConnection::active() const { return _isActive; }

size_t TCPConnection::write(const string &data) {
//    std::cout << "\r\n msg:client write start." << std::endl;
    size_t len =_sender.stream_in().write(data);
    _sender.fill_window();
    send_ack_data(TCPConnection::Type::ACK, _receiver);
    _isActive = true;
//    std::cout << "\r\n msg:client write end." << std::endl;

    return len;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    //std::cout << "\r\n msg:tick start" << std::endl;
    _sender.tick(ms_since_last_tick);
    _tick_count++;
    if(_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS)
    {
        // 终止连接 并向对方发送RST
        _sender.send_empty_segment();
        _sender.segments_out().front().header().rst= true;
        _tick_count =0 ;
        _isActive = false;
    }

    // 如果timer大于0 则表明，自最后一次接收到seg 至少执行了一次tick 因此属于徘徊状态
    if (time_since_last_segment_received() > ms_since_last_tick){
        _linger_after_streams_finish = false;
    }

    // 干净的关闭
    if(_tcpState == TCPState::State::TIME_WAIT
        && time_since_last_segment_received() > (10 * TCPConfig::TIMEOUT_DFLT))
    {
        _isActive = false;
        _tcpState = TCPState::State::CLOSED;
    }
    //std::cout << "\r\n msg:tick end" << std::endl;
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
}

void TCPConnection::connect() {

   _sender.fill_window();
   send_syn_fin_rst_data(TCPConnection::Type::SYN);
   _tcpState = TCPState::State::SYN_SENT;
   _isActive = true;
//    std::cout << "\n msg:client connect done." << std::endl;
   // todo ? win size 对吗？
   //_segments_out.front().header().win = static_cast<uint16_t>(_receiver.window_size());
}

void TCPConnection::send_syn_fin_rst_data(TCPConnection::Type type) {
   TCPSegment seg;
   while (!_sender.segments_out().empty())
   {
        seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        if(type == TCPConnection::Type::SYN){
            seg.header().syn = true;
        }
        else if(type == TCPConnection::Type::FIN) {
            seg.header().fin = true;
        }else if(type == TCPConnection::Type::RST){
            seg.header().rst = true;
        }
        _segments_out.push(seg);
   }
}

void TCPConnection::send_ack_data(TCPConnection::Type type , const TCPReceiver &rcv) {
   TCPSegment tmpSeg;

   while (!_sender.segments_out().empty())
   {
        tmpSeg = _sender.segments_out().front();
        tmpSeg.header().ack = true;
        if(type == TCPConnection::Type::SYN){
            tmpSeg.header().syn = true;
        }
        else if(type == TCPConnection::Type::FIN){
            tmpSeg.header().fin = true;
        }
        tmpSeg.header().ackno = rcv.ackno().value();
        tmpSeg.header().win = rcv.window_size();
        _segments_out.push(tmpSeg);
        _sender.segments_out().pop();
   }
//   cerr << "\r\n msg: get ack  in syn  "<< tmpSeg.header().to_string()<< std::endl;
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
//            push_segments();
            _isActive = false;

        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}


