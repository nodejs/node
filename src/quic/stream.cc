#include "stream.h"
#include <aliased_struct-inl.h>
#include <async_wrap-inl.h>
#include <base_object-inl.h>
#include <crypto/crypto_random.h>
#include <debug_utils-inl.h>
#include <env-inl.h>
#include <memory_tracker-inl.h>
#include <ngtcp2/ngtcp2.h>
#include <node_bob-inl.h>
#include <node_errors.h>
#include <stream_base-inl.h>
#include <util.h>
#include <v8.h>
#include "endpoint.h"
#include "quic.h"
#include "session.h"

namespace node {

using v8::Array;
using v8::ArrayBuffer;
using v8::ArrayBufferView;
using v8::BigInt;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Integer;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Nothing;
using v8::Object;
using v8::PropertyAttribute;
using v8::Uint32;
using v8::Uint8Array;
using v8::Undefined;
using v8::Value;

namespace quic {

// ======================================================================================
// The NullSource is used when no payload source is provided for a Stream.
// Whenever DoPull is called, it simply immediately responds with no data and
// EOS set.
namespace {
class NullSource final : public Buffer::Source {
 public:
  NullSource() : Source() {}

  BaseObjectPtr<BaseObject> GetStrongPtr() override {
    return BaseObjectPtr<BaseObject>();
  }

  size_t Acknowledge(uint64_t offset, size_t datalen) override { return 0; }
  size_t Seek(size_t amount) override { return 0; }
  bool is_closed() const override { return true; }
  bool is_finished() const override { return true; }
  void set_finished() override {}
  void set_closed() override {}

  int DoPull(bob::Next<ngtcp2_vec> next,
             int options,
             ngtcp2_vec* data,
             size_t count,
             size_t max_count_hint) override {
    std::move(next)(bob::Status::STATUS_END, nullptr, 0, [](size_t len) {});
    return bob::Status::STATUS_END;
  }

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(NullSource)
  SET_SELF_SIZE(NullSource)
};

NullSource null_source_;
}  // namespace

// ======================================================================================
// Stream

Stream* Stream::From(ngtcp2_conn* conn, void* stream_user_data) {
  Stream* stream = static_cast<Stream*>(stream_user_data);
  CHECK_NOT_NULL(stream);
  CHECK_EQ(stream->session()->connection(), conn);
  return stream;
}

Local<FunctionTemplate> Stream::GetConstructorTemplate(Environment* env) {
  auto& state = BindingState::Get(env);
  Local<FunctionTemplate> tmpl = state.stream_constructor_template();
  if (tmpl.IsEmpty()) {
    auto isolate = env->isolate();
    tmpl = NewFunctionTemplate(isolate, IllegalConstructor);
    tmpl->SetClassName(FIXED_ONE_BYTE_STRING(isolate, "Stream"));
    tmpl->Inherit(AsyncWrap::GetConstructorTemplate(env));
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        Stream::kInternalFieldCount);
    SetProtoMethod(isolate, tmpl, "destroy", DoDestroy);
    SetProtoMethod(isolate, tmpl, "attachSource", AttachSource);
    SetProtoMethod(isolate, tmpl, "flushInbound", FlushInbound);
    SetProtoMethod(isolate, tmpl, "sendHeaders", DoSendHeaders);
    SetProtoMethod(isolate, tmpl, "stopSending", DoStopSending);
    SetProtoMethod(isolate, tmpl, "resetStream", DoResetStream);
    SetProtoMethod(isolate, tmpl, "setPriority", DoSetPriority);
    SetProtoMethodNoSideEffect(isolate, tmpl, "getPriority", DoGetPriority);
    state.set_stream_constructor_template(tmpl);
  }
  return tmpl;
}

void Stream::RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(DoDestroy);
  registry->Register(AttachSource);
  registry->Register(FlushInbound);
  registry->Register(DoSendHeaders);
  registry->Register(DoStopSending);
  registry->Register(DoResetStream);
  ArrayBufferViewSource::RegisterExternalReferences(registry);
  StreamSource::RegisterExternalReferences(registry);
  StreamBaseSource::RegisterExternalReferences(registry);
  BlobSource::RegisterExternalReferences(registry);
}

void ArrayBufferViewSource::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(New);
}
void BlobSource::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(New);
}
void StreamSource::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(New);
  registry->Register(End);
  registry->Register(Write);
  registry->Register(WriteV);
}
void StreamBaseSource::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(New);
}

void Stream::Initialize(Environment* env, Local<Object> target) {
  USE(GetConstructorTemplate(env));

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
      static_cast<int>(Session::Application::HeadersKind::INFO);
  constexpr int QUIC_STREAM_HEADERS_KIND_INITIAL =
      static_cast<int>(Session::Application::HeadersKind::INITIAL);
  constexpr int QUIC_STREAM_HEADERS_KIND_TRAILING =
      static_cast<int>(Session::Application::HeadersKind::TRAILING);

  constexpr int QUIC_STREAM_HEADERS_FLAGS_NONE =
      static_cast<int>(Session::Application::HeadersFlags::NONE);
  constexpr int QUIC_STREAM_HEADERS_FLAGS_TERMINAL =
      static_cast<int>(Session::Application::HeadersFlags::TERMINAL);

  NODE_DEFINE_CONSTANT(target, QUIC_STREAM_HEADERS_KIND_INFO);
  NODE_DEFINE_CONSTANT(target, QUIC_STREAM_HEADERS_KIND_INITIAL);
  NODE_DEFINE_CONSTANT(target, QUIC_STREAM_HEADERS_KIND_TRAILING);

  NODE_DEFINE_CONSTANT(target, QUIC_STREAM_HEADERS_FLAGS_NONE);
  NODE_DEFINE_CONSTANT(target, QUIC_STREAM_HEADERS_FLAGS_TERMINAL);
}

