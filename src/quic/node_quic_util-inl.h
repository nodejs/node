#ifndef SRC_QUIC_NODE_QUIC_UTIL_INL_H_
#define SRC_QUIC_NODE_QUIC_UTIL_INL_H_

#include "debug_utils-inl.h"
#include "node_internals.h"
#include "node_quic_crypto.h"
#include "node_quic_util.h"
#include "memory_tracker-inl.h"
#include "env-inl.h"
#include "histogram-inl.h"
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
      local.length(),
      const_cast<SocketAddress*>(&local));
  ngtcp2_addr_init(
      &this->remote,
      remote.data(),
      remote.length(),
      const_cast<SocketAddress*>(&remote));
}

size_t QuicCID::Hash::operator()(const QuicCID& token) const {
  size_t hash = 0;
  for (size_t n = 0; n < token->datalen; n++) {
    hash ^= std::hash<uint8_t>{}(token->data[n]) + 0x9e3779b9 +
            (hash << 6) + (hash >> 2);
  }
  return hash;
}

QuicCID& QuicCID::operator=(const QuicCID& cid) {
  if (this == &cid) return *this;
  this->~QuicCID();
  return *new(this) QuicCID(std::move(cid));
}

bool QuicCID::operator==(const QuicCID& other) const {
  return memcmp(cid()->data, other.cid()->data, cid()->datalen) == 0;
}

bool QuicCID::operator!=(const QuicCID& other) const {
  return !(*this == other);
}

std::string QuicCID::ToString() const {
  std::vector<char> dest(ptr_->datalen * 2 + 1);
  dest[dest.size() - 1] = '\0';
  size_t written = StringBytes::hex_encode(
      reinterpret_cast<const char*>(ptr_->data),
      ptr_->datalen,
      dest.data(),
      dest.size());
  return std::string(dest.data(), written);
}

