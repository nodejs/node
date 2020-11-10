#ifndef SRC_QUIC_NODE_QUIC_SESSION_H_
#define SRC_QUIC_NODE_QUIC_SESSION_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "aliased_buffer.h"
#include "aliased_struct.h"
#include "async_wrap.h"
#include "env.h"
#include "handle_wrap.h"
#include "node.h"
#include "node_crypto.h"
#include "node_http_common.h"
#include "node_mem.h"
#include "node_quic_state.h"
#include "node_quic_buffer-inl.h"
#include "node_quic_crypto.h"
#include "node_quic_util.h"
#include "node_sockaddr.h"
#include "stream_base.h"
#include "timer_wrap.h"
#include "v8.h"
#include "uv.h"

#include <ngtcp2/ngtcp2.h>
#include <ngtcp2/ngtcp2_crypto.h>
#include <nghttp3/nghttp3.h>
#include <openssl/ssl.h>

#include <unordered_map>
#include <string>
#include <vector>

namespace node {
namespace quic {

using ConnectionPointer = DeleteFnPtr<ngtcp2_conn, ngtcp2_conn_del>;

class QuicApplication;
class QuicPacket;
class QuicSocket;
class QuicStream;

using QuicHeader = NgHeaderBase<QuicApplication>;

using StreamsMap = std::unordered_map<int64_t, BaseObjectPtr<QuicStream>>;

enum class QlogMode {
  kDisabled,
  kEnabled
};

typedef void(*ConnectionIDStrategy)(
    QuicSession* session,
    ngtcp2_cid* cid,
    size_t cidlen);

typedef void(*PreferredAddressStrategy)(
    QuicSession* session,
    const PreferredAddress& preferred_address);

// The QuicSessionConfig class holds the initial transport parameters and
// configuration options set by the JavaScript side when either a
// client or server QuicSession is created. Instances are
// stack created and use a combination of an AliasedBuffer to pass
// the numeric settings quickly (see node_quic_state.h) and passed
// in non-numeric settings (e.g. preferred_addr).
class QuicSessionConfig final : public ngtcp2_settings {
 public:
  QuicSessionConfig() = default;

  explicit QuicSessionConfig(QuicState* quic_state) {
    Set(quic_state);
  }

  QuicSessionConfig(const QuicSessionConfig& config) {
    initial_ts = uv_hrtime();
    transport_params = config.transport_params;
    max_udp_payload_size = config.max_udp_payload_size;
    cc_algo = config.cc_algo;
    cc = config.cc;
    qlog = config.qlog;
    log_printf = config.log_printf;
    token = config.token;
  }

  void ResetToDefaults(QuicState* quic_state);

  // QuicSessionConfig::Set() pulls values out of the AliasedBuffer
  // defined in node_quic_state.h and stores the values in settings_.
  // If preferred_addr is not nullptr, it is copied into the
  // settings_.preferred_addr field
  void Set(QuicState* quic_state,
           const struct sockaddr* preferred_addr = nullptr);

  inline void set_original_connection_id(
      const QuicCID& ocid,
      const QuicCID& scid);

  // Generates the stateless reset token for the settings_
  inline void GenerateStatelessResetToken(
      QuicSession* session,
      const QuicCID& cid);

  // If the preferred address is set, generates the associated tokens
  inline void GeneratePreferredAddressToken(
      ConnectionIDStrategy connection_id_strategy,
      QuicSession* session,
      QuicCID* pscid);

  inline void set_qlog(const ngtcp2_qlog_settings& qlog);
};

// Options to alter the behavior of various functions on the
// server QuicSession. These are set on the QuicSocket when
// the listen() function is called and are passed to the
// constructor of the server QuicSession.
enum QuicServerSessionOptions : uint32_t {
  // When set, instructs the server QuicSession to reject
  // client authentication certs that cannot be verified.
  QUICSERVERSESSION_OPTION_REJECT_UNAUTHORIZED = 0x1,

  // When set, instructs the server QuicSession to request
  // a client authentication cert
  QUICSERVERSESSION_OPTION_REQUEST_CERT = 0x2
};

// Options to alter the behavior of various functions on the
// client QuicSession. These are set on the client QuicSession
// constructor.
enum QuicClientSessionOptions : uint32_t {
  // When set, instructs the client QuicSession to include an
  // OCSP request in the initial TLS handshake
  QUICCLIENTSESSION_OPTION_REQUEST_OCSP = 0x1,

  // When set, instructs the client QuicSession to verify the
  // hostname identity. This is required by QUIC and enabled
  // by default. We allow disabling it only for debugging
  // purposes.
  QUICCLIENTSESSION_OPTION_VERIFY_HOSTNAME_IDENTITY = 0x2,

