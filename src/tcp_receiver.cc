#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  // Your code here.
  if ( message.RST )
    this->reassembler_.reader().set_error();
  if (message.FIN)
    this->reassembler_.close();
  if ( message.SYN )
    ISN = message.seqno;
  else if (!ackno.has_value())
    return;
  else
    this->reassembler_.insert(
      message.seqno.unwrap( ISN, absolute_seqno ) - 1,
      message.payload,
      message.FIN );
  absolute_seqno += message.sequence_length();
  ackno = ackno.value_or(ISN) + message.sequence_length();
}

TCPReceiverMessage TCPReceiver::send() const
{
  // 发送ackno
  auto window_size = this->reassembler_.reader().getCapacity();
  return { ackno,
           (window_size > UINT16_MAX) ? uint16_t (UINT16_MAX) : static_cast<uint16_t>(window_size),
           this->reassembler_.reader().has_error()};
}
