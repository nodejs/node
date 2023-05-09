#if HAVE_OPENSSL && NODE_OPENSSL_HAS_QUIC
#include "streams.h"
#include <async_wrap-inl.h>
#include <base_object-inl.h>
#include <env-inl.h>
#include <memory_tracker-inl.h>
#include <node_bob-inl.h>
#include <node_sockaddr-inl.h>
#include "session.h"

namespace node {
namespace quic {

Stream::Stream(BaseObjectPtr<Session> session, v8::Local<v8::Object> obj)
    : AsyncWrap(session->env(), obj, AsyncWrap::PROVIDER_QUIC_STREAM) {
  MakeWeak();
}

Stream* Stream::From(void* stream_user_data) {
  DCHECK_NOT_NULL(stream_user_data);
  return static_cast<Stream*>(stream_user_data);
}

BaseObjectPtr<Stream> Stream::Create(Session* session, int64_t id) {
  return BaseObjectPtr<Stream>();
}

int64_t Stream::id() const {
  return 0;
}
Side Stream::origin() const {
  return Side::CLIENT;
}
Direction Stream::direction() const {
  return Direction::BIDIRECTIONAL;
}

bool Stream::is_destroyed() const {
  return false;
}
bool Stream::is_eos() const {
  return false;
}

void Stream::Acknowledge(size_t datalen) {}
void Stream::Blocked() {}
void Stream::Commit(size_t datalen) {}
void Stream::End() {}
void Stream::Destroy(QuicError error) {}

void Stream::ReceiveData(const uint8_t* data,
                         size_t len,
                         ReceiveDataFlags flags) {}
void Stream::ReceiveStopSending(QuicError error) {}
void Stream::ReceiveStreamReset(uint64_t final_size, QuicError error) {}

void Stream::Schedule(Stream::Queue* queue) {}
void Stream::Unschedule() {}

}  // namespace quic
}  // namespace node

#endif  // HAVE_OPENSSL && NODE_OPENSSL_HAS_QUIC
