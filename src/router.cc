#include "router.hh"

#include <iostream>
#include <limits>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  route_table.emplace_back( route_prefix, prefix_length, next_hop, interface_num );
}

uint32_t Router::find_next_ip(uint32_t src_ip)
{
  auto best_route = route_table[0];
  for (auto route : route_table)
  {
    auto prefix_len = route.prefix_length;
    if ((src_ip & (1 << prefix_len)) == route.route_prefix && best_route.prefix_length <= prefix_len)
    {
      best_route = route;
    }
  }
  return best_route.route_prefix;
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
// 遍历这个路由器得到的包，并转发出去
// 路由器通常具有多个网络接口，比如WAN、LAN
void Router::route()
{
  for (auto &interface_ptr : this->_interfaces)
  {
    // 每个网络接口都有自己的ip和MAC地址，我们需要帮它找到下一个传输的ip，然后把所有的信息继续传递
    auto network_interface = *interface_ptr;
    auto &datagrams = network_interface.datagrams_received();
    while (!datagrams.empty())
    {
      auto &packet = datagrams.front();
      auto dst_ip = packet.header.dst;

      // 找到下一跳ip
      auto next_hop_ip = find_next_ip(dst_ip);
      network_interface.send_datagram( packet, Address::from_ipv4_numeric( next_hop_ip ) );

      datagrams.pop();
    }
  }
}
