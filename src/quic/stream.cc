#include "quic/buffer.h"
#include "quic/crypto.h"
#include "quic/endpoint.h"
#include "quic/session.h"
#include "quic/stream.h"
#include "quic/quic.h"
#include "aliased_struct-inl.h"
#include "async_wrap-inl.h"
#include "base_object-inl.h"
#include "debug_utils-inl.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "node_bob-inl.h"
#include "node_errors.h"
#include "node_http_common-inl.h"
#include "node_sockaddr-inl.h"
#include "v8.h"

#include <string>

namespace node {

using v8::Array;
using v8::BigInt;
using v8::Context;
using v8::EscapableHandleScope;
using v8::Exception;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Object;
using v8::PropertyAttribute;
using v8::String;
using v8::Uint32;
using v8::Undefined;
using v8::Value;

namespace quic {

namespace {
MaybeLocal<Value> AppErrorCodeToException(
    Environment* env,
    error_code app_error_code) {
  if (app_error_code == NGHTTP3_H3_NO_ERROR ||
      app_error_code == NGTCP2_NO_ERROR) {
    return MaybeLocal<Value>();
  }

  BindingState* state = BindingState::Get(env);
  EscapableHandleScope scope(env->isolate());
  Local<Value> arg;

  std::string message =
      SPrintF("Stream closed", static_cast<uint64_t>(app_error_code));
  Local<String> msg =
      OneByteString(env->isolate(), message.c_str(), message.length());
  Local<Object> except;
  if (!Exception::Error(msg)->ToObject(env->context()).ToLocal(&except))
    return MaybeLocal<Value>();
  if (except->Set(env->context(),
                  env->code_string(),
                  state->err_stream_closed_string()).IsNothing()) {
    return MaybeLocal<Value>();
  }
  arg = except;

  return scope.Escape(arg);
}
}  // namespace

Local<FunctionTemplate> Stream::GetConstructorTemplate(Environment* env) {
  BindingState* state = env->GetBindingData<BindingState>(env->context());
  CHECK_NOT_NULL(state);
  Local<FunctionTemplate> tmpl = state->stream_constructor_template();
  if (tmpl.IsEmpty()) {
    tmpl = FunctionTemplate::New(env->isolate());
    tmpl->SetClassName(FIXED_ONE_BYTE_STRING(env->isolate(), "Stream"));
    tmpl->Inherit(AsyncWrap::GetConstructorTemplate(env));
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        Stream::kInternalFieldCount);
    env->SetProtoMethod(tmpl, "destroy", DoDestroy);
    env->SetProtoMethod(tmpl, "attachSource", AttachSource);
    env->SetProtoMethod(tmpl, "attachConsumer", AttachConsumer);
    env->SetProtoMethod(tmpl, "sendHeaders", DoSendHeaders);
    state->set_stream_constructor_template(tmpl);
  }
  return tmpl;
}

bool Stream::HasInstance(Environment* env, const Local<Object>& obj) {
  return GetConstructorTemplate(env)->HasInstance(obj);
}

void Stream::Initialize(Environment* env, Local<Object> target) {
  USE(GetConstructorTemplate(env));

  JSQuicBufferConsumer::Initialize(env, target);
  ArrayBufferViewSource::Initialize(env, target);
  StreamSource::Initialize(env, target);
  StreamBaseSource::Initialize(env, target);
  BlobSource::Initialize(env, target);

#define V(name, _, __) NODE_DEFINE_CONSTANT(target, IDX_STATS_STREAM_##name);
  STREAM_STATS(V)
  NODE_DEFINE_CONSTANT(target, IDX_STATS_STREAM_COUNT);
#undef V
#define V(name, _, __) NODE_DEFINE_CONSTANT(target, IDX_STATE_STREAM_##name);
  STREAM_STATE(V)
  NODE_DEFINE_CONSTANT(target, IDX_STATE_STREAM_COUNT);
#undef V

  constexpr int QUIC_STREAM_HEADERS_KIND_INFO =
      static_cast<int>(Stream::HeadersKind::INFO);
  constexpr int QUIC_STREAM_HEADERS_KIND_INITIAL =
      static_cast<int>(Stream::HeadersKind::INITIAL);
  constexpr int QUIC_STREAM_HEADERS_KIND_TRAILING =
      static_cast<int>(Stream::HeadersKind::TRAILING);

  constexpr int QUIC_STREAM_HEADERS_FLAGS_NONE =
      static_cast<int>(SendHeadersFlags::NONE);
  constexpr int QUIC_STREAM_HEADERS_FLAGS_TERMINAL =
      static_cast<int>(SendHeadersFlags::TERMINAL);

  NODE_DEFINE_CONSTANT(target, QUIC_STREAM_HEADERS_KIND_INFO);
  NODE_DEFINE_CONSTANT(target, QUIC_STREAM_HEADERS_KIND_INITIAL);
  NODE_DEFINE_CONSTANT(target, QUIC_STREAM_HEADERS_KIND_TRAILING);

  NODE_DEFINE_CONSTANT(target, QUIC_STREAM_HEADERS_FLAGS_NONE);
  NODE_DEFINE_CONSTANT(target, QUIC_STREAM_HEADERS_FLAGS_TERMINAL);
}

BaseObjectPtr<Stream> Stream::Create(
    Environment* env,
    Session* session,
    stream_id id) {
  Local<Object> obj;
  Local<FunctionTemplate> tmpl = GetConstructorTemplate(env);
  CHECK(!tmpl.IsEmpty());
  if (!tmpl->InstanceTemplate()->NewInstance(env->context()).ToLocal(&obj))
    return BaseObjectPtr<Stream>();

  return MakeBaseObject<Stream>(session, obj, id);
}

Stream::Stream(
    Session* session,
    Local<Object> object,
    stream_id id,
    Buffer::Source* source)
    : AsyncWrap(session->env(), object, AsyncWrap::PROVIDER_QUICSTREAM),
      StreamStatsBase(session->env()),
      session_(session),
      state_(session->env()),
      id_(id) {
  MakeWeak();
  Debug(this, "Created");

  AttachOutboundSource(source);

  object->DefineOwnProperty(
      env()->context(),
      env()->state_string(),
      state_.GetArrayBuffer(),
      PropertyAttribute::ReadOnly).Check();

  object->DefineOwnProperty(
      env()->context(),
      env()->stats_string(),
      ToBigUint64Array(env()),
      PropertyAttribute::ReadOnly).Check();

  object->DefineOwnProperty(
      env()->context(),
      env()->id_string(),
      BigInt::New(env()->isolate(), static_cast<int64_t>(id)),
      PropertyAttribute::ReadOnly).Check();

  ngtcp2_transport_params params;
  ngtcp2_conn_get_local_transport_params(session->connection(), &params);
  IncrementStat(&StreamStats::max_offset, params.initial_max_data);
}

Stream::~Stream() {
  DebugStats(this);
}

void Stream::Acknowledge(uint64_t offset, size_t datalen) {
  if (is_destroyed() || outbound_source_ == nullptr)
    return;

  // ngtcp2 guarantees that offset must always be greater
  // than the previously received offset.
  DCHECK_GE(offset, GetStat(&StreamStats::max_offset_ack));
  SetStat(&StreamStats::max_offset_ack, offset);

  Debug(this, "Acknowledging %d bytes", datalen);

  // Consumes the given number of bytes in the buffer.
  CHECK_LE(outbound_source_->Acknowledge(offset, datalen), datalen);
}

bool Stream::AddHeader(std::unique_ptr<Header> header) {
  size_t len = header->length();
  Session::Application* app = session()->application();
  if (!app->CanAddHeader(headers_.size(), current_headers_length_, len))
    return false;

  current_headers_length_ += len;
  Debug(this, "Header - %s", header.get());

  BindingState* state = BindingState::Get(env());

  {
    EscapableHandleScope scope(env()->isolate());
    Local<Value> value;
    if (UNLIKELY(!header->GetName(state).ToLocal(&value)))
      return false;
    headers_.push_back(scope.Escape(value));
  }

  {
    EscapableHandleScope scope(env()->isolate());
    Local<Value> value;
    if (UNLIKELY(!header->GetValue(state).ToLocal(&value)))
      return false;
    headers_.push_back(scope.Escape(value));
  }

  return true;
}

void Stream::AttachInboundConsumer(
    Buffer::Consumer* consumer,
    BaseObjectPtr<AsyncWrap> strong_ptr) {
  CHECK_IMPLIES(strong_ptr, consumer != nullptr);
  Debug(this, "%s data consumer",
      consumer != nullptr ? "Attaching" : "Clearing");
  inbound_consumer_ = consumer;
  inbound_consumer_strong_ptr_ = std::move(strong_ptr);
  ProcessInbound();
}

void Stream::AttachOutboundSource(Buffer::Source* source) {
  Debug(this, "%s data source",
        source != nullptr ? "Attaching" : "Clearing");
  outbound_source_ = source;
  if (source != nullptr) {
    outbound_source_strong_ptr_ = source->GetStrongPtr();
    source->set_owner(this);
  } else {
    outbound_source_strong_ptr_.reset();
  }
  Resume();
}

void Stream::BeginHeaders(HeadersKind kind) {
  Debug(this, "Beginning Headers");
  headers_.clear();
  set_headers_kind(kind);
}

void Stream::OnBlocked() {
  BindingState* state = BindingState::Get(env());
  HandleScope handle_scope(env()->isolate());
  Context::Scope context_scope(env()->context());

  BaseObjectPtr<Stream> ptr(this);
  USE(state->stream_blocked_callback()->Call(
      env()->context(),
      object(),
      0, nullptr));
}

void Stream::OnReset(error_code app_error_code) {
  BindingState* state = BindingState::Get(env());
  HandleScope scope(env()->isolate());
  Context::Scope context_scope(env()->context());

  Local<Value> arg;
  if (AppErrorCodeToException(env(), app_error_code).ToLocal(&arg) &&
      outbound_source_ != nullptr) {
    outbound_source_->RejectDone(arg);
  } else {
    arg = Undefined(env()->isolate());
  }

  BaseObjectPtr<Stream> ptr(this);

  USE(state->stream_reset_callback()->Call(
      env()->context(),
      session()->object(),
      1, &arg));
}

void Stream::OnClose(error_code app_error_code) {
  BindingState* state = BindingState::Get(env());
  HandleScope scope(env()->isolate());
  Context::Scope context_scope(env()->context());

  Local<Value> arg;
  if (AppErrorCodeToException(env(), app_error_code).ToLocal(&arg) &&
      outbound_source_ != nullptr) {
    outbound_source_->RejectDone(arg);
  } else {
    arg = Undefined(env()->isolate());
  }

  BaseObjectPtr<Stream> ptr(this);
  USE(state->stream_close_callback()->Call(
      env()->context(),
      object(),
      1, &arg));
}

// Sends a signal to the remote peer to stop transmitting.
void Stream::StopSending(const QuicError& error) {
  CHECK_EQ(error.type, QuicError::Type::APPLICATION);
  Session::SendSessionScope send_scope(session());
  ngtcp2_conn_shutdown_stream_read(
      session()->connection(),
      id_,
      error.code);
  state_->read_ended = 1;
}

void Stream::Commit(size_t amount) {
  CHECK(!is_destroyed());
  if (outbound_source_ == nullptr)
    return;
  size_t actual = outbound_source_->Seek(amount);
  CHECK_LE(actual, amount);
}

int Stream::DoPull(
    bob::Next<ngtcp2_vec> next,
    int options,
    ngtcp2_vec* data,
    size_t count,
    size_t max_count_hint) {
  Debug(this, "Pulling outbound data for serialization");
  // If an outbound source has not yet been attached, block until
  // one is available. When AttachOutboundSource() is called the
  // stream will be resumed.
  if (outbound_source_ == nullptr) {
    int status = bob::Status::STATUS_BLOCK;
    std::move(next)(status, nullptr, 0, [](size_t len) {});
    return status;
  }

  return outbound_source_->Pull(
      std::move(next),
      options,
      data,
      count,
      max_count_hint);
}

void Stream::EndHeaders() {
  Debug(this, "End Headers");

  if (headers_.size() == 0)
    return;

  BindingState* state = BindingState::Get(env());
  HandleScope scope(env()->isolate());
  Context::Scope context_scope(env()->context());

  Local<Value> argv[] = {
    Array::New(env()->isolate(), headers_.data(), headers_.size()),
    Integer::NewFromUnsigned(
        env()->isolate(),
        static_cast<uint32_t>(headers_kind_))
  };

  headers_.clear();

  BaseObjectPtr<Stream> ptr(this);
  USE(state->stream_headers_callback()->Call(
      env()->context(),
      object(),
      arraysize(argv),
      argv));
}

void Stream::DoSendHeaders(const FunctionCallbackInfo<Value>& args) {
  Stream* stream;
  ASSIGN_OR_RETURN_UNWRAP(&stream, args.Holder());
  CHECK(args[0]->IsUint32());  // Kind
  CHECK(args[1]->IsArray());  // Headers
  CHECK(args[2]->IsUint32());  // Flags

  HeadersKind kind =
      static_cast<HeadersKind>(args[0].As<Uint32>()->Value());
  Local<Array> headers = args[1].As<Array>();
  SendHeadersFlags flags =
      static_cast<SendHeadersFlags>(args[2].As<Uint32>()->Value());

  args.GetReturnValue().Set(stream->SendHeaders(kind, headers, flags));
}

void Stream::DoDestroy(const FunctionCallbackInfo<Value>& args) {
  Stream* stream;
  ASSIGN_OR_RETURN_UNWRAP(&stream, args.Holder());
  stream->Destroy();
}

void Stream::AttachSource(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Stream* stream;
  ASSIGN_OR_RETURN_UNWRAP(&stream, args.Holder());

  CHECK_IMPLIES(!args[0]->IsUndefined(), args[0]->IsObject());

  Buffer::Source* source = nullptr;

  if (args[0]->IsUndefined()) {
    BaseObjectPtr<NullSource> null_source = NullSource::Create(env);
    CHECK(null_source);
    source = null_source.get();
  } else if (ArrayBufferViewSource::HasInstance(env, args[0])) {
    ArrayBufferViewSource* view;
    ASSIGN_OR_RETURN_UNWRAP(&view, args[0]);
    source = view;
  } else if (StreamSource::HasInstance(env, args[0])) {
    StreamSource* view;
    ASSIGN_OR_RETURN_UNWRAP(&view, args[0]);
    source = view;
  } else if (StreamBaseSource::HasInstance(env, args[0])) {
    StreamBaseSource* view;
    ASSIGN_OR_RETURN_UNWRAP(&view, args[0]);
    source = view;
  } else if (BlobSource::HasInstance(env, args[0])) {
    BlobSource* blob;
    ASSIGN_OR_RETURN_UNWRAP(&blob, args[0]);
    source = blob;
  } else {
    UNREACHABLE();
  }

  stream->AttachOutboundSource(source);
}

void Stream::AttachConsumer(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Stream* stream;
  ASSIGN_OR_RETURN_UNWRAP(&stream, args.Holder());

  if (JSQuicBufferConsumer::HasInstance(env, args[0])) {
    JSQuicBufferConsumer* consumer;
    ASSIGN_OR_RETURN_UNWRAP(&consumer, args[0]);
    stream->AttachInboundConsumer(consumer, BaseObjectPtr<AsyncWrap>(consumer));
  } else {
    UNREACHABLE();
  }
}

void Stream::Destroy() {
  if (destroyed_)
    return;
  destroyed_ = true;

  // Removes the stream from the outbound send queue
  Unschedule();

  // Detach stream sources...
  if (outbound_source_ != nullptr) {
    outbound_source_ = nullptr;
    outbound_source_strong_ptr_.reset();
  }

  if (!inbound_.is_ended()) {
    inbound_.End();
    ProcessInbound();
  }

  // Remove the stream from the owning session and reset the pointer
  session()->RemoveStream(id_);
  session_.reset();
}

void Stream::ProcessInbound() {
  // If there is no inbound consumer, do nothing.
  if (inbound_consumer_ == nullptr)
    return;

  Debug(this, "Releasing the inbound queue to the consumer");

  Maybe<size_t> amt = inbound_.Release(inbound_consumer_);
  if (amt.IsNothing()) {
    Debug(this, "Failed to process the inbound queue");
    return Destroy();
  }
  size_t len = amt.FromJust();

  Debug(this, "Released %" PRIu64 " bytes to consumer", len);
  IncrementStat(&StreamStats::max_offset, len);

  if (session_)
    session_->ExtendStreamOffset(id_, len);
}

void Stream::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("outbound", outbound_source_);
  tracker->TrackField("outbound_strong_ptr", outbound_source_strong_ptr_);
  tracker->TrackField("inbound", inbound_);
  tracker->TrackField("inbound_consumer_strong_ptr_",
                      inbound_consumer_strong_ptr_);
  tracker->TrackField("headers", headers_);
  StatsBase::StatsMemoryInfo(tracker);
}

