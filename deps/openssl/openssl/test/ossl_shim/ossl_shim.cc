/*
 * Copyright 1995-2018 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#if !defined(__STDC_FORMAT_MACROS)
#define __STDC_FORMAT_MACROS
#endif

#include "packeted_bio.h"
#include <openssl/e_os2.h>

#if !defined(OPENSSL_SYS_WINDOWS)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#else
#include <io.h>
OPENSSL_MSVC_PRAGMA(warning(push, 3))
#include <winsock2.h>
#include <ws2tcpip.h>
OPENSSL_MSVC_PRAGMA(warning(pop))

OPENSSL_MSVC_PRAGMA(comment(lib, "Ws2_32.lib"))
#endif

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/bn.h>
#include <openssl/crypto.h>
#include <openssl/dh.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/objects.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include <memory>
#include <string>
#include <vector>

#include "async_bio.h"
#include "test_config.h"

namespace bssl {

#if !defined(OPENSSL_SYS_WINDOWS)
static int closesocket(int sock) {
  return close(sock);
}

static void PrintSocketError(const char *func) {
  perror(func);
}
#else
static void PrintSocketError(const char *func) {
  fprintf(stderr, "%s: %d\n", func, WSAGetLastError());
}
#endif

static int Usage(const char *program) {
  fprintf(stderr, "Usage: %s [flags...]\n", program);
  return 1;
}

struct TestState {
  // async_bio is async BIO which pauses reads and writes.
  BIO *async_bio = nullptr;
  // packeted_bio is the packeted BIO which simulates read timeouts.
  BIO *packeted_bio = nullptr;
  bool cert_ready = false;
  bool handshake_done = false;
  // private_key is the underlying private key used when testing custom keys.
  bssl::UniquePtr<EVP_PKEY> private_key;
  bool got_new_session = false;
  bssl::UniquePtr<SSL_SESSION> new_session;
  bool ticket_decrypt_done = false;
  bool alpn_select_done = false;
};

static void TestStateExFree(void *parent, void *ptr, CRYPTO_EX_DATA *ad,
                            int index, long argl, void *argp) {
  delete ((TestState *)ptr);
}

static int g_config_index = 0;
static int g_state_index = 0;

static bool SetTestConfig(SSL *ssl, const TestConfig *config) {
  return SSL_set_ex_data(ssl, g_config_index, (void *)config) == 1;
}

static const TestConfig *GetTestConfig(const SSL *ssl) {
  return (const TestConfig *)SSL_get_ex_data(ssl, g_config_index);
}

static bool SetTestState(SSL *ssl, std::unique_ptr<TestState> state) {
  // |SSL_set_ex_data| takes ownership of |state| only on success.
  if (SSL_set_ex_data(ssl, g_state_index, state.get()) == 1) {
    state.release();
    return true;
  }
  return false;
}

static TestState *GetTestState(const SSL *ssl) {
  return (TestState *)SSL_get_ex_data(ssl, g_state_index);
}

static bssl::UniquePtr<X509> LoadCertificate(const std::string &file) {
  bssl::UniquePtr<BIO> bio(BIO_new(BIO_s_file()));
  if (!bio || !BIO_read_filename(bio.get(), file.c_str())) {
    return nullptr;
  }
  return bssl::UniquePtr<X509>(PEM_read_bio_X509(bio.get(), NULL, NULL, NULL));
}

static bssl::UniquePtr<EVP_PKEY> LoadPrivateKey(const std::string &file) {
  bssl::UniquePtr<BIO> bio(BIO_new(BIO_s_file()));
  if (!bio || !BIO_read_filename(bio.get(), file.c_str())) {
    return nullptr;
  }
  return bssl::UniquePtr<EVP_PKEY>(
      PEM_read_bio_PrivateKey(bio.get(), NULL, NULL, NULL));
}

template<typename T>
struct Free {
  void operator()(T *buf) {
    free(buf);
  }
};

static bool GetCertificate(SSL *ssl, bssl::UniquePtr<X509> *out_x509,
                           bssl::UniquePtr<EVP_PKEY> *out_pkey) {
  const TestConfig *config = GetTestConfig(ssl);

  if (!config->key_file.empty()) {
    *out_pkey = LoadPrivateKey(config->key_file.c_str());
    if (!*out_pkey) {
      return false;
    }
  }
  if (!config->cert_file.empty()) {
    *out_x509 = LoadCertificate(config->cert_file.c_str());
    if (!*out_x509) {
      return false;
    }
  }
  return true;
}

static bool InstallCertificate(SSL *ssl) {
  bssl::UniquePtr<X509> x509;
  bssl::UniquePtr<EVP_PKEY> pkey;
  if (!GetCertificate(ssl, &x509, &pkey)) {
    return false;
  }

  if (pkey && !SSL_use_PrivateKey(ssl, pkey.get())) {
    return false;
  }

  if (x509 && !SSL_use_certificate(ssl, x509.get())) {
    return false;
  }

  return true;
}

static int ClientCertCallback(SSL *ssl, X509 **out_x509, EVP_PKEY **out_pkey) {
  if (GetTestConfig(ssl)->async && !GetTestState(ssl)->cert_ready) {
    return -1;
  }

  bssl::UniquePtr<X509> x509;
  bssl::UniquePtr<EVP_PKEY> pkey;
  if (!GetCertificate(ssl, &x509, &pkey)) {
    return -1;
  }

  // Return zero for no certificate.
  if (!x509) {
    return 0;
  }

  // Asynchronous private keys are not supported with client_cert_cb.
  *out_x509 = x509.release();
  *out_pkey = pkey.release();
  return 1;
}

static int VerifySucceed(X509_STORE_CTX *store_ctx, void *arg) {
  return 1;
}

static int VerifyFail(X509_STORE_CTX *store_ctx, void *arg) {
  X509_STORE_CTX_set_error(store_ctx, X509_V_ERR_APPLICATION_VERIFICATION);
  return 0;
}

static int NextProtosAdvertisedCallback(SSL *ssl, const uint8_t **out,
                                        unsigned int *out_len, void *arg) {
  const TestConfig *config = GetTestConfig(ssl);
  if (config->advertise_npn.empty()) {
    return SSL_TLSEXT_ERR_NOACK;
  }

  *out = (const uint8_t*)config->advertise_npn.data();
  *out_len = config->advertise_npn.size();
  return SSL_TLSEXT_ERR_OK;
}

static int NextProtoSelectCallback(SSL* ssl, uint8_t** out, uint8_t* outlen,
                                   const uint8_t* in, unsigned inlen, void* arg) {
  const TestConfig *config = GetTestConfig(ssl);
  if (config->select_next_proto.empty()) {
    return SSL_TLSEXT_ERR_NOACK;
  }

  *out = (uint8_t*)config->select_next_proto.data();
  *outlen = config->select_next_proto.size();
  return SSL_TLSEXT_ERR_OK;
}

static int AlpnSelectCallback(SSL* ssl, const uint8_t** out, uint8_t* outlen,
                              const uint8_t* in, unsigned inlen, void* arg) {
  if (GetTestState(ssl)->alpn_select_done) {
    fprintf(stderr, "AlpnSelectCallback called after completion.\n");
    exit(1);
  }

  GetTestState(ssl)->alpn_select_done = true;

  const TestConfig *config = GetTestConfig(ssl);
  if (config->decline_alpn) {
    return SSL_TLSEXT_ERR_NOACK;
  }

  if (!config->expected_advertised_alpn.empty() &&
      (config->expected_advertised_alpn.size() != inlen ||
       memcmp(config->expected_advertised_alpn.data(),
              in, inlen) != 0)) {
    fprintf(stderr, "bad ALPN select callback inputs\n");
    exit(1);
  }

  *out = (const uint8_t*)config->select_alpn.data();
  *outlen = config->select_alpn.size();
  return SSL_TLSEXT_ERR_OK;
}

static unsigned PskClientCallback(SSL *ssl, const char *hint,
                                  char *out_identity,
                                  unsigned max_identity_len,
                                  uint8_t *out_psk, unsigned max_psk_len) {
  const TestConfig *config = GetTestConfig(ssl);

  if (config->psk_identity.empty()) {
    if (hint != nullptr) {
      fprintf(stderr, "Server PSK hint was non-null.\n");
      return 0;
    }
  } else if (hint == nullptr ||
             strcmp(hint, config->psk_identity.c_str()) != 0) {
    fprintf(stderr, "Server PSK hint did not match.\n");
    return 0;
  }

  // Account for the trailing '\0' for the identity.
  if (config->psk_identity.size() >= max_identity_len ||
      config->psk.size() > max_psk_len) {
    fprintf(stderr, "PSK buffers too small\n");
    return 0;
  }

  BUF_strlcpy(out_identity, config->psk_identity.c_str(),
              max_identity_len);
  memcpy(out_psk, config->psk.data(), config->psk.size());
  return config->psk.size();
}

static unsigned PskServerCallback(SSL *ssl, const char *identity,
                                  uint8_t *out_psk, unsigned max_psk_len) {
  const TestConfig *config = GetTestConfig(ssl);

  if (strcmp(identity, config->psk_identity.c_str()) != 0) {
    fprintf(stderr, "Client PSK identity did not match.\n");
    return 0;
  }

  if (config->psk.size() > max_psk_len) {
    fprintf(stderr, "PSK buffers too small\n");
    return 0;
  }

  memcpy(out_psk, config->psk.data(), config->psk.size());
  return config->psk.size();
}

static int CertCallback(SSL *ssl, void *arg) {
  const TestConfig *config = GetTestConfig(ssl);

  // Check the CertificateRequest metadata is as expected.
  //
  // TODO(davidben): Test |SSL_get_client_CA_list|.
  if (!SSL_is_server(ssl) &&
      !config->expected_certificate_types.empty()) {
    const uint8_t *certificate_types;
    size_t certificate_types_len =
        SSL_get0_certificate_types(ssl, &certificate_types);
    if (certificate_types_len != config->expected_certificate_types.size() ||
        memcmp(certificate_types,
               config->expected_certificate_types.data(),
               certificate_types_len) != 0) {
      fprintf(stderr, "certificate types mismatch\n");
      return 0;
    }
  }

  // The certificate will be installed via other means.
  if (!config->async ||
      config->use_old_client_cert_callback) {
    return 1;
  }

  if (!GetTestState(ssl)->cert_ready) {
    return -1;
  }
  if (!InstallCertificate(ssl)) {
    return 0;
  }
  return 1;
}

static void InfoCallback(const SSL *ssl, int type, int val) {
  if (type == SSL_CB_HANDSHAKE_DONE) {
    if (GetTestConfig(ssl)->handshake_never_done) {
      fprintf(stderr, "Handshake unexpectedly completed.\n");
      // Abort before any expected error code is printed, to ensure the overall
      // test fails.
      abort();
    }
    GetTestState(ssl)->handshake_done = true;

    // Callbacks may be called again on a new handshake.
    GetTestState(ssl)->ticket_decrypt_done = false;
    GetTestState(ssl)->alpn_select_done = false;
  }
}

static int NewSessionCallback(SSL *ssl, SSL_SESSION *session) {
  GetTestState(ssl)->got_new_session = true;
  GetTestState(ssl)->new_session.reset(session);
  return 1;
}

static int TicketKeyCallback(SSL *ssl, uint8_t *key_name, uint8_t *iv,
                             EVP_CIPHER_CTX *ctx, HMAC_CTX *hmac_ctx,
                             int encrypt) {
  if (!encrypt) {
    if (GetTestState(ssl)->ticket_decrypt_done) {
      fprintf(stderr, "TicketKeyCallback called after completion.\n");
      return -1;
    }

    GetTestState(ssl)->ticket_decrypt_done = true;
  }

  // This is just test code, so use the all-zeros key.
  static const uint8_t kZeros[16] = {0};

  if (encrypt) {
    memcpy(key_name, kZeros, sizeof(kZeros));
    RAND_bytes(iv, 16);
  } else if (memcmp(key_name, kZeros, 16) != 0) {
    return 0;
  }

  if (!HMAC_Init_ex(hmac_ctx, kZeros, sizeof(kZeros), EVP_sha256(), NULL) ||
      !EVP_CipherInit_ex(ctx, EVP_aes_128_cbc(), NULL, kZeros, iv, encrypt)) {
    return -1;
  }

  if (!encrypt) {
    return GetTestConfig(ssl)->renew_ticket ? 2 : 1;
  }
  return 1;
}

// kCustomExtensionValue is the extension value that the custom extension
// callbacks will add.
static const uint16_t kCustomExtensionValue = 1234;
static void *const kCustomExtensionAddArg =
    reinterpret_cast<void *>(kCustomExtensionValue);
static void *const kCustomExtensionParseArg =
    reinterpret_cast<void *>(kCustomExtensionValue + 1);
static const char kCustomExtensionContents[] = "custom extension";

static int CustomExtensionAddCallback(SSL *ssl, unsigned extension_value,
                                      const uint8_t **out, size_t *out_len,
                                      int *out_alert_value, void *add_arg) {
  if (extension_value != kCustomExtensionValue ||
      add_arg != kCustomExtensionAddArg) {
    abort();
  }

  if (GetTestConfig(ssl)->custom_extension_skip) {
    return 0;
  }
  if (GetTestConfig(ssl)->custom_extension_fail_add) {
    return -1;
  }

  *out = reinterpret_cast<const uint8_t*>(kCustomExtensionContents);
  *out_len = sizeof(kCustomExtensionContents) - 1;

  return 1;
}

static void CustomExtensionFreeCallback(SSL *ssl, unsigned extension_value,
                                        const uint8_t *out, void *add_arg) {
  if (extension_value != kCustomExtensionValue ||
      add_arg != kCustomExtensionAddArg ||
      out != reinterpret_cast<const uint8_t *>(kCustomExtensionContents)) {
    abort();
  }
}

static int CustomExtensionParseCallback(SSL *ssl, unsigned extension_value,
                                        const uint8_t *contents,
                                        size_t contents_len,
                                        int *out_alert_value, void *parse_arg) {
  if (extension_value != kCustomExtensionValue ||
      parse_arg != kCustomExtensionParseArg) {
    abort();
  }

  if (contents_len != sizeof(kCustomExtensionContents) - 1 ||
      memcmp(contents, kCustomExtensionContents, contents_len) != 0) {
    *out_alert_value = SSL_AD_DECODE_ERROR;
    return 0;
  }

  return 1;
}

static int ServerNameCallback(SSL *ssl, int *out_alert, void *arg) {
  // SNI must be accessible from the SNI callback.
  const TestConfig *config = GetTestConfig(ssl);
  const char *server_name = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
  if (server_name == nullptr ||
      std::string(server_name) != config->expected_server_name) {
    fprintf(stderr, "servername mismatch (got %s; want %s)\n", server_name,
            config->expected_server_name.c_str());
    return SSL_TLSEXT_ERR_ALERT_FATAL;
  }

  return SSL_TLSEXT_ERR_OK;
}

// Connect returns a new socket connected to localhost on |port| or -1 on
// error.
static int Connect(uint16_t port) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == -1) {
    PrintSocketError("socket");
    return -1;
  }
  int nodelay = 1;
  if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
          reinterpret_cast<const char*>(&nodelay), sizeof(nodelay)) != 0) {
    PrintSocketError("setsockopt");
    closesocket(sock);
    return -1;
  }
  sockaddr_in sin;
  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port = htons(port);
  if (!inet_pton(AF_INET, "127.0.0.1", &sin.sin_addr)) {
    PrintSocketError("inet_pton");
    closesocket(sock);
    return -1;
  }
  if (connect(sock, reinterpret_cast<const sockaddr*>(&sin),
              sizeof(sin)) != 0) {
    PrintSocketError("connect");
    closesocket(sock);
    return -1;
  }
  return sock;
}

class SocketCloser {
 public:
  explicit SocketCloser(int sock) : sock_(sock) {}
  ~SocketCloser() {
    // Half-close and drain the socket before releasing it. This seems to be
    // necessary for graceful shutdown on Windows. It will also avoid write
    // failures in the test runner.
#if defined(OPENSSL_SYS_WINDOWS)
    shutdown(sock_, SD_SEND);
#else
    shutdown(sock_, SHUT_WR);
#endif
    while (true) {
      char buf[1024];
      if (recv(sock_, buf, sizeof(buf), 0) <= 0) {
        break;
      }
    }
    closesocket(sock_);
  }

 private:
  const int sock_;
};

static bssl::UniquePtr<SSL_CTX> SetupCtx(const TestConfig *config) {
  const char sess_id_ctx[] = "ossl_shim";
  bssl::UniquePtr<SSL_CTX> ssl_ctx(SSL_CTX_new(
      config->is_dtls ? DTLS_method() : TLS_method()));
  if (!ssl_ctx) {
    return nullptr;
  }

  SSL_CTX_set_security_level(ssl_ctx.get(), 0);
#if 0
  /* Disabled for now until we have some TLS1.3 support */
  // Enable TLS 1.3 for tests.
  if (!config->is_dtls &&
      !SSL_CTX_set_max_proto_version(ssl_ctx.get(), TLS1_3_VERSION)) {
    return nullptr;
  }
