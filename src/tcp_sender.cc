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
  if (window_size_ == TCPConfig::MAX_PAYLOAD_SIZE + 1)
  {
    transmit( { isn_, true, "", false, false } );
    ackno_ = isn_;
    seqno_ = isn_ + 1;
    window_size_ = 1;
  }
  else if (this->input_.writer().is_closed())
  {
    // 当ByteStream中的所有数据都已读取并发送完毕，并且发送方没有更多的数据需要发送时，同时窗口大小正好大一个。发FIN
    // 上面是错误的。测试会调用close方法，就关闭了
    // 发送window_size_ - sequence_numbers_in_flight() - 1
    auto push_num = window_size_ - sequence_numbers_in_flight() - 1;
    if (push_num > window_size_) // close会发一个空的""默认push一下，这个别管
      return;
    auto str_size = this->input_.reader().peek().size();
    transmit( { seqno_, false,
                string(this->input_.reader().peek().substr(str_size - push_num)), true, false } );
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
    // 大部分情况下ackno不能大于seqno，除非卡在0左右
    auto &new_ackno = msg.ackno.value();
    if (new_ackno.getRawValue() < UINT32_MAX - UINT16_MAX && new_ackno.getRawValue() > seqno_.getRawValue())
      return;
    auto pop_num = min( static_cast<uint64_t>(msg.ackno->getRawValue() - ackno_.getRawValue()),
                        this->reader().bytes_buffered() );
    this->input_.reader().pop(pop_num);
    ackno_ = msg.ackno.value();

    // 当前ackno已经超过了之前的
    while (!outstanding_segments.empty() && ackno_.getRawValue() > outstanding_segments.front().first.getRawValue())
      outstanding_segments.pop();
  }
  if (msg.window_size > window_size_)
    window_expand_ = msg.window_size - window_size_;
  else
    window_expand_ = 0;
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
      if (!outstanding_segments.empty()) {
        transmit( { outstanding_segments.front().first, false,
                    outstanding_segments.front().second, false, false } );
      } else {
        retrans_cnt = 0;
        retrans_RTO = initial_RTO_ms_;
      }
    }
    retrans_timer = 0;
    retrans_cnt++;
  }
}