BaseObjectPtr<Stream> Stream::Create(Environment* env,
                                     Session* session,
                                     stream_id id) {
  Local<Object> obj;
  Local<FunctionTemplate> tmpl = GetConstructorTemplate(env);
  CHECK(!tmpl.IsEmpty());
  if (!tmpl->InstanceTemplate()->NewInstance(env->context()).ToLocal(&obj))
    return BaseObjectPtr<Stream>();

  return MakeBaseObject<Stream>(BaseObjectPtr<Session>(session), obj, id);
}

Stream::Stream(BaseObjectPtr<Session> session,
               Local<Object> object,
               stream_id id,
               Buffer::Source* source)
    : AsyncWrap(session->env(), object, AsyncWrap::PROVIDER_QUICSTREAM),
      StreamStatsBase(session->env()),
      session_(session),
      state_(session->env()->isolate()) {
  MakeWeak();
  state_->id = id;

  USE(ngtcp2_conn_set_stream_user_data(session->connection(), id, this));
  AttachOutboundSource(source);

  const auto defineProperty = [&](auto name, auto value) {
    object
        ->DefineOwnProperty(
            env()->context(), name, value, PropertyAttribute::ReadOnly)
        .Check();
  };

  defineProperty(env()->state_string(), state_.GetArrayBuffer());
  defineProperty(env()->stats_string(), ToBigUint64Array(env()));

  auto params = ngtcp2_conn_get_local_transport_params(session->connection());
  IncrementStat(&StreamStats::max_offset, params->initial_max_data);
}

void Stream::Acknowledge(uint64_t offset, size_t datalen) {
  if (is_destroyed() || outbound_source_ == nullptr) return;

  // ngtcp2 guarantees that offset must always be greater than the previously
  // received offset.
  DCHECK_GE(offset, GetStat(&StreamStats::max_offset_ack));
  SetStat(&StreamStats::max_offset_ack, offset);

  // Consumes the given number of bytes in the buffer.
  CHECK_LE(outbound_source_->Acknowledge(offset, datalen), datalen);
}

bool Stream::AddHeader(const Header& header) {
  if (is_destroyed()) return false;
  size_t len = header.length();
  auto& app = session()->application();
  if (!app.CanAddHeader(headers_.size(), current_headers_length_, len))
    return false;

  current_headers_length_ += len;

  auto& state = BindingState::Get(env());

  const auto push = [&](auto raw) {
    Local<Value> value;
    if (UNLIKELY(!raw.ToLocal(&value))) return false;
    headers_.push_back(value);
    return true;
  };

  return push(header.GetName(&state)) && push(header.GetValue(&state));
}

void Stream::AttachOutboundSource(Buffer::Source* source) {
  outbound_source_ = source;
  outbound_source_strong_ptr_.reset();
  if (source != nullptr) {
    outbound_source_strong_ptr_ = source->GetStrongPtr();
    Resume();
  }
}

void Stream::BeginHeaders(Session::Application::HeadersKind kind) {
  if (is_destroyed()) return;
  headers_.clear();
  headers_kind_ = kind;
}

void Stream::Blocked() {
  if (is_destroyed()) return;
  EmitBlocked();
}

Maybe<size_t> Stream::EmitData(Buffer::Chunk::Queue queue, bool ended) {
  if (!env()->can_call_into_js()) return Nothing<size_t>();
  CallbackScope cb_scope(this);
  auto& state = BindingState::Get(env());

  std::vector<Local<Value>> items;
  size_t len = 0;
  while (!queue.empty()) {
    Local<Value> val;
    len += queue.front().length();
    // If this fails, the error is unrecoverable and neither is the data. Return
    // nothing to signal error and handle upstream.
    if (!queue.front().Release(env()).ToLocal(&val)) return Nothing<size_t>();
    queue.pop_front();
    items.emplace_back(val);
  }

  Local<Value> args[] = {
      Array::New(env()->isolate(), items.data(), items.size()),
      ended ? v8::True(env()->isolate()) : v8::False(env()->isolate())};
  MakeCallback(state.stream_data_callback(), arraysize(args), args);
  return Just(len);
}

void Stream::EmitBlocked() {
  if (!env()->can_call_into_js()) return;
  CallbackScope cb_scope(this);
  MakeCallback(BindingState::Get(env()).stream_blocked_callback(), 0, nullptr);
}

void Stream::EmitClose() {
  if (!env()->can_call_into_js()) return;
  CallbackScope cb_scope(this);
  MakeCallback(BindingState::Get(env()).stream_close_callback(), 0, nullptr);
}

void Stream::EmitReset(QuicError error) {
  if (!env()->can_call_into_js()) return;
  CallbackScope cb_scope(this);
  Local<Value> argv[] = {
      Integer::New(env()->isolate(), static_cast<int>(error.type())),
      BigInt::NewFromUnsigned(env()->isolate(), error.code()),
      Undefined(env()->isolate()),
  };

  if (error->reasonlen > 0 &&
      !ToV8Value(env()->context(), error.reason()).ToLocal(&argv[2])) {
    return;
  }

  MakeCallback(
      BindingState::Get(env()).stream_reset_callback(), arraysize(argv), argv);
}

