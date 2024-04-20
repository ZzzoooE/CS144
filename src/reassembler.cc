#include "reassembler.hh"

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
  if (is_last_substring) {
    _eof = true;  
    final_index = first_index + max((size_t)0, data.size() - 1);
  }
  tryclose(output);
  auto _remain = output.available_capacity();
  if (data.empty() || _remain == 0) return ;

  auto R = first_index + data.size() - 1;
  auto first_unacceptable_index = _next_readIndex + _remain;
  //too left or too right
  if (R < _next_readIndex || first_index > first_unacceptable_index) {
      return ;
  }
  R = min(R, first_unacceptable_index - 1);
  auto L = max(first_index, _next_readIndex);
  if (L > R) {
      return ;
  }
  block t{L, R, data.substr(L - first_index, R - L + 1)};
  auto it = S.lower_bound({L, R, ""});

  //如果[L,R]已经存在于存储器中直接退出
  if ((it != S.end() && it->l == L) || (it != S.begin() && prev(it)->l <= L && prev(it)->r >= R)){
    return ;
  }
  
  //换成L,L是因为L,R会找不到可以被[L,R]吸收的块
  it = S.lower_bound({L, L, ""});
  //尝试合并前驱
  if (it != S.begin()) {
    auto pre_r = prev(it)->r;
    if (pre_r >= L - 1) {
       --it;
      t.l = it->l;
      t.str = it->str + (pre_r == L - 1 ? t.str : t.str.substr(pre_r - L + 1));
      _unassembledbytes -= it->str.size();
      it = S.erase(it);
    }
  }

  //不断合并l∈[L,R]的区间
  while (it != S.end() && it->l <= R) {
    _unassembledbytes -= it->str.size();
    if (it->r > R) {
      t.str += it->str.substr(R - it->l + 1);
      t.r = it->r;
    }
    it = S.erase(it);
  }

  //尝试合并后驱
  if (it != S.end() && it->l == R + 1) {
    t.r = it->r;
    t.str += it->str;
    _unassembledbytes -= it->str.size();
    S.erase(it);
  }
  
  //写进字节流或者存进重组器
  if (L == _next_readIndex) {
    _next_readIndex = t.r + 1;
    output.push(t.str);
  } else {
    _unassembledbytes += t.str.size();
    S.insert(move(t));
  }

  tryclose(output);
}

void Reassembler::tryclose( Writer& output ) {
  if (_eof && _next_readIndex == final_index + 1) {
      output.close();
  }
}


uint64_t Reassembler::bytes_pending() const
{
  // Your code here.
  return _unassembledbytes;
}
