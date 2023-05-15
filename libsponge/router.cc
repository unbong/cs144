#include "router.hh"

#include <iostream>
#include <string>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";

    // Your code here.

    IpTableItem item;
    item.route_prefix = route_prefix;
    item.prefix_length = prefix_length;
    item.next_hop = next_hop;
    item.interface_num = interface_num;

    _iptables.push_back(item);
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    // Your code here.

    IPv4Header & header = dgram.header();
    // ttl 检查 如果等于0 或 减1后等于0 丢弃
    uint8_t ttl = header.ttl;
    if(ttl == 0 || ttl-1 == 0)
    {
        return;
    }
    ttl--;

    //匹配
    IpTableItem target{};

    match(header.dst, target);


    // 如果找到匹配的则更新ttl后转发
    header.ttl = ttl;
    Address next_hop (target.next_hop->ip(), target.next_hop->port());
    if(!target.next_hop.has_value()) {
        next_hop = Address::from_ipv4_numeric(header.dst);
    };
    interface(target.interface_num).send_datagram(dgram,next_hop );
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}


int Router::match(uint32_t dst, Router::IpTableItem &tableItem) {

    int longest_length = 0;
    // 找到匹配
    for(auto iter = _iptables.begin(); iter != _iptables.end(); iter++)
    {

        // save default route
        if(iter->route_prefix == 0) {
            tableItem = *iter;
        }
        // 1000...0000 -> 1111...0000
        int and_mask = 0xFFFFFFFF;
        if(iter->prefix_length <= 32 && iter->prefix_length >0  ) {

            uint8_t right_shift = iter->prefix_length -1;
            and_mask = RIGHT_MOVE_BASE >> right_shift;
        }

        // 1010...1111 & 1111...0000 ->  1010...0000
        uint32_t prefix = static_cast<uint32_t> (iter->route_prefix & and_mask);
        // 1010...1010 xor 1010...0000 -> 0000... 1010
        uint32_t xor_res = dst ^ prefix;
        // 将xor结果用and_mask 过滤后 如果为0则说明匹配了
        if( (xor_res & and_mask) == 0)
        {
            if(iter->prefix_length > longest_length) {
                longest_length = iter->prefix_length;
                tableItem = *iter;
            }
        }
    }

    return 0;
}
