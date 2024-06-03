#include <iostream>
#include <utility>

#include "reassembler.hh"

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  if ( data.size() + bytes_pending() > this->writer().available_capacity() )
    return;

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
