#ifndef SRC_QUIC_CRYPTO_H_
#define SRC_QUIC_CRYPTO_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "quic/quic.h"
#include "crypto/crypto_context.h"
#include "crypto/crypto_util.h"
#include "node_sockaddr.h"
#include "v8.h"

#include <ngtcp2/ngtcp2.h>
#include <ngtcp2/ngtcp2_crypto.h>
#include <openssl/ssl.h>

// Check required capabilities were not excluded from the OpenSSL build:
// - OPENSSL_NO_SSL_TRACE excludes SSL_trace()
// - OPENSSL_NO_STDIO excludes BIO_new_fp()
// HAVE_SSL_TRACE is available on the internal tcp_wrap binding for the tests.
#if defined(OPENSSL_NO_SSL_TRACE) || defined(OPENSSL_NO_STDIO)
# define HAVE_SSL_TRACE 0
#else
# define HAVE_SSL_TRACE 1
#endif

namespace node {

namespace quic {

constexpr uint8_t kRetryTokenMagic = 0xb6;
constexpr uint8_t kTokenMagic = 0x36;
constexpr int kCryptoTokenKeylen = 16;
constexpr int kCryptoTokenIvlen = 12;
constexpr size_t kTokenRandLen = 16;
// 1 accounts for the magic byte, 16 accounts for aead tag
constexpr size_t kMaxRetryTokenLen =
    1 + sizeof(uint64_t) + NGTCP2_MAX_CIDLEN + 16 + kTokenRandLen;
constexpr size_t kMinRetryTokenLen = 1 + kTokenRandLen;

// 1 accounts for the magic byte, 16 accounts for aead tag
constexpr size_t kMaxTokenLen = 1 + sizeof(uint64_t) + 16 + kTokenRandLen;

// Forward declaration
class Session;

// many ngtcp2 functions return 0 to indicate success
// and non-zero to indicate failure. Most of the time,
// for such functions we don't care about the specific
// return value so we simplify using a macro.

#define NGTCP2_ERR(V) (V != 0)
#define NGTCP2_OK(V) (V == 0)

// Called by QuicInitSecureContext to initialize the
// given SecureContext with the defaults for the given
// QUIC side (client or server).
void InitializeSecureContext(
    crypto::SecureContext* sc,
    ngtcp2_crypto_side side);

void InitializeTLS(Session* session, const crypto::SSLPointer& ssl);

// Generates a stateless reset token using HKDF with the
// cid and token secret as input. The token secret is
// either provided by user code when an Endpoint is
// created or is generated randomly.
//
// QUIC leaves the generation of stateless session tokens
// up to the implementation to figure out. The idea, however,
// is that it ought to be possible to generate a stateless
// reset token reliably even when all state for a connection
// has been lost. We use the cid as it's the only reliably
// consistent bit of data we have when a session is destroyed.
bool GenerateResetToken(uint8_t* token, const uint8_t* secret, const CID& cid);

// The Retry Token is an encrypted token that is sent to the client
// by the server as part of the path validation flow. The plaintext
// format within the token is opaque and only meaningful the server.
// We can structure it any way we want. It needs to:
//   * be hard to guess
//   * be time limited
//   * be specific to the client address
//   * be specific to the original cid
//   * contain random data.
std::unique_ptr<Packet> GenerateRetryPacket(
    quic_version version,
    const uint8_t* token_secret,
    const CID& dcid,
    const CID& scid,
    const std::shared_ptr<SocketAddress>& local_addr,
    const std::shared_ptr<SocketAddress>& remote_addr,
    const ngtcp2_crypto_aead& aead,
    const ngtcp2_crypto_md& md);

// The IPv6 Flow Label is generated and set whenever IPv6 is used.
// The label is derived as a cryptographic function of the CID,
// local and remote addresses, and the given secret, that is then
// truncated to a 20-bit value (per IPv6 requirements). In QUIC,
// the flow label *may* be used as a way of disambiguating IP
// packets that belong to the same flow from a remote peer.
uint32_t GenerateFlowLabel(
    const std::shared_ptr<SocketAddress>& local,
    const std::shared_ptr<SocketAddress>& remote,
    const CID& cid,
    const uint8_t* secret,
    size_t secretlen);

enum class GetCertificateType {
  SELF,
  ISSUER,
};

v8::MaybeLocal<v8::Value> GetCertificateData(
    Environment* env,
    crypto::SecureContext* sc,
    GetCertificateType type = GetCertificateType::SELF);

// Validates a retry token. Returns Nothing<CID>() if the
// token is *not valid*, returns the OCID otherwise.
v8::Maybe<CID> ValidateRetryToken(
    const ngtcp2_vec& token,
    const std::shared_ptr<SocketAddress>& addr,
    const CID& dcid,
    const uint8_t* token_secret,
    uint64_t verification_expiration,
    const ngtcp2_crypto_aead& aead,
    const ngtcp2_crypto_md& md);

bool GenerateToken(
    uint8_t* token,
    size_t* tokenlen,
    const std::shared_ptr<SocketAddress>& addr,
    const uint8_t* token_secret,
    const ngtcp2_crypto_aead& aead,
    const ngtcp2_crypto_md& md);

bool ValidateToken(
    const ngtcp2_vec& token,
    const std::shared_ptr<SocketAddress>& addr,
    const uint8_t* token_secret,
    uint64_t verification_expiration,
    const ngtcp2_crypto_aead& aead,
    const ngtcp2_crypto_md& md);

// Get the ALPN protocol identifier that was negotiated for the session
v8::Local<v8::Value> GetALPNProtocol(const Session& session);

ngtcp2_crypto_level from_ossl_level(OSSL_ENCRYPTION_LEVEL ossl_level);
const char* crypto_level_name(ngtcp2_crypto_level level);

// SessionTicketAppData is a utility class that is used only during
// the generation or access of TLS stateless sesson tickets. It
// exists solely to provide a easier way for QuicApplication instances
// to set relevant metadata in the session ticket when it is created,
// and the exract and subsequently verify that data when a ticket is
// received and is being validated. The app data is completely opaque
// to anything other than the server-side of the QuicApplication that
// sets it.
class SessionTicketAppData {
 public:
  enum class Status {
    TICKET_USE,
    TICKET_USE_RENEW,
    TICKET_IGNORE,
    TICKET_IGNORE_RENEW
  };

