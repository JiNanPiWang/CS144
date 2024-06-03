#include <iostream>
#include <utility>

#include "reassembler.hh"

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  // 如果available_capacity < data.size()，map就只存substr，并且只要有空间，就读
  if ( bytes_pending() >= this->writer().available_capacity() )
    return;

  // 如果first_index >= current_pos，就正常存
  // 如果first_index < current_pos <= first_index + data.size()的，也按current_pos插入

  // first_index + data.size() <= current_pos + this->writer().available_capacity()
  // 只能插current_pos到current_pos + this->writer().available_capacity()的内容进来
  if (first_index >= current_pos)
  {
    if (first_index + data.size() > current_pos + this->writer().available_capacity())
      data = data.substr( 0, current_pos + this->writer().available_capacity() - first_index );
  }
  else // first_index < current_pos <= first_index + data.size()
  {
    if (first_index + data.size() >= current_pos)
      data = data.substr( current_pos - first_index );
    if (current_pos + this->writer().available_capacity() < data.size() + first_index)
      data = data.substr( 0, this->writer().available_capacity() );
    first_index = current_pos;
  }

  pending_bytes_ += data.size();
  fragments_map[first_index] = std::move( data );

  if ( is_last_substring )
    close_flag = true;

  while ( fragments_map.find( current_pos ) != fragments_map.end() ) // 可以插入了
  {
    auto& cur_str = fragments_map.at( current_pos );
    auto new_pos = current_pos + cur_str.size();

    this->output_.writer().push( cur_str );

    pending_bytes_ -= cur_str.size();
    fragments_map.erase( current_pos );

    current_pos = new_pos;
  }

  if ( close_flag && fragments_map.empty() )
    this->output_.writer().close();
}

uint64_t Reassembler::bytes_pending() const
{
  return pending_bytes_;
}