#else
  /* Ensure we don't negotiate TLSv1.3 until we can handle it */
  if (!config->is_dtls &&
      !SSL_CTX_set_max_proto_version(ssl_ctx.get(), TLS1_2_VERSION)) {
    return nullptr;
  }
#endif

  std::string cipher_list = "ALL";
  if (!config->cipher.empty()) {
    cipher_list = config->cipher;
    SSL_CTX_set_options(ssl_ctx.get(), SSL_OP_CIPHER_SERVER_PREFERENCE);
  }
  if (!SSL_CTX_set_cipher_list(ssl_ctx.get(), cipher_list.c_str())) {
    return nullptr;
  }

  DH *tmpdh;

  if (config->use_sparse_dh_prime) {
    BIGNUM *p, *g;
    p = BN_new();
    g = BN_new();
    tmpdh = DH_new();
    if (p == NULL || g == NULL || tmpdh == NULL) {
        BN_free(p);
        BN_free(g);
        DH_free(tmpdh);
        return nullptr;
    }
    // This prime number is 2^1024 + 643 – a value just above a power of two.
    // Because of its form, values modulo it are essentially certain to be one
    // byte shorter. This is used to test padding of these values.
    if (BN_hex2bn(
            &p,
            "1000000000000000000000000000000000000000000000000000000000000000"
            "0000000000000000000000000000000000000000000000000000000000000000"
            "0000000000000000000000000000000000000000000000000000000000000000"
            "0000000000000000000000000000000000000000000000000000000000000028"
            "3") == 0 ||
        !BN_set_word(g, 2)) {
      BN_free(p);
      BN_free(g);
      DH_free(tmpdh);
      return nullptr;
    }
    DH_set0_pqg(tmpdh, p, NULL, g);
  } else {
      tmpdh = DH_get_2048_256();
  }

  bssl::UniquePtr<DH> dh(tmpdh);

  if (!dh || !SSL_CTX_set_tmp_dh(ssl_ctx.get(), dh.get())) {
    return nullptr;
  }

  SSL_CTX_set_session_cache_mode(ssl_ctx.get(), SSL_SESS_CACHE_BOTH);

  if (config->use_old_client_cert_callback) {
    SSL_CTX_set_client_cert_cb(ssl_ctx.get(), ClientCertCallback);
  }

  SSL_CTX_set_npn_advertised_cb(
      ssl_ctx.get(), NextProtosAdvertisedCallback, NULL);
  if (!config->select_next_proto.empty()) {
    SSL_CTX_set_next_proto_select_cb(ssl_ctx.get(), NextProtoSelectCallback,
                                     NULL);
  }

  if (!config->select_alpn.empty() || config->decline_alpn) {
    SSL_CTX_set_alpn_select_cb(ssl_ctx.get(), AlpnSelectCallback, NULL);
  }

  SSL_CTX_set_info_callback(ssl_ctx.get(), InfoCallback);
  SSL_CTX_sess_set_new_cb(ssl_ctx.get(), NewSessionCallback);

  if (config->use_ticket_callback) {
    SSL_CTX_set_tlsext_ticket_key_cb(ssl_ctx.get(), TicketKeyCallback);
  }

  if (config->enable_client_custom_extension &&
      !SSL_CTX_add_client_custom_ext(
          ssl_ctx.get(), kCustomExtensionValue, CustomExtensionAddCallback,
          CustomExtensionFreeCallback, kCustomExtensionAddArg,
          CustomExtensionParseCallback, kCustomExtensionParseArg)) {
    return nullptr;
  }

  if (config->enable_server_custom_extension &&
      !SSL_CTX_add_server_custom_ext(
          ssl_ctx.get(), kCustomExtensionValue, CustomExtensionAddCallback,
          CustomExtensionFreeCallback, kCustomExtensionAddArg,
          CustomExtensionParseCallback, kCustomExtensionParseArg)) {
    return nullptr;
  }

  if (config->verify_fail) {
    SSL_CTX_set_cert_verify_callback(ssl_ctx.get(), VerifyFail, NULL);
  } else {
    SSL_CTX_set_cert_verify_callback(ssl_ctx.get(), VerifySucceed, NULL);
  }

  if (config->use_null_client_ca_list) {
    SSL_CTX_set_client_CA_list(ssl_ctx.get(), nullptr);
  }

  if (!SSL_CTX_set_session_id_context(ssl_ctx.get(),
                                      (const unsigned char *)sess_id_ctx,
                                      sizeof(sess_id_ctx) - 1))
    return nullptr;

  if (!config->expected_server_name.empty()) {
    SSL_CTX_set_tlsext_servername_callback(ssl_ctx.get(), ServerNameCallback);
  }

  return ssl_ctx;
}

