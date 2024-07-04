#include <iostream>

#include "arp_message.hh"
#include "exception.hh"
#include "network_interface.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  EthernetFrame efram;

  // 如果这个arp对应的MAC地址超过了30秒，就重新ARP
  if (arp_table.contains(next_hop.ipv4_numeric()) &&
       current_time - arp_table[next_hop.ipv4_numeric()].second > ARP_MAPPING_EXPIRATION)
    arp_table.erase( next_hop.ipv4_numeric() );

  // 如果对应ip找不到MAC地址，那就进行arp
  if (!arp_table.contains(next_hop.ipv4_numeric()))
  {
    failed_messages_mmap.emplace(next_hop.ipv4_numeric(), failed_messages(dgram, next_hop));
    // 下面决定是否需要发送arp
    // 如果arp表没有就发，如果arp表有但是超过了时限也发
    if (!last_arp_request_time.contains(next_hop.ipv4_numeric()))
      last_arp_request_time[next_hop.ipv4_numeric()] = current_time;
    else if (current_time - last_arp_request_time[next_hop.ipv4_numeric()] > ARP_REQUEST_COOL_DOWN)
      ;
    else
      return;

    // 发送的是广播地址
    efram = NetworkInterface::make_eth_fram_head(this->ethernet_address_,
                                                  ETHERNET_BROADCAST,
                                                  EthernetHeader::TYPE_ARP);

    // target_ethernet_address默认全0，ARP 请求的目的是发现目标设备的MAC地址，但实际上并不知道目标设备的MAC地址
    ARPMessage arp_fram = NetworkInterface::make_arp_fram(ARPMessage::OPCODE_REQUEST,
                                                           this->ethernet_address_,
                                                           this->ip_address_.ipv4_numeric(),
                                                           {} ,
                                                           next_hop.ipv4_numeric());
    efram.payload = serialize( arp_fram );
    transmit( efram );
  }
  else // 找到了MAC，直接发送
  {
    efram = NetworkInterface::make_eth_fram_head(this->ethernet_address_,
                                                  arp_table[next_hop.ipv4_numeric()].first,
                                                  EthernetHeader::TYPE_IPv4);
    efram.payload = serialize( dgram );
    transmit( efram );
  }
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  EthernetFrame efram;

  if (frame.header.type == EthernetHeader::TYPE_ARP) // 收到ARP
  {
    Parser parser{frame.payload}; // parser使用payload初始化
    ARPMessage arp_fram_recved;
    arp_fram_recved.parse( parser ); // ARPMessage的parse函数是把内容解析到该message里面

    // arp是通过ip找MAC，如果发来的ip不是找我们的，那就不管
    if (arp_fram_recved.target_ip_address != this->ip_address_.ipv4_numeric())
      return;

    if (arp_fram_recved.opcode == ARPMessage::OPCODE_REQUEST) // 需要我们返回MAC
    {
      // 回复自己的MAC地址
      ARPMessage arp_to_send = make_arp_fram(ARPMessage::OPCODE_REPLY,
                                              this->ethernet_address_,
                                              this->ip_address_.ipv4_numeric(),
                                              arp_fram_recved.sender_ethernet_address,
                                              arp_fram_recved.sender_ip_address);
      efram = NetworkInterface::make_eth_fram_head(this->ethernet_address_,
                                                    arp_fram_recved.sender_ethernet_address,
                                                    EthernetHeader::TYPE_ARP );
      efram.payload = serialize( arp_to_send );
      transmit( efram );

      // 收到对方的arp，我们也更新arp表
      arp_table[arp_fram_recved.sender_ip_address] = { arp_fram_recved.sender_ethernet_address, current_time };
    }
    else if (arp_fram_recved.opcode == ARPMessage::OPCODE_REPLY)  // 得到了对方的MAC
    {
      auto new_ip = arp_fram_recved.sender_ip_address;
      arp_table[new_ip] = { arp_fram_recved.sender_ethernet_address, current_time };
      auto range = failed_messages_mmap.equal_range( new_ip );
      vector<failed_messages> message_to_resend;
      for (auto it = range.first; it != range.second; ++it)
      {
        message_to_resend.emplace_back(it->second);
      }
      failed_messages_mmap.erase( new_ip );
      for (auto &mess : message_to_resend)
        send_datagram( mess.dgram, mess.next_hop );
    }
  }
  else if (frame.header.type == EthernetHeader::TYPE_IPv4) // 收到IP报
  {
    if (frame.header.dst != this->ethernet_address_)
      return;
    Parser parser{frame.payload};
    IPv4Datagram ip_fram_recved;
    ip_fram_recved.parse( parser );
    this->datagrams_received_.push(ip_fram_recved);
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  current_time += ms_since_last_tick;
}

EthernetFrame NetworkInterface::make_eth_fram_head(EthernetAddress _src, EthernetAddress _dst, uint16_t _type)
{
  EthernetFrame ethernetFrame;
  ethernetFrame.header.src = _src;
  ethernetFrame.header.dst = _dst;
  ethernetFrame.header.type = _type;
  return ethernetFrame;
}

ARPMessage NetworkInterface::make_arp_fram(uint16_t _opcode,
                           EthernetAddress _sender_ethernet_address, uint32_t _sender_ip_address,
                           EthernetAddress _target_ethernet_address, uint32_t _target_ip_address)
{
  ARPMessage arp_fram;
  arp_fram.opcode = _opcode;
  arp_fram.sender_ethernet_address = _sender_ethernet_address;
  arp_fram.sender_ip_address = _sender_ip_address;
  arp_fram.target_ethernet_address = _target_ethernet_address;
  arp_fram.target_ip_address = _target_ip_address;
  return arp_fram;
}

template<typename T>
requires NetworkInterface::has_serialize_function<T>::value
auto serialize(T &frame)
{
  Serializer payload_seri{};
  frame.serialize( payload_seri );
  return payload_seri.output();
}