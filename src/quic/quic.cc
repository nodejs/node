
#include "node.h"
#include "quic/defs.h"
#if NODE_OPENSSL_HAS_QUIC
#include <base_object-inl.h>
#include <crypto/crypto_random.h>
#include <crypto/crypto_util.h>
#include <env-inl.h>
#include <memory_tracker-inl.h>
#include <ngtcp2/ngtcp2.h>
#include <ngtcp2/ngtcp2_crypto.h>
#include <node_bob-inl.h>
#include <node_buffer.h>
#include <node_errors.h>
#include <node_external_reference.h>
#include <node_mem-inl.h>
#include <node_sockaddr-inl.h>
#include <req_wrap-inl.h>
#include <util.h>
#include <v8-value-serializer.h>
#include <v8.h>
#include <cstdint>
#include "crypto.h"
#include "endpoint.h"
#include "quic.h"
#include "session.h"
#include "stream.h"
#endif  // NODE_OPENSSL_HAS_QUIC

namespace node {

using crypto::CSPRNG;
using v8::ArrayBuffer;
using v8::ArrayBufferView;
using v8::BackingStore;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;

namespace quic {

// The internalBinding('quic') will be available even if quic support is not
// enabled. This prevents the internalBinding call from throwing. However, if
// quic is not enabled, the binding will have no exports.

#if NODE_OPENSSL_HAS_QUIC

// ======================================================================================
// BindingState

constexpr FastStringKey BindingState::type_name;

BindingState& BindingState::Get(Environment* env) {
  return *env->GetBindingData<BindingState>(env->context());
}

bool BindingState::Initialize(Environment* env, Local<Object> target) {
  BindingState* const state =
      env->AddBindingData<BindingState>(env->context(), target);
  return state != nullptr;
}

BindingState::BindingState(Environment* env, v8::Local<v8::Object> object)
    : BaseObject(env, object) {
  MakeWeak();
}

BindingState::operator ngtcp2_mem() const {
  return BindingState::Get(env()).MakeAllocator();
}

BindingState::operator nghttp3_mem() const {
  ngtcp2_mem allocator = *this;
  nghttp3_mem http3_allocator = {
      allocator.user_data,
      allocator.malloc,
      allocator.free,
      allocator.calloc,
      allocator.realloc,
  };
  return http3_allocator;
}

void BindingState::MemoryInfo(MemoryTracker* tracker) const {
#define V(name, _) tracker->TrackField(#name, name##_callback());
  QUIC_JS_CALLBACKS(V)
#undef V
#define V(name, _) tracker->TrackField(#name, name##_string());
  QUIC_STRINGS(V)
#undef V
}

void BindingState::CheckAllocatedSize(size_t previous_size) const {
  CHECK_GE(current_ngtcp2_memory_, previous_size);
}

void BindingState::IncreaseAllocatedSize(size_t size) {
  current_ngtcp2_memory_ += size;
}

void BindingState::DecreaseAllocatedSize(size_t size) {
  current_ngtcp2_memory_ -= size;
}

#define V(name)                                                                \
  void BindingState::set_##name##_constructor_template(                        \
      Local<FunctionTemplate> tmpl) {                                          \
    name##_constructor_template_.Reset(env()->isolate(), tmpl);                \
  }                                                                            \
  Local<FunctionTemplate> BindingState::name##_constructor_template() const {  \
    return PersistentToLocal::Default(env()->isolate(),                        \
                                      name##_constructor_template_);           \
  }
QUIC_CONSTRUCTORS(V)
#undef V

#define V(name, _)                                                             \
  void BindingState::set_##name##_callback(Local<Function> fn) {               \
    name##_callback_.Reset(env()->isolate(), fn);                              \
  }                                                                            \
  Local<Function> BindingState::name##_callback() const {                      \
    return PersistentToLocal::Default(env()->isolate(), name##_callback_);     \
  }
QUIC_JS_CALLBACKS(V)
#undef V