void Stream::EmitError(QuicError error) {
  if (!env()->can_call_into_js()) return;
  CallbackScope cb_scope(this);
  Local<Value> argv[] = {
      Integer::New(env()->isolate(), static_cast<int>(error.type())),
      BigInt::NewFromUnsigned(env()->isolate(), error.code()),
      Undefined(env()->isolate()),
  };
  if (error->reasonlen > 0 &&
      !ToV8Value(env()->context(), error.reason()).ToLocal(&argv[2])) {
    return;
  }
  MakeCallback(
      BindingState::Get(env()).stream_error_callback(), arraysize(argv), argv);
}

void Stream::Commit(size_t amount) {
  if (is_destroyed() || outbound_source_ == nullptr) return;
  size_t actual = outbound_source_->Seek(amount);
  CHECK_LE(actual, amount);
}

int Stream::DoPull(bob::Next<ngtcp2_vec> next,
                   int options,
                   ngtcp2_vec* data,
                   size_t count,
                   size_t max_count_hint) {
  if (is_destroyed() || state_->reset == 1) return bob::Status::STATUS_EOS;
  // If an outbound source has not yet been attached, block until one is
  // available. When AttachOutboundSource() is called the stream will be
  // resumed.
  if (outbound_source_ == nullptr) {
    int status = bob::Status::STATUS_BLOCK;
    std::move(next)(status, nullptr, 0, [](size_t len) {});
    return status;
  }

  return outbound_source_->Pull(
      std::move(next), options, data, count, max_count_hint);
}

void Stream::EndHeaders() {
  // Nothing to do here.
}

void Stream::EmitHeaders() {
  if (!env()->can_call_into_js()) return;
  CallbackScope cb_scope(this);

  Local<Value> argv[] = {
      Array::New(env()->isolate(), headers_.data(), headers_.size()),
      Integer::NewFromUnsigned(env()->isolate(),
                               static_cast<uint32_t>(headers_kind_))};

  headers_.clear();

  MakeCallback(BindingState::Get(env()).stream_headers_callback(),
               arraysize(argv),
               argv);
}

void Stream::DoStopSending(const FunctionCallbackInfo<Value>& args) {
  Stream* stream;
  ASSIGN_OR_RETURN_UNWRAP(&stream, args.Holder());
  if (!args[0]->IsUndefined()) {
    CHECK(args[0]->IsBigInt());
    bool lossless = false;  // not used.
    error_code code = args[0].As<BigInt>()->Uint64Value(&lossless);
    stream->StopSending(QuicError::ForApplication(code));
  } else {
    stream->StopSending(QuicError::ForApplication(NGTCP2_APP_NOERROR));
  }
}

void Stream::DoResetStream(const FunctionCallbackInfo<Value>& args) {
  Stream* stream;
  ASSIGN_OR_RETURN_UNWRAP(&stream, args.Holder());
  if (!args[0]->IsUndefined()) {
    CHECK(args[0]->IsBigInt());
    bool lossless = false;  // not used.
    error_code code = args[0].As<BigInt>()->Uint64Value(&lossless);
    stream->ResetStream(QuicError::ForApplication(code));
  } else {
    stream->ResetStream(QuicError::ForApplication(NGTCP2_APP_NOERROR));
  }
}

void Stream::DoSendHeaders(const FunctionCallbackInfo<Value>& args) {
  Stream* stream;
  ASSIGN_OR_RETURN_UNWRAP(&stream, args.Holder());
  CHECK(args[0]->IsUint32());  // Kind
  CHECK(args[1]->IsArray());   // Headers
  CHECK(args[2]->IsUint32());  // Flags

  Session::Application::HeadersKind kind =
      static_cast<Session::Application::HeadersKind>(
          args[0].As<Uint32>()->Value());
  Local<Array> headers = args[1].As<Array>();
  Session::Application::HeadersFlags flags =
      static_cast<Session::Application::HeadersFlags>(
          args[2].As<Uint32>()->Value());

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
    source = &null_source_;
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

void Stream::EndWritable() {
  if (is_destroyed() || outbound_source_ == nullptr) return;

  Unschedule();

  outbound_source_->set_closed();
  outbound_source_->set_finished();
  outbound_source_ = nullptr;
  outbound_source_strong_ptr_.reset();
}

void Stream::FlushInbound(const FunctionCallbackInfo<Value>& args) {
  Stream* stream;
  ASSIGN_OR_RETURN_UNWRAP(&stream, args.Holder());
  stream->ProcessInbound();
}

void Stream::DoSetPriority(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Stream* stream;
  ASSIGN_OR_RETURN_UNWRAP(&stream, args.Holder());
  CHECK(args[0]->IsUint32());  // Priority
  CHECK(args[1]->IsUint32());  // Priority flag

  Session::Application::StreamPriority priority =
      static_cast<Session::Application::StreamPriority>(
          args[0].As<Uint32>()->Value());
  Session::Application::StreamPriorityFlags flags =
      static_cast<Session::Application::StreamPriorityFlags>(
          args[1].As<Uint32>()->Value());

  stream->SetPriority(priority, flags);
}

void Stream::DoGetPriority(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Stream* stream;
  ASSIGN_OR_RETURN_UNWRAP(&stream, args.Holder());
  args.GetReturnValue().Set(static_cast<uint32_t>(stream->GetPriority()));
}

void Stream::Destroy(Maybe<QuicError> error) {
  if (is_destroyed()) return;

  // End the writable before marking as destroyed.
  EndWritable();

  state_->destroyed = 1;

  DEBUG_ARGS(this, "Closing stream %" PRIi64, id());

  // Immediately try flushing any pending inbound data.
  if (!inbound_.is_ended()) {
    inbound_.End();
    state_->paused = 0;
    ProcessInbound();
  }

  auto on_exit = OnScopeLeave([&]() {
    // Remove the stream from the owning session and reset the pointer.
    // Note that this must be done after the EmitClose and EmitError
    // calls below since once the stream is removed, there might not be
    // any strong pointers remaining keeping it from being destroyed.
    session_->RemoveStream(id());
  });

  if (error.IsJust()) {
    EmitError(error.FromJust());
  } else {
    EmitClose();
  }
}

void Stream::ProcessInbound() {
  if (is_destroyed() || inbound_.is_finished()) return;

  if (state_->data == 0 ||       // We don't have a data listener
      state_->paused == 1 ||     // We are paused
      inbound_.is_finished()) {  // Or the inbound buffer has been ended and
                                 // drained.
    return;
  }

  // Dumps all of the data currently held in the inbound buffer out
  // to the JavaScript data listener, returning the amount of data
  // that was delivered.
  Maybe<size_t> amt = inbound_.Release(this);

  if (amt.IsNothing())
    return Destroy(Just(QuicError::ForNgtcp2Error(NGTCP2_ERR_INTERNAL)));

  size_t len = amt.FromJust();
  IncrementStat(&StreamStats::max_offset, len);
  if (session_) session_->ExtendStreamOffset(id(), len);
}

void Stream::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("outbound", outbound_source_);
  tracker->TrackField("outbound_strong_ptr", outbound_source_strong_ptr_);
  tracker->TrackField("inbound", inbound_);
  tracker->TrackField("headers", headers_);
  StatsBase::StatsMemoryInfo(tracker);
}

