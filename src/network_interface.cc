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

  if (arp_table.count(dgram.header.src) == 0)
  {
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
    failed_messages_queue.emplace( dgram, next_hop, current_time );
  }
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  EthernetFrame efram;

  if (frame.header.type == EthernetHeader::TYPE_ARP)
  {
    Parser parser{frame.payload}; // parser使用payload初始化
    ARPMessage arp_fram_recved;
    arp_fram_recved.parse( parser ); // ARPMessage的parse函数是把内容解析到该message里面

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
    }
    else // 得到了对方的MAC
    {
      arp_table[arp_fram_recved.sender_ip_address] = arp_fram_recved.sender_ethernet_address;
      auto now_time = current_time;
      while (failed_messages_queue.front().last_attempt_time < now_time)
      {
        if (now_time - failed_messages_queue.front().last_attempt_time > 5 * 1000) // 5秒内mei发过就不发
        {
          send_datagram(failed_messages_queue.front().dgram, failed_messages_queue.front().next_hop);
          failed_messages_queue.pop();
        }
        else
        {
          failed_messages_queue.push(failed_messages_queue.front());
          failed_messages_queue.pop();
        }
      }
    }
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