// RetryAsync is called after a failed operation on |ssl| with return code
// |ret|. If the operation should be retried, it simulates one asynchronous
// event and returns true. Otherwise it returns false.
static bool RetryAsync(SSL *ssl, int ret) {
  // No error; don't retry.
  if (ret >= 0) {
    return false;
  }

  TestState *test_state = GetTestState(ssl);
  assert(GetTestConfig(ssl)->async);

  if (test_state->packeted_bio != nullptr &&
      PacketedBioAdvanceClock(test_state->packeted_bio)) {
    // The DTLS retransmit logic silently ignores write failures. So the test
    // may progress, allow writes through synchronously.
    AsyncBioEnforceWriteQuota(test_state->async_bio, false);
    int timeout_ret = DTLSv1_handle_timeout(ssl);
    AsyncBioEnforceWriteQuota(test_state->async_bio, true);

    if (timeout_ret < 0) {
      fprintf(stderr, "Error retransmitting.\n");
      return false;
    }
    return true;
  }

  // See if we needed to read or write more. If so, allow one byte through on
  // the appropriate end to maximally stress the state machine.
  switch (SSL_get_error(ssl, ret)) {
    case SSL_ERROR_WANT_READ:
      AsyncBioAllowRead(test_state->async_bio, 1);
      return true;
    case SSL_ERROR_WANT_WRITE:
      AsyncBioAllowWrite(test_state->async_bio, 1);
      return true;
    case SSL_ERROR_WANT_X509_LOOKUP:
      test_state->cert_ready = true;
      return true;
    default:
      return false;
  }
}

