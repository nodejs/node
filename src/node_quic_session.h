#ifndef SRC_NODE_QUIC_SESSION_H_
#define SRC_NODE_QUIC_SESSION_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "aliased_buffer.h"
#include "async_wrap.h"
#include "env.h"
#include "handle_wrap.h"
#include "histogram-inl.h"
#include "node.h"
#include "node_crypto.h"
#include "node_mem.h"
#include "node_quic_buffer.h"
#include "node_quic_crypto.h"
#include "node_quic_util.h"
#include "node_sockaddr.h"
#include "v8.h"
#include "uv.h"

#include <ngtcp2/ngtcp2.h>
#include <ngtcp2/ngtcp2_crypto.h>
#include <openssl/ssl.h>

#include <map>
#include <vector>

namespace node {
namespace quic {

using ConnectionPointer = DeleteFnPtr<ngtcp2_conn, ngtcp2_conn_del>;

class QuicSocket;
class QuicPacket;
class QuicStream;
class QuicHeader;

enum class QlogMode {
  kDisabled,
  kEnabled
};

// The QuicSessionConfig class holds the initial transport parameters and
// configuration options set by the JavaScript side when either a
// client or server QuicSession is created. Instances are
// stack created and use a combination of an AliasedBuffer to pass
// the numeric settings quickly (see node_quic_state.h) and passed
// in non-numeric settings (e.g. preferred_addr).
class QuicSessionConfig {
 public:
  QuicSessionConfig() {
    ResetToDefaults();
  }

  explicit QuicSessionConfig(Environment* env) : QuicSessionConfig() {
    Set(env);
  }

  QuicSessionConfig(const QuicSessionConfig& config) {
    settings_ = config.settings_;
    max_crypto_buffer_ = config.max_crypto_buffer_;
    settings_.initial_ts = uv_hrtime();
  }

  void ResetToDefaults();

  // QuicSessionConfig::Set() pulls values out of the AliasedBuffer
  // defined in node_quic_state.h and stores the values in settings_.
  // If preferred_addr is not nullptr, it is copied into the
  // settings_.preferred_addr field
  void Set(Environment* env,
           const struct sockaddr* preferred_addr = nullptr);

  void SetOriginalConnectionID(const ngtcp2_cid* ocid);

  // Generates the stateless reset token for the settings_
  void GenerateStatelessResetToken();

  // If the preferred address is set, generates the associated tokens
  void GeneratePreferredAddressToken(ngtcp2_cid* pscid);

  uint64_t GetMaxCryptoBuffer() const { return max_crypto_buffer_; }

  void SetQlog(const ngtcp2_qlog_settings& qlog);

  const ngtcp2_settings& operator*() const { return settings_; }
  const ngtcp2_settings* operator->() const { return &settings_; }

  const ngtcp2_settings* data() { return &settings_; }

 private:
  uint64_t max_crypto_buffer_ = DEFAULT_MAX_CRYPTO_BUFFER;
  ngtcp2_settings settings_;
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


// The QuicSessionState enums are used with the QuicSession's
// private state_ array. This is exposed to JavaScript via an
// aliased buffer and is used to communicate various types of
// state efficiently across the native/JS boundary.
enum QuicSessionState : int {
  // Communicates whether a 'keylog' event listener has been
  // registered on the JavaScript QuicSession object. The
  // value will be either 1 or 0. When set to 1, the native
  // code will emit TLS keylog entries to the JavaScript
  // side triggering the 'keylog' event once for each line.
  IDX_QUIC_SESSION_STATE_KEYLOG_ENABLED,

  // Communicates whether a 'clientHello' event listener has
  // been registered on the JavaScript QuicServerSession.
  // The value will be either 1 or 0. When set to 1, the
  // native code will callout to the JavaScript side causing
  // the 'clientHello' event to be emitted. This is only
  // used on server QuicSession instances.
  IDX_QUIC_SESSION_STATE_CLIENT_HELLO_ENABLED,

  // Communicates whether a 'cert' event listener has been
  // registered on the JavaScript QuicSession. The value will
  // be either 1 or 0. When set to 1, the native code will
  // callout to the JavaScript side causing the 'cert' event
  // to be emitted.
  IDX_QUIC_SESSION_STATE_CERT_ENABLED,

  // Communicates whether a 'pathValidation' event listener
  // has been registered on the JavaScript QuicSession. The
  // value will be either 1 or 0. When set to 1, the native
  // code will callout to the JavaScript side causing the
  // 'pathValidation' event to be emitted
  IDX_QUIC_SESSION_STATE_PATH_VALIDATED_ENABLED,

  // Communicates the current max cumulative number of
  // bidi and uni streams that may be opened on the session
  IDX_QUIC_SESSION_STATE_MAX_STREAMS_BIDI,
  IDX_QUIC_SESSION_STATE_MAX_STREAMS_UNI,

  // Communicates the current maxinum number of bytes that
  // the local endpoint can send in this connection
  // (updated immediately after processing sent/received packets)
  IDX_QUIC_SESSION_STATE_MAX_DATA_LEFT,

  // Communicates the current total number of bytes in flight
  IDX_QUIC_SESSION_STATE_BYTES_IN_FLIGHT,

