#include "node_quic_crypto.h"
#include "env-inl.h"
#include "node_crypto.h"
#include "node_crypto_common.h"
#include "node_process.h"
#include "node_quic_session-inl.h"
#include "node_quic_util-inl.h"
#include "node_sockaddr-inl.h"
#include "node_url.h"
#include "string_bytes.h"
#include "v8.h"

#include <ngtcp2/ngtcp2.h>
#include <ngtcp2/ngtcp2_crypto.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>

#include <iterator>
#include <numeric>
#include <unordered_map>
#include <string>
#include <sstream>
#include <vector>

namespace node {

using crypto::EntropySource;
using v8::Local;
using v8::String;
using v8::Value;

namespace quic {

namespace {
constexpr int kCryptoKeylen = 64;
constexpr int kCryptoIvlen = 64;
constexpr int kCryptoTokenSecretlen = 32;
constexpr int kCryptoTokenKeylen = 32;
constexpr int kCryptoTokenIvlen = 32;

constexpr char kQuicClientEarlyTrafficSecret[] =
    "QUIC_CLIENT_EARLY_TRAFFIC_SECRET";
constexpr char kQuicClientHandshakeTrafficSecret[] =
    "QUIC_CLIENT_HANDSHAKE_TRAFFIC_SECRET";
constexpr char kQuicClientTrafficSecret0[] =
    "QUIC_CLIENT_TRAFFIC_SECRET_0";
constexpr char kQuicServerHandshakeTrafficSecret[] =
    "QUIC_SERVER_HANDSHAKE_TRAFFIC_SECRET";
constexpr char kQuicServerTrafficSecret[] =
    "QUIC_SERVER_TRAFFIC_SECRET_0";

// Used solely to derive the keys used to generate retry tokens.
bool DeriveTokenKey(
    uint8_t* token_key,
    uint8_t* token_iv,
    const uint8_t* rand_data,
    size_t rand_datalen,
    const ngtcp2_crypto_ctx& ctx,
    const uint8_t* token_secret) {
  uint8_t secret[kCryptoTokenSecretlen];

  return
      NGTCP2_OK(ngtcp2_crypto_hkdf_extract(
          secret,
          &ctx.md,
          token_secret,
          kTokenSecretLen,
          rand_data,
          rand_datalen)) &&
      NGTCP2_OK(ngtcp2_crypto_derive_packet_protection_key(
          token_key,
          token_iv,
          nullptr,
          &ctx.aead,
          &ctx.md,
          secret,
          kCryptoTokenSecretlen));
}

void GenerateRandData(uint8_t* buf, size_t len) {
  uint8_t rand[16];
  uint8_t md[32];

  const EVP_MD* meth = EVP_sha256();
  unsigned int mdlen = EVP_MD_size(meth);
  DeleteFnPtr<EVP_MD_CTX, EVP_MD_CTX_free> ctx;
  ctx.reset(EVP_MD_CTX_new());
  CHECK(ctx);

  EntropySource(rand, arraysize(rand));
  CHECK_EQ(EVP_DigestInit_ex(ctx.get(), meth, nullptr), 1);
  CHECK_EQ(EVP_DigestUpdate(ctx.get(), rand, arraysize(rand)), 1);
  CHECK_EQ(EVP_DigestFinal_ex(ctx.get(), rand, &mdlen), 1);
  CHECK_LE(len, arraysize(md));
  std::copy_n(std::begin(md), len, buf);
}

bool GenerateRetryToken(
    uint8_t* token,
    size_t* tokenlen,
    const SocketAddress& addr,
    const QuicCID& ocid,
    const uint8_t* token_secret) {
  std::array<uint8_t, 4096> plaintext;
  uint8_t rand_data[kTokenRandLen];
  uint8_t token_key[kCryptoTokenKeylen];
  uint8_t token_iv[kCryptoTokenIvlen];

  ngtcp2_crypto_ctx ctx;
  ngtcp2_crypto_ctx_initial(&ctx);
  size_t ivlen = ngtcp2_crypto_packet_protection_ivlen(&ctx.aead);
  uint64_t now = uv_hrtime();

  auto p = std::begin(plaintext);
  p = std::copy_n(addr.raw(), addr.length(), p);
  p = std::copy_n(reinterpret_cast<uint8_t*>(&now), sizeof(uint64_t), p);
  p = std::copy_n(ocid->data, ocid->datalen, p);

  GenerateRandData(rand_data, kTokenRandLen);

  if (!DeriveTokenKey(
          token_key,
          token_iv,
          rand_data,
          kTokenRandLen,
          ctx,
          token_secret)) {
    return false;
  }

  size_t plaintextlen = std::distance(std::begin(plaintext), p);
  if (NGTCP2_ERR(ngtcp2_crypto_encrypt(
          token,
          &ctx.aead,
          plaintext.data(),
          plaintextlen,
          token_key,
          token_iv,
          ivlen,
          addr.raw(),
          addr.length()))) {
    return false;
  }

  *tokenlen = plaintextlen + ngtcp2_crypto_aead_taglen(&ctx.aead);
  memcpy(token + (*tokenlen), rand_data, kTokenRandLen);
  *tokenlen += kTokenRandLen;
  return true;
}
}  // namespace

bool GenerateResetToken(
    uint8_t* token,
    const uint8_t* secret,
    const QuicCID& cid) {
  ngtcp2_crypto_ctx ctx;
  ngtcp2_crypto_ctx_initial(&ctx);
  return NGTCP2_OK(ngtcp2_crypto_generate_stateless_reset_token(
      token,
      &ctx.md,
      secret,
      NGTCP2_STATELESS_RESET_TOKENLEN,
      cid.cid()));
}


std::unique_ptr<QuicPacket> GenerateRetryPacket(
    const uint8_t* token_secret,
    const QuicCID& dcid,
    const QuicCID& scid,
    const SocketAddress& local_addr,
    const SocketAddress& remote_addr) {

  uint8_t token[256];
  size_t tokenlen = sizeof(token);

  if (!GenerateRetryToken(token, &tokenlen, remote_addr, dcid, token_secret))
    return {};

  QuicCID cid;
  EntropySource(cid.data(), kScidLen);
  cid.set_length(kScidLen);

  size_t pktlen = tokenlen + (2 * NGTCP2_MAX_CIDLEN) + scid.length() + 8;
  CHECK_LE(pktlen, NGTCP2_MAX_PKT_SIZE);

  auto packet = QuicPacket::Create("retry", pktlen);
  ssize_t nwrite =
      ngtcp2_crypto_write_retry(
          packet->data(),
          NGTCP2_MAX_PKTLEN_IPV4,
          scid.cid(),
          cid.cid(),
          dcid.cid(),
          token,
          tokenlen);
  if (nwrite <= 0)
    return {};
  packet->set_length(nwrite);
  return std::move(packet);
}

// True if the received retry token is invalid.
bool InvalidRetryToken(
    const ngtcp2_pkt_hd& hd,
    const SocketAddress& addr,
    QuicCID* ocid,
    const uint8_t* token_secret,
    uint64_t verification_expiration) {

  if (hd.tokenlen < kTokenRandLen)
    return true;

  ngtcp2_crypto_ctx ctx;
  ngtcp2_crypto_ctx_initial(&ctx);

  size_t ivlen = ngtcp2_crypto_packet_protection_ivlen(&ctx.aead);

  size_t ciphertextlen = hd.tokenlen - kTokenRandLen;
  const uint8_t* ciphertext = hd.token;
  const uint8_t* rand_data = hd.token + ciphertextlen;

  uint8_t token_key[kCryptoTokenKeylen];
  uint8_t token_iv[kCryptoTokenIvlen];

  if (!DeriveTokenKey(
          token_key,
          token_iv,
          rand_data,
          kTokenRandLen,
          ctx,
          token_secret)) {
    return true;
  }

  uint8_t plaintext[4096];

  if (NGTCP2_ERR(ngtcp2_crypto_decrypt(
          plaintext,
          &ctx.aead,
          ciphertext,
          ciphertextlen,
          token_key,
          token_iv,
          ivlen,
          addr.raw(),
          addr.length()))) {
    return true;
  }

  size_t plaintextlen = ciphertextlen - ngtcp2_crypto_aead_taglen(&ctx.aead);
  if (plaintextlen < addr.length() + sizeof(uint64_t))
    return true;

  ssize_t cil = plaintextlen - addr.length() - sizeof(uint64_t);
  if ((cil != 0 && (cil < NGTCP2_MIN_CIDLEN || cil > NGTCP2_MAX_CIDLEN)) ||
      memcmp(plaintext, addr.raw(), addr.length()) != 0) {
    return true;
  }

  uint64_t t;
  memcpy(&t, plaintext + addr.length(), sizeof(uint64_t));

  // 10-second window by default, but configurable for each
  // QuicSocket instance with a MIN_RETRYTOKEN_EXPIRATION second
  // minimum and a MAX_RETRYTOKEN_EXPIRATION second maximum.
  if (t + verification_expiration * NGTCP2_SECONDS < uv_hrtime())
    return true;

  ngtcp2_cid_init(
      ocid->cid(),
      plaintext + addr.length() + sizeof(uint64_t),
      cil);

  return false;
}

namespace {

bool SplitHostname(
    const char* hostname,
    std::vector<std::string>* parts,
    const char delim = '.') {
  static std::string check_str =
      "\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2A\x2B\x2C\x2D\x2E\x2F\x30"
      "\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3A\x3B\x3C\x3D\x3E\x3F\x40"
      "\x41\x42\x43\x44\x45\x46\x47\x48\x49\x4A\x4B\x4C\x4D\x4E\x4F\x50"
      "\x51\x52\x53\x54\x55\x56\x57\x58\x59\x5A\x5B\x5C\x5D\x5E\x5F\x60"
      "\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6A\x6B\x6C\x6D\x6E\x6F\x70"
      "\x71\x72\x73\x74\x75\x76\x77\x78\x79\x7A\x7B\x7C\x7D\x7E\x7F";

  std::stringstream str(hostname);
  std::string part;
  while (getline(str, part, delim)) {
    // if (part.length() == 0 ||
    //     part.find_first_not_of(check_str) != std::string::npos) {
    //   return false;
    // }
    for (size_t n = 0; n < part.length(); n++) {
      if (part[n] >= 'A' && part[n] <= 'Z')
        part[n] = (part[n] | 0x20);  // Lower case the letter
      if (check_str.find(part[n]) == std::string::npos)
        return false;
    }
    parts->push_back(part);
  }
  return true;
}

bool CheckCertNames(
    const std::vector<std::string>& host_parts,
    const std::string& name,
    bool use_wildcard = true) {

  if (name.length() == 0)
    return false;

  std::vector<std::string> name_parts;
  if (!SplitHostname(name.c_str(), &name_parts))
    return false;

  if (name_parts.size() != host_parts.size())
    return false;

  for (size_t n = host_parts.size() - 1; n > 0; --n) {
    if (host_parts[n] != name_parts[n])
      return false;
  }

  if (name_parts[0].find("*") == std::string::npos ||
      name_parts[0].find("xn--") != std::string::npos) {
    return host_parts[0] == name_parts[0];
  }

  if (!use_wildcard)
    return false;

  std::vector<std::string> sub_parts;
  SplitHostname(name_parts[0].c_str(), &sub_parts, '*');

  if (sub_parts.size() > 2)
    return false;

  if (name_parts.size() <= 2)
    return false;

  std::string prefix;
  std::string suffix;
  if (sub_parts.size() == 2) {
    prefix = sub_parts[0];
    suffix = sub_parts[1];
  } else {
    prefix = "";
    suffix = sub_parts[0];
  }

  if (prefix.length() + suffix.length() > host_parts[0].length())
    return false;

  if (host_parts[0].compare(0, prefix.length(), prefix))
    return false;

  if (host_parts[0].compare(
          host_parts[0].length() - suffix.length(),
          suffix.length(), suffix)) {
    return false;
  }

  return true;
}

}  // namespace

int VerifyHostnameIdentity(SSL* ssl, const char* hostname) {
  int err = X509_V_ERR_HOSTNAME_MISMATCH;
  crypto::X509Pointer cert(SSL_get_peer_certificate(ssl));
  if (!cert)
    return err;

  // There are several pieces of information we need from the cert at this point
  // 1. The Subject (if it exists)
  // 2. The collection of Alt Names (if it exists)
  //
  // The certificate may have many Alt Names. We only care about the ones that
  // are prefixed with 'DNS:', 'URI:', or 'IP Address:'. We might check
  // additional ones later but we'll start with these.
  //
  // Ideally, we'd be able to *just* use OpenSSL's built in name checking for
  // this (SSL_set1_host and X509_check_host) but it does not appear to do
  // checking on URI or IP Address Alt names, which is unfortunate. We need
  // both of those to retain compatibility with the peer identity verification
  // Node.js already does elsewhere. At the very least, we'll use
  // X509_check_host here first as a first step. If it is successful, awesome,
  // there's nothing else for us to do. Return and be happy!
  if (X509_check_host(
          cert.get(),
          hostname,
          strlen(hostname),
          X509_CHECK_FLAG_ALWAYS_CHECK_SUBJECT |
          X509_CHECK_FLAG_MULTI_LABEL_WILDCARDS |
          X509_CHECK_FLAG_SINGLE_LABEL_SUBDOMAINS,
          nullptr) > 0) {
    return 0;
  }

  if (X509_check_ip_asc(
          cert.get(),
          hostname,
          X509_CHECK_FLAG_ALWAYS_CHECK_SUBJECT) > 0) {
    return 0;
  }

  // If we've made it this far, then we have to perform a more check
  return VerifyHostnameIdentity(
      hostname,
      crypto::GetCertificateCN(cert.get()),
      crypto::GetCertificateAltNames(cert.get()));
}

int VerifyHostnameIdentity(
    const char* hostname,
    const std::string& cert_cn,
    const std::unordered_multimap<std::string, std::string>& altnames) {

  int err = X509_V_ERR_HOSTNAME_MISMATCH;

  // 1. If the hostname is an IP address (v4 or v6), the certificate is valid
  //    if and only if there is an 'IP Address:' alt name specifying the same
  //    IP address. The IP address must be canonicalized to ensure a proper
  //    check. It's possible that the X509_check_ip_asc covers this. If so,
  //    we can remove this check.

  if (SocketAddress::is_numeric_host(hostname)) {
    auto ips = altnames.equal_range("ip");
    for (auto ip = ips.first; ip != ips.second; ++ip) {
      if (ip->second.compare(hostname) == 0) {
        // Success!
        return 0;
      }
    }
    // No match, and since the hostname is an IP address, skip any
    // further checks
    return err;
  }

  auto dns_names = altnames.equal_range("dns");
  auto uri_names = altnames.equal_range("uri");

  size_t dns_count = std::distance(dns_names.first, dns_names.second);
  size_t uri_count = std::distance(uri_names.first, uri_names.second);

  std::vector<std::string> host_parts;
  SplitHostname(hostname, &host_parts);

  // 2. If there no 'DNS:' or 'URI:' Alt names, if the certificate has a
  //    Subject, then we need to extract the CN field from the Subject. and
  //    check that the hostname matches the CN, taking into consideration
  //    the possibility of a wildcard in the CN. If there is a match, congrats,
  //    we have a valid certificate. Return and be happy.

  if (dns_count == 0 && uri_count == 0) {
    if (cert_cn.length() > 0 && CheckCertNames(host_parts, cert_cn))
        return 0;
    // No match, and since there are no dns or uri entries, return
    return err;
  }

  // 3. If, however, there are 'DNS:' and 'URI:' Alt names, things become more
  //    complicated. Essentially, we need to iterate through each 'DNS:' and
  //    'URI:' Alt name to find one that matches. The 'DNS:' Alt names are
  //    relatively simple but may include wildcards. The 'URI:' Alt names
  //    require the name to be parsed as a URL, then extract the hostname from
  //    the URL, which is then checked against the hostname. If you find a
  //    match, yay! Return and be happy. (Note, it's possible that the 'DNS:'
  //    check in this step is redundant to the X509_check_host check. If so,
  //    we can simplify by removing those checks here.)

  // First, let's check dns names
  for (auto name = dns_names.first; name != dns_names.second; ++name) {
    if (name->first.length() > 0 &&
        CheckCertNames(host_parts, name->second)) {
      return 0;
    }
  }

  // Then, check uri names
  for (auto name = uri_names.first; name != uri_names.second; ++name) {
    if (name->first.length() > 0 &&
        CheckCertNames(host_parts, name->second, false)) {
      return 0;
    }
  }

  // 4. Failing all of the previous checks, we assume the certificate is
  //    invalid for an unspecified reason.
  return err;
}

// Get the ALPN protocol identifier that was negotiated for the session
Local<Value> GetALPNProtocol(QuicSession* session) {
  Local<Value> alpn;
  const unsigned char* alpn_buf = nullptr;
  unsigned int alpnlen;
  QuicCryptoContext* ctx = session->crypto_context();

  SSL_get0_alpn_selected(ctx->ssl(), &alpn_buf, &alpnlen);
  if (alpnlen == sizeof(NGTCP2_ALPN_H3) - 2 &&
      memcmp(alpn_buf, NGTCP2_ALPN_H3 + 1, sizeof(NGTCP2_ALPN_H3) - 2) == 0) {
    alpn = session->env()->quic_alpn_string();
  } else {
    alpn = OneByteString(session->env()->isolate(), alpn_buf, alpnlen);
  }
  return alpn;
}

namespace {
int CertCB(SSL* ssl, void* arg) {
  QuicSession* session = static_cast<QuicSession*>(arg);

  int type = SSL_get_tlsext_status_type(ssl);
  switch (type) {
    case TLSEXT_STATUSTYPE_ocsp:
      return session->crypto_context()->OnOCSP();
    default:
      return 1;
  }
}

void Keylog_CB(const SSL* ssl, const char* line) {
  QuicSession* session = static_cast<QuicSession*>(SSL_get_app_data(ssl));
  session->crypto_context()->Keylog(line);
}

int Client_Hello_CB(
    SSL* ssl,
    int* tls_alert,
    void* arg) {
  QuicSession* session = static_cast<QuicSession*>(SSL_get_app_data(ssl));
  int ret = session->crypto_context()->OnClientHello();
  switch (ret) {
    case 0:
      return 1;
    case -1:
      return -1;
    default:
      *tls_alert = ret;
      return 0;
  }
}

int AlpnSelection(
    SSL* ssl,
    const unsigned char** out,
    unsigned char* outlen,
    const unsigned char* in,
    unsigned int inlen,
    void* arg) {
  QuicSession* session = static_cast<QuicSession*>(SSL_get_app_data(ssl));

  unsigned char* tmp;

  // The QuicServerSession supports exactly one ALPN identifier. If that does
  // not match any of the ALPN identifiers provided in the client request,
  // then we fail here. Note that this will not fail the TLS handshake, so
  // we have to check later if the ALPN matches the expected identifier or not.
  if (SSL_select_next_proto(
          &tmp,
          outlen,
          reinterpret_cast<const unsigned char*>(session->alpn().c_str()),
          session->alpn().length(),
          in,
          inlen) == OPENSSL_NPN_NO_OVERLAP) {
    return SSL_TLSEXT_ERR_NOACK;
  }
  *out = tmp;
  return SSL_TLSEXT_ERR_OK;
}

int TLS_Status_Callback(SSL* ssl, void* arg) {
  QuicSession* session = static_cast<QuicSession*>(SSL_get_app_data(ssl));
  return session->crypto_context()->OnTLSStatus();
}

int New_Session_Callback(SSL* ssl, SSL_SESSION* session) {
  QuicSession* s = static_cast<QuicSession*>(SSL_get_app_data(ssl));
  return s->set_session(session);
}

int SetEncryptionSecrets(
    SSL* ssl,
    OSSL_ENCRYPTION_LEVEL ossl_level,
    const uint8_t* read_secret,
    const uint8_t* write_secret,
    size_t secret_len) {
  QuicSession* session = static_cast<QuicSession*>(SSL_get_app_data(ssl));
  return session->crypto_context()->OnSecrets(
      from_ossl_level(ossl_level),
      read_secret,
      write_secret,
      secret_len) ? 1 : 0;
}

int AddHandshakeData(
    SSL* ssl,
    OSSL_ENCRYPTION_LEVEL ossl_level,
    const uint8_t* data,
    size_t len) {
  QuicSession* session = static_cast<QuicSession*>(SSL_get_app_data(ssl));
  session->crypto_context()->WriteHandshake(
      from_ossl_level(ossl_level),
      data,
      len);
  return 1;
}

int FlushFlight(SSL* ssl) { return 1; }

int SendAlert(
    SSL* ssl,
    enum ssl_encryption_level_t level,
    uint8_t alert) {
  QuicSession* session = static_cast<QuicSession*>(SSL_get_app_data(ssl));
  session->crypto_context()->set_tls_alert(alert);
  return 1;
}

bool SetTransportParams(ngtcp2_conn* connection, SSL* ssl) {
  ngtcp2_transport_params params;
  ngtcp2_conn_get_local_transport_params(connection, &params);
  uint8_t buf[512];
  ssize_t nwrite = ngtcp2_encode_transport_params(
      buf,
      arraysize(buf),
      NGTCP2_TRANSPORT_PARAMS_TYPE_ENCRYPTED_EXTENSIONS,
      &params);
  return nwrite >= 0 && SSL_set_quic_transport_params(ssl, buf, nwrite) == 1;
}

SSL_QUIC_METHOD quic_method = SSL_QUIC_METHOD{
  SetEncryptionSecrets,
  AddHandshakeData,
  FlushFlight,
  SendAlert
};

void SetHostname(SSL* ssl, const std::string& hostname) {
  // If the hostname is an IP address, use an empty string
  // as the hostname instead.
  if (SocketAddress::is_numeric_host(hostname.c_str())) {
    SSL_set_tlsext_host_name(ssl, "");
  } else {
    SSL_set_tlsext_host_name(ssl, hostname.c_str());
  }
}

}  // namespace

void InitializeTLS(QuicSession* session) {
  QuicCryptoContext* ctx = session->crypto_context();
  Environment* env = session->env();

  SSL* ssl = ctx->ssl();
  SSL_set_app_data(ssl, session);
  SSL_set_cert_cb(ssl, CertCB, session);
  SSL_set_verify(ssl, SSL_VERIFY_NONE, crypto::VerifyCallback);

  // Enable tracing if the `--trace-tls` command line flag is used.
  if (env->options()->trace_tls) {
    ctx->EnableTrace();
    if (env->quic_state()->warn_trace_tls) {
      env->quic_state()->warn_trace_tls = false;
      ProcessEmitWarning(env,
          "Enabling --trace-tls can expose sensitive data "
          "in the resulting log");
    }
  }

  switch (ctx->side()) {
    case NGTCP2_CRYPTO_SIDE_CLIENT: {
      SSL_set_connect_state(ssl);
      crypto::SetALPN(ssl, session->alpn());
      SetHostname(ssl, session->hostname());
      if (ctx->is_option_set(QUICCLIENTSESSION_OPTION_REQUEST_OCSP))
        SSL_set_tlsext_status_type(ssl, TLSEXT_STATUSTYPE_ocsp);
      break;
    }
    case NGTCP2_CRYPTO_SIDE_SERVER: {
      SSL_set_accept_state(ssl);
      if (ctx->is_option_set(QUICSERVERSESSION_OPTION_REQUEST_CERT)) {
        int verify_mode = SSL_VERIFY_PEER;
        if (ctx->is_option_set(QUICSERVERSESSION_OPTION_REJECT_UNAUTHORIZED))
          verify_mode |= SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
        SSL_set_verify(ssl, verify_mode, crypto::VerifyCallback);
      }
      break;
    }
    default:
      UNREACHABLE();
  }

  SetTransportParams(session->connection(), ssl);
}

void InitializeSecureContext(
    crypto::SecureContext* sc,
    ngtcp2_crypto_side side) {
  constexpr auto ssl_server_opts =
      (SSL_OP_ALL & ~SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS) |
      SSL_OP_SINGLE_ECDH_USE |
      SSL_OP_CIPHER_SERVER_PREFERENCE |
      SSL_OP_NO_ANTI_REPLAY;
  switch (side) {
    case NGTCP2_CRYPTO_SIDE_SERVER:
      SSL_CTX_set_options(**sc, ssl_server_opts);
      SSL_CTX_set_mode(**sc, SSL_MODE_RELEASE_BUFFERS);
      SSL_CTX_set_max_early_data(**sc, std::numeric_limits<uint32_t>::max());
      SSL_CTX_set_alpn_select_cb(**sc, AlpnSelection, nullptr);
      SSL_CTX_set_client_hello_cb(**sc, Client_Hello_CB, nullptr);
      break;
    case NGTCP2_CRYPTO_SIDE_CLIENT:
      SSL_CTX_set_session_cache_mode(
          **sc,
          SSL_SESS_CACHE_CLIENT |
          SSL_SESS_CACHE_NO_INTERNAL_STORE);
      SSL_CTX_sess_set_new_cb(**sc, New_Session_Callback);
      break;
    default:
      UNREACHABLE();
  }
  SSL_CTX_set_min_proto_version(**sc, TLS1_3_VERSION);
  SSL_CTX_set_max_proto_version(**sc, TLS1_3_VERSION);
  SSL_CTX_set_default_verify_paths(**sc);
  SSL_CTX_set_tlsext_status_cb(**sc, TLS_Status_Callback);
  SSL_CTX_set_keylog_callback(**sc, Keylog_CB);
  SSL_CTX_set_tlsext_status_arg(**sc, nullptr);
  SSL_CTX_set_quic_method(**sc, &quic_method);
}

bool SetCryptoSecrets(
    QuicSession* session,
    ngtcp2_crypto_level level,
    const uint8_t* rx_secret,
    const uint8_t* tx_secret,
    size_t secretlen) {

  uint8_t rx_key[kCryptoKeylen];
  uint8_t rx_hp[kCryptoKeylen];
  uint8_t tx_key[kCryptoKeylen];
  uint8_t tx_hp[kCryptoKeylen];
  uint8_t rx_iv[kCryptoIvlen];
  uint8_t tx_iv[kCryptoIvlen];

  QuicCryptoContext* ctx = session->crypto_context();
  SSL* ssl = ctx->ssl();

  if (NGTCP2_ERR(ngtcp2_crypto_derive_and_install_key(
          session->connection(),
          ssl,
          rx_key,
          rx_iv,
          rx_hp,
          tx_key,
          tx_iv,
          tx_hp,
          level,
          rx_secret,
          tx_secret,
          secretlen,
          ctx->side()))) {
    return false;
  }

  switch (level) {
  case NGTCP2_CRYPTO_LEVEL_EARLY:
    crypto::LogSecret(
        ssl,
        kQuicClientEarlyTrafficSecret,
        rx_secret,
        secretlen);
    break;
  case NGTCP2_CRYPTO_LEVEL_HANDSHAKE:
    crypto::LogSecret(
        ssl,
        kQuicClientHandshakeTrafficSecret,
        rx_secret,
        secretlen);
    crypto::LogSecret(
        ssl,
        kQuicServerHandshakeTrafficSecret,
        tx_secret,
        secretlen);
    break;
  case NGTCP2_CRYPTO_LEVEL_APP:
    crypto::LogSecret(
        ssl,
        kQuicClientTrafficSecret0,
        rx_secret,
        secretlen);
    crypto::LogSecret(
        ssl,
        kQuicServerTrafficSecret,
        tx_secret,
        secretlen);
    break;
  default:
    UNREACHABLE();
  }

  return true;
}

bool DeriveAndInstallInitialKey(
    QuicSession* session,
    const QuicCID& dcid) {
  uint8_t initial_secret[NGTCP2_CRYPTO_INITIAL_SECRETLEN];
  uint8_t rx_secret[NGTCP2_CRYPTO_INITIAL_SECRETLEN];
  uint8_t tx_secret[NGTCP2_CRYPTO_INITIAL_SECRETLEN];
  uint8_t rx_key[NGTCP2_CRYPTO_INITIAL_KEYLEN];
  uint8_t tx_key[NGTCP2_CRYPTO_INITIAL_KEYLEN];
  uint8_t rx_hp[NGTCP2_CRYPTO_INITIAL_KEYLEN];
  uint8_t tx_hp[NGTCP2_CRYPTO_INITIAL_KEYLEN];
  uint8_t rx_iv[NGTCP2_CRYPTO_INITIAL_IVLEN];
  uint8_t tx_iv[NGTCP2_CRYPTO_INITIAL_IVLEN];
  return NGTCP2_OK(ngtcp2_crypto_derive_and_install_initial_key(
      session->connection(),
      rx_secret,
      tx_secret,
      initial_secret,
      rx_key,
      rx_iv,
      rx_hp,
      tx_key,
      tx_iv,
      tx_hp,
      dcid.cid(),
      session->crypto_context()->side()));
}

ngtcp2_crypto_level from_ossl_level(OSSL_ENCRYPTION_LEVEL ossl_level) {
  switch (ossl_level) {
  case ssl_encryption_initial:
    return NGTCP2_CRYPTO_LEVEL_INITIAL;
  case ssl_encryption_early_data:
    return NGTCP2_CRYPTO_LEVEL_EARLY;
  case ssl_encryption_handshake:
    return NGTCP2_CRYPTO_LEVEL_HANDSHAKE;
  case ssl_encryption_application:
    return NGTCP2_CRYPTO_LEVEL_APP;
  default:
    UNREACHABLE();
  }
}

const char* crypto_level_name(ngtcp2_crypto_level level) {
  switch (level) {
    case NGTCP2_CRYPTO_LEVEL_INITIAL:
      return "initial";
    case NGTCP2_CRYPTO_LEVEL_EARLY:
      return "early";
    case NGTCP2_CRYPTO_LEVEL_HANDSHAKE:
      return "handshake";
    case NGTCP2_CRYPTO_LEVEL_APP:
      return "app";
    default:
      UNREACHABLE();
  }
}

// When using IPv6, QUIC recommends the use of IPv6 Flow Labels
// as specified in https://tools.ietf.org/html/rfc6437. These
// are used as a means of reliably associating packets exchanged
// as part of a single flow and protecting against certain kinds
// of attacks.
uint32_t GenerateFlowLabel(
    const SocketAddress& local,
    const SocketAddress& remote,
    const QuicCID& cid,
    const uint8_t* secret,
    size_t secretlen) {
  static constexpr size_t kInfoLen =
      (sizeof(sockaddr_in6) * 2) + NGTCP2_MAX_CIDLEN;

  uint32_t label = 0;

  std::array<uint8_t, kInfoLen> plaintext;
  size_t infolen = local.length() + remote.length() + cid.length();
  CHECK_LE(infolen, kInfoLen);

  ngtcp2_crypto_ctx ctx;
  ngtcp2_crypto_ctx_initial(&ctx);

  auto p = std::begin(plaintext);
  p = std::copy_n(local.raw(), local.length(), p);
  p = std::copy_n(remote.raw(), remote.length(), p);
  p = std::copy_n(cid->data, cid->datalen, p);

  ngtcp2_crypto_hkdf_expand(
      reinterpret_cast<uint8_t*>(&label),
      sizeof(label),
      &ctx.md,
      secret,
      secretlen,
      plaintext.data(),
      infolen);

  label &= kLabelMask;
  DCHECK_LE(label, kLabelMask);
  return label;
}

}  // namespace quic
}  // namespace node
