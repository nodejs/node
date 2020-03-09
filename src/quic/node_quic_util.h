#ifndef SRC_QUIC_NODE_QUIC_UTIL_H_
#define SRC_QUIC_NODE_QUIC_UTIL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node.h"
#include "node_sockaddr.h"
#include "uv.h"
#include "v8.h"
#include "histogram.h"
#include "memory_tracker.h"

#include <ngtcp2/ngtcp2.h>
#include <openssl/ssl.h>

#include <algorithm>
#include <functional>
#include <limits>
#include <string>
#include <unordered_map>

namespace node {
namespace quic {

// k-constants are used internally, all-caps constants
// are exposed to javascript as constants (see node_quic.cc)

constexpr size_t kMaxSizeT = std::numeric_limits<size_t>::max();
constexpr size_t kMaxValidateAddressLru = 10;
constexpr size_t kMinInitialQuicPktSize = 1200;
constexpr size_t kScidLen = NGTCP2_MAX_CIDLEN;
constexpr size_t kTokenRandLen = 16;
constexpr size_t kTokenSecretLen = 16;

constexpr uint64_t DEFAULT_MAX_CONNECTIONS =
    std::min<uint64_t>(kMaxSizeT, kMaxSafeJsInteger);
constexpr uint64_t DEFAULT_MAX_CONNECTIONS_PER_HOST = 100;
constexpr uint64_t NGTCP2_APP_NOERROR = 0xff00;
constexpr uint64_t MIN_RETRYTOKEN_EXPIRATION = 1;
constexpr uint64_t MAX_RETRYTOKEN_EXPIRATION = 60;
constexpr uint64_t DEFAULT_ACTIVE_CONNECTION_ID_LIMIT = 2;
constexpr uint64_t DEFAULT_MAX_STREAM_DATA_BIDI_LOCAL = 256 * 1024;
constexpr uint64_t DEFAULT_MAX_STREAM_DATA_BIDI_REMOTE = 256 * 1024;
constexpr uint64_t DEFAULT_MAX_STREAM_DATA_UNI = 256 * 1024;
constexpr uint64_t DEFAULT_MAX_DATA = 1 * 1024 * 1024;
constexpr uint64_t DEFAULT_MAX_STATELESS_RESETS_PER_HOST = 10;
constexpr uint64_t DEFAULT_MAX_STREAMS_BIDI = 100;
constexpr uint64_t DEFAULT_MAX_STREAMS_UNI = 3;
constexpr uint64_t DEFAULT_MAX_IDLE_TIMEOUT = 10;
constexpr uint64_t DEFAULT_RETRYTOKEN_EXPIRATION = 10ULL;

constexpr int ERR_FAILED_TO_CREATE_SESSION = -1;

// The preferred address policy determines how a client QuicSession
// handles a server-advertised preferred address. As suggested, the
// preferred address is the address the server would prefer the
// client to use for subsequent communication for a QuicSession.
// The client may choose to ignore the preference but really shouldn't.
// We currently only support two options in the Node.js implementation,
// but additional options may be added later.
enum SelectPreferredAddressPolicy : int {
  // Ignore the server-provided preferred address
  QUIC_PREFERRED_ADDRESS_IGNORE,
  // Accept the server-provided preferred address
  QUIC_PREFERRED_ADDRESS_ACCEPT
};

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

template <typename T> class StatsBase;

template <typename T, typename Q>
struct StatsTraits {
  using Stats = T;
  using Base = Q;

  template <typename Fn>
  static void ToString(const Q& ptr, Fn&& add_field);
};

// StatsBase is a utility help for classes (like QuicSession)
// that record performance statistics. The template takes a
// single Traits argument (see QuicStreamStatsTraits in
// node_quic_stream.h as an example). When the StatsBase
// is deconstructed, collected statistics are output to
// Debug automatically.
template <typename T>
class StatsBase {
 public:
  // A StatsBase instance may have one of three histogram
  // instances. One that records rate of data flow, one
  // that records size of data chunk, and one that records
  // rate of data ackwowledgement. These may be used in
  // slightly different ways of different StatsBase
  // instances or may be turned off entirely.
  enum HistogramOptions {
    NONE = 0,
    RATE = 1,
    SIZE = 2,
    ACK = 4
  };

  inline StatsBase(
      Environment* env,
      v8::Local<v8::Object> wrap,
      int options = HistogramOptions::NONE);

  inline ~StatsBase() = default;

  // The StatsDebug utility is used when StatsBase is destroyed
  // to output statistical information to Debug. It is designed
  // to only incur a performance cost constructing the debug
  // output when Debug output is enabled.
  struct StatsDebug {
    typename T::Base* ptr;
    explicit StatsDebug(typename T::Base* ptr_) : ptr(ptr_) {}
    std::string ToString() const;
  };

  // Increments the given stat field by the given amount or 1 if
  // no amount is specified.
  inline void IncrementStat(uint64_t T::Stats::*member, uint64_t amount = 1);

  // Sets an entirely new value for the given stat field
  inline void SetStat(uint64_t T::Stats::*member, uint64_t value);

  // Sets the given stat field to the current uv_hrtime()
  inline void RecordTimestamp(uint64_t T::Stats::*member);

  // Gets the current value of the given stat field
  inline uint64_t GetStat(uint64_t T::Stats::*member) const;

  // If the rate histogram is used, records the time elapsed
  // between now and the timestamp specified by the member
  // field.
  inline void RecordRate(uint64_t T::Stats::*member);

  // If the size histogram is used, records the given size.
  inline void RecordSize(uint64_t val);

  // If the ack rate histogram is used, records the time
  // elapsed between now and the timestamp specified by
  // the member field.
  inline void RecordAck(uint64_t T::Stats::*member);

