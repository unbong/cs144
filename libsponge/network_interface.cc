#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    // 如果在缓存中找到对应物理地址
    auto search = _ip_mac_caches.find(next_hop_ip);
    if(search != _ip_mac_caches.end())
    {
        EthernetAddress ea = search ->second.ethernetAddress;
        EthernetHeader eh;
        eh.dst = ea;
        eh.src = _ethernet_address;
        eh.type = EthernetHeader::TYPE_IPv4;

        EthernetFrame ef ;
        ef.header() = eh;
        ef.payload() = dgram.serialize();
        _frames_out.push(ef);
    }
    // 在缓存中没有找到对应物理地址，则准备arp消息，并发送
    else
    {
        EthernetHeader eh;
        eh.dst = ETHERNET_BROADCAST;
        eh.src = _ethernet_address;
        eh.type =  EthernetHeader::TYPE_ARP;

        ARPMessage am ;
        am.opcode = ARPMessage::OPCODE_REQUEST;
        am.sender_ethernet_address = _ethernet_address;
        am.sender_ip_address = _ip_address.ipv4_numeric();
        am.target_ethernet_address = {0,0,0,0,0,0};
        am.target_ip_address = next_hop_ip;

        EthernetFrame ef;
        ef.header() = eh;
        ef.payload() = am.serialize();
        //  如果过去5秒内发送过相同的arp则停止发送
        auto is_sended_in_5sec_iter = _send_in_5sec.find(next_hop_ip);
        if (is_sended_in_5sec_iter == _send_in_5sec.end()){
            _frames_out.push(ef);
            _send_in_5sec.insert_or_assign(next_hop_ip, _since_last_tick);
        }
        else if ( (_since_last_tick > is_sended_in_5sec_iter->second ) &&
                 (_since_last_tick - is_sended_in_5sec_iter->second) >= _FIVE_SECOND)
        {
            _frames_out.push(ef);
            _send_in_5sec.insert_or_assign(next_hop_ip, _since_last_tick);
        }
        else if( (_since_last_tick < is_sended_in_5sec_iter->second ) &&
                 (_since_last_tick +( UINT64_MAX -  is_sended_in_5sec_iter->second )) >= _FIVE_SECOND)
        {
            _frames_out.push(ef);
            _send_in_5sec.insert_or_assign(next_hop_ip, _since_last_tick);
        }

        // 将没有发送出去的报文缓存起来
        _cache.push_back({next_hop_ip, dgram });
    }

}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {

    EthernetHeader eh = frame.header();
    if(eh.type == EthernetHeader::TYPE_IPv4)
    {
        // 如果不是我要接收的数据则无视
        if(eh.dst != _ethernet_address ) return {};

        InternetDatagram  iDataGram;
        ParseResult res = iDataGram.parse(frame.payload());
        if(res == ParseResult::NoError){
            cerr<< "no err" << endl;
            return optional<InternetDatagram>(iDataGram);
        }
        return {};
    }
    else if(eh.type == EthernetHeader::TYPE_ARP){
        // 如果是arp
        ARPMessage arpMes ;
        ParseResult res = arpMes.parse(frame.payload());
        if(res == ParseResult::NoError)
        {
            // 将物理地址与ip地址进行映射
            uint32_t src_ip = arpMes.sender_ip_address;
            EthernetAddress src_ether = arpMes.sender_ethernet_address;
            EthAddr_Tick et{src_ether, _since_last_tick};
            _ip_mac_caches.insert_or_assign(src_ip, et);

            bool isInCache = false;
            auto search = _ip_mac_caches.find(arpMes.target_ip_address);
            // 如果在缓存中存在，则将缓存中的mac与ip返回
            uint32_t tar_ip_addr ;
            EthernetAddress tar_eth_addr;
            if(search != _ip_mac_caches.end()){
                isInCache = true;
                tar_eth_addr = search->second.ethernetAddress;
                tar_ip_addr = search->first;
            }
            else
            {
                tar_eth_addr = _ethernet_address;
                tar_ip_addr = _ip_address.ipv4_numeric();

            }

            // arp type request 则回复arp请求
            uint32_t ip_addr = arpMes.sender_ip_address;
            if (arpMes.opcode == ARPMessage::OPCODE_REQUEST)
            {

                // 如果dst不是我, 且 在缓存里没有 则无视
                if( !isInCache && arpMes.target_ip_address != _ip_address.ipv4_numeric() ) return{};

                EthernetAddress  ea = arpMes.sender_ethernet_address;
                arpMes.sender_ethernet_address = tar_eth_addr;
                arpMes.sender_ip_address = tar_ip_addr;
                arpMes.target_ip_address =ip_addr;
                arpMes.target_ethernet_address = ea;
                arpMes.opcode = ARPMessage::OPCODE_REPLY ;

                EthernetHeader reply_eh;
                reply_eh.type = eh.type;
                reply_eh.src = _ethernet_address;
                reply_eh.dst = eh.src;

                EthernetFrame ef ;
                ef.header() = reply_eh;
                ef.payload() = arpMes.serialize();

                _frames_out.push(ef);
            }

            else if(arpMes.opcode == ARPMessage::OPCODE_REPLY)
            {
                _send_in_5sec.erase(ip_addr);

                for(auto iter = _cache.begin(); iter != _cache.end(); )
                {
                    if(iter->first == ip_addr)
                    {
                        //
                        EthernetHeader header;
                        header.dst = arpMes.sender_ethernet_address;
                        header.src = _ethernet_address;
                        header.type = EthernetHeader::TYPE_IPv4;

                        EthernetFrame ef ;
                        ef.header() = header;
                        ef.payload() = iter->second.serialize();
                        _frames_out.push(ef);
                        iter = _cache.erase(iter);
                    }
                    else
                        iter++;
                }
            }
        }
    }

    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    _since_last_tick +=ms_since_last_tick;

    for(auto iter = _ip_mac_caches.begin(); iter!= _ip_mac_caches.end();)
    {

        if((_since_last_tick - iter->second.tick) >= _THIRTY_SECOND ){
            iter = _ip_mac_caches.erase(iter);
        }
        else
            iter++;
    }
}