  enum class Flag {
    STATUS_NONE,
    STATUS_RENEW
  };

  explicit SessionTicketAppData(SSL_SESSION* session) : session_(session) {}
  bool Set(const uint8_t* data, size_t len);
  bool Get(uint8_t** data, size_t* len) const;

 private:
  bool set_ = false;
  SSL_SESSION* session_;
};

ngtcp2_crypto_aead CryptoAeadAes128GCM();

ngtcp2_crypto_md CryptoMDSha256();

class AeadContextPointer {
 public:
  enum class Mode {
    ENCRYPT,
    DECRYPT
  };

  inline AeadContextPointer(
      Mode mode,
      const uint8_t* key,
      const ngtcp2_crypto_aead& aead) {
    switch (mode) {
      case Mode::ENCRYPT:
        inited_ = NGTCP2_OK(ngtcp2_crypto_aead_ctx_encrypt_init(
            &ctx_,
            &aead,
            key,
            kCryptoTokenIvlen));
        break;
      case Mode::DECRYPT:
        inited_ = NGTCP2_OK(ngtcp2_crypto_aead_ctx_decrypt_init(
            &ctx_,
            &aead,
            key,
            kCryptoTokenIvlen));
        break;
      default:
        UNREACHABLE();
    }
  }

  inline ~AeadContextPointer() {
    ngtcp2_crypto_aead_ctx_free(&ctx_);
  }

  inline ngtcp2_crypto_aead_ctx* get() { return &ctx_; }

  inline operator bool() const noexcept { return inited_; }

 private:
  ngtcp2_crypto_aead_ctx ctx_;
  bool inited_ = false;
};

}  // namespace quic
}  // namespace node

#endif  // NODE_WANT_INTERNALS
#endif  // SRC_QUIC_CRYPTO_H_
