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
    EthernetFrame frame; // which will be sent.
    frame.header().type = EthernetHeader::TYPE_IPv4;
    frame.header().src = _ethernet_address;
    frame.payload() = std::move(dgram.serialize());
    // Still need to fill frame.header().dst
    // Look the dst up in _mapping_table
    if (_mapping_table.count(next_hop_ip) && _timer <= _mapping_table[next_hop_ip].time_to_live_value) {
        // Set dst
        frame.header().dst = _mapping_table[next_hop_ip].macAddr;
        // Send frame
        _frames_out.push(frame);
    } else {
        _pending_arg.push(next_hop_ip);
        retransmission_arp_packet();

        WaittingFrame f {frame, next_hop_ip};
        _frame_waitting_queue.push(f);
    }
    
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    // The code should ignore any frames not destined for the network interface 
    // (meaning, the Ethernet destination is either the broadcast address or the interface’s own Ethernet address
    // stored in the ethernet address member variable).
    if (!is_ethernet_addr_equal(frame.header().dst, ETHERNET_BROADCAST) && !is_ethernet_addr_equal(frame.header().dst, _ethernet_address)) {
        return std::nullopt;
    } else if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram ipDatagram;
        // If the inbound frame is IPv4, parse the payload as an InternetDatagram and, 
        // if successful (meaning the parse() method returned ParseResult::NoError), 
        // return the resulting InternetDatagram to the caller.
        if (ipDatagram.parse(frame.payload()) == ParseResult::NoError) {
            return ipDatagram;
        } else
            return std::nullopt;
        }
    } else if(frame.header().type == EthernetHeader::TYPE_ARP) {
        ARPMessage msg;
        if (reply.parse(frame.payload()) == ParseResult::NoError) {
            uint32_t ipAddr = msg.sender_ip_address;

            // Update mapping table
            _mapping_table[ipAddr].macAddr = msg.sender_ethernet_address;

            // Remember the mapping between the sender’s IP address and Ethernet address for 30 seconds.
            _mapping_table[ipAddr].time_to_live_value = _timer + 30 * 1000;

            // This ARPMessage comes here because of me!
            if (msg.opcode == ARPMessage::OPCODE_REQUEST && msg.target_ip_address == _ip_address.ipv4_numeric()) {
                ARPMessage reply;
                reply.opcode = ARPMessage::OPCODE_REPLY;
                reply.sender_ethernet_address = _ethernet_address;
                reply.sender_ip_address = _ip_address.ipv4_numeric();
                reply.target_ethernet_address = msg.sender_ethernet_address;
                reply.target_ip_address = msg.sender_ip_address;

                EthernetFrame arp_frame;
                arp_frame.header().type = EthernetHeader::TYPE_ARP;
                arp_frame.header().src = _ethernet_address;
                arp_frame.header().dst = msg.sender_ethernet_address;
                arp_frame.payload() = move(reply.serialize());
                _frames_out.push(arp_frame);
            }

            
        }
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    _timer += ms_since_last_tick;
    retransmission_arp_packet();
}

bool NetworkInterface::is_ethernet_addr_equal(EthernetAddress addr1, EthernetAddress addr2) {
    // MAC addr is total 48 bits
    for (int i = 0; i < 6; i++) {
        if (addr1[i] != addr2[i]) {
            return false;
        }
    }
    return true;
}

void NetworkInterface::retransmission_arp_packet() {
    if (!_pending_arg.empty()) {
        // You don’t want to ﬂood the network with ARP requests. 
        // If the network interface already sent an ARP request about the same IP address in the last ﬁve seconds, 
        // don’t send a second request—just wait for a reply to the ﬁrst one.
        // Again, queue the datagram until you learn the destination Ethernet address.
        if (!_is_pending_flag || (_is_pending_flag && ((_timer - _pending_timer) > 5000))) {
            uint32_t ipAddr = _pending_arg.front();

            ARPMessage msg;
            msg.opcode = ARPMessage::OPCODE_REQUEST;

            msg.sender_ethernet_address = _ethernet_address;
            msg.sender_ip_address = _ip_address.ipv4_numeric();

            msg.target_ip_address = ipAddr;
            // What we request is the corresponding ethernet addr of ipAddr.
            // So for now, let it be arbitrary.
            msg.target_ethernet_address = {0, 0, 0, 0, 0, 0}; 

            EthernetFrame frame_with_arp_msg_inside;
            frame_with_arp_msg_inside.header().type = EthernetHeader::TYPE_ARP;
            frame_with_arp_msg_inside.header().src = _ethernet_address;
            // We send a broadcast ARP request message (destination FF:FF:FF:FF:FF:FF MAC address), 
            // which is accepted by all computers on the local network, requesting an answer for ipAddr.
            frame_with_arp_msg_inside.header().dst = ETHERNET_BROADCAST;
            frame_with_arp_msg_inside.payload() = std::move(msg.serialize());

            _frames_out.push(frame_with_arp_msg_inside);

            _is_pending_flag = true;
            _pending_timer = _timer;
        }
    }
}
