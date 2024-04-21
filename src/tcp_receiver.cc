#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
  auto& [seqno, SYN, payload, FIN] = message;
  if (SYN) {
    initial_seqno = seqno;
    synced = true;
  } else if (!synced) {
    return ;
  }

  //seq -> stream idx
  auto first_index = seqno.unwrap(initial_seqno, inbound_stream.bytes_pushed()) - !SYN;
  reassembler.insert(first_index, payload, FIN, inbound_stream);
}

TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  TCPReceiverMessage message;
  if ( synced ) {
    // stream idx -> seq
    message.ackno = Wrap32::wrap(inbound_stream.bytes_pushed(), initial_seqno) + synced + inbound_stream.is_closed();
  }
  message.window_size = min(uint64_t(UINT16_MAX), inbound_stream.available_capacity());
  return message;
}