  // Just the number of session state enums for use when
  // creating the AliasedBuffer.
  IDX_QUIC_SESSION_STATE_COUNT
};

enum QuicSessionStatsIdx : int {
  IDX_QUIC_SESSION_STATS_CREATED_AT,
  IDX_QUIC_SESSION_STATS_HANDSHAKE_START_AT,
  IDX_QUIC_SESSION_STATS_HANDSHAKE_SEND_AT,
  IDX_QUIC_SESSION_STATS_HANDSHAKE_CONTINUE_AT,
  IDX_QUIC_SESSION_STATS_HANDSHAKE_COMPLETED_AT,
  IDX_QUIC_SESSION_STATS_HANDSHAKE_ACKED_AT,
  IDX_QUIC_SESSION_STATS_SENT_AT,
  IDX_QUIC_SESSION_STATS_RECEIVED_AT,
  IDX_QUIC_SESSION_STATS_CLOSING_AT,
  IDX_QUIC_SESSION_STATS_BYTES_RECEIVED,
  IDX_QUIC_SESSION_STATS_BYTES_SENT,
  IDX_QUIC_SESSION_STATS_BIDI_STREAM_COUNT,
  IDX_QUIC_SESSION_STATS_UNI_STREAM_COUNT,
  IDX_QUIC_SESSION_STATS_STREAMS_IN_COUNT,
  IDX_QUIC_SESSION_STATS_STREAMS_OUT_COUNT,
  IDX_QUIC_SESSION_STATS_KEYUPDATE_COUNT,
  IDX_QUIC_SESSION_STATS_RETRY_COUNT,
  IDX_QUIC_SESSION_STATS_LOSS_RETRANSMIT_COUNT,
  IDX_QUIC_SESSION_STATS_ACK_DELAY_RETRANSMIT_COUNT,
  IDX_QUIC_SESSION_STATS_PATH_VALIDATION_SUCCESS_COUNT,
  IDX_QUIC_SESSION_STATS_PATH_VALIDATION_FAILURE_COUNT,
  IDX_QUIC_SESSION_STATS_MAX_BYTES_IN_FLIGHT,
  IDX_QUIC_SESSION_STATS_BLOCK_COUNT,
};

class QuicSessionListener {
 public:
  virtual ~QuicSessionListener();

  virtual void OnKeylog(const char* str, size_t size);
  virtual void OnClientHello(
      const char* alpn,
      const char* server_name);
  virtual void OnCert(const char* server_name);
  virtual void OnOCSP(const std::string& ocsp);
  virtual void OnStreamHeaders(
      int64_t stream_id,
      int kind,
      const std::vector<std::unique_ptr<QuicHeader>>& headers);
  virtual void OnStreamClose(
      int64_t stream_id,
      uint64_t app_error_code);
  virtual void OnStreamReset(
      int64_t stream_id,
      uint64_t final_size,
      uint64_t app_error_code);
  virtual void OnSessionDestroyed();
  virtual void OnSessionClose(QuicError error);
  virtual void OnStreamReady(BaseObjectPtr<QuicStream> stream);
  virtual void OnHandshakeCompleted();
  virtual void OnPathValidation(
      ngtcp2_path_validation_result res,
      const sockaddr* local,
      const sockaddr* remote);
  virtual void OnSessionTicket(int size, SSL_SESSION* session);
  virtual void OnSessionSilentClose(bool stateless_reset, QuicError error);
  virtual void OnVersionNegotiation(
      uint32_t supported_version,
      const uint32_t* versions,
      size_t vcnt);
  virtual void OnQLog(const uint8_t* data, size_t len);

  QuicSession* Session() { return session_; }

 private:
  QuicSession* session_ = nullptr;
  QuicSessionListener* previous_listener_ = nullptr;
  friend class QuicSession;
};

class JSQuicSessionListener : public QuicSessionListener {
 public:
  void OnKeylog(const char* str, size_t size) override;
  void OnClientHello(
      const char* alpn,
      const char* server_name) override;
  void OnCert(const char* server_name) override;
  void OnOCSP(const std::string& ocsp) override;
  void OnStreamHeaders(
      int64_t stream_id,
      int kind,
      const std::vector<std::unique_ptr<QuicHeader>>& headers) override;
  void OnStreamClose(
      int64_t stream_id,
      uint64_t app_error_code) override;
  void OnStreamReset(
      int64_t stream_id,
      uint64_t final_size,
      uint64_t app_error_code) override;
  void OnSessionDestroyed() override;
  void OnSessionClose(QuicError error) override;
  void OnStreamReady(BaseObjectPtr<QuicStream> stream) override;
  void OnHandshakeCompleted() override;
  void OnPathValidation(
      ngtcp2_path_validation_result res,
      const sockaddr* local,
      const sockaddr* remote) override;
  void OnSessionTicket(int size, SSL_SESSION* session) override;
  void OnSessionSilentClose(bool stateless_reset, QuicError error) override;
  void OnVersionNegotiation(
      uint32_t supported_version,
      const uint32_t* versions,
      size_t vcnt) override;
  void OnQLog(const uint8_t* data, size_t len) override;

 private:
  friend class QuicSession;
};

// Currently, we generate Connection IDs randomly. In the future,
// however, there may be other strategies that need to be supported.
class ConnectionIDStrategy {
 public:
  virtual void GetNewConnectionID(
      QuicSession* session,
      ngtcp2_cid* cid,
      size_t cidlen) = 0;
};

class RandomConnectionIDStrategy : public ConnectionIDStrategy {
 public:
  void GetNewConnectionID(
      QuicSession* session,
      ngtcp2_cid* cid,
      size_t cidlen) override;
};

class StatelessResetTokenStrategy {
 public:
  virtual void GetNewStatelessToken(
      QuicSession* session,
      ngtcp2_cid* cid,
      uint8_t* token,
      size_t tokenlen) = 0;
};

class CryptoStatelessResetTokenStrategy : public StatelessResetTokenStrategy {
 public:
  void GetNewStatelessToken(
      QuicSession* session,
      ngtcp2_cid* cid,
      uint8_t* token,
      size_t tokenlen) override;
};

// The QuicCryptoContext class encapsulates all of the crypto/TLS
// handshake details on behalf of a QuicSession.
class QuicCryptoContext : public MemoryRetainer {
 public:
  uint64_t Cancel();

