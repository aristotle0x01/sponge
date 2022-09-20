#include "router.hh"

#include <iostream>

using namespace std;

// implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

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

    auto it = _route_map.find(prefix_length);
    if (it != _route_map.end()) {
        _route_map[prefix_length][route_prefix] = {next_hop, interface_num};
    } else {
        std::map<uint32_t, std::pair<optional<Address>, size_t>> route_map{};
        route_map[route_prefix] = {next_hop, interface_num};
        _route_map[prefix_length] = route_map;
    }
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    if (dgram.header().ttl <= 1) {
        return;
    }
    dgram.header().ttl = dgram.header().ttl - 1;

    uint32_t dst = dgram.header().dst;

    auto it = _route_map.rbegin();
    while (it != _route_map.rend()) {
        uint8_t prefix_length = it->first;
        auto mappings = it->second;

        uint8_t shift_length = 32 - prefix_length;

        auto it_0 = mappings.begin();
        while (it_0 != mappings.end()) {
            uint32_t route_prefix = it_0->first;
            optional<Address> next_hop = it_0->second.first;
            size_t interface_num = it_0->second.second;

            it_0++;

            // https://stackoverflow.com/questions/7401888/why-doesnt-left-bit-shift-for-32-bit-integers-work-as-expected-when-used
            // In C++, shift is only well-defined if you shift a value less steps than the size
            // of the type. If int is 32 bits, then only 0 to, and including, 31 steps is well-defined.
            uint32_t r = static_cast<uint64_t>(dst ^ route_prefix) >> shift_length;
            if (r != 0) {
                continue;
            }

            if (next_hop.has_value()) {
                interface(interface_num).send_datagram(dgram, next_hop.value());
            } else {
                interface(interface_num).send_datagram(dgram, Address::from_ipv4_numeric(dst));
            }
            return;
        }

        it++;
    }
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
