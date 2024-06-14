#include "wrapping_integers.hh"
#include <cmath>

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // 自然溢出，相当于取模
  return Wrap32 { zero_point + n };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint64_t max2_32 = (uint64_t)1 << 32;
  uint64_t pos_0 = raw_value_ - zero_point.raw_value_;

  // 找离checkpoint最近的，可以加n个1<<32
  auto n = checkpoint / max2_32;
  checkpoint %= max2_32;

  // n或n+1，checkpoint处于(n - 1) * max2_32到n * max2_32到(n + 1) * max2_32之间
  if ((checkpoint > pos_0) && (checkpoint - pos_0 > max2_32 / 2))
    return pos_0 + (n + 1) * max2_32;
  else if ((pos_0 > checkpoint) && (pos_0 - checkpoint > max2_32 / 2))
    return pos_0 + (n - 1) * max2_32;
  return pos_0 + n * max2_32;
}
