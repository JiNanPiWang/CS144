#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  // Your code here.
  if ( message.RST )
    this->reassembler_.reader().set_error();
  if ( message.SYN )
    ISN = message.seqno;
  else
    this->reassembler_.insert(
                        message.seqno.unwrap( ISN, absolute_seqno ) - 1,
                        message.payload,
                        message.FIN );
  absolute_seqno += message.sequence_length();
}

TCPReceiverMessage TCPReceiver::send() const
{
  // 发送ackno
  return { absolute_seqno == 0 ? std::nullopt : std::optional<Wrap32>(Wrap32(absolute_seqno)),
           static_cast<uint16_t>(this->reassembler_.reader().getCapacity()),
           this->reassembler_.reader().has_error()};
}