void Stream::ReadyForTrailers() {
  if (LIKELY(!trailers())) return;

  BindingState* state = BindingState::Get(env());
  HandleScope handle_scope(env()->isolate());
  Context::Scope context_scope(env()->context());

  BaseObjectPtr<Stream> ptr(this);
  USE(state->stream_trailers_callback()->Call(
      env()->context(),
      object(),
      0, nullptr));
}

void Stream::ReceiveData(
    uint32_t flags,
    const uint8_t* data,
    size_t datalen,
    uint64_t offset) {
  CHECK(!is_destroyed());
  Debug(this, "Receiving %d bytes. Final? %s",
        datalen,
        flags & NGTCP2_STREAM_DATA_FLAG_FIN ? "yes" : "no");

  // ngtcp2 guarantees that datalen will only be 0 if fin is set.
  DCHECK_IMPLIES(datalen == 0, flags & NGTCP2_STREAM_DATA_FLAG_FIN);

  // ngtcp2 guarantees that offset is greater than the previously received.
  DCHECK_GE(offset, GetStat(&StreamStats::max_offset_received));
  SetStat(&StreamStats::max_offset_received, offset);

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
    UpdateStats(datalen);
    inbound_.Push(env(), data, datalen);
  }

  if (flags & NGTCP2_STREAM_DATA_FLAG_FIN) {
    set_final_size(offset + datalen);
    inbound_.End();
  }

  ProcessInbound();
}

