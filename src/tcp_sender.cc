// #include "tcp_sender.hh"
// #include "tcp_config.hh"

// #include <algorithm>
// #include <random>

// using namespace std;

// Timer::Timer( uint64_t init_RTO ) : timer( init_RTO ), initial_RTO( init_RTO ), RTO( init_RTO ), running( false ) {}

// void Timer::elapse( uint64_t time_elapsed )
// {
//   if ( running ) {
//     timer -= std::min( timer, time_elapsed );
//   }
// }

// void Timer::double_RTO()
// {
//   if ( RTO & ( 1ULL << 63 ) ) {
//     RTO = UINT64_MAX;
//   } else {
//     RTO <<= 1;
//   }
// }

// void Timer::reset()
// {
//   timer = RTO;
// }

// void Timer::start()
// {
//   running = true;
// }

// void Timer::stop()
// {
//   running = false;
// }

// bool Timer::expired() const
// {
//   return timer == 0;
// }

// bool Timer::is_stopped() const
// {
//   return !running || expired();
// }

// void Timer::restore_RTO()
// {
//   RTO = initial_RTO;
// }

// void Timer::restart()
// {
//   reset();
//   start();
// }

// /* TCPSender constructor (uses a random ISN if none given) */
// TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
//   : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) ), timer( initial_RTO_ms )
// {}

// uint64_t TCPSender::sequence_numbers_in_flight() const
// {
//   return pushed_no - ack_no;
// }

// uint64_t TCPSender::consecutive_retransmissions() const
// {
//   return retransmissions;
// }

// optional<TCPSenderMessage> TCPSender::maybe_send()
// {
//   if ( !messages_to_be_sent.empty() ) {
//     if ( timer.is_stopped() ) {
//       timer.restart();
//     }

//     auto msg = messages_to_be_sent.front();
//     messages_to_be_sent.pop();
//     return *msg;
//   }
//   return std::nullopt;
// }

// void TCPSender::push( Reader& outbound_stream )
// {
//   uint64_t allowed_no = received_ack_no + std::max( window_size, uint16_t { 1 } );
//   if ( pushed_no == 0 ) {
//     std::shared_ptr<TCPSenderMessage> message = std::make_shared<TCPSenderMessage>( send_empty_message() );

//     if ( outbound_stream.is_finished() && !FIN_sent && allowed_no - pushed_no > 1 ) {
//       message->FIN = true;
//     }

//     if ( message->FIN ) {
//       FIN_sent = true;

//     }

//     pushed_no += message->sequence_length();
//     messages_to_be_sent.push( message );
//     outstanding_messages.push( message );
//   }

//   while ( ( outbound_stream.bytes_buffered() > 0 || ( outbound_stream.is_finished() && !FIN_sent ) )
//           && pushed_no < allowed_no ) {
//     uint64_t msg_len
//       = std::min( { allowed_no - pushed_no, outbound_stream.bytes_buffered(), TCPConfig::MAX_PAYLOAD_SIZE } );
//     Buffer buffer { std::string { outbound_stream.peek().substr( 0, msg_len ) } };
//     outbound_stream.pop( msg_len );

//     bool FIN = outbound_stream.is_finished() && !FIN_sent && allowed_no - pushed_no > msg_len;

//     std::shared_ptr<TCPSenderMessage> message = std::make_shared<TCPSenderMessage>();
//     message->seqno = Wrap32::wrap( pushed_no, isn_ );
//     message->SYN = false;
//     message->payload = buffer;
//     message->FIN = FIN;

//     if ( message->FIN ) {
//       FIN_sent = true;
//     }

//     pushed_no += message->sequence_length();
//     messages_to_be_sent.push( message );
//     outstanding_messages.push( message );
//   }
// }

// TCPSenderMessage TCPSender::send_empty_message() const
// {
//   TCPSenderMessage message;
//   message.seqno = Wrap32::wrap( pushed_no, isn_ );
//   message.SYN = ( pushed_no == 0 );
//   message.payload = {};
//   message.FIN = false;

//   return message;
// }

// void TCPSender::receive( const TCPReceiverMessage& msg )
// {
//   if ( msg.ackno.has_value() ) {
//     uint64_t received_ackno = msg.ackno.value().unwrap( isn_, ack_no );
//     if ( received_ackno <= pushed_no ) {
//       while ( !outstanding_messages.empty()
//               && received_ackno >= ack_no + outstanding_messages.front()->sequence_length() ) {
//         ack_no += outstanding_messages.front()->sequence_length();
//         outstanding_messages.pop();

//         timer.restore_RTO();
//         retransmissions = 0;
//         if ( !outstanding_messages.empty() ) {
//           timer.restart();
//         } else {
//           timer.stop();
//         }
//       }
//       received_ack_no = std::max( received_ack_no, received_ackno );
//     }
//   }


//   window_size = msg.window_size;
// }

// void TCPSender::tick( const size_t ms_since_last_tick )
// {
//   timer.elapse( ms_since_last_tick );
//   if ( timer.expired() ) {
//     messages_to_be_sent.push( outstanding_messages.front() );

//     if ( window_size ) {
//       timer.double_RTO();
//       retransmissions += 1;
//     }