void Stream::ReadyForTrailers() {
  if (LIKELY(state_->trailers == 0)) return;

  EmitTrailers();
}

void Stream::EmitTrailers() {
  if (!env()->can_call_into_js()) return;
  CallbackScope cb_scope(this);
  MakeCallback(BindingState::Get(env()).stream_trailers_callback(), 0, nullptr);
}

void Stream::ReceiveData(Session::Application::ReceiveStreamDataFlags flags,
                         const uint8_t* data,
                         size_t datalen,
                         uint64_t offset) {
  if (is_destroyed()) return;

  // If reading has ended, do nothing and drop the data on the floor.
  if (state_->read_ended == 1) return;

  // ngtcp2 guarantees that datalen will only be 0 if fin is set.
  DCHECK_IMPLIES(datalen == 0, flags.fin);

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

  if (flags.fin) {
    set_final_size(offset + datalen);
    inbound_.End();
  }

  ProcessInbound();
}

void Stream::ReceiveResetStream(size_t final_size, QuicError error) {
  if (is_destroyed()) return;
  set_final_size(final_size);
  // Importantly, reset stream only impacts the inbound data flow.
  // It has no impact on the outbound data flow.
  inbound_.End();
  state_->read_ended = 1;
  ProcessInbound();
  EmitReset(error);
}

void Stream::ReceiveStopSending(QuicError error) {
  if (is_destroyed() || state_->read_ended) return;
  // Note that this comes from *this* endpoint, not the other side. We handle it
  // if we haven't already shutdown our *receiving* side of the stream.

  ngtcp2_conn_shutdown_stream_read(session()->connection(), id(), error.code());
  inbound_.End();
  state_->read_ended = 1;
}

void Stream::ResetStream(QuicError error) {
  if (is_destroyed()) return;
  CHECK_EQ(error.type(), QuicError::Type::APPLICATION);
  Session::SendPendingDataScope send_scope(session());
  EndWritable();
  ngtcp2_conn_shutdown_stream_write(
      session()->connection(), id(), error.code());
  state_->reset = 1;
}

void Stream::StopSending(QuicError error) {
  if (is_destroyed()) return;
  CHECK_EQ(error.type(), QuicError::Type::APPLICATION);
  Session::SendPendingDataScope send_scope(session());
  // Now we shut down the stream readable side.
  ngtcp2_conn_shutdown_stream_read(session()->connection(), id(), error.code());
  inbound_.End();
  state_->read_ended = 1;
}

void Stream::Resume() {
  if (is_destroyed() || outbound_source_ == nullptr) return;
  if (!outbound_source_->is_finished()) {
    Session::SendPendingDataScope send_scope(session());
    session()->ResumeStream(id());
  } else {
    // What are we doing here if we're already finished?
    EndWritable();
  }
}

bool Stream::SendHeaders(Session::Application::HeadersKind kind,
                         const Local<Array>& headers,
                         Session::Application::HeadersFlags flags) {
  if (is_destroyed()) return false;
  return session_->application().SendHeaders(id(), kind, headers, flags);
}

void Stream::UpdateStats(size_t datalen) {
  uint64_t len = static_cast<uint64_t>(datalen);
  IncrementStat(&StreamStats::bytes_received, len);
}

void Stream::set_final_size(uint64_t final_size) {
  CHECK_IMPLIES(state_->fin_received == 1,
                final_size <= GetStat(&StreamStats::final_size));
  state_->fin_received = 1;
  SetStat(&StreamStats::final_size, final_size);
}

void Stream::Schedule(Queue* queue) {
  if (is_destroyed() || outbound_source_ == nullptr) return;
  // If this stream is not already in the queue to send data, add it.
  if (stream_queue_.IsEmpty()) queue->PushBack(this);
}

void Stream::SetPriority(Session::Application::StreamPriority priority,
                         Session::Application::StreamPriorityFlags flags) {
  if (is_destroyed()) return;
  session_->application().SetStreamPriority(this, priority, flags);
}

