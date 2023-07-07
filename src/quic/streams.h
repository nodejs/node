#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#if HAVE_OPENSSL && NODE_OPENSSL_HAS_QUIC

#include <async_wrap.h>
#include <base_object.h>
#include <env.h>
#include <node_bob.h>
#include "bindingdata.h"
#include "data.h"

namespace node {
namespace quic {

class Session;

using Ngtcp2Source = bob::SourceImpl<ngtcp2_vec>;

// TODO(@jasnell): This is currently a placeholder for the actual definition.
class Stream : public AsyncWrap, public Ngtcp2Source {
 public:
  static Stream* From(void* stream_user_data);

  static BaseObjectPtr<Stream> Create(Session* session, int64_t id);

  Stream(BaseObjectPtr<Session> session, v8::Local<v8::Object> obj);

  int64_t id() const;
  Side origin() const;
  Direction direction() const;

  bool is_destroyed() const;
  bool is_eos() const;

  void Acknowledge(size_t datalen);
  void Blocked();
  void Commit(size_t datalen);
  void End();
  void Destroy(QuicError error);

  struct ReceiveDataFlags final {
    // Identifies the final chunk of data that the peer will send for the
    // stream.
    bool fin = false;
    // Indicates that this chunk of data was received in a 0RTT packet before
    // the TLS handshake completed, suggesting that is is not as secure and
    // could be replayed by an attacker.
    bool early = false;
  };

  void ReceiveData(const uint8_t* data, size_t len, ReceiveDataFlags flags);
  void ReceiveStopSending(QuicError error);
  void ReceiveStreamReset(uint64_t final_size, QuicError error);

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(Stream)
  SET_SELF_SIZE(Stream)

  ListNode<Stream> stream_queue_;

 public:
  // The Queue/Schedule/Unschedule here are part of the mechanism used to
  // determine which streams have data to send on the session. When a stream
  // potentially has data available, it will be scheduled in the Queue. Then,
  // when the Session::Application starts sending pending data, it will check
  // the queue to see if there are streams waiting. If there are, it will grab
  // one and check to see if there is data to send. When a stream does not have
  // data to send (such as when it is initially created or is using an async
  // source that is still waiting for data to be pushed) it will not appear in
  // the queue.
  using Queue = ListHead<Stream, &Stream::stream_queue_>;

  void Schedule(Queue* queue);
  void Unschedule();
};

}  // namespace quic
}  // namespace node

#endif  // HAVE_OPENSSL && NODE_OPENSSL_HAS_QUIC
#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