  // When set, instructs the client QuicSession to perform
  // additional checks on TLS session resumption.
  QUICCLIENTSESSION_OPTION_RESUME = 0x4
};

#define QUICSESSION_SHARED_STATE(V)                                            \
  V(KEYLOG_ENABLED, keylog_enabled, uint8_t)                                   \
  V(CLIENT_HELLO_ENABLED, client_hello_enabled, uint8_t)                       \
  V(OCSP_ENABLED, ocsp_enabled, uint8_t)                                       \
  V(PATH_VALIDATED_ENABLED, path_validated_enabled, uint8_t)                   \
  V(USE_PREFERRED_ADDRESS_ENABLED, use_preferred_address_enabled, uint8_t)     \
  V(HANDSHAKE_CONFIRMED, handshake_confirmed, uint8_t)                         \
  V(IDLE_TIMEOUT, idle_timeout, uint8_t)                                       \
  V(MAX_STREAMS_BIDI, max_streams_bidi, uint64_t)                              \
  V(MAX_STREAMS_UNI, max_streams_uni, uint64_t)                                \
  V(MAX_DATA_LEFT, max_data_left, uint64_t)                                    \
  V(BYTES_IN_FLIGHT, bytes_in_flight, uint64_t)

#define V(_, name, type) type name;
struct QuicSessionState {
  QUICSESSION_SHARED_STATE(V)
};
#undef V

#define V(id, name, _)                                                         \
  IDX_QUICSESSION_STATE_##id = offsetof(QuicSessionState, name),
enum QuicSessionStateFields {
  QUICSESSION_SHARED_STATE(V)
  IDX_QUICSESSION_STATE_END
};
#undef V

#define SESSION_STATS(V)                                                       \
  V(CREATED_AT, created_at, "Created At")                                      \
  V(HANDSHAKE_START_AT, handshake_start_at, "Handshake Started")               \
  V(HANDSHAKE_SEND_AT, handshake_send_at, "Handshke Last Sent")                \
  V(HANDSHAKE_CONTINUE_AT, handshake_continue_at, "Handshke Continued")        \
  V(HANDSHAKE_COMPLETED_AT, handshake_completed_at, "Handshake Completed")     \
  V(HANDSHAKE_CONFIRMED_AT, handshake_confirmed_at, "Handshake Confirmed")     \
  V(HANDSHAKE_ACKED_AT, handshake_acked_at, "Handshake Last Acknowledged")     \
  V(SENT_AT, sent_at, "Last Sent At")                                          \
  V(RECEIVED_AT, received_at, "Last Received At")                              \
  V(CLOSING_AT, closing_at, "Closing")                                         \
  V(DESTROYED_AT, destroyed_at, "Destroyed At")                                \
  V(BYTES_RECEIVED, bytes_received, "Bytes Received")                          \
  V(BYTES_SENT, bytes_sent, "Bytes Sent")                                      \
  V(BIDI_STREAM_COUNT, bidi_stream_count, "Bidi Stream Count")                 \
  V(UNI_STREAM_COUNT, uni_stream_count, "Uni Stream Count")                    \
  V(STREAMS_IN_COUNT, streams_in_count, "Streams In Count")                    \
  V(STREAMS_OUT_COUNT, streams_out_count, "Streams Out Count")                 \
  V(KEYUPDATE_COUNT, keyupdate_count, "Key Update Count")                      \
  V(LOSS_RETRANSMIT_COUNT, loss_retransmit_count, "Loss Retransmit Count")     \
  V(ACK_DELAY_RETRANSMIT_COUNT,                                                \
    ack_delay_retransmit_count,                                                \
    "Ack Delay Retransmit Count")                                              \
  V(PATH_VALIDATION_SUCCESS_COUNT,                                             \
    path_validation_success_count,                                             \
    "Path Validation Success Count")                                           \
  V(PATH_VALIDATION_FAILURE_COUNT,                                             \
    path_validation_failure_count,                                             \
    "Path Validation Failure Count")                                           \
  V(MAX_BYTES_IN_FLIGHT, max_bytes_in_flight, "Max Bytes In Flight")           \
  V(BLOCK_COUNT, block_count, "Block Count")                                   \
  V(MIN_RTT, min_rtt, "Minimum RTT")                                           \
  V(LATEST_RTT, latest_rtt, "Latest RTT")                                      \
  V(SMOOTHED_RTT, smoothed_rtt, "Smoothed RTT")

#define V(name, _, __) IDX_QUIC_SESSION_STATS_##name,
enum QuicSessionStatsIdx : int {
  SESSION_STATS(V)
  IDX_QUIC_SESSION_STATS_COUNT
};
#undef V

#define V(_, name, __) uint64_t name;
struct QuicSessionStats {
  SESSION_STATS(V)
};
#undef V

struct QuicSessionStatsTraits {
  using Stats = QuicSessionStats;
  using Base = QuicSession;

  template <typename Fn>
  static void ToString(const Base& ptr, Fn&& add_field);
};

class QLogStream final : public AsyncWrap,
                         public StreamBase {
 public:
  static BaseObjectPtr<QLogStream> Create(Environment* env);

  QLogStream(Environment* env, v8::Local<v8::Object> obj);

  void Emit(const uint8_t* data, size_t len);

  void End() { ended_ = true; }

  int ReadStart() override { return 0; }
  int ReadStop() override { return 0; }
  int DoShutdown(ShutdownWrap* req_wrap) override;
  int DoWrite(WriteWrap* w,
              uv_buf_t* bufs,
              size_t count,
              uv_stream_t* send_handle) override;
  bool IsAlive() override;
  bool IsClosing() override;
  AsyncWrap* GetAsyncWrap() override { return this; }

  SET_NO_MEMORY_INFO();
  SET_MEMORY_INFO_NAME(QLogStream);
  SET_SELF_SIZE(QLogStream);

 private:
  bool ended_ = false;
};

class QuicSessionListener {
 public:
  enum SessionCloseFlags {
    SESSION_CLOSE_FLAG_NONE,
    SESSION_CLOSE_FLAG_SILENT,
    SESSION_CLOSE_FLAG_STATELESS_RESET
  };

  virtual ~QuicSessionListener();

  virtual void OnKeylog(const char* str, size_t size);
  virtual void OnClientHello(
      const char* alpn,
      const char* server_name);
  virtual void OnCert(const char* server_name);
  virtual void OnOCSP(v8::Local<v8::Value> ocsp);
  virtual void OnStreamHeaders(
      int64_t stream_id,
      int kind,
      const std::vector<std::unique_ptr<QuicHeader>>& headers,
      int64_t push_id);
  virtual void OnStreamClose(
      int64_t stream_id,
      uint64_t app_error_code);
  virtual void OnStreamReset(
      int64_t stream_id,
      uint64_t app_error_code);
  virtual void OnSessionClose(
      QuicError error,
      int flags = SESSION_CLOSE_FLAG_NONE);
  virtual void OnStreamReady(BaseObjectPtr<QuicStream> stream);
  virtual void OnHandshakeCompleted();
  virtual void OnPathValidation(
      ngtcp2_path_validation_result res,
      const sockaddr* local,
      const sockaddr* remote);
  virtual void OnUsePreferredAddress(
      int family,
      const PreferredAddress& preferred_address);
  virtual void OnSessionTicket(int size, SSL_SESSION* session);
  virtual void OnStreamBlocked(int64_t stream_id);
  virtual void OnVersionNegotiation(
      uint32_t supported_version,
      const uint32_t* versions,
      size_t vcnt);
  virtual void OnQLog(QLogStream* qlog_stream);

  QuicSession* session() const { return session_.get(); }

