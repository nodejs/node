#if HAVE_OPENSSL && HAVE_QUIC
#include "guard.h"
#ifndef OPENSSL_NO_QUIC
#include <aliased_struct-inl.h>
#include <async_wrap-inl.h>
#include <debug_utils-inl.h>
#include <env-inl.h>
#include <memory_tracker-inl.h>
#include <ngtcp2/ngtcp2.h>
#include <node_errors.h>
#include <node_external_reference.h>
#include <node_process-inl.h>
#include <node_sockaddr-inl.h>
#include <permission/permission.h>
#include <timer_wrap-inl.h>
#include <util-inl.h>
#include <uv.h>
#include <v8.h>
#include <limits>
#include "application.h"
#include "bindingdata.h"
#include "defs.h"
#include "endpoint.h"
#include "http3.h"
#include "ncrypto.h"
#include "session_manager.h"

namespace node {

using v8::Array;
using v8::ArrayBufferView;
using v8::HandleScope;
using v8::Integer;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::Nothing;
using v8::Number;
using v8::Object;
using v8::ObjectTemplate;
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
  /* Max concurrent connections per IP (0 = unlimited) */                      \
  V(MAX_CONNECTIONS_PER_HOST, max_connections_per_host, uint16_t)              \
  /* Max total concurrent connections (0 = unlimited) */                       \
  V(MAX_CONNECTIONS_TOTAL, max_connections_total, uint16_t)                    \
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
  V(RETRY_RATE_LIMITED, retry_rate_limited)                                    \
  V(VERSION_NEGOTIATION_COUNT, version_negotiation_count)                      \
  V(VERSION_NEGOTIATION_RATE_LIMITED, version_negotiation_rate_limited)        \
  V(STATELESS_RESET_COUNT, stateless_reset_count)                              \
  V(STATELESS_RESET_RATE_LIMITED, stateless_reset_rate_limited)                \
  V(IMMEDIATE_CLOSE_COUNT, immediate_close_count)                              \
  V(IMMEDIATE_CLOSE_RATE_LIMITED, immediate_close_rate_limited)                \
  V(SESSION_CREATION_RATE_LIMITED, session_creation_rate_limited)              \
  V(PACKETS_BLOCKED, packets_blocked)

struct Endpoint::State {
#define V(_, name, type) type name;
  ENDPOINT_STATE(V)
#undef V
};

STAT_STRUCT(Endpoint, ENDPOINT)

TokenBucket::TokenBucket(double rate, double burst)
    : rate(rate), burst(burst), tokens(burst), last_ts(uv_hrtime()) {}

void TokenBucket::InitOnce(double r, double b, uint64_t now) {
  if (last_ts == 0) {
    rate = r;
    burst = b;
    tokens = b;
    last_ts = now;
  }
}

// Try to consume one token. Refills based on elapsed time, then
// attempts to consume. Returns true if the request is allowed.
bool TokenBucket::consume(uint64_t now) {
  double elapsed = static_cast<double>(now - last_ts) / 1e9;  // seconds
  last_ts = now;
  tokens = std::min(burst, tokens + elapsed * rate);
  if (tokens >= 1.0) {
    tokens -= 1.0;
    return true;
  }
  return false;
}

// ============================================================================
// Endpoint::Options
namespace {
#ifdef DEBUG
bool is_diagnostic_packet_loss(double probability) {
  if (probability == 0.0) [[unlikely]] {
    return false;
  }
  unsigned char c = 255;
  CHECK(ncrypto::CSPRNG(&c, 1));
  return (static_cast<double>(c) / 255) < probability;
}
#endif  // DEBUG

template <typename Opt, double Opt::*member>
bool SetOption(Environment* env,
               Opt* options,
               const Local<Object>& object,
               const Local<String>& name) {
  Local<Value> value;
  if (!object->Get(env->context(), name).ToLocal(&value)) return false;
  if (!value->IsUndefined()) {
    if (!value->IsNumber()) {
      Utf8Value nameStr(env->isolate(), name);
      THROW_ERR_INVALID_ARG_VALUE(
          env, "The %s option must be a number", nameStr);
      return false;
    }
    Local<Number> num = value.As<Number>();
    double dbl = num->Value();
    if (dbl < 0) {
      Utf8Value nameStr(env->isolate(), name);
      THROW_ERR_INVALID_ARG_VALUE(
          env, "The %s option must be a non-negative number", nameStr);
      return false;
    }
    options->*member = dbl;
  }
  return true;
}

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
          env, "The %s option must be an uint8", nameStr);
      return false;
    }
    Local<Uint32> num = value.As<Uint32>();
    uint32_t val = num->Value();
    if (val > std::numeric_limits<uint8_t>::max()) {
      Utf8Value nameStr(env->isolate(), name);
      THROW_ERR_INVALID_ARG_VALUE(
          env, "The %s option must be an uint8", nameStr);
      return false;
    }
    options->*member = val;
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
          env, "The %s option must be an ArrayBufferView", nameStr);
      return false;
    }
    Store store = Store::CopyFrom(value.As<ArrayBufferView>());
    if (store.length() != TokenSecret::QUIC_TOKENSECRET_LEN) {
      Utf8Value nameStr(env->isolate(), name);
      THROW_ERR_INVALID_ARG_VALUE(
          env,
          "The %s option must be an ArrayBufferView of length %d",
          nameStr,
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
    if (value->IsUndefined()) return Just(Options());
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
      !SET(address_lru_size) || !SET(validate_address) ||
      !SET(disable_stateless_reset) || !SET(ipv6_only) || !SET(reuse_port) ||
      !SET(retry_rate) || !SET(retry_burst) || !SET(stateless_reset_rate) ||
      !SET(stateless_reset_burst) || !SET(version_negotiation_rate) ||
      !SET(version_negotiation_burst) || !SET(immediate_close_rate) ||
      !SET(immediate_close_burst) || !SET(session_creation_rate) ||
      !SET(session_creation_burst) ||
#ifdef DEBUG
      !SET(rx_loss) || !SET(tx_loss) ||
#endif
      !SET(udp_receive_buffer_size) || !SET(udp_send_buffer_size) ||
      !SET(udp_ttl) || !SET(idle_timeout) || !SET(reset_token_secret) ||
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
    auto addr = FromJSObject<SocketAddressBase>(address.As<Object>());
    options.local_address = addr->address();
  } else {
    options.local_address = std::make_shared<SocketAddress>();
    if (!SocketAddress::New("127.0.0.1", 0, options.local_address.get())) {
      THROW_ERR_INVALID_ADDRESS(env);
      return Nothing<Options>();
    }
  }

  // Parse block list option. Expects the C++ SocketAddressBlockListWrap handle
  // (the JS side extracts [kHandle] before passing it through).
  Local<Value> block_list_val;
  if (!params->Get(env->context(), state.block_list_string())
           .ToLocal(&block_list_val)) {
    return Nothing<Options>();
  }
  if (!block_list_val->IsUndefined()) {
    if (!SocketAddressBlockListWrap::HasInstance(env, block_list_val)) {
      THROW_ERR_INVALID_ARG_TYPE(
          env, "The blockList option must be a BlockList handle");
      return Nothing<Options>();
    }
    auto* wrap =
        FromJSObject<SocketAddressBlockListWrap>(block_list_val.As<Object>());
    options.block_list = wrap->blocklist();
  }

  // Parse block list policy.
  Local<Value> policy_val;
  if (!params->Get(env->context(), state.block_list_policy_string())
           .ToLocal(&policy_val)) {
    return Nothing<Options>();
  }
  if (!policy_val->IsUndefined()) {
    if (policy_val->StrictEquals(state.allow_string())) {
      options.block_list_policy = Options::BlockListPolicy::ALLOW;
    } else if (policy_val->StrictEquals(state.deny_string())) {
      options.block_list_policy = Options::BlockListPolicy::DENY;
    } else {
      THROW_ERR_INVALID_ARG_VALUE(
          env, "The blockListPolicy option must be 'deny' or 'allow'");
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

std::string Endpoint::Options::ToString() const {
  DebugIndentScope indent;
  auto prefix = indent.Prefix();
  auto boolToString = [](uint8_t val) {
    return val ? std::string("yes") : std::string("no");
  };

  std::string res = "{ ";
  res += prefix + "local address: " + local_address->ToString();
  res += prefix +
         "retry token expiration: " + std::to_string(retry_token_expiration) +
         " seconds";
  res += prefix + "token expiration: " + std::to_string(token_expiration) +
         " seconds";
  res += prefix + "address lru size: " + std::to_string(address_lru_size);
  res += prefix + "retry rate: " + std::to_string(retry_rate) + "/s";
  res += prefix + "retry burst: " + std::to_string(retry_burst);
  res += prefix +
         "stateless reset rate: " + std::to_string(stateless_reset_rate) + "/s";
  res += prefix +
         "stateless reset burst: " + std::to_string(stateless_reset_burst);
  res += prefix + "version negotiation rate: " +
         std::to_string(version_negotiation_rate) + "/s";
  res += prefix + "version negotiation burst: " +
         std::to_string(version_negotiation_burst);
  res += prefix +
         "immediate close rate: " + std::to_string(immediate_close_rate) + "/s";
  res += prefix +
         "immediate close burst: " + std::to_string(immediate_close_burst);
  res += prefix +
         "session creation rate: " + std::to_string(session_creation_rate) +
         "/s";
  res += prefix +
         "session creation burst: " + std::to_string(session_creation_burst);
  res += prefix + "validate address: " + boolToString(validate_address);
  res += prefix +
         "disable stateless reset: " + boolToString(disable_stateless_reset);
#ifdef DEBUG
  res += prefix + "rx loss: " + std::to_string(rx_loss);
  res += prefix + "tx loss: " + std::to_string(tx_loss);
#endif
  res += prefix + "reset token secret: " + reset_token_secret.ToString();
  res += prefix + "token secret: " + token_secret.ToString();
  res += prefix + "ipv6 only: " + boolToString(ipv6_only);
  res += prefix + "reuse port: " + boolToString(reuse_port);
  res += prefix +
         "udp receive buffer size: " + std::to_string(udp_receive_buffer_size);
  res +=
      prefix + "udp send buffer size: " + std::to_string(udp_send_buffer_size);
  res += prefix + "udp ttl: " + std::to_string(udp_ttl);
  res += prefix + "idle timeout: " + std::to_string(idle_timeout) + " seconds";

  res += indent.Close();
  return res;
}

// ======================================================================================
// Endpoint::UDP and Endpoint::UDP::Impl

class Endpoint::UDP::Impl final : public HandleWrap {
 public:
  JS_CONSTRUCTOR(Impl);

  static Impl* Create(Endpoint* endpoint) {
    JS_NEW_INSTANCE_OR_RETURN(endpoint->env(), obj, nullptr);
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
                   PROVIDER_QUIC_UDP),
        endpoint_(endpoint) {
    CHECK_EQ(uv_udp_init_ex(endpoint->env()->event_loop(),
                            &handle_,
                            AF_UNSPEC | UV_UDP_RECVMMSG),
             0);
    handle_.data = this;
  }

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(Endpoint::UDP::Impl)
  SET_SELF_SIZE(Impl)

 private:
  // Pre-allocated receive buffer sized for recvmmsg batching. libuv's
  // recvmmsg path partitions the alloc buffer into 64KB chunks (one per
  // datagram). With kRecvBatchSize chunks we can receive up to that many
  // packets in a single recvmmsg syscall. ngtcp2_conn_read_pkt is
  // synchronous — it copies what it needs — so the buffer is safely
  // reused across batches.
  // libuv's recvmmsg partitions the buffer into UV__UDP_DGRAM_MAXSIZE (64KB)
  // chunks regardless of actual packet size. QUIC packets are ~1200 bytes,
  // so most of each 64KB chunk is wasted. We use a modest batch size to
  // balance syscall reduction against memory usage.
  static constexpr size_t kDgramMaxSize = 65536;  // UV__UDP_DGRAM_MAXSIZE
  static constexpr size_t kRecvBatchSize = 5;
  static constexpr size_t kRecvBufferSize = kDgramMaxSize * kRecvBatchSize;
  std::array<char, kRecvBufferSize> recv_buf_ = {};

  static void OnAlloc(uv_handle_t* handle,
                      size_t suggested_size,
                      uv_buf_t* buf) {
    auto* impl = From(handle);
    *buf = uv_buf_init(impl->recv_buf_.data(), kRecvBufferSize);
  }

  static void OnReceive(uv_udp_t* handle,
                        ssize_t nread,
                        const uv_buf_t* buf,
                        const sockaddr* addr,
                        unsigned int flags) {
    auto impl = From(handle);
    DCHECK_NOT_NULL(impl);
    DCHECK_NOT_NULL(impl->endpoint_);

    // UV_UDP_MMSG_FREE signals the end of a recvmmsg batch — the
    // buffer can be reused. Since our buffer is pre-allocated and
    // persistent, there is nothing to free.
    if (flags & UV_UDP_MMSG_FREE) {
      return;
    }

    // Nothing to do in these cases. Specifically, if the nread
    // is zero or we have received a partial packet, we are just
    // going to ignore it. No buffer release needed — recv_buf_
    // is pre-allocated and reused.
    if (nread == 0 || flags & UV_UDP_PARTIAL) {
      return;
    }

    if (nread < 0) {
      impl->endpoint_->Destroy(CloseContext::RECEIVE_FAILURE,
                               static_cast<int>(nread));
      return;
    }

    // UV_UDP_MMSG_CHUNK is set for each packet in a recvmmsg batch.
    // Processing is the same as for a single-message receive — ngtcp2
    // copies what it needs synchronously from the buf slice.
    impl->endpoint_->Receive(reinterpret_cast<const uint8_t*>(buf->base),
                             static_cast<size_t>(nread),
                             SocketAddress(addr));
  }

  uv_udp_t handle_;
  Endpoint* endpoint_;

  friend class UDP;
};

JS_CONSTRUCTOR_IMPL(Endpoint::UDP::Impl, udp_constructor_template, {
  JS_ILLEGAL_CONSTRUCTOR();
  JS_INHERIT(HandleWrap);
  JS_CLASS(endpoint_udp);
})

Endpoint::UDP::UDP(Endpoint* endpoint) : impl_(Impl::Create(endpoint)) {
  DCHECK(impl_);
  // The endpoint starts in an inactive, unref'd state. It will be ref'd when
  // the endpoint is either configured to listen as a server or when then are
  // active client sessions.
  Unref();
}

Endpoint::UDP::~UDP() {
  Close();
}

int Endpoint::UDP::Bind(const Options& options) {
  if (flags_.is_bound) return UV_EALREADY;
  if (is_closed_or_closing()) return UV_EBADF;

  int flags = 0;
  if (options.local_address->family() == AF_INET6 && options.ipv6_only)
    flags |= UV_UDP_IPV6ONLY;
  if (options.reuse_port) flags |= UV_UDP_REUSEPORT;
  int err = uv_udp_bind(&impl_->handle_, options.local_address->data(), flags);
  int size;

  if (!err) {
    flags_.is_bound = true;
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
  if (flags_.is_started) return 0;
  int err = uv_udp_recv_start(&impl_->handle_, Impl::OnAlloc, Impl::OnReceive);
  flags_.is_started = (err == 0);
  return err;
}

void Endpoint::UDP::Stop() {
  if (is_closed_or_closing() || !flags_.is_started) return;
  USE(uv_udp_recv_stop(&impl_->handle_));
  flags_.is_started = false;
}

void Endpoint::UDP::Close() {
  if (is_closed_or_closing()) return;
  DCHECK(impl_);
  Stop();
  flags_.is_bound = false;
  flags_.is_closed = true;
  impl_->Close();
  impl_.reset();
}

bool Endpoint::UDP::is_bound() const {
  return flags_.is_bound;
}

bool Endpoint::UDP::is_closed() const {
  return flags_.is_closed;
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

int Endpoint::UDP::Send(Packet::Ptr packet) {
  DCHECK(packet);
  if (is_closed_or_closing()) return UV_EBADF;

  // Detach from the Ptr — libuv takes ownership until the callback fires.
  Packet* raw = packet.release();
  uv_buf_t buf = *raw;
  int err = uv_udp_send(raw->req(),
                        &impl_->handle_,
                        &buf,
                        1,
                        raw->destination().data(),
                        [](uv_udp_send_t* req, int status) {
                          Packet* p = Packet::FromReq(req);
                          p->listener()->PacketDone(status);
                          ArenaPool<Packet>::Release(p);
                        });
  if (err < 0) {
    // Send failed — release the packet back to the pool immediately.
    ArenaPool<Packet>::Release(raw);
  }
  return err;
}

int Endpoint::UDP::TrySend(const Packet::Ptr& packet) {
  DCHECK(packet);
  if (is_closed_or_closing()) return UV_EBADF;
  uv_buf_t buf = *packet;
  return uv_udp_try_send(
      &impl_->handle_, &buf, 1, packet->destination().data());
}

int Endpoint::UDP::TrySendBatch(uv_buf_t* bufs[],
                                unsigned int nbufs[],
                                struct sockaddr* addrs[],
                                size_t count) {
  DCHECK_GT(count, 0);
  if (is_closed_or_closing()) return UV_EBADF;
  return uv_udp_try_send2(
      &impl_->handle_, static_cast<unsigned int>(count), bufs, nbufs, addrs, 0);
}

void Endpoint::UDP::MemoryInfo(MemoryTracker* tracker) const {
  if (impl_) tracker->TrackField("impl", impl_);
}

// ============================================================================

JS_CONSTRUCTOR_IMPL(Endpoint, endpoint_constructor_template, {
  auto isolate = env->isolate();
  JS_NEW_CONSTRUCTOR();
  JS_INHERIT(AsyncWrap);
  JS_CLASS(endpoint);
  SetProtoMethod(isolate, tmpl, "listen", DoListen);
  SetProtoMethod(isolate, tmpl, "closeGracefully", DoCloseGracefully);
  SetProtoMethod(isolate, tmpl, "connect", DoConnect);
  SetProtoMethod(isolate, tmpl, "markBusy", MarkBusy);
  SetProtoMethod(isolate, tmpl, "ref", Ref);
  SetProtoMethod(isolate, tmpl, "setSNIContexts", DoSetSNIContexts);
  SetProtoMethodNoSideEffect(isolate, tmpl, "address", LocalAddress);
})

void Endpoint::InitPerIsolate(IsolateData* data, Local<ObjectTemplate> target) {
  // TODO(@jasnell): Implement the per-isolate state
}

void Endpoint::InitPerContext(Realm* realm, Local<Object> target) {
#define V(name, _) IDX_STATS_ENDPOINT_##name,
  enum IDX_STATS_ENDPOINT { ENDPOINT_STATS(V) IDX_STATS_ENDPOINT_COUNT };
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

  NODE_DEFINE_CONSTANT(target, DEFAULT_MAX_SOCKETADDRESS_LRU_SIZE);

  static constexpr auto DEFAULT_RETRYTOKEN_EXPIRATION =
      RetryToken::QUIC_DEFAULT_RETRYTOKEN_EXPIRATION / NGTCP2_SECONDS;
  static constexpr auto DEFAULT_REGULARTOKEN_EXPIRATION =
      RegularToken::QUIC_DEFAULT_REGULARTOKEN_EXPIRATION / NGTCP2_SECONDS;
  static constexpr auto DEFAULT_MAX_PACKET_LENGTH = kDefaultMaxPacketLength;
  NODE_DEFINE_CONSTANT(target, DEFAULT_RETRYTOKEN_EXPIRATION);
  NODE_DEFINE_CONSTANT(target, DEFAULT_REGULARTOKEN_EXPIRATION);
  NODE_DEFINE_CONSTANT(target, DEFAULT_MAX_PACKET_LENGTH);

  static constexpr auto CLOSECONTEXT_CLOSE =
      static_cast<uint8_t>(CloseContext::CLOSE);
  static constexpr auto CLOSECONTEXT_BIND_FAILURE =
      static_cast<uint8_t>(CloseContext::BIND_FAILURE);
  static constexpr auto CLOSECONTEXT_LISTEN_FAILURE =
      static_cast<uint8_t>(CloseContext::LISTEN_FAILURE);
  static constexpr auto CLOSECONTEXT_RECEIVE_FAILURE =
      static_cast<uint8_t>(CloseContext::RECEIVE_FAILURE);
  static constexpr auto CLOSECONTEXT_SEND_FAILURE =
      static_cast<uint8_t>(CloseContext::SEND_FAILURE);
  static constexpr auto CLOSECONTEXT_START_FAILURE =
      static_cast<uint8_t>(CloseContext::START_FAILURE);
  NODE_DEFINE_CONSTANT(target, CLOSECONTEXT_CLOSE);
  NODE_DEFINE_CONSTANT(target, CLOSECONTEXT_BIND_FAILURE);
  NODE_DEFINE_CONSTANT(target, CLOSECONTEXT_LISTEN_FAILURE);
  NODE_DEFINE_CONSTANT(target, CLOSECONTEXT_RECEIVE_FAILURE);
  NODE_DEFINE_CONSTANT(target, CLOSECONTEXT_SEND_FAILURE);
  NODE_DEFINE_CONSTANT(target, CLOSECONTEXT_START_FAILURE);

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
  registry->Register(DoSetSNIContexts);
  registry->Register(LocalAddress);
  registry->Register(Ref);
  registry->Register(MarkBusy);
}

Endpoint::Endpoint(Environment* env,
                   Local<Object> object,
                   const Options& options)
    : AsyncWrap(env, object, PROVIDER_QUIC_ENDPOINT),
      stats_(env->isolate()),
      state_(env->isolate()),
      options_(options),
      packet_pool_(kDefaultMaxPacketLength,
                   ArenaPool<Packet>::kDefaultSlotsPerBlock),
      udp_(this),
      idle_timer_(env,
                  [this] {
                    HandleScope scope(this->env()->isolate());
                    Destroy();
                  }),
      addr_validation_lru_(options_.address_lru_size),
      retry_bucket_(options_.retry_rate, options_.retry_burst),
      stateless_reset_bucket_(options_.stateless_reset_rate,
                              options_.stateless_reset_burst),
      version_negotiation_bucket_(options_.version_negotiation_rate,
                                  options_.version_negotiation_burst),
      immediate_close_bucket_(options_.immediate_close_rate,
                              options_.immediate_close_burst) {
  MakeWeak();
  udp_.Unref();
  idle_timer_.Unref();
  STAT_RECORD_TIMESTAMP(Stats, created_at);
  IF_QUIC_DEBUG(env) {
    Debug(this, "Endpoint created. Options %s", options.ToString());
  }

  JS_DEFINE_READONLY_PROPERTY(
      env, object, env->state_string(), state_.GetArrayBuffer());
  JS_DEFINE_READONLY_PROPERTY(
      env, object, env->stats_string(), stats_.GetArrayBuffer());
}

Packet::Ptr Endpoint::CreatePacket(const SocketAddress& destination,
                                   size_t length,
                                   const char* diagnostic_label) {
  auto ptr = packet_pool_.AcquireExtra(static_cast<Packet::Listener*>(this),
                                       destination);
  if (ptr) {
    ptr->Truncate(std::min(length, ptr->capacity()));
    ptr->set_diagnostic_label(diagnostic_label);
  }
  return ptr;
}

SocketAddress Endpoint::local_address() const {
  DCHECK(!is_closed() && !is_closing());
  return udp_.local_address();
}

void Endpoint::MarkAsBusy(bool on) {
  Debug(this, "Marking endpoint as %s", on ? "busy" : "not busy");
  if (on) STAT_INCREMENT(Stats, server_busy_count);
  state_->busy = on ? 1 : 0;
}

RegularToken Endpoint::GenerateNewToken(uint32_t version,
                                        const SocketAddress& remote_address) {
  Debug(this,
        "Generating new regular token for version %u and remote address %s",
        version,
        remote_address);
  DCHECK(!is_closed() && !is_closing());
  return RegularToken(version, remote_address, options_.token_secret);
}

SessionManager& Endpoint::session_manager() const {
  return BindingData::Get(env()).session_manager();
}

StatelessResetToken Endpoint::GenerateNewStatelessResetToken(
    uint8_t* token, const CID& cid) const {
  DCHECK(!is_closed() && !is_closing());
  return StatelessResetToken(token, options_.reset_token_secret, cid);
}

void Endpoint::AddSession(const CID& cid, BaseObjectPtr<Session> session) {
  DCHECK(!is_closed() && !is_closing());
  Debug(this, "Adding session for CID %s", cid);
  if (state_->max_connections_per_host > 0) {
    conn_counts_per_host_[session->remote_address()]++;
  }
  auto& mgr = session_manager();
  // Associate peer-chosen CIDs in the local dcid_to_scid_ map.
  AssociateCID(session->config().dcid, session->config().scid);
  mgr.AddSession(cid, session);
  mgr.SetPrimaryEndpoint(session.get(), this);
  // For server sessions, associate the client's original DCID (ocid) so
  // that 0-RTT packets arriving in a separate UDP datagram can be routed
  // to this session. This must happen after the session is added (so
  // FindSession can resolve the mapping) but before EmitNewSession (which
  // runs JS and may yield to libuv, allowing the 0-RTT packet to arrive).
  if (session->is_server() && session->config().ocid) {
    AssociateCID(session->config().ocid, session->config().scid);
  }
  // After Retry, the client continues to use the Retry SCID as its DCID
  // until the handshake completes. Register it so retransmitted Initials
  // and subsequent handshake packets can be routed to this session.
  if (session->is_server() && session->config().retry_scid) {
    AssociateCID(session->config().retry_scid, session->config().scid);
  }
  // Increment the primary session count and ref the handle BEFORE
  // EmitNewSession. EmitNewSession calls into JS, which may close/destroy
  // the session synchronously. The session's ~Impl calls RemoveSession
  // which decrements the count. If we increment after EmitNewSession,
  // RemoveSession would see count=0 and the count would be permanently
  // off by one.
  if (primary_session_count_++ == 0) {
    idle_timer_.Stop();
    udp_.Ref();
  }
  if (session->is_server()) {
    STAT_INCREMENT(Stats, server_sessions);
    // We only emit the new session event for server sessions.
    EmitNewSession(session);
    // It is important to note that the session may be closed/destroyed
    // when it is emitted here.
  } else {
    STAT_INCREMENT(Stats, client_sessions);
  }
}

void Endpoint::RemoveSession(const CID& cid,
                             const SocketAddress& remote_address) {
  if (is_closed()) return;
  Debug(this, "Removing session for CID %s", cid);
  auto it = conn_counts_per_host_.find(remote_address);
  if (it != conn_counts_per_host_.end()) {
    if (--it->second == 0) {
      conn_counts_per_host_.erase(it);
    }
  }
  if (primary_session_count_ > 0 && --primary_session_count_ == 0) {
    if (!is_listening()) {
      udp_.Unref();
    }
    session_manager().RemoveSession(cid);
    // The endpoint may be idle (no sessions, not listening). MaybeDestroy
    // handles both closing (immediate destroy) and idle timeout (start
    // timer or destroy based on idle_timeout setting).
    MaybeDestroy();
    return;
  }
  session_manager().RemoveSession(cid);
  if (state_->closing == 1) MaybeDestroy();
}

BaseObjectPtr<Session> Endpoint::FindSession(const CID& cid) {
  // First, try the SessionManager's primary sessions_ map directly.
  // This handles the common case where the CID is a locally-generated SCID.
  auto session = session_manager().FindSession(cid);
  if (session) return session;

  // If not found, check this endpoint's local dcid_to_scid_ map for a
  // secondary CID mapping. This map contains peer-chosen CID values that
  // are only meaningful in the context of this endpoint's sessions.
  auto scid_it = dcid_to_scid_.find(cid);
  if (scid_it != dcid_to_scid_.end()) {
    session = session_manager().FindSession(scid_it->second);
    if (session) return session;
    // Stale mapping — clean up.
    dcid_to_scid_.erase(scid_it);
  }

  return {};
}

void Endpoint::AssociateCID(const CID& cid, const CID& scid) {
  if (!is_closed() && !is_closing() && cid && scid && cid != scid) {
    auto it = dcid_to_scid_.find(cid);
    if (it == dcid_to_scid_.end() || it->second != scid) {
      Debug(this, "Associating CID %s with SCID %s", cid, scid);
      dcid_to_scid_[cid] = scid;
    }
  }
}

void Endpoint::DisassociateCID(const CID& cid) {
  if (!is_closed() && cid) {
    Debug(this, "Disassociating CID %s", cid);
    dcid_to_scid_.erase(cid);
  }
}

void Endpoint::AssociateStatelessResetToken(const StatelessResetToken& token,
                                            Session* session) {
  if (is_closed() || is_closing()) return;
  Debug(this, "Associating stateless reset token %s with session", token);
  session_manager().AssociateStatelessResetToken(token, session);
}

void Endpoint::DisassociateStatelessResetToken(
    const StatelessResetToken& token) {
  if (!is_closed()) {
    Debug(this, "Disassociating stateless reset token %s", token);
    session_manager().DisassociateStatelessResetToken(token);
  }
}

void Endpoint::Send(Packet::Ptr packet) {
#ifdef DEBUG
  // When diagnostic packet loss is enabled, the packet will be randomly
  // dropped. This can happen to any type of packet. We use this only in
  // testing to test various reliability issues.
  if (is_diagnostic_packet_loss(options_.tx_loss)) [[unlikely]] {
    // Simulating tx packet loss. Ptr destructor releases the packet.
    return;
  }
#endif  // DEBUG

  if (is_closed() || is_closing() || packet->length() == 0) {
    // Ptr destructor releases the packet back to the pool.
    return;
  }
  Debug(this, "Sending %s", packet->ToString());
  size_t packet_length = packet->length();

  state_->pending_callbacks++;
  env()->IncreaseWaitingRequestCounter();

  int err = udp_.Send(std::move(packet));
  if (err != 0) {
    Debug(this, "Sending packet failed with error %d", err);
    // The packet was already released in UDP::Send on error.
    state_->pending_callbacks--;
    env()->DecreaseWaitingRequestCounter();
    Destroy(CloseContext::SEND_FAILURE, err);
    return;
  }
  STAT_INCREMENT_N(Stats, bytes_sent, packet_length);
  STAT_INCREMENT(Stats, packets_sent);
}

void Endpoint::SendOrTrySend(Packet::Ptr packet) {
#ifdef DEBUG
  if (is_diagnostic_packet_loss(options_.tx_loss)) [[unlikely]] {
    return;
  }
#endif

  if (is_closed() || is_closing() || !packet || packet->length() == 0) {
    return;
  }

  Debug(this, "TrySend %s", packet->ToString());

  // Attempt synchronous send. On success (returns number of bytes sent),
  // the packet is delivered immediately — no callback overhead, no
  // waiting for the next poll cycle.
  int err = udp_.TrySend(packet);
  if (err >= 0) {
    // Synchronous send succeeded.
    STAT_INCREMENT_N(Stats, bytes_sent, packet->length());
    STAT_INCREMENT(Stats, packets_sent);
    // Ptr destructor releases back to arena pool.
    return;
  }

  if (err == UV_EAGAIN) {
    // Socket not writable or async sends are queued. Fall back to the
    // async path — the packet will be queued and flushed on the next
    // POLLOUT cycle.
    Debug(this, "TrySend got EAGAIN, falling back to async Send");
    return Send(std::move(packet));
  }

  // Other errors are fatal.
  Debug(this, "TrySend failed with error %d", err);
  Destroy(CloseContext::SEND_FAILURE, err);
}

void Endpoint::SendBatch(Packet::Ptr* packets, size_t count) {
  if (count == 0) return;

#ifdef DEBUG
  if (is_diagnostic_packet_loss(options_.tx_loss)) [[unlikely]] {
    for (size_t i = 0; i < count; i++) packets[i].reset();
    return;
  }
#endif

  if (is_closed() || is_closing()) {
    for (size_t i = 0; i < count; i++) packets[i].reset();
    return;
  }

  static constexpr size_t kMaxBatch = 64;
  DCHECK_LE(count, kMaxBatch);

  // Build libuv argument arrays directly from the Ptr array.
  // Packets with zero length are released and skipped.
  uv_buf_t bufs[kMaxBatch];
  uv_buf_t* buf_ptrs[kMaxBatch];
  unsigned int nbufs[kMaxBatch];
  struct sockaddr* addrs[kMaxBatch];
  // Map from valid-index back to the original packets[] index.
  size_t index_map[kMaxBatch];
  size_t valid_count = 0;

  for (size_t i = 0; i < count; i++) {
    if (!packets[i] || packets[i]->length() == 0) {
      packets[i].reset();
      continue;
    }
    bufs[valid_count] = *packets[i];
    buf_ptrs[valid_count] = &bufs[valid_count];
    nbufs[valid_count] = 1;
    addrs[valid_count] =
        const_cast<struct sockaddr*>(packets[i]->destination().data());
    index_map[valid_count] = i;
    valid_count++;
  }

  if (valid_count == 0) return;

  // Attempt synchronous batched send via sendmmsg.
  int sent = udp_.TrySendBatch(buf_ptrs, nbufs, addrs, valid_count);

  if (sent > 0) {
    // Packets [0, sent) were delivered synchronously.
    // Release them immediately — no async callback needed.
    for (size_t i = 0; i < static_cast<size_t>(sent); i++) {
      size_t idx = index_map[i];
      STAT_INCREMENT_N(Stats, bytes_sent, packets[idx]->length());
      STAT_INCREMENT(Stats, packets_sent);
      packets[idx].reset();
    }
  }

  // Any unsent packets (EAGAIN, partial send, or total failure) fall
  // back to async uv_udp_send.
  size_t start = (sent > 0) ? static_cast<size_t>(sent) : 0;
  for (size_t i = start; i < valid_count; i++) {
    size_t idx = index_map[i];
    Send(std::move(packets[idx]));
  }
}

void Endpoint::SendRetry(const PathDescriptor& options, uint64_t now) {
  Debug(this, "Sending retry on path %s", options);
  if (!retry_bucket_.consume(now)) {
    Debug(this, "Retry rate limit exceeded (global)");
    STAT_INCREMENT(Stats, retry_rate_limited);
    return;
  }

  auto packet =
      Packet::CreateRetryPacket(*this, options, options_.token_secret);
  if (packet) {
    STAT_INCREMENT(Stats, retry_count);
    Send(std::move(packet));
  }
}

void Endpoint::SendVersionNegotiation(const PathDescriptor& options,
                                      uint64_t now) {
  Debug(this, "Sending version negotiation on path %s", options);
  if (!version_negotiation_bucket_.consume(now)) {
    Debug(this, "Version negotiation rate limit exceeded (global)");
    STAT_INCREMENT(Stats, version_negotiation_rate_limited);
    return;
  }

  auto packet = Packet::CreateVersionNegotiationPacket(*this, options);
  if (packet) {
    STAT_INCREMENT(Stats, version_negotiation_count);
    Send(std::move(packet));
  }
}

bool Endpoint::SendStatelessReset(const PathDescriptor& options,
                                  size_t source_len,
                                  uint64_t now) {
  if (options_.disable_stateless_reset) [[unlikely]] {
    return false;
  }
  Debug(this,
        "Sending stateless reset on path %s with len %" PRIu64,
        options,
        source_len);

  if (!stateless_reset_bucket_.consume(now)) {
    Debug(this, "Stateless reset rate limit exceeded (global)");
    STAT_INCREMENT(Stats, stateless_reset_rate_limited);
    return false;
  }

  auto packet = Packet::CreateStatelessResetPacket(
      *this, options, options_.reset_token_secret, source_len);

  if (packet) {
    Debug(this, "Sending stateless reset packet (%zu bytes)", packet->length());
    STAT_INCREMENT(Stats, stateless_reset_count);
    Send(std::move(packet));
    return true;
  }
  Debug(this, "Failed to create stateless reset packet");
  return false;
}

void Endpoint::SendImmediateConnectionClose(const PathDescriptor& options,
                                            QuicError reason,
                                            uint64_t now) {
  Debug(this,
        "Sending immediate connection close on path %s with reason %s",
        options,
        reason);
  if (!immediate_close_bucket_.consume(now)) {
    Debug(this, "Immediate connection close rate limit exceeded (global)");
    STAT_INCREMENT(Stats, immediate_close_rate_limited);
    return;
  }

  auto packet =
      Packet::CreateImmediateConnectionClosePacket(*this, options, reason);
  if (packet) {
    STAT_INCREMENT(Stats, immediate_close_count);
    Send(std::move(packet));
  }
}

bool Endpoint::Start() {
  if (is_closed() || is_closing()) return false;

  // state_->receiving indicates that we're accepting inbound packets. It
  // could be for server or client side, or both.
  if (state_->receiving == 1) return true;
  Debug(this, "Starting");

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
  udp_.Ref();
  if (err != 0) {
    // If we failed to start listening, destroy the endpoint. There's nothing we
    // can do.
    Destroy(CloseContext::START_FAILURE, err);
    return false;
  }

  auto& binding = BindingData::Get(env());
  binding.listening_endpoints[this] = BaseObjectPtr<Endpoint>(this);
  binding.session_manager().RegisterEndpoint(this, udp_.local_address());
  state_->receiving = 1;
  return true;
}

void Endpoint::Listen(const Session::Options& options) {
  if (is_closed() || is_closing() || state_->listening == 1) return;
  DCHECK(!server_state_.has_value());

  // We need at least one key and one cert to complete the tls handshake on the
  // server. Why not make this an error? We could but it's not strictly
  // necessary.
  if (options.tls_options.keys.empty() || options.tls_options.certs.empty()) {
    env()->EmitProcessEnvWarning();
    ProcessEmitWarning(env(),
                       "The QUIC TLS options did not include a key or cert. "
                       "This means the TLS handshake will fail. This is likely "
                       "not what you want.");
  }

  auto context = TLSContext::CreateServer(env(), options.tls_options);
  if (!*context) {
    THROW_ERR_INVALID_STATE(
        env(), "Failed to create TLS context: %s", context->validation_error());
    return;
  }

  // Create additional TLS contexts for SNI entries (virtual hosts).
  for (const auto& [hostname, sni_options] : options.sni) {
    if (!context->AddSNIContext(env(), hostname, sni_options)) {
      THROW_ERR_INVALID_STATE(
          env(), "Failed to create TLS context for SNI host '%s'", hostname);
      return;
    }
  }

  server_state_ = {
      options,
      std::move(context),
  };
  if (Start()) {
    Debug(this, "Listening with options %s", server_state_->options);
    idle_timer_.Stop();
    state_->listening = 1;
  }
}

BaseObjectPtr<Session> Endpoint::Connect(
    const SocketAddress& remote_address,
    const Session::Options& options,
    std::optional<SessionTicket> session_ticket) {
  // If starting fails, the endpoint will be destroyed.
  if (!Start()) return {};

  Session::Config config(env(), options, local_address(), remote_address);

  Debug(this,
        "Connecting to %s with options %s and config %s [has 0rtt ticket? %s]",
        remote_address,
        options,
        config,
        session_ticket.has_value() ? "yes" : "no");

  auto tls_context = TLSContext::CreateClient(env(), options.tls_options);
  if (!*tls_context) {
    THROW_ERR_INVALID_STATE(env(),
                            "Failed to create TLS context: %s",
                            tls_context->validation_error());
    return {};
  }
  auto session =
      Session::Create(this, config, tls_context.get(), session_ticket);
  if (!session) {
    THROW_ERR_INVALID_STATE(env(), "Failed to create session");
    return {};
  }
  if (!session->tls_session()) {
    THROW_ERR_INVALID_STATE(env(),
                            "Failed to create TLS session: %s",
                            session->tls_session().validation_error());
    return {};
  }

  // Marking a session as "wrapped" means that the reference has been
  // (or will be) passed out to JavaScript.
  Session::SendPendingDataScope send_scope(session);
  session->set_wrapped();
  AddSession(config.scid, session);
  return session;
}

void Endpoint::MaybeDestroy() {
  if (is_closed() || primary_session_count_ > 0 ||
      state_->pending_callbacks > 0 || state_->listening == 1) {
    return;
  }
  if (options_.idle_timeout > 0) {
    // Start the idle timer. If it fires before a new session or listen
    // call reactivates this endpoint, the endpoint will be destroyed.
    idle_timer_.Update(options_.idle_timeout * 1000);
    return;
  }
  // With idle_timeout == 0, only destroy if the endpoint is actively
  // closing (via close() or CloseGracefully). An idle endpoint that
  // is not closing stays alive with an unref'd handle so the process
  // can still exit.
  if (state_->closing != 1) return;
  // Destroy potentially creates v8 handles so let's make sure
  // we have a HandleScope on the stack.
  HandleScope scope(env()->isolate());
  Destroy();
}

void Endpoint::Destroy(CloseContext context, int status) {
  if (is_closed()) return;

  IF_QUIC_DEBUG(env()) {
    auto ctx = ([&] {
      switch (context) {
        case CloseContext::BIND_FAILURE:
          return "bind failure";
        case CloseContext::CLOSE:
          return "close";
        case CloseContext::LISTEN_FAILURE:
          return "listen failure";
        case CloseContext::RECEIVE_FAILURE:
          return "receive failure";
        case CloseContext::SEND_FAILURE:
          return "send failure";
        case CloseContext::START_FAILURE:
          return "start failure";
      }
      return "<unknown>";
    })();
    Debug(
        this, "Destroying endpoint due to \"%s\" with status %d", ctx, status);
  }

  state_->listening = 0;

  close_context_ = context;
  close_status_ = status;

  // If there are open sessions still, shut them down. As those clean themselves
  // up, they will remove themselves. The cleanup here will be synchronous and
  // no attempt will be made to communicate further with the peer.
  idle_timer_.Close();
  session_manager().CloseAllSessionsFor(this);
  DCHECK_EQ(primary_session_count_, 0);
  dcid_to_scid_.clear();
  server_state_.reset();

  udp_.Close();
  state_->closing = 0;
  state_->bound = 0;
  state_->receiving = 0;
  auto& binding = BindingData::Get(env());
  binding.listening_endpoints.erase(this);
  binding.session_manager().UnregisterEndpoint(this);
  STAT_RECORD_TIMESTAMP(Stats, destroyed_at);

  EmitClose(close_context_, close_status_);
}

void Endpoint::CloseGracefully() {
  if (is_closed() || is_closing()) return;

  Debug(this, "Closing gracefully");
  state_->listening = 0;
  state_->closing = 1;

  // Maybe we can go ahead and destroy now?
  MaybeDestroy();
}

void Endpoint::Receive(const uint8_t* data,
                       size_t len,
                       const SocketAddress& remote_address) {
  const uint64_t now = uv_hrtime();

  // Block list filtering — applied before any packet processing to
  // minimize resource expenditure on blocked sources.
  if (options_.block_list) {
    bool matched = options_.block_list->Apply(remote_address);
    bool drop = (options_.block_list_policy == Options::BlockListPolicy::DENY)
                    ? matched    // deny list: drop if address matches
                    : !matched;  // allow list: drop if address doesn't match
    if (drop) {
      Debug(this, "Packet from %s blocked by block list", remote_address);
      STAT_INCREMENT(Stats, packets_blocked);
      return;
    }
  }

  const auto receive = [&](Session* session,
                           const uint8_t* pkt_data,
                           size_t pkt_len,
                           const SocketAddress& local_address,
                           const SocketAddress& remote_address,
                           const CID& dcid,
                           const CID& scid) {
    DCHECK_NOT_NULL(session);
    if (session->is_destroyed()) return;
    // Use ReadPacket (no SendPendingDataScope) so that multiple packets
    // received in the same I/O burst are processed before any responses
    // are generated. The deferred flush via BindingData's uv_check
    // callback calls SendPendingData once per dirty session after all
    // packets in the burst have been read.
    if (session->ReadPacket(pkt_data,
                            pkt_len,
                            local_address,
                            remote_address,
                            PacketInfo(),
                            now)) {
      STAT_INCREMENT_N(Stats, bytes_received, pkt_len);
      STAT_INCREMENT(Stats, packets_received);
    }
    // Schedule the session for deferred SendPendingData if it hasn't
    // been scheduled already in this burst.
    if (!session->is_destroyed() && !session->flags_.pending_flush) {
      session->flags_.pending_flush = true;
      BindingData::Get(env()).ScheduleSessionFlush(
          BaseObjectPtr<Session>(session));
    }
  };

  const auto accept = [&](const Session::Config& config,
                          const uint8_t* pkt_data,
                          size_t pkt_len) {
    // One final check. If the endpoint is closed, closing, or is not listening
    // as a server, then we cannot accept the initial packet.
    if (is_closed() || is_closing() || !is_listening()) return;

    // Per-host session creation rate limit. The bucket is initialized
    // on first access with the configured rate/burst from options.
    auto info = addr_validation_lru_.Upsert(config.remote_address, now);
    info->session_creation_bucket.InitOnce(
        options_.session_creation_rate, options_.session_creation_burst, now);
    if (!info->session_creation_bucket.consume(now)) {
      Debug(this,
            "Session creation rate limit exceeded for %s",
            config.remote_address);
      STAT_INCREMENT(Stats, session_creation_rate_limited);
      return;
    }

    Debug(this, "Creating new session for %s", config.dcid);

    std::optional<SessionTicket> no_ticket = std::nullopt;
    auto session = Session::Create(
        this, config, server_state_->tls_context.get(), no_ticket);
    if (!session) {
      Debug(this, "Failed to create session for %s", config.dcid);
      return;
    }
    if (!session->tls_session()) {
      Debug(this,
            "Failed to create TLS session for %s: %s",
            config.dcid,
            session->tls_session().validation_error());
      return;
    }

    AddSession(config.scid, session);
    // It is possible that the session was created then immediately destroyed
    // during the call to AddSession. If that's the case, we'll just return
    // early.
    if (session->is_destroyed()) [[unlikely]]
      return;

    receive(session.get(),
            pkt_data,
            pkt_len,
            config.local_address,
            config.remote_address,
            config.dcid,
            config.scid);
  };

  const auto acceptInitialPacket = [&](const uint32_t version,
                                       const CID& dcid,
                                       const CID& scid,
                                       const uint8_t* pkt_data,
                                       size_t pkt_len,
                                       const SocketAddress& local_address,
                                       const SocketAddress& remote_address) {
    // If we're not listening as a server, do not accept an initial packet.
    if (!is_listening()) return;

    ngtcp2_pkt_hd hd;

    // This is our first condition check... A minimal check to see if ngtcp2 can
    // even recognize this packet as a quic packet.
    if (ngtcp2_accept(&hd, pkt_data, pkt_len) != NGTCP2_SUCCESS) {
      // Per the ngtcp2 docs, ngtcp2_accept returns 0 if the check was
      // successful, or an error code if it was not. Currently there's only one
      // documented error code (NGTCP2_ERR_INVALID_ARGUMENT) but we'll handle
      // any error here the same -- by ignoring the packet entirely.
      return;
    }

    // Unsupported versions are handled earlier in Receive() via the
    // NGTCP2_ERR_VERSION_NEGOTIATION return from ngtcp2_pkt_decode_version_cid.
    // If we reach here, the version must be supported.
    CHECK_NE(ngtcp2_is_supported_version(hd.version), 0);

    // This is the next important condition check... If the server has been
    // marked busy or the remote peer has exceeded their maximum number of
    // concurrent connections, any new connections will be shut down
    // immediately.
    const auto limits_exceeded = ([&] {
      if (state_->max_connections_total > 0 &&
          primary_session_count_ >= state_->max_connections_total) {
        return true;
      }
      if (state_->max_connections_per_host > 0) {
        auto it = conn_counts_per_host_.find(remote_address);
        if (it != conn_counts_per_host_.end() &&
            it->second >= state_->max_connections_per_host) {
          return true;
        }
      }
      return false;
    })();

    if (state_->busy || limits_exceeded) {
      Debug(this,
            "Packet was not accepted because the endpoint is busy or the "
            "remote address %s has exceeded their maximum number of concurrent "
            "connections",
            remote_address);
      // Endpoint is busy or the connection count is exceeded. The connection is
      // refused. For the purpose of stats collection, we'll count both of these
      // the same.
      if (state_->busy) STAT_INCREMENT(Stats, server_busy_count);
      SendImmediateConnectionClose(
          PathDescriptor{version, dcid, scid, local_address, remote_address},
          QuicError::ForTransport(NGTCP2_CONNECTION_REFUSED),
          now);
      // The packet was successfully processed, even if we did refuse the
      // connection.
      STAT_INCREMENT(Stats, packets_received);
      return;
    }

    Debug(
        this, "Accepting initial packet for %s from %s", dcid, remote_address);

    // Generate a fresh server SCID rather than reusing the client's original
    // DCID. The client's original DCID is typically short (8 bytes) and we
    // need a 20-byte SCID to properly match short_dcidlen passed to
    // ngtcp2_pkt_decode_version_cid.
    auto server_scid = server_state_->options.cid_factory->Generate();

    // At this point, we start to set up the configuration for our local
    // session. We pass the received scid here as the dcid argument value
    // because that is the value *this* session will use as the outbound dcid.
    Session::Config config(env(),
                           Side::SERVER,
                           server_state_->options,
                           version,
                           local_address,
                           remote_address,
                           scid,
                           server_scid,
                           dcid);

    Debug(this, "Using session config %s", config);

    // The this point, the config.scid and config.dcid represent *our* views of
    // the CIDs. Specifically, config.dcid identifies the peer and config.scid
    // identifies us. config.dcid should equal scid (peer's SCID is our DCID),
    // and config.ocid should equal dcid (peer's original DCID).
    DCHECK(config.dcid == scid);
    DCHECK(config.ocid == dcid);

    const auto is_remote_address_validated = ([&] {
      auto info = addr_validation_lru_.Peek(remote_address);
      return info != nullptr ? info->validated : false;
    })();

    // Retry token processing and address validation are two separate
    // concerns. A retry token MUST always be parsed when present because
    // it carries the original_destination_connection_id (ODCID) that the
    // server must echo in its transport parameters. Without it, the peer
    // will reject the connection with PROTOCOL_VIOLATION.
    //
    // The address validation LRU cache determines whether we need to
    // *send* a Retry, but must NOT skip *processing* an incoming retry
    // token — a concurrent connection may have already validated the
    // address (populating the LRU) while this connection's Retry was
    // still in flight.

    // Step 1: Always process a retry token if present, to extract the
    // ODCID regardless of address validation state.
    if (hd.type == NGTCP2_PKT_INITIAL && hd.tokenlen > 0 &&
        hd.token[0] == RetryToken::kTokenMagic) {
      RetryToken token(hd.token, hd.tokenlen);
      Debug(this,
            "Initial packet from %s has retry token %s",
            remote_address,
            token);
      auto ocid =
          token.Validate(version,
                         remote_address,
                         dcid,
                         options_.token_secret,
                         options_.retry_token_expiration * NGTCP2_SECONDS);
      if (!ocid.has_value()) {
        Debug(this, "Retry token from %s is invalid.", remote_address);
        SendImmediateConnectionClose(
            PathDescriptor{version, scid, dcid, local_address, remote_address},
            QuicError::ForTransport(NGTCP2_CONNECTION_REFUSED),
            now);
        STAT_INCREMENT(Stats, packets_received);
        return;
      }

      Debug(this,
            "Retry token from %s is valid. Original dcid %s",
            remote_address,
            ocid.value());
      config.ocid = ocid.value();
      config.retry_scid = dcid;
      config.set_token(token);

      // Mark the address as validated since the retry round-trip proves
      // reachability.
      Debug(this, "Remote address %s is validated", remote_address);
      addr_validation_lru_.Upsert(remote_address, now)->validated = true;
    }

    // Step 2: Address validation — decide whether to send a Retry or
    // accept the packet. This only applies when the address has not
    // been validated yet (no LRU hit and no retry token above).
    if (!is_remote_address_validated && !config.retry_scid) {
      Debug(this, "Remote address %s is not validated", remote_address);
      switch (hd.type) {
        case NGTCP2_PKT_INITIAL:
          if (options_.validate_address) {
            if (hd.tokenlen == 0) {
              Debug(this,
                    "Initial packet has no token. Sending retry to %s to start "
                    "validation",
                    remote_address);
              SendRetry(
                  PathDescriptor{
                      version,
                      dcid,
                      scid,
                      local_address,
                      remote_address,
                  },
                  now);
              STAT_INCREMENT(Stats, packets_received);
              return;
            }

            // Non-retry tokens (regular tokens).
            switch (hd.token[0]) {
              case RegularToken::kTokenMagic: {
                RegularToken token(hd.token, hd.tokenlen);
                Debug(this,
                      "Initial packet from %s has regular token %s",
                      remote_address,
                      token);
                if (!token.Validate(
                        version,
                        remote_address,
                        options_.token_secret,
                        options_.token_expiration * NGTCP2_SECONDS)) {
                  Debug(this,
                        "Regular token from %s is invalid.",
                        remote_address);
                  SendRetry(
                      PathDescriptor{
                          version,
                          dcid,
                          scid,
                          local_address,
                          remote_address,
                      },
                      now);
                  STAT_INCREMENT(Stats, packets_received);
                  return;
                }
                Debug(this, "Regular token from %s is valid.", remote_address);
                config.set_token(token);
                break;
              }
              default: {
                Debug(this,
                      "Initial packet from %s has unknown token type",
                      remote_address);
                SendRetry(
                    PathDescriptor{
                        version,
                        dcid,
                        scid,
                        local_address,
                        remote_address,
                    },
                    now);
                STAT_INCREMENT(Stats, packets_received);
                return;
              }
            }

            Debug(this, "Remote address %s is validated", remote_address);
            addr_validation_lru_.Upsert(remote_address, now)->validated = true;
          } else if (hd.tokenlen > 0) {
            Debug(this,
                  "Ignoring initial packet from %s with unexpected token",
                  remote_address);
            return;
          }
          break;
        case NGTCP2_PKT_0RTT:
          if (options_.validate_address) {
            Debug(
                this, "Sending retry to %s due to 0RTT packet", remote_address);
            SendRetry(
                PathDescriptor{
                    version,
                    dcid,
                    scid,
                    local_address,
                    remote_address,
                },
                now);
            STAT_INCREMENT(Stats, packets_received);
            return;
          }
          Debug(this,
                "Accepting 0RTT packet from %s without "
                "address validation",
                remote_address);
          break;
      }
    }

    accept(config, pkt_data, pkt_len);
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
                                       const uint8_t* pkt_data,
                                       size_t pkt_len,
                                       const SocketAddress& local_address,
                                       const SocketAddress& remote_address) {
    // Support for stateless resets can be disabled by the application. If that
    // case, or if the packet is too short to contain a reset token, then we
    // skip the remaining checks.
    if (options_.disable_stateless_reset ||
        pkt_len < NGTCP2_STATELESS_RESET_TOKENLEN) {
      return false;
    }

    // The stateless reset token itself is the *final*
    // NGTCP2_STATELESS_RESET_TOKENLEN bytes in the received packet. If it is a
    // stateless reset then then rest of the bytes in the packet are garbage
    // that we'll ignore.
    const uint8_t* token_pos =
        pkt_data + (pkt_len - NGTCP2_STATELESS_RESET_TOKENLEN);

    // If a Session has been associated with the token, then it is a valid
    // stateless reset token. We need to dispatch it to the session to be
    // processed.
    auto* session = session_manager().FindSessionByStatelessResetToken(
        StatelessResetToken(token_pos));
    if (session != nullptr) {
      // If the session happens to have been destroyed already, we'll
      // just ignore the packet.
      if (!session->is_destroyed()) [[likely]] {
        receive(session,
                pkt_data,
                pkt_len,
                local_address,
                remote_address,
                dcid,
                scid);
      }
      return true;
    }

    // Otherwise, it's not a valid stateless reset token.
    return false;
  };

#ifdef DEBUG
  // When diagnostic packet loss is enabled, the packet will be randomly
  // dropped.
  if (is_diagnostic_packet_loss(options_.rx_loss)) [[unlikely]] {
    // Simulating rx packet loss
    return;
  }
#endif  // DEBUG

  Debug(this, "Received %zu-byte packet from %s", len, remote_address);

  ngtcp2_version_cid pversion_cid;

  // This is our first check to see if the received data can be processed as a
  // QUIC packet. If this fails, then the QUIC packet header is invalid and
  // cannot be processed; all we can do is ignore it. If it succeeds, we have a
  // valid QUIC header but there is still no guarantee that the packet can be
  // successfully processed.
  switch (ngtcp2_pkt_decode_version_cid(
      &pversion_cid, data, len, NGTCP2_MAX_CIDLEN)) {
    case 0:
      break;  // Supported version, continue processing.
    case NGTCP2_ERR_VERSION_NEGOTIATION: {
      // The packet has an unsupported version but the CIDs were
      // successfully decoded. Send a Version Negotiation response
      // per RFC 9000 Section 6. The VN packet's DCID is the client's
      // SCID and vice versa (mirrored back to the client).
      //
      // ngtcp2_pkt_decode_version_cid() only enforces the
      // NGTCP2_MAX_CIDLEN limit for *supported* versions; for an
      // unsupported version it returns the raw connection ID lengths
      // taken from the single-byte length fields on the wire, which can
      // be up to 255. Constructing a CID -- backed by a fixed
      // NGTCP2_MAX_CIDLEN-byte buffer -- from such a length writes past
      // the buffer (an assertion abort in release builds). A single
      // unauthenticated UDP datagram could therefore crash the endpoint
      // before any handshake. Drop these packets, mirroring the
      // CID-length policy applied below for supported versions.
      if (pversion_cid.dcidlen > NGTCP2_MAX_CIDLEN ||
          pversion_cid.scidlen > NGTCP2_MAX_CIDLEN) {
        Debug(this,
              "Version negotiation packet had incorrectly sized CIDs, "
              "ignoring");
        return;
      }
      Debug(this,
            "Packet version %d is not supported, sending version negotiation",
            pversion_cid.version);
      CID dcid(pversion_cid.dcid, pversion_cid.dcidlen);
      CID scid(pversion_cid.scid, pversion_cid.scidlen);
      SendVersionNegotiation(PathDescriptor{pversion_cid.version,
                                            dcid,
                                            scid,
                                            local_address(),
                                            remote_address},
                             now);
      STAT_INCREMENT(Stats, packets_received);
      return;
    }
    default:
      // Truly invalid packet — cannot be decoded at all.
      Debug(this, "Failed to decode packet header, ignoring");
      return;
  }

  // QUIC currently requires CID lengths of max NGTCP2_MAX_CIDLEN. Ignore any
  // packet with a non-standard CID length.
  if (pversion_cid.dcidlen > NGTCP2_MAX_CIDLEN ||
      pversion_cid.scidlen > NGTCP2_MAX_CIDLEN) {
    Debug(this, "Packet had incorrectly sized CIDs, ignoring");
    return;  // Ignore the packet!
  }

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

  Debug(this, "Packet dcid %s, scid %s", dcid, scid);

  // We index the current sessions by the dcid of the client. For initial
  // packets, the dcid is some random value and the scid is omitted from the
  // header (it uses what quic calls a "short header"). It is unlikely (but not
  // impossible) that this randomly selected dcid will be in our index. If we do
  // happen to have a collision, as unlikely as it is, ngtcp2 will do the right
  // thing when it tries to process the packet so we really don't have to worry
  // about it here. If the dcid is not known, the session here will be nullptr.
  //
  // When the session is established, this peer will create its own scid and
  // will send that back to the remote peer to use as the new dcid on
  // subsequent packets. When that session is added, we will index it by the
  // local scid, so as long as the client sends the subsequent packets with the
  // right dcid, everything should just work.

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
    Debug(this, "No existing session for dcid %s", dcid);

    // Handle possible reception of a stateless reset token... If it is a
    // stateless reset, the packet will be handled with no additional action
    // necessary here. We want to return immediately without committing any
    // further resources.
    if (pversion_cid.version == 0 &&
        maybeStatelessReset(dcid, scid, data, len, addr, remote_address)) {
      Debug(this, "Packet was a stateless reset");
      return;  // Stateless reset! Don't do any further processing.
    }

    // If this is a short header packet for an unknown DCID, send a
    // stateless reset so the peer knows the session is gone. Short header
    // packets are identified by version == 0 (set by ngtcp2_pkt_decode_
    // version_cid). We must NOT use !scid here because long header Initial
    // packets can have a 0-length SCID (valid per RFC 9000 Section 7.2).
    if (pversion_cid.version == 0) {
      Debug(this, "Sending stateless reset for unknown short header packet");
      SendStatelessReset(
          PathDescriptor{
              pversion_cid.version, dcid, scid, addr, remote_address},
          len,
          now);
      return;
    }

    // Process the packet as an initial packet...
    return acceptInitialPacket(
        pversion_cid.version, dcid, scid, data, len, addr, remote_address);
  }

  if (session->is_destroyed()) [[unlikely]] {
    // The session has been destroyed. Well that's not good.
    Debug(this, "Session for dcid %s has been destroyed", dcid);
    return;
  }

  // If we got here, the dcid matched the scid of a known local session. Yay!
  // The session will take over any further processing of the packet.
  Debug(this, "Dispatching packet to known session");
  receive(session.get(), data, len, addr, remote_address, dcid, scid);

  // It is important to note that the session may have been destroyed during
  // the call to receive(...). If that's the case, the session object still
  // exists but it is in a destroyed state. Care should be taken accessing
  // session after this point.
}

void Endpoint::PacketDone(int status) {
  if (is_closed()) return;
  // At this point we should be waiting on at least one packet.
  DCHECK_GE(state_->pending_callbacks, 1);
  state_->pending_callbacks--;
  env()->DecreaseWaitingRequestCounter();
  // Check if we can close or start the idle timer now that this
  // pending callback has completed.
  if (state_->closing == 1 || primary_session_count_ == 0) MaybeDestroy();
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
  tracker->TrackField("packet_pool", packet_pool_);
  tracker->TrackField("udp", udp_);
  if (server_state_.has_value()) {
    tracker->TrackField("server_options", server_state_->options);
    tracker->TrackField("server_tls_context", server_state_->tls_context);
  }
  tracker->TrackField("address LRU", addr_validation_lru_);
}

// ======================================================================================
// Endpoint::SocketAddressInfoTraits

bool Endpoint::SocketAddressInfoTraits::CheckExpired(
    const SocketAddress& address, const Type& type, uint64_t now) {
  return (now - type.timestamp) > kSocketAddressInfoTimeout;
}

void Endpoint::SocketAddressInfoTraits::Touch(const SocketAddress& address,
                                              Type* type,
                                              uint64_t now) {
  type->timestamp = now;
}

// ======================================================================================
// JavaScript call outs

void Endpoint::EmitNewSession(const BaseObjectPtr<Session>& session) {
  if (!env()->can_call_into_js()) return;
  CallbackScope<Endpoint> scope(this);
  session->set_wrapped();
  Local<Value> arg = session->object();

  Debug(this, "Notifying JavaScript about new session");
  MakeCallback(BindingData::Get(env()).session_new_callback(), 1, &arg);

  // It is important to note that the session may have been destroyed during
  // the call to MakeCallback. If that's the case, the session object still
  // exists but it is in a destroyed state. Care should be taken accessing
  // session after this point.
}

void Endpoint::EmitClose(CloseContext context, int status) {
  if (!env()->can_call_into_js()) return;
  CallbackScope<Endpoint> scope(this);
  auto isolate = env()->isolate();
  Local<Value> argv[] = {Integer::New(isolate, static_cast<int>(context)),
                         Integer::New(isolate, static_cast<int>(status))};

  Debug(this, "Notifying JavaScript about endpoint closing");
  MakeCallback(
      BindingData::Get(env()).endpoint_close_callback(), arraysize(argv), argv);
}

// ======================================================================================
// Endpoint JavaScript API

JS_METHOD_IMPL(Endpoint::New) {
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

JS_METHOD_IMPL(Endpoint::DoConnect) {
  auto env = Environment::GetCurrent(args);
  Endpoint* endpoint;
  ASSIGN_OR_RETURN_UNWRAP(&endpoint, args.This());

  // args[0] is a SocketAddress
  // args[1] is a Session OptionsObject (see session.cc)
  // args[2] is an optional SessionTicket

  DCHECK(SocketAddressBase::HasInstance(env, args[0]));
  SocketAddressBase* address;
  ASSIGN_OR_RETURN_UNWRAP(&address, args[0]);

  THROW_IF_INSUFFICIENT_PERMISSIONS(
      env, permission::PermissionScope::kNet, address->address()->ToString());

  DCHECK(args[1]->IsObject());
  Session::Options options;
  if (!Session::Options::From(env, args[1]).To(&options)) {
    // There was an error. Return to propagate
    return;
  }

  BaseObjectWeakPtr<Session> session;

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

JS_METHOD_IMPL(Endpoint::DoListen) {
  Endpoint* endpoint;
  ASSIGN_OR_RETURN_UNWRAP(&endpoint, args.This());
  auto env = Environment::GetCurrent(args);

  THROW_IF_INSUFFICIENT_PERMISSIONS(env, permission::PermissionScope::kNet, "");

  Session::Options options;
  if (Session::Options::From(env, args[0]).To(&options)) {
    endpoint->Listen(options);
  }
}

JS_METHOD_IMPL(Endpoint::MarkBusy) {
  Endpoint* endpoint;
  ASSIGN_OR_RETURN_UNWRAP(&endpoint, args.This());
  endpoint->MarkAsBusy(args[0]->IsTrue());
}

JS_METHOD_IMPL(Endpoint::DoCloseGracefully) {
  Endpoint* endpoint;
  ASSIGN_OR_RETURN_UNWRAP(&endpoint, args.This());
  endpoint->CloseGracefully();
}

JS_METHOD_IMPL(Endpoint::LocalAddress) {
  auto env = Environment::GetCurrent(args);
  Endpoint* endpoint;
  ASSIGN_OR_RETURN_UNWRAP(&endpoint, args.This());
  if (endpoint->is_closed() || !endpoint->udp_.is_bound()) return;
  auto addr = SocketAddressBase::Create(
      env, std::make_shared<SocketAddress>(endpoint->local_address()));
  if (addr) args.GetReturnValue().Set(addr->object());
}

JS_METHOD_IMPL(Endpoint::Ref) {
  Endpoint* endpoint;
  ASSIGN_OR_RETURN_UNWRAP(&endpoint, args.This());
  auto env = Environment::GetCurrent(args);
  if (args[0]->BooleanValue(env->isolate())) {
    endpoint->udp_.Ref();
  } else {
    endpoint->udp_.Unref();
  }
}

JS_METHOD_IMPL(Endpoint::DoSetSNIContexts) {
  auto env = Environment::GetCurrent(args);
  Endpoint* endpoint;
  ASSIGN_OR_RETURN_UNWRAP(&endpoint, args.This());

  if (!endpoint->server_state_.has_value()) {
    THROW_ERR_INVALID_STATE(env, "Endpoint is not listening");
    return;
  }

  if (args.Length() < 1 || !args[0]->IsObject()) {
    THROW_ERR_INVALID_ARG_TYPE(env, "entries must be an object");
    return;
  }

  bool replace = args.Length() > 1 && args[1]->IsTrue();

  auto entries_obj = args[0].As<Object>();
  Local<Array> hostnames;
  if (!entries_obj->GetOwnPropertyNames(env->context()).ToLocal(&hostnames)) {
    return;
  }

  if (replace) {
    std::unordered_map<std::string, TLSContext::Options> entries;
    for (uint32_t i = 0; i < hostnames->Length(); i++) {
      Local<Value> key;
      Local<Value> entry_val;
      if (!hostnames->Get(env->context(), i).ToLocal(&key) ||
          !key->IsString() ||
          !entries_obj->Get(env->context(), key).ToLocal(&entry_val)) {
        return;
      }
      Utf8Value hostname(env->isolate(), key);
      auto entry_options = TLSContext::Options::From(env, entry_val);
      if (entry_options.IsNothing()) return;
      entries[std::string(*hostname, hostname.length())] =
          entry_options.FromJust();
    }

    if (!endpoint->server_state_->tls_context->SetSNIContexts(env, entries)) {
      THROW_ERR_INVALID_STATE(env, "Failed to set SNI contexts");
      return;
    }
  } else {
    for (uint32_t i = 0; i < hostnames->Length(); i++) {
      Local<Value> key;
      Local<Value> entry_val;
      if (!hostnames->Get(env->context(), i).ToLocal(&key) ||
          !key->IsString() ||
          !entries_obj->Get(env->context(), key).ToLocal(&entry_val)) {
        return;
      }
      Utf8Value hostname(env->isolate(), key);
      auto entry_options = TLSContext::Options::From(env, entry_val);
      if (entry_options.IsNothing()) return;

      if (!endpoint->server_state_->tls_context->AddSNIContext(
              env,
              std::string(*hostname, hostname.length()),
              entry_options.FromJust())) {
        THROW_ERR_INVALID_STATE(
            env, "Failed to add SNI context for '%s'", *hostname);
        return;
      }
    }
  }
}

}  // namespace quic
}  // namespace node
#endif  // OPENSSL_NO_QUIC
#endif  // HAVE_OPENSSL && HAVE_QUIC