  // Outgoing crypto data must be retained in memory until it is
  // explicitly acknowledged.
  void AcknowledgeCryptoData(ngtcp2_crypto_level level, size_t datalen);

  // Enables openssl's TLS tracing mechanism
  void EnableTrace();

  // Returns the server's prepared OCSP response for transmission. This
  // is not used by client QuicSession instances.
  std::string GetOCSPResponse();

  ngtcp2_crypto_level GetReadCryptoLevel();

  ngtcp2_crypto_level GetWriteCryptoLevel();

  bool IsOptionSet(uint32_t option) const {
    return options_ & option;
  }

  // Emits a single keylog line to the JavaScript layer
  void Keylog(const char* line);

  int OnClientHello();

  void OnClientHelloDone();

  int OnOCSP();

  void OnOCSPDone(
      crypto::SecureContext* context,
      v8::Local<v8::Value> ocsp_response);

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
  void ResumeHandshake();

  void SetOption(uint32_t option, bool on = true) {
    if (on)
      options_ |= option;
    else
      options_ &= ~option;
  }

  bool SetSession(const unsigned char* data, size_t len);

  void SetTLSAlert(int err);

  bool SetupInitialKey(const ngtcp2_cid* dcid);

  ngtcp2_crypto_side Side() const { return side_; }

  SSL* ssl() { return ssl_.get(); }
  SSL* operator->() { return ssl_.get(); }

  void WriteHandshake(
      ngtcp2_crypto_level level,
      const uint8_t* data,
      size_t datalen);

  bool InitiateKeyUpdate();

  bool KeyUpdate(
    uint8_t* rx_key,
    uint8_t* rx_iv,
    uint8_t* tx_key,
    uint8_t* tx_iv);

  int VerifyPeerIdentity(const char* hostname);

  void MemoryInfo(MemoryTracker* tracker) const override;

  SET_MEMORY_INFO_NAME(QuicCryptoContext)
  SET_SELF_SIZE(QuicCryptoContext)

 private:
  QuicCryptoContext(
      QuicSession* session,
      crypto::SecureContext* ctx,
      ngtcp2_crypto_side side,
      uint32_t options);

  QuicSession* session_;
  ngtcp2_crypto_side side_;
  crypto::SSLPointer ssl_;
  std::vector<uint8_t> tx_secret_;
  std::vector<uint8_t> rx_secret_;
  QuicBuffer handshake_[3];
  bool in_tls_callback_ = false;
  bool in_key_update_ = false;
  bool in_ocsp_request_ = false;
  bool in_client_hello_ = false;
  uint32_t options_;

  v8::Global<v8::ArrayBufferView> ocsp_response_;
  crypto::BIOPointer bio_trace_;

  class TLSCallbackScope {
   public:
    explicit TLSCallbackScope(QuicCryptoContext* context) :
        context_(context) {
      context_->in_tls_callback_ = true;
    }

    ~TLSCallbackScope() {
      context_->in_tls_callback_ = false;
    }

    static bool IsInCallback(QuicCryptoContext* context) {
      return context->in_tls_callback_;
    }

   private:
    QuicCryptoContext* context_;
  };

  class TLSHandshakeScope {
   public:
    TLSHandshakeScope(
        QuicCryptoContext* context,
        bool* monitor) :
        context_(context),
        monitor_(monitor) {}

    ~TLSHandshakeScope() {
      if (!IsHandshakeSuspended())
        return;

      *monitor_ = false;
      // Only continue the TLS handshake if we are not currently running
      // synchronously within the TLS handshake function. This can happen
      // when the callback function passed to the clientHello and cert
      // event handlers is called synchronously. If the function is called
      // asynchronously, then we have to manually continue the handshake.
      if (!TLSCallbackScope::IsInCallback(context_))
        context_->ResumeHandshake();
    }

   private:
    bool IsHandshakeSuspended() const {
      return context_->in_ocsp_request_ || context_->in_client_hello_;
    }


    QuicCryptoContext* context_;
    bool* monitor_;
  };

  friend class QuicSession;
};

// A QuicApplication encapsulates the specific details of
// working with a specific QUIC application (e.g. http/3).
class QuicApplication : public MemoryRetainer {
 public:
  explicit QuicApplication(QuicSession* session);
  virtual ~QuicApplication() = default;

