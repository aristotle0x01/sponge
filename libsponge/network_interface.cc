#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

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

    EthernetFrame frame;
    frame.payload() = dgram.serialize();
    frame.header().src = _ethernet_address;
    frame.header().type = EthernetHeader::TYPE_IPv4;
    auto it = _ip_mac_cache.find(next_hop_ip);
    if (it != _ip_mac_cache.end()) {
        frame.header().dst = _ip_mac_cache[next_hop_ip].second;
        _frames_out.push(frame);
    } else {
        auto it_0 = _frames_need_fill.find(next_hop_ip);
        if (it_0 != _frames_need_fill.end()) {
            _frames_need_fill[next_hop_ip].push_back(frame);
        } else {
            std::list<EthernetFrame> list;
            list.push_back(frame);
            _frames_need_fill[next_hop_ip] = list;
        }

        auto it_1 = _arp_request_in_flight.find(next_hop_ip);
        if (it_1 != _arp_request_in_flight.end()) {
            return;
        }

        ARPMessage arp;
        arp.opcode = ARPMessage::OPCODE_REQUEST;
        arp.sender_ip_address = _ip_address.ipv4_numeric();
        arp.sender_ethernet_address = _ethernet_address;
        arp.target_ip_address = next_hop_ip;
        EthernetFrame arp_frame;
        arp_frame.payload() = arp.serialize();
        arp_frame.header().src = _ethernet_address;
        arp_frame.header().dst = ETHERNET_BROADCAST;
        arp_frame.header().type = EthernetHeader::TYPE_ARP;
        _frames_out.push(arp_frame);

        _arp_request_in_flight[next_hop_ip] = _ms_total_tick;
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if (frame.header().dst != ETHERNET_BROADCAST and frame.header().dst != _ethernet_address) {
        return {};
    }

    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram datagram;
        ParseResult result = datagram.parse(frame.payload());
        if (result == ParseResult::NoError) {
            return optional(datagram);
        }
    } else if (frame.header().type == EthernetHeader::TYPE_ARP) {
        ARPMessage arp;
        ParseResult result = arp.parse(frame.payload());
        if (result != ParseResult::NoError) {
            return {};
        }

        // arp reply
        if (arp.opcode == ARPMessage::OPCODE_REQUEST and arp.target_ip_address == _ip_address.ipv4_numeric()) {
            ARPMessage reply;
            reply.opcode = ARPMessage::OPCODE_REPLY;
            reply.sender_ethernet_address = _ethernet_address;
            reply.sender_ip_address = _ip_address.ipv4_numeric();
            reply.target_ip_address = arp.sender_ip_address;
            reply.target_ethernet_address = arp.sender_ethernet_address;
            EthernetFrame reply_frame;
            reply_frame.payload() = reply.serialize();
            reply_frame.header().src = _ethernet_address;
            reply_frame.header().dst = frame.header().src;
            reply_frame.header().type = EthernetHeader::TYPE_ARP;

            _frames_out.push(reply_frame);
        }

        // reserve mapping
        _ip_mac_cache[arp.sender_ip_address] = {_ms_total_tick, arp.sender_ethernet_address};

        // see if any can be filled in _frames_need_fill
        for (auto it = _frames_need_fill.begin(), next = it; it != _frames_need_fill.end(); it = next) {
            ++next;

            auto it_0 = _ip_mac_cache.find(it->first);
            if (it_0 == _ip_mac_cache.end()) {
                continue;
            }

            EthernetAddress dst_ether_addr = it_0->second.second;
            auto it_1 = it->second.begin();
            while (it_1 != it->second.end()) {
                it_1->header().dst = dst_ether_addr;
                _frames_out.push(*it_1);

                it_1++;
            }

            _frames_need_fill.erase(it);
        }
    }

    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    _ms_total_tick = _ms_total_tick + ms_since_last_tick;

    const size_t gap_30s = 30 * 1000;
    for (auto it = _ip_mac_cache.begin(), next = it; it != _ip_mac_cache.end(); it = next) {
        ++next;
        if (_ms_total_tick >= (it->second.first + gap_30s)) {
            _ip_mac_cache.erase(it);
        }
    }

    const size_t gap_5s = 5 * 1000;
    for (auto it = _arp_request_in_flight.begin(), next = it; it != _arp_request_in_flight.end(); it = next) {
        ++next;
        if (_ms_total_tick >= (it->second + gap_5s)) {
            _arp_request_in_flight.erase(it);
        }
    }
}
