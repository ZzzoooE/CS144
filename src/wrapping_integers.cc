#include "wrapping_integers.hh"

using namespace std;

// abs_seq -> seq
Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return zero_point + static_cast<uint32_t>(n);
}

// sqe -> abs_seq
//将64位哈希到32位,32位的每个数字会代表2^32个数字(比如1代表1, 2^32+1,2*2^32+1,3*2^32+1 ... 2^32*2^32+1)
uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  Wrap32 c = wrap(checkpoint, zero_point);
  int32_t offset = raw_value_ - c.raw_value_;
  int64_t res = checkpoint + offset;
  if (res < 0) res += 1ul << 32;
  return res;
}
