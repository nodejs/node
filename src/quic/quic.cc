
#if HAVE_OPENSSL
#include <openssl/crypto.h>
#endif  // HAVE_OPENSSL

#ifdef OPENSSL_INFO_QUIC
# include "quic/quic.h"
# include "quic/endpoint.h"
# include "quic/session.h"
# include "quic/stream.h"
# include "crypto/crypto_random.h"
# include "crypto/crypto_context.h"
#endif  // OPENSSL_INFO_QUIC

#include "async_wrap-inl.h"
#include "base_object-inl.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "node.h"
#include "node_errors.h"
#include "node_mem-inl.h"
#include "node_sockaddr-inl.h"
#include "util-inl.h"
#include "v8.h"
#include "uv.h"

namespace node {

using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;

namespace quic {
#ifdef OPENSSL_INFO_QUIC

constexpr FastStringKey BindingState::type_name;

void IllegalConstructor(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
  Environment* env = Environment::GetCurrent(args);
  THROW_ERR_ILLEGAL_CONSTRUCTOR(env);
}

BindingState* BindingState::Get(Environment* env) {
  return env->GetBindingData<BindingState>(env->context());
}

bool BindingState::Initialize(Environment* env, Local<Object> target) {
  BindingState* const state =
      env->AddBindingData<BindingState>(env->context(), target);
  return state != nullptr;
}

BindingState::BindingState(Environment* env, Local<Object> object)
    : BaseObject(env, object) {}

ngtcp2_mem BindingState::GetAllocator(Environment* env) {
  return BindingState::Get(env)->MakeAllocator();
}

nghttp3_mem BindingState::GetHttp3Allocator(Environment* env) {
  ngtcp2_mem allocator = GetAllocator(env);
  nghttp3_mem http3_allocator = {
    allocator.mem_user_data,
    allocator.malloc,
    allocator.free,
    allocator.calloc,
    allocator.realloc,
  };
  return http3_allocator;
}

void BindingState::MemoryInfo(MemoryTracker* tracker) const {
#define V(name, _) tracker->TrackField(#name, name ## _callback());
  QUIC_JS_CALLBACKS(V)
#undef V
#define V(name, _) tracker->TrackField(#name, name ## _string());
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
  void BindingState::set_ ## name ## _constructor_template(                    \
      Local<FunctionTemplate> tmpl) {                                          \
    name ## _constructor_template_.Reset(env()->isolate(), tmpl);              \
  }                                                                            \
  Local<FunctionTemplate> BindingState::name ## _constructor_template() const {\
    return PersistentToLocal::Default(                                         \
        env()->isolate(), name ## _constructor_template_);                     \
  }
  QUIC_CONSTRUCTORS(V)
#undef V

#define V(name, _)                                                             \
  void BindingState::set_ ## name ## _callback(Local<Function> fn) {           \
    name ## _callback_.Reset(env()->isolate(), fn);                            \
  }                                                                            \
  Local<Function> BindingState::name ## _callback() const {                    \
    return PersistentToLocal::Default(env()->isolate(), name ## _callback_);   \
  }
  QUIC_JS_CALLBACKS(V)
#undef V

#define V(name, value)                                                         \
  Local<String> BindingState::name ## _string() const {                        \
    if (name ## _string_.IsEmpty())                                            \
      name ## _string_.Set(                                                    \
          env()->isolate(),                                                    \
          OneByteString(env()->isolate(), value));                             \
    return name ## _string_.Get(env()->isolate());                             \
  }
  QUIC_STRINGS(V)
#undef V

PreferredAddress::Address PreferredAddress::ipv4() const {
  Address address;
  address.family = AF_INET;
  address.port = paddr_->ipv4_port;

  char host[NI_MAXHOST];
  // Return an empty string if unable to convert...
  if (uv_inet_ntop(AF_INET, paddr_->ipv4_addr, host, sizeof(host)) == 0)
    address.address = std::string(host);

  return address;
}

PreferredAddress::Address PreferredAddress::ipv6() const {
  Address address;
  address.family = AF_INET6;
  address.port = paddr_->ipv6_port;

  char host[NI_MAXHOST];
  // Return an empty string if unable to convert...
  if (uv_inet_ntop(AF_INET6, paddr_->ipv6_addr, host, sizeof(host)) == 0)
    address.address = std::string(host);

  return address;
}

bool PreferredAddress::Use(const Address& address) const {
  uv_getaddrinfo_t req;

  if (!Resolve(address, &req))
    return false;

  dest_->addrlen = req.addrinfo->ai_addrlen;
  memcpy(dest_->addr, req.addrinfo->ai_addr, req.addrinfo->ai_addrlen);
  uv_freeaddrinfo(req.addrinfo);
  return true;
}

