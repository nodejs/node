#ifndef SRC_NODE_QUIC_UTIL_H_
#define SRC_NODE_QUIC_UTIL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node.h"
#include "node_sockaddr.h"
#include "uv.h"
#include "v8.h"

#include <ngtcp2/ngtcp2.h>
#include <openssl/ssl.h>

#include <functional>
#include <limits>
#include <string>

namespace node {
namespace quic {

constexpr uint64_t NGTCP2_APP_NOERROR = 0xff00;

constexpr size_t MIN_INITIAL_QUIC_PKT_SIZE = 1200;
constexpr size_t NGTCP2_SV_SCIDLEN = NGTCP2_MAX_CIDLEN;
constexpr size_t TOKEN_RAND_DATALEN = 16;
constexpr size_t TOKEN_SECRETLEN = 16;

constexpr size_t kMaxSizeT = std::numeric_limits<size_t>::max();
constexpr size_t DEFAULT_MAX_CONNECTIONS_PER_HOST = 100;
constexpr uint64_t MIN_MAX_CRYPTO_BUFFER = 4096;
constexpr uint64_t MIN_RETRYTOKEN_EXPIRATION = 1;
constexpr uint64_t MAX_RETRYTOKEN_EXPIRATION = 60;
constexpr uint64_t DEFAULT_MAX_CRYPTO_BUFFER = MIN_MAX_CRYPTO_BUFFER * 4;
constexpr uint64_t DEFAULT_ACTIVE_CONNECTION_ID_LIMIT = 10;
constexpr uint64_t DEFAULT_MAX_STREAM_DATA_BIDI_LOCAL = 256 * 1024;
constexpr uint64_t DEFAULT_MAX_STREAM_DATA_BIDI_REMOTE = 256 * 1024;
constexpr uint64_t DEFAULT_MAX_STREAM_DATA_UNI = 256 * 1024;
constexpr uint64_t DEFAULT_MAX_DATA = 1 * 1024 * 1024;
constexpr uint64_t DEFAULT_MAX_STREAMS_BIDI = 100;
constexpr uint64_t DEFAULT_MAX_STREAMS_UNI = 3;
constexpr uint64_t DEFAULT_IDLE_TIMEOUT = 10 * 10000000000;
constexpr uint64_t DEFAULT_RETRYTOKEN_EXPIRATION = 10ULL;

using RetryTokenSecret = std::array<uint8_t, TOKEN_SECRETLEN>;
using ResetTokenSecret = std::array<uint8_t, NGTCP2_STATELESS_RESET_TOKENLEN>;

enum SelectPreferredAddressPolicy : int {
  // Ignore the server-provided preferred address
  QUIC_PREFERRED_ADDRESS_IGNORE,
  // Accept the server-provided preferred address
  QUIC_PREFERRED_ADDRESS_ACCEPT
};

// Fun hash combine trick based on a variadic template that
// I came across a while back but can't remember where. Will add an attribution
// if I can find the source.
inline void hash_combine(size_t* seed) { }

template <typename T, typename... Args>
inline void hash_combine(size_t* seed, const T& value, Args... rest) {
    *seed ^= std::hash<T>{}(value) + 0x9e3779b9 + (*seed << 6) + (*seed >> 2);
    hash_combine(seed, rest...);
}

// QUIC error codes generally fall into two distinct namespaces:
// Connection Errors and Application Errors. Connection errors
// are further subdivided into Crypto and non-Crypto. Application
// errors are entirely specific to the QUIC application being
// used. An easy rule of thumb is that Application errors are
// semantically associated with the ALPN identifier negotiated
// for the QuicSession. So, if a connection is closed with
// family: QUIC_ERROR_APPLICATION and code: 123, you have to
// look at the ALPN identifier to determine exactly what it
// means. Connection (Session) and Crypto errors, on the other
// hand, share the same meaning regardless of the ALPN.
enum QuicErrorFamily : int32_t {
  QUIC_ERROR_SESSION,
  QUIC_ERROR_CRYPTO,
  QUIC_ERROR_APPLICATION
};

struct QuicError {
  int32_t family;
  uint64_t code;
  inline QuicError(
      int32_t family_ = QUIC_ERROR_SESSION,
      int code_ = NGTCP2_NO_ERROR);
  inline QuicError(
      int32_t family_ = QUIC_ERROR_SESSION,
      uint64_t code_ = NGTCP2_NO_ERROR);
  explicit inline QuicError(ngtcp2_connection_close_error_code code);
  inline QuicError(
      Environment* env,
      v8::Local<v8::Value> codeArg,
      v8::Local<v8::Value> familyArg = v8::Local<v8::Object>(),
      int32_t family_ = QUIC_ERROR_SESSION);
  inline const char* GetFamilyName();
};

inline size_t GetMaxPktLen(const sockaddr* addr);

inline bool ResolvePreferredAddress(
    Environment* env,
    int local_address_family,
    const ngtcp2_preferred_addr* paddr,
    uv_getaddrinfo_t* req);

inline ngtcp2_addr* ToNgtcp2Addr(
    const SocketAddress& addr,
    ngtcp2_addr* dest);

struct QuicPath : public ngtcp2_path {
  QuicPath(
      SocketAddress* local,
      SocketAddress* remote) {
    ngtcp2_addr_init(
        &this->local,
        local->data(),
        local->GetLength(),
        local);
    ngtcp2_addr_init(
        &this->remote,
        local->data(),
        remote->GetLength(),
        remote);
  }
  QuicPath(
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
};

struct QuicPathStorage : public ngtcp2_path_storage {
  QuicPathStorage() {
    ngtcp2_path_storage_zero(this);
  }
};

// Simple wrapper for ngtcp2_cid that handles hex encoding
// and conversion to std::string automatically
class QuicCID {
 public:
  explicit QuicCID(ngtcp2_cid* cid) :
      cid_(*cid),
      str_(cid->data, cid->data + cid->datalen) {}
  explicit QuicCID(const ngtcp2_cid* cid) :
      cid_(*cid),
      str_(cid->data, cid->data + cid->datalen) {}
  explicit QuicCID(const ngtcp2_cid& cid) :
      cid_(cid),
      str_(cid.data, cid.data + cid.datalen) {}
  QuicCID(const uint8_t* cid, size_t len) {
    ngtcp2_cid_init(&cid_, cid, len);
    str_ = std::string(cid_.data, cid_.data + cid_.datalen);
  }