Session::Application::StreamPriority Stream::GetPriority() {
  if (is_destroyed()) return Session::Application::StreamPriority::DEFAULT;
  return session_->application().GetStreamPriority(this);
}

// ======================================================================================
// Buffer::SourceObject

BaseObjectPtr<BaseObject> Buffer::Source::GetStrongPtr() {
  return BaseObjectPtr<BaseObject>();
}

void Buffer::Source::Resume() {
  if (stream != nullptr) stream->Resume();
}

Buffer::SourceObject::SourceObject(Environment* env) : state_(env->isolate()) {}

void Buffer::SourceObject::set_finished() {
  state_->eos = 1;
}

void Buffer::SourceObject::set_closed() {
  state_->closed = 1;
}

bool Buffer::SourceObject::is_finished() const {
  return state_->eos == 1;
}

bool Buffer::SourceObject::is_closed() const {
  return state_->closed == 1;
}

// ======================================================================================
// ArrayBufferViewSource

Local<FunctionTemplate> ArrayBufferViewSource::GetConstructorTemplate(
    Environment* env) {
  auto& state = BindingState::Get(env);
  Local<FunctionTemplate> tmpl =
      state.arraybufferviewsource_constructor_template();
  if (tmpl.IsEmpty()) {
    tmpl = NewFunctionTemplate(env->isolate(), New);
    tmpl->Inherit(BaseObject::GetConstructorTemplate(env));
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        BaseObject::kInternalFieldCount);
    tmpl->SetClassName(state.arraybufferviewsource_string());
    state.set_arraybufferviewsource_constructor_template(tmpl);
  }
  return tmpl;
}

void ArrayBufferViewSource::Initialize(Environment* env, Local<Object> target) {
  SetConstructorFunction(env->context(),
                         target,
                         "ArrayBufferViewSource",
                         GetConstructorTemplate(env),
                         SetConstructorFunctionFlag::NONE);
}

void ArrayBufferViewSource::New(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
  CHECK(args[0]->IsArrayBufferView());
  Environment* env = Environment::GetCurrent(args);
  new ArrayBufferViewSource(
      env, args.This(), Store(args[0].As<ArrayBufferView>()));
}

ArrayBufferViewSource::ArrayBufferViewSource(Environment* env,
                                             Local<Object> object,
                                             Store&& source)
    : Buffer::SourceObject(env),
      BaseObject(env, object),
      buffer_(std::move(source)) {
  MakeWeak();

  // All of the data for this source is provided by the provided Store
  set_closed();

  const auto defineProperty = [&](auto name, auto value) {
    object
        ->DefineOwnProperty(
            env->context(), name, value, PropertyAttribute::ReadOnly)
        .Check();
  };

  defineProperty(env->state_string(), state_.GetArrayBuffer());
}

int ArrayBufferViewSource::DoPull(bob::Next<ngtcp2_vec> next,
                                  int options,
                                  ngtcp2_vec* data,
                                  size_t count,
                                  size_t max_count_hint) {
  return buffer_.Pull(std::move(next), options, data, count, max_count_hint);
}

size_t ArrayBufferViewSource::Acknowledge(uint64_t offset, size_t datalen) {
  return buffer_.Acknowledge(datalen);
}

size_t ArrayBufferViewSource::Seek(size_t amount) {
  size_t ret = buffer_.Seek(amount);
  if (buffer_.is_finished()) set_finished();
  return ret;
}

void ArrayBufferViewSource::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("buffer", buffer_);
}

BaseObjectPtr<BaseObject> ArrayBufferViewSource::GetStrongPtr() {
  return BaseObjectPtr<BaseObject>(this);
}

// ======================================================================================
// StreamSource

Local<FunctionTemplate> StreamSource::GetConstructorTemplate(Environment* env) {
  auto& state = BindingState::Get(env);
  Local<FunctionTemplate> tmpl = state.streamsource_constructor_template();
  if (tmpl.IsEmpty()) {
    auto isolate = env->isolate();
    tmpl = NewFunctionTemplate(isolate, New);
    tmpl->Inherit(AsyncWrap::GetConstructorTemplate(env));
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        AsyncWrap::kInternalFieldCount);
    SetProtoMethod(isolate, tmpl, "end", End);
    SetProtoMethod(isolate, tmpl, "write", Write);
    SetProtoMethod(isolate, tmpl, "writev", WriteV);
    tmpl->InstanceTemplate()->Set(env->owner_symbol(), Null(env->isolate()));
    tmpl->SetClassName(state.streamsource_string());
    state.set_streamsource_constructor_template(tmpl);
  }
  return tmpl;
}

void StreamSource::Initialize(Environment* env, Local<Object> target) {
  SetConstructorFunction(env->context(),
                         target,
                         "StreamSource",
                         GetConstructorTemplate(env),
                         SetConstructorFunctionFlag::NONE);
}

void StreamSource::New(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
  Environment* env = Environment::GetCurrent(args);
  new StreamSource(env, args.This());
}

StreamSource::StreamSource(Environment* env, Local<Object> object)
    : AsyncWrap(env, object, AsyncWrap::PROVIDER_QUICSTREAMSOURCE),
      Buffer::SourceObject(env) {
  MakeWeak();

  const auto defineProperty = [&](auto name, auto value) {
    object
        ->DefineOwnProperty(
            env->context(), name, value, PropertyAttribute::ReadOnly)
        .Check();
  };

  defineProperty(env->state_string(), state_.GetArrayBuffer());
}

