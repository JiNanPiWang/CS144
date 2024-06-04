#include <iostream>
#include <utility>

#include "reassembler.hh"

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  bool changed_tail = false;

  // 如果available_capacity < data.size()，map就只存substr，并且只要有空间，就读
  if ( bytes_pending() >= this->writer().available_capacity() )
    return;

  // 如果first_index >= current_pos，就正常存
  // 如果first_index < current_pos <= first_index + data.size()的，也按current_pos插入

  // first_index + data.size() <= current_pos + this->writer().available_capacity()
  // 只能插current_pos到current_pos + this->writer().available_capacity()的内容进来
  if (first_index >= current_pos) // cur在fir左边，存available_capacity内的内容
  {
    if (first_index + data.size() > current_pos + this->writer().available_capacity())
    {
      data = data.substr( 0, current_pos + this->writer().available_capacity() - first_index );
      changed_tail = true;
    }
  }
  else // cur在fir左边，详见check1.md配图
  {
    if (first_index + data.size() >= current_pos)
      data = data.substr( current_pos - first_index );
    else
      return;
    if (current_pos + this->writer().available_capacity() < data.size() + first_index)
    {
      data = data.substr( 0, this->writer().available_capacity() );
      changed_tail = true;
    }
    first_index = current_pos;
  }

  pending_bytes_ += data.size();
  fragments_map[first_index] = std::move( data );

  if ( is_last_substring && !changed_tail )
    close_flag = true;


  while ( !fragments_map.empty() && fragments_map.begin()->first <= current_pos) // 可以插入了，只插入在范围内的
  {
    auto& cur_str = fragments_map.begin()->second;
    pending_bytes_ -= cur_str.size();
    // 判断字符串是否在范围内，不在，就不算
    if (fragments_map.begin()->first + cur_str.size() < current_pos)
    {
      fragments_map.erase( fragments_map.begin() );
      continue;
    }
    // 修剪字符串
    if (fragments_map.begin()->first != current_pos)
      cur_str = cur_str.substr( current_pos - fragments_map.begin()->first );

    auto new_pos = current_pos + cur_str.size();
    this->output_.writer().push( cur_str );

    fragments_map.erase( fragments_map.begin() );
    current_pos = new_pos;
  }

  if ( close_flag && fragments_map.empty() )
    this->output_.writer().close();
}

uint64_t Reassembler::bytes_pending() const
{
  if (fragments_map.empty())
    return 0;
  uint64_t pos = fragments_map.begin()->first;
  uint64_t result = 0;
  for (auto &p : fragments_map)
  {
    if (p.first >= pos)
      result += p.second.size();
    else if (p.first + p.second.size() >= pos)
      result += p.first + p.second.size() - pos;
    else
      continue;
    pos = p.first + p.second.size();
  }
  return result;
}