// DoRead reads from |ssl|, resolving any asynchronous operations. It returns
// the result value of the final |SSL_read| call.
static int DoRead(SSL *ssl, uint8_t *out, size_t max_out) {
  const TestConfig *config = GetTestConfig(ssl);
  TestState *test_state = GetTestState(ssl);
  int ret;
  do {
    if (config->async) {
      // The DTLS retransmit logic silently ignores write failures. So the test
      // may progress, allow writes through synchronously. |SSL_read| may
      // trigger a retransmit, so disconnect the write quota.
      AsyncBioEnforceWriteQuota(test_state->async_bio, false);
    }
    ret = config->peek_then_read ? SSL_peek(ssl, out, max_out)
                                 : SSL_read(ssl, out, max_out);
    if (config->async) {
      AsyncBioEnforceWriteQuota(test_state->async_bio, true);
    }
  } while (config->async && RetryAsync(ssl, ret));

  if (config->peek_then_read && ret > 0) {
    std::unique_ptr<uint8_t[]> buf(new uint8_t[static_cast<size_t>(ret)]);

    // SSL_peek should synchronously return the same data.
    int ret2 = SSL_peek(ssl, buf.get(), ret);
    if (ret2 != ret ||
        memcmp(buf.get(), out, ret) != 0) {
      fprintf(stderr, "First and second SSL_peek did not match.\n");
      return -1;
    }

    // SSL_read should synchronously return the same data and consume it.
    ret2 = SSL_read(ssl, buf.get(), ret);
    if (ret2 != ret ||
        memcmp(buf.get(), out, ret) != 0) {
      fprintf(stderr, "SSL_peek and SSL_read did not match.\n");
      return -1;
    }
  }

  return ret;
}