#define V(name, value)                                                         \
  Local<String> BindingState::name##_string() const {                          \
    if (name##_string_.IsEmpty())                                              \
      name##_string_.Set(env()->isolate(),                                     \
                         OneByteString(env()->isolate(), value));              \
    return name##_string_.Get(env()->isolate());                               \
  }
QUIC_STRINGS(V)
#undef V

// =============================================================================
// CID

CID::CID(const uint8_t* cid, size_t length) : CID() {
  CHECK_EQ(ptr_, &cid_);
  CHECK_LE(length, NGTCP2_MAX_CIDLEN);
  ngtcp2_cid_init(&cid_, cid, length);
}

CID::CID(const CID& other) : CID() {
  CHECK_EQ(ptr_, &cid_);
  ngtcp2_cid_init(&cid_, other.ptr_->data, other.ptr_->datalen);
}

CID& CID::operator=(const CID& other) {
  CHECK_EQ(ptr_, &cid_);
  if (this == &other) return *this;
  ngtcp2_cid_init(&cid_, other.ptr_->data, other.ptr_->datalen);
  return *this;
}

std::string CID::ToString() const {
  std::vector<char> dest(ptr_->datalen * 2 + 1);
  dest[dest.size() - 1] = '\0';
  size_t written =
      StringBytes::hex_encode(reinterpret_cast<const char*>(ptr_->data),
                              ptr_->datalen,
                              dest.data(),
                              dest.size());
  return std::string(dest.data(), written);
}

void CIDFactory::Generate(CID* cid, size_t length) {
  Generate(&cid->cid_, length);
}

CID CIDFactory::Generate(size_t length_hint) {
  CID cid;
  Generate(&cid, length_hint);
  return cid;
}

void CIDFactory::RandomCIDFactory::Generate(ngtcp2_cid* cid,
                                            size_t length_hint) {
  CHECK_LE(length_hint, NGTCP2_MAX_CIDLEN);
  if (length_hint == 0) return;
  CHECK(CSPRNG(cid->data, length_hint).is_ok());
  cid->datalen = length_hint;
}

Local<FunctionTemplate> CIDFactory::Base::GetConstructorTemplate(
    Environment* env) {
  auto& state = BindingState::Get(env);
  Local<FunctionTemplate> tmpl = state.cidfactorybase_constructor_template();
  if (tmpl.IsEmpty()) {
    tmpl = FunctionTemplate::New(env->isolate());
    tmpl->Inherit(BaseObject::GetConstructorTemplate(env));
    tmpl->InstanceTemplate()->SetInternalFieldCount(Base::kInternalFieldCount);
    tmpl->SetClassName(state.cidfactorybase_string());
    state.set_cidfactorybase_constructor_template(tmpl);
  }
  return tmpl;
}

CIDFactory::Base::Base(Environment* env, v8::Local<v8::Object> object)
    : BaseObject(env, object) {
  MakeWeak();
}

BaseObjectPtr<BaseObject> CIDFactory::Base::StrongRef() {
  return BaseObjectPtr<BaseObject>(this);
}

// =============================================================================
// Packet

Local<FunctionTemplate> Packet::GetConstructorTemplate(Environment* env) {
  auto& state = BindingState::Get(env);
  Local<FunctionTemplate> tmpl = state.send_wrap_constructor_template();
  if (tmpl.IsEmpty()) {
    tmpl = NewFunctionTemplate(env->isolate(), IllegalConstructor);
    tmpl->Inherit(PacketReq::GetConstructorTemplate(env));
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        Packet::kInternalFieldCount);
    tmpl->SetClassName(state.packetwrap_string());
    state.set_send_wrap_constructor_template(tmpl);
  }
  return tmpl;
}

BaseObjectPtr<Packet> Packet::Create(Environment* env,
                                     Endpoint* endpoint,
                                     const SocketAddress& destination,
                                     const char* diagnostic_label,
                                     size_t length) {
  Local<Object> obj;
  if (UNLIKELY(!GetConstructorTemplate(env)
                    ->InstanceTemplate()
                    ->NewInstance(env->context())
                    .ToLocal(&obj))) {
    return BaseObjectPtr<Packet>();
  }

  return MakeBaseObject<Packet>(
      env, endpoint, obj, destination, length, diagnostic_label);
}

