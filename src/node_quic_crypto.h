#ifndef SRC_NODE_QUIC_CRYPTO_H_
#define SRC_NODE_QUIC_CRYPTO_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node_crypto.h"
#include "node_quic_util.h"
#include "v8.h"

#include <ngtcp2/ngtcp2.h>
#include <ngtcp2/ngtcp2_crypto.h>
#include <openssl/ssl.h>

namespace node {

namespace quic {

// Forward declaration
class QuicSession;

#define NGTCP2_ERR(V) (V != 0)
#define NGTCP2_OK(V) (V == 0)

// Called by QuicSession::OnSecrets when openssl
// delivers crypto secrets to the QuicSession.
// This will derive and install the keys and iv
// for TX and RX at the specified level, and
// generates the QUIC specific keylog events.
bool SetCryptoSecrets(
    QuicSession* session,
    ngtcp2_crypto_level level,
    const uint8_t* rx_secret,
    const uint8_t* tx_secret,
    size_t secretlen);

// Called by QuicInitSecureContext to initialize the
// given SecureContext with the defaults for the given
// QUIC side (client or server).
void InitializeSecureContext(
    crypto::SecureContext* sc,
    ngtcp2_crypto_side side);

// Called in the QuicSession::InitServer and
// QuicSession::InitClient to configure the
// appropriate settings for the SSL* associated
// with the session.
void InitializeTLS(QuicSession* session);

// Called when the client QuicSession is created and
// when the server QuicSession first receives the
// client hello.
bool DeriveAndInstallInitialKey(
    QuicSession* session,
    const ngtcp2_cid* dcid);

bool GenerateResetToken(
    uint8_t* token,
    const ResetTokenSecret& secret,
    const ngtcp2_cid* cid);

bool GenerateRetryToken(
    uint8_t* token,
    size_t* tokenlen,
    const sockaddr* addr,
    const ngtcp2_cid* ocid,
    const RetryTokenSecret& token_secret);

bool InvalidRetryToken(
    Environment* env,
    ngtcp2_cid* ocid,
    const ngtcp2_pkt_hd* hd,
    const sockaddr* addr,
    const RetryTokenSecret& token_secret,
    uint64_t verification_expiration);

int VerifyHostnameIdentity(SSL* ssl, const char* hostname);
int VerifyHostnameIdentity(
    const char* hostname,
    const std::string& cert_cn,
    const std::unordered_multimap<std::string, std::string>& altnames);

// Get the ALPN protocol identifier that was negotiated for the session
v8::Local<v8::Value> GetALPNProtocol(QuicSession* session);

}  // namespace quic
}  // namespace node

#endif  // NODE_WANT_INTERNALS
#endif  // SRC_NODE_QUIC_CRYPTO_H_
