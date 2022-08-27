#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <crypto/crypto_context.h>
#include <crypto/crypto_keys.h>
#include <memory_tracker.h>
#include <ngtcp2/ngtcp2.h>
#include <ngtcp2/ngtcp2_crypto.h>
#include <node_sockaddr.h>
#include <util.h>
#include <v8.h>
#include "quic.h"

namespace node {
namespace quic {

// =============================================================================
// SessionTicket

class SessionTicket final : public BaseObject {
 public:
  HAS_INSTANCE()
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static void Initialize(Environment* env, v8::Local<v8::Object> target);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);
  static BaseObjectPtr<SessionTicket> Create(Environment* env,
                                             Store&& ticket,
                                             Store&& transport_params);

  SessionTicket(Environment* env,
                v8::Local<v8::Object> object,
                Store&& ticket,
                Store&& transport_params);

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(SessionTicket)
  SET_SELF_SIZE(SessionTicket)

  inline uv_buf_t ticket() const { return ticket_; }
  inline ngtcp2_vec transport_params() const { return transport_params_; }

 private:
  v8::MaybeLocal<v8::Object> Encoded();

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetEncoded(const v8::FunctionCallbackInfo<v8::Value>& args);

  Store ticket_;
  Store transport_params_;
};

// =============================================================================
class StatelessResetToken final : public MemoryRetainer {
 public:
  StatelessResetToken() = default;
  StatelessResetToken(uint8_t* token, const uint8_t* secret, const CID& cid);

  // Generates a new token derived from the secret and CID
  StatelessResetToken(const uint8_t* secret, const CID& cid);

  // Copies the given token
  explicit StatelessResetToken(const uint8_t* token);

  std::string ToString() const;

  inline const uint8_t* data() const { return buf_; }

  struct Hash {
    size_t operator()(const StatelessResetToken& token) const;
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
      std::unordered_map<StatelessResetToken, T, StatelessResetToken::Hash>;

 private:
  // Generates a stateless reset token using HKDF with the cid and token secret
  // as input. The token secret is either provided by user code when an Endpoint
  // is created or is generated randomly.
  //
  // QUIC leaves the generation of stateless session tokens up to the
  // implementation to figure out. The idea, however, is that it ought to be
  // possible to generate a stateless reset token reliably even when all state
  // for a connection has been lost. We use the cid as it's the only reliably
  // consistent bit of data we have when a session is destroyed.
  bool GenerateResetToken(const uint8_t* secret, const CID& cid);

  uint8_t buf_[NGTCP2_STATELESS_RESET_TOKENLEN];
};

// =============================================================================

// The Retry Token is an encrypted token that is sent to the client by the
// server as part of the path validation flow. The plaintext format within the
// token is opaque and only meaningful the server. We can structure it any way
// we want. It needs to:
//   * be hard to guess
//   * be time limited
//   * be specific to the client address
//   * be specific to the original cid
//   * contain random data.
bool GenerateRetryPacket(const BaseObjectPtr<Packet>& packet,
                         quic_version version,
                         const uint8_t* token_secret,
                         const CID& dcid,
                         const CID& scid,
                         const SocketAddress& remote_addr);

enum class GetCertificateType {
  SELF,
  ISSUER,
};

v8::MaybeLocal<v8::Value> GetCertificateData(
    Environment* env,
    crypto::SecureContext* sc,
    GetCertificateType type = GetCertificateType::SELF);

// Validates a retry token. Returns Nothing<CID>() if the token is *not valid*,
// returns the OCID otherwise.
v8::Maybe<CID> ValidateRetryToken(quic_version version,
                                  const ngtcp2_vec& token,
                                  const SocketAddress& addr,
                                  const CID& dcid,
                                  const uint8_t* token_secret,
                                  uint64_t verification_expiration);

v8::Maybe<size_t> GenerateToken(quic_version version,
                                uint8_t* token,
                                const SocketAddress& addr,
                                const uint8_t* token_secret);

bool ValidateToken(quic_version version,
                   const ngtcp2_vec& token,
                   const SocketAddress& addr,
                   const uint8_t* token_secret,
                   uint64_t verification_expiration);

// Get the ALPN protocol identifier that was negotiated for the session
v8::Local<v8::Value> GetALPNProtocol(Session* session);

ngtcp2_crypto_level from_ossl_level(OSSL_ENCRYPTION_LEVEL ossl_level);
const char* crypto_level_name(ngtcp2_crypto_level level);

// ======================================================================================
// SessionTicketAppData is a utility class that is used only during the
// generation or access of TLS stateless sesson tickets. It exists solely to
// provide a easier way for Session::Application instances to set relevant
// metadata in the session ticket when it is created, and the exract and
// subsequently verify that data when a ticket is received and is being
// validated. The app data is completely opaque to anything other than the
// server-side of the Session::Application that sets it.
class SessionTicketAppData final {
 public:
  enum class Status {
    TICKET_USE,
    TICKET_USE_RENEW,
    TICKET_IGNORE,
    TICKET_IGNORE_RENEW
  };