void PreferredAddress::CopyToTransportParams(
    ngtcp2_transport_params* params,
    const sockaddr* addr) {
  CHECK_NOT_NULL(params);
  CHECK_NOT_NULL(addr);
  params->preferred_address_present = 1;
  switch (addr->sa_family) {
    case AF_INET: {
      const sockaddr_in* src = reinterpret_cast<const sockaddr_in*>(addr);
      memcpy(
          params->preferred_address.ipv4_addr,
          &src->sin_addr,
          sizeof(params->preferred_address.ipv4_addr));
      params->preferred_address.ipv4_port = SocketAddress::GetPort(addr);
      break;
    }
    case AF_INET6: {
      const sockaddr_in6* src = reinterpret_cast<const sockaddr_in6*>(addr);
      memcpy(
          params->preferred_address.ipv6_addr,
          &src->sin6_addr,
          sizeof(params->preferred_address.ipv6_addr));
      params->preferred_address.ipv6_port = SocketAddress::GetPort(addr);
      break;
    }
    default:
      UNREACHABLE();
  }
}

bool PreferredAddress::Resolve(
    const Address& address,
    uv_getaddrinfo_t* req) const {
  addrinfo hints{};
  hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;
  hints.ai_family = address.family;
  hints.ai_socktype = SOCK_DGRAM;

  // Unfortunately ngtcp2 requires the selection of the
  // preferred address to be synchronous, which means we
  // have to do a sync resolve using uv_getaddrinfo here.
  return
      uv_getaddrinfo(
          env_->event_loop(),
          req,
          nullptr,
          address.address.c_str(),
          std::to_string(address.port).c_str(),
          &hints) == 0 &&
      req->addrinfo != nullptr;
}

void Packet::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackFieldWithSize("allocated", ptr_ != data_ ? len_ : 0);
}

Path::Path(
    const std::shared_ptr<SocketAddress>& local,
    const std::shared_ptr<SocketAddress>& remote) {
  CHECK(local);
  CHECK(remote);
  ngtcp2_addr_init(
      &this->local,
      local->data(),
      local->length(),
      nullptr);
  ngtcp2_addr_init(
      &this->remote,
      remote->data(),
      remote->length(),
      nullptr);
}

StatelessResetToken::StatelessResetToken(
    uint8_t* token,
    const uint8_t* secret,
    const CID& cid) {
  GenerateResetToken(token, secret, cid);
  memcpy(buf_, token, sizeof(buf_));
}

StatelessResetToken::StatelessResetToken(
    const uint8_t* secret,
    const CID& cid) {
  GenerateResetToken(buf_, secret, cid);
}

void RandomConnectionIDTraits::NewConnectionID(
  const Options& options,
  State* state,
  Session* session,
  ngtcp2_cid* cid,
  size_t length_hint) {
  CHECK_NOT_NULL(cid);
  crypto::EntropySource(
      reinterpret_cast<unsigned char*>(cid->data),
      length_hint);
  cid->data[0] |= 0xc0;
  cid->datalen = length_hint;
}

void RandomConnectionIDTraits::New(
    const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
  Environment* env = Environment::GetCurrent(args);
  new RandomConnectionIDBase(env, args.This());
}

Local<FunctionTemplate> RandomConnectionIDTraits::GetConstructorTemplate(
    Environment* env) {
  BindingState* state = env->GetBindingData<BindingState>(env->context());
  Local<FunctionTemplate> tmpl =
      state->random_connection_id_strategy_constructor_template();
  if (tmpl.IsEmpty()) {
    tmpl = env->NewFunctionTemplate(New);
    tmpl->SetClassName(OneByteString(env->isolate(), name));
    tmpl->Inherit(BaseObject::GetConstructorTemplate(env));
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        BaseObject::kInternalFieldCount);
    state->set_random_connection_id_strategy_constructor_template(tmpl);
  }
  return tmpl;
}

bool RandomConnectionIDTraits::HasInstance(
    Environment* env,
    Local<Value> value) {
  return GetConstructorTemplate(env)->HasInstance(value);
}

