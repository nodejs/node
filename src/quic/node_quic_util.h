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

constexpr uint64_t DEFAULT_ACTIVE_CONNECTION_ID_LIMIT = 2;
constexpr uint64_t DEFAULT_MAX_CONNECTIONS =
    std::min<uint64_t>(kMaxSizeT, kMaxSafeJsInteger);
constexpr uint64_t DEFAULT_MAX_CONNECTIONS_PER_HOST = 100;
constexpr uint64_t DEFAULT_MAX_STREAM_DATA_BIDI_LOCAL = 256 * 1024;
constexpr uint64_t DEFAULT_MAX_STREAM_DATA_BIDI_REMOTE = 256 * 1024;
constexpr uint64_t DEFAULT_MAX_STREAM_DATA_UNI = 256 * 1024;
constexpr uint64_t DEFAULT_MAX_DATA = 1 * 1024 * 1024;
constexpr uint64_t DEFAULT_MAX_STATELESS_RESETS_PER_HOST = 10;
constexpr uint64_t DEFAULT_MAX_STREAMS_BIDI = 100;
constexpr uint64_t DEFAULT_MAX_STREAMS_UNI = 3;
constexpr uint64_t DEFAULT_MAX_IDLE_TIMEOUT = 10;
constexpr uint64_t DEFAULT_RETRYTOKEN_EXPIRATION = 10;
constexpr uint64_t MIN_RETRYTOKEN_EXPIRATION = 1;
constexpr uint64_t MAX_RETRYTOKEN_EXPIRATION = 60;
constexpr uint64_t NGTCP2_APP_NOERROR = 0xff00;

constexpr int ERR_FAILED_TO_CREATE_SESSION = -1;

// The preferred address policy determines how a client QuicSession
// handles a server-advertised preferred address. As suggested, the
// preferred address is the address the server would prefer the
// client to use for subsequent communication for a QuicSession.
// The client may choose to ignore the preference but really shouldn't
// without good reason. We currently only support two options but
// additional options may be added later.
enum SelectPreferredAddressPolicy : int {
  // Ignore the server-provided preferred address
  QUIC_PREFERRED_ADDRESS_IGNORE,

  // Use the server-provided preferred address.
  // With this policy in effect, when a client
  // receives a preferred address from the server,
  // the client QuicSession will be automatically
  // switched to use the selected address if it
  // matches the current local address family.
  QUIC_PREFERRED_ADDRESS_USE
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
  typedef typename T::Stats Stats;

  // A StatsBase instance may have one of three histogram
  // instances. One that records rate of data flow, one
  // that records size of data chunk, and one that records
  // rate of data acknowledgement. These may be used in
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

  inline ~StatsBase() { if (stats_ != nullptr) stats_->~Stats(); }

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
  inline void IncrementStat(uint64_t Stats::*member, uint64_t amount = 1);

  // Sets an entirely new value for the given stat field
  inline void SetStat(uint64_t Stats::*member, uint64_t value);

  // Sets the given stat field to the current uv_hrtime()
  inline void RecordTimestamp(uint64_t Stats::*member);

  // Gets the current value of the given stat field
  inline uint64_t GetStat(uint64_t Stats::*member) const;

  // If the rate histogram is used, records the time elapsed
  // between now and the timestamp specified by the member
  // field.
  inline void RecordRate(uint64_t Stats::*member);

  // If the size histogram is used, records the given size.
  inline void RecordSize(uint64_t val);

  // If the ack rate histogram is used, records the time
  // elapsed between now and the timestamp specified by
  // the member field.
  inline void RecordAck(uint64_t Stats::*member);

  inline void StatsMemoryInfo(MemoryTracker* tracker) const;

  inline void DebugStats();

 private:
  BaseObjectPtr<HistogramBase> rate_;
  BaseObjectPtr<HistogramBase> size_;
  BaseObjectPtr<HistogramBase> ack_;
  std::shared_ptr<v8::BackingStore> stats_store_;
  Stats* stats_ = nullptr;
};

// PreferredAddress is a helper class used only when a client QuicSession
// receives an advertised preferred address from a server. The helper provides
// information about the preferred address. The Use() function is used to let
// ngtcp2 know to use the preferred address for the given family.
class PreferredAddress {
 public:
  PreferredAddress(
      Environment* env,
      ngtcp2_addr* dest,
      const ngtcp2_preferred_addr* paddr) :
      env_(env),
      dest_(dest),
      paddr_(paddr) {}

  // When a preferred address is advertised by a server, the
  // advertisement also includes a new CID and (optionally)
  // a stateless reset token. If the preferred address is
  // selected, then the client QuicSession will make use of
  // these new values. Access to the cid and reset token
  // are provided via the PreferredAddress class only as a
  // convenience.
  inline const ngtcp2_cid* cid() const;

  // The stateless reset token associated with the preferred
  // address CID
  inline const uint8_t* stateless_reset_token() const;

  // A preferred address advertisement may include both an
  // IPv4 and IPv6 address. Only one of which will be used.

  inline std::string ipv4_address() const;

  inline uint16_t ipv4_port() const;

  inline std::string ipv6_address() const;

  inline uint16_t ipv6_port() const;

  // Instructs the QuicSession to use the advertised
  // preferred address matching the given family. If
  // the advertisement does not include a matching
  // address, the preferred address is ignored.
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
// CIDs are used to identify QuicSession instances and may
// be between 0 and 20 bytes in length.
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
  inline QuicCID& operator=(const QuicCID& cid);
  const ngtcp2_cid& operator*() const { return *ptr_; }
  const ngtcp2_cid* operator->() const { return ptr_; }

  inline std::string ToString() const;

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

// A Stateless Reset Token is a mechanism by which a QUIC
// endpoint can discreetly signal to a peer that it has
// lost all state associated with a connection. This
// helper class is used to both store received tokens and
// provide storage when creating new tokens to send.
class StatelessResetToken : public MemoryRetainer {
 public:
  inline StatelessResetToken(
      uint8_t* token,
      const uint8_t* secret,
      const QuicCID& cid);

  inline StatelessResetToken(
      const uint8_t* secret,
      const QuicCID& cid);

  explicit inline StatelessResetToken(
      const uint8_t* token);

  inline std::string ToString() const;

  const uint8_t* data() const { return buf_; }

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
};

template <typename T>
inline size_t get_length(const T*, size_t len);

}  // namespace quic
}  // namespace node

#endif  // NOE_WANT_INTERNALS

#endif  // SRC_QUIC_NODE_QUIC_UTIL_H_