size_t GetMaxPktLen(const SocketAddress& addr) {
  return addr.family() == AF_INET6 ?
      NGTCP2_MAX_PKTLEN_IPV6 :
      NGTCP2_MAX_PKTLEN_IPV4;
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

const ngtcp2_cid* PreferredAddress::cid() const {
  return &paddr_->cid;
}

const uint8_t* PreferredAddress::stateless_reset_token() const {
  return paddr_->stateless_reset_token;
}

std::string PreferredAddress::ipv6_address() const {
  char host[NI_MAXHOST];
  // Return an empty string if unable to convert...
  if (uv_inet_ntop(AF_INET6, paddr_->ipv6_addr, host, sizeof(host)) != 0)
    return std::string();

  return std::string(host);
}
std::string PreferredAddress::ipv4_address() const {
  char host[NI_MAXHOST];
  // Return an empty string if unable to convert...
  if (uv_inet_ntop(AF_INET, paddr_->ipv4_addr, host, sizeof(host)) != 0)
    return std::string();

  return std::string(host);
}

uint16_t PreferredAddress::ipv6_port() const {
  return paddr_->ipv6_port;
}

uint16_t PreferredAddress::ipv4_port() const {
  return paddr_->ipv4_port;
}

bool PreferredAddress::Use(int family) const {
  uv_getaddrinfo_t req;

  if (!ResolvePreferredAddress(family, &req))
    return false;

  dest_->addrlen = req.addrinfo->ai_addrlen;
  memcpy(dest_->addr, req.addrinfo->ai_addr, req.addrinfo->ai_addrlen);
  uv_freeaddrinfo(req.addrinfo);
  return true;
}

bool PreferredAddress::ResolvePreferredAddress(
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

StatelessResetToken::StatelessResetToken(
    uint8_t* token,
    const uint8_t* secret,
    const QuicCID& cid) {
  GenerateResetToken(token, secret, cid);
  memcpy(buf_, token, sizeof(buf_));
}

StatelessResetToken::StatelessResetToken(
    const uint8_t* secret,
    const QuicCID& cid)  {
  GenerateResetToken(buf_, secret, cid);
}

StatelessResetToken::StatelessResetToken(
    const uint8_t* token) {
  memcpy(buf_, token, sizeof(buf_));
}

std::string StatelessResetToken::ToString() const {
  std::vector<char> dest(NGTCP2_STATELESS_RESET_TOKENLEN * 2 + 1);
  dest[dest.size() - 1] = '\0';
  size_t written = StringBytes::hex_encode(
      reinterpret_cast<const char*>(buf_),
      NGTCP2_STATELESS_RESET_TOKENLEN,
      dest.data(),
      dest.size());
  return std::string(dest.data(), written);
}

size_t StatelessResetToken::Hash::operator()(
    const StatelessResetToken& token) const {
  size_t hash = 0;
  for (size_t n = 0; n < NGTCP2_STATELESS_RESET_TOKENLEN; n++)
    hash ^= std::hash<uint8_t>{}(token.buf_[n]) + 0x9e3779b9 +
            (hash << 6) + (hash >> 2);
  return hash;
}

bool StatelessResetToken::operator==(const StatelessResetToken& other) const {
  return memcmp(data(), other.data(), NGTCP2_STATELESS_RESET_TOKENLEN) == 0;
}

bool StatelessResetToken::operator!=(const StatelessResetToken& other) const {
  return !(*this == other);
}

template <typename T>
StatsBase<T>::StatsBase(
    Environment* env,
    v8::Local<v8::Object> wrap,
    int options) {
  static constexpr uint64_t kMax = std::numeric_limits<int64_t>::max();

  // Create the backing store for the statistics
  size_t size = sizeof(Stats);
  size_t count = size / sizeof(uint64_t);
  stats_store_ = v8::ArrayBuffer::NewBackingStore(env->isolate(), size);
  stats_ = new (stats_store_->Data()) Stats;

  DCHECK_NOT_NULL(stats_);
  stats_->created_at = uv_hrtime();

  // The stats buffer is exposed as a BigUint64Array on
  // the JavaScript side to allow statistics to be monitored.
  v8::Local<v8::ArrayBuffer> stats_buffer =
      v8::ArrayBuffer::New(env->isolate(), stats_store_);
  v8::Local<v8::BigUint64Array> stats_array =
      v8::BigUint64Array::New(stats_buffer, 0, count);
  USE(wrap->DefineOwnProperty(
      env->context(),
      env->stats_string(),
      stats_array,
      v8::PropertyAttribute::ReadOnly));

  if (options & HistogramOptions::ACK) {
    ack_ = HistogramBase::New(env, 1, kMax);
    wrap->DefineOwnProperty(
        env->context(),
        env->ack_string(),
        ack_->object(),
        v8::PropertyAttribute::ReadOnly).Check();
  }

  if (options & HistogramOptions::RATE) {
    rate_ = HistogramBase::New(env, 1, kMax);
    wrap->DefineOwnProperty(
        env->context(),
        env->rate_string(),
        rate_->object(),
        v8::PropertyAttribute::ReadOnly).Check();
  }

  if (options & HistogramOptions::SIZE) {
    size_ = HistogramBase::New(env, 1, kMax);
    wrap->DefineOwnProperty(
        env->context(),
        env->size_string(),
        size_->object(),
        v8::PropertyAttribute::ReadOnly).Check();
  }
}

template <typename T>
void StatsBase<T>::IncrementStat(uint64_t Stats::*member, uint64_t amount) {
  static constexpr uint64_t kMax = std::numeric_limits<uint64_t>::max();
  stats_->*member += std::min(amount, kMax - stats_->*member);
}

template <typename T>
void StatsBase<T>::SetStat(uint64_t Stats::*member, uint64_t value) {
  stats_->*member = value;
}

template <typename T>
void StatsBase<T>::RecordTimestamp(uint64_t Stats::*member) {
  stats_->*member = uv_hrtime();
}

template <typename T>
uint64_t StatsBase<T>::GetStat(uint64_t Stats::*member) const {
  return stats_->*member;
}

template <typename T>
inline void StatsBase<T>::RecordRate(uint64_t Stats::*member) {
  CHECK(rate_);
  uint64_t received_at = GetStat(member);
  uint64_t now = uv_hrtime();
  if (received_at > 0)
    rate_->Record(now - received_at);
  SetStat(member, now);
}

template <typename T>
inline void StatsBase<T>::RecordSize(uint64_t val) {
  CHECK(size_);
  size_->Record(val);
}

template <typename T>
inline void StatsBase<T>::RecordAck(uint64_t Stats::*member) {
  CHECK(ack_);
  uint64_t acked_at = GetStat(member);
  uint64_t now = uv_hrtime();
  if (acked_at > 0)
    ack_->Record(now - acked_at);
  SetStat(member, now);
}

template <typename T>
void StatsBase<T>::StatsMemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("stats_store", stats_store_);
  tracker->TrackField("rate_histogram", rate_);
  tracker->TrackField("size_histogram", size_);
  tracker->TrackField("ack_histogram", ack_);
}

template <typename T>
void StatsBase<T>::DebugStats() {
  StatsDebug stats_debug(static_cast<typename T::Base*>(this));
  Debug(static_cast<typename T::Base*>(this), "Destroyed. %s", stats_debug);
}

template <typename T>
std::string StatsBase<T>::StatsDebug::ToString() const {
  std::string out = "Statistics:\n";
  auto add_field = [&out](const char* name, uint64_t val) {
    out += "  ";
    out += std::string(name);
    out += ": ";
    out += std::to_string(val);
    out += "\n";
  };
  add_field("Duration", uv_hrtime() - ptr->GetStat(&Stats::created_at));
  T::ToString(*ptr, add_field);
  return out;
}

template <typename T>
size_t get_length(const T* vec, size_t count) {
  CHECK_NOT_NULL(vec);
  size_t len = 0;
  for (size_t n = 0; n < count; n++)
    len += vec[n].len;
  return len;
}

}  // namespace quic
}  // namespace node

#endif  // SRC_QUIC_NODE_QUIC_UTIL_INL_H_
