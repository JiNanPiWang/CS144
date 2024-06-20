#include "tcp_sender.hh"
#include "tcp_config.hh"

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return unwrap_seq_num(seqno_) - unwrap_seq_num(ackno_);
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return retrans_cnt;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  while (sequence_numbers_in_flight() < window_size_ || (!had_FIN && this->input_.writer().is_closed()) )
  {
    TCPSenderMessage to_trans { seqno_, false, "", false, false };
    if ( !has_SYN ) // 还没开始，准备SYN
    {
      to_trans.seqno = isn_;
      to_trans.SYN = true;
      ackno_ = isn_;
      seqno_ = isn_ + 1;
      window_size_ = 1;
      has_SYN = true;
    }
    // 已经被关闭了，准备FIN，且有空间发FIN
    if ( this->input_.writer().is_closed() && this->reader().bytes_buffered() < window_size_ )
    {
      // 测试会调用close方法，就关闭了
      // 发送window_size_ - sequence_numbers_in_flight() - 1
      if ( had_FIN ) // 发过了就不发了
        return;

      auto fake_ackno_ = ackno_;
      auto fake_segments = flying_segments;
      while ( !fake_segments.empty() ) // 要FIN了，之前发了正在fly的内容，现在也不用发
      {
        fake_ackno_ = fake_segments.front().seqno + fake_segments.front().payload.size();
        fake_segments.pop();
      }
      if ( this->input_.reader().bytes_buffered() >= window_size_ )
        return;
      // start from fake ackno，也就是之前发了一半还存在Byte stream里面的内容，继续发完，但是是相对地址
      auto push_pos
        = min( static_cast<uint64_t>( window_size_ ), unwrap_seq_num(fake_ackno_) - unwrap_seq_num(ackno_) );

      to_trans.payload = string( this->input_.reader().peek().substr( push_pos ) );
      to_trans.FIN = true;

      seqno_ = seqno_ + this->input_.reader().peek().substr( push_pos ).size() + 1;

      had_FIN = true;
    }
    else if ( this->input_.reader().bytes_buffered() ) // 正常情况，被ack了，发ack后面的
    {
      auto push_pos = unwrap_seq_num(seqno_) - unwrap_seq_num(ackno_); // 从buffer的哪里开始push
      auto push_num = min( static_cast<uint64_t>( window_size_ ), this->reader().bytes_buffered() )
                      - sequence_numbers_in_flight(); // push的数量
      push_num = min( push_num, TCPConfig::MAX_PAYLOAD_SIZE );

      if ( push_num == 0 ) // 这里不能用make_empty_message方法，因为这里也是用的transmit，所以无需考虑
        return;

      to_trans.payload = string( this->input_.reader().peek().substr( push_pos, push_num ) );

      seqno_ = seqno_ + push_num;
    }
    else if ( !to_trans.SYN ) // 什么都不是
      return;
    transmit( to_trans );
    flying_segments.push( to_trans );
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

    auto &new_ackno = msg.ackno.value();
    // ackno不能大于seqno
    if (unwrap_seq_num(new_ackno) > unwrap_seq_num(seqno_))
      return;
    // 接收过的信息就不接收了，也就是现在收到的ack必须大于fly第一个的ack，除非要发FIN
    if (!flying_segments.empty() && unwrap_seq_num(new_ackno) <= unwrap_seq_num(flying_segments.front().seqno) &&
         !this->writer().is_closed())
      return;


    // 如果发过来的不是对SYN的ACK，我们才pop
    if (unwrap_seq_num(msg.ackno.value()) != 1)
    {
      auto pop_num = min( static_cast<uint64_t>( unwrap_seq_num(msg.ackno.value()) - unwrap_seq_num(ackno_) ),
                          this->reader().bytes_buffered() );
      this->input_.reader().pop( pop_num );
    }

    ackno_ = msg.ackno.value();

    // 当前ackno已经超过了之前的
    while (!flying_segments.empty() && unwrap_seq_num(ackno_) > unwrap_seq_num(flying_segments.front().seqno))
      flying_segments.pop();
  }
  if (msg.window_size != 0)
  {
    window_size_ = msg.window_size;
    retrans_cnt = 0;
    retrans_timer = 0;
    retrans_RTO = initial_RTO_ms_;
    zero_window = false;
  }
  else // window_size = 0，特判
  {
    window_size_ = 1;
    zero_window = true;
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  // retransmit ackno_ to seqno_ - 1
  retrans_timer += ms_since_last_tick;
  if ( retrans_timer >= retrans_RTO )
  {
    if (window_size_ != 0 && !zero_window) // 重传时间翻倍
      retrans_RTO <<= 1;

    if (!flying_segments.empty())
    {
      transmit( flying_segments.front() ); // 重传第一段
    }
    else
    {
      retrans_cnt = 0;
      retrans_RTO = initial_RTO_ms_;
    }
    retrans_timer = 0;
    retrans_cnt++;
  }
}

uint64_t TCPSender::unwrap_seq_num( const Wrap32& num ) const
{
  // or this->input_.writer().bytes_pushed()
  return num.unwrap(isn_, this->input_.reader().bytes_popped());
}
