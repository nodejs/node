#ifndef SRC_QUIC_NODE_QUIC_STREAM_INL_H_
#define SRC_QUIC_NODE_QUIC_STREAM_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "debug_utils-inl.h"
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

void QuicStream::set_final_size(uint64_t final_size) {
  // Only set the final size once.
  if (state_->fin_received == 1) {
    CHECK_LE(final_size, GetStat(&QuicStreamStats::final_size));
    return;
  }
  state_->fin_received = 1;
  SetStat(&QuicStreamStats::final_size, final_size);
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
  return was_ever_writable() && !streambuf_.is_ended();
}

bool QuicStream::was_ever_readable() const {
  if (direction() == QUIC_STREAM_UNIDIRECTIONAL) {
    return session_->is_server() ?
        origin() == QUIC_STREAM_CLIENT :
        origin() == QUIC_STREAM_SERVER;
  }

  return true;
}

void QuicStream::set_fin_sent() {
  Debug(this, "final stream frame sent");
  state_->fin_sent = 1;
  if (shutdown_done_ != nullptr) {
    shutdown_done_(0);
  }
}

void QuicStream::set_destroyed() {
  destroyed_ = true;
}

bool QuicStream::is_readable() const {
  return was_ever_readable() && state_->read_ended == 0;
}

bool QuicStream::is_write_finished() const {
  return state_->fin_sent == 1 && streambuf_.length() == 0;
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

void QuicStream::Commit(size_t amount) {
  CHECK(!is_destroyed());
  streambuf_.Seek(amount);
}

// ResetStream will cause ngtcp2 to queue a RESET_STREAM and STOP_SENDING
// frame, as appropriate, for the given stream_id. For a locally-initiated
// unidirectional stream, only a RESET_STREAM frame will be scheduled and
// the stream will be immediately closed. For a bidirectional stream, a
// STOP_SENDING frame will be sent.
void QuicStream::ResetStream(uint64_t app_error_code) {
  QuicSession::SendSessionScope send_scope(session());
  session()->ShutdownStream(id(), app_error_code);
  state_->read_ended = 1;
  streambuf_.Cancel();
  streambuf_.End();
}

// StopSending will cause ngtcp2 to queue a STOP_SENDING frame if the
// stream is still inbound readable.
void QuicStream::StopSending(uint64_t app_error_code) {
  QuicSession::SendSessionScope send_scope(session());
  ngtcp2_conn_shutdown_stream_read(
      session()->connection(),
      stream_id_,
      app_error_code);
  state_->read_ended = 1;
}

void QuicStream::CancelPendingWrites() {
  // In case this stream is scheduled for sending data, remove it
  // from the schedule queue
  Unschedule();

  // If there is data currently buffered in the streambuf_,
  // then cancel will call out to invoke an arbitrary
  // JavaScript callback (the on write callback). Within
  // that callback, however, the QuicStream will no longer
  // be usable to send or receive data.
  streambuf_.End();
  streambuf_.Cancel();
  CHECK_EQ(streambuf_.length(), 0);
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