int StreamSource::DoPull(bob::Next<ngtcp2_vec> next,
                         int options,
                         ngtcp2_vec* data,
                         size_t count,
                         size_t max_count_hint) {
  return queue_.Pull(std::move(next), options, data, count, max_count_hint);
}

void StreamSource::set_closed() {
  queue_.End();
  SourceObject::set_closed();
}

void StreamSource::End(const FunctionCallbackInfo<Value>& args) {
  StreamSource* source;
  ASSIGN_OR_RETURN_UNWRAP(&source, args.Holder());

  CHECK(!source->is_finished());
  CHECK(!source->is_closed());

  // There's no more data to be sent. Close the source and let the stream know.
  DEBUG(source, "Received end() from JavaScript.");
  source->set_closed();
  source->Resume();
}

void StreamSource::Write(const FunctionCallbackInfo<Value>& args) {
  StreamSource* source;
  ASSIGN_OR_RETURN_UNWRAP(&source, args.Holder());

  // The JavaScript side is responsible for making sure not to write
  // when closed or finished are set.
  CHECK(!source->is_closed());
  CHECK(!source->is_finished());

  CHECK(args[0]->IsArrayBufferView());

  auto view = args[0].As<ArrayBufferView>();
  if (view->ByteLength() == 0) return;

  DEBUG_ARGS(
      source, "Writing %" PRIu64 " bytes from JavaScript.", view->ByteLength());

  source->queue_.Push(Store(view));
  source->Resume();
}

void StreamSource::WriteV(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  StreamSource* source;
  ASSIGN_OR_RETURN_UNWRAP(&source, args.Holder());

  // The JavaScript side is responsible for making sure not to write
  // when closed or finished are set.

  CHECK(args[0]->IsArray());
  Local<Array> data = args[0].As<Array>();
  for (size_t n = 0; n < data->Length(); n++) {
    Local<Value> item;
    if (!data->Get(env->context(), n).ToLocal(&item)) return;

    CHECK(item->IsArrayBufferView());

    auto view = item.As<ArrayBufferView>();
    if (view->ByteLength() == 0) continue;

    DEBUG_ARGS(source,
               "Writing %" PRIu64 " bytes from JavaScript.",
               view->ByteLength());

    source->queue_.Push(Store(item.As<ArrayBufferView>()));
  }

  source->Resume();
}

size_t StreamSource::Acknowledge(uint64_t offset, size_t datalen) {
  return queue_.Acknowledge(datalen);
}

size_t StreamSource::Seek(size_t amount) {
  size_t ret = queue_.Seek(amount);
  if (queue_.is_finished()) set_finished();
  return ret;
}

void StreamSource::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("queue", queue_);
}

BaseObjectPtr<BaseObject> StreamSource::GetStrongPtr() {
  return BaseObjectPtr<BaseObject>(this);
}

// ======================================================================================
// StreamBaseSource

Local<FunctionTemplate> StreamBaseSource::GetConstructorTemplate(
    Environment* env) {
  auto& state = BindingState::Get(env);
  Local<FunctionTemplate> tmpl = state.streambasesource_constructor_template();
  if (tmpl.IsEmpty()) {
    tmpl = NewFunctionTemplate(env->isolate(), New);
    tmpl->Inherit(AsyncWrap::GetConstructorTemplate(env));
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        AsyncWrap::kInternalFieldCount);
    tmpl->SetClassName(state.streambasesource_string());
    state.set_streambasesource_constructor_template(tmpl);
  }
  return tmpl;
}

void StreamBaseSource::Initialize(Environment* env, Local<Object> target) {
  SetConstructorFunction(env->context(),
                         target,
                         "StreamBaseSource",
                         GetConstructorTemplate(env),
                         SetConstructorFunctionFlag::NONE);
}

void StreamBaseSource::New(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
  CHECK(args[0]->IsObject());

  Environment* env = Environment::GetCurrent(args);

  StreamBase* wrap = StreamBase::FromObject(args[0].As<Object>());
  CHECK_NOT_NULL(wrap);
  StreamBaseSource* source = new StreamBaseSource(
      env, args.This(), wrap, BaseObjectPtr<AsyncWrap>(wrap->GetAsyncWrap()));
  wrap->PushStreamListener(source);
  wrap->ReadStart();
}

StreamBaseSource::StreamBaseSource(Environment* env,
                                   Local<Object> object,
                                   StreamBase* resource,
                                   BaseObjectPtr<AsyncWrap> strong_ptr)
    : AsyncWrap(env, object, AsyncWrap::PROVIDER_QUICSTREAMBASESOURCE),
      Buffer::SourceObject(env),
      resource_(resource),
      strong_ptr_(std::move(strong_ptr)) {
  MakeWeak();

  const auto defineProperty = [&](auto name, auto value) {
    object
        ->DefineOwnProperty(
            env->context(), name, value, PropertyAttribute::ReadOnly)
        .Check();
  };

  defineProperty(env->state_string(), state_.GetArrayBuffer());

  CHECK_NOT_NULL(resource);
}

void StreamBaseSource::set_closed() {
  buffer_.End();
  resource_->ReadStop();
  resource_->RemoveStreamListener(this);
  SourceObject::set_closed();
}

uv_buf_t StreamBaseSource::OnStreamAlloc(size_t suggested_size) {
  uv_buf_t buf;
  buf.base = Malloc<char>(suggested_size);
  buf.len = suggested_size;
  return buf;
}