void Stream::ResetStream(const QuicError& error) {
  CHECK_EQ(error.type, QuicError::Type::APPLICATION);
  Session::SendSessionScope send_scope(session());
  session()->ShutdownStream(id_, error.code);
  state_->read_ended = 1;
}

void Stream::Resume() {
  CHECK(session());
  Session::SendSessionScope send_scope(session());
  Debug(this, "Resuming stream %" PRIu64, id_);
  if (outbound_source_ != nullptr && !outbound_source_->is_finished())
    session()->ResumeStream(id_);
}

bool Stream::SendHeaders(
    HeadersKind kind,
    const Local<Array>& headers,
    SendHeadersFlags flags) {
  return session_->application()->SendHeaders(id_, kind, headers, flags);
}

void Stream::UpdateStats(size_t datalen) {
  uint64_t len = static_cast<uint64_t>(datalen);
  IncrementStat(&StreamStats::bytes_received, len);
}

void Stream::set_final_size(uint64_t final_size) {
  CHECK_IMPLIES(
      state_->fin_received == 1,
      final_size <= GetStat(&StreamStats::final_size));
  state_->fin_received = 1;
  SetStat(&StreamStats::final_size, final_size);
  Debug(this, "Set final size to %" PRIu64, final_size);
}

void Stream::Schedule(Queue* queue) {
  if (stream_queue_.IsEmpty()) {
    queue->PushBack(this);
  }
}

template <>
void StatsTraitsImpl<StreamStats, Stream>::ToString(
    const Stream& ptr,
    AddStatsField add_field) {
#define V(_, name, label) add_field(label, ptr.GetStat(&StreamStats::name));
  STREAM_STATS(V)
#undef V
}

}  // namespace quic
}  // namespace node