  virtual bool Initialize() = 0;
  virtual bool ReceiveStreamData(
      int64_t stream_id,
      int fin,
      const uint8_t* data,
      size_t datalen,
      uint64_t offset) = 0;
  virtual void AcknowledgeStreamData(
      int64_t stream_id,
      uint64_t offset,
      size_t datalen) = 0;
  virtual void ExtendMaxStreamsRemoteUni(uint64_t max_streams) {}
  virtual void ExtendMaxStreamsRemoteBidi(uint64_t max_streams) {}
  virtual void ExtendMaxStreamData(int64_t stream_id, uint64_t max_data) {}
  virtual bool SendPendingData() = 0;
  virtual bool SendStreamData(QuicStream* stream) = 0;
  virtual void StreamHeaders(
      int64_t stream_id,
      int kind,
      const std::vector<std::unique_ptr<QuicHeader>>& headers);
  virtual void StreamClose(
      int64_t stream_id,
      uint64_t app_error_code);
  virtual void StreamOpen(int64_t stream_id) {}
  virtual void StreamReset(
      int64_t stream_id,
      uint64_t final_size,
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

  inline Environment* env() const;

 protected:
  QuicSession* Session() const { return session_; }
  bool NeedsInit() const { return needs_init_; }
  void SetInitDone() { needs_init_ = false; }
  std::unique_ptr<QuicPacket> CreateStreamDataPacket();

 private:
  QuicSession* session_;
  bool needs_init_ = true;
};

// The QuicSession class is an virtual class that serves as
// the basis for both client and server QuicSession.
// It implements the functionality that is shared for both
// QUIC clients and servers.
//
// QUIC sessions are virtual connections that exchange data
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
class QuicSession : public AsyncWrap,
                    public mem::NgLibMemoryManager<QuicSession, ngtcp2_mem> {
 public:
  static void Initialize(
      Environment* env,
      v8::Local<v8::Object> target,
      v8::Local<v8::Context> context);

  static BaseObjectPtr<QuicSession> CreateServer(
      QuicSocket* socket,
      QuicSessionConfig* config,
      const ngtcp2_cid* rcid,
      const SocketAddress& local_addr,
      const struct sockaddr* remote_addr,
      const ngtcp2_cid* dcid,
      const ngtcp2_cid* ocid,
      uint32_t version,
      const std::string& alpn = NGTCP2_ALPN_H3,
      uint32_t options = 0,
      uint64_t initial_connection_close = NGTCP2_NO_ERROR,
      QlogMode qlog = QlogMode::kDisabled);

  static BaseObjectPtr<QuicSession> CreateClient(
      QuicSocket* socket,
      const struct sockaddr* addr,
      crypto::SecureContext* context,
      v8::Local<v8::Value> early_transport_params,
      v8::Local<v8::Value> session_ticket,
      v8::Local<v8::Value> dcid,
      SelectPreferredAddressPolicy select_preferred_address_policy =
          QUIC_PREFERRED_ADDRESS_IGNORE,
      const std::string& alpn = NGTCP2_ALPN_H3,
      const std::string& hostname = "",
      uint32_t options = 0,
      QlogMode qlog = QlogMode::kDisabled);

  static const int kInitialClientBufferLength = 4096;

  // The QuiSession::CryptoContext encapsulates all details of the
  // TLS context on behalf of the QuicSession.
  QuicSession(
      ngtcp2_crypto_side side,
      // The QuicSocket that created this session. Note that
      // it is possible to replace this socket later, after
      // the TLS handshake has completed. The QuicSession
      // should never assume that the socket will always
      // remain the same.
      QuicSocket* socket,
      v8::Local<v8::Object> wrap,
      crypto::SecureContext* ctx,
      AsyncWrap::ProviderType provider_type,
      // QUIC is generally just a transport. The ALPN identifier
      // is used to specify the application protocol that is
      // layered on top. If not specified, this will default
      // to the HTTP/3 identifier. For QUIC, the alpn identifier
      // is always required.
      const std::string& alpn,
      const std::string& hostname,
      const ngtcp2_cid* rcid,
      uint32_t options = 0,
      SelectPreferredAddressPolicy select_preferred_address_policy =
          QUIC_PREFERRED_ADDRESS_ACCEPT,
      uint64_t initial_connection_close = NGTCP2_NO_ERROR);

  // Server Constructor
  QuicSession(
      QuicSocket* socket,
      QuicSessionConfig* config,
      v8::Local<v8::Object> wrap,
      const ngtcp2_cid* rcid,
      const SocketAddress& local_addr,
      const struct sockaddr* remote_addr,
      const ngtcp2_cid* dcid,
      const ngtcp2_cid* ocid,
      uint32_t version,
      const std::string& alpn,
      uint32_t options,
      uint64_t initial_connection_close,
      QlogMode qlog);

  // Client Constructor
  QuicSession(
      QuicSocket* socket,
      v8::Local<v8::Object> wrap,
      const SocketAddress& local_addr,
      const struct sockaddr* remote_addr,
      crypto::SecureContext* context,
      v8::Local<v8::Value> early_transport_params,
      v8::Local<v8::Value> session_ticket,
      v8::Local<v8::Value> dcid,
      SelectPreferredAddressPolicy select_preferred_address_policy,
      const std::string& alpn,
      const std::string& hostname,
      uint32_t options,
      QlogMode qlog);

  ~QuicSession() override;

  std::string diagnostic_name() const override;

  QuicApplication* Application() { return application_.get(); }

  enum InitialPacketResult : int {
    PACKET_OK,
    PACKET_IGNORE,
    PACKET_VERSION,
    PACKET_RETRY
  };

  static InitialPacketResult Accept(
    ngtcp2_pkt_hd* hd,
    uint32_t version,
    const uint8_t* data,
    ssize_t nread);

  QuicCryptoContext* CryptoContext() { return crypto_context_.get(); }
  QuicSessionListener* Listener() { return listener_; }

  QuicStream* CreateStream(int64_t id);
  QuicStream* FindStream(int64_t id);

  inline bool HasStream(int64_t id);

  inline QuicError GetLastError() const;

  // Returns true if StartGracefulClose() has been called and the
  // QuicSession is currently in the process of a graceful close.
  inline bool IsGracefullyClosing() const;

  // Returns true if Destroy() has been called and the
  // QuicSession is no longer usable.
  inline bool IsDestroyed() const;

  inline size_t GetMaxPacketLength() const;

  // Returns true if the QuicSession has entered the
  // closing period following a call to ImmediateClose.
  // While true, the QuicSession is only permitted to
  // transmit CONNECTION_CLOSE frames until either the
  // idle timeout period elapses or until the QuicSession
  // is explicitly destroyed.
  inline bool IsInClosingPeriod();

  // Returns true if the QuicSession has received a
  // CONNECTION_CLOSE frame from the peer. Once in
  // the draining period, the QuicSession is not
  // permitted to send any frames to the peer. The
  // QuicSession will be silently closed after either
  // the idle timeout period elapses or until the
  // QuicSession is explicitly destroyed.
  inline bool IsInDrainingPeriod();

  inline bool IsServer() const;

  // Starting a GracefulClose disables the ability to open or accept
  // new streams for this session. Existing streams are allowed to
  // close naturally on their own. Once called, the QuicSession will
  // be immediately closed once there are no remaining streams. Note
  // that no notification is given to the connecting peer that we're
  // in a graceful closing state. A CONNECTION_CLOSE will be sent only
  // once ImmediateClose() is called.
  inline void StartGracefulClose();

  // Get the ALPN protocol identifier configured for this QuicSession.
  // For server sessions, this will be compared against the client requested
  // ALPN identifier to determine if there is a protocol match.
  const std::string& GetALPN() const { return alpn_; }

  // Get the hostname configured for this QuicSession. This is generally
  // only used by client sessions.
  const std::string& GetHostname() const { return hostname_; }

  // Returns the associated peer's address. Note that this
  // value can change over the lifetime of the QuicSession.
  // The fact that the session is not tied intrinsically to
  // a single address is one of the benefits of QUIC.
  const SocketAddress& GetRemoteAddress() const { return remote_address_; }

  const ngtcp2_cid* scid() const { return &scid_; }

  // Only used with server sessions
  ngtcp2_cid* pscid() { return &pscid_; }

  // Only used with server sessions
  const ngtcp2_cid* rcid() const { return &rcid_; }

  inline QuicSocket* Socket() const;

  ngtcp2_conn* Connection() { return connection_.get(); }

  void AddStream(BaseObjectPtr<QuicStream> stream);
  void AddToSocket(QuicSocket* socket);

  // Immediately discards the state of the QuicSession
  // and renders the QuicSession instance completely
  // unusable.
  void Destroy();

  // Extends the QUIC stream flow control window. This is
  // called after received data has been consumed and we
  // want to allow the peer to send more data.
  void ExtendStreamOffset(QuicStream* stream, size_t amount);

  // Extends the QUIC session flow control window
  void ExtendOffset(size_t amount);

  // Retrieve the local transport parameters established for
  // this ngtcp2_conn
  void GetLocalTransportParams(ngtcp2_transport_params* params);

  // The QUIC version that has been negotiated for this session
  uint32_t GetNegotiatedVersion();

  // True only if ngtcp2 considers the TLS handshake to be completed
  bool IsHandshakeCompleted();

  // Checks to see if data needs to be retransmitted
  void MaybeTimeout();

  // Called when the session has been determined to have been
  // idle for too long and needs to be torn down.
  void OnIdleTimeout();

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
      const struct sockaddr* remote_addr,
      unsigned int flags);

