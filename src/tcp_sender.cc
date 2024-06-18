#include "tcp_sender.hh"
#include "tcp_config.hh"

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return seqno_.getRawValue() - ackno_.getRawValue();
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  // Your code here.
  return {};
}

void TCPSender::push( const TransmitFunction& transmit )
{
  if (window_size_ == TCPConfig::MAX_PAYLOAD_SIZE + 1) {
    transmit( { isn_, true, "", false, false } );
    ackno_ = isn_;
    seqno_ = isn_ + 1;
    window_size_ = 1;
  }
  else if (this->input_.reader().bytes_buffered())
  {
    transmit( { seqno_, false, string(this->input_.reader().peek()), false, false } );
    seqno_ = seqno_ + this->input_.reader().bytes_buffered();
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  // Your code here.
  return { seqno_, false, "", false, false};
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if (msg.ackno.has_value()) {
    auto pop_num = min( static_cast<uint64_t>(msg.ackno->getRawValue() - ackno_.getRawValue()),
                        this->reader().bytes_buffered() );
    this->input_.reader().pop(pop_num);
    ackno_ = msg.ackno.value();
  }
  window_size_ = msg.window_size;
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  // Your code here.
  (void)ms_since_last_tick;
  (void)transmit;
  (void)initial_RTO_ms_;
}
