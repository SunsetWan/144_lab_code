#include "router.hh"

#include <iostream>

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

    
    RouteEntry newEntry {route_prefix, prefix_length, next_hop, interface_num};
    forwardingTable.push_back(newEntry);
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {

    // If the TTL was zero already, 
    // or hits zero after the decrement, 
    // the router should drop the datagram.
    if (dgram.header().ttl <= 1) {
        return;
    }

    // The router decrements the datagram’s TTL (time to live).
    dgram.header().ttl -= 1;

    RouteEntry entry;
    bool isFound = false;
    uint32_t targetIPAddr = dgram.header().dst;
    
    // Look route up in forwardingTable
    // The time complexity of linear searching is O(n).
    for (size_t i = 0; i < forwardingTable.size(); i++) {
        if (is_prefix_equal(targetIPAddr, forwardingTable[i].route_prefix, forwardingTable[i].prefix_length)) {
            if (!isFound || entry.prefix_length < forwardingTable[i].prefix_length) {
                // Need to find the longest prefix match!
                entry = forwardingTable[i];
                isFound = true;
            }
        }
    }

    if (!isFound) {
        // If no routes matched, the router drops the datagram.
        return;
    }

    if (entry.next_hop.has_value()) {
        _interfaces[entry.network_interface_number].send_datagram(dgram, entry.next_hop.value());
    } else {
        // If the router is directly attached to the network in question, 
        // the next hop will be an empty optional. 
        // In that case, the next hop is the datagram’s destination address.
        Address targetAddr = Address::from_ipv4_numeric(dgram.header().dst);
        _interfaces[entry.network_interface_number].send_datagram(dgram, targetAddr);
    }
}



bool Router::is_prefix_equal(uint32_t targetIPAddr, uint32_t route_prefix, uint8_t prefix_length) {
    // Recall that in C and C++, 
    // it can produce undeﬁned behavior to shift a 32-bit integer by 32 bits.
    // special judge right when shift 32 bit
    uint32_t offset = (prefix_length == 0) ? 0 : 0xffffffff << (32 - prefix_length);
    return (targetIPAddr & offset) == (route_prefix & offset);
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