  // Receive a chunk of QUIC stream data received from the peer
  bool ReceiveStreamData(
      int64_t stream_id,
      int fin,
      const uint8_t* data,
      size_t datalen,
      uint64_t offset);

  void RemoveStream(int64_t stream_id);
  void RemoveFromSocket();

  // Causes pending ngtcp2 frames to be serialized and sent
  void SendPendingData();

  // Causes pending QuicStream data to be serialized and sent
  bool SendStreamData(QuicStream* stream);

  bool SendPacket(
      std::unique_ptr<QuicPacket> packet,
      const ngtcp2_path_storage& path);

  inline uint64_t GetMaxDataLeft();

  inline void SetLastError(
      QuicError error = {
          uint32_t{QUIC_ERROR_SESSION},
          uint64_t{NGTCP2_NO_ERROR}
      });
  inline void SetLastError(int32_t family, uint64_t error_code);
  inline void SetLastError(int32_t family, int error_code);

  inline uint64_t GetMaxLocalStreamsUni();

  int SetRemoteTransportParams(ngtcp2_transport_params* params);
  bool SetEarlyTransportParams(v8::Local<v8::Value> buffer);
  bool SetSocket(QuicSocket* socket, bool nat_rebinding = false);
  int SetSession(SSL_SESSION* session);
  bool SetSession(v8::Local<v8::Value> buffer);

  std::map<int64_t, BaseObjectPtr<QuicStream>>* GetStreams() {
    return &streams_;
  }

  // ResetStream will cause ngtcp2 to queue a
  // RESET_STREAM and STOP_SENDING frame, as appropriate,
  // for the given stream_id. For a locally-initiated
  // unidirectional stream, only a RESET_STREAM frame
  // will be scheduled and the stream will be immediately
  // closed. For a bi-directional stream, a STOP_SENDING
  // frame will be sent.
  //
  // It is important to note that the QuicStream is
  // not destroyed immediately following ShutdownStream.
  // The sending QuicSession will not close the stream
  // until the RESET_STREAM is acknowledged.
  //
  // Once the RESET_STREAM is sent, the QuicSession
  // should not send any new frames for the stream,
  // and all inbound stream frames should be discarded.
  // Once ngtcp2 receives the appropriate notification
  // that the RESET_STREAM has been acknowledged, the
  // stream will be closed.
  //
  // Once the stream has been closed, it will be
  // destroyed and memory will be freed. User code
  // can request that a stream be immediately and
  // abruptly destroyed without calling ShutdownStream.
  // Likewise, an idle timeout may cause the stream
  // to be silently destroyed without calling
  // ShutdownStream.
  void ResetStream(
      int64_t stream_id,
      uint64_t error_code = NGTCP2_APP_NOERROR);

