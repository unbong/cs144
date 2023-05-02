#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    TCPHeader header = seg.header();

    // syn segment
    if(header.syn )
    {
        _synFlag = true;
        _isn = header.seqno;
    }
    // fin segment
    if (header.fin ){
        _finFlag = true;
        _finSeq = header.seqno;
        // fin syn 同时到达时
        if (header.syn  ) {
            _synFlag = true;
            _isn = header.seqno;
            _finSeq = _finSeq+1;
        }

        // fin seq转换未ABS seq checkpoint的值为最后一个组装的字节数
        uint64_t finSeq = unwrap(_finSeq, _isn, _reassembler.stream_out().bytes_written()  );
        // 写入最后一个字符，并指定未EOF
        _reassembler.push_substring(seg.payload().copy(),finSeq-1,  EOF_IS_TRUE);
        // 因为接收到了fin，ackNo需要占用一个位置，+1 且从流的索引转换到绝对的序列号时也需要+1

        _ackNo = _reassembler.stream_out().bytes_written()+1;
        if(_reassembler.stream_out().input_ended())
            _ackNo++;

    }
    // 已经建立连接的前提下
    else if(_synFlag)
    {

        uint64_t absSeq = unwrap(header.seqno, _isn, _reassembler.stream_out().bytes_written() );
        // 如果时syn和数据一起收到时数据的index需要加1
        //if(absSeq == 0 && header.syn )
        Buffer payload = seg.payload();

        // 写入StreamReAssembler中
        // absSeq与 第二个参数是 -1 的关系， 因此absSeq最小值传递1 在reassembler中是正确的
        if(absSeq > 0 )
            _reassembler.push_substring(payload.copy(), absSeq-1, !EOF_IS_TRUE);
        // 特殊情况 syn=true且payload中存着数据时，将该数据以index为0的值组装
        else if ( header.syn && absSeq == 0 && payload.size()> 0)
        {
            _reassembler.push_substring(payload.copy(), 0, !EOF_IS_TRUE);
        }

        _ackNo = _reassembler.stream_out().bytes_written()+1;
        // 输入结束时ack需要加1
        if(_reassembler.stream_out().input_ended())
            _ackNo++;
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {

    // 还没收到syn 则返回空
    if (_synFlag == false){
        return std::optional<WrappingInt32> ();
    }
    //
    WrappingInt32 ackNo = wrap(_ackNo, _isn);
    return optional<WrappingInt32>(ackNo);

}

size_t TCPReceiver::window_size() const {
    uint64_t remainingCap = _reassembler.stream_out().remaining_capacity();
    //uint64_t unAssembleSize = _reassembler.unassembled_bytes();wsat
    return remainingCap ;
}