void StreamBaseSource::OnStreamRead(ssize_t nread, const uv_buf_t& buf_) {
  if (nread == UV_EOF && buffer_.is_ended()) {
    CHECK_NULL(buf_.base);
    return;
  }
  CHECK(!buffer_.is_ended());

  if (nread < 0) {
    DEBUG_ARGS(
        this, "Done reading from StreamBase or Error [%" PRIi64 "]", nread);
    // TODO(@jasnell): There's either been an error or we've reached the end.
    // handle appropriately. For now, we're not reporting the error, just
    // closing the source and moving on.
    set_closed();
    Resume();
  } else if (nread > 0) {
    DEBUG_ARGS(this, "Read %" PRIi64 " bytes from StreamBase", nread);
    CHECK_NOT_NULL(buf_.base);
    size_t read = nread;
    buffer_.Push(Store(ArrayBuffer::NewBackingStore(
                           static_cast<void*>(buf_.base),
                           read,
                           [](void* ptr, size_t len, void* deleter_data) {
                             std::unique_ptr<char> delete_me(
                                 static_cast<char*>(ptr));
                           },
                           nullptr),
                       read));
    Resume();
  } else if (nread == 0 && buf_.base != nullptr) {
    DEBUG(this, "Received zero byte read from StreamBase?");
    // An empty read is odd but not an error. Just drop it on the floor and
    // continue.
    std::unique_ptr<char> delete_me(buf_.base);
  }
}

int StreamBaseSource::DoPull(bob::Next<ngtcp2_vec> next,
                             int options,
                             ngtcp2_vec* data,
                             size_t count,
                             size_t max_count_hint) {
  return buffer_.Pull(std::move(next), options, data, count, max_count_hint);
}

size_t StreamBaseSource::Acknowledge(uint64_t offset, size_t datalen) {
  return buffer_.Acknowledge(datalen);
}

size_t StreamBaseSource::Seek(size_t amount) {
  size_t ret = buffer_.Seek(amount);
  if (buffer_.is_finished()) set_finished();
  return ret;
}

void StreamBaseSource::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("queue", buffer_);
}

BaseObjectPtr<BaseObject> StreamBaseSource::GetStrongPtr() {
  return BaseObjectPtr<BaseObject>(this);
}

// ======================================================================================
// BlobSource

Local<FunctionTemplate> BlobSource::GetConstructorTemplate(Environment* env) {
  auto& state = BindingState::Get(env);
  Local<FunctionTemplate> tmpl = state.blobsource_constructor_template();
  if (tmpl.IsEmpty()) {
    auto isolate = env->isolate();
    tmpl = NewFunctionTemplate(isolate, New);
    tmpl->Inherit(AsyncWrap::GetConstructorTemplate(env));
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        AsyncWrap::kInternalFieldCount);
    tmpl->SetClassName(state.blobsource_string());
    state.set_blobsource_constructor_template(tmpl);
  }
  return tmpl;
}

void BlobSource::Initialize(Environment* env, Local<Object> target) {
  SetConstructorFunction(env->context(),
                         target,
                         "BlobSource",
                         GetConstructorTemplate(env),
                         SetConstructorFunctionFlag::NONE);
}

void BlobSource::New(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
  Environment* env = Environment::GetCurrent(args);
  CHECK(Blob::HasInstance(env, args[0]));
  Blob* blob;
  ASSIGN_OR_RETURN_UNWRAP(&blob, args[0]);
  new BlobSource(env, args.This(), BaseObjectPtr<Blob>(blob));
}

BlobSource::BlobSource(Environment* env,
                       Local<Object> object,
                       BaseObjectPtr<Blob> blob)
    : AsyncWrap(env, object, AsyncWrap::PROVIDER_QUICBLOBSOURCE),
      Buffer::SourceObject(env),
      buffer_(*blob) {
  MakeWeak();
  set_closed();

  const auto defineProperty = [&](auto name, auto value) {
    object
        ->DefineOwnProperty(
            env->context(), name, value, PropertyAttribute::ReadOnly)
        .Check();
  };

  defineProperty(env->state_string(), state_.GetArrayBuffer());
}

int BlobSource::DoPull(bob::Next<ngtcp2_vec> next,
                       int options,
                       ngtcp2_vec* data,
                       size_t count,
                       size_t max_count_hint) {
  return buffer_.Pull(std::move(next), options, data, count, max_count_hint);
}

size_t BlobSource::Acknowledge(uint64_t offset, size_t datalen) {
  return buffer_.Acknowledge(datalen);
}

size_t BlobSource::Seek(size_t amount) {
  size_t ret = buffer_.Seek(amount);
  if (buffer_.is_finished()) set_finished();
  return ret;
}

void BlobSource::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("buffer", buffer_);
}

BaseObjectPtr<BaseObject> BlobSource::GetStrongPtr() {
  return BaseObjectPtr<BaseObject>(this);
}

// ======================================================================================
// Buffer::Chunk

Buffer::Chunk::Chunk(Store&& store)
    : data_(std::move(store)), unacknowledged_(data_.length()) {}

Buffer::Chunk Buffer::Chunk::Create(Environment* env,
                                    const uint8_t* data,
                                    size_t len) {
  auto store = v8::ArrayBuffer::NewBackingStore(env->isolate(), len);
  memcpy(store->Data(), data, len);
  return Buffer::Chunk(Store(std::move(store), len));
}

Buffer::Chunk Buffer::Chunk::Create(Store&& data) {
  return Buffer::Chunk(std::move(data));
}

MaybeLocal<Value> Buffer::Chunk::Release(Environment* env) {
  CHECK(data_);
  auto ret = data_.ToArrayBufferView<Uint8Array>(env);
  USE(std::move(data_));
  read_ = 0;
  unacknowledged_ = 0;
  return ret;
}