  enum class Flag { STATUS_NONE, STATUS_RENEW };

  explicit SessionTicketAppData(SSL_SESSION* session);
  QUIC_NO_COPY_OR_MOVE(SessionTicketAppData)

  bool Set(const uint8_t* data, size_t len);
  bool Get(uint8_t** data, size_t* len) const;

 private:
  bool set_ = false;
  SSL_SESSION* session_;
};

// ======================================================================================
// Every Session has exactly one CryptoContext that maintains the state of the
// TLS handshake and negotiated cipher keys after the handshake has been
// completed. It is separated out from the main Session class only as a
// convenience to help make the code more maintainable and understandable.
class CryptoContext final : public MemoryRetainer {
 public:
  struct Options final : public MemoryRetainer {
    // When true, TLS keylog data will be emitted to the JavaScript session
    // object.
    bool keylog = false;

    // When set, the peer certificate is verified against the list of supplied
    // CAs. If verification fails, the connection will be refused.
    bool reject_unauthorized = true;

    // When set, the clienthello event will be emitted on the Session to allow
    // user code an opportunity to provide a different SecureContext based on
    // alpn, SNI servername, and ciphers.
    bool client_hello = false;

    // When set, enables TLS tracing for the session. This should only be used
    // for debugging.
    bool enable_tls_trace = false;

    // Options only used by server sessions:

    // When set, instructs the server session to request a client authentication
    // certificate.
    bool request_peer_certificate = false;

    // Options pnly used by client sessions:

    // When set, instructs the client session to include an OCSP request in the
    // initial TLS handshake. For server sessions, instructs the session not to
    // ignore ocsp requests.
    bool ocsp = false;

    // When set, instructs the client session to verify the hostname default.
    // This is required by QUIC and enabled by default. We allow disabling it
    // only for debugging.
    bool verify_hostname_identity = true;

    // The TLS session ID context (only used on the server)
    std::string session_id_ctx = "Node.js QUIC Server";

    // TLS cipher suite
    std::string ciphers = DEFAULT_CIPHERS;

    // TLS groups
    std::string groups = DEFAULT_GROUPS;

    // The TLS private key to use for this session.
    std::vector<std::shared_ptr<crypto::KeyObjectData>> keys;

    // Collection of certificates to use for this session.
    std::vector<Store> certs;

    // Optional certificate authority overrides to use.
    std::vector<Store> ca;

    // Optional certificate revocation lists to use.
    std::vector<Store> crl;

    void MemoryInfo(MemoryTracker* tracker) const override;
    SET_MEMORY_INFO_NAME(CryptoContext::Options)
    SET_SELF_SIZE(Options);
  };

  CryptoContext(Session* session,
                const Options& options,
                ngtcp2_crypto_side side);

  QUIC_NO_COPY_OR_MOVE(CryptoContext);

  // Returns the server's prepared OCSP response for transmission (if any). The
  // store will be empty if there was an error or if no OCSP response was
  // provided. If release is true, the internal std::shared_ptr will be reset
  // (which is the default).
  Store ocsp_response(bool release = true);

  // Returns ngtcp2's understanding of the current inbound crypto level
  ngtcp2_crypto_level read_crypto_level() const;

  // Returns ngtcp2's understanding of the current outbound crypto level
  ngtcp2_crypto_level write_crypto_level() const;

  void Start();

