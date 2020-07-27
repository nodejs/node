#ifndef SRC_QUIC_NODE_QUIC_CRYPTO_H_
#define SRC_QUIC_NODE_QUIC_CRYPTO_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node_crypto.h"
#include "node_quic_util.h"
#include "v8.h"

#include <ngtcp2/ngtcp2.h>
#include <ngtcp2/ngtcp2_crypto.h>
#include <openssl/ssl.h>

namespace node {

namespace quic {

// Crypto and OpenSSL related utility functions used in
// various places throughout the QUIC implementation.

// Forward declaration
class QuicSession;
class QuicPacket;

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
    BaseObjectPtr<crypto::SecureContext> sc,
    bool early_data,
    ngtcp2_crypto_side side);

// Called in the QuicSession::InitServer and
// QuicSession::InitClient to configure the
// appropriate settings for the SSL* associated
// with the session.
void InitializeTLS(QuicSession* session, const crypto::SSLPointer& ssl);

// Generates a stateless reset token using HKDF with the
// cid and token secret as input. The token secret is
// either provided by user code when a QuicSocket is
// created or is generated randomly.
//
// QUIC leaves the generation of stateless session tokens
// up to the implementation to figure out. The idea, however,
// is that it ought to be possible to generate a stateless
// reset token reliably even when all state for a connection
// has been lost. We use the cid as it's the only reliably
// consistent bit of data we have when a session is destroyed.
bool GenerateResetToken(
    uint8_t* token,
    const uint8_t* secret,
    const QuicCID& cid);

// The Retry Token is an encrypted token that is sent to the client
// by the server as part of the path validation flow. The plaintext
// format within the token is opaque and only meaningful the server.
// We can structure it any way we want. It needs to:
//   * be hard to guess
//   * be time limited
//   * be specific to the client address
//   * be specific to the original cid
//   * contain random data.
std::unique_ptr<QuicPacket> GenerateRetryPacket(
    const uint8_t* token_secret,
    const QuicCID& dcid,
    const QuicCID& scid,
    const SocketAddress& local_addr,
    const SocketAddress& remote_addr);

// The IPv6 Flow Label is generated and set whenever IPv6 is used.
// The label is derived as a cryptographic function of the CID,
// local and remote addresses, and the given secret, that is then
// truncated to a 20-bit value (per IPv6 requirements). In QUIC,
// the flow label *may* be used as a way of disambiguating IP
// packets that belong to the same flow from a remote peer.
uint32_t GenerateFlowLabel(
    const SocketAddress& local,
    const SocketAddress& remote,
    const QuicCID& cid,
    const uint8_t* secret,
    size_t secretlen);

// Verifies the validity of a retry token. Returns true if the
// token is *not valid*, false otherwise. If the token is valid,
// the ocid will be updated to the original CID value encoded
// within the successfully validated, encrypted token.
bool InvalidRetryToken(
    const ngtcp2_vec& token,
    const SocketAddress& addr,
    QuicCID* ocid,
    const uint8_t* token_secret,
    uint64_t verification_expiration);

// Get the ALPN protocol identifier that was negotiated for the session
v8::Local<v8::Value> GetALPNProtocol(const QuicSession& session);

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

}  // namespace quic
}  // namespace node

#endif  // NODE_WANT_INTERNALS
#endif  // SRC_QUIC_NODE_QUIC_CRYPTO_H_