BaseObjectPtr<Packet> Packet::Clone() const {
  Local<Object> obj;
  if (UNLIKELY(!GetConstructorTemplate(env())
                    ->InstanceTemplate()
                    ->NewInstance(env()->context())
                    .ToLocal(&obj))) {
    return BaseObjectPtr<Packet>();
  }

  return MakeBaseObject<Packet>(env(), endpoint_, obj, destination_, data_);
}

Packet::Packet(Environment* env,
               Endpoint* endpoint,
               v8::Local<v8::Object> object,
               const SocketAddress& destination,
               std::shared_ptr<Data> data)
    : PacketReq(env, object, AsyncWrap::PROVIDER_QUICPACKET),
      endpoint_(endpoint),
      destination_(destination),
      data_(std::move(data)) {}

Packet::Packet(Environment* env,
               Endpoint* endpoint,
               v8::Local<v8::Object> object,
               const SocketAddress& destination,
               size_t length,
               const char* diagnostic_label)
    : Packet(env,
             endpoint,
             object,
             destination,
             std::make_shared<Data>(length, diagnostic_label)) {}

void Packet::Done(int status) {
  CHECK_NOT_NULL(endpoint_);
  endpoint_->OnSendDone(status);
  handle_.reset();
}

Packet::operator uv_buf_t() const {
  CHECK(data_);
  uv_buf_t buf;
  buf.base = reinterpret_cast<char*>(data_->ptr_);
  buf.len = data_->len_;
  return buf;
}

Packet::operator ngtcp2_vec() const {
  CHECK(data_);
  ngtcp2_vec vec;
  vec.base = data_->ptr_;
  vec.len = data_->len_;
  return vec;
}

void Packet::Truncate(size_t len) {
  CHECK(data_);
  CHECK_LE(len, data_->len_);
  data_->len_ = len;
}

std::string Packet::ToString() const {
  if (!data_) return "Packet (<empty>)";
  return std::string("Packet (") + data_->diagnostic_label_ + ", " +
         std::to_string(data_->len_) + ")";
}

Packet::Data::Data(size_t length, const char* diagnostic_label)
    : ptr_(length <= kDefaultMaxPacketLength ? data_ : Malloc<uint8_t>(length)),
      len_(length),
      diagnostic_label_(diagnostic_label) {}

Packet::Data::~Data() {
  if (ptr_ != nullptr && ptr_ != data_) std::unique_ptr<uint8_t> free_me(ptr_);
}

void Packet::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("destination", destination_);
  tracker->TrackField("data", data_);
  tracker->TrackField("handle", handle_);
}

void Packet::Data::MemoryInfo(MemoryTracker* tracker) const {
  if (ptr_ != data_) tracker->TrackFieldWithSize("buffer", len_);
}

// =============================================================================
// QuicError

namespace {
std::string TypeName(QuicError::Type type) {
  switch (type) {
    case QuicError::Type::APPLICATION:
      return "APPLICATION";
    case QuicError::Type::TRANSPORT:
      return "TRANSPORT";
    case QuicError::Type::VERSION_NEGOTIATION:
      return "VERSION_NEGOTIATION";
    case QuicError::Type::IDLE_CLOSE:
      return "IDLE_CLOSE";
  }
  UNREACHABLE();
}
}  // namespace

QuicError QuicError::ForTransport(error_code code, const std::string& reason) {
  QuicError error(reason);
  ngtcp2_connection_close_error_set_transport_error(
      &error.error_, code, error.reason_c_str(), reason.length());
  return error;
}

QuicError QuicError::ForApplication(error_code code,
                                    const std::string& reason) {
  QuicError error(reason);
  ngtcp2_connection_close_error_set_application_error(
      &error.error_, code, error.reason_c_str(), reason.length());
  return error;
}