 private:
  BaseObjectWeakPtr<QuicSession> session_;
  QuicSessionListener* previous_listener_ = nullptr;
  friend class QuicSession;
};

class JSQuicSessionListener final : public QuicSessionListener {
 public:
  void OnKeylog(const char* str, size_t size) override;
  void OnClientHello(
      const char* alpn,
      const char* server_name) override;
  void OnCert(const char* server_name) override;
  void OnOCSP(v8::Local<v8::Value> ocsp) override;
  void OnStreamHeaders(
      int64_t stream_id,
      int kind,
      const std::vector<std::unique_ptr<QuicHeader>>& headers,
      int64_t push_id) override;
  void OnStreamClose(
      int64_t stream_id,
      uint64_t app_error_code) override;
  void OnStreamReset(
      int64_t stream_id,
      uint64_t app_error_code) override;
  void OnSessionClose(
      QuicError error,
      int flags = SESSION_CLOSE_FLAG_NONE) override;
  void OnStreamReady(BaseObjectPtr<QuicStream> stream) override;
  void OnHandshakeCompleted() override;
  void OnPathValidation(
      ngtcp2_path_validation_result res,
      const sockaddr* local,
      const sockaddr* remote) override;
  void OnSessionTicket(int size, SSL_SESSION* session) override;
  void OnUsePreferredAddress(
      int family,
      const PreferredAddress& preferred_address) override;
  void OnStreamBlocked(int64_t stream_id) override;
  void OnVersionNegotiation(
      uint32_t supported_version,
      const uint32_t* versions,
      size_t vcnt) override;
  void OnQLog(QLogStream* qlog_stream) override;

 private:
  friend class QuicSession;
};

#define QUICCRYPTOCONTEXT_FLAGS(V)                                             \
  V(IN_TLS_CALLBACK, in_tls_callback)                                          \
  V(IN_KEY_UPDATE, in_key_update)                                              \
  V(IN_OCSP_RESPONSE, in_ocsp_request)                                         \
  V(IN_CLIENT_HELLO, in_client_hello)                                          \
  V(EARLY_DATA, early_data)

// The QuicCryptoContext class encapsulates all of the crypto/TLS
// handshake details on behalf of a QuicSession.
class QuicCryptoContext final : public MemoryRetainer {
 public:
  inline QuicCryptoContext(
      QuicSession* session,
      BaseObjectPtr<crypto::SecureContext> secure_context,
      ngtcp2_crypto_side side,
      uint32_t options);

  ~QuicCryptoContext() override;

  inline uint64_t Cancel();

  // Outgoing crypto data must be retained in memory until it is
  // explicitly acknowledged. AcknowledgeCryptoData will be invoked
  // when ngtcp2 determines that it has received an acknowledgement
  // for crypto data at the specified level. This is our indication
  // that the data for that level can be released.
  void AcknowledgeCryptoData(ngtcp2_crypto_level level, uint64_t datalen);

  inline void Initialize();

  // Enables openssl's TLS tracing mechanism for this session only.
  void EnableTrace();

  // Returns the server's prepared OCSP response for transmission. This
  // is not used by client QuicSession instances.
  inline v8::MaybeLocal<v8::Value> ocsp_response() const;

  // Returns ngtcp2's understanding of the current inbound crypto level
  inline ngtcp2_crypto_level read_crypto_level() const;

  // Returns ngtcp2's understanding of the current outbound crypto level
  inline ngtcp2_crypto_level write_crypto_level() const;

  inline bool early_data() const;

  bool is_option_set(uint32_t option) const { return options_ & option; }

  // Emits a single keylog line to the JavaScript layer
  inline void Keylog(const char* line);

  int OnClientHello();

  void OnClientHelloDone(BaseObjectPtr<crypto::SecureContext> context);

  int OnOCSP();

  void OnOCSPDone(v8::Local<v8::Value> ocsp_response);

  bool OnSecrets(
      ngtcp2_crypto_level level,
      const uint8_t* rx_secret,
      const uint8_t* tx_secret,
      size_t secretlen);

  int OnTLSStatus();

  // Receives and processes TLS handshake details
  int Receive(
      ngtcp2_crypto_level crypto_level,
      uint64_t offset,
      const uint8_t* data,
      size_t datalen);

  // Resumes the TLS handshake following a client hello or
  // OCSP callback
  inline void ResumeHandshake();

  inline v8::MaybeLocal<v8::Value> cert() const;
  inline v8::MaybeLocal<v8::Value> cipher_name() const;
  inline v8::MaybeLocal<v8::Value> cipher_version() const;
  inline v8::MaybeLocal<v8::Object> ephemeral_key() const;
  inline const char* hello_alpn() const;
  inline v8::MaybeLocal<v8::Array> hello_ciphers() const;
  inline const char* hello_servername() const;
  inline v8::MaybeLocal<v8::Value> peer_cert(bool abbreviated) const;
  inline std::string selected_alpn() const;
  inline const char* servername() const;