  // Submits informational headers to the QUIC Application
  // implementation. If headers are not supported, false
  // will be returned. Otherwise, returns true
  bool SubmitInformation(
      int64_t stream_id,
      v8::Local<v8::Array> headers);

  // Submits initial headers to the QUIC Application
  // implementation. If headers are not supported, false
  // will be returned. Otherwise, returns true
  bool SubmitHeaders(
      int64_t stream_id,
      v8::Local<v8::Array> headers,
      uint32_t flags);

  // Submits trailing headers to the QUIC Application
  // implementation. If headers are not supported, false
  // will be returned. Otherwise, returns true
  bool SubmitTrailers(
      int64_t stream_id,
      v8::Local<v8::Array> headers);

  // Error handling for the QuicSession. client and server
  // instances will do different things here, but ultimately
  // an error means that the QuicSession
  // should be torn down.
  void HandleError();

  bool SendConnectionClose();

  // Implementation for mem::NgLibMemoryManager
  inline void CheckAllocatedSize(size_t previous_size) const;
  inline void IncreaseAllocatedSize(size_t size);
  inline void DecreaseAllocatedSize(size_t size);

  // Immediately close the QuicSession. All currently open
  // streams are implicitly reset and closed with RESET_STREAM
  // and STOP_SENDING frames transmitted as necessary. A
  // CONNECTION_CLOSE frame will be sent and the session
  // will enter the closing period until either the idle
  // timeout period elapses or until the QuicSession is
  // explicitly destroyed. During the closing period,
  // the only frames that may be transmitted to the peer
  // are repeats of the already sent CONNECTION_CLOSE.
  //
  // The CONNECTION_CLOSE will use the error code set using
  // the most recent call to SetLastError()
  void ImmediateClose();

  // Silently, and immediately close the QuicSession. This is
  // generally only done during an idle timeout. That is, per
  // the QUIC specification, if the session remains idle for
  // longer than both the advertised idle timeout and three
  // times the current probe timeout (PTO). In such cases, all
  // currently open streams are implicitly reset and closed
  // without sending corresponding RESET_STREAM and
  // STOP_SENDING frames, the connection state is
  // discarded, and the QuicSession is destroyed without
  // sending a CONNECTION_CLOSE frame.
  //
  // Silent close may also be used to explicitly destroy
  // a QuicSession that has either already entered the
  // closing or draining periods; or in response to user
  // code requests to forcefully terminate a QuicSession
  // without transmitting any additional frames to the
  // peer.
  void SilentClose(bool stateless_reset = false);

  void PushListener(QuicSessionListener* listener);
  void RemoveListener(QuicSessionListener* listener);

  void SetConnectionIDStrategory(ConnectionIDStrategy* strategy);
  void SetStatelessResetTokenStrategy(StatelessResetTokenStrategy* strategy);

  // Report that the stream data is flow control blocked
  void StreamDataBlocked(int64_t stream_id);

  // Tracks whether or not we are currently within an ngtcp2 callback
  // function. Certain ngtcp2 APIs are not supposed to be called when
  // within a callback. We use this as a gate to check.
  class Ngtcp2CallbackScope {
   public:
    explicit Ngtcp2CallbackScope(QuicSession* session) : session_(session) {
      CHECK(!InNgtcp2CallbackScope(session));
      session_->SetFlag(QUICSESSION_FLAG_NGTCP2_CALLBACK);
    }

    ~Ngtcp2CallbackScope() {
      session_->SetFlag(QUICSESSION_FLAG_NGTCP2_CALLBACK, false);
    }

    static bool InNgtcp2CallbackScope(QuicSession* session) {
      return session->IsFlagSet(QUICSESSION_FLAG_NGTCP2_CALLBACK);
    }

   private:
    QuicSession* session_;
  };

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(QuicSession)
  SET_SELF_SIZE(QuicSession)

 private:
  // Initialize the QuicSession as a server
  void InitServer(
      QuicSessionConfig* config,
      const SocketAddress& local_addr,
      const struct sockaddr* remote_addr,
      const ngtcp2_cid* dcid,
      const ngtcp2_cid* ocid,
      uint32_t version,
      QlogMode qlog);

  // Initialize the QuicSession as a client
  bool InitClient(
      const SocketAddress& local_addr,
      const struct sockaddr* remote_addr,
      v8::Local<v8::Value> early_transport_params,
      v8::Local<v8::Value> session_ticket,
      v8::Local<v8::Value> dcid,
      QlogMode qlog);

  void InitApplication();

  void AckedStreamDataOffset(
      int64_t stream_id,
      uint64_t offset,
      size_t datalen);
  void AssociateCID(ngtcp2_cid* cid);

  void DisassociateCID(const ngtcp2_cid* cid);
  void ExtendMaxStreamData(int64_t stream_id, uint64_t max_data);
  void ExtendMaxStreams(bool bidi, uint64_t max_streams);
  void ExtendMaxStreamsUni(uint64_t max_streams);
  void ExtendMaxStreamsBidi(uint64_t max_streams);
  void ExtendMaxStreamsRemoteUni(uint64_t max_streams);
  void ExtendMaxStreamsRemoteBidi(uint64_t max_streams);
  int GetNewConnectionID(ngtcp2_cid* cid, uint8_t* token, size_t cidlen);
  void GetConnectionCloseInfo();
  void HandshakeCompleted();
  void PathValidation(
    const ngtcp2_path* path,
    ngtcp2_path_validation_result res);
  bool ReceiveClientInitial(const ngtcp2_cid* dcid);
  bool ReceivePacket(ngtcp2_path* path, const uint8_t* data, ssize_t nread);
  bool ReceiveRetry();
  void RemoveConnectionID(const ngtcp2_cid* cid);
  void ScheduleRetransmit();
  bool SelectPreferredAddress(
    ngtcp2_addr* dest,
    const ngtcp2_preferred_addr* paddr);
  bool SendPacket(std::unique_ptr<QuicPacket> packet);
  void SetLocalAddress(const ngtcp2_addr* addr);
  void StreamClose(int64_t stream_id, uint64_t app_error_code);
  void StreamOpen(int64_t stream_id);
  void StreamReset(
      int64_t stream_id,
      uint64_t final_size,
      uint64_t app_error_code);
  bool WritePackets(const char* diagnostic_label = nullptr);
  void UpdateRecoveryStats();
  void UpdateDataStats();
  void UpdateEndpoint(const ngtcp2_path& path);

  void VersionNegotiation(
      const ngtcp2_pkt_hd* hd,
      const uint32_t* sv,
      size_t nsv);

