#ifndef SRC_QUIC_QUIC_H_
#define SRC_QUIC_QUIC_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base_object.h"
#include "env.h"
#include "node_mem.h"
#include "node_sockaddr.h"
#include "string_bytes.h"
#include "util.h"

#include "nghttp3/nghttp3.h"
#include "ngtcp2/ngtcp2.h"
#include <ngtcp2/ngtcp2_crypto.h>
#include "uv.h"
#include "v8.h"

#include <algorithm>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace node {
namespace quic {

class BindingState;
class Session;

using QuicConnectionPointer = DeleteFnPtr<ngtcp2_conn, ngtcp2_conn_del>;
using QuicMemoryManager = mem::NgLibMemoryManager<BindingState, ngtcp2_mem>;

using stream_id = int64_t;
using error_code = uint64_t;
using quic_version = uint32_t;

// The constants prefixed with k are used internally.
// The constants in all caps snake case are exported to the JavaScript binding.

// NGTCP2_MAX_PKTLEN_IPV4 should always be larger, but we check just in case.
constexpr size_t kDefaultMaxPacketLength =
    std::max<size_t>(NGTCP2_MAX_PKTLEN_IPV4, NGTCP2_MAX_PKTLEN_IPV6);
constexpr size_t kMaxSizeT = std::numeric_limits<size_t>::max();
constexpr size_t kTokenSecretLen = 16;
constexpr size_t kMaxDynamicServerIDLength = 7;
constexpr size_t kMinDynamicServerIDLength = 1;
constexpr uint64_t kSocketAddressInfoTimeout = 10000000000;  // 10 seconds

constexpr uint64_t DEFAULT_ACTIVE_CONNECTION_ID_LIMIT = 2;
constexpr uint64_t DEFAULT_MAX_STREAM_DATA_BIDI_LOCAL = 256 * 1024;
constexpr uint64_t DEFAULT_MAX_STREAM_DATA_BIDI_REMOTE = 256 * 1024;
constexpr uint64_t DEFAULT_MAX_STREAM_DATA_UNI = 256 * 1024;
constexpr uint64_t DEFAULT_MAX_STREAMS_BIDI = 100;
constexpr uint64_t DEFAULT_MAX_STREAMS_UNI = 3;
constexpr uint64_t DEFAULT_MAX_DATA = 1 * 1024 * 1024;
constexpr uint64_t DEFAULT_MAX_IDLE_TIMEOUT = 10;
constexpr size_t DEFAULT_MAX_CONNECTIONS =
    std::min<size_t>(
        kMaxSizeT,
        static_cast<size_t>(kMaxSafeJsInteger));
constexpr size_t DEFAULT_MAX_CONNECTIONS_PER_HOST = 100;
constexpr size_t DEFAULT_MAX_SOCKETADDRESS_LRU_SIZE =
   (DEFAULT_MAX_CONNECTIONS_PER_HOST * 10);
constexpr size_t DEFAULT_MAX_STATELESS_RESETS = 10;
constexpr size_t DEFAULT_MAX_RETRY_LIMIT = 10;
constexpr uint64_t DEFAULT_RETRYTOKEN_EXPIRATION = 10;
constexpr uint64_t DEFAULT_TOKEN_EXPIRATION = 3600;
constexpr uint64_t NGTCP2_APP_NOERROR = 65280;

// The constructors are v8::FunctionTemplates that are stored
// persistently in the quic::BindingState class. These are
// used for creating instances of the various objects, as well
// as for performing HasInstance type checks. We choose to
// store these on the BindingData instead of the Environment
// in order to keep like-things together and to reduce the
// additional memory overhead on the Environment when QUIC is
// not being used.
#define QUIC_CONSTRUCTORS(V)                                                   \
  V(arraybufferviewsource)                                                     \
  V(blobsource)                                                                \
  V(endpoint)                                                                  \
  V(endpoint_config)                                                           \
  V(http3_options)                                                             \
  V(jsquicbufferconsumer)                                                      \
  V(logstream)                                                                 \
  V(nullsource)                                                                \
  V(random_connection_id_strategy)                                             \
  V(send_wrap)                                                                 \
  V(session)                                                                   \
  V(session_options)                                                           \
  V(stream)                                                                    \
  V(streamsource)                                                              \
  V(streambasesource)                                                          \
  V(udp)

// The callbacks are persistent v8::Function references that
// are set in the quic::BindingState used to communicate data
// and events back out to the JS environment. They are set once
// from the JavaScript side when the internalBinding('quic') is
// first loaded.
//
// The corresponding implementations of the callbacks can be
// found in lib/internal/quic/binding.js
#define QUIC_JS_CALLBACKS(V)                                                   \
  V(endpoint_done, EndpointDone)                                               \
  V(endpoint_error, EndpointError)                                             \
  V(session_new, SessionNew)                                                   \
  V(session_client_hello, SessionClientHello)                                  \
  V(session_close, SessionClose)                                               \
  V(session_datagram, SessionDatagram)                                         \
  V(session_handshake, SessionHandshake)                                       \
  V(session_ocsp_request, SessionOcspRequest)                                  \
  V(session_ocsp_response, SessionOcspResponse)                                \
  V(session_ticket, SessionTicket)                                             \
  V(session_version_negotiation, SessionVersionNegotiation)                    \
  V(stream_close, StreamClose)                                                 \
  V(stream_created, StreamCreated)                                             \
  V(stream_reset, StreamReset)                                                 \
  V(stream_headers, StreamHeaders)                                             \
  V(stream_blocked, StreamBlocked)                                             \
  V(stream_trailers, StreamTrailers)

// The strings are persistent/eternal v8::Strings that are set in
// the quic::BindingState.
#define QUIC_STRINGS(V)                                                        \
  V(http3_alpn, &NGHTTP3_ALPN_H3[1])                                           \
  V(ack_delay_exponent, "ackDelayExponent")                                    \
  V(active_connection_id_limit, "activeConnectionIdLimit")                     \
  V(address_lru_size, "addressLRUSize")                                        \
  V(cc_algorithm, "ccAlgorithm")                                               \
  V(client_hello, "clientHello")                                               \
  V(disable_active_migration, "disableActiveMigration")                        \
  V(disable_stateless_reset, "disableStatelessReset")                          \
  V(enable_tls_trace, "enableTLSTrace")                                        \
  V(err_stream_closed, "ERR_QUIC_STREAM_CLOSED")                               \
  V(initial_max_stream_data_bidi_local, "initialMaxStreamDataBidiLocal")       \
  V(initial_max_stream_data_bidi_remote, "initialMaxStreamDataBidiRemote")     \
  V(initial_max_stream_data_uni, "initialMaxStreamDataUni")                    \
  V(initial_max_data, "initialMaxData")                                        \
  V(initial_max_streams_bidi, "initialMaxStreamsBidi")                         \
  V(initial_max_streams_uni, "initialMaxStreamsUni")                           \
  V(ipv6_only, "ipv6Only")                                                     \
  V(keylog, "keylog")                                                          \
  V(max_ack_delay, "maxAckDelay")                                              \
  V(max_connections_per_host, "maxConnectionsPerHost")                         \
  V(max_connections_total, "maxConnectionsTotal")                              \
  V(max_datagram_frame_size, "maxDatagramFrameSize")                           \
  V(max_field_section_size, "maxFieldSectionSize")                             \
  V(max_header_pairs, "maxHeaderPairs")                                        \
  V(max_header_length, "maxHeaderLength")                                      \
  V(max_idle_timeout, "maxIdleTimeout")                                        \
  V(max_payload_size, "maxPayloadSize")                                        \
  V(max_pushes, "maxPushes")                                                   \
  V(max_stateless_resets, "maxStatelessResets")                                \
  V(max_stream_window_override, "maxStreamWindowOverride")                     \
  V(max_window_override, "maxWindowOverride")                                  \
  V(ocsp, "ocsp")                                                              \
  V(pskcallback, "pskCallback")                                                \
  V(qlog, "qlog")                                                              \
  V(qpack_blocked_streams, "qpackBlockedStreams")                              \
  V(qpack_max_table_capacity, "qpackMaxTableCapacity")                         \
  V(reject_unauthorized, "rejectUnauthorized")                                 \
  V(request_peer_certificate, "requestPeerCertificate")                        \
  V(retry_limit, "retryLimit")                                                 \
  V(retry_token_expiration, "retryTokenExpiration")                            \
  V(rx_packet_loss, "rxPacketLoss")                                            \
  V(token_expiration, "tokenExpiration")                                       \
  V(tx_packet_loss, "txPacketLoss")                                            \
  V(udp_receive_buffer_size, "receiveBufferSize")                              \
  V(udp_send_buffer_size, "sendBufferSize")                                    \
  V(udp_ttl, "ttl")                                                            \
  V(unacknowledged_packet_threshold, "unacknowledgedPacketThreshold")          \
  V(verify_hostname_identity, "verifyHostnameIdentity")                        \
  V(validate_address, "validateAddress")

class BindingState final : public BaseObject,
                           public QuicMemoryManager {
 public:
  static bool Initialize(Environment* env, v8::Local<v8::Object> target);

  static BindingState* Get(Environment* env);

  static ngtcp2_mem GetAllocator(Environment* env);

  static nghttp3_mem GetHttp3Allocator(Environment* env);

  static constexpr FastStringKey type_name { "quic" };
  BindingState(Environment* env, v8::Local<v8::Object> object);

  BindingState(const BindingState&) = delete;
  BindingState(const BindingState&&) = delete;
  BindingState& operator=(const BindingState&) = delete;
  BindingState& operator=(const BindingState&&) = delete;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(BindingState);
  SET_SELF_SIZE(BindingState);

  // NgLibMemoryManager (QuicMemoryManager)
  void CheckAllocatedSize(size_t previous_size) const;
  void IncreaseAllocatedSize(size_t size);
  void DecreaseAllocatedSize(size_t size);

  bool warn_trace_tls = true;
  bool initialized = false;

#define V(name)                                                               \
  void set_ ## name ## _constructor_template(                                 \
      v8::Local<v8::FunctionTemplate> tmpl);                                  \
  v8::Local<v8::FunctionTemplate> name ## _constructor_template() const;
  QUIC_CONSTRUCTORS(V)
#undef V

#define V(name, _)                                                             \
  void set_ ## name ## _callback(v8::Local<v8::Function> fn);                  \
  v8::Local<v8::Function> name ## _callback() const;
  QUIC_JS_CALLBACKS(V)
#undef V

#define V(name, _) v8::Local<v8::String> name ## _string() const;
  QUIC_STRINGS(V)
#undef V