  // TLS Keylogging is enabled per-Session by attaching an handler to the
  // "keylog" event. Each keylog line is emitted to JavaScript where it can be
  // routed to whatever destination makes sense. Typically, this will be to a
  // keylog file that can be consumed by tools like Wireshark to intercept and
  // decrypt QUIC network traffic.
  void Keylog(const char* line) const;

  // Causes the session to emit the "client hello" callback. The TLS handshake
  // will be paused pending user code triggering the OnClientHelloDone.
  int OnClientHello();

  // TODO(@jasnell): For now, this doesn't accept any parameters. In the future,
  // we'll want to use this to allow additional keys, certs, etc to be added.
  void OnClientHelloDone();

  // The OnCert callback provides an opportunity to prompt the server to perform
  // on OCSP request on behalf of the client (when the client requests it). If
  // there is a listener for the 'OCSPRequest' event on the JavaScript side, the
  // IDX_QUIC_SESSION_STATE_CERT_ENABLED session state slot will equal 1, which
  // will cause the callback to be invoked. The callback will be given a
  // reference to a JavaScript function that must be called in order for the TLS
  // handshake to continue.
  int OnOCSP();
  void OnOCSPDone(Store&& ocsp_response);

  // When the client has requested OSCP, this function will be called to provide
  // the OSCP response. The OnOSCP() callback should have already been called by
  // this point if any data is to be provided. If it hasn't, and ocsp_response_
  // is empty, no OCSP response will be sent.
  int OnTLSStatus();

  // Called when a chunk of peer TLS handshake data is received. For every
  // chunk, we move the TLS handshake further along until it is complete.
  int Receive(ngtcp2_crypto_level crypto_level,
              uint64_t offset,
              const uint8_t* data,
              size_t datalen);

  v8::MaybeLocal<v8::Object> cert(Environment* env) const;
  v8::MaybeLocal<v8::Object> peer_cert(Environment* env) const;
  v8::MaybeLocal<v8::Value> cipher_name(Environment* env) const;
  v8::MaybeLocal<v8::Value> cipher_version(Environment* env) const;
  v8::MaybeLocal<v8::Object> ephemeral_key(Environment* env) const;
  v8::MaybeLocal<v8::Array> hello_ciphers(Environment* env) const;
  v8::MaybeLocal<v8::Value> hello_servername(Environment* env) const;
  v8::MaybeLocal<v8::Value> hello_alpn(Environment* env) const;
  std::string servername() const;
  std::string selected_alpn() const;

  // Triggers key update to begin. This will fail and return false if either a
  // previous key update is in progress and has not been confirmed or if the
  // initial handshake has not yet been confirmed.
  bool InitiateKeyUpdate();

  int VerifyPeerIdentity();
  void EnableTrace();

  inline const Session& session() const { return *session_; }
  inline ngtcp2_crypto_side side() const { return side_; }

  bool was_early_data_accepted() const;

  inline bool verify_hostname_identity() const {
    return options_.verify_hostname_identity;
  }

  int OnNewSession(SSL_SESSION* session);

  void MaybeSetEarlySession(const BaseObjectPtr<SessionTicket>& sessionTicket);

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(CryptoContext)
  SET_SELF_SIZE(CryptoContext)

 private:
  static ngtcp2_conn* getConnection(ngtcp2_crypto_conn_ref* ref);
  ngtcp2_crypto_conn_ref conn_ref_;

  void ResumeHandshake();

  Session* session_;
  Options options_;
  BaseObjectPtr<crypto::SecureContext> secure_context_;
  ngtcp2_crypto_side side_;
  crypto::SSLPointer ssl_;
  crypto::BIOPointer bio_trace_;

  bool in_tls_callback_ = false;
  bool in_ocsp_request_ = false;
  bool in_client_hello_ = false;
  bool in_key_update_ = false;
  bool early_data_ = false;

  Store ocsp_response_;

  struct CallbackScope;
  struct ResumeHandshakeScope;

  static BaseObjectPtr<crypto::SecureContext> InitializeSecureContext(
      const Session& session,
      const CryptoContext::Options& options,
      ngtcp2_crypto_side side);

  friend class Session;
  friend struct CallbackScope;
  friend struct ResumeHandshakeScope;
};

}  // namespace quic
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