// WriteAll writes |in_len| bytes from |in| to |ssl|, resolving any asynchronous
// operations. It returns the result of the final |SSL_write| call.
static int WriteAll(SSL *ssl, const uint8_t *in, size_t in_len) {
  const TestConfig *config = GetTestConfig(ssl);
  int ret;
  do {
    ret = SSL_write(ssl, in, in_len);
    if (ret > 0) {
      in += ret;
      in_len -= ret;
    }
  } while ((config->async && RetryAsync(ssl, ret)) || (ret > 0 && in_len > 0));
  return ret;
}

// DoShutdown calls |SSL_shutdown|, resolving any asynchronous operations. It
// returns the result of the final |SSL_shutdown| call.
static int DoShutdown(SSL *ssl) {
  const TestConfig *config = GetTestConfig(ssl);
  int ret;
  do {
    ret = SSL_shutdown(ssl);
  } while (config->async && RetryAsync(ssl, ret));
  return ret;
}

static uint16_t GetProtocolVersion(const SSL *ssl) {
  uint16_t version = SSL_version(ssl);
  if (!SSL_is_dtls(ssl)) {
    return version;
  }
  return 0x0201 + ~version;
}

// CheckHandshakeProperties checks, immediately after |ssl| completes its
// initial handshake (or False Starts), whether all the properties are
// consistent with the test configuration and invariants.
static bool CheckHandshakeProperties(SSL *ssl, bool is_resume) {
  const TestConfig *config = GetTestConfig(ssl);

  if (SSL_get_current_cipher(ssl) == nullptr) {
    fprintf(stderr, "null cipher after handshake\n");
    return false;
  }

  if (is_resume &&
      (!!SSL_session_reused(ssl) == config->expect_session_miss)) {
    fprintf(stderr, "session was%s reused\n",
            SSL_session_reused(ssl) ? "" : " not");
    return false;
  }

  if (!GetTestState(ssl)->handshake_done) {
    fprintf(stderr, "handshake was not completed\n");
    return false;
  }

  if (!config->is_server) {
    bool expect_new_session =
        !config->expect_no_session &&
        (!SSL_session_reused(ssl) || config->expect_ticket_renewal) &&
        // Session tickets are sent post-handshake in TLS 1.3.
        GetProtocolVersion(ssl) < TLS1_3_VERSION;
    if (expect_new_session != GetTestState(ssl)->got_new_session) {
      fprintf(stderr,
              "new session was%s cached, but we expected the opposite\n",
              GetTestState(ssl)->got_new_session ? "" : " not");
      return false;
    }
  }

  if (!config->expected_server_name.empty()) {
    const char *server_name =
        SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
    if (server_name == nullptr ||
            std::string(server_name) != config->expected_server_name) {
      fprintf(stderr, "servername mismatch (got %s; want %s)\n",
              server_name, config->expected_server_name.c_str());
      return false;
    }
  }

  if (!config->expected_next_proto.empty()) {
    const uint8_t *next_proto;
    unsigned next_proto_len;
    SSL_get0_next_proto_negotiated(ssl, &next_proto, &next_proto_len);
    if (next_proto_len != config->expected_next_proto.size() ||
        memcmp(next_proto, config->expected_next_proto.data(),
               next_proto_len) != 0) {
      fprintf(stderr, "negotiated next proto mismatch\n");
      return false;
    }
  }

  if (!config->expected_alpn.empty()) {
    const uint8_t *alpn_proto;
    unsigned alpn_proto_len;
    SSL_get0_alpn_selected(ssl, &alpn_proto, &alpn_proto_len);
    if (alpn_proto_len != config->expected_alpn.size() ||
        memcmp(alpn_proto, config->expected_alpn.data(),
               alpn_proto_len) != 0) {
      fprintf(stderr, "negotiated alpn proto mismatch\n");
      return false;
    }
  }

  if (config->expect_extended_master_secret) {
    if (!SSL_get_extms_support(ssl)) {
      fprintf(stderr, "No EMS for connection when expected");
      return false;
    }
  }

  if (config->expect_verify_result) {
    int expected_verify_result = config->verify_fail ?
      X509_V_ERR_APPLICATION_VERIFICATION :
      X509_V_OK;

    if (SSL_get_verify_result(ssl) != expected_verify_result) {
      fprintf(stderr, "Wrong certificate verification result\n");
      return false;
    }
  }

  if (!config->psk.empty()) {
    if (SSL_get_peer_cert_chain(ssl) != nullptr) {
      fprintf(stderr, "Received peer certificate on a PSK cipher.\n");
      return false;
    }
  } else if (!config->is_server || config->require_any_client_certificate) {
    if (SSL_get_peer_certificate(ssl) == nullptr) {
      fprintf(stderr, "Received no peer certificate but expected one.\n");
      return false;
    }
  }

  return true;
}

