#include "tcp_sender.hh"
#include "tcp_config.hh"

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return seqno_.getRawValue() - ackno_.getRawValue();
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return retrans_cnt;
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
    auto push_pos = seqno_.getRawValue() - ackno_.getRawValue(); // 从buffer的哪里开始push
    auto push_num = min( static_cast<uint64_t>(window_size_), this->reader().bytes_buffered() ) -
                    sequence_numbers_in_flight(); // push的数量
    if (push_num == 0) // 这里不能用make_empty_message方法，因为这里也是用的transmit，所以无需考虑
      return;
    transmit( { seqno_, false,
                string(this->input_.reader().peek().substr(push_pos, push_num)), false, false } );
    outstanding_segments.emplace( seqno_, string( this->input_.reader().peek().substr( push_pos, push_num ) ) );
    seqno_ = seqno_ + push_num;
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  return { seqno_, false, "", false, false};
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // receive后，测试的调用会自动触发push
  if (msg.ackno.has_value()) {
    auto pop_num = min( static_cast<uint64_t>(msg.ackno->getRawValue() - ackno_.getRawValue()),
                        this->reader().bytes_buffered() );
    this->input_.reader().pop(pop_num);
    ackno_ = msg.ackno.value();

    // 当前ackno已经超过了之前的
    while (!outstanding_segments.empty() && ackno_.getRawValue() > outstanding_segments.front().first.getRawValue())
      outstanding_segments.pop();
  }
  window_size_ = msg.window_size;
  retrans_cnt = 0;
  retrans_timer = 0;
  retrans_RTO = initial_RTO_ms_;
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  // retransmit ackno_ to seqno_ - 1
  retrans_timer += ms_since_last_tick;
  if ( retrans_timer >= retrans_RTO ) {
    if (window_size_ != 0)
      retrans_RTO <<= 1;
    if ( ackno_ == isn_ )
      transmit( { isn_, true, "", false, false } );
    else {
      if (!outstanding_segments.empty())
        transmit( { outstanding_segments.front().first, false,
                  outstanding_segments.front().second, false, false } );
    }
    retrans_timer = 0;
    retrans_cnt++;
  }
}