 private:
  size_t current_ngtcp2_memory_ = 0;

#define V(name)                                                                \
  v8::Global<v8::FunctionTemplate> name ## _constructor_template_;
  QUIC_CONSTRUCTORS(V)
#undef V

#define V(name, _) v8::Global<v8::Function> name ## _callback_;
  QUIC_JS_CALLBACKS(V)
#undef V

#define V(name, _) mutable v8::Eternal<v8::String> name ## _string_;
  QUIC_STRINGS(V)
#undef V
};

// CIDs are used to identify endpoints participating in a QUIC session
class CID final : public MemoryRetainer {
 public:
  inline CID() : ptr_(&cid_) {}

  inline CID(const CID& cid) noexcept : CID(cid->data, cid->datalen) {}

  inline explicit CID(const ngtcp2_cid& cid) : CID(cid.data, cid.datalen) {}

  inline explicit CID(const ngtcp2_cid* cid) : ptr_(cid) {}

  inline CID(const uint8_t* cid, size_t len) : CID() {
    ngtcp2_cid* ptr = this->cid();
    ngtcp2_cid_init(ptr, cid, len);
    ptr_ = ptr;
  }

  CID(CID&&cid) = delete;

  struct Hash final {
    inline size_t operator()(const CID& cid) const {
      size_t hash = 0;
      for (size_t n = 0; n < cid->datalen; n++) {
        hash ^= std::hash<uint8_t>{}(cid->data[n]) + 0x9e3779b9 +
                (hash << 6) + (hash >> 2);
      }
      return hash;
    }
  };

  inline bool operator==(const CID& other) const noexcept {
    return memcmp(cid()->data, other.cid()->data, cid()->datalen) == 0;
  }

  inline bool operator!=(const CID& other) const noexcept {
    return !(*this == other);
  }

  inline CID& operator=(const CID& cid) noexcept {
    if (this == &cid) return *this;
    this->~CID();
    return *new(this) CID(cid);
  }

  inline const ngtcp2_cid& operator*() const { return *ptr_; }

  inline const ngtcp2_cid* operator->() const { return ptr_; }

  inline std::string ToString() const {
    std::vector<char> dest(ptr_->datalen * 2 + 1);
    dest[dest.size() - 1] = '\0';
    size_t written = StringBytes::hex_encode(
        reinterpret_cast<const char*>(ptr_->data),
        ptr_->datalen,
        dest.data(),
        dest.size());
    return std::string(dest.data(), written);
  }

  inline const ngtcp2_cid* cid() const { return ptr_; }

  inline const uint8_t* data() const { return ptr_->data; }

  inline operator bool() const { return ptr_->datalen > 0; }

  inline size_t length() const { return ptr_->datalen; }

  inline ngtcp2_cid* cid() {
    CHECK_EQ(ptr_, &cid_);
    return &cid_;
  }

  inline unsigned char* data() {
    return reinterpret_cast<unsigned char*>(cid()->data);
  }

  inline void set_length(size_t length) {
    cid()->datalen = length;
  }

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(CID)
  SET_SELF_SIZE(CID)

  template <typename T> using Map = std::unordered_map<CID, T, CID::Hash>;

 private:
  ngtcp2_cid cid_{};
  const ngtcp2_cid* ptr_;
};

// A serialized QUIC packet. Packets are transient. They are created, filled
// with the contents of a serialized packet, and passed off immediately to the
// Endpoint to be sent. As soon as the packet is sent, it is freed.
class Packet final : public MemoryRetainer {
 public:
  inline static std::unique_ptr<Packet> Copy(
      const std::unique_ptr<Packet>& other) {
    return std::make_unique<Packet>(*other.get());
  }

  // The diagnostic_label is a debugging utility that is printed when debug
  // output is enabled. It allows differentiated between different types of
  // packets sent for different reasons since the packets themselves are
  // opaque and often encrypted.
  inline explicit Packet(const char* diagnostic_label = nullptr)
      : ptr_(data_),
        diagnostic_label_(diagnostic_label) { }

  inline Packet(size_t len, const char* diagnostic_label = nullptr)
      : ptr_(len <= kDefaultMaxPacketLength ? data_ : Malloc<uint8_t>(len)),
        len_(len),
        diagnostic_label_(diagnostic_label) {
    CHECK_GT(len_, 0);
    CHECK_NOT_NULL(ptr_);
  }

  inline Packet(const Packet& other) noexcept
      : Packet(other.len_, other.diagnostic_label_) {
    if (UNLIKELY(len_ == 0)) return;
    memcpy(ptr_, other.ptr_, len_);
  }

  Packet(Packet&& other) = delete;
  Packet& operator=(Packet&& other) = delete;

  inline ~Packet() {
    if (ptr_ != data_)
      std::unique_ptr<uint8_t> free_me(ptr_);
  }

  inline Packet& operator=(const Packet& other) noexcept {
    if (this == &other) return *this;
    this->~Packet();
    return *new(this) Packet(other);
  }

  inline uint8_t* data() { return ptr_; }

  inline size_t length() const { return len_; }

  inline uv_buf_t buf() const {
    return uv_buf_init(reinterpret_cast<char*>(ptr_), len_);
  }

  inline void set_length(size_t len) {
    CHECK_LE(len, len_);
    len_ = len;
  }

  inline const char* diagnostic_label() const {
    return diagnostic_label_;
  }

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(Packet);
  SET_SELF_SIZE(Packet);

 private:
  uint8_t data_[kDefaultMaxPacketLength];
  uint8_t* ptr_ = nullptr;
  size_t len_ = kDefaultMaxPacketLength;
  const char* diagnostic_label_ = nullptr;
};

// A utility class that wraps ngtcp2_path to adapt it to work with SocketAddress
struct Path final : public ngtcp2_path {
  Path(
      const std::shared_ptr<SocketAddress>& local,
      const std::shared_ptr<SocketAddress>& remote);
};

struct PathStorage final : public ngtcp2_path_storage {
  inline PathStorage() { ngtcp2_path_storage_zero(this); }
};

// PreferredAddress is a helper class used only when a client Session
// receives an advertised preferred address from a server. The helper provides
// information about the servers advertised preferred address. Call Use()
// to let ngtcp2 know which preferred address to use (if any).
class PreferredAddress final {
 public:
  enum class Policy {
    IGNORE_PREFERED,
    USE
  };

  struct Address final {
    int family;
    uint16_t port;
    std::string address;
  };

  inline PreferredAddress(
      Environment* env,
      ngtcp2_addr* dest,
      const ngtcp2_preferred_addr* paddr)
      : env_(env),
        dest_(dest),
        paddr_(paddr) {}

  PreferredAddress(const PreferredAddress& other) = delete;
  PreferredAddress(PreferredAddress&& other) = delete;
  PreferredAddress* operator=(const PreferredAddress& other) = delete;
  PreferredAddress* operator=(PreferredAddress&& other) = delete;

  // When a preferred address is advertised by a server, the
  // advertisement also includes a new CID and (optionally)
  // a stateless reset token. If the preferred address is
  // selected, then the client Session will make use of
  // these new values. Access to the cid and reset token
  // are provided via the PreferredAddress class only as a
  // convenience.
  inline const ngtcp2_cid* cid() const {
    return &paddr_->cid;
  }

  // The stateless reset token associated with the preferred address CID
  inline const uint8_t* stateless_reset_token() const {
    return paddr_->stateless_reset_token;
  }

  // A preferred address advertisement may include both an
  // IPv4 and IPv6 address. Only one of which will be used.

  Address ipv4() const;

  Address ipv6() const;

  // Instructs the Session to use the advertised
  // preferred address matching the given family. If
  // the advertisement does not include a matching
  // address, the preferred address is ignored. If
  // the given address cannot be successfully resolved
  // using uv_getaddrinfo it is ignored.
  bool Use(const Address& address) const;

  void CopyToTransportParams(
      ngtcp2_transport_params* params,
      const sockaddr* addr);

 private:
  inline bool Resolve(const Address& address, uv_getaddrinfo_t* req) const;

  Environment* env_;
  mutable ngtcp2_addr* dest_;
  const ngtcp2_preferred_addr* paddr_;
};

// Encapsulates a QUIC Error. QUIC makes a distinction between Transport and
// Application error codes but allows the error code values to overlap. That
// is, a Transport error and Application error can both use code 1 to mean
// entirely different things.
struct QuicError final {
  enum class Type {
    TRANSPORT,
    APPLICATION
  };
  Type type;
  error_code code;

  bool operator==(const QuicError& other) const noexcept {
    return type == other.type && code == other.code;
  }

  std::string ToString() const {
    return std::to_string(code) + " (" + TypeName(*this) + ")";
  }

  static inline QuicError FromNgtcp2(
      ngtcp2_connection_close_error_code close_code) {
    switch (close_code.type) {
      case NGTCP2_CONNECTION_CLOSE_ERROR_CODE_TYPE_TRANSPORT:
        return QuicError { Type::TRANSPORT, close_code.error_code };
      case NGTCP2_CONNECTION_CLOSE_ERROR_CODE_TYPE_APPLICATION:
        return QuicError { Type::APPLICATION, close_code.error_code };
      default:
        UNREACHABLE();
    }
  }

  static inline const char* TypeName(QuicError error) {
    switch (error.type) {
      case Type::TRANSPORT: return "transport";
      case Type::APPLICATION: return "application";
      default: UNREACHABLE();
    }
  }
};

static constexpr QuicError kQuicNoError =
    QuicError { QuicError::Type::TRANSPORT, NGTCP2_NO_ERROR };

static constexpr QuicError kQuicInternalError =
    QuicError { QuicError::Type::TRANSPORT, NGTCP2_INTERNAL_ERROR };

static constexpr QuicError kQuicAppNoError =
    QuicError { QuicError::Type::APPLICATION, NGTCP2_APP_NOERROR };

// A Stateless Reset Token is a mechanism by which a QUIC
// endpoint can discreetly signal to a peer that it has
// lost all state associated with a connection. This
// helper class is used to both store received tokens and
// provide storage when creating new tokens to send.
class StatelessResetToken final : public MemoryRetainer {
 public:
  StatelessResetToken(
      uint8_t* token,
      const uint8_t* secret,
      const CID& cid);

  StatelessResetToken(const uint8_t* secret, const CID& cid);

  explicit inline StatelessResetToken(const uint8_t* token) {
    memcpy(buf_, token, sizeof(buf_));
  }

  StatelessResetToken(const StatelessResetToken& other)
      : StatelessResetToken(other.buf_) {}

  StatelessResetToken(StatelessResetToken&& other) = delete;
  StatelessResetToken& operator=(StatelessResetToken&& other) = delete;

  StatelessResetToken& operator=(const StatelessResetToken& other) {
    if (this == &other) return *this;
    this->~StatelessResetToken();
    return *new(this) StatelessResetToken(other);
  }

  inline std::string ToString() const {
    std::vector<char> dest(NGTCP2_STATELESS_RESET_TOKENLEN * 2 + 1);
    dest[dest.size() - 1] = '\0';
    size_t written = StringBytes::hex_encode(
        reinterpret_cast<const char*>(buf_),
        NGTCP2_STATELESS_RESET_TOKENLEN,
        dest.data(),
        dest.size());
    return std::string(dest.data(), written);
  }

  inline const uint8_t* data() const { return buf_; }

  struct Hash {
    inline size_t operator()(const StatelessResetToken& token) const {
      size_t hash = 0;
      for (size_t n = 0; n < NGTCP2_STATELESS_RESET_TOKENLEN; n++)
        hash ^= std::hash<uint8_t>{}(token.buf_[n]) + 0x9e3779b9 +
                (hash << 6) + (hash >> 2);
      return hash;
    }
  };

  inline bool operator==(const StatelessResetToken& other) const {
    return memcmp(data(), other.data(), NGTCP2_STATELESS_RESET_TOKENLEN) == 0;
  }

  inline bool operator!=(const StatelessResetToken& other) const {
    return !(*this == other);
  }

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(StatelessResetToken)
  SET_SELF_SIZE(StatelessResetToken)

  template <typename T>
  using Map =
      std::unordered_map<
          StatelessResetToken, T,
          StatelessResetToken::Hash>;

 private:
  uint8_t buf_[NGTCP2_STATELESS_RESET_TOKENLEN]{};
};

// The https://tools.ietf.org/html/draft-ietf-quic-load-balancers-06
// specification defines a model for creation of Connection Identifiers
// (CIDs) that embed information usable by load balancers for intelligently
// routing QUIC packets and sessions.
class RoutableConnectionIDStrategy {
 public:
  virtual ~RoutableConnectionIDStrategy() {}

  virtual void NewConnectionID(
      ngtcp2_cid* cid,
      size_t length_hint = NGTCP2_MAX_CIDLEN) = 0;

  inline void NewConnectionID(
      CID* cid,
      size_t length_hint = NGTCP2_MAX_CIDLEN) {
    NewConnectionID(cid->cid(), length_hint);
  }

  virtual void UpdateCIDState(const CID& cid) = 0;
};

template <typename Traits>
class RoutableConnectionIDStrategyImpl final
    : public RoutableConnectionIDStrategy,
      public MemoryRetainer {
 public:
  using Options = typename Traits::Options;
  using State = typename Traits::State;

  RoutableConnectionIDStrategyImpl(
      Session* session,
      const Options& options)
      : session_(session),
        options_(options) {}

  void NewConnectionID(
      ngtcp2_cid* cid,
      size_t length_hint = NGTCP2_MAX_CIDLEN) override {
    Traits::NewConnectionID(
        options_,
        &state_,
        session_,
        cid,
        length_hint);
  }

  void UpdateCIDState(const CID& cid) override {
    Traits::UpdateCIDState(options_, &state_, session_, cid);
  }

  const Options& options() const { return options_; }

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(RoutableConnectionIDStrategyImpl)
  SET_SELF_SIZE(RoutableConnectionIDStrategyImpl<Traits>)

 private:
  Session* session_;
  const Options options_;
  State state_;
};

class RoutableConnectionIDConfig {
 public:
  virtual std::unique_ptr<RoutableConnectionIDStrategy> NewInstance(
      Session* session) = 0;
};

