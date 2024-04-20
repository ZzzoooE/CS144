#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

uint64_t Writer::capacity() {
  return capacity_;
}

void Writer::push( string data )
{
  if (is_closed() || available_capacity() == 0 || data.empty()) {
    return ;
  }
  auto n = min(available_capacity(), data.size());
  if (data.size() > n) {
    data = data.substr(0, n);
  }
  buffer_.push_back(move(data));
  viewbuffer_.emplace_back(buffer_.back());
  bytes_buffered_ += n;
  bytes_pushed_ += n;
}

void Writer::close()
{
  // Your code here.
  is_closed_ = true;
}

void Writer::set_error()
{
  // Your code here.
  has_error_ = true;
}

bool Writer::is_closed() const
{
  // Your code here.
  return is_closed_;
}

uint64_t Writer::available_capacity() const
{
  // Your code here.
  return capacity_ - bytes_buffered_;
}

uint64_t Writer::bytes_pushed() const
{
  // Your code here.
  return bytes_pushed_;
}

string_view Reader::peek() const
{
  // Your code here.
  if ( viewbuffer_.empty() ) {
    return {};
  }
  return viewbuffer_.front();
}

bool Reader::is_finished() const
{
  return is_closed_ && bytes_buffered() == 0;
}

bool Reader::has_error() const
{
  // Your code here.
  return has_error_;
}

void Reader::pop( uint64_t len )
{
  // Your code here.
  auto n = min(len, bytes_buffered());
  while (n > 0 && !buffer_.empty()) {
    auto sz = viewbuffer_.front().size();
    if (n < sz) {
      viewbuffer_.front().remove_prefix(n);
      bytes_popped_ += n;
      bytes_buffered_ -= n;
      return ;
    } 
    bytes_popped_ += sz;
    bytes_buffered_ -= sz;
    n -= sz;
    buffer_.pop_front();
    viewbuffer_.pop_front();
  }
}

uint64_t Reader::bytes_buffered() const
{
  // Your code here.
  return bytes_buffered_;
}

uint64_t Reader::bytes_popped() const
{
  // Your code here.
  return bytes_popped_;
}