  void set_option(uint32_t option, bool on = true) {
    if (on)
      options_ |= option;
    else
      options_ &= ~option;
  }

#define V(id, name)                                                            \
  inline bool is_##name() const {                                              \
    return flags_ & (1 << QUICCRYPTOCONTEXT_FLAG_##id); }                      \
  inline void set_##name(bool on = true) {                                     \
    if (on)                                                                    \
      flags_ |= (1 << QUICCRYPTOCONTEXT_FLAG_##id);                            \
    else                                                                       \
      flags_ &= ~(1 << QUICCRYPTOCONTEXT_FLAG_##id);                           \
  }
  QUICCRYPTOCONTEXT_FLAGS(V)
#undef V

  inline bool set_session(crypto::SSLSessionPointer session);

  inline void set_tls_alert(int err);

  ngtcp2_crypto_side side() const { return side_; }

  void WriteHandshake(
      ngtcp2_crypto_level level,
      const uint8_t* data,
      size_t datalen);

  bool InitiateKeyUpdate();

  int VerifyPeerIdentity();

  QuicSession* session() const { return session_.get(); }

  void MemoryInfo(MemoryTracker* tracker) const override;

  SET_MEMORY_INFO_NAME(QuicCryptoContext)
  SET_SELF_SIZE(QuicCryptoContext)

 private:
  bool SetSecrets(
      ngtcp2_crypto_level level,
      const uint8_t* rx_secret,
      const uint8_t* tx_secret,
      size_t secretlen);

  BaseObjectWeakPtr<QuicSession> session_;
  BaseObjectPtr<crypto::SecureContext> secure_context_;
  ngtcp2_crypto_side side_;
  crypto::SSLPointer ssl_;
  QuicBuffer handshake_[3];
  uint32_t options_;
  uint32_t flags_ = 0;

  v8::Global<v8::ArrayBufferView> ocsp_response_;
  crypto::BIOPointer bio_trace_;

#define V(id, _) QUICCRYPTOCONTEXT_FLAG_##id,
  enum QuicCryptoContextFlags : uint32_t {
    QUICCRYPTOCONTEXT_FLAGS(V)
    QUICCRYPTOCONTEXT_FLAG_COUNT
  };
#undef V

  class TLSCallbackScope {
   public:
    explicit TLSCallbackScope(QuicCryptoContext* context) :
        context_(context) {
      context_->set_in_tls_callback();
    }

    ~TLSCallbackScope() {
      context_->set_in_tls_callback(false);
    }

    static bool is_in_callback(QuicCryptoContext* context) {
      return context->is_in_tls_callback();
    }

   private:
    QuicCryptoContext* context_;
  };

  class TLSHandshakeScope {
   public:
    using DoneCB = std::function<void()>;
    TLSHandshakeScope(
        QuicCryptoContext* context,
        DoneCB done) :
        context_(context),
        done_(done) {}

    ~TLSHandshakeScope() {
      if (!is_handshake_suspended())
        return;

      done_();
      // Only continue the TLS handshake if we are not currently running
      // synchronously within the TLS handshake function. This can happen
      // when the callback function passed to the clientHello and cert
      // event handlers is called synchronously. If the function is called
      // asynchronously, then we have to manually continue the handshake.
      if (!TLSCallbackScope::is_in_callback(context_))
        context_->ResumeHandshake();
    }

   private:
    bool is_handshake_suspended() const {
      return context_->is_in_ocsp_request() || context_->is_in_client_hello();
    }


    QuicCryptoContext* context_;
    DoneCB done_;
  };

  friend class QuicSession;
};

// A QuicApplication encapsulates the specific details of
// working with a specific QUIC application (e.g. http/3).
class QuicApplication : public MemoryRetainer,
                        public mem::NgLibMemoryManagerBase {
 public:
  inline explicit QuicApplication(QuicSession* session);
  virtual ~QuicApplication() = default;

  // The QuicSession will call Initialize as soon as the TLS
  // secrets have been set. See QuicCryptoContext::OnSecrets
  virtual bool Initialize() = 0;

  // QuicSession will forward all received stream data immediately
  // on to the QuicApplication. The only additional processing the
  // QuicSession does is to automatically adjust the QuicSession-level
  // flow control window. It is up to the QuicApplication to do
  // the same for the QuicStream-level flow control.
  //
  // flags are passed on directly from ngtcp2. The most important
  // of which here is NGTCP2_STREAM_DATA_FLAG_FIN, which indicates
  // that this is the final chunk of data that the peer will send
  // for this stream.
  //
  // It is also possible for the NGTCP2_STREAM_DATA_FLAG_0RTT flag
  // to be set, indicating that this chunk of data was received in
  // a 0RTT packet before the TLS handshake completed. This would
  // indicate that it is not as secure and could be replayed by
  // an attacker. We're not currently making use of that flag.
  virtual bool ReceiveStreamData(
      uint32_t flags,
      int64_t stream_id,
      const uint8_t* data,
      size_t datalen,
      uint64_t offset) = 0;

  virtual void AcknowledgeStreamData(
      int64_t stream_id,
      uint64_t offset,
      size_t datalen) {
    Acknowledge(stream_id, offset, datalen);
  }

  virtual bool BlockStream(int64_t id) { return true; }

  virtual void ExtendMaxStreamsRemoteUni(uint64_t max_streams) {}

  virtual void ExtendMaxStreamsRemoteBidi(uint64_t max_streams) {}

  virtual void ExtendMaxStreamData(int64_t stream_id, uint64_t max_data) {}

  virtual void ResumeStream(int64_t stream_id) {}

  virtual void SetSessionTicketAppData(const SessionTicketAppData& app_data) {
    // TODO(@jasnell): Different QUIC applications may wish to set some
    // application data in the session ticket (e.g. http/3 would set
    // server settings in the application data). For now, doing nothing
    // as I'm just adding the basic mechanism.
  }

  virtual SessionTicketAppData::Status GetSessionTicketAppData(
      const SessionTicketAppData& app_data,
      SessionTicketAppData::Flag flag) {
    // TODO(@jasnell): Different QUIC application may wish to set some
    // application data in the session ticket (e.g. http/3 would set
    // server settings in the application data). For now, doing nothing
    // as I'm just adding the basic mechanism.
    return flag == SessionTicketAppData::Flag::STATUS_RENEW ?
      SessionTicketAppData::Status::TICKET_USE_RENEW :
      SessionTicketAppData::Status::TICKET_USE;
  }

  virtual void StreamHeaders(
      int64_t stream_id,
      int kind,
      const std::vector<std::unique_ptr<QuicHeader>>& headers,
      int64_t push_id = 0);

  virtual void StreamClose(
      int64_t stream_id,
      uint64_t app_error_code);

  virtual void StreamReset(
      int64_t stream_id,
      uint64_t app_error_code);

  virtual bool SubmitInformation(
      int64_t stream_id,
      v8::Local<v8::Array> headers) { return false; }

  virtual bool SubmitHeaders(
      int64_t stream_id,
      v8::Local<v8::Array> headers,
      uint32_t flags) { return false; }

  virtual bool SubmitTrailers(
      int64_t stream_id,
      v8::Local<v8::Array> headers) { return false; }

  virtual BaseObjectPtr<QuicStream> SubmitPush(
      int64_t stream_id,
      v8::Local<v8::Array> headers) {
    // By default, push streams are not supported
    // by an application.
    return {};
  }

  inline Environment* env() const;

  bool SendPendingData();
  size_t max_header_pairs() const { return max_header_pairs_; }
  size_t max_header_length() const { return max_header_length_; }

 protected:
  QuicSession* session() const { return session_.get(); }
  bool needs_init() const { return needs_init_; }
  void set_init_done() { needs_init_ = false; }
  inline void set_stream_fin(int64_t stream_id);
  void set_max_header_pairs(size_t max) { max_header_pairs_ = max; }
  void set_max_header_length(size_t max) { max_header_length_ = max; }
  inline std::unique_ptr<QuicPacket> CreateStreamDataPacket();

  struct StreamData {
    size_t count = 0;
    size_t remaining = 0;
    int64_t id = -1;
    int fin = 0;
    ngtcp2_vec data[kMaxVectorCount] {};
    ngtcp2_vec* buf = nullptr;
    BaseObjectPtr<QuicStream> stream;
    StreamData() { buf = data; }
  };

  void Acknowledge(
      int64_t stream_id,
      uint64_t offset,
      size_t datalen);
  virtual int GetStreamData(StreamData* data) = 0;
  virtual bool StreamCommit(StreamData* data, size_t datalen) = 0;
  virtual bool ShouldSetFin(const StreamData& data) = 0;

  ssize_t WriteVStream(
      QuicPathStorage* path,
      uint8_t* buf,
      ssize_t* ndatalen,
      const StreamData& stream_data);

 private:
  void MaybeSetFin(const StreamData& stream_data);
  BaseObjectWeakPtr<QuicSession> session_;
  bool needs_init_ = true;
  size_t max_header_pairs_ = 0;
  size_t max_header_length_ = 0;
};

// QUICSESSION_FLAGS are converted into is_{name}() and set_{name}(bool on)
// accessors on the QuicSession class.
#define QUICSESSION_FLAGS(V)                                                   \
    V(WRAPPED, wrapped)                                                        \
    V(CLOSING, closing)                                                        \
    V(GRACEFUL_CLOSING, graceful_closing)                                      \
    V(DESTROYED, destroyed)                                                    \
    V(TRANSPORT_PARAMS_SET, transport_params_set)                              \
    V(NGTCP2_CALLBACK, in_ngtcp2_callback)                                     \
    V(CONNECTION_CLOSE_SCOPE, in_connection_close_scope)                       \
    V(SILENT_CLOSE, silent_closing)                                            \
    V(STATELESS_RESET, stateless_reset)                                        \
    V(CLOSING_TIMER_ENABLED, closing_timer_enabled)

// QUIC sessions are logical connections that exchange data
// back and forth between peer endpoints via UDP. Every QuicSession
// has an associated TLS context and all data transfered between
// the peers is always encrypted. Unlike TLS over TCP, however,
// The QuicSession uses a session identifier that is independent
// of both the local *and* peer IP address, allowing a QuicSession
// to persist across changes in the network (one of the key features
// of QUIC). QUIC sessions also support 0RTT, implement error
// correction mechanisms to recover from lost packets, and flow
// control. In other words, there's quite a bit going on within
// a QuicSession object.
class QuicSession final : public AsyncWrap,
                          public mem::NgLibMemoryManager<
                              QuicSession,
                              ngtcp2_mem>,
                          public StatsBase<QuicSessionStatsTraits> {
 public:
  // The default preferred address strategy is to ignore it
  static void IgnorePreferredAddressStrategy(
      QuicSession* session,
      const PreferredAddress& preferred_address);

  static void UsePreferredAddressStrategy(
      QuicSession* session,
      const PreferredAddress& preferred_address);

  static void Initialize(
      Environment* env,
      v8::Local<v8::Object> target,
      v8::Local<v8::Context> context);

  static BaseObjectPtr<QuicSession> CreateServer(
      QuicSocket* socket,
      const QuicSessionConfig& config,
      const SocketAddress& local_addr,
      const SocketAddress& remote_addr,
      const QuicCID& dcid,
      const QuicCID& scid,
      const QuicCID& ocid,
      uint32_t version,
      const std::string& alpn = NGHTTP3_ALPN_H3,
      uint32_t options = 0,
      QlogMode qlog = QlogMode::kDisabled);

  static BaseObjectPtr<QuicSession> CreateClient(
      QuicSocket* socket,
      const SocketAddress& local_addr,
      const SocketAddress& remote_addr,
      BaseObjectPtr<crypto::SecureContext> secure_context,
      ngtcp2_transport_params* early_transport_params,
      crypto::SSLSessionPointer early_session_ticket,
      v8::Local<v8::Value> dcid,
      PreferredAddressStrategy preferred_address_strategy =
          IgnorePreferredAddressStrategy,
      const std::string& alpn = NGHTTP3_ALPN_H3,
      const std::string& hostname = "",
      uint32_t options = 0,
      QlogMode qlog = QlogMode::kDisabled);

  static const int kInitialClientBufferLength = 4096;

  QuicSession(
      ngtcp2_crypto_side side,
      // The QuicSocket that created this session. Note that
      // it is possible to replace this socket later, after
      // the TLS handshake has completed. The QuicSession
      // should never assume that the socket will always
      // remain the same.
      QuicSocket* socket,
      v8::Local<v8::Object> wrap,
      BaseObjectPtr<crypto::SecureContext> secure_context,
      AsyncWrap::ProviderType provider_type,
      // QUIC is generally just a transport. The ALPN identifier
      // is used to specify the application protocol that is
      // layered on top. If not specified, this will default
      // to the HTTP/3 identifier. For QUIC, the alpn identifier
      // is always required.
      const std::string& alpn,
      const std::string& hostname,
      const QuicCID& dcid,
      uint32_t options = 0,
      PreferredAddressStrategy preferred_address_strategy =
          IgnorePreferredAddressStrategy);

  // Server Constructor
  QuicSession(
      QuicSocket* socket,
      const QuicSessionConfig& config,
      v8::Local<v8::Object> wrap,
      const SocketAddress& local_addr,
      const SocketAddress& remote_addr,
      const QuicCID& dcid,
      const QuicCID& scid,
      const QuicCID& ocid,
      uint32_t version,
      const std::string& alpn,
      uint32_t options,
      QlogMode qlog);

  // Client Constructor
  QuicSession(
      QuicSocket* socket,
      v8::Local<v8::Object> wrap,
      const SocketAddress& local_addr,
      const SocketAddress& remote_addr,
      BaseObjectPtr<crypto::SecureContext> secure_context,
      ngtcp2_transport_params* early_transport_params,
      crypto::SSLSessionPointer early_session_ticket,
      v8::Local<v8::Value> dcid,
      PreferredAddressStrategy preferred_address_strategy,
      const std::string& alpn,
      const std::string& hostname,
      uint32_t options,
      QlogMode qlog);

  ~QuicSession() override;

  std::string diagnostic_name() const override;

  inline QuicCID dcid() const;

  QuicApplication* application() const { return application_.get(); }

  QuicCryptoContext* crypto_context() const { return crypto_context_.get(); }

  QuicSessionListener* listener() const { return listener_; }

  BaseObjectPtr<QuicStream> CreateStream(int64_t id);

  BaseObjectPtr<QuicStream> FindStream(int64_t id) const;

  inline bool HasStream(int64_t id) const;

  inline bool allow_early_data() const;

#define V(id, name)                                                            \
  bool is_##name() const { return flags_ & (1 << QUICSESSION_FLAG_##id); }     \
  void set_##name(bool on = true) {                                            \
    if (on)                                                                    \
      flags_ |= (1 << QUICSESSION_FLAG_##id);                                  \
    else                                                                       \
      flags_ &= ~(1 << QUICSESSION_FLAG_##id);                                 \
  }
  QUICSESSION_FLAGS(V)
#undef V

  // Returns true if the QuicSession has entered the
  // closing period after sending a CONNECTION_CLOSE.
  // While true, the QuicSession is only permitted to
  // transmit CONNECTION_CLOSE frames until either the
  // idle timeout period elapses or until the QuicSession
  // is explicitly destroyed.
  inline bool is_in_closing_period() const;

  // Returns true if the QuicSession has received a
  // CONNECTION_CLOSE frame from the peer. Once in
  // the draining period, the QuicSession is not
  // permitted to send any frames to the peer. The
  // QuicSession will be silently closed after either
  // the idle timeout period elapses or until the
  // QuicSession is explicitly destroyed.
  inline bool is_in_draining_period() const;

  inline bool is_server() const;

  // Starting a GracefulClose disables the ability to open or accept
  // new streams for this session. Existing streams are allowed to
  // close naturally on their own. Once called, the QuicSession will
  // be immediately closed once there are no remaining streams. Note
  // that no notification is given to the connecting peer that we're
  // in a graceful closing state. A CONNECTION_CLOSE will be sent only
  // once Close() is called.
  inline void StartGracefulClose();

  QuicError last_error() const { return last_error_; }

  size_t max_packet_length() const { return max_pktlen_; }

  BaseObjectPtr<QLogStream> qlog_stream();

  // Get the ALPN protocol identifier configured for this QuicSession.
  // For server sessions, this will be compared against the client requested
  // ALPN identifier to determine if there is a protocol match.
  const std::string& alpn() const { return alpn_; }

  // Get the hostname configured for this QuicSession. This is generally
  // only used by client sessions.
  const std::string& hostname() const { return hostname_; }

  // Returns the associated peer's address. Note that this
  // value can change over the lifetime of the QuicSession.
  // The fact that the session is not tied intrinsically to
  // a single address is one of the benefits of QUIC.
  const SocketAddress& remote_address() const { return remote_address_; }

  inline QuicSocket* socket() const;

  ngtcp2_conn* connection() const { return connection_.get(); }

  void AddStream(BaseObjectPtr<QuicStream> stream);

  void AddToSocket(QuicSocket* socket);

  // Immediately discards the state of the QuicSession
  // and renders the QuicSession instance completely
  // unusable.
  void Destroy();

  // Extends the QUIC stream flow control window. This is
  // called after received data has been consumed and we
  // want to allow the peer to send more data.
  inline void ExtendStreamOffset(int64_t stream_id, size_t amount);

  // Extends the QUIC session flow control window
  inline void ExtendOffset(size_t amount);

  // Retrieve the local transport parameters established for
  // this ngtcp2_conn
  inline void GetLocalTransportParams(ngtcp2_transport_params* params);

  // The QUIC version that has been negotiated for this session
  inline uint32_t negotiated_version() const;

  // True only if ngtcp2 considers the TLS handshake to be completed
  inline bool is_handshake_completed() const;

  // Checks to see if data needs to be retransmitted
  void OnRetransmitTimeout();

  // Called when the session has been determined to have been
  // idle for too long and needs to be torn down.
  inline void OnIdleTimeout();

  bool OpenBidirectionalStream(int64_t* stream_id);

  bool OpenUnidirectionalStream(int64_t* stream_id);

  // Ping causes the QuicSession to serialize any currently
  // pending frames in it's queue, including any necessary
  // PROBE packets. This is a best attempt, fire-and-forget
  // type of operation. There is no way to listen for a ping
  // response. The main intent of using Ping is to either keep
  // the connection from becoming idle or to update RTT stats.
  void Ping();

  // Receive and process a QUIC packet received from the peer
  bool Receive(
      ssize_t nread,
      const uint8_t* data,
      const SocketAddress& local_addr,
      const SocketAddress& remote_addr,
      unsigned int flags);

  // Receive a chunk of QUIC stream data received from the peer
  bool ReceiveStreamData(
      uint32_t flags,
      int64_t stream_id,
      const uint8_t* data,
      size_t datalen,
      uint64_t offset);

  void RemoveStream(int64_t stream_id);

  void RemoveFromSocket();

  // Causes pending ngtcp2 frames to be serialized and sent
  void SendPendingData();

  inline void ShutdownStream(int64_t stream_id, uint64_t code);

  inline bool SendPacket(
      std::unique_ptr<QuicPacket> packet,
      const ngtcp2_path_storage& path);

  inline uint64_t max_data_left() const;

  inline uint64_t max_local_streams_uni() const;

  inline void set_last_error(
      QuicError error = {
          uint32_t{QUIC_ERROR_SESSION},
          uint64_t{NGTCP2_NO_ERROR}
      });

  inline void set_last_error(int32_t family, uint64_t error_code);

  inline void set_last_error(int32_t family, int error_code);

  inline void set_remote_transport_params();

  bool set_socket(QuicSocket* socket, bool nat_rebinding = false);

  int set_session(SSL_SESSION* session);

  const StreamsMap& streams() const { return streams_; }

  void ResumeStream(int64_t stream_id);

  // Submits informational headers to the QUIC Application
  // implementation. If headers are not supported, false
  // will be returned. Otherwise, returns true
  inline bool SubmitInformation(
      int64_t stream_id,
      v8::Local<v8::Array> headers);

  // Submits initial headers to the QUIC Application
  // implementation. If headers are not supported, false
  // will be returned. Otherwise, returns true
  inline bool SubmitHeaders(
      int64_t stream_id,
      v8::Local<v8::Array> headers,
      uint32_t flags);

  // Submits trailing headers to the QUIC Application
  // implementation. If headers are not supported, false
  // will be returned. Otherwise, returns true
  inline bool SubmitTrailers(
      int64_t stream_id,
      v8::Local<v8::Array> headers);

  inline BaseObjectPtr<QuicStream> SubmitPush(
      int64_t stream_id,
      v8::Local<v8::Array> headers);

  // Error handling for the QuicSession. client and server
  // instances will do different things here, but ultimately
  // an error means that the QuicSession
  // should be torn down.
  void HandleError();

  bool SendConnectionClose();

  bool IsResetToken(
      const QuicCID& cid,
      const uint8_t* data,
      size_t datalen);

  // Implementation for mem::NgLibMemoryManager
  inline void CheckAllocatedSize(size_t previous_size) const;

  inline void IncreaseAllocatedSize(size_t size);

  inline void DecreaseAllocatedSize(size_t size);

  // Initiate closing of the QuicSession. This will round trip
  // through JavaScript, causing all currently opened streams
  // to be closed. If the SESSION_CLOSE_FLAG_SILENT flag is
  // set, the connected peer will not be notified, otherwise
  // an attempt will be made to send a CONNECTION_CLOSE frame
  // to the peer. If Close is called while within the ngtcp2
  // callback scope, sending the CONNECTION_CLOSE will be
  // deferred until the ngtcp2 callback scope exits.
  void Close(
      int close_flags = QuicSessionListener::SESSION_CLOSE_FLAG_NONE);

  void PushListener(QuicSessionListener* listener);

  void RemoveListener(QuicSessionListener* listener);

  inline bool is_unable_to_send_packets();

  inline void set_connection_id_strategy(
      ConnectionIDStrategy strategy);

  inline void set_preferred_address_strategy(
      PreferredAddressStrategy strategy);

  inline void SetSessionTicketAppData(
      const SessionTicketAppData& app_data);

  inline SessionTicketAppData::Status GetSessionTicketAppData(
      const SessionTicketAppData& app_data,
      SessionTicketAppData::Flag flag);

  inline void SelectPreferredAddress(
      const PreferredAddress& preferred_address);

  // Report that the stream data is flow control blocked
  inline void StreamDataBlocked(int64_t stream_id);

  // SendSessionScope triggers SendPendingData() when not executing
  // within the context of an ngtcp2 callback. When within an ngtcp2
  // callback, SendPendingData will always be called when the callbacks
  // complete.
  class SendSessionScope final {
   public:
    explicit SendSessionScope(QuicSession* session)
        : session_(session) {
      CHECK(session_);
    }

    SendSessionScope(const SendSessionScope& other) = delete;

    ~SendSessionScope() {
      if (NgCallbackScope::InNgCallbackScope(session_.get()) ||
          session_->is_in_closing_period() ||
          session_->is_in_draining_period()) {
        return;
      }
      session_->SendPendingData();
    }

   private:
    BaseObjectPtr<QuicSession> session_;
  };

  // ConnectionCloseScope triggers sending a CONNECTION_CLOSE
  // when not executing within the context of an ngtcp2 callback
  // and the session is in the correct state.
  class ConnectionCloseScope final {
   public:
    ConnectionCloseScope(QuicSession* session, bool silent = false)
        : session_(session),
          silent_(silent) {
      CHECK(session_);
      // If we are already in a ConnectionCloseScope, ignore.
      if (session_->is_in_connection_close_scope())
        silent_ = true;
      else
        session_->set_in_connection_close_scope();
    }

    ConnectionCloseScope(const ConnectionCloseScope& other) = delete;

    ~ConnectionCloseScope() {
      if (silent_ ||
          NgCallbackScope::InNgCallbackScope(session_.get()) ||
          session_->is_in_closing_period() ||
          session_->is_in_draining_period()) {
        return;
      }
      session_->set_in_connection_close_scope(false);
      session_->SendConnectionClose();
    }

   private:
    BaseObjectPtr<QuicSession> session_;
    bool silent_ = false;
  };

  // Tracks whether or not we are currently within an ngtcp2 callback
  // function. Certain ngtcp2 APIs are not supposed to be called when
  // within a callback. We use this as a gate to check.
  class NgCallbackScope final {
   public:
    explicit NgCallbackScope(QuicSession* session) : session_(session) {
      CHECK(session_);
      CHECK(!InNgCallbackScope(session));
      session_->set_in_ngtcp2_callback();
    }

    NgCallbackScope(const NgCallbackScope& other) = delete;

    ~NgCallbackScope() {
      session_->set_in_ngtcp2_callback(false);
    }

    static bool InNgCallbackScope(QuicSession* session) {
      return session->is_in_ngtcp2_callback();
    }

   private:
    BaseObjectPtr<QuicSession> session_;
  };

  QuicState* quic_state() { return quic_state_.get(); }

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(QuicSession)
  SET_SELF_SIZE(QuicSession)

 private:
  static void RandomConnectionIDStrategy(
      QuicSession* session,
      ngtcp2_cid* cid,
      size_t cidlen);

  // Initialize the QuicSession as a server
  void InitServer(
      QuicSessionConfig config,
      const SocketAddress& local_addr,
      const SocketAddress& remote_addr,
      const QuicCID& dcid,
      const QuicCID& scid,
      const QuicCID& ocid,
      uint32_t version,
      QlogMode qlog);

  // Initialize the QuicSession as a client
  void InitClient(
      const SocketAddress& local_addr,
      const SocketAddress& remote_addr,
      ngtcp2_transport_params* early_transport_params,
      crypto::SSLSessionPointer early_session_ticket,
      v8::Local<v8::Value> dcid,
      QlogMode qlog);

  bool InitApplication();

  void AckedStreamDataOffset(
      int64_t stream_id,
      uint64_t offset,
      uint64_t datalen);

  void ExtendMaxStreamData(int64_t stream_id, uint64_t max_data);

  void ExtendMaxStreams(bool bidi, uint64_t max_streams);

  void ExtendMaxStreamsUni(uint64_t max_streams);

  void ExtendMaxStreamsBidi(uint64_t max_streams);

  void ExtendMaxStreamsRemoteUni(uint64_t max_streams);

  void ExtendMaxStreamsRemoteBidi(uint64_t max_streams);

  void GetNewConnectionID(ngtcp2_cid* cid, uint8_t* token, size_t cidlen);

  void GetConnectionCloseInfo();

  void HandshakeCompleted();

  void HandshakeConfirmed();

  void PathValidation(
      const ngtcp2_path* path,
      ngtcp2_path_validation_result res);

  bool ReceivePacket(ngtcp2_path* path, const uint8_t* data, ssize_t nread);

  void ScheduleRetransmit();

  bool SendPacket(std::unique_ptr<QuicPacket> packet);

  void StreamClose(int64_t stream_id, uint64_t app_error_code);

  void StreamReset(
      int64_t stream_id,
      uint64_t final_size,
      uint64_t app_error_code);

  bool WritePackets(const char* diagnostic_label = nullptr);

  void UpdateRecoveryStats();

  void UpdateConnectionID(
      int type,
      const QuicCID& cid,
      const StatelessResetToken& token);

  void UpdateDataStats();

  void UpdateEndpoint(const ngtcp2_path& path);

  void VersionNegotiation(const uint32_t* sv, size_t nsv);

  void UpdateIdleTimer();

  void UpdateClosingTimer();

  void UpdateRetransmitTimer(uint64_t timeout);

  bool StartClosingPeriod();

  void IncrementConnectionCloseAttempts();

  bool ShouldAttemptConnectionClose();

  static int OnReceiveCryptoData(
      ngtcp2_conn* conn,
      ngtcp2_crypto_level crypto_level,
      uint64_t offset,
      const uint8_t* data,
      size_t datalen,
      void* user_data);

  static int OnHandshakeCompleted(
      ngtcp2_conn* conn,
      void* user_data);

  static int OnHandshakeConfirmed(
      ngtcp2_conn* conn,
      void* user_data);

  static int OnReceiveStreamData(
      ngtcp2_conn* conn,
      uint32_t flags,
      int64_t stream_id,
      uint64_t offset,
      const uint8_t* data,
      size_t datalen,
      void* user_data,
      void* stream_user_data);

  static int OnAckedCryptoOffset(
      ngtcp2_conn* conn,
      ngtcp2_crypto_level crypto_level,
      uint64_t offset,
      uint64_t datalen,
      void* user_data);

  static int OnAckedStreamDataOffset(
      ngtcp2_conn* conn,
      int64_t stream_id,
      uint64_t offset,
      uint64_t datalen,
      void* user_data,
      void* stream_user_data);

  static int OnSelectPreferredAddress(
      ngtcp2_conn* conn,
      ngtcp2_addr* dest,
      const ngtcp2_preferred_addr* paddr,
      void* user_data);

  static int OnStreamClose(
      ngtcp2_conn* conn,
      int64_t stream_id,
      uint64_t app_error_code,
      void* user_data,
      void* stream_user_data);

  static int OnStreamOpen(
      ngtcp2_conn* conn,
      int64_t stream_id,
      void* user_data);

  static int OnStreamReset(
      ngtcp2_conn* conn,
      int64_t stream_id,
      uint64_t final_size,
      uint64_t app_error_code,
      void* user_data,
      void* stream_user_data);

  static int OnRand(
      ngtcp2_conn* conn,
      uint8_t* dest,
      size_t destlen,
      ngtcp2_rand_ctx ctx,
      void* user_data);

  static int OnGetNewConnectionID(
      ngtcp2_conn* conn,
      ngtcp2_cid* cid,
      uint8_t* token,
      size_t cidlen,
      void* user_data);

  static int OnRemoveConnectionID(
      ngtcp2_conn* conn,
      const ngtcp2_cid* cid,
      void* user_data);

  static int OnPathValidation(
      ngtcp2_conn* conn,
      const ngtcp2_path* path,
      ngtcp2_path_validation_result res,
      void* user_data);

  static int OnExtendMaxStreamsUni(
      ngtcp2_conn* conn,
      uint64_t max_streams,
      void* user_data);

  static int OnExtendMaxStreamsBidi(
      ngtcp2_conn* conn,
      uint64_t max_streams,
      void* user_data);

  static int OnExtendMaxStreamData(
      ngtcp2_conn* conn,
      int64_t stream_id,
      uint64_t max_data,
      void* user_data,
      void* stream_user_data);

  static int OnVersionNegotiation(
      ngtcp2_conn* conn,
      const ngtcp2_pkt_hd* hd,
      const uint32_t* sv,
      size_t nsv,
      void* user_data);

  static int OnStatelessReset(
      ngtcp2_conn* conn,
      const ngtcp2_pkt_stateless_reset* sr,
      void* user_data);

  static int OnExtendMaxStreamsRemoteUni(
      ngtcp2_conn* conn,
      uint64_t max_streams,
      void* user_data);

  static int OnExtendMaxStreamsRemoteBidi(
      ngtcp2_conn* conn,
      uint64_t max_streams,
      void* user_data);

  static int OnConnectionIDStatus(
      ngtcp2_conn* conn,
      int type,
      uint64_t seq,
      const ngtcp2_cid* cid,
      const uint8_t* token,
      void* user_data);

  static void OnQlogWrite(void* user_data, const void* data, size_t len);

#define V(id, _) QUICSESSION_FLAG_##id,
  enum QuicSessionFlags : uint32_t {
    QUICSESSION_FLAGS(V)
    QUICSESSION_FLAG_COUNT
  };
#undef V

  // Select the QUIC Application based on the configured ALPN identifier
  QuicApplication* SelectApplication(QuicSession* session);

  ngtcp2_mem alloc_info_;
  std::unique_ptr<QuicCryptoContext> crypto_context_;
  std::unique_ptr<QuicApplication> application_;
  BaseObjectWeakPtr<QuicSocket> socket_;
  std::string alpn_;
  std::string hostname_;
  QuicError last_error_ = {
      uint32_t{QUIC_ERROR_SESSION},
      uint64_t{NGTCP2_NO_ERROR}
  };
  ConnectionPointer connection_;
  SocketAddress local_address_{};
  SocketAddress remote_address_{};
  uint32_t flags_ = 0;
  size_t max_pktlen_ = 0;
  size_t current_ngtcp2_memory_ = 0;
  size_t connection_close_attempts_ = 0;
  size_t connection_close_limit_ = 1;

  ConnectionIDStrategy connection_id_strategy_ = nullptr;
  PreferredAddressStrategy preferred_address_strategy_ = nullptr;

  QuicSessionListener* listener_ = nullptr;
  JSQuicSessionListener default_listener_;

  TimerWrapHandle idle_;
  TimerWrapHandle retransmit_;

  QuicCID scid_;
  QuicCID dcid_;
  QuicCID pscid_;
  ngtcp2_transport_params transport_params_;

  std::unique_ptr<QuicPacket> conn_closebuf_;

  StreamsMap streams_;

  AliasedStruct<QuicSessionState> state_;

  struct RemoteTransportParamsDebug {
    QuicSession* session;
    explicit RemoteTransportParamsDebug(QuicSession* session_)
        : session(session_) {}
    std::string ToString() const;
  };

  static const ngtcp2_conn_callbacks callbacks[2];

  BaseObjectPtr<QuicState> quic_state_;
  BaseObjectWeakPtr<QLogStream> qlog_stream_;

  friend class QuicCryptoContext;
  friend class QuicSessionListener;
  friend class JSQuicSessionListener;
};

class QuicCallbackScope {
 public:
  explicit QuicCallbackScope(QuicSession* session);
  ~QuicCallbackScope();

  void operator=(const QuicCallbackScope&) = delete;
  void operator=(QuicCallbackScope&&) = delete;
  QuicCallbackScope(const QuicCallbackScope&) = delete;
  QuicCallbackScope(QuicCallbackScope&&) = delete;

 private:
  BaseObjectPtr<QuicSession> session_;
  std::unique_ptr<InternalCallbackScope> private_;
  v8::TryCatch try_catch_;
};

}  // namespace quic
}  // namespace node

#endif  // NODE_WANT_INTERNALS
#endif  // SRC_QUIC_NODE_QUIC_SESSION_H_