// DoExchange runs a test SSL exchange against the peer. On success, it returns
// true and sets |*out_session| to the negotiated SSL session. If the test is a
// resumption attempt, |is_resume| is true and |session| is the session from the
// previous exchange.
static bool DoExchange(bssl::UniquePtr<SSL_SESSION> *out_session,
                       SSL_CTX *ssl_ctx, const TestConfig *config,
                       bool is_resume, SSL_SESSION *session) {
  bssl::UniquePtr<SSL> ssl(SSL_new(ssl_ctx));
  if (!ssl) {
    return false;
  }

  if (!SetTestConfig(ssl.get(), config) ||
      !SetTestState(ssl.get(), std::unique_ptr<TestState>(new TestState))) {
    return false;
  }

  if (config->fallback_scsv &&
      !SSL_set_mode(ssl.get(), SSL_MODE_SEND_FALLBACK_SCSV)) {
    return false;
  }
  // Install the certificate synchronously if nothing else will handle it.
  if (!config->use_old_client_cert_callback &&
      !config->async &&
      !InstallCertificate(ssl.get())) {
    return false;
  }
  SSL_set_cert_cb(ssl.get(), CertCallback, nullptr);
  if (config->require_any_client_certificate) {
    SSL_set_verify(ssl.get(), SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                   NULL);
  }
  if (config->verify_peer) {
    SSL_set_verify(ssl.get(), SSL_VERIFY_PEER, NULL);
  }
  if (config->partial_write) {
    SSL_set_mode(ssl.get(), SSL_MODE_ENABLE_PARTIAL_WRITE);
  }
  if (config->no_tls13) {
    SSL_set_options(ssl.get(), SSL_OP_NO_TLSv1_3);
  }
  if (config->no_tls12) {
    SSL_set_options(ssl.get(), SSL_OP_NO_TLSv1_2);
  }
  if (config->no_tls11) {
    SSL_set_options(ssl.get(), SSL_OP_NO_TLSv1_1);
  }
  if (config->no_tls1) {
    SSL_set_options(ssl.get(), SSL_OP_NO_TLSv1);
  }
  if (config->no_ssl3) {
    SSL_set_options(ssl.get(), SSL_OP_NO_SSLv3);
  }
  if (!config->host_name.empty() &&
      !SSL_set_tlsext_host_name(ssl.get(), config->host_name.c_str())) {
    return false;
  }
  if (!config->advertise_alpn.empty() &&
      SSL_set_alpn_protos(ssl.get(),
                          (const uint8_t *)config->advertise_alpn.data(),
                          config->advertise_alpn.size()) != 0) {
    return false;
  }
  if (!config->psk.empty()) {
    SSL_set_psk_client_callback(ssl.get(), PskClientCallback);
    SSL_set_psk_server_callback(ssl.get(), PskServerCallback);
  }
  if (!config->psk_identity.empty() &&
      !SSL_use_psk_identity_hint(ssl.get(), config->psk_identity.c_str())) {
    return false;
  }
  if (!config->srtp_profiles.empty() &&
      SSL_set_tlsext_use_srtp(ssl.get(), config->srtp_profiles.c_str())) {
    return false;
  }
  if (config->min_version != 0 &&
      !SSL_set_min_proto_version(ssl.get(), (uint16_t)config->min_version)) {
    return false;
  }
  if (config->max_version != 0 &&
      !SSL_set_max_proto_version(ssl.get(), (uint16_t)config->max_version)) {
    return false;
  }
  if (config->mtu != 0) {
    SSL_set_options(ssl.get(), SSL_OP_NO_QUERY_MTU);
    SSL_set_mtu(ssl.get(), config->mtu);
  }
  if (config->renegotiate_freely) {
    // This is always on for OpenSSL.
  }
  if (!config->check_close_notify) {
    SSL_set_quiet_shutdown(ssl.get(), 1);
  }
  if (config->p384_only) {
    int nid = NID_secp384r1;
    if (!SSL_set1_curves(ssl.get(), &nid, 1)) {
      return false;
    }
  }
  if (config->enable_all_curves) {
    static const int kAllCurves[] = {
      NID_X25519, NID_X9_62_prime256v1, NID_X448, NID_secp521r1, NID_secp384r1
    };
    if (!SSL_set1_curves(ssl.get(), kAllCurves,
                         OPENSSL_ARRAY_SIZE(kAllCurves))) {
      return false;
    }
  }
  if (config->max_cert_list > 0) {
    SSL_set_max_cert_list(ssl.get(), config->max_cert_list);
  }

  if (!config->async) {
    SSL_set_mode(ssl.get(), SSL_MODE_AUTO_RETRY);
  }

  int sock = Connect(config->port);
  if (sock == -1) {
    return false;
  }
  SocketCloser closer(sock);

  bssl::UniquePtr<BIO> bio(BIO_new_socket(sock, BIO_NOCLOSE));
  if (!bio) {
    return false;
  }
  if (config->is_dtls) {
    bssl::UniquePtr<BIO> packeted = PacketedBioCreate(!config->async);
    if (!packeted) {
      return false;
    }
    GetTestState(ssl.get())->packeted_bio = packeted.get();
    BIO_push(packeted.get(), bio.release());
    bio = std::move(packeted);
  }
  if (config->async) {
    bssl::UniquePtr<BIO> async_scoped =
        config->is_dtls ? AsyncBioCreateDatagram() : AsyncBioCreate();
    if (!async_scoped) {
      return false;
    }
    BIO_push(async_scoped.get(), bio.release());
    GetTestState(ssl.get())->async_bio = async_scoped.get();
    bio = std::move(async_scoped);
  }
  SSL_set_bio(ssl.get(), bio.get(), bio.get());
  bio.release();  // SSL_set_bio takes ownership.

  if (session != NULL) {
    if (!config->is_server) {
      if (SSL_set_session(ssl.get(), session) != 1) {
        return false;
      }
    }
  }

#if 0
  // KNOWN BUG: OpenSSL's SSL_get_current_cipher behaves incorrectly when
  // offering resumption.
  if (SSL_get_current_cipher(ssl.get()) != nullptr) {
    fprintf(stderr, "non-null cipher before handshake\n");
    return false;
  }
#endif

  int ret;
  if (config->implicit_handshake) {
    if (config->is_server) {
      SSL_set_accept_state(ssl.get());
    } else {
      SSL_set_connect_state(ssl.get());
    }
  } else {
    do {
      if (config->is_server) {
        ret = SSL_accept(ssl.get());
      } else {
        ret = SSL_connect(ssl.get());
      }
    } while (config->async && RetryAsync(ssl.get(), ret));
    if (ret != 1 ||
        !CheckHandshakeProperties(ssl.get(), is_resume)) {
      return false;
    }

    // Reset the state to assert later that the callback isn't called in
    // renegotiations.
    GetTestState(ssl.get())->got_new_session = false;
  }

  if (config->export_keying_material > 0) {
    std::vector<uint8_t> result(
        static_cast<size_t>(config->export_keying_material));
    if (SSL_export_keying_material(
            ssl.get(), result.data(), result.size(),
            config->export_label.data(), config->export_label.size(),
            reinterpret_cast<const uint8_t*>(config->export_context.data()),
            config->export_context.size(), config->use_export_context) != 1) {
      fprintf(stderr, "failed to export keying material\n");
      return false;
    }
    if (WriteAll(ssl.get(), result.data(), result.size()) < 0) {
      return false;
    }
  }

  if (config->write_different_record_sizes) {
    if (config->is_dtls) {
      fprintf(stderr, "write_different_record_sizes not supported for DTLS\n");
      return false;
    }
    // This mode writes a number of different record sizes in an attempt to
    // trip up the CBC record splitting code.
    static const size_t kBufLen = 32769;
    std::unique_ptr<uint8_t[]> buf(new uint8_t[kBufLen]);
    memset(buf.get(), 0x42, kBufLen);
    static const size_t kRecordSizes[] = {
        0, 1, 255, 256, 257, 16383, 16384, 16385, 32767, 32768, 32769};
    for (size_t i = 0; i < OPENSSL_ARRAY_SIZE(kRecordSizes); i++) {
      const size_t len = kRecordSizes[i];
      if (len > kBufLen) {
        fprintf(stderr, "Bad kRecordSizes value.\n");
        return false;
      }
      if (WriteAll(ssl.get(), buf.get(), len) < 0) {
        return false;
      }
    }
  } else {
    if (config->shim_writes_first) {
      if (WriteAll(ssl.get(), reinterpret_cast<const uint8_t *>("hello"),
                   5) < 0) {
        return false;
      }
    }
    if (!config->shim_shuts_down) {
      for (;;) {
        static const size_t kBufLen = 16384;
        std::unique_ptr<uint8_t[]> buf(new uint8_t[kBufLen]);

        // Read only 512 bytes at a time in TLS to ensure records may be
        // returned in multiple reads.
        int n = DoRead(ssl.get(), buf.get(), config->is_dtls ? kBufLen : 512);
        int err = SSL_get_error(ssl.get(), n);
        if (err == SSL_ERROR_ZERO_RETURN ||
            (n == 0 && err == SSL_ERROR_SYSCALL)) {
          if (n != 0) {
            fprintf(stderr, "Invalid SSL_get_error output\n");
            return false;
          }
          // Stop on either clean or unclean shutdown.
          break;
        } else if (err != SSL_ERROR_NONE) {
          if (n > 0) {
            fprintf(stderr, "Invalid SSL_get_error output\n");
            return false;
          }
          return false;
        }
        // Successfully read data.
        if (n <= 0) {
          fprintf(stderr, "Invalid SSL_get_error output\n");
          return false;
        }

        // After a successful read, with or without False Start, the handshake
        // must be complete.
        if (!GetTestState(ssl.get())->handshake_done) {
          fprintf(stderr, "handshake was not completed after SSL_read\n");
          return false;
        }

        for (int i = 0; i < n; i++) {
          buf[i] ^= 0xff;
        }
        if (WriteAll(ssl.get(), buf.get(), n) < 0) {
          return false;
        }
      }
    }
  }

  if (!config->is_server &&
      !config->implicit_handshake &&
      // Session tickets are sent post-handshake in TLS 1.3.
      GetProtocolVersion(ssl.get()) < TLS1_3_VERSION &&
      GetTestState(ssl.get())->got_new_session) {
    fprintf(stderr, "new session was established after the handshake\n");
    return false;
  }

  if (GetProtocolVersion(ssl.get()) >= TLS1_3_VERSION && !config->is_server) {
    bool expect_new_session =
        !config->expect_no_session && !config->shim_shuts_down;
    if (expect_new_session != GetTestState(ssl.get())->got_new_session) {
      fprintf(stderr,
              "new session was%s cached, but we expected the opposite\n",
              GetTestState(ssl.get())->got_new_session ? "" : " not");
      return false;
    }
  }

  if (out_session) {
    *out_session = std::move(GetTestState(ssl.get())->new_session);
  }

  ret = DoShutdown(ssl.get());

  if (config->shim_shuts_down && config->check_close_notify) {
    // We initiate shutdown, so |SSL_shutdown| will return in two stages. First
    // it returns zero when our close_notify is sent, then one when the peer's
    // is received.
    if (ret != 0) {
      fprintf(stderr, "Unexpected SSL_shutdown result: %d != 0\n", ret);
      return false;
    }
    ret = DoShutdown(ssl.get());
  }

  if (ret != 1) {
    fprintf(stderr, "Unexpected SSL_shutdown result: %d != 1\n", ret);
    return false;
  }

  if (SSL_total_renegotiations(ssl.get()) !=
      config->expect_total_renegotiations) {
    fprintf(stderr, "Expected %d renegotiations, got %ld\n",
            config->expect_total_renegotiations,
            SSL_total_renegotiations(ssl.get()));
    return false;
  }

  return true;
}

