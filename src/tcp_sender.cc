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
    // FIN会是最后一个消息
    if (had_FIN)
      return;
    TCPSenderMessage to_trans { seqno_, false, "", false, false };
    if ( this->input_.has_error() )
      to_trans.RST = true;
    if ( !has_SYN ) // 还没开始，准备SYN
    {
      to_trans.seqno = isn_;
      to_trans.SYN = true;
      if (window_size_ == UINT32_MAX) // 如果没被初始化，就初始化
        window_size_ = 1;
      has_SYN = true;
    }
    // 已经被关闭了，准备FIN，且有空间发FIN；如果buffer大于等于window，那就是普通情况，等一下再发FIN
    // FIN不占payload的size，但是占window
    // 需要考虑MAX_PAYLOAD_SIZE，要不然一个10000大小的payload，分成10次发，会每次都带FIN
    if ( this->input_.writer().is_closed() &&
         this->reader().bytes_buffered() < window_size_ &&
         this->reader().bytes_buffered() - sequence_numbers_in_flight() <= TCPConfig::MAX_PAYLOAD_SIZE )
    {
      // 测试会调用close方法，就关闭了
      if ( had_FIN ) // 发过了就不发了
        return;
      to_trans.FIN = true;
      had_FIN = true;
    }

    // start from last byte + 1，但是如果Bytestream里面只有SYN，那就提取不出来内容，需要取min得到0
    auto push_pos = min(this->reader().bytes_buffered(), sequence_numbers_in_flight());
    // push的数量，现在缓存了多少个减去发出还没确认的个数，bytes_buffered肯定是>=sequence_numbers_in_flight的
    // 同时循环也确定了sequence_numbers_in_flight() < window_size_，否则不进行push操作
    // 减to_trans.SYN的原因是可能SYN和data一起，会占一个位置
    auto push_num = this->reader().bytes_buffered() - sequence_numbers_in_flight();
    push_num = min( push_num, window_size_ - sequence_numbers_in_flight() - to_trans.SYN);
    push_num = min( push_num, TCPConfig::MAX_PAYLOAD_SIZE );

    if ( push_num + to_trans.SYN + to_trans.FIN == 0) // 如果所有内容全空，规格错误，就不发送
      return;

    to_trans.payload = string( this->input_.reader().peek().substr( push_pos, push_num ) );

    seqno_ = seqno_ + to_trans.payload.size() + to_trans.SYN + to_trans.FIN;

    transmit( to_trans );
    flying_segments.push( to_trans );
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  return { seqno_, false, "", false, this->input_.has_error()};
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // receive后，测试的调用会自动触发push
  if (msg.RST)
    this->input_.set_error();
  if (msg.ackno.has_value() && has_SYN) {

    auto &new_ackno = msg.ackno.value();
    auto &first_fly_ele = flying_segments.front();
    // ackno不能大于seqno
    if (unwrap_seq_num(new_ackno) > unwrap_seq_num(seqno_))
      return;
    // 新的ack也不能小于老的ack，否则无效
    if ( unwrap_seq_num(new_ackno) < unwrap_seq_num(ackno_))
      return;
    // 接收过的信息就不接收了，也就是现在收到的ack必须大于fly第一个的ack，除非要发FIN
    if (!flying_segments.empty() && unwrap_seq_num(new_ackno) <= unwrap_seq_num(first_fly_ele.seqno) &&
         !this->writer().is_closed())
      return;
    // 如果接收的ack不够弹出fly的第一个块，那么ack不算数
    if (!flying_segments.empty() &&
         unwrap_seq_num(new_ackno) > unwrap_seq_num(ackno_) &&
         unwrap_seq_num(new_ackno) - unwrap_seq_num(ackno_) < first_fly_ele.sequence_length())
      return;

    // 如果发过来的不是对SYN的ACK，我们才pop，syn和data和fin一起
    if (unwrap_seq_num(ackno_) == 0)
      ackno_ = ackno_ + 1;

    auto pop_num = min( static_cast<uint64_t>( unwrap_seq_num(msg.ackno.value()) - unwrap_seq_num(ackno_) ),
                        this->reader().bytes_buffered() );
    this->input_.reader().pop( pop_num );

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
  if (flying_segments.empty())
    return;
  retrans_timer += ms_since_last_tick;
  if ( retrans_timer >= retrans_RTO )
  {
    if (window_size_ != 0 && !zero_window) // 重传时间翻倍
    {
      retrans_RTO <<= 1;
      retrans_cnt++;
    }
    retrans_timer = 0;

    if (!flying_segments.empty())
    {
      transmit( flying_segments.front() ); // 重传第一段
    }
    else
    {
      retrans_cnt = 0;
      retrans_RTO = initial_RTO_ms_;
    }
  }
}

uint64_t TCPSender::unwrap_seq_num( const Wrap32& num ) const
{
  // or this->input_.writer().bytes_pushed()
  return num.unwrap(isn_, this->input_.reader().bytes_popped());
}