  inline void StatsMemoryInfo(MemoryTracker* tracker) const;

  inline void DebugStats();

 private:
  typename T::Stats stats_{};
  BaseObjectPtr<HistogramBase> rate_;
  BaseObjectPtr<HistogramBase> size_;
  BaseObjectPtr<HistogramBase> ack_;
  AliasedBigUint64Array stats_buffer_;
};

// QuicPreferredAddress is a helper class used only when a
// client QuicSession receives an advertised preferred address
// from a server. The helper provides information about the
// preferred address. The Use() function is used to let
// ngtcp2 know to use the preferred address for the given family.
class QuicPreferredAddress {
 public:
  QuicPreferredAddress(
      Environment* env,
      ngtcp2_addr* dest,
      const ngtcp2_preferred_addr* paddr) :
      env_(env),
      dest_(dest),
      paddr_(paddr) {}

  inline const ngtcp2_cid* cid() const;
  inline std::string preferred_ipv6_address() const;
  inline std::string preferred_ipv4_address() const;
  inline int16_t preferred_ipv6_port() const;
  inline int16_t preferred_ipv4_port() const;
  inline const uint8_t* stateless_reset_token() const;

  inline bool Use(int family = AF_INET) const;

 private:
  inline bool ResolvePreferredAddress(
      int local_address_family,
      uv_getaddrinfo_t* req) const;

  Environment* env_;
  mutable ngtcp2_addr* dest_;
  const ngtcp2_preferred_addr* paddr_;
};

// QuicError is a helper class used to encapsulate basic
// details about a QUIC protocol error. There are three
// basic types of errors (see QuicErrorFamily)
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
  inline const char* family_name();
};

// Helper function that returns the maximum QUIC packet size for
// the given socket address.
inline size_t GetMaxPktLen(const SocketAddress& addr);

// QuicPath is a utility class that wraps ngtcp2_path to adapt
// it to work with SocketAddress
struct QuicPath : public ngtcp2_path {
  inline QuicPath(const SocketAddress& local, const SocketAddress& remote);
};

struct QuicPathStorage : public ngtcp2_path_storage {
  QuicPathStorage() {
    ngtcp2_path_storage_zero(this);
  }
};

// Simple wrapper for ngtcp2_cid that handles hex encoding
class QuicCID : public MemoryRetainer {
 public:
  // Empty constructor
  QuicCID() : ptr_(&cid_) {}

  // Copy constructor
  QuicCID(const QuicCID& cid) : QuicCID(cid->data, cid->datalen) {}

  // Copy constructor
  explicit QuicCID(const ngtcp2_cid& cid) : QuicCID(cid.data, cid.datalen) {}

  // Wrap constructor
  explicit QuicCID(const ngtcp2_cid* cid) : ptr_(cid) {}

  QuicCID(const uint8_t* cid, size_t len) : QuicCID() {
    ngtcp2_cid* ptr = this->cid();
    ngtcp2_cid_init(ptr, cid, len);
    ptr_ = ptr;
  }

  struct Hash {
    inline size_t operator()(const QuicCID& cid) const;
  };

  inline bool operator==(const QuicCID& other) const;
  inline bool operator!=(const QuicCID& other) const;

  inline std::string ToString() const;

  // Copy assignment
  QuicCID& operator=(const QuicCID& cid) {
    if (this == &cid) return *this;
    this->~QuicCID();
    return *new(this) QuicCID(std::move(cid));
  }

  const ngtcp2_cid& operator*() const { return *ptr_; }
  const ngtcp2_cid* operator->() const { return ptr_; }
  const ngtcp2_cid* cid() const { return ptr_; }
  const uint8_t* data() const { return ptr_->data; }
  operator bool() const { return ptr_->datalen > 0; }
  size_t length() const { return ptr_->datalen; }

  ngtcp2_cid* cid() {
    CHECK_EQ(ptr_, &cid_);
    return &cid_;
  }

  unsigned char* data() {
    return reinterpret_cast<unsigned char*>(cid()->data);
  }

  void set_length(size_t length) {
    cid()->datalen = length;
  }

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(QuicCID)
  SET_SELF_SIZE(QuicCID)

  template <typename T>
  using Map = std::unordered_map<QuicCID, T, QuicCID::Hash>;

 private:
  ngtcp2_cid cid_{};
  const ngtcp2_cid* ptr_;
};

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

class StatelessResetToken : public MemoryRetainer {
 public:
  inline StatelessResetToken(
      uint8_t* token,
      const uint8_t* secret,
      const QuicCID& cid);
  inline StatelessResetToken(
      const uint8_t* secret,
      const QuicCID& cid);
  explicit StatelessResetToken(
      const uint8_t* token)
      : token_(token) {}

  inline std::string ToString() const;
  const uint8_t* data() const { return token_; }

  struct Hash {
    inline size_t operator()(const StatelessResetToken& token) const;
  };

  inline bool operator==(const StatelessResetToken& other) const;
  inline bool operator!=(const StatelessResetToken& other) const;

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(StatelessResetToken)
  SET_SELF_SIZE(StatelessResetToken)

  template <typename T>
  using Map =
      std::unordered_map<
          StatelessResetToken,
          BaseObjectPtr<T>,
          StatelessResetToken::Hash>;

 private:
  uint8_t buf_[NGTCP2_STATELESS_RESET_TOKENLEN]{};
  const uint8_t* token_;
};

template <typename T>
inline size_t get_length(const T*, size_t len);

}  // namespace quic
}  // namespace node

#endif  // NOE_WANT_INTERNALS

#endif  // SRC_QUIC_NODE_QUIC_UTIL_H_
