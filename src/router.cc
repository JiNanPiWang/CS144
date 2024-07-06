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

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
// 遍历这个路由器得到的包，并转发出去
// 路由器通常具有多个网络接口，比如WAN、LAN
void Router::route()
{
  bool all_messages_done = true;
  do
  {
    all_messages_done = true;
    for ( auto& interface_ptr : this->_interfaces ) {
      // 每个网络接口都有自己的ip和MAC地址，我们需要帮它找到下一个传输的ip，然后把所有的信息继续传递
      // 路由器直接把这条消息”给“对应的网络接口
      auto &network_interface = *interface_ptr;
      auto &datagrams = network_interface.datagrams_received();
      if ( datagrams.empty() )
        continue;
      while (!datagrams.empty())
      {
        auto datagram = datagrams.front();
        all_messages_done = false;
        auto next_interface_route = find_next_interface( datagram.header.dst );
        auto &next_interface = *interface( next_interface_route.interface_num );

        // TODO：特殊处理id=0的default_router
        // TODO：处理next hop


        if (next_interface.name() != network_interface.name()) // 如果不在我们的网段内，我们就直接”给“下一个路由器
        {
          datagram.header.ttl--;
          if (next_interface_route.next_hop.has_value())
          {
            next_interface.send_datagram( datagram,
                                          Address::from_ipv4_numeric(
                                            next_interface_route.next_hop.value().ipv4_numeric()) );
          }
          else
          {
            next_interface.datagrams_received().push( datagram );
          }
        }
        else // 如果在我们的网段内，我们就直接发送
          next_interface.send_datagram( datagram, Address::from_ipv4_numeric(datagram.header.dst) );

        // network_interfacsend_datagram( datagram, Address::from_ipv4_numeric(next_ip) );
        datagrams.pop();
      }
      if (all_messages_done)
        break;
    }
  } while (!all_messages_done);
}

Router::next_route Router::find_next_interface(uint32_t dst_ip)
{
  auto best_route = route_table[0];
  for (auto route : route_table)
  {
    auto prefix_len = route.prefix_length;
    uint32_t mask = UINT32_MAX - ((1ULL << (32 - prefix_len)) - 1);
    if ((dst_ip & (mask)) == route.route_prefix && best_route.prefix_length <= prefix_len)
    {
      best_route = route;
    }
  }
  return best_route;
}