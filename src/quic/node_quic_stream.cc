#include "node_quic_stream-inl.h"  // NOLINT(build/include)
#include "aliased_struct-inl.h"
#include "async_wrap-inl.h"
#include "debug_utils-inl.h"
#include "env-inl.h"
#include "node.h"
#include "node_buffer.h"
#include "node_internals.h"
#include "stream_base-inl.h"
#include "node_sockaddr-inl.h"
#include "node_http_common-inl.h"
#include "node_quic_session-inl.h"
#include "node_quic_socket-inl.h"
#include "node_quic_util-inl.h"
#include "v8.h"
#include "uv.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

namespace node {

using v8::Array;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::ObjectTemplate;
using v8::PropertyAttribute;
using v8::String;
using v8::Value;

namespace quic {

QuicStream::QuicStream(
    QuicSession* sess,
    Local<Object> wrap,
    int64_t stream_id,
    int64_t push_id)
  : AsyncWrap(sess->env(), wrap, AsyncWrap::PROVIDER_QUICSTREAM),
    StreamBase(sess->env()),
    StatsBase(sess->env(), wrap,
              HistogramOptions::ACK |
              HistogramOptions::RATE |
              HistogramOptions::SIZE),
    session_(sess),
    stream_id_(stream_id),
    push_id_(push_id),
    state_(sess->env()->isolate()),
    quic_state_(sess->quic_state()) {
  CHECK_NOT_NULL(sess);
  Debug(this, "Created");
  StreamBase::AttachToObject(GetObject());

  wrap->DefineOwnProperty(
      env()->context(),
      env()->state_string(),
      state_.GetArrayBuffer(),
      PropertyAttribute::ReadOnly).Check();

  ngtcp2_transport_params params;
  ngtcp2_conn_get_local_transport_params(session()->connection(), &params);
  IncrementStat(&QuicStreamStats::max_offset, params.initial_max_data);
}

QuicStream::~QuicStream() {
  DebugStats();
}

template <typename Fn>
void QuicStreamStatsTraits::ToString(const QuicStream& ptr, Fn&& add_field) {
#define V(_n, name, label)                                                     \
  add_field(label, ptr.GetStat(&QuicStreamStats::name));
  STREAM_STATS(V)
#undef V
}

// Acknowledge is called when ngtcp2 has received an acknowledgement
// for one or more stream frames for this QuicStream. This will cause
// data stored in the streambuf_ outbound queue to be consumed and may
// result in the JavaScript callback for the write to be invoked.
void QuicStream::Acknowledge(uint64_t offset, size_t datalen) {
  if (is_destroyed())
    return;

  // ngtcp2 guarantees that offset must always be greater
  // than the previously received offset, but let's just
  // make sure that holds.
  CHECK_GE(offset, GetStat(&QuicStreamStats::max_offset_ack));
  SetStat(&QuicStreamStats::max_offset_ack, offset);

  Debug(this, "Acknowledging %d bytes", datalen);

  // Consumes the given number of bytes in the buffer. This may
  // have the side-effect of causing the onwrite callback to be
  // invoked if a complete chunk of buffered data has been acknowledged.
  streambuf_.Consume(datalen);

  RecordAck(&QuicStreamStats::acked_at);
}

// While not all QUIC applications will support headers, QuicStream
// includes basic, generic support for storing them.
bool QuicStream::AddHeader(std::unique_ptr<QuicHeader> header) {
  size_t len = header->length();
  QuicApplication* app = session()->application();
  // We cannot add the header if we've either reached
  // * the max number of header pairs or
  // * the max number of header bytes
  if (headers_.size() == app->max_header_pairs() ||
      current_headers_length_ + len > app->max_header_length()) {
    return false;
  }

  current_headers_length_ += header->length();
  Debug(this, "Header - %s", header.get());
  headers_.emplace_back(std::move(header));
  return true;
}

std::string QuicStream::diagnostic_name() const {
  return std::string("QuicStream ") + std::to_string(stream_id_) +
         " (" + std::to_string(static_cast<int64_t>(get_async_id())) +
         ", " + session_->diagnostic_name() + ")";
}

void QuicStream::Destroy(QuicError* error) {
  if (destroyed_)
    return;
  destroyed_ = true;

  if (is_writable() || is_readable())
    session()->ShutdownStream(id(), 0);

  CancelPendingWrites();

  session_->RemoveStream(stream_id_);
}

// Do shutdown is called when the JS stream writable side is closed.
// If we're not within an ngtcp2 callback, this will trigger the
// QuicSession to send any pending data. If a final stream frame
// has not already been sent, it will be after this.
int QuicStream::DoShutdown(ShutdownWrap* req_wrap) {
  if (is_destroyed())
    return UV_EPIPE;

  // If the fin bit has already been sent, we can return
  // immediately because there's nothing else to do. The
  // _final callback will be invoked immediately.
  if (state_->fin_sent || !is_writable()) {
    Debug(this, "Shutdown write immediately");
    return 1;
  }
  Debug(this, "Deferred shutdown. Waiting for fin sent");

  CHECK_NULL(shutdown_done_);
  CHECK_NOT_NULL(req_wrap);
  shutdown_done_ = [=](int status) {
    CHECK_NOT_NULL(req_wrap);
    shutdown_done_ = nullptr;
    req_wrap->Done(status);
  };

  QuicSession::SendSessionScope send_scope(session());

  Debug(this, "Shutdown writable side");
  RecordTimestamp(&QuicStreamStats::closing_at);
  state_->write_ended = 1;
  streambuf_.End();
  session()->ResumeStream(stream_id_);

  return 0;
}

int QuicStream::DoWrite(
    WriteWrap* req_wrap,
    uv_buf_t* bufs,
    size_t nbufs,
    uv_stream_t* send_handle) {
  CHECK_NULL(send_handle);
  CHECK(!streambuf_.is_ended());

  // A write should not have happened if we've been destroyed or
  // the QuicStream is no longer (or was never) writable.
  if (is_destroyed() || !is_writable()) {
    req_wrap->Done(UV_EPIPE);
    return 0;
  }

  // Nothing to write.
  size_t length = get_length(bufs, nbufs);
  if (length == 0) {
    req_wrap->Done(0);
    return 0;
  }

  QuicSession::SendSessionScope send_scope(session());

  Debug(this, "Queuing %" PRIu64 " bytes of data from %d buffers",
        length, nbufs);
  IncrementStat(&QuicStreamStats::bytes_sent, static_cast<uint64_t>(length));

  BaseObjectPtr<AsyncWrap> strong_ref{req_wrap->GetAsyncWrap()};
  // The list of buffers will be appended onto streambuf_ without
  // copying. Those will remain in the buffer until the serialized
  // stream frames are acknowledged.
  // This callback function will be invoked once this
  // complete batch of buffers has been acknowledged
  // by the peer. This will have the side effect of
  // blocking additional pending writes from the
  // javascript side, so writing data to the stream
  // will be throttled by how quickly the peer is
  // able to acknowledge stream packets. This is good
  // in the sense of providing back-pressure, but
  // also means that writes will be significantly
  // less performant unless written in batches.
  streambuf_.Push(
      bufs,
      nbufs,
      [req_wrap, strong_ref](int status) {
        req_wrap->Done(status);
      });

  // If end() was called on the JS side, the write_ended flag
  // will have been set. This allows us to know early if this
  // is the final chunk. But this is only only to be triggered
  // if end() was called with a final chunk of data to write.
  // Otherwise, we have to wait for DoShutdown to be called.
  if (state_->write_ended == 1) {
    RecordTimestamp(&QuicStreamStats::closing_at);
    streambuf_.End();
  }

  session()->ResumeStream(stream_id_);

  return 0;
}

bool QuicStream::IsAlive() {
  return !is_destroyed() && !IsClosing();
}

bool QuicStream::IsClosing() {
  return !is_writable() && !is_readable();
}

int QuicStream::ReadStart() {
  CHECK(!is_destroyed());
  CHECK(is_readable());
  state_->read_started = 1;
  state_->read_paused = 0;
  IncrementStat(
      &QuicStreamStats::max_offset,
      inbound_consumed_data_while_paused_);
  session_->ExtendStreamOffset(id(), inbound_consumed_data_while_paused_);
  return 0;
}

int QuicStream::ReadStop() {
  CHECK(!is_destroyed());
  CHECK(is_readable());
  state_->read_paused = 1;
  return 0;
}

void QuicStream::IncrementStats(size_t datalen) {
  uint64_t len = static_cast<uint64_t>(datalen);
  IncrementStat(&QuicStreamStats::bytes_received, len);
  RecordRate(&QuicStreamStats::received_at);
  RecordSize(len);
}

void QuicStream::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("buffer", &streambuf_);
  StatsBase::StatsMemoryInfo(tracker);
  tracker->TrackField("headers", headers_);
}

BaseObjectPtr<QuicStream> QuicStream::New(
    QuicSession* session,
    int64_t stream_id,
    int64_t push_id) {
  Local<Object> obj;
  if (!session->env()
              ->quicserverstream_instance_template()
              ->NewInstance(session->env()->context()).ToLocal(&obj)) {
    return {};
  }
  BaseObjectPtr<QuicStream> stream =
      MakeDetachedBaseObject<QuicStream>(
          session,
          obj,
          stream_id,
          push_id);
  CHECK(stream);
  session->AddStream(stream);
  return stream;
}

// Passes chunks of data on to the JavaScript side as soon as they are
// received but only if we're still readable. The caller of this must have a
// HandleScope.
//
// Note that this is pushing data to the JS side regardless of whether
// anything is listening. For flow-control, we only send window updates
// to the sending peer if the stream is in flowing mode, so the sender
// should not be sending too much data.
void QuicStream::ReceiveData(
    uint32_t flags,
    const uint8_t* data,
    size_t datalen,
    uint64_t offset) {
  CHECK(!is_destroyed());
  Debug(this, "Receiving %d bytes. Final? %s. Readable? %s",
        datalen,
        flags & NGTCP2_STREAM_DATA_FLAG_FIN ? "yes" : "no",
        is_readable() ? "yes" : "no");

  // If the QuicStream is not (or was never) readable, just ignore the chunk.
  if (!is_readable())
    return;

  // ngtcp2 guarantees that datalen will only be 0 if fin is set.
  // Let's just make sure.
  CHECK(datalen > 0 || flags & NGTCP2_STREAM_DATA_FLAG_FIN);

  // ngtcp2 guarantees that offset is always greater than the previously
  // received offset. Let's just make sure.
  CHECK_GE(offset, GetStat(&QuicStreamStats::max_offset_received));
  SetStat(&QuicStreamStats::max_offset_received, offset);

  if (datalen > 0) {
    // IncrementStats will update the data_rx_rate_ and data_rx_size_
    // histograms. These will provide data necessary to detect and
    // prevent Slow Send DOS attacks specifically by allowing us to
    // see if a connection is sending very small chunks of data at very
    // slow speeds. It is important to emphasize, however, that slow send
    // rates may be perfectly legitimate so we cannot simply take blanket
    // action when slow rates are detected. Nor can we reliably define what
    // a slow rate even is! Will will need to determine some reasonable
    // default and allow user code to change the default as well as determine
    // what action to take. The current strategy will be to trigger an event
    // on the stream when data transfer rates are likely to be considered too
    // slow.
    IncrementStats(datalen);

    while (datalen > 0) {
      uv_buf_t buf = EmitAlloc(datalen);
      size_t avail = std::min(static_cast<size_t>(buf.len), datalen);

      // For now, we're allocating and copying. Once we determine if we can
      // safely switch to a non-allocated mode like we do with http2 streams,
      // we can make this branch more efficient by using the LIKELY
      // optimization. The way ngtcp2 currently works, however, we have
      // to memcpy here.
      if (UNLIKELY(buf.base == nullptr))
        buf.base = reinterpret_cast<char*>(const_cast<uint8_t*>(data));
      else
        memcpy(buf.base, data, avail);
      data += avail;
      datalen -= avail;
      // Capture read_paused before EmitRead in case user code callbacks
      // alter the state when EmitRead is called.
      bool read_paused = state_->read_paused == 1;
      EmitRead(avail, buf);
      // Reading can be paused while we are processing. If that's
      // the case, we still want to acknowledge the current bytes
      // so that pausing does not throw off our flow control.
      if (read_paused) {
        inbound_consumed_data_while_paused_ += avail;
      } else {
        IncrementStat(&QuicStreamStats::max_offset, avail);
        session_->ExtendStreamOffset(id(), avail);
      }
    }
  }

  // When fin != 0, we've received that last chunk of data for this
  // stream, indicating that the stream will no longer be readable.
  if (flags & NGTCP2_STREAM_DATA_FLAG_FIN) {
    set_final_size(offset + datalen);
    EmitRead(UV_EOF);
  }
}

int QuicStream::DoPull(
    bob::Next<ngtcp2_vec> next,
    int options,
    ngtcp2_vec* data,
    size_t count,
    size_t max_count_hint) {
  return streambuf_.Pull(
      std::move(next),
      options,
      data,
      count,
      max_count_hint);
}

// JavaScript API
namespace {
void QuicStreamGetID(const FunctionCallbackInfo<Value>& args) {
  QuicStream* stream;
  ASSIGN_OR_RETURN_UNWRAP(&stream, args.Holder());
  args.GetReturnValue().Set(static_cast<double>(stream->id()));
}

void OpenUnidirectionalStream(const FunctionCallbackInfo<Value>& args) {
  CHECK(!args.IsConstructCall());
  CHECK(args[0]->IsObject());
  QuicSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args[0].As<Object>());

  int64_t stream_id;
  if (!session->OpenUnidirectionalStream(&stream_id))
    return;

  BaseObjectPtr<QuicStream> stream = QuicStream::New(session, stream_id);
  args.GetReturnValue().Set(stream->object());
}

void OpenBidirectionalStream(const FunctionCallbackInfo<Value>& args) {
  CHECK(!args.IsConstructCall());
  CHECK(args[0]->IsObject());
  QuicSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args[0].As<Object>());

  int64_t stream_id;
  if (!session->OpenBidirectionalStream(&stream_id))
    return;

  BaseObjectPtr<QuicStream> stream = QuicStream::New(session, stream_id);
  args.GetReturnValue().Set(stream->object());
}

void QuicStreamDestroy(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  QuicStream* stream;
  ASSIGN_OR_RETURN_UNWRAP(&stream, args.Holder());
  QuicError error(env, args[0], args[1], QUIC_ERROR_APPLICATION);
  stream->Destroy(&error);
}

void QuicStreamReset(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  QuicStream* stream;
  ASSIGN_OR_RETURN_UNWRAP(&stream, args.Holder());

  QuicError error(env, args[0], args[1], QUIC_ERROR_APPLICATION);

  stream->ResetStream(
      error.family == QUIC_ERROR_APPLICATION ?
          error.code : static_cast<uint64_t>(NGTCP2_NO_ERROR));
}

void QuicStreamStopSending(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  QuicStream* stream;
  ASSIGN_OR_RETURN_UNWRAP(&stream, args.Holder());

  QuicError error(env, args[0], args[1], QUIC_ERROR_APPLICATION);

  stream->StopSending(
      error.family == QUIC_ERROR_APPLICATION ?
          error.code : static_cast<uint64_t>(NGTCP2_NO_ERROR));
}

// Requests transmission of a block of informational headers. Not all
// QUIC Applications will support headers. If headers are not supported,
// This will set the return value to false, otherwise the return value
// is set to true
void QuicStreamSubmitInformation(const FunctionCallbackInfo<Value>& args) {
  QuicStream* stream;
  ASSIGN_OR_RETURN_UNWRAP(&stream, args.Holder());
  CHECK(args[0]->IsArray());
  args.GetReturnValue().Set(stream->SubmitInformation(args[0].As<Array>()));
}

// Requests transmission of a block of initial headers. Not all
// QUIC Applications will support headers. If headers are not supported,
// this will set the return value to false, otherwise the return value
// is set to true. For http/3, these may be request or response headers.
void QuicStreamSubmitHeaders(const FunctionCallbackInfo<Value>& args) {
  QuicStream* stream;
  ASSIGN_OR_RETURN_UNWRAP(&stream, args.Holder());
  CHECK(args[0]->IsArray());
  uint32_t flags = QUICSTREAM_HEADER_FLAGS_NONE;
  CHECK(args[1]->Uint32Value(stream->env()->context()).To(&flags));
  args.GetReturnValue().Set(stream->SubmitHeaders(args[0].As<Array>(), flags));
}

