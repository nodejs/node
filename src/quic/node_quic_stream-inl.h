#ifndef SRC_QUIC_NODE_QUIC_STREAM_INL_H_
#define SRC_QUIC_NODE_QUIC_STREAM_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "debug_utils.h"
#include "node_quic_session.h"
#include "node_quic_stream.h"
#include "node_quic_buffer-inl.h"

namespace node {
namespace quic {

QuicStreamDirection QuicStream::direction() const {
  return stream_id_ & 0b10 ?
      QUIC_STREAM_UNIDIRECTIONAL :
      QUIC_STREAM_BIRECTIONAL;
}

QuicStreamOrigin QuicStream::origin() const {
  return stream_id_ & 0b01 ?
      QUIC_STREAM_SERVER :
      QUIC_STREAM_CLIENT;
}

bool QuicStream::is_destroyed() const {
  return flags_ & QUICSTREAM_FLAG_DESTROYED;
}

bool QuicStream::has_received_fin() const {
  return flags_ & QUICSTREAM_FLAG_FIN;
}

bool QuicStream::has_sent_fin() const {
  return flags_ & QUICSTREAM_FLAG_FIN_SENT;
}

bool QuicStream::was_ever_writable() const {
  if (direction() == QUIC_STREAM_UNIDIRECTIONAL) {
    return session_->is_server() ?
        origin() == QUIC_STREAM_SERVER :
        origin() == QUIC_STREAM_CLIENT;
  }
  return true;
}

bool QuicStream::is_writable() const {
  if (flags_ & QUICSTREAM_FLAG_WRITE_CLOSED)
    return false;

  return was_ever_writable();
}

bool QuicStream::was_ever_readable() const {
  if (direction() == QUIC_STREAM_UNIDIRECTIONAL) {
    return session_->is_server() ?
        origin() == QUIC_STREAM_CLIENT :
        origin() == QUIC_STREAM_SERVER;
  }

  return true;
}

bool QuicStream::is_readable() const {
  if (flags_ & QUICSTREAM_FLAG_READ_CLOSED)
    return false;

  return was_ever_readable();
}

bool QuicStream::is_read_started() const {
  return flags_ & QUICSTREAM_FLAG_READ_STARTED;
}

bool QuicStream::is_read_paused() const {
  return flags_ & QUICSTREAM_FLAG_READ_PAUSED;
}

void QuicStream::set_fin_sent() {
  CHECK(!is_writable());
  flags_ |= QUICSTREAM_FLAG_FIN_SENT;
}

bool QuicStream::is_write_finished() const {
  return has_sent_fin() && streambuf_.length() == 0;
}

void QuicStream::IncrementAvailableOutboundLength(size_t amount) {
  available_outbound_length_ += amount;
}

void QuicStream::DecrementAvailableOutboundLength(size_t amount) {
  available_outbound_length_ -= amount;
}

void QuicStream::set_fin_received() {
  flags_ |= QUICSTREAM_FLAG_FIN;
  set_read_close();
}

void QuicStream::set_write_close() {
  flags_ |= QUICSTREAM_FLAG_WRITE_CLOSED;
}

void QuicStream::set_read_close() {
  flags_ |= QUICSTREAM_FLAG_READ_CLOSED;
}

void QuicStream::set_read_start() {
  flags_ |= QUICSTREAM_FLAG_READ_STARTED;
}

void QuicStream::set_read_pause() {
  flags_ |= QUICSTREAM_FLAG_READ_PAUSED;
}

void QuicStream::set_read_resume() {
  flags_ &= QUICSTREAM_FLAG_READ_PAUSED;
}

void QuicStream::set_destroyed() {
  flags_ |= QUICSTREAM_FLAG_DESTROYED;
}

bool QuicStream::SubmitInformation(v8::Local<v8::Array> headers) {
  return session_->SubmitInformation(stream_id_, headers);
}

bool QuicStream::SubmitHeaders(v8::Local<v8::Array> headers, uint32_t flags) {
  return session_->SubmitHeaders(stream_id_, headers, flags);
}

bool QuicStream::SubmitTrailers(v8::Local<v8::Array> headers) {
  return session_->SubmitTrailers(stream_id_, headers);
}

BaseObjectPtr<QuicStream> QuicStream::SubmitPush(
    v8::Local<v8::Array> headers) {
  return session_->SubmitPush(stream_id_, headers);
}

void QuicStream::EndHeaders(int64_t push_id) {
  Debug(this, "End Headers");
  // Upon completion of a block of headers, convert the
  // vector of Header objects into an array of name+value
  // pairs, then call the on_stream_headers function.
  session()->application()->StreamHeaders(
      stream_id_,
      headers_kind_,
      headers_,
      push_id);
  headers_.clear();
}

void QuicStream::set_headers_kind(QuicStreamHeadersKind kind) {
  headers_kind_ = kind;
}

void QuicStream::BeginHeaders(QuicStreamHeadersKind kind) {
  Debug(this, "Beginning Headers");
  // Upon start of a new block of headers, ensure that any
  // previously collected ones are cleaned up.
  headers_.clear();
  set_headers_kind(kind);
}

void QuicStream::Commit(ssize_t amount) {
  CHECK(!is_destroyed());
  CHECK_GE(amount, 0);
  streambuf_.Seek(amount);
}

void QuicStream::ResetStream(uint64_t app_error_code) {
  // On calling shutdown, the stream will no longer be
  // readable or writable, all any pending data in the
  // streambuf_ will be canceled, and all data pending
  // to be acknowledged at the ngtcp2 level will be
  // abandoned.
  set_read_close();
  set_write_close();
  session_->ResetStream(stream_id_, app_error_code);
}

template <typename T>
size_t QuicStream::DrainInto(
    std::vector<T>* vec,
    size_t max_count) {
  CHECK(!is_destroyed());
  size_t length = 0;
  streambuf_.DrainInto(vec, &length, max_count);
  return length;
}

template <typename T>
size_t QuicStream::DrainInto(
    T* vec,
    size_t* count,
    size_t max_count) {
  CHECK(!is_destroyed());
  size_t length = 0;
  streambuf_.DrainInto(vec, count, &length, max_count);
  return length;
}

void QuicStream::Schedule(Queue* queue) {
  if (!stream_queue_.IsEmpty())  // Already scheduled?
    return;
  queue->PushBack(this);
}

void QuicStream::Unschedule() {
  stream_queue_.Remove();
}

}  // namespace quic
}  // namespace node

#endif  // NODE_WANT_INTERNALS

#endif  // SRC_QUIC_NODE_QUIC_STREAM_INL_H_
