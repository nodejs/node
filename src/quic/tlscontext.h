#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#if HAVE_OPENSSL && NODE_OPENSSL_HAS_QUIC

#include <base_object.h>
#include <crypto/crypto_context.h>
#include <crypto/crypto_keys.h>
#include <memory_tracker.h>
#include <ngtcp2/ngtcp2_crypto.h>
#include "bindingdata.h"
#include "data.h"
#include "sessionticket.h"

namespace node {
namespace quic {

class Session;

// Every QUIC Session has exactly one TLSContext that maintains the state
// of the TLS handshake and negotiated cipher keys after the handshake has
// been completed. It is separated out from the main Session class only as a
// convenience to help make the code more maintainable and understandable.
class TLSContext final : public MemoryRetainer {
 public:
  enum class EncryptionLevel {
    INITIAL = NGTCP2_ENCRYPTION_LEVEL_INITIAL,
    HANDSHAKE = NGTCP2_ENCRYPTION_LEVEL_HANDSHAKE,
    ONERTT = NGTCP2_ENCRYPTION_LEVEL_1RTT,
    ZERORTT = NGTCP2_ENCRYPTION_LEVEL_0RTT,
  };

  static constexpr auto DEFAULT_CIPHERS = "TLS_AES_128_GCM_SHA256:"
                                          "TLS_AES_256_GCM_SHA384:"
                                          "TLS_CHACHA20_POLY1305_"
                                          "SHA256:TLS_AES_128_CCM_SHA256";
  static constexpr auto DEFAULT_GROUPS = "X25519:P-256:P-384:P-521";

  static inline const TLSContext& From(const SSL* ssl);
  static inline TLSContext& From(SSL* ssl);

  struct Options final : public MemoryRetainer {
    // The protocol identifier to be used by this Session.
    std::string alpn = NGHTTP3_ALPN_H3;

    // The SNI hostname to be used. This is used only by client Sessions to
    // identify the SNI host in the TLS client hello message.
    std::string hostname = "";

    // When true, TLS keylog data will be emitted to the JavaScript session.
    bool keylog = false;

    // When set, the peer certificate is verified against the list of supplied
    // CAs. If verification fails, the connection will be refused.
    bool reject_unauthorized = true;

    // When set, enables TLS tracing for the session. This should only be used
    // for debugging.
    bool enable_tls_trace = false;

    // Options only used by server sessions:

    // When set, instructs the server session to request a client authentication
    // certificate.
    bool request_peer_certificate = false;

    // Options only used by client sessions:

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
    SET_SELF_SIZE(Options)

    static const Options kDefault;

    static v8::Maybe<Options> From(Environment* env,
                                   v8::Local<v8::Value> value);
  };

  static const Options kDefaultOptions;

  TLSContext(Environment* env,
             Side side,
             Session* session,
             const Options& options);
  TLSContext(const TLSContext&) = delete;
  TLSContext(TLSContext&&) = delete;
  TLSContext& operator=(const TLSContext&) = delete;
  TLSContext& operator=(TLSContext&&) = delete;

  // Start the TLS handshake.
  void Start();

  // TLS Keylogging is enabled per-Session by attaching a handler to the
  // "keylog" event. Each keylog line is emitted to JavaScript where it can be
  // routed to whatever destination makes sense. Typically, this will be to a
  // keylog file that can be consumed by tools like Wireshark to intercept and
  // decrypt QUIC network traffic.
  void Keylog(const char* line) const;

  // Called when a chunk of peer TLS handshake data is received. For every
  // chunk, we move the TLS handshake further along until it is complete.
  int Receive(TLSContext::EncryptionLevel level,
              uint64_t offset,
              const uint8_t* data,
              size_t datalen);

  v8::MaybeLocal<v8::Object> cert(Environment* env) const;
  v8::MaybeLocal<v8::Object> peer_cert(Environment* env) const;
  v8::MaybeLocal<v8::Value> cipher_name(Environment* env) const;
  v8::MaybeLocal<v8::Value> cipher_version(Environment* env) const;
  v8::MaybeLocal<v8::Object> ephemeral_key(Environment* env) const;

  // The SNI servername negotiated for the session
  const std::string_view servername() const;

  // The ALPN (protocol name) negotiated for the session
  const std::string_view alpn() const;

  // Triggers key update to begin. This will fail and return false if either a
  // previous key update is in progress and has not been confirmed or if the
  // initial handshake has not yet been confirmed.
  bool InitiateKeyUpdate();

  int VerifyPeerIdentity();

  Side side() const;
  const Options& options() const;

  int OnNewSession(SSL_SESSION* session);

  void MaybeSetEarlySession(const SessionTicket& sessionTicket);
  bool early_data_was_accepted() const;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(CryptoContext)
  SET_SELF_SIZE(TLSContext)

 private:
  static ngtcp2_conn* getConnection(ngtcp2_crypto_conn_ref* ref);
  ngtcp2_crypto_conn_ref conn_ref_;

  Side side_;
  Environment* env_;
  Session* session_;
  const Options options_;
  BaseObjectPtr<crypto::SecureContext> secure_context_;
  crypto::SSLPointer ssl_;
  crypto::BIOPointer bio_trace_;

  bool in_key_update_ = false;
  bool early_data_ = false;

  friend class Session;
};

}  // namespace quic
}  // namespace node

#endif  // HAVE_OPENSSL && NODE_OPENSSL_HAS_QUIC
#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