template <typename Traits>
class RoutableConnectionIDConfigImpl final
    : public RoutableConnectionIDConfig,
      public MemoryRetainer {
 public:
  using Options = typename Traits::Options;

  std::unique_ptr<RoutableConnectionIDStrategy> NewInstance(
      Session* session) override {
    return std::make_unique<RoutableConnectionIDStrategyImpl<Traits>>(
        session, options());
  }

  Options& operator*() { return options_; }
  Options* operator->() { return &options_; }
  const Options& options() const { return options_; }

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(RoutableConnectionIDConfig)
  SET_SELF_SIZE(RoutableConnectionIDConfigImpl<Traits>)

 private:
  Options options_;
};

struct RandomConnectionIDTraits final {
  struct Options final {};
  struct State final {};

  static void NewConnectionID(
      const Options& options,
      State* state,
      Session* session,
      ngtcp2_cid* cid,
      size_t length_hint);

  static void UpdateCIDState(
      const Options& options,
      State* state,
      Session* session,
      const CID& cid) {}

  static constexpr const char* name = "RandomConnectionIDStrategy";

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);

  static bool HasInstance(Environment* env, v8::Local<v8::Value> value);
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
};

template <typename Traits>
class RoutableConnectionIDStrategyBase final : public BaseObject {
 public:
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env) {
    return Traits::GetConstructorTemplate(env);
  }

  static bool HasInstance(Environment* env, const v8::Local<v8::Value>& value) {
    return GetConstructorTemplate(env)->HasInstance(value);
  }

  static void Initialize(Environment* env, v8::Local<v8::Object> target) {
    env->SetConstructorFunction(
        target,
        Traits::name,
        GetConstructorTemplate(env));
  }

  RoutableConnectionIDStrategyBase(
      Environment* env,
      v8::Local<v8::Object> object,
      std::shared_ptr<RoutableConnectionIDConfigImpl<Traits>> strategy =
          std::make_shared<RoutableConnectionIDConfigImpl<Traits>>())
      : BaseObject(env, object),
        strategy_(std::move(strategy)) {
    MakeWeak();
  }

  RoutableConnectionIDConfig* strategy() const {
    return strategy_.get();
  }

  void MemoryInfo(MemoryTracker* tracker) const override {
    tracker->TrackField("strategy", strategy_);
  }

  SET_MEMORY_INFO_NAME(RoutableConnectionIDStrategyBase)
  SET_SELF_SIZE(RoutableConnectionIDStrategyBase<Traits>)

  class TransferData final : public worker::TransferData {
   public:
     TransferData(
         std::shared_ptr<RoutableConnectionIDConfigImpl<Traits>> strategy)
         : strategy_(strategy) {}

      BaseObjectPtr<BaseObject> Deserialize(
          Environment* env,
          v8::Local<v8::Context> context,
          std::unique_ptr<worker::TransferData> self) override {
        v8::Local<v8::Object> obj;
        if (!GetConstructorTemplate(env)
                ->InstanceTemplate()
                ->NewInstance(context).ToLocal(&obj)) {
          return BaseObjectPtr<BaseObject>();
        }
        return MakeBaseObject<RoutableConnectionIDStrategyBase<Traits>>(
            env, obj, std::move(strategy_));
      }

      void MemoryInfo(MemoryTracker* tracker) const override {
        tracker->TrackField("strategy", strategy_);
      }

      SET_MEMORY_INFO_NAME(RoutableConnectionIDStrategyBase::TransferData)
      SET_SELF_SIZE(TransferData)

   private:
     std::shared_ptr<RoutableConnectionIDConfigImpl<Traits>> strategy_;
  };

  TransferMode GetTransferMode() const override {
    return TransferMode::kCloneable;
  }

  std::unique_ptr<worker::TransferData> CloneForMessaging() const override {
    return std::make_unique<TransferData>(strategy_);
  }

 private:
  std::shared_ptr<RoutableConnectionIDConfigImpl<Traits>> strategy_;
};

using RandomConnectionIDBase =
    RoutableConnectionIDStrategyBase<RandomConnectionIDTraits>;

using RandomConnectionIDConfig =
    RoutableConnectionIDConfigImpl<RandomConnectionIDTraits>;

void IllegalConstructor(const v8::FunctionCallbackInfo<v8::Value>& args);

}  // namespace quic
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_QUIC_QUIC_H_