QuicError QuicError::ForVersionNegotiation(const std::string& reason) {
  QuicError error(reason);
  ngtcp2_connection_close_error_set_transport_error_liberr(
      &error.error_,
      NGTCP2_ERR_RECV_VERSION_NEGOTIATION,
      error.reason_c_str(),
      reason.length());
  return error;
}

QuicError QuicError::ForIdleClose(const std::string& reason) {
  QuicError error(reason);
  ngtcp2_connection_close_error_set_transport_error_liberr(
      &error.error_,
      NGTCP2_ERR_IDLE_CLOSE,
      error.reason_c_str(),
      reason.length());
  return error;
}

QuicError QuicError::ForNgtcp2Error(int code, const std::string& reason) {
  QuicError error(reason);
  ngtcp2_connection_close_error_set_transport_error_liberr(
      &error.error_, code, error.reason_c_str(), reason.length());
  return error;
}

QuicError QuicError::ForTlsAlert(int code, const std::string& reason) {
  QuicError error(reason);
  ngtcp2_connection_close_error_set_transport_error_tls_alert(
      &error.error_, code, error.reason_c_str(), reason.length());
  return error;
}

QuicError QuicError::From(Session* session) {
  QuicError error("");
  ngtcp2_conn_get_connection_close_error(session->connection(), &error.error_);
  return error;
}

std::string QuicError::ToString() const {
  auto str = std::string("QuicError(") + TypeName(type()) + ") " +
             std::to_string(code());
  if (reason_.length() > 0) str += ": " + reason_;
  return str;
}

void QuicError::MemoryInfo(MemoryTracker* tracker) const {
  if (ptr_ == &error_) tracker->TrackField("reason", reason_);
}

void IllegalConstructor(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
  Environment* env = Environment::GetCurrent(args);
  THROW_ERR_ILLEGAL_CONSTRUCTOR(env);
}

// ======================================================================================
// Store

void Store::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("store", store_);
}

Store::Store(std::shared_ptr<v8::BackingStore> store,
             size_t length,
             size_t offset)
    : store_(std::move(store)), offset_(offset), length_(length) {}

Store::Store(std::unique_ptr<v8::BackingStore> store,
             size_t length,
             size_t offset)
    : store_(std::move(store)), offset_(offset), length_(length) {}

Store::Store(v8::Local<v8::ArrayBuffer> buffer)
    : Store(buffer->GetBackingStore(), buffer->ByteLength()) {}

Store::Store(v8::Local<v8::ArrayBufferView> view)
    : Store(view->Buffer()->GetBackingStore(),
            view->ByteLength(),
            view->ByteOffset()) {}

Store::operator uv_buf_t() const {
  uv_buf_t buf;
  buf.base = store_ != nullptr ? static_cast<char*>(store_->Data()) + offset_
                               : nullptr,
  buf.len = length_;
  return buf;
}

Store::operator ngtcp2_vec() const {
  ngtcp2_vec vec;
  vec.base = store_ != nullptr ? static_cast<uint8_t*>(store_->Data()) + offset_
                               : nullptr;
  vec.len = length_;
  return vec;
}