size_t Buffer::Chunk::Seek(size_t amount) {
  CHECK(data_);
  amount = std::min(amount, remaining());
  read_ += amount;
  CHECK_LE(read_, data_.length());
  return amount;
}

size_t Buffer::Chunk::Acknowledge(size_t amount) {
  amount = std::min(amount, unacknowledged_);
  unacknowledged_ -= amount;
  return amount;
}

Buffer::Chunk::operator ngtcp2_vec() const {
  CHECK(data_);
  ngtcp2_vec vec = data_;
  CHECK_LE(remaining(), vec.len);
  ngtcp2_vec ret;
  ret.base = vec.base;
  ret.len = remaining();
  return ret;
}

Buffer::Chunk::operator uv_buf_t() const {
  CHECK(data_);
  uv_buf_t buf = data_;
  CHECK_LE(remaining(), buf.len);
  uv_buf_t ret;
  ret.base = buf.base;
  ret.len = remaining();
  return ret;
}

void Buffer::Chunk::MemoryInfo(MemoryTracker* tracker) const {
  if (data_) tracker->TrackFieldWithSize("data", data_ ? data_.length() : 0);
}

const uint8_t* Buffer::Chunk::data() const {
  CHECK(data_);
  ngtcp2_vec vec = data_;
  return vec.base + read_;
}

// ======================================================================================
// Buffer

Buffer::Buffer(const Blob& blob) {
  for (const auto& entry : blob.entries())
    Push(Store(entry.store, entry.length, entry.offset));
  End();
}

Buffer::Buffer(Store&& store) {
  Push(std::move(store));
  End();
}

void Buffer::Push(Environment* env, const uint8_t* data, size_t len) {
  CHECK(!ended_);
  queue_.emplace_back(Buffer::Chunk::Create(env, data, len));
  length_ += len;
  remaining_ += len;
}

void Buffer::Push(Store&& store) {
  CHECK(!ended_);
  length_ += store.length();
  remaining_ += store.length();
  queue_.push_back(Chunk::Create(std::move(store)));
}

size_t Buffer::Seek(size_t amount) {
  if (queue_.empty()) {
    CHECK_EQ(remaining_, 0);  // Remaining should be zero
    if (ended_) finished_ = true;
    return 0;
  }
  amount = std::min(amount, remaining_);
  size_t len = 0;
  while (amount > 0) {
    size_t chunk_remaining_ = queue_[head_].remaining();
    size_t actual = queue_[head_].Seek(amount);
    CHECK_LE(actual, amount);
    amount -= actual;
    remaining_ -= actual;
    len += actual;
    if (actual >= chunk_remaining_) {
      head_++;
      // head_ should never extend beyond queue size!
      CHECK_LE(head_, queue_.size());
    }
  }
  if (remaining_ == 0 && ended_) finished_ = true;
  return len;
}

size_t Buffer::Acknowledge(size_t amount) {
  if (queue_.empty()) return 0;
  amount = std::min(amount, length_);
  size_t len = 0;
  while (amount > 0) {
    CHECK_GT(queue_.size(), 0);
    size_t actual = queue_.front().Acknowledge(amount);

    CHECK_LE(actual, amount);
    amount -= actual;
    length_ -= actual;
    len += actual;
    // If we've acknowledged all of the bytes in the current chunk, pop it to
    // free the memory and decrement the head_ pointer if necessary.
    if (queue_.front().length() == 0) {
      queue_.pop_front();
      if (head_ > 0) head_--;
    }
  }
  return len;
}

void Buffer::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("queue", queue_);
}

int Buffer::DoPull(bob::Next<ngtcp2_vec> next,
                   int options,
                   ngtcp2_vec* data,
                   size_t count,
                   size_t max_count_hint) {
  size_t len = 0;
  size_t numbytes = 0;
  int status = bob::Status::STATUS_CONTINUE;

  // There's no data to read.
  if (queue_.empty() || !remaining_) {
    status = ended_ ? bob::Status::STATUS_END : bob::Status::STATUS_BLOCK;
    std::move(next)(status, nullptr, 0, [](size_t len) {});
    return status;
  }

  // Ensure that there's storage space.
  MaybeStackBuffer<ngtcp2_vec, kMaxVectorCount> vec;
  size_t queue_size = queue_.size() - head_;

  max_count_hint =
      (max_count_hint == 0) ? queue_size : std::min(max_count_hint, queue_size);

  CHECK_IMPLIES(data == nullptr, count == 0);
  if (data == nullptr) {
    vec.AllocateSufficientStorage(max_count_hint);
    data = vec.out();
    count = max_count_hint;
  }

  // Build the list of buffers.
  for (size_t n = head_; n < queue_.size() && len < count; n++, len++) {
    data[len] = queue_[n];
    numbytes += data[len].len;
  }

  // If the buffer is ended, and the number of bytes matches the total
  // remaining, and OPTIONS_END is used, set the status to STATUS_END.
  if (is_ended() && numbytes == remaining() && options & bob::OPTIONS_END) {
    status = bob::Status::STATUS_END;
  }

  // Pass the data back out to the caller.
  std::move(next)(status, data, len, [this](size_t len) {
    size_t actual = Seek(len);
    CHECK_LE(actual, len);
  });

  return status;
}

Maybe<size_t> Buffer::Release(Stream* stream) {
  if (queue_.empty()) return Just(static_cast<size_t>(0));
  head_ = 0;
  length_ = 0;
  remaining_ = 0;
  if (ended_) finished_ = true;
  return stream->EmitData(std::move(queue_), ended_);
}

}  // namespace quic
}  // namespace node
