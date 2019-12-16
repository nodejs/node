#ifndef SRC_NODE_QUIC_UTIL_INL_H_
#define SRC_NODE_QUIC_UTIL_INL_H_

#include "node_internals.h"
#include "node_quic_util.h"
#include "env-inl.h"
#include "string_bytes.h"
#include "util-inl.h"
#include "uv.h"

#include <string>

namespace node {

namespace quic {

std::string QuicCID::ToStr() const {
  return std::string(cid_.data, cid_.data + cid_.datalen);
}

std::string QuicCID::ToHex() const {
  MaybeStackBuffer<char, 64> dest;
  dest.AllocateSufficientStorage(cid_.datalen * 2);
  dest.SetLengthAndZeroTerminate(cid_.datalen * 2);
  size_t written = StringBytes::hex_encode(
      reinterpret_cast<const char*>(cid_.data),
      cid_.datalen,
      *dest,
      dest.length());
  return std::string(*dest, written);
}

ngtcp2_addr* ToNgtcp2Addr(const SocketAddress& addr, ngtcp2_addr* dest) {
  if (dest == nullptr)
    dest = new ngtcp2_addr();
  return ngtcp2_addr_init(dest, addr.data(), addr.GetLength(), nullptr);
}

size_t GetMaxPktLen(const sockaddr* addr) {
  return addr->sa_family == AF_INET6 ?
      NGTCP2_MAX_PKTLEN_IPV6 :
      NGTCP2_MAX_PKTLEN_IPV4;
}

bool ResolvePreferredAddress(
    Environment* env,
    int local_address_family,
    const ngtcp2_preferred_addr* paddr,
    uv_getaddrinfo_t* req) {
  int af;
  const uint8_t* binaddr;
  uint16_t port;
  constexpr uint8_t empty_addr[] = {0, 0, 0, 0, 0, 0, 0, 0,
                                    0, 0, 0, 0, 0, 0, 0, 0};

  if (local_address_family == AF_INET &&
      memcmp(empty_addr, paddr->ipv4_addr, sizeof(paddr->ipv4_addr)) != 0) {
    af = AF_INET;
    binaddr = paddr->ipv4_addr;
    port = paddr->ipv4_port;
  } else if (local_address_family == AF_INET6 &&
              memcmp(empty_addr,
                    paddr->ipv6_addr,
                    sizeof(paddr->ipv6_addr)) != 0) {
    af = AF_INET6;
    binaddr = paddr->ipv6_addr;
    port = paddr->ipv6_port;
  } else {
    return false;
  }

  char host[NI_MAXHOST];
  if (uv_inet_ntop(af, binaddr, host, sizeof(host)) != 0)
    return false;

  addrinfo hints{};
  hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;
  hints.ai_family = af;
  hints.ai_socktype = SOCK_DGRAM;

  return
      uv_getaddrinfo(
          env->event_loop(),
          req,
          nullptr,
          host,
          std::to_string(port).c_str(),
          &hints) == 0;
}

Timer::Timer(Environment* env, std::function<void()> fn)
  : env_(env),
    fn_(fn) {
  uv_timer_init(env_->event_loop(), &timer_);
  timer_.data = this;
}

void Timer::Stop() {
  if (stopped_)
    return;
  stopped_ = true;

  if (timer_.data == this) {
    uv_timer_stop(&timer_);
    timer_.data = nullptr;
  }
}

// If the timer is not currently active, interval must be either 0 or greater.
// If the timer is already active, interval is ignored.
void Timer::Update(uint64_t interval) {
  if (stopped_)
    return;
  uv_timer_start(&timer_, OnTimeout, interval, interval);
  uv_unref(reinterpret_cast<uv_handle_t*>(&timer_));
}

void Timer::Free(Timer* timer) {
  timer->env_->CloseHandle(
      reinterpret_cast<uv_handle_t*>(&timer->timer_),
      [&](uv_handle_t* timer) {
        Timer* t = ContainerOf(
            &Timer::timer_,
            reinterpret_cast<uv_timer_t*>(timer));
        delete t;
      });
}

void Timer::OnTimeout(uv_timer_t* timer) {
  Timer* t = ContainerOf(&Timer::timer_, timer);
  t->fn_();
}

ngtcp2_crypto_level from_ossl_level(OSSL_ENCRYPTION_LEVEL ossl_level) {
  switch (ossl_level) {
  case ssl_encryption_initial:
    return NGTCP2_CRYPTO_LEVEL_INITIAL;
  case ssl_encryption_early_data:
    return NGTCP2_CRYPTO_LEVEL_EARLY;
  case ssl_encryption_handshake:
    return NGTCP2_CRYPTO_LEVEL_HANDSHAKE;
  case ssl_encryption_application:
    return NGTCP2_CRYPTO_LEVEL_APP;
  default:
    UNREACHABLE();
  }
}

const char* crypto_level_name(ngtcp2_crypto_level level) {
  switch (level) {
    case NGTCP2_CRYPTO_LEVEL_INITIAL:
      return "initial";
    case NGTCP2_CRYPTO_LEVEL_EARLY:
      return "early";
    case NGTCP2_CRYPTO_LEVEL_HANDSHAKE:
      return "handshake";
    case NGTCP2_CRYPTO_LEVEL_APP:
      return "app";
    default:
      UNREACHABLE();
  }
}

QuicError::QuicError(
    int32_t family_,
    uint64_t code_) :
    family(family_),
    code(code_) {}

QuicError::QuicError(
    int32_t family_,
    int code_) :
    family(family_) {
  switch (family) {
    case QUIC_ERROR_CRYPTO:
      code_ |= NGTCP2_CRYPTO_ERROR;
      // Fall-through...
    case QUIC_ERROR_SESSION:
      code = ngtcp2_err_infer_quic_transport_error_code(code_);
      break;
    case QUIC_ERROR_APPLICATION:
      code = code_;
      break;
    default:
      UNREACHABLE();
  }
}

QuicError::QuicError(ngtcp2_connection_close_error_code ccec) :
    family(QUIC_ERROR_SESSION),
    code(ccec.error_code) {
  switch (ccec.type) {
    case NGTCP2_CONNECTION_CLOSE_ERROR_CODE_TYPE_APPLICATION:
      family = QUIC_ERROR_APPLICATION;
      break;
    case NGTCP2_CONNECTION_CLOSE_ERROR_CODE_TYPE_TRANSPORT:
      if (code & NGTCP2_CRYPTO_ERROR)
        family = QUIC_ERROR_CRYPTO;
      break;
    default:
      UNREACHABLE();
  }
}

QuicError::QuicError(
    Environment* env,
    v8::Local<v8::Value> codeArg,
    v8::Local<v8::Value> familyArg,
    int32_t family_) :
    family(family_),
    code(NGTCP2_NO_ERROR) {
  if (codeArg->IsBigInt()) {
    code = codeArg.As<v8::BigInt>()->Int64Value();
  } else if (codeArg->IsNumber()) {
    double num = 0;
    CHECK(codeArg->NumberValue(env->context()).To(&num));
    code = static_cast<uint64_t>(num);
  }
  if (familyArg->IsNumber()) {
    CHECK(familyArg->Int32Value(env->context()).To(&family));
  }
}

const char* QuicError::GetFamilyName() {
  switch (family) {
    case QUIC_ERROR_SESSION:
      return "Session";
    case QUIC_ERROR_APPLICATION:
      return "Application";
    case QUIC_ERROR_CRYPTO:
      return "Crypto";
    default:
      UNREACHABLE();
  }
}

}  // namespace quic
}  // namespace node

#endif  // SRC_NODE_QUIC_UTIL_INL_H_
