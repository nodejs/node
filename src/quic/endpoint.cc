#if HAVE_OPENSSL && NODE_OPENSSL_HAS_QUIC

#include "endpoint.h"
#include <aliased_struct-inl.h>
#include <async_wrap-inl.h>
#include <env-inl.h>
#include <memory_tracker-inl.h>
#include <ngtcp2/ngtcp2.h>
#include <node_errors.h>
#include <node_external_reference.h>
#include <node_sockaddr-inl.h>
#include <req_wrap-inl.h>
#include <util-inl.h>
#include <uv.h>
#include <v8.h>
#include <limits>
#include "application.h"
#include "bindingdata.h"
#include "defs.h"

namespace node {

using v8::ArrayBufferView;
using v8::BackingStore;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Int32;
using v8::Integer;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::Nothing;
using v8::Number;
using v8::Object;
using v8::ObjectTemplate;
using v8::PropertyAttribute;
using v8::String;
using v8::Uint32;
using v8::Value;

namespace quic {

#define ENDPOINT_STATE(V)                                                      \
  /* Bound to the UDP port */                                                  \
  V(BOUND, bound, uint8_t)                                                     \
  /* Receiving packets on the UDP port */                                      \
  V(RECEIVING, receiving, uint8_t)                                             \
  /* Listening as a QUIC server */                                             \
  V(LISTENING, listening, uint8_t)                                             \
  /* In the process of closing down, waiting for pending send callbacks */     \
  V(CLOSING, closing, uint8_t)                                                 \
  /* Temporarily paused serving new initial requests */                        \
  V(BUSY, busy, uint8_t)                                                       \
  /* The number of pending send callbacks */                                   \
  V(PENDING_CALLBACKS, pending_callbacks, uint64_t)

#define ENDPOINT_STATS(V)                                                      \
  V(CREATED_AT, created_at)                                                    \
  V(DESTROYED_AT, destroyed_at)                                                \
  V(BYTES_RECEIVED, bytes_received)                                            \
  V(BYTES_SENT, bytes_sent)                                                    \
  V(PACKETS_RECEIVED, packets_received)                                        \
  V(PACKETS_SENT, packets_sent)                                                \
  V(SERVER_SESSIONS, server_sessions)                                          \
  V(CLIENT_SESSIONS, client_sessions)                                          \
  V(SERVER_BUSY_COUNT, server_busy_count)                                      \
  V(RETRY_COUNT, retry_count)                                                  \
  V(VERSION_NEGOTIATION_COUNT, version_negotiation_count)                      \
  V(STATELESS_RESET_COUNT, stateless_reset_count)                              \
  V(IMMEDIATE_CLOSE_COUNT, immediate_close_count)

#define ENDPOINT_CC(V)                                                         \
  V(RENO, reno)                                                                \
  V(CUBIC, cubic)                                                              \
  V(BBR, bbr)

struct Endpoint::State {
#define V(_, name, type) type name;
  ENDPOINT_STATE(V)
#undef V
};

STAT_STRUCT(Endpoint, ENDPOINT)

// ============================================================================
// Endpoint::Options
namespace {
#ifdef DEBUG
bool is_diagnostic_packet_loss(double probability) {
  if (LIKELY(probability == 0.0)) return false;
  unsigned char c = 255;
  CHECK(crypto::CSPRNG(&c, 1).is_ok());
  return (static_cast<double>(c) / 255) < probability;
}
#endif  // DEBUG

Maybe<ngtcp2_cc_algo> getAlgoFromString(Environment* env, Local<String> input) {
  auto& state = BindingData::Get(env);
#define V(name, str)                                                           \
  if (input->StringEquals(state.str##_string())) {                             \
    return Just(NGTCP2_CC_ALGO_##name);                                        \
  }

  ENDPOINT_CC(V)

#undef V
  return Nothing<ngtcp2_cc_algo>();
}

template <typename Opt, ngtcp2_cc_algo Opt::*member>
bool SetOption(Environment* env,
               Opt* options,
               const Local<Object>& object,
               const Local<String>& name) {
  Local<Value> value;
  if (!object->Get(env->context(), name).ToLocal(&value)) return false;
  if (!value->IsUndefined()) {
    ngtcp2_cc_algo algo;
    if (value->IsString()) {
      if (!getAlgoFromString(env, value.As<String>()).To(&algo)) {
        THROW_ERR_INVALID_ARG_VALUE(env, "The cc_algorithm option is invalid");
        return false;
      }
    } else {
      if (!value->IsInt32()) {
        THROW_ERR_INVALID_ARG_VALUE(
            env, "The cc_algorithm option must be a string or an integer");
        return false;
      }
      Local<Int32> num;
      if (!value->ToInt32(env->context()).ToLocal(&num)) {
        THROW_ERR_INVALID_ARG_VALUE(env, "The cc_algorithm option is invalid");
        return false;
      }
      switch (num->Value()) {
#define V(name, _)                                                             \
  case NGTCP2_CC_ALGO_##name:                                                  \
    break;
        ENDPOINT_CC(V)
#undef V
        default:
          THROW_ERR_INVALID_ARG_VALUE(env,
                                      "The cc_algorithm option is invalid");
          return false;
      }
      algo = static_cast<ngtcp2_cc_algo>(num->Value());
    }
    options->*member = algo;
  }
  return true;
}

#if DEBUG
template <typename Opt, double Opt::*member>
bool SetOption(Environment* env,
               Opt* options,
               const Local<Object>& object,
               const Local<String>& name) {
  Local<Value> value;
  if (!object->Get(env->context(), name).ToLocal(&value)) return false;
  if (!value->IsUndefined()) {
    Local<Number> num;
    if (!value->ToNumber(env->context()).ToLocal(&num)) {
      Utf8Value nameStr(env->isolate(), name);
      THROW_ERR_INVALID_ARG_VALUE(
          env, "The %s option must be a number", *nameStr);
      return false;
    }
    options->*member = num->Value();
  }
  return true;
}
#endif  // DEBUG

template <typename Opt, uint8_t Opt::*member>
bool SetOption(Environment* env,
               Opt* options,
               const Local<Object>& object,
               const Local<String>& name) {
  Local<Value> value;
  if (!object->Get(env->context(), name).ToLocal(&value)) return false;
  if (!value->IsUndefined()) {
    if (!value->IsUint32()) {
      Utf8Value nameStr(env->isolate(), name);
      THROW_ERR_INVALID_ARG_VALUE(
          env, "The %s option must be an uint8", *nameStr);
      return false;
    }
    Local<Uint32> num;
    if (!value->ToUint32(env->context()).ToLocal(&num) ||
        num->Value() > std::numeric_limits<uint8_t>::max()) {
      Utf8Value nameStr(env->isolate(), name);
      THROW_ERR_INVALID_ARG_VALUE(
          env, "The %s option must be an uint8", *nameStr);
      return false;
    }
    options->*member = num->Value();
  }
  return true;
}

template <typename Opt, TokenSecret Opt::*member>
bool SetOption(Environment* env,
               Opt* options,
               const Local<Object>& object,
               const Local<String>& name) {
  Local<Value> value;
  if (!object->Get(env->context(), name).ToLocal(&value)) return false;
  if (!value->IsUndefined()) {
    if (!value->IsArrayBufferView()) {
      Utf8Value nameStr(env->isolate(), name);
      THROW_ERR_INVALID_ARG_VALUE(
          env, "The %s option must be an ArrayBufferView", *nameStr);
      return false;
    }
    Store store(value.As<ArrayBufferView>());
    if (store.length() != TokenSecret::QUIC_TOKENSECRET_LEN) {
      Utf8Value nameStr(env->isolate(), name);
      THROW_ERR_INVALID_ARG_VALUE(
          env,
          "The %s option must be an ArrayBufferView of length %d",
          *nameStr,
          TokenSecret::QUIC_TOKENSECRET_LEN);
      return false;
    }
    ngtcp2_vec buf = store;
    TokenSecret secret(buf.base);
    options->*member = secret;
  }
  return true;
}
}  // namespace

Maybe<Endpoint::Options> Endpoint::Options::From(Environment* env,
                                                 Local<Value> value) {
  if (value.IsEmpty() || !value->IsObject()) {
    THROW_ERR_INVALID_ARG_TYPE(env, "options must be an object");
    return Nothing<Options>();
  }

  auto& state = BindingData::Get(env);
  auto params = value.As<Object>();
  Options options;

#define SET(name)                                                              \
  SetOption<Endpoint::Options, &Endpoint::Options::name>(                      \
      env, &options, params, state.name##_string())

  if (!SET(retry_token_expiration) || !SET(token_expiration) ||
      !SET(max_connections_per_host) || !SET(max_connections_total) ||
      !SET(max_stateless_resets) || !SET(address_lru_size) ||
      !SET(max_retries) || !SET(max_payload_size) ||
      !SET(unacknowledged_packet_threshold) || !SET(validate_address) ||
      !SET(disable_stateless_reset) || !SET(ipv6_only) ||
#ifdef DEBUG
      !SET(rx_loss) || !SET(tx_loss) ||
#endif
      !SET(cc_algorithm) || !SET(udp_receive_buffer_size) ||
      !SET(udp_send_buffer_size) || !SET(udp_ttl) || !SET(reset_token_secret) ||
      !SET(token_secret)) {
    return Nothing<Options>();
  }

  Local<Value> address;
  if (!params->Get(env->context(), env->address_string()).ToLocal(&address)) {
    return Nothing<Options>();
  }
  if (!address->IsUndefined()) {
    if (!SocketAddressBase::HasInstance(env, address)) {
      THROW_ERR_INVALID_ARG_TYPE(env,
                                 "The address option must be a SocketAddress");
      return Nothing<Options>();
    }
    auto addr = FromJSObject<SocketAddressBase>(address.As<v8::Object>());
    options.local_address = addr->address();
  } else {
    options.local_address = std::make_shared<SocketAddress>();
    if (!SocketAddress::New("127.0.0.1", 0, options.local_address.get())) {
      THROW_ERR_INVALID_ADDRESS(env);
      return Nothing<Options>();
    }
  }

  return Just<Options>(options);

#undef SET
}

void Endpoint::Options::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("reset_token_secret", reset_token_secret);
  tracker->TrackField("token_secret", token_secret);
}

// ======================================================================================
// Endpoint::UDP and Endpoint::UDP::Impl

class Endpoint::UDP::Impl final : public HandleWrap {
 public:
  static Local<FunctionTemplate> GetConstructorTemplate(Environment* env) {
    auto& state = BindingData::Get(env);
    auto tmpl = state.udp_constructor_template();
    if (tmpl.IsEmpty()) {
      tmpl = NewFunctionTemplate(env->isolate(), IllegalConstructor);
      tmpl->Inherit(HandleWrap::GetConstructorTemplate(env));
      tmpl->InstanceTemplate()->SetInternalFieldCount(
          HandleWrap::kInternalFieldCount);
      tmpl->SetClassName(state.endpoint_udp_string());
      state.set_udp_constructor_template(tmpl);
    }
    return tmpl;
  }

  static Impl* Create(Endpoint* endpoint) {
    Local<Object> obj;
    if (!GetConstructorTemplate(endpoint->env())
             ->InstanceTemplate()
             ->NewInstance(endpoint->env()->context())
             .ToLocal(&obj)) {
      return nullptr;
    }
    return new Impl(endpoint, obj);
  }

  static Impl* From(uv_udp_t* handle) {
    return ContainerOf(&Impl::handle_, handle);
  }

  static Impl* From(uv_handle_t* handle) {
    return From(reinterpret_cast<uv_udp_t*>(handle));
  }

  Impl(Endpoint* endpoint, Local<Object> object)
      : HandleWrap(endpoint->env(),
                   object,
                   reinterpret_cast<uv_handle_t*>(&handle_),
                   AsyncWrap::PROVIDER_QUIC_UDP),
        endpoint_(endpoint) {
    CHECK_EQ(uv_udp_init(endpoint->env()->event_loop(), &handle_), 0);
    handle_.data = this;
  }

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(Endpoint::UDP::Impl)
  SET_SELF_SIZE(Impl)

 private:
  static void OnAlloc(uv_handle_t* handle,
                      size_t suggested_size,
                      uv_buf_t* buf) {
    *buf = From(handle)->env()->allocate_managed_buffer(suggested_size);
  }

  static void OnReceive(uv_udp_t* handle,
                        ssize_t nread,
                        const uv_buf_t* buf,
                        const sockaddr* addr,
                        unsigned int flags) {
    // Nothing to do in these cases. Specifically, if the nread
    // is zero or we've received a partial packet, we're just
    // going to ignore it.
    if (nread == 0 || flags & UV_UDP_PARTIAL) return;

    auto impl = From(handle);
    DCHECK_NOT_NULL(impl);
    DCHECK_NOT_NULL(impl->endpoint_);

    if (nread < 0) {
      impl->endpoint_->Destroy(CloseContext::RECEIVE_FAILURE,
                               static_cast<int>(nread));
      return;
    }

    impl->endpoint_->Receive(uv_buf_init(buf->base, static_cast<size_t>(nread)),
                             SocketAddress(addr));
  }

  uv_udp_t handle_;
  Endpoint* endpoint_;

  friend class UDP;
};

Endpoint::UDP::UDP(Endpoint* endpoint) : impl_(Impl::Create(endpoint)) {
  DCHECK(impl_);
}

Endpoint::UDP::~UDP() {
  Close();
}

int Endpoint::UDP::Bind(const Endpoint::Options& options) {
  if (is_bound_) return UV_EALREADY;
  if (is_closed_or_closing()) return UV_EBADF;

  int flags = 0;
  if (options.local_address->family() == AF_INET6 && options.ipv6_only)
    flags |= UV_UDP_IPV6ONLY;
  int err = uv_udp_bind(&impl_->handle_, options.local_address->data(), flags);
  int size;

  if (!err) {
    is_bound_ = true;
    size = static_cast<int>(options.udp_receive_buffer_size);
    if (size > 0) {
      err = uv_recv_buffer_size(reinterpret_cast<uv_handle_t*>(&impl_->handle_),
                                &size);
      if (err) return err;
    }

    size = static_cast<int>(options.udp_send_buffer_size);
    if (size > 0) {
      err = uv_send_buffer_size(reinterpret_cast<uv_handle_t*>(&impl_->handle_),
                                &size);
      if (err) return err;
    }

    size = static_cast<int>(options.udp_ttl);
    if (size > 0) {
      err = uv_udp_set_ttl(&impl_->handle_, size);
      if (err) return err;
    }
  }

  return err;
}

void Endpoint::UDP::Ref() {
  if (!is_closed_or_closing()) {
    uv_ref(reinterpret_cast<uv_handle_t*>(&impl_->handle_));
  }
}

void Endpoint::UDP::Unref() {
  if (!is_closed_or_closing()) {
    uv_unref(reinterpret_cast<uv_handle_t*>(&impl_->handle_));
  }
}

int Endpoint::UDP::Start() {
  if (is_closed_or_closing()) return UV_EBADF;
  if (is_started_) return 0;
  int err = uv_udp_recv_start(&impl_->handle_, Impl::OnAlloc, Impl::OnReceive);
  is_started_ = (err == 0);
  return err;
}

void Endpoint::UDP::Stop() {
  if (is_closed_or_closing() || !is_started_) return;
  USE(uv_udp_recv_stop(&impl_->handle_));
  is_started_ = false;
}

void Endpoint::UDP::Close() {
  if (is_closed_or_closing()) return;
  DCHECK(impl_);
  Stop();
  is_bound_ = false;
  is_closed_ = true;
  impl_->Close();
  impl_.reset();
}

bool Endpoint::UDP::is_bound() const {
  return is_bound_;
}

bool Endpoint::UDP::is_closed() const {
  return is_closed_;
}

bool Endpoint::UDP::is_closed_or_closing() const {
  if (is_closed() || !impl_) return true;
  return impl_->IsHandleClosing();
}

Endpoint::UDP::operator bool() const {
  return impl_;
}

SocketAddress Endpoint::UDP::local_address() const {
  DCHECK(!is_closed_or_closing() && is_bound());
  return SocketAddress::FromSockName(impl_->handle_);
}

int Endpoint::UDP::Send(BaseObjectPtr<Packet> packet) {
  if (is_closed_or_closing()) return UV_EBADF;
  DCHECK(packet && !packet->is_sending());
  uv_buf_t buf = *packet;
  return packet->Dispatch(
      uv_udp_send,
      &impl_->handle_,
      &buf,
      1,
      packet->destination().data(),
      uv_udp_send_cb{[](uv_udp_send_t* req, int status) {
        auto ptr = static_cast<Packet*>(ReqWrap<uv_udp_send_t>::from_req(req));
        ptr->Done(status);
      }});
}

void Endpoint::UDP::MemoryInfo(MemoryTracker* tracker) const {
  if (impl_) tracker->TrackField("impl", impl_);
}

// ============================================================================

bool Endpoint::HasInstance(Environment* env, Local<Value> value) {
  return GetConstructorTemplate(env)->HasInstance(value);
}

Local<FunctionTemplate> Endpoint::GetConstructorTemplate(Environment* env) {
  auto& state = BindingData::Get(env);
  auto tmpl = state.endpoint_constructor_template();
  if (tmpl.IsEmpty()) {
    auto isolate = env->isolate();
    tmpl = NewFunctionTemplate(isolate, New);
    tmpl->Inherit(AsyncWrap::GetConstructorTemplate(env));
    tmpl->SetClassName(state.endpoint_string());
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        Endpoint::kInternalFieldCount);
    SetProtoMethod(isolate, tmpl, "listen", DoListen);
    SetProtoMethod(isolate, tmpl, "closeGracefully", DoCloseGracefully);
    SetProtoMethod(isolate, tmpl, "connect", DoConnect);
    SetProtoMethod(isolate, tmpl, "markBusy", MarkBusy);
    SetProtoMethod(isolate, tmpl, "ref", Ref);
    SetProtoMethodNoSideEffect(isolate, tmpl, "address", LocalAddress);
    state.set_endpoint_constructor_template(tmpl);
  }
  return tmpl;
}

void Endpoint::InitPerIsolate(IsolateData* data, Local<ObjectTemplate> target) {
  // TODO(@jasnell): Implement the per-isolate state
}

void Endpoint::InitPerContext(Realm* realm, Local<Object> target) {
#define V(name, str)                                                           \
  NODE_DEFINE_CONSTANT(target, QUIC_CC_ALGO_##name);                           \
  NODE_DEFINE_STRING_CONSTANT(target, "QUIC_CC_ALGO_" #name "_STR", #str);
  ENDPOINT_CC(V)
#undef V

#define V(name, _) IDX_STATS_ENDPOINT_##name,
  enum IDX_STATS_ENDPONT { ENDPOINT_STATS(V) IDX_STATS_ENDPOINT_COUNT };
  NODE_DEFINE_CONSTANT(target, IDX_STATS_ENDPOINT_COUNT);
#undef V

#define V(name, key) NODE_DEFINE_CONSTANT(target, IDX_STATS_ENDPOINT_##name);
  ENDPOINT_STATS(V);
#undef V

#define V(name, key, type)                                                     \
  static constexpr auto IDX_STATE_ENDPOINT_##name =                            \
      offsetof(Endpoint::State, key);                                          \
  static constexpr auto IDX_STATE_ENDPOINT_##name##_SIZE = sizeof(type);       \
  NODE_DEFINE_CONSTANT(target, IDX_STATE_ENDPOINT_##name);                     \
  NODE_DEFINE_CONSTANT(target, IDX_STATE_ENDPOINT_##name##_SIZE);
  ENDPOINT_STATE(V)
#undef V

  NODE_DEFINE_CONSTANT(target, DEFAULT_MAX_CONNECTIONS);
  NODE_DEFINE_CONSTANT(target, DEFAULT_MAX_CONNECTIONS_PER_HOST);
  NODE_DEFINE_CONSTANT(target, DEFAULT_MAX_SOCKETADDRESS_LRU_SIZE);
  NODE_DEFINE_CONSTANT(target, DEFAULT_MAX_STATELESS_RESETS);
  NODE_DEFINE_CONSTANT(target, DEFAULT_MAX_RETRY_LIMIT);

  static constexpr auto DEFAULT_RETRYTOKEN_EXPIRATION =
      RetryToken::QUIC_DEFAULT_RETRYTOKEN_EXPIRATION / NGTCP2_SECONDS;
  static constexpr auto DEFAULT_REGULARTOKEN_EXPIRATION =
      RegularToken::QUIC_DEFAULT_REGULARTOKEN_EXPIRATION / NGTCP2_SECONDS;
  static constexpr auto DEFAULT_MAX_PACKET_LENGTH = kDefaultMaxPacketLength;
  NODE_DEFINE_CONSTANT(target, DEFAULT_RETRYTOKEN_EXPIRATION);
  NODE_DEFINE_CONSTANT(target, DEFAULT_REGULARTOKEN_EXPIRATION);
  NODE_DEFINE_CONSTANT(target, DEFAULT_MAX_PACKET_LENGTH);

  SetConstructorFunction(realm->context(),
                         target,
                         "Endpoint",
                         GetConstructorTemplate(realm->env()));
}

void Endpoint::RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(New);
  registry->Register(DoConnect);
  registry->Register(DoListen);
  registry->Register(DoCloseGracefully);
  registry->Register(LocalAddress);
  registry->Register(Ref);
  registry->Register(MarkBusy);
}

Endpoint::Endpoint(Environment* env,
                   Local<Object> object,
                   const Endpoint::Options& options)
    : AsyncWrap(env, object, AsyncWrap::PROVIDER_QUIC_ENDPOINT),
      stats_(env->isolate()),
      state_(env->isolate()),
      options_(options),
      udp_(this),
      addrLRU_(options_.address_lru_size) {
  MakeWeak();

  const auto defineProperty = [&](auto name, auto value) {
    object
        ->DefineOwnProperty(
            env->context(), name, value, PropertyAttribute::ReadOnly)
        .Check();
  };

  defineProperty(env->state_string(), state_.GetArrayBuffer());
  defineProperty(env->stats_string(), stats_.GetArrayBuffer());
}

SocketAddress Endpoint::local_address() const {
  DCHECK(!is_closed() && !is_closing());
  return udp_.local_address();
}

void Endpoint::MarkAsBusy(bool on) {
  state_->busy = on ? 1 : 0;
}

RegularToken Endpoint::GenerateNewToken(uint32_t version,
                                        const SocketAddress& remote_address) {
  DCHECK(!is_closed() && !is_closing());
  return RegularToken(version, remote_address, options_.token_secret);
}

StatelessResetToken Endpoint::GenerateNewStatelessResetToken(
    uint8_t* token, const CID& cid) const {
  DCHECK(!is_closed() && !is_closing());
  return StatelessResetToken(token, options_.reset_token_secret, cid);
}

void Endpoint::AddSession(const CID& cid, BaseObjectPtr<Session> session) {
  if (is_closed() || is_closing()) return;
  sessions_[cid] = session;
  IncrementSocketAddressCounter(session->remote_address());
  if (session->is_server()) {
    STAT_INCREMENT(Stats, server_sessions);
  } else {
    STAT_INCREMENT(Stats, client_sessions);
  }
  if (session->is_server()) EmitNewSession(session);
}

void Endpoint::RemoveSession(const CID& cid) {
  if (is_closed()) return;
  auto session = FindSession(cid);
  if (!session) return;
  DecrementSocketAddressCounter(session->remote_address());
  sessions_.erase(cid);
  if (state_->closing == 1) MaybeDestroy();
}

BaseObjectPtr<Session> Endpoint::FindSession(const CID& cid) {
  BaseObjectPtr<Session> session;
  auto session_it = sessions_.find(cid);
  if (session_it == std::end(sessions_)) {
    auto scid_it = dcid_to_scid_.find(cid);
    if (scid_it != std::end(dcid_to_scid_)) {
      session_it = sessions_.find(scid_it->second);
      CHECK_NE(session_it, std::end(sessions_));
      session = session_it->second;
    }
  } else {
    session = session_it->second;
  }
  return session;
}

void Endpoint::AssociateCID(const CID& cid, const CID& scid) {
  if (!is_closed() && !is_closing() && cid && scid && cid != scid &&
      dcid_to_scid_[cid] != scid) {
    dcid_to_scid_.emplace(cid, scid);
  }
}

void Endpoint::DisassociateCID(const CID& cid) {
  if (!is_closed() && cid) dcid_to_scid_.erase(cid);
}

void Endpoint::AssociateStatelessResetToken(const StatelessResetToken& token,
                                            Session* session) {
  if (is_closed() || is_closing()) return;
  token_map_[token] = session;
}

void Endpoint::DisassociateStatelessResetToken(
    const StatelessResetToken& token) {
  if (!is_closed()) token_map_.erase(token);
}

void Endpoint::Send(BaseObjectPtr<Packet> packet) {
#ifdef DEBUG
  // When diagnostic packet loss is enabled, the packet will be randomly
  // dropped. This can happen to any type of packet. We use this only in
  // testing to test various reliability issues.
  if (UNLIKELY(is_diagnostic_packet_loss(options_.tx_loss))) {
    packet->Done(0);
    // Simulating tx packet loss
    return;
  }
#endif  // DEBUG

  if (is_closed() || is_closing() || packet->length() == 0) return;
  state_->pending_callbacks++;
  int err = udp_.Send(packet);

  if (err != 0) {
    packet->Done(err);
    Destroy(CloseContext::SEND_FAILURE, err);
  }
  STAT_INCREMENT_N(Stats, bytes_sent, packet->length());
  STAT_INCREMENT(Stats, packets_sent);
}

void Endpoint::SendRetry(const PathDescriptor& options) {
  // Generating and sending retry packets does consume some system resources,
  // and it is possible for a malicious peer to trigger sending a large number
  // of retry packets, resulting in a potential DOS vector. To help ward that
  // off, we track how many retry packets we send to a particular host and
  // enforce limits. Note that since we are using an LRU cache these limits
  // aren't strict. If a retry is sent, we increment the retry_count statistic
  // to give application code a means of detecting and responding to abuse on
  // its own. What this count does not give is the rate of retry, so it is still
  // somewhat limited.
  auto info = addrLRU_.Upsert(options.remote_address);
  if (++(info->retry_count) <= options_.max_retries) {
    auto packet =
        Packet::CreateRetryPacket(env(), this, options, options_.token_secret);
    if (packet) {
      STAT_INCREMENT(Stats, retry_count);
      Send(std::move(packet));
    }

    // If creating the retry is unsuccessful, we just drop things on the floor.
    // It's not worth committing any further resources to this one packet. We
    // might want to log the failure at some point tho.
  }
}

void Endpoint::SendVersionNegotiation(const PathDescriptor& options) {
  // While creating and sending a version negotiation packet does consume a
  // small amount of system resources, and while it is fairly trivial for a
  // malicious peer to force a version negotiation to be sent, these are more
  // trivial to create than the cryptographically generated retry and stateless
  // reset packets. If the packet is sent, then we'll at least increment the
  // version_negotiation_count statistic so that application code can keep an
  // eye on it.
  auto packet = Packet::CreateVersionNegotiationPacket(env(), this, options);
  if (packet) {
    STAT_INCREMENT(Stats, version_negotiation_count);
    Send(std::move(packet));
  }

  // If creating the packet is unsuccessful, we just drop things on the floor.
  // It's not worth committing any further resources to this one packet. We
  // might want to log the failure at some point tho.
}

bool Endpoint::SendStatelessReset(const PathDescriptor& options,
                                  size_t source_len) {
  if (UNLIKELY(options_.disable_stateless_reset)) return false;

  const auto exceeds_limits = [&] {
    SocketAddressInfoTraits::Type* counts =
        addrLRU_.Peek(options.remote_address);
    auto count = counts != nullptr ? counts->reset_count : 0;
    return count >= options_.max_stateless_resets;
  };

  // Per the QUIC spec, we need to protect against sending too many stateless
  // reset tokens to an endpoint to prevent endless looping.
  if (exceeds_limits()) return false;

  auto packet = Packet::CreateStatelessResetPacket(
      env(), this, options, options_.reset_token_secret, source_len);

  if (packet) {
    addrLRU_.Upsert(options.remote_address)->reset_count++;
    STAT_INCREMENT(Stats, stateless_reset_count);
    Send(std::move(packet));
    return true;
  }
  return false;
}

void Endpoint::SendImmediateConnectionClose(const PathDescriptor& options,
                                            QuicError reason) {
  // While it is possible for a malicious peer to cause us to create a large
  // number of these, generating them is fairly trivial.
  auto packet = Packet::CreateImmediateConnectionClosePacket(
      env(), this, options, reason);
  if (packet) {
    STAT_INCREMENT(Stats, immediate_close_count);
    Send(std::move(packet));
  }
}

bool Endpoint::Start() {
  if (is_closed() || is_closing()) return false;
  if (state_->receiving == 1) return true;

  int err = 0;
  if (state_->bound == 0) {
    err = udp_.Bind(options_);
    if (err != 0) {
      // If we failed to bind, destroy the endpoint. There's nothing we can do.
      Destroy(CloseContext::BIND_FAILURE, err);
      return false;
    }
    state_->bound = 1;
  }

  err = udp_.Start();
  if (err != 0) {
    // If we failed to start listening, destroy the endpoint. There's nothing we
    // can do.
    Destroy(CloseContext::START_FAILURE, err);
    return false;
  }

  BindingData::Get(env()).listening_endpoints[this] =
      BaseObjectPtr<Endpoint>(this);
  state_->receiving = 1;
  return true;
}

void Endpoint::Listen(const Session::Options& options) {
  if (is_closed() || is_closing() || state_->listening == 1) return;
  server_options_ = options;
  if (Start()) state_->listening = 1;
}

BaseObjectPtr<Session> Endpoint::Connect(
    const SocketAddress& remote_address,
    const Session::Options& options,
    std::optional<SessionTicket> session_ticket) {
  // If starting fails, the endpoint will be destroyed.
  if (!Start()) return BaseObjectPtr<Session>();

  auto config = Session::Config(
      *this, options, local_address(), remote_address, session_ticket);

  auto session = Session::Create(BaseObjectPtr<Endpoint>(this), config);
  if (!session) return BaseObjectPtr<Session>();
  session->set_wrapped();

  session->application().SendPendingData();
  return BaseObjectPtr<Session>();
}

void Endpoint::MaybeDestroy() {
  if (!is_closed() && sessions_.empty() && state_->pending_callbacks == 0 &&
      state_->listening == 0) {
    Destroy();
  }
}

void Endpoint::Destroy(CloseContext context, int status) {
  if (is_closed()) return;

  STAT_RECORD_TIMESTAMP(Stats, destroyed_at);

  state_->listening = 0;

  close_context_ = context;
  close_status_ = status;

  // If there are open sessions still, shut them down. As those clean themselves
  // up, they will remove themselves. The cleanup here will be synchronous and
  // no attempt will be made to communicate further with the peer.
  // Intentionally copy the sessions map so that we can safely iterate over it
  // while those clean themselves up.
  auto sessions = sessions_;
  for (auto& session : sessions)
    session.second->Close(Session::CloseMethod::SILENT);
  sessions.clear();
  DCHECK(sessions_.empty());
  token_map_.clear();
  dcid_to_scid_.clear();

  udp_.Close();
  state_->closing = 0;
  state_->bound = 0;
  state_->receiving = 0;
  BindingData::Get(env()).listening_endpoints.erase(this);

  EmitClose(close_context_, close_status_);
}

void Endpoint::CloseGracefully() {
  if (is_closed() || is_closing()) return;

  state_->listening = 0;
  state_->closing = 1;

  // Maybe we can go ahead and destroy now?
  MaybeDestroy();
}

void Endpoint::Receive(const uv_buf_t& buf,
                       const SocketAddress& remote_address) {
  const auto receive = [&](Store&& store,
                           const SocketAddress& local_address,
                           const SocketAddress& remote_address,
                           const CID& dcid,
                           const CID& scid) {
    STAT_INCREMENT_N(Stats, bytes_received, store.length());
    auto session = FindSession(dcid);
    return session && !session->is_destroyed()
               ? session->Receive(
                     std::move(store), local_address, remote_address)
               : false;
  };

  const auto accept = [&](const Session::Config& config, Store&& store) {
    if (is_closed() || is_closing() || !is_listening()) return false;

    auto session = Session::Create(BaseObjectPtr<Endpoint>(this), config);

    return session ? session->Receive(std::move(store),
                                      config.local_address,
                                      config.remote_address)
                   : false;
  };

  const auto acceptInitialPacket = [&](const uint32_t version,
                                       const CID& dcid,
                                       const CID& scid,
                                       Store&& store,
                                       const SocketAddress& local_address,
                                       const SocketAddress& remote_address) {
    // Conditionally accept an initial packet to create a new session.

    // If we're not listening, do not accept.
    if (state_->listening == 0) return false;

    ngtcp2_pkt_hd hd;

    // This is our first condition check... A minimal check to see if ngtcp2 can
    // even recognize this packet as a quic packet with the correct version.
    ngtcp2_vec vec = store;
    switch (ngtcp2_accept(&hd, vec.base, vec.len)) {
      case 1:
        // The requested QUIC protocol version is not supported
        SendVersionNegotiation(
            PathDescriptor{version, dcid, scid, local_address, remote_address});
        // The packet was successfully processed, even if we did refuse the
        // connection and send a version negotiation in response.
        return true;
      case -1:
        // The packet is invalid and we're just going to ignore it.
        return false;
    }

    // This is the second condition check... If the server has been marked busy
    // or the remote peer has exceeded their maximum number of concurrent
    // connections, any new connections will be shut down immediately.
    const auto limits_exceeded = [&] {
      if (sessions_.size() >= options_.max_connections_total) return true;

      SocketAddressInfoTraits::Type* counts = addrLRU_.Peek(remote_address);
      auto count = counts != nullptr ? counts->active_connections : 0;
      return count >= options_.max_connections_per_host;
    };

    if (state_->busy || limits_exceeded()) {
      // Endpoint is busy or the connection count is exceeded. The connection is
      // refused.
      if (state_->busy) STAT_INCREMENT(Stats, server_busy_count);
      SendImmediateConnectionClose(
          PathDescriptor{version, scid, dcid, local_address, remote_address},
          QuicError::ForTransport(NGTCP2_CONNECTION_REFUSED));
      // The packet was successfully processed, even if we did refuse the
      // connection.
      return true;
    }

    // At this point, we start to set up the configuration for our local
    // session. The second argument to the Config constructor here is the dcid.
    // We pass the received scid here as the value because that is the value
    // *this* session will use as it's outbound dcid.
    auto config = Session::Config(Side::SERVER,
                                  *this,
                                  server_options_.value(),
                                  version,
                                  local_address,
                                  remote_address,
                                  scid,
                                  dcid);

    // The this point, the config.scid and config.dcid represent *our* views of
    // the CIDs. Specifically, config.dcid identifies the peer and config.scid
    // identifies us. config.dcid should equal scid. config.scid should *not*
    // equal dcid.

    const auto is_remote_address_validated = [&] {
      auto info = addrLRU_.Peek(remote_address);
      return info != nullptr ? info->validated : false;
    };

    // QUIC has address validation built in to the handshake but allows for
    // an additional explicit validation request using RETRY frames. If we
    // are using explicit validation, we check for the existence of a valid
    // token in the packet. If one does not exist, we send a retry with
    // a new token. If it does exist, and if it's valid, we grab the original
    // cid and continue.
    if (!is_remote_address_validated()) {
      switch (hd.type) {
        case NGTCP2_PKT_INITIAL:
          // First, let's see if we need to do anything here.

          if (options_.validate_address) {
            // If there is no token, generate and send one.
            if (hd.tokenlen == 0) {
              SendRetry(PathDescriptor{
                  version,
                  dcid,
                  scid,
                  local_address,
                  remote_address,
              });
              // We still consider this a successfully handled packet even
              // if we send a retry.
              return true;
            }

            // We have two kinds of tokens, each prefixed with a different magic
            // byte.
            switch (hd.token[0]) {
              case RetryToken::kTokenMagic: {
                RetryToken token(hd.token, hd.tokenlen);
                auto ocid = token.Validate(
                    version,
                    remote_address,
                    dcid,
                    options_.token_secret,
                    options_.retry_token_expiration * NGTCP2_SECONDS);
                if (ocid == std::nullopt) {
                  // Invalid retry token was detected. Close the connection.
                  SendImmediateConnectionClose(
                      PathDescriptor{
                          version, scid, dcid, local_address, remote_address},
                      QuicError::ForTransport(NGTCP2_CONNECTION_REFUSED));
                  // We still consider this a successfully handled packet even
                  // if we send a connection close.
                  return true;
                }

                // The ocid is the original dcid that was encoded into the
                // original retry packet sent to the client. We use it for
                // validation.
                config.ocid = ocid.value();
                config.retry_scid = dcid;
                break;
              }
              case RegularToken::kTokenMagic: {
                RegularToken token(hd.token, hd.tokenlen);
                if (!token.Validate(
                        version,
                        remote_address,
                        options_.token_secret,
                        options_.token_expiration * NGTCP2_SECONDS)) {
                  SendRetry(PathDescriptor{
                      version,
                      dcid,
                      scid,
                      local_address,
                      remote_address,
                  });
                  // We still consider this to be a successfully handled packet
                  // if a retry is sent.
                  return true;
                }
                hd.token = nullptr;
                hd.tokenlen = 0;
                break;
              }
              default: {
                SendRetry(PathDescriptor{
                    version,
                    dcid,
                    scid,
                    local_address,
                    remote_address,
                });
                return true;
              }
            }

            // Ok! If we've got this far, our token is valid! Which means our
            // path to the remote address is valid (for now). Let's record that
            // so we don't have to do this dance again for this endpoint
            // instance.
            addrLRU_.Upsert(remote_address)->validated = true;
          } else if (hd.tokenlen > 0) {
            // If validation is turned off and there is a token, that's weird.
            // The peer should only have a token if we sent it to them and we
            // wouldn't have sent it unless validation was turned on. Let's
            // assume the peer is buggy or malicious and drop the packet on the
            // floor.
            return false;
          }
          break;
        case NGTCP2_PKT_0RTT:
          // If it's a 0RTT packet, we're always going to perform path
          // validation no matter what. This is a bit unfortunate since
          // ORTT is supposed to be, you know, 0RTT, but sending a retry
          // forces a round trip... but if the remote address is not
          // validated, there's a possibility that this 0RTT is forged
          // or otherwise suspicious. Before we can do anything with it,
          // we have to validate it. Keep in mind that this means the
          // client needs to respond with a proper initial packet in
          // order to proceed.
          // TODO(@jasnell): Validate this further to ensure this is
          // the correct behavior.
          SendRetry(PathDescriptor{
              version,
              dcid,
              scid,
              local_address,
              remote_address,
          });
          return true;
      }
    }

    return accept(config, std::move(store));
  };

  // When a received packet contains a QUIC short header but cannot be matched
  // to a known Session, it is either (a) garbage, (b) a valid packet for a
  // connection we no longer have state for, or (c) a stateless reset. Because
  // we do not yet know if we are going to process the packet, we need to try to
  // quickly determine -- with as little cost as possible -- whether the packet
  // contains a reset token. We do so by checking the final
  // NGTCP2_STATELESS_RESET_TOKENLEN bytes in the packet to see if they match
  // one of the known reset tokens previously given by the remote peer. If
  // there's a match, then it's a reset token, if not, we move on the to the
  // next check. It is very important that this check be as inexpensive as
  // possible to avoid a DOS vector.
  const auto maybeStatelessReset = [&](const CID& dcid,
                                       const CID& scid,
                                       Store& store,
                                       const SocketAddress& local_address,
                                       const SocketAddress& remote_address) {
    if (options_.disable_stateless_reset ||
        store.length() < NGTCP2_STATELESS_RESET_TOKENLEN)
      return false;

    ngtcp2_vec vec = store;
    vec.base += vec.len;
    vec.base -= NGTCP2_STATELESS_RESET_TOKENLEN;

    Session* session = nullptr;
    auto it = token_map_.find(StatelessResetToken(vec.base));
    if (it != token_map_.end()) session = it->second;

    return session != nullptr ? receive(std::move(store),
                                        local_address,
                                        remote_address,
                                        dcid,
                                        scid)
                              : false;
  };

#ifdef DEBUG
  // When diagnostic packet loss is enabled, the packet will be randomly
  // dropped.
  if (UNLIKELY(is_diagnostic_packet_loss(options_.rx_loss))) {
    // Simulating rx packet loss
    return;
  }
#endif  // DEBUG

  // TODO(@jasnell): Implement blocklist support
  // if (UNLIKELY(block_list_->Apply(remote_address))) {
  //   Debug(this, "Ignoring blocked remote address: %s", remote_address);
  //   return;
  // }

  std::shared_ptr<BackingStore> backing = env()->release_managed_buffer(buf);
  if (UNLIKELY(!backing))
    return Destroy(CloseContext::RECEIVE_FAILURE, UV_ENOMEM);

  Store store(backing, buf.len, 0);

  ngtcp2_vec vec = store;
  ngtcp2_version_cid pversion_cid;

  // This is our first check to see if the received data can be processed as a
  // QUIC packet. If this fails, then the QUIC packet header is invalid and
  // cannot be processed; all we can do is ignore it. If it succeeds, we have a
  // valid QUIC header but there is still no guarantee that the packet can be
  // successfully processed.
  if (ngtcp2_pkt_decode_version_cid(
          &pversion_cid, vec.base, vec.len, NGTCP2_MAX_CIDLEN) < 0) {
    return;  // Ignore the packet!
  }

  // QUIC currently requires CID lengths of max NGTCP2_MAX_CIDLEN. The ngtcp2
  // API allows non-standard lengths, and we may want to allow non-standard
  // lengths later. But for now, we're going to ignore any packet with a
  // non-standard CID length.
  if (pversion_cid.dcidlen > NGTCP2_MAX_CIDLEN ||
      pversion_cid.scidlen > NGTCP2_MAX_CIDLEN)
    return;  // Ignore the packet!

  // Each QUIC peer has two CIDs: The Source Connection ID (or scid), and the
  // Destination Connection ID (or dcid). For each peer, the dcid is the CID
  // identifying the other peer, and the scid is the CID identifying itself.
  // That is, the client's scid is the server dcid; likewise the server's scid
  // is the client's dcid.
  //
  // The dcid and scid below are the values sent from the peer received in the
  // current packet, so in this case, dcid represents who the peer sent the
  // packet too (this endpoint) and the scid represents who sent the packet.
  CID dcid(pversion_cid.dcid, pversion_cid.dcidlen);
  CID scid(pversion_cid.scid, pversion_cid.scidlen);

  // We index the current sessions by the dcid of the client. For initial
  // packets, the dcid is some random value and the scid is omitted from the
  // header (it uses what quic calls a "short header"). It is unlikely (but not
  // impossible) that this randomly selected dcid will be in our index. If we do
  // happen to have a collision, as unlikely as it is, ngtcp2 will do the right
  // thing when it tries to process the packet so we really don't have to worry
  // about it here. If the dcid is not known, the listener here will be nullptr.
  //
  // When the session is established, this peer will create it's own scid and
  // will send that back to the remote peer to use as it's new dcid on
  // subsequent packets. When that session is added, we will index it by the
  // local scid, so as long as the client sends the subsequent packets with the
  // right dcid, everything will just work.

  auto session = FindSession(dcid);
  auto addr = local_address();

  HandleScope handle_scope(env()->isolate());

  // If a session is not found, there are four possible reasons:
  // 1. The session has not been created yet
  // 2. The session existed once but we've lost the local state for it
  // 3. The packet is a stateless reset sent by the peer
  // 4. This is a malicious or malformed packet.
  if (!session) {
    // No existing session.

    // Handle possible reception of a stateless reset token... If it is a
    // stateless reset, the packet will be handled with no additional action
    // necessary here. We want to return immediately without committing any
    // further resources.
    if (!scid && maybeStatelessReset(dcid, scid, store, addr, remote_address))
      return;  // Stateless reset! Don't do any further processing.

    if (acceptInitialPacket(pversion_cid.version,
                            dcid,
                            scid,
                            std::move(store),
                            addr,
                            remote_address)) {
      // Packet was successfully received.
      STAT_INCREMENT(Stats, packets_received);
    }
    return;
  }

  // If we got here, the dcid matched the scid of a known local session. Yay!
  if (receive(std::move(store), addr, remote_address, dcid, scid))
    STAT_INCREMENT(Stats, packets_received);
}

void Endpoint::PacketDone(int status) {
  if (is_closed()) return;
  state_->pending_callbacks--;
  // Can we go ahead and close now?
  if (state_->closing == 1) {
    // MaybeDestroy potentially creates v8 handles so let's make sure
    // we have a HandleScope on the stack.
    HandleScope scope(env()->isolate());
    MaybeDestroy();
  }
}

void Endpoint::IncrementSocketAddressCounter(const SocketAddress& addr) {
  addrLRU_.Upsert(addr)->active_connections++;
}

void Endpoint::DecrementSocketAddressCounter(const SocketAddress& addr) {
  auto* counts = addrLRU_.Peek(addr);
  if (counts != nullptr && counts->active_connections > 0)
    counts->active_connections--;
}

bool Endpoint::is_closed() const {
  return !udp_;
}
bool Endpoint::is_closing() const {
  return state_->closing;
}
bool Endpoint::is_listening() const {
  return state_->listening;
}

void Endpoint::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("options", options_);
  tracker->TrackField("udp", udp_);
  if (server_options_.has_value()) {
    tracker->TrackField("server_options", server_options_.value());
  }
  tracker->TrackField("token_map", token_map_);
  tracker->TrackField("sessions", sessions_);
  tracker->TrackField("cid_map", dcid_to_scid_);
  tracker->TrackField("address LRU", addrLRU_);
}

// ======================================================================================
// Endpoint::SocketAddressInfoTraits

bool Endpoint::SocketAddressInfoTraits::CheckExpired(
    const SocketAddress& address, const Type& type) {
  return (uv_hrtime() - type.timestamp) > kSocketAddressInfoTimeout;
}

void Endpoint::SocketAddressInfoTraits::Touch(const SocketAddress& address,
                                              Type* type) {
  type->timestamp = uv_hrtime();
}

// ======================================================================================
// JavaScript call outs

void Endpoint::EmitNewSession(const BaseObjectPtr<Session>& session) {
  if (!env()->can_call_into_js()) return;
  CallbackScope<Endpoint> scope(this);
  session->set_wrapped();
  Local<Value> arg = session->object();

  MakeCallback(BindingData::Get(env()).session_new_callback(), 1, &arg);
}

void Endpoint::EmitClose(CloseContext context, int status) {
  if (!env()->can_call_into_js()) return;
  CallbackScope<Endpoint> scope(this);
  auto isolate = env()->isolate();
  Local<Value> argv[] = {Integer::New(isolate, static_cast<int>(context)),
                         Integer::New(isolate, static_cast<int>(status))};

  MakeCallback(
      BindingData::Get(env()).endpoint_close_callback(), arraysize(argv), argv);
}

// ======================================================================================
// Endpoint JavaScript API

void Endpoint::New(const FunctionCallbackInfo<Value>& args) {
  DCHECK(args.IsConstructCall());
  auto env = Environment::GetCurrent(args);
  Options options;
  // Options::From will validate that args[0] is the correct type.
  if (!Options::From(env, args[0]).To(&options)) {
    // There was an error. Just exit to propagate.
    return;
  }

  new Endpoint(env, args.This(), options);
}

void Endpoint::DoConnect(const FunctionCallbackInfo<Value>& args) {
  auto env = Environment::GetCurrent(args);
  Endpoint* endpoint;
  ASSIGN_OR_RETURN_UNWRAP(&endpoint, args.Holder());

  // args[0] is a SocketAddress
  // args[1] is a Session OptionsObject (see session.cc)
  // args[2] is an optional SessionTicket

  DCHECK(SocketAddressBase::HasInstance(env, args[0]));
  SocketAddressBase* address;
  ASSIGN_OR_RETURN_UNWRAP(&address, args[0]);

  DCHECK(args[1]->IsObject());
  Session::Options options;
  if (!Session::Options::From(env, args[1]).To(&options)) {
    // There was an error. Return to propagate
    return;
  }

  BaseObjectPtr<Session> session;

  if (!args[2]->IsUndefined()) {
    SessionTicket ticket;
    if (SessionTicket::FromV8Value(env, args[2]).To(&ticket)) {
      session = endpoint->Connect(*address->address(), options, ticket);
    }
  } else {
    session = endpoint->Connect(*address->address(), options);
  }

  if (session) args.GetReturnValue().Set(session->object());
}

void Endpoint::DoListen(const FunctionCallbackInfo<Value>& args) {
  Endpoint* endpoint;
  ASSIGN_OR_RETURN_UNWRAP(&endpoint, args.Holder());
  auto env = Environment::GetCurrent(args);

  Session::Options options;
  if (Session::Options::From(env, args[0]).To(&options)) {
    endpoint->Listen(options);
  }
}

void Endpoint::MarkBusy(const FunctionCallbackInfo<Value>& args) {
  Endpoint* endpoint;
  ASSIGN_OR_RETURN_UNWRAP(&endpoint, args.Holder());
  endpoint->MarkAsBusy(args[0]->IsTrue());
}

void Endpoint::DoCloseGracefully(const FunctionCallbackInfo<Value>& args) {
  Endpoint* endpoint;
  ASSIGN_OR_RETURN_UNWRAP(&endpoint, args.Holder());
  endpoint->CloseGracefully();
}

void Endpoint::LocalAddress(const FunctionCallbackInfo<Value>& args) {
  auto env = Environment::GetCurrent(args);
  Endpoint* endpoint;
  ASSIGN_OR_RETURN_UNWRAP(&endpoint, args.Holder());
  if (endpoint->is_closed() || !endpoint->udp_.is_bound()) return;
  auto addr = SocketAddressBase::Create(
      env, std::make_shared<SocketAddress>(endpoint->local_address()));
  if (addr) args.GetReturnValue().Set(addr->object());
}

void Endpoint::Ref(const FunctionCallbackInfo<Value>& args) {
  Endpoint* endpoint;
  ASSIGN_OR_RETURN_UNWRAP(&endpoint, args.Holder());
  auto env = Environment::GetCurrent(args);
  if (args[0]->BooleanValue(env->isolate())) {
    endpoint->udp_.Ref();
  } else {
    endpoint->udp_.Unref();
  }
}

}  // namespace quic
}  // namespace node

#endif  // HAVE_OPENSSL && NODE_OPENSSL_HAS_QUIC
