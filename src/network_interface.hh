#pragma once

#include <queue>
#include <unordered_map>

#include "address.hh"
#include "ethernet_frame.hh"
#include "ipv4_datagram.hh"
#include "arp_message.hh"

// A "network interface" that connects IP (the internet layer, or network layer)
// with Ethernet (the network access layer, or link layer).

// This module is the lowest layer of a TCP/IP stack
// (connecting IP with the lower-layer network protocol,
// e.g. Ethernet). But the same module is also used repeatedly
// as part of a router: a router generally has many network
// interfaces, and the router's job is to route Internet datagrams
// between the different interfaces.

// The network interface translates datagrams (coming from the
// "customer," e.g. a TCP/IP stack or router) into Ethernet
// frames. To fill in the Ethernet destination address, it looks up
// the Ethernet address of the next IP hop of each datagram, making
// requests with the [Address Resolution Protocol](\ref rfc::rfc826).
// In the opposite direction, the network interface accepts Ethernet
// frames, checks if they are intended for it, and if so, processes
// the the payload depending on its type. If it's an IPv4 datagram,
// the network interface passes it up the stack. If it's an ARP
// request or reply, the network interface processes the frame
// and learns or replies as necessary.
class NetworkInterface
{
public:
  // An abstraction for the physical output port where the NetworkInterface sends Ethernet frames
  class OutputPort
  {
  public:
    virtual void transmit( const NetworkInterface& sender, const EthernetFrame& frame ) = 0;
    virtual ~OutputPort() = default;
  };

  // Construct a network interface with given Ethernet (network-access-layer) and IP (internet-layer)
  // addresses
  NetworkInterface( std::string_view name,
                    std::shared_ptr<OutputPort> port,
                    const EthernetAddress& ethernet_address,
                    const Address& ip_address );

  // Sends an Internet datagram, encapsulated in an Ethernet frame (if it knows the Ethernet destination
  // address). Will need to use [ARP](\ref rfc::rfc826) to look up the Ethernet destination address for the next
  // hop. Sending is accomplished by calling `transmit()` (a member variable) on the frame.
  void send_datagram( const InternetDatagram& dgram, const Address& next_hop );

  // Receives an Ethernet frame and responds appropriately.
  // If type is IPv4, pushes the datagram to the datagrams_in queue.
  // If type is ARP request, learn a mapping from the "sender" fields, and send an ARP reply.
  // If type is ARP reply, learn a mapping from the "sender" fields.
  void recv_frame( const EthernetFrame& frame );

  // Called periodically when time elapses
  void tick( size_t ms_since_last_tick );

  // Accessors
  const std::string& name() const { return name_; }
  const OutputPort& output() const { return *port_; }
  OutputPort& output() { return *port_; }
  std::queue<InternetDatagram>& datagrams_received() { return datagrams_received_; }
  static ARPMessage make_arp_fram(uint16_t _opcode,
                             EthernetAddress _sender_ethernet_address, uint32_t _sender_ip_address,
                             EthernetAddress _target_ethernet_address, uint32_t _target_ip_address);
  static EthernetFrame make_eth_fram_head(EthernetAddress _src, EthernetAddress _dst, uint16_t _type);

  // 使用 std::void_t 来检查类型是否有 serialize 函数
  template <typename T, typename = void>
  struct has_serialize_function : std::false_type {};

  // std::declval主要用途是生成一个类型 T 的右值引用（Rvalue reference），但不实际创建任何对象。
  // decltype(std::declval<T>().serialize()) 获取的是 T 类型的 serialize 成员函数的类型。
  // std::void_t 是一个 C++17 中的工具模板，它接受任意数量的模板参数，并且对每个参数都返回 void。
  // 如果 serialize 成员函数存在且可访问，则这里的 void_t 将返回 void；
  // 否则，它会导致 SFINAE（Substitution Failure Is Not An Error）机制导致模板参数推断失败。
  template <typename T>
  struct has_serialize_function<T, std::void_t<decltype(std::declval<T>().serialize())>> : std::true_type {};


private:
  // Human-readable name of the interface
  std::string name_;

  // The physical output port (+ a helper function `transmit` that uses it to send an Ethernet frame)
  std::shared_ptr<OutputPort> port_;
  void transmit( const EthernetFrame& frame ) const { port_->transmit( *this, frame ); }

  // Ethernet (known as hardware, network-access-layer, or link-layer) address of the interface
  EthernetAddress ethernet_address_;

  // IP (known as internet-layer or network-layer) address of the interface
  Address ip_address_;

  // Datagrams that have been received
  std::queue<InternetDatagram> datagrams_received_ {};

    // 一个arp信息应该同时记录last_update_time
  std::unordered_map<uint32_t, std::pair<EthernetAddress, size_t>> arp_table {};

  const uint16_t ARP_REQUEST_COOL_DOWN = 5 * 1000;
  const uint16_t ARP_MAPPING_EXPIRATION = 30 * 1000;
  std::unordered_map<uint32_t, size_t> last_arp_request_time {};

  size_t current_time {};

  struct failed_messages
  {
    const InternetDatagram dgram;
    const Address next_hop;
  };
  std::unordered_multimap<uint32_t, failed_messages> failed_messages_mmap {};
};