// ======================================================================================
namespace {
void SetCallbacks(const FunctionCallbackInfo<Value>& args) {
  auto env = Environment::GetCurrent(args);
  auto isolate = env->isolate();
  BindingState& state = BindingState::Get(env);
  CHECK(!state.initialized);
  if (!args[0]->IsObject())
    return THROW_ERR_INVALID_ARG_TYPE(env, "Missing Callbacks");

  Local<Object> obj = args[0].As<Object>();

#define V(name, key)                                                           \
  do {                                                                         \
    Local<Value> val;                                                          \
    if (!obj->Get(env->context(), FIXED_ONE_BYTE_STRING(isolate, "on" #key))   \
             .ToLocal(&val) ||                                                 \
        !val->IsFunction()) {                                                  \
      return THROW_ERR_MISSING_ARGS(isolate, "Missing Callback: on" #key);     \
    }                                                                          \
    state.set_##name##_callback(val.As<Function>());                           \
  } while (0);
  QUIC_JS_CALLBACKS(V)

#undef V
  state.initialized = true;
}
}  // namespace

#endif  // NODE_OPENSSL_HAS_QUIC

void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
#if NODE_OPENSSL_HAS_QUIC
  auto env = Environment::GetCurrent(context);

  if (UNLIKELY(!BindingState::Initialize(env, target))) return;

  Endpoint::Initialize(env, target);
  Session::Initialize(env, target);
  Stream::Initialize(env, target);
  SessionTicket::Initialize(env, target);

  SetMethod(context, target, "setCallbacks", SetCallbacks);

  constexpr uint32_t QUIC_PREFERRED_ADDRESS_USE =
      static_cast<uint32_t>(PreferredAddress::Policy::USE);
  constexpr uint32_t QUIC_PREFERRED_ADDRESS_IGNORE =
      static_cast<uint32_t>(PreferredAddress::Policy::IGNORE_PREFERED);

#define V(Name, _, __) NODE_DEFINE_CONSTANT(target, IDX_STATS_ENDPOINT_##Name);
  ENDPOINT_STATS(V)
#undef V
#define V(Name, _, __) NODE_DEFINE_CONSTANT(target, IDX_STATE_ENDPOINT_##Name);
  ENDPOINT_STATE(V)
#undef V
#define V(Name, _, __) NODE_DEFINE_CONSTANT(target, IDX_STATS_SESSION_##Name);
  SESSION_STATS(V)
#undef V
#define V(Name, _, __) NODE_DEFINE_CONSTANT(target, IDX_STATE_SESSION_##Name);
  SESSION_STATE(V)
#undef V
#define V(Name, _, __) NODE_DEFINE_CONSTANT(target, IDX_STATS_STREAM_##Name);
  STREAM_STATS(V)
#undef V
#define V(Name, _, __) NODE_DEFINE_CONSTANT(target, IDX_STATE_STREAM_##Name);
  STREAM_STATE(V)
#undef V

  NODE_DEFINE_CONSTANT(target, QUIC_CC_ALGO_CUBIC);
  NODE_DEFINE_CONSTANT(target, QUIC_CC_ALGO_RENO);
  NODE_DEFINE_CONSTANT(target, QUIC_CC_ALGO_BBR);
  NODE_DEFINE_CONSTANT(target, QUIC_CC_ALGO_BBR2);
  NODE_DEFINE_CONSTANT(target, QUIC_PREFERRED_ADDRESS_IGNORE);
  NODE_DEFINE_CONSTANT(target, QUIC_PREFERRED_ADDRESS_USE);
  NODE_DEFINE_CONSTANT(target, QUIC_MAX_CIDLEN);
  NODE_DEFINE_CONSTANT(target, QUIC_HEADERS_KIND_INFO);
  NODE_DEFINE_CONSTANT(target, QUIC_HEADERS_KIND_INITIAL);
  NODE_DEFINE_CONSTANT(target, QUIC_HEADERS_KIND_TRAILING);
  NODE_DEFINE_CONSTANT(target, QUIC_HEADERS_FLAGS_NONE);
  NODE_DEFINE_CONSTANT(target, QUIC_HEADERS_FLAGS_TERMINAL);

  NODE_DEFINE_CONSTANT(target, QUIC_STREAM_PRIORITY_DEFAULT);
  NODE_DEFINE_CONSTANT(target, QUIC_STREAM_PRIORITY_LOW);
  NODE_DEFINE_CONSTANT(target, QUIC_STREAM_PRIORITY_HIGH);
  NODE_DEFINE_CONSTANT(target, QUIC_STREAM_PRIORITY_FLAGS_NONE);
  NODE_DEFINE_CONSTANT(target, QUIC_STREAM_PRIORITY_FLAGS_NON_INCREMENTAL);

#define V(Name)                                                                \
  do {                                                                         \
    constexpr auto QUIC_ERR_##Name = NGTCP2_##Name;                            \
    NODE_DEFINE_CONSTANT(target, QUIC_ERR_##Name);                             \
  } while (false);
  QUIC_TRANSPORT_ERRORS(V)
#undef V

#define V(Name)                                                                \
  do {                                                                         \
    constexpr auto QUIC_ERR_##Name = NGHTTP3_##Name;                           \
    NODE_DEFINE_CONSTANT(target, QUIC_ERR_##Name);                             \
  } while (false);
  HTTP3_APPLICATION_ERRORS(V)
#undef V

  NODE_DEFINE_CONSTANT(target, QUIC_ERROR_TYPE_TRANSPORT);
  NODE_DEFINE_CONSTANT(target, QUIC_ERROR_TYPE_APPLICATION);
  NODE_DEFINE_CONSTANT(target, QUIC_ERROR_TYPE_VERSION_NEGOTIATION);
  NODE_DEFINE_CONSTANT(target, QUIC_ERROR_TYPE_IDLE_CLOSE);

  NODE_DEFINE_CONSTANT(target, QUIC_ENDPOINT_CLOSE_CONTEXT_CLOSE);
  NODE_DEFINE_CONSTANT(target, QUIC_ENDPOINT_CLOSE_CONTEXT_BIND_FAILURE);
  NODE_DEFINE_CONSTANT(target, QUIC_ENDPOINT_CLOSE_CONTEXT_START_FAILURE);
  NODE_DEFINE_CONSTANT(target, QUIC_ENDPOINT_CLOSE_CONTEXT_RECEIVE_FAILURE);
  NODE_DEFINE_CONSTANT(target, QUIC_ENDPOINT_CLOSE_CONTEXT_SEND_FAILURE);
  NODE_DEFINE_CONSTANT(target, QUIC_ENDPOINT_CLOSE_CONTEXT_LISTEN_FAILURE);

#define DEFAULT_UNACKNOWLEDGED_PACKET_THRESHOLD 0

  NODE_DEFINE_STRING_CONSTANT(target, "HTTP3_ALPN", &NGHTTP3_ALPN_H3[1]);
  NODE_DEFINE_STRING_CONSTANT(target, "QUIC_DEFAULT_CIPHERS", DEFAULT_CIPHERS);
  NODE_DEFINE_STRING_CONSTANT(target, "QUIC_DEFAULT_GROUPS", DEFAULT_GROUPS);

  NODE_DEFINE_CONSTANT(target, DEFAULT_RETRYTOKEN_EXPIRATION);
  NODE_DEFINE_CONSTANT(target, DEFAULT_TOKEN_EXPIRATION);
  NODE_DEFINE_CONSTANT(target, DEFAULT_MAX_CONNECTIONS_PER_HOST);
  NODE_DEFINE_CONSTANT(target, DEFAULT_MAX_CONNECTIONS);
  NODE_DEFINE_CONSTANT(target, DEFAULT_MAX_STATELESS_RESETS);
  NODE_DEFINE_CONSTANT(target, DEFAULT_MAX_SOCKETADDRESS_LRU_SIZE);
  NODE_DEFINE_CONSTANT(target, DEFAULT_MAX_RETRY_LIMIT);
  NODE_DEFINE_CONSTANT(target, DEFAULT_UNACKNOWLEDGED_PACKET_THRESHOLD);

  NODE_DEFINE_CONSTANT(target, HTTP3_ERR_NO_ERROR);
#endif  // NODE_OPENSSL_HAS_QUIC
}

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
#if NODE_OPENSSL_HAS_QUIC
  registry->Register(SetCallbacks);
  Endpoint::RegisterExternalReferences(registry);
  Session::RegisterExternalReferences(registry);
  Stream::RegisterExternalReferences(registry);
  SessionTicket::RegisterExternalReferences(registry);
#endif  // NODE_OPENSSL_HAS_QUIC
}

}  // namespace quic
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(quic, node::quic::Initialize)
NODE_MODULE_EXTERNAL_REFERENCE(quic, node::quic::RegisterExternalReferences)
