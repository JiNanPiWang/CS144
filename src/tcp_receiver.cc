#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  if ( message.RST )
    this->reassembler_.reader().set_error();
  if (message.FIN)
    this->reassembler_.close();
  if ( message.SYN )
    ISN = message.seqno;
  else if (!ackno_base.has_value())
    return;
  // 应该是seqno位置-当前位置 > this->reassembler_.writer().available_capacity(), UINT16_MAX
  // 但是比较难实现，下面是无奈之举，简单判断是不是在ISN之前
  else if (message.seqno.unwrap(ISN, absolute_seqno) <=
            ISN.unwrap(ISN, absolute_seqno))
    return;
  else
    this->reassembler_.insert(
      message.seqno.unwrap( ISN, absolute_seqno ) - 1,
      message.payload,
      message.FIN );
  absolute_seqno += message.sequence_length();
  ackno_base = ackno_base.value_or(ISN) + message.SYN + message.FIN;
}

TCPReceiverMessage TCPReceiver::send() const
{
  // 发送ackno
  auto window_size = this->reassembler_.writer().available_capacity();

  // window: available capacity in the output ByteStream
  // ackno: reassembler_.writer().bytes_pushed()，写成功了了几个，ackno就是几，再加ISN等
  auto pushedB = reassembler_.writer().bytes_pushed();
  return { ackno_base.has_value() ? optional<Wrap32>( ackno_base.value() + pushedB) : nullopt,
           (window_size > UINT16_MAX) ? uint16_t (UINT16_MAX) : static_cast<uint16_t>(window_size),
           this->reassembler_.reader().has_error()};
}