  inline std::string ToStr() const;

  inline std::string ToHex() const;

  const ngtcp2_cid& operator*() const { return cid_; }
  const ngtcp2_cid* operator->() const { return &cid_; }
  const ngtcp2_cid* cid() const { return &cid_; }

  const uint8_t* data() const { return cid_.data; }
  size_t length() const { return cid_.datalen; }

 private:
  ngtcp2_cid cid_;
  std::string str_;
  mutable std::string hex_;
};

// https://stackoverflow.com/questions/33701430/template-function-to-access-struct-members
template <typename C, typename T>
decltype(auto) access(C* cls, T C::*member) {
  return (cls->*member);
}

template <typename C, typename T, typename... Mems>
decltype(auto) access(C* cls, T C::*member, Mems... rest) {
  return access((cls->*member), rest...);
}

template <typename A, typename... Members>
void IncrementStat(
    uint64_t amount,
    A* a,
    Members... mems) {
  static uint64_t max = std::numeric_limits<uint64_t>::max();
  uint64_t current = access(a, mems...);
  uint64_t delta = std::min(amount, max - current);
  access(a, mems...) += delta;
}

// Simple timer wrapper that is used to implement the internals
// for idle and retransmission timeouts. Call Update to start or
// reset the timer; Stop to halt the timer.
class Timer final : public MemoryRetainer {
 public:
  inline explicit Timer(Environment* env, std::function<void()> fn);

  // Stops the timer with the side effect of the timer no longer being usable.
  // It will be cleaned up and the Timer object will be destroyed.
  inline void Stop();

  // If the timer is not currently active, interval must be either 0 or greater.
  // If the timer is already active, interval is ignored.
  inline void Update(uint64_t interval);

  static inline void Free(Timer* timer);

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(Timer)
  SET_SELF_SIZE(Timer)

 private:
  static inline void OnTimeout(uv_timer_t* timer);

  bool stopped_ = false;
  Environment* env_;
  std::function<void()> fn_;
  uv_timer_t timer_;
};

using TimerPointer = DeleteFnPtr<Timer, Timer::Free>;

inline ngtcp2_crypto_level from_ossl_level(OSSL_ENCRYPTION_LEVEL ossl_level);
inline const char* crypto_level_name(ngtcp2_crypto_level level);

}  // namespace quic
}  // namespace node

#endif  // NOE_WANT_INTERNALS

#endif  // SRC_NODE_QUIC_UTIL_H_
