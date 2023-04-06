#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;


StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {
}

void StreamReassembler::calculatePayload(size_t index,size_t  size, size_t &payload) {

    payload = size ;
    size_t left = 0;
    size_t right = 0;
    for(auto iter = _unAssemblePayloads.begin(); iter != _unAssemblePayloads.end(); iter++)
    {
        // ---111-----1111
        //     i        s
        left = max(index, iter->first);
        right = min((index+size), (iter->first+iter->second));
        if(left < right)
            payload-=(right-left);
    }
}



//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {


    // 如果新数据已被组装，则返回
    if (_expectedIndex > (index + data.size()))
        return;
    // 有效载荷
    size_t payLoadSize =0;
    // 计算有效载荷 通过循环map来找到要插入数据的有效载荷
    calculatePayload(index, data.size(), payLoadSize);
    _unAssembledByteSize+= payLoadSize;

    // 将数据插入到以index为排序的优先队列中
    Segment seg (index, data, eof,payLoadSize);
    _unAssembledSegments.push(seg);
    Segment toAssembleSeg =  _unAssembledSegments.top() ;

    // 将数据插入到map中
    auto iter = _unAssemblePayloads.find(index);
    if( (iter != _unAssemblePayloads.end() && iter->second < data.size()) ||
        iter == _unAssemblePayloads.end())
    {
        _unAssemblePayloads.insert_or_assign(index, data.size());
    }

    // 循环便利优先队列
    while(_expectedIndex >= toAssembleSeg._index
           && !_unAssembledSegments.empty())
    {
        // 如果是已被组装了的数据， 则忽略
        if (_expectedIndex > (toAssembleSeg._index + toAssembleSeg._data.size()))
        {
            _unAssemblePayloads.erase(toAssembleSeg._index);
            _unAssembledByteSize -= toAssembleSeg._payLoadSize;
            _unAssembledSegments.pop() ;
            toAssembleSeg =  _unAssembledSegments.top() ;
            continue ;
        }

        // 写入到字节流中的正确的位置
        uint64_t pos = _expectedIndex-toAssembleSeg._index;
        _output.write(toAssembleSeg._data.substr(pos));
        // 更新预期字符串的索引
        _expectedIndex = _output.bytes_written();
        // 更新未被组装的报文的字节数
        _unAssembledByteSize -= toAssembleSeg._payLoadSize;

        // 如果是EOF且已写入的字节的字节为最后一个时 指定写入结束
        if( toAssembleSeg._eof && _output.bytes_written() == (toAssembleSeg._index+toAssembleSeg._data.size() )) _output.end_input();

        //map更新
        _unAssemblePayloads.erase(toAssembleSeg._index);
        // 优先队列中删除第一项
        _unAssembledSegments.pop() ;
        toAssembleSeg =  _unAssembledSegments.top() ;

    }
}

size_t StreamReassembler::unassembled_bytes() const { return _unAssembledByteSize; }

bool StreamReassembler::empty() const { return _unAssembledByteSize == 0; }