//     timer.restart();
//   }
// }
#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <random>
#include <algorithm>

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, std::optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { std::random_device()() } ) ), _initial_RTO_ms( initial_RTO_ms )
{}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return _pushed_no - _ackno;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return _consecutive_retransmissions;
}

std::optional<TCPSenderMessage> TCPSender::maybe_send()
{
  if (!_segments_to_send.empty()) {
    auto segment = _segments_to_send.front();
    _segments_to_send.pop();
    return segment;
  }
  return std::nullopt;
}

/*
The TCPSender is asked to fill the window from the outbound byte stream: it reads
from the stream and generates as many TCPSenderMessages as possible, as long as
there are new bytes to be read and space available in the window.
You’ll want to make sure that every TCPSenderMessage you send fits fully inside the
receiver’s window. Make each individual message as big as possible, but no bigger than
the value given by TCPConfig::MAX PAYLOAD SIZE (1452 bytes).
You can use the TCPSenderMessage::sequence length() method to count the total
number of sequence numbers occupied by a segment. Remember that the SYN and
FIN flags also occupy a sequence number each, which means that they occupy space in
the window
*/
//填满滑动窗口的过程
void TCPSender::push( Reader& outbound_stream )
{
  if (outbound_stream.is_finished() || _FIN_sent || _window_size == _lengthFilled_in_window) {
    return ;
  }
  
  //send_empty_message
  if (_window_size == 0) {
    _segments_to_send.push(std::move(send_empty_message()));
    return ;
  }

  // SYN or FIN
  if (_pushed_no == 0) {
    TCPSenderMessage message = {Wrap32::wrap(_pushed_no, isn_), true, Buffer{}, false};
    _SYN_sent = true;
    _lengthFilled_in_window ++ ;
    _pushed_no ++ ;
    _length_of_out ++ ;
    _segments_to_send.push(message);
    _segments_out.push(message);
  }
  
  if (outbound_stream.is_finished() && !_FIN_sent) {
    TCPSenderMessage message = {Wrap32::wrap(_pushed_no, isn_), false, Buffer{}, true};
    _FIN_sent = true;
    _lengthFilled_in_window ++ ;
    _pushed_no ++ ;
    _length_of_out ++ ;
    _segments_to_send.push(message);
    _segments_out.push(message);
  }
 
  while (outbound_stream.bytes_buffered() > 0 && _lengthFilled_in_window < _window_size && _pushed_no < _final_windowSeq) {
    uint64_t _msg_len = std::min({_window_size - _lengthFilled_in_window, outbound_stream.bytes_buffered(), TCPConfig::MAX_PAYLOAD_SIZE}); 
    TCPSenderMessage message = { Wrap32::wrap(_pushed_no, isn_), false, Buffer{}, false};
    message.FIN = outbound_stream.is_finished() && !_FIN_sent && (_window_size - _lengthFilled_in_window > _msg_len);
    message.payload = Buffer{ outbound_stream.pop(_msg_len) };
    if (message.FIN) {
      _FIN_sent = true;
    }
    _msg_len = message.sequence_length();
    _lengthFilled_in_window += _msg_len;
    _pushed_no += _msg_len;
    _length_of_out += _msg_len;
    _segments_to_send.push(message);
    _segments_out.push(message);
  }
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  return TCPSenderMessage{Wrap32::wrap(_pushed_no, isn_), false, Buffer{}, false};
}

/*
A message is received from the receiver, conveying the new left (= ackno) and right (=
ackno + window size) edges of the window. The TCPSender should look through its
collection of outstanding segments and remove any that have now been fully acknowl-
edged (the ackno is greater than all of the sequence numbers in the segment).
*/
void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if (msg.ackno.has_value()) {
    auto ackno = msg.ackno.value().unwrap(isn_, _final_windowSeq);
    if (ackno - 1 >= _final_windowSeq || ackno < _ackno) {
      _window_size = msg.window_size;
      _final_windowSeq = _ackno + msg.window_size;
      return ;
    }
    while (_segments_out.size() &&
     _segments_out.front().seqno.unwrap(isn_, _final_windowSeq) + _segments_out.front().sequence_length() - 1 < ackno) {
      _length_of_out -= _segments_out.front().sequence_length();
      _ackno += _segments_out.front().sequence_length();
      _segments_out.pop();
      
      _RTO = _initial_RTO_ms;
      _consecutive_retransmissions = 0;
      if (_segments_out.size()) {
        _time_accumulate = 0;
      } else {
        stop_timer();
      }
    }
    
    _ackno = std::max(_ackno, ackno);
  }
  _window_size = msg.window_size;
  _final_windowSeq = _ackno + msg.window_size;
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  if (!running) {
    return ;
  }

  time_elapse(ms_since_last_tick);

  //超时重传
  if (timer_is_expired()) {
    if (!_segments_out.empty()) {
      _segments_to_send.push(_segments_out.front());
    }

    if (_window_size) {
      _consecutive_retransmissions ++ ;
      _RTO *= 2;
    }

    _time_accumulate = 0;
  }
  
}

void TCPSender::stop_timer() {
  running = false;
}

void TCPSender::start_timer() {
  running = true;
}

void TCPSender::time_elapse(uint64_t ms_passed) {
  _time_accumulate += ms_passed;
}

bool TCPSender::timer_is_expired() {
  return running && _time_accumulate >= _RTO;
}