namespace {
void InitializeCallbacks(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  BindingState* state = env->GetBindingData<BindingState>(env->context());
  CHECK(!state->initialized);
  if (!args[0]->IsObject())
    return THROW_ERR_INVALID_ARG_TYPE(env, "Missing Callbacks");
  Local<Object> obj = args[0].As<Object>();
#define V(name, key)                                                           \
  do {                                                                         \
    Local<Value> val;                                                          \
    if (!obj->Get(                                                             \
            env->context(),                                                    \
            FIXED_ONE_BYTE_STRING(                                             \
                env->isolate(),                                                \
                "on" # key)).ToLocal(&val) ||                                  \
        !val->IsFunction()) {                                                  \
      return THROW_ERR_MISSING_ARGS(                                           \
          env->isolate(),                                                      \
          "Missing Callback: on" # key);                                       \
    }                                                                          \
    state->set_ ## name ## _callback(val.As<Function>());                      \
  } while (0);
  QUIC_JS_CALLBACKS(V)
#undef V
  state->initialized = true;
}

template <ngtcp2_crypto_side side>
void CreateSecureContext(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  crypto::SecureContext* context = crypto::SecureContext::Create(env);
  if (UNLIKELY(context == nullptr)) return;
  InitializeSecureContext(context, side);
  args.GetReturnValue().Set(context->object());
}
}  // namespace

#endif  // OPENSSL_INFO_QUIC

void Initialize(
    Local<Object> target,
    Local<Value> unused,
    Local<Context> context,
    void* priv) {
#ifdef OPENSSL_INFO_QUIC
  Environment* env = Environment::GetCurrent(context);

  if (UNLIKELY(!BindingState::Initialize(env, target)))
    return;

  EndpointWrap::Initialize(env, target);
  Session::Initialize(env, target);
  Stream::Initialize(env, target);
  RandomConnectionIDBase::Initialize(env, target);

  env->SetMethod(target, "initializeCallbacks", InitializeCallbacks);
  env->SetMethod(target, "createClientSecureContext",
                 CreateSecureContext<NGTCP2_CRYPTO_SIDE_CLIENT>);
  env->SetMethod(target, "createServerSecureContext",
                 CreateSecureContext<NGTCP2_CRYPTO_SIDE_SERVER>);

  constexpr uint32_t NGTCP2_PREFERRED_ADDRESS_USE =
      static_cast<uint32_t>(PreferredAddress::Policy::USE);
  constexpr uint32_t NGTCP2_PREFERRED_ADDRESS_IGNORE =
      static_cast<uint32_t>(PreferredAddress::Policy::IGNORE);

  constexpr uint32_t QUICERROR_TYPE_TRANSPORT =
      static_cast<uint32_t>(QuicError::Type::TRANSPORT);
  constexpr uint32_t QUICERROR_TYPE_APPLICATION =
      static_cast<uint32_t>(QuicError::Type::APPLICATION);

  NODE_DEFINE_STRING_CONSTANT(target, "HTTP3_ALPN", &NGHTTP3_ALPN_H3[1]);
  NODE_DEFINE_CONSTANT(target, AF_INET);
  NODE_DEFINE_CONSTANT(target, AF_INET6);
  NODE_DEFINE_CONSTANT(target, NGTCP2_CC_ALGO_CUBIC);
  NODE_DEFINE_CONSTANT(target, NGTCP2_CC_ALGO_RENO);
  NODE_DEFINE_CONSTANT(target, NGTCP2_PREFERRED_ADDRESS_IGNORE);
  NODE_DEFINE_CONSTANT(target, NGTCP2_PREFERRED_ADDRESS_USE);
  NODE_DEFINE_CONSTANT(target, NGTCP2_MAX_CIDLEN);
  NODE_DEFINE_CONSTANT(target, NGTCP2_APP_NOERROR);
  NODE_DEFINE_CONSTANT(target, NGTCP2_NO_ERROR);
  NODE_DEFINE_CONSTANT(target, NGTCP2_INTERNAL_ERROR);
  NODE_DEFINE_CONSTANT(target, NGTCP2_CONNECTION_REFUSED);
  NODE_DEFINE_CONSTANT(target, NGTCP2_FLOW_CONTROL_ERROR);
  NODE_DEFINE_CONSTANT(target, NGTCP2_STREAM_LIMIT_ERROR);
  NODE_DEFINE_CONSTANT(target, NGTCP2_STREAM_STATE_ERROR);
  NODE_DEFINE_CONSTANT(target, NGTCP2_FINAL_SIZE_ERROR);
  NODE_DEFINE_CONSTANT(target, NGTCP2_FRAME_ENCODING_ERROR);
  NODE_DEFINE_CONSTANT(target, NGTCP2_TRANSPORT_PARAMETER_ERROR);
  NODE_DEFINE_CONSTANT(target, NGTCP2_CONNECTION_ID_LIMIT_ERROR);
  NODE_DEFINE_CONSTANT(target, NGTCP2_PROTOCOL_VIOLATION);
  NODE_DEFINE_CONSTANT(target, NGTCP2_INVALID_TOKEN);
  NODE_DEFINE_CONSTANT(target, NGTCP2_APPLICATION_ERROR);
  NODE_DEFINE_CONSTANT(target, NGTCP2_CRYPTO_BUFFER_EXCEEDED);
  NODE_DEFINE_CONSTANT(target, NGTCP2_KEY_UPDATE_ERROR);
  NODE_DEFINE_CONSTANT(target, NGTCP2_CRYPTO_ERROR);
  NODE_DEFINE_CONSTANT(target, NGHTTP3_H3_NO_ERROR);
  NODE_DEFINE_CONSTANT(target, QUICERROR_TYPE_TRANSPORT);
  NODE_DEFINE_CONSTANT(target, QUICERROR_TYPE_APPLICATION);
#endif  // OPENSSL_INFO_QUIC
}

}  // namespace quic
}  // namespace node

// The internalBinding('quic') will be available even if quic
// support is not enabled. This prevents the internalBinding call
// from throwing. However, if quic is not enabled, the binding will
// have no exports.
NODE_MODULE_CONTEXT_AWARE_INTERNAL(quic, node::quic::Initialize)