// Requests transmission of a block of trailing headers. Not all
// QUIC Applications will support headers. If headers are not supported,
// this will set the return value to false, otherwise the return value
// is set to true.
void QuicStreamSubmitTrailers(const FunctionCallbackInfo<Value>& args) {
  QuicStream* stream;
  ASSIGN_OR_RETURN_UNWRAP(&stream, args.Holder());
  CHECK(args[0]->IsArray());
  args.GetReturnValue().Set(stream->SubmitTrailers(args[0].As<Array>()));
}

// Requests creation of a push stream. Not all QUIC Applications will
// support push streams. If pushes are not supported, the return value
// will be undefined, otherwise the return value will be the created
// QuicStream representing the push.
void QuicStreamSubmitPush(const FunctionCallbackInfo<Value>& args) {
  QuicStream* stream;
  ASSIGN_OR_RETURN_UNWRAP(&stream, args.Holder());
  CHECK(args[0]->IsArray());
  BaseObjectPtr<QuicStream> push_stream =
      stream->SubmitPush(args[0].As<Array>());
  if (push_stream)
    args.GetReturnValue().Set(push_stream->object());
}

}  // namespace

void QuicStream::Initialize(
    Environment* env,
    Local<Object> target,
    Local<Context> context) {
  Isolate* isolate = env->isolate();
  Local<String> class_name = FIXED_ONE_BYTE_STRING(isolate, "QuicStream");
  Local<FunctionTemplate> stream = FunctionTemplate::New(env->isolate());
  stream->SetClassName(class_name);
  stream->Inherit(AsyncWrap::GetConstructorTemplate(env));
  StreamBase::AddMethods(env, stream);
  Local<ObjectTemplate> streamt = stream->InstanceTemplate();
  streamt->SetInternalFieldCount(StreamBase::kInternalFieldCount);
  streamt->Set(env->owner_symbol(), Null(env->isolate()));
  env->SetProtoMethod(stream, "destroy", QuicStreamDestroy);
  env->SetProtoMethod(stream, "resetStream", QuicStreamReset);
  env->SetProtoMethod(stream, "stopSending", QuicStreamStopSending);
  env->SetProtoMethod(stream, "id", QuicStreamGetID);
  env->SetProtoMethod(stream, "submitInformation", QuicStreamSubmitInformation);
  env->SetProtoMethod(stream, "submitHeaders", QuicStreamSubmitHeaders);
  env->SetProtoMethod(stream, "submitTrailers", QuicStreamSubmitTrailers);
  env->SetProtoMethod(stream, "submitPush", QuicStreamSubmitPush);
  env->set_quicserverstream_instance_template(streamt);
  target->Set(env->context(),
              class_name,
              stream->GetFunction(env->context()).ToLocalChecked()).Check();

  env->SetMethod(target, "openBidirectionalStream", OpenBidirectionalStream);
  env->SetMethod(target, "openUnidirectionalStream", OpenUnidirectionalStream);
}

}  // namespace quic
}  // namespace node