class StderrDelimiter {
 public:
  ~StderrDelimiter() { fprintf(stderr, "--- DONE ---\n"); }
};

static int Main(int argc, char **argv) {
  // To distinguish ASan's output from ours, add a trailing message to stderr.
  // Anything following this line will be considered an error.
  StderrDelimiter delimiter;

#if defined(OPENSSL_SYS_WINDOWS)
  /* Initialize Winsock. */
  WORD wsa_version = MAKEWORD(2, 2);
  WSADATA wsa_data;
  int wsa_err = WSAStartup(wsa_version, &wsa_data);
  if (wsa_err != 0) {
    fprintf(stderr, "WSAStartup failed: %d\n", wsa_err);
    return 1;
  }
  if (wsa_data.wVersion != wsa_version) {
    fprintf(stderr, "Didn't get expected version: %x\n", wsa_data.wVersion);
    return 1;
  }
#else
  signal(SIGPIPE, SIG_IGN);
#endif

  OPENSSL_init_crypto(0, NULL);
  OPENSSL_init_ssl(0, NULL);
  g_config_index = SSL_get_ex_new_index(0, NULL, NULL, NULL, NULL);
  g_state_index = SSL_get_ex_new_index(0, NULL, NULL, NULL, TestStateExFree);
  if (g_config_index < 0 || g_state_index < 0) {
    return 1;
  }

  TestConfig config;
  if (!ParseConfig(argc - 1, argv + 1, &config)) {
    return Usage(argv[0]);
  }

  bssl::UniquePtr<SSL_CTX> ssl_ctx = SetupCtx(&config);
  if (!ssl_ctx) {
    ERR_print_errors_fp(stderr);
    return 1;
  }

  bssl::UniquePtr<SSL_SESSION> session;
  for (int i = 0; i < config.resume_count + 1; i++) {
    bool is_resume = i > 0;
    if (is_resume && !config.is_server && !session) {
      fprintf(stderr, "No session to offer.\n");
      return 1;
    }

    bssl::UniquePtr<SSL_SESSION> offer_session = std::move(session);
    if (!DoExchange(&session, ssl_ctx.get(), &config, is_resume,
                    offer_session.get())) {
      fprintf(stderr, "Connection %d failed.\n", i + 1);
      ERR_print_errors_fp(stderr);
      return 1;
    }
  }

  return 0;
}

}  // namespace bssl

int main(int argc, char **argv) {
  return bssl::Main(argc, argv);
}