  // static ngtcp2 callbacks
  static int OnClientInitial(
      ngtcp2_conn* conn,
      void* user_data);
  static int OnReceiveClientInitial(
      ngtcp2_conn* conn,
      const ngtcp2_cid* dcid,
      void* user_data);
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
  static int OnEncrypt(
      ngtcp2_conn* conn,
      uint8_t* dest,
      const ngtcp2_crypto_aead* aead,
      const uint8_t* plaintext,
      size_t plaintextlen,
      const uint8_t* key,
      const uint8_t* nonce,
      size_t noncelen,
      const uint8_t* ad,
      size_t adlen,
      void* user_data);
  static int OnDecrypt(
      ngtcp2_conn* conn,
      uint8_t* dest,
      const ngtcp2_crypto_aead* aead,
      const uint8_t* ciphertext,
      size_t ciphertextlen,
      const uint8_t* key,
      const uint8_t* nonce,
      size_t noncelen,
      const uint8_t* ad,
      size_t adlen,
      void* user_data);
  static int OnHPMask(
      ngtcp2_conn* conn,
      uint8_t* dest,
      const ngtcp2_crypto_cipher* hp,
      const uint8_t* hp_key,
      const uint8_t* sample,
      void* user_data);
  static int OnReceiveStreamData(
      ngtcp2_conn* conn,
      int64_t stream_id,
      int fin,
      uint64_t offset,
      const uint8_t* data,
      size_t datalen,
      void* user_data,
      void* stream_user_data);
  static int OnReceiveRetry(
      ngtcp2_conn* conn,
      const ngtcp2_pkt_hd* hd,
      const ngtcp2_pkt_retry* retry,
      void* user_data);
  static int OnAckedCryptoOffset(
      ngtcp2_conn* conn,
      ngtcp2_crypto_level crypto_level,
      uint64_t offset,
      size_t datalen,
      void* user_data);
  static int OnAckedStreamDataOffset(
      ngtcp2_conn* conn,
      int64_t stream_id,
      uint64_t offset,
      size_t datalen,
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
  static int OnUpdateKey(
      ngtcp2_conn* conn,
      uint8_t* rx_key,
      uint8_t* rx_iv,
      uint8_t* tx_key,
      uint8_t* tx_iv,
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
  static void OnQlogWrite(void* user_data, const void* data, size_t len);

  void UpdateIdleTimer();
  void UpdateRetransmitTimer(uint64_t timeout);
  void StopRetransmitTimer();
  void StopIdleTimer();
  bool StartClosingPeriod();

  enum QuicSessionFlags : uint32_t {
    // Initial state when a QuicSession is created but nothing yet done.
    QUICSESSION_FLAG_INITIAL = 0x1,

    // Set while the QuicSession is in the process of an Immediate
    // or silent close.
    QUICSESSION_FLAG_CLOSING = 0x2,

    // Set while the QuicSession is in the process of a graceful close.
    QUICSESSION_FLAG_GRACEFUL_CLOSING = 0x4,

    // Set when the QuicSession has been destroyed (but not
    // yet freed)
    QUICSESSION_FLAG_DESTROYED = 0x8,

    QUICSESSION_FLAG_HAS_TRANSPORT_PARAMS = 0x10,

    // Set while the QuicSession is executing an ngtcp2 callback
    QUICSESSION_FLAG_NGTCP2_CALLBACK = 0x100,

    // Set if the QuicSession is in the middle of a silent close
    // (that is, a CONNECTION_CLOSE should not be sent)
    QUICSESSION_FLAG_SILENT_CLOSE = 0x200,

    QUICSESSION_FLAG_HANDSHAKE_RX = 0x400,
    QUICSESSION_FLAG_HANDSHAKE_TX = 0x800,
    QUICSESSION_FLAG_HANDSHAKE_KEYS =
        QUICSESSION_FLAG_HANDSHAKE_RX |
        QUICSESSION_FLAG_HANDSHAKE_TX,
    QUICSESSION_FLAG_SESSION_RX = 0x1000,
    QUICSESSION_FLAG_SESSION_TX = 0x2000,
    QUICSESSION_FLAG_SESSION_KEYS =
        QUICSESSION_FLAG_SESSION_RX |
        QUICSESSION_FLAG_SESSION_TX
  };

  void SetFlag(QuicSessionFlags flag, bool on = true) {
    if (on)
      flags_ |= flag;
    else
      flags_ &= ~flag;
  }

  bool IsFlagSet(QuicSessionFlags flag) const {
    return (flags_ & flag) == flag;
  }

  void IncrementConnectionCloseAttempts() {
    if (connection_close_attempts_ < kMaxSizeT)
      connection_close_attempts_++;
  }

  bool ShouldAttemptConnectionClose() {
    if (connection_close_attempts_ == connection_close_limit_) {
      if (connection_close_limit_ * 2 <= kMaxSizeT)
        connection_close_limit_ *= 2;
      else
        connection_close_limit_ = kMaxSizeT;
      return true;
    }
    return false;
  }

  typedef ssize_t(*ngtcp2_close_fn)(
    ngtcp2_conn* conn,
    ngtcp2_path* path,
    uint8_t* dest,
    size_t destlen,
    uint64_t error_code,
    ngtcp2_tstamp ts);

  static inline ngtcp2_close_fn SelectCloseFn(uint32_t family) {
    if (family == QUIC_ERROR_APPLICATION)
      return ngtcp2_conn_write_application_close;
    return ngtcp2_conn_write_connection_close;
  }

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
  SocketAddress local_address_;
  SocketAddress remote_address_;
  uint32_t flags_ = 0;
  uint64_t initial_connection_close_ = NGTCP2_NO_ERROR;
  size_t max_pktlen_ = 0;
  size_t max_crypto_buffer_ = DEFAULT_MAX_CRYPTO_BUFFER;
  size_t current_ngtcp2_memory_ = 0;
  size_t connection_close_attempts_ = 0;
  size_t connection_close_limit_ = 1;

  ConnectionIDStrategy* connection_id_strategy_ = nullptr;
  RandomConnectionIDStrategy default_connection_id_strategy_;

  StatelessResetTokenStrategy* stateless_reset_strategy_ = nullptr;
  CryptoStatelessResetTokenStrategy default_stateless_reset_strategy_;

  QuicSessionListener* listener_ = nullptr;
  JSQuicSessionListener default_listener_;

  TimerPointer idle_;
  TimerPointer retransmit_;

  ngtcp2_cid scid_;
  ngtcp2_cid rcid_;
  ngtcp2_cid pscid_{};
  ngtcp2_transport_params transport_params_;
  SelectPreferredAddressPolicy select_preferred_address_policy_;

  std::unique_ptr<QuicPacket> conn_closebuf_;

  std::map<int64_t, BaseObjectPtr<QuicStream>> streams_;

  AliasedFloat64Array state_;

  struct session_stats {
    // The timestamp at which the session was created
    uint64_t created_at;
    // The timestamp at which the handshake was started
    uint64_t handshake_start_at;
    // The timestamp at which the most recent handshake
    // message was sent
    uint64_t handshake_send_at;
    // The timestamp at which the most recent handshake
    // message was received
    uint64_t handshake_continue_at;
    // The timestamp at which handshake completed
    uint64_t handshake_completed_at;
    // The timestamp at which the handshake was most recently acked
    uint64_t handshake_acked_at;
    // The timestamp at which the most recently sent
    // non-handshake packets were sent
    uint64_t session_sent_at;
    // The timestamp at which the most recently received
    // non-handshake packets were received
    uint64_t session_received_at;
    // The timestamp at which a graceful close was started
    uint64_t closing_at;
    // The total number of bytes received (and not ignored)
    // by this QuicSession
    uint64_t bytes_received;
    // The total number of bytes sent by this QuicSession
    uint64_t bytes_sent;
    // The total bidirectional stream count
    uint64_t bidi_stream_count;
    // The total unidirectional stream count
    uint64_t uni_stream_count;
    // The total number of peer-initiated streams
    uint64_t streams_in_count;
    // The total number of local-initiated streams
    uint64_t streams_out_count;
    // The total number of keyupdates
    uint64_t keyupdate_count;
    // The total number of retries received
    uint64_t retry_count;
    // The total number of loss detection retransmissions
    uint64_t loss_retransmit_count;
    // The total number of ack delay retransmissions
    uint64_t ack_delay_retransmit_count;
    // The total number of successful path validations
    uint64_t path_validation_success_count;
    // The total number of failed path validations
    uint64_t path_validation_failure_count;
    // The max number of in flight bytes recorded
    uint64_t max_bytes_in_flight;
    // The total number of times the session has been
    // flow control blocked.
    uint64_t block_count;
  };
  session_stats session_stats_{};

  // crypto_rx_ack_ measures the elapsed time between crypto acks
  // for this stream. This data can be used to detect peers that are
  // generally taking too long to acknowledge crypto data.
  BaseObjectPtr<HistogramBase> crypto_rx_ack_;

  // crypto_handshake_rate_ measures the elapsed time between
  // crypto continuation steps. This data can be used to detect
  // peers that are generally taking too long to carry out the
  // handshake
  BaseObjectPtr<HistogramBase> crypto_handshake_rate_;

  struct recovery_stats {
    double min_rtt;
    double latest_rtt;
    double smoothed_rtt;
  };
  recovery_stats recovery_stats_{};

  AliasedBigUint64Array stats_buffer_;
  AliasedFloat64Array recovery_stats_buffer_;

  static const ngtcp2_conn_callbacks callbacks[2];

  friend class QuicCryptoContext;
  friend class QuicSessionListener;
  friend class JSQuicSessionListener;
};

}  // namespace quic
}  // namespace node

#endif  // NODE_WANT_INTERNALS
#endif  // SRC_NODE_QUIC_SESSION_H_
