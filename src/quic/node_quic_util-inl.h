#ifndef SRC_QUIC_NODE_QUIC_UTIL_INL_H_
#define SRC_QUIC_NODE_QUIC_UTIL_INL_H_

#include "node_internals.h"
#include "node_quic_util.h"
#include "env-inl.h"
#include "string_bytes.h"
#include "util-inl.h"
#include "uv.h"

#include <string>

namespace node {

namespace quic {

QuicPath::QuicPath(
    const SocketAddress& local,
    const SocketAddress& remote) {
  ngtcp2_addr_init(
      &this->local,
      local.data(),
      local.GetLength(),
      const_cast<SocketAddress*>(&local));
  ngtcp2_addr_init(
      &this->remote,
      remote.data(),
      remote.GetLength(),
      const_cast<SocketAddress*>(&remote));
}

size_t QuicCID::Hash::operator()(const QuicCID& token) const {
  size_t hash = 0;
  for (size_t n = 0; n < token.cid_.datalen; n++)
    hash ^= std::hash<uint8_t>{}(token.cid_.data[n]) + 0x9e3779b9 +
            (hash << 6) + (hash >> 2);
  return hash;
}

bool QuicCID::Compare::operator()(
    const QuicCID& lcid,
    const QuicCID& rcid) const {
  if (lcid.cid_.datalen != rcid.cid_.datalen)
    return false;
  return memcmp(
      lcid.cid_.data,
      rcid.cid_.data,
      lcid.cid_.datalen) == 0;
}

std::string QuicCID::ToHex() const {
  std::vector<char> dest(cid_.datalen * 2 + 1);
  dest[dest.size() - 1] = '\0';
  size_t written = StringBytes::hex_encode(
      reinterpret_cast<const char*>(cid_.data),
      cid_.datalen,
      dest.data(),
      dest.size());
  return std::string(dest.data(), written);
}

size_t GetMaxPktLen(const sockaddr* addr) {
  return addr->sa_family == AF_INET6 ?
      NGTCP2_MAX_PKTLEN_IPV6 :
      NGTCP2_MAX_PKTLEN_IPV4;
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

const char* QuicError::family_name() {
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

const ngtcp2_cid* QuicPreferredAddress::cid() const {
  return &paddr_->cid;
}

const uint8_t* QuicPreferredAddress::stateless_reset_token() const {
  return paddr_->stateless_reset_token;
}

std::string QuicPreferredAddress::preferred_ipv6_address() const {
  char host[NI_MAXHOST];
  // Return an empty string if unable to convert...
  if (uv_inet_ntop(AF_INET6, paddr_->ipv6_addr, host, sizeof(host)) != 0)
    return std::string();

  return std::string(host);
}
std::string QuicPreferredAddress::preferred_ipv4_address() const {
  char host[NI_MAXHOST];
  // Return an empty string if unable to convert...
  if (uv_inet_ntop(AF_INET, paddr_->ipv4_addr, host, sizeof(host)) != 0)
    return std::string();

  return std::string(host);
}

int16_t QuicPreferredAddress::preferred_ipv6_port() const {
  return paddr_->ipv6_port;
}
int16_t QuicPreferredAddress::preferred_ipv4_port() const {
  return paddr_->ipv4_port;
}

bool QuicPreferredAddress::Use(int family) const {
  uv_getaddrinfo_t req;

  if (!ResolvePreferredAddress(family, &req))
    return false;

  dest_->addrlen = req.addrinfo->ai_addrlen;
  memcpy(dest_->addr, req.addrinfo->ai_addr, req.addrinfo->ai_addrlen);
  uv_freeaddrinfo(req.addrinfo);
  return true;
}

bool QuicPreferredAddress::ResolvePreferredAddress(
    int local_address_family,
    uv_getaddrinfo_t* req) const {
  int af;
  const uint8_t* binaddr;
  uint16_t port;
  switch (local_address_family) {
    case AF_INET:
      if (paddr_->ipv4_port > 0) {
        af = AF_INET;
        binaddr = paddr_->ipv4_addr;
        port = paddr_->ipv4_port;
        break;
      }
      return false;
    case AF_INET6:
      if (paddr_->ipv6_port > 0) {
        af = AF_INET6;
        binaddr = paddr_->ipv6_addr;
        port = paddr_->ipv6_port;
        break;
      }
      return false;
    default:
      UNREACHABLE();
  }

  char host[NI_MAXHOST];
  if (uv_inet_ntop(af, binaddr, host, sizeof(host)) != 0)
    return false;

  addrinfo hints{};
  hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;
  hints.ai_family = af;
  hints.ai_socktype = SOCK_DGRAM;

  // Unfortunately ngtcp2 requires the selection of the
  // preferred address to be synchronous, which means we
  // have to do a sync resolve using uv_getaddrinfo here.
  return
      uv_getaddrinfo(
          env_->event_loop(),
          req,
          nullptr,
          host,
          std::to_string(port).c_str(),
          &hints) == 0 &&
      req->addrinfo != nullptr;
}

std::string StatelessResetToken::ToHex() const {
  std::vector<char> dest(NGTCP2_STATELESS_RESET_TOKENLEN * 2 + 1);
  dest[dest.size() - 1] = '\0';
  size_t written = StringBytes::hex_encode(
      reinterpret_cast<const char*>(token_),
      NGTCP2_STATELESS_RESET_TOKENLEN,
      dest.data(),
      dest.size());
  return std::string(dest.data(), written);
}

size_t StatelessResetToken::Hash::operator()(
    const StatelessResetToken& token) const {
  size_t hash = 0;
  for (size_t n = 0; n < NGTCP2_STATELESS_RESET_TOKENLEN; n++)
    hash ^= std::hash<uint8_t>{}(token.token_[n]) + 0x9e3779b9 +
            (hash << 6) + (hash >> 2);
  return hash;
}

bool StatelessResetToken::Compare::operator()(
    const StatelessResetToken& ltoken,
    const StatelessResetToken& rtoken) const {
  return memcmp(
      ltoken.token_,
      rtoken.token_,
      NGTCP2_STATELESS_RESET_TOKENLEN) == 0;
}

}  // namespace quic
}  // namespace node

#endif  // SRC_QUIC_NODE_QUIC_UTIL_INL_H_
