/*
 * ngtcp2
 *
 * Copyright (c) 2020 ngtcp2 contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <assert.h>
#include <string.h>

#include <ngtcp2/ngtcp2_crypto.h>
#include <ngtcp2/ngtcp2_crypto_boringssl.h>

#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <openssl/hkdf.h>
#include <openssl/chacha.h>

#include "shared.h"

/* Define cipher types because BoringSSL does not implement EVP
   interface for chacha20. */
typedef enum ngtcp2_crypto_boringssl_cipher_type {
  NGTCP2_CRYPTO_BORINGSSL_CIPHER_TYPE_EVP_AES_128_CTR,
  NGTCP2_CRYPTO_BORINGSSL_CIPHER_TYPE_EVP_AES_256_CTR,
  NGTCP2_CRYPTO_BORINGSSL_CIPHER_TYPE_CHACHA20,
} ngtcp2_crypto_boringssl_cipher_type;

typedef struct ngtcp2_crypto_boringssl_cipher {
  ngtcp2_crypto_boringssl_cipher_type type;
} ngtcp2_crypto_boringssl_cipher;

static ngtcp2_crypto_boringssl_cipher crypto_cipher_evp_aes_128_ctr = {
    NGTCP2_CRYPTO_BORINGSSL_CIPHER_TYPE_EVP_AES_128_CTR,
};

static ngtcp2_crypto_boringssl_cipher crypto_cipher_evp_aes_256_ctr = {
    NGTCP2_CRYPTO_BORINGSSL_CIPHER_TYPE_EVP_AES_256_CTR,
};

static ngtcp2_crypto_boringssl_cipher crypto_cipher_chacha20 = {
    NGTCP2_CRYPTO_BORINGSSL_CIPHER_TYPE_CHACHA20,
};

ngtcp2_crypto_ctx *ngtcp2_crypto_ctx_initial(ngtcp2_crypto_ctx *ctx) {
  ngtcp2_crypto_aead_init(&ctx->aead, (void *)EVP_aead_aes_128_gcm());
  ctx->md.native_handle = (void *)EVP_sha256();
  ctx->hp.native_handle = (void *)&crypto_cipher_evp_aes_128_ctr;
  ctx->max_encryption = 0;
  ctx->max_decryption_failure = 0;
  return ctx;
}

ngtcp2_crypto_aead *ngtcp2_crypto_aead_init(ngtcp2_crypto_aead *aead,
                                            void *aead_native_handle) {
  aead->native_handle = aead_native_handle;
  aead->max_overhead = EVP_AEAD_max_overhead(aead->native_handle);
  return aead;
}

ngtcp2_crypto_aead *ngtcp2_crypto_aead_retry(ngtcp2_crypto_aead *aead) {
  return ngtcp2_crypto_aead_init(aead, (void *)EVP_aead_aes_128_gcm());
}

static const EVP_AEAD *crypto_ssl_get_aead(SSL *ssl) {
  switch (SSL_CIPHER_get_id(SSL_get_current_cipher(ssl))) {
  case TLS1_CK_AES_128_GCM_SHA256:
    return EVP_aead_aes_128_gcm();
  case TLS1_CK_AES_256_GCM_SHA384:
    return EVP_aead_aes_256_gcm();
  case TLS1_CK_CHACHA20_POLY1305_SHA256:
    return EVP_aead_chacha20_poly1305();
  default:
    return NULL;
  }
}

static uint64_t crypto_ssl_get_aead_max_encryption(SSL *ssl) {
  switch (SSL_CIPHER_get_id(SSL_get_current_cipher(ssl))) {
  case TLS1_CK_AES_128_GCM_SHA256:
  case TLS1_CK_AES_256_GCM_SHA384:
    return NGTCP2_CRYPTO_MAX_ENCRYPTION_AES_GCM;
  case TLS1_CK_CHACHA20_POLY1305_SHA256:
    return NGTCP2_CRYPTO_MAX_ENCRYPTION_CHACHA20_POLY1305;
  default:
    return 0;
  }
}

static uint64_t crypto_ssl_get_aead_max_decryption_failure(SSL *ssl) {
  switch (SSL_CIPHER_get_id(SSL_get_current_cipher(ssl))) {
  case TLS1_CK_AES_128_GCM_SHA256:
  case TLS1_CK_AES_256_GCM_SHA384:
    return NGTCP2_CRYPTO_MAX_DECRYPTION_FAILURE_AES_GCM;
  case TLS1_CK_CHACHA20_POLY1305_SHA256:
    return NGTCP2_CRYPTO_MAX_DECRYPTION_FAILURE_CHACHA20_POLY1305;
  default:
    return 0;
  }
}

static const ngtcp2_crypto_boringssl_cipher *crypto_ssl_get_hp(SSL *ssl) {
  switch (SSL_CIPHER_get_id(SSL_get_current_cipher(ssl))) {
  case TLS1_CK_AES_128_GCM_SHA256:
    return &crypto_cipher_evp_aes_128_ctr;
  case TLS1_CK_AES_256_GCM_SHA384:
    return &crypto_cipher_evp_aes_256_ctr;
  case TLS1_CK_CHACHA20_POLY1305_SHA256:
    return &crypto_cipher_chacha20;
  default:
    return NULL;
  }
}

static const EVP_MD *crypto_ssl_get_md(SSL *ssl) {
  switch (SSL_CIPHER_get_id(SSL_get_current_cipher(ssl))) {
  case TLS1_CK_AES_128_GCM_SHA256:
  case TLS1_CK_CHACHA20_POLY1305_SHA256:
    return EVP_sha256();
  case TLS1_CK_AES_256_GCM_SHA384:
    return EVP_sha384();
  default:
    return NULL;
  }
}

ngtcp2_crypto_ctx *ngtcp2_crypto_ctx_tls(ngtcp2_crypto_ctx *ctx,
                                         void *tls_native_handle) {
  SSL *ssl = tls_native_handle;
  ngtcp2_crypto_aead_init(&ctx->aead, (void *)crypto_ssl_get_aead(ssl));
  ctx->md.native_handle = (void *)crypto_ssl_get_md(ssl);
  ctx->hp.native_handle = (void *)crypto_ssl_get_hp(ssl);
  ctx->max_encryption = crypto_ssl_get_aead_max_encryption(ssl);
  ctx->max_decryption_failure = crypto_ssl_get_aead_max_decryption_failure(ssl);
  return ctx;
}

ngtcp2_crypto_ctx *ngtcp2_crypto_ctx_tls_early(ngtcp2_crypto_ctx *ctx,
                                               void *tls_native_handle) {
  return ngtcp2_crypto_ctx_tls(ctx, tls_native_handle);
}

static size_t crypto_md_hashlen(const EVP_MD *md) {
  return (size_t)EVP_MD_size(md);
}

size_t ngtcp2_crypto_md_hashlen(const ngtcp2_crypto_md *md) {
  return crypto_md_hashlen(md->native_handle);
}

static size_t crypto_aead_keylen(const EVP_AEAD *aead) {
  return (size_t)EVP_AEAD_key_length(aead);
}

size_t ngtcp2_crypto_aead_keylen(const ngtcp2_crypto_aead *aead) {
  return crypto_aead_keylen(aead->native_handle);
}

static size_t crypto_aead_noncelen(const EVP_AEAD *aead) {
  return (size_t)EVP_AEAD_nonce_length(aead);
}

size_t ngtcp2_crypto_aead_noncelen(const ngtcp2_crypto_aead *aead) {
  return crypto_aead_noncelen(aead->native_handle);
}

int ngtcp2_crypto_aead_ctx_encrypt_init(ngtcp2_crypto_aead_ctx *aead_ctx,
                                        const ngtcp2_crypto_aead *aead,
                                        const uint8_t *key, size_t noncelen) {
  const EVP_AEAD *cipher = aead->native_handle;
  size_t keylen = crypto_aead_keylen(cipher);
  EVP_AEAD_CTX *actx;

  (void)noncelen;

  actx = EVP_AEAD_CTX_new(cipher, key, keylen, EVP_AEAD_DEFAULT_TAG_LENGTH);
  if (actx == NULL) {
    return -1;
  }

  aead_ctx->native_handle = actx;

  return 0;
}

int ngtcp2_crypto_aead_ctx_decrypt_init(ngtcp2_crypto_aead_ctx *aead_ctx,
                                        const ngtcp2_crypto_aead *aead,
                                        const uint8_t *key, size_t noncelen) {
  return ngtcp2_crypto_aead_ctx_encrypt_init(aead_ctx, aead, key, noncelen);
}

void ngtcp2_crypto_aead_ctx_free(ngtcp2_crypto_aead_ctx *aead_ctx) {
  if (aead_ctx->native_handle) {
    EVP_AEAD_CTX_free(aead_ctx->native_handle);
  }
}

typedef enum ngtcp2_crypto_boringssl_cipher_ctx_type {
  NGTCP2_CRYPTO_BORINGSSL_CIPHER_CTX_TYPE_EVP,
  NGTCP2_CRYPTO_BORINGSSL_CIPHER_CTX_TYPE_CHACHA20,
} ngtcp2_crypto_boringssl_cipher_ctx_type;

typedef struct ngtcp2_crypto_boringssl_cipher_ctx {
  ngtcp2_crypto_boringssl_cipher_ctx_type type;
  union {
    /* ctx is EVP_CIPHER_CTX used when type ==
       NGTCP2_CRYPTO_BORINGSSL_CIPHER_CTX_TYPE_EVP. */
    EVP_CIPHER_CTX *ctx;
    /* key contains an encryption key when type ==
       NGTCP2_CRYPTO_BORINGSSL_CIPHER_CTX_TYPE_CHACHA20. */
    uint8_t key[32];
  };
} ngtcp2_crypto_boringssl_cipher_ctx;

int ngtcp2_crypto_cipher_ctx_encrypt_init(ngtcp2_crypto_cipher_ctx *cipher_ctx,
                                          const ngtcp2_crypto_cipher *cipher,
                                          const uint8_t *key) {
  ngtcp2_crypto_boringssl_cipher *hp_cipher = cipher->native_handle;
  ngtcp2_crypto_boringssl_cipher_ctx *ctx;
  EVP_CIPHER_CTX *actx;
  const EVP_CIPHER *evp_cipher;

  switch (hp_cipher->type) {
  case NGTCP2_CRYPTO_BORINGSSL_CIPHER_TYPE_EVP_AES_128_CTR:
    evp_cipher = EVP_aes_128_ctr();
    break;
  case NGTCP2_CRYPTO_BORINGSSL_CIPHER_TYPE_EVP_AES_256_CTR:
    evp_cipher = EVP_aes_256_ctr();
    break;
  case NGTCP2_CRYPTO_BORINGSSL_CIPHER_TYPE_CHACHA20:
    ctx = malloc(sizeof(*ctx));
    if (ctx == NULL) {
      return -1;
    }

    ctx->type = NGTCP2_CRYPTO_BORINGSSL_CIPHER_CTX_TYPE_CHACHA20;
    memcpy(ctx->key, key, sizeof(ctx->key));

    cipher_ctx->native_handle = ctx;

    return 0;
  default:
    assert(0);
  };

  actx = EVP_CIPHER_CTX_new();
  if (actx == NULL) {
    return -1;
  }

  if (!EVP_EncryptInit_ex(actx, evp_cipher, NULL, key, NULL)) {
    EVP_CIPHER_CTX_free(actx);
    return -1;
  }

  ctx = malloc(sizeof(*ctx));
  if (ctx == NULL) {
    EVP_CIPHER_CTX_free(actx);
    return -1;
  }

  ctx->type = NGTCP2_CRYPTO_BORINGSSL_CIPHER_CTX_TYPE_EVP;
  ctx->ctx = actx;

  cipher_ctx->native_handle = ctx;

  return 0;
}

void ngtcp2_crypto_cipher_ctx_free(ngtcp2_crypto_cipher_ctx *cipher_ctx) {
  ngtcp2_crypto_boringssl_cipher_ctx *ctx;

  if (!cipher_ctx->native_handle) {
    return;
  }

  ctx = cipher_ctx->native_handle;

  if (ctx->type == NGTCP2_CRYPTO_BORINGSSL_CIPHER_CTX_TYPE_EVP) {
    EVP_CIPHER_CTX_free(ctx->ctx);
  }

  free(ctx);
}

int ngtcp2_crypto_hkdf_extract(uint8_t *dest, const ngtcp2_crypto_md *md,
                               const uint8_t *secret, size_t secretlen,
                               const uint8_t *salt, size_t saltlen) {
  const EVP_MD *prf = md->native_handle;
  size_t destlen = (size_t)EVP_MD_size(prf);

  if (HKDF_extract(dest, &destlen, prf, secret, secretlen, salt, saltlen) !=
      1) {
    return -1;
  }

  return 0;
}

int ngtcp2_crypto_hkdf_expand(uint8_t *dest, size_t destlen,
                              const ngtcp2_crypto_md *md, const uint8_t *secret,
                              size_t secretlen, const uint8_t *info,
                              size_t infolen) {
  const EVP_MD *prf = md->native_handle;

  if (HKDF_expand(dest, destlen, prf, secret, secretlen, info, infolen) != 1) {
    return -1;
  }

  return 0;
}

int ngtcp2_crypto_encrypt(uint8_t *dest, const ngtcp2_crypto_aead *aead,
                          const ngtcp2_crypto_aead_ctx *aead_ctx,
                          const uint8_t *plaintext, size_t plaintextlen,
                          const uint8_t *nonce, size_t noncelen,
                          const uint8_t *ad, size_t adlen) {
  const EVP_AEAD *cipher = aead->native_handle;
  EVP_AEAD_CTX *actx = aead_ctx->native_handle;
  size_t max_outlen = plaintextlen + EVP_AEAD_max_overhead(cipher);
  size_t outlen;

  if (EVP_AEAD_CTX_seal(actx, dest, &outlen, max_outlen, nonce, noncelen,
                        plaintext, plaintextlen, ad, adlen) != 1) {
    return -1;
  }

  return 0;
}

int ngtcp2_crypto_decrypt(uint8_t *dest, const ngtcp2_crypto_aead *aead,
                          const ngtcp2_crypto_aead_ctx *aead_ctx,
                          const uint8_t *ciphertext, size_t ciphertextlen,
                          const uint8_t *nonce, size_t noncelen,
                          const uint8_t *ad, size_t adlen) {
  EVP_AEAD_CTX *actx = aead_ctx->native_handle;
  size_t max_outlen = ciphertextlen;
  size_t outlen;

  (void)aead;

  if (EVP_AEAD_CTX_open(actx, dest, &outlen, max_outlen, nonce, noncelen,
                        ciphertext, ciphertextlen, ad, adlen) != 1) {
    return -1;
  }

  return 0;
}

int ngtcp2_crypto_hp_mask(uint8_t *dest, const ngtcp2_crypto_cipher *hp,
                          const ngtcp2_crypto_cipher_ctx *hp_ctx,
                          const uint8_t *sample) {
  static const uint8_t PLAINTEXT[] = "\x00\x00\x00\x00\x00";
  ngtcp2_crypto_boringssl_cipher_ctx *ctx = hp_ctx->native_handle;
  EVP_CIPHER_CTX *actx;
  int len;
  uint32_t counter;

  (void)hp;

  switch (ctx->type) {
  case NGTCP2_CRYPTO_BORINGSSL_CIPHER_CTX_TYPE_EVP:
    actx = ctx->ctx;
    if (!EVP_EncryptInit_ex(actx, NULL, NULL, NULL, sample) ||
        !EVP_EncryptUpdate(actx, dest, &len, PLAINTEXT,
                           sizeof(PLAINTEXT) - 1) ||
        !EVP_EncryptFinal_ex(actx, dest + sizeof(PLAINTEXT) - 1, &len)) {
      return -1;
    }
    return 0;
  case NGTCP2_CRYPTO_BORINGSSL_CIPHER_CTX_TYPE_CHACHA20:
#if defined(WORDS_BIGENDIAN)
    counter = (uint32_t)sample[0] + (uint32_t)(sample[1] << 8) +
              (uint32_t)(sample[2] << 16) + (uint32_t)(sample[3] << 24);
#else  /* !WORDS_BIGENDIAN */
    memcpy(&counter, sample, sizeof(counter));
#endif /* !WORDS_BIGENDIAN */
    CRYPTO_chacha_20(dest, PLAINTEXT, sizeof(PLAINTEXT) - 1, ctx->key,
                     sample + sizeof(counter), counter);
    return 0;
  default:
    assert(0);
    abort();
  }
}

int ngtcp2_crypto_read_write_crypto_data(ngtcp2_conn *conn,
                                         ngtcp2_crypto_level crypto_level,
                                         const uint8_t *data, size_t datalen) {
  SSL *ssl = ngtcp2_conn_get_tls_native_handle(conn);
  int rv;
  int err;

  if (SSL_provide_quic_data(
          ssl, ngtcp2_crypto_boringssl_from_ngtcp2_crypto_level(crypto_level),
          data, datalen) != 1) {
    return -1;
  }

  if (!ngtcp2_conn_get_handshake_completed(conn)) {
  retry:
    rv = SSL_do_handshake(ssl);
    if (rv <= 0) {
      err = SSL_get_error(ssl, rv);
      switch (err) {
      case SSL_ERROR_WANT_READ:
      case SSL_ERROR_WANT_WRITE:
        return 0;
      case SSL_ERROR_SSL:
        return -1;
      case SSL_ERROR_EARLY_DATA_REJECTED:
        assert(!ngtcp2_conn_is_server(conn));

        SSL_reset_early_data_reject(ssl);

        rv = ngtcp2_conn_early_data_rejected(conn);
        if (rv != 0) {
          return -1;
        }

        goto retry;
      default:
        return -1;
      }
    }

    if (SSL_in_early_data(ssl)) {
      return 0;
    }

    ngtcp2_conn_handshake_completed(conn);
  }

  rv = SSL_process_quic_post_handshake(ssl);
  if (rv != 1) {
    err = SSL_get_error(ssl, rv);
    switch (err) {
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
      return 0;
    case SSL_ERROR_SSL:
    case SSL_ERROR_ZERO_RETURN:
      return -1;
    default:
      return -1;
    }
  }

  return 0;
}

int ngtcp2_crypto_set_remote_transport_params(ngtcp2_conn *conn, void *tls) {
  SSL *ssl = tls;
  ngtcp2_transport_params_type exttype =
      ngtcp2_conn_is_server(conn)
          ? NGTCP2_TRANSPORT_PARAMS_TYPE_CLIENT_HELLO
          : NGTCP2_TRANSPORT_PARAMS_TYPE_ENCRYPTED_EXTENSIONS;
  const uint8_t *tp;
  size_t tplen;
  ngtcp2_transport_params params;
  int rv;

  SSL_get_peer_quic_transport_params(ssl, &tp, &tplen);

  rv = ngtcp2_decode_transport_params(&params, exttype, tp, tplen);
  if (rv != 0) {
    ngtcp2_conn_set_tls_error(conn, rv);
    return -1;
  }

  rv = ngtcp2_conn_set_remote_transport_params(conn, &params);
  if (rv != 0) {
    ngtcp2_conn_set_tls_error(conn, rv);
    return -1;
  }

  return 0;
}

int ngtcp2_crypto_set_local_transport_params(void *tls, const uint8_t *buf,
                                             size_t len) {
  if (SSL_set_quic_transport_params(tls, buf, len) != 1) {
    return -1;
  }

  return 0;
}

ngtcp2_crypto_level ngtcp2_crypto_boringssl_from_ssl_encryption_level(
    enum ssl_encryption_level_t ssl_level) {
  switch (ssl_level) {
  case ssl_encryption_initial:
    return NGTCP2_CRYPTO_LEVEL_INITIAL;
  case ssl_encryption_early_data:
    return NGTCP2_CRYPTO_LEVEL_EARLY;
  case ssl_encryption_handshake:
    return NGTCP2_CRYPTO_LEVEL_HANDSHAKE;
  case ssl_encryption_application:
    return NGTCP2_CRYPTO_LEVEL_APPLICATION;
  default:
    assert(0);
  }
}

enum ssl_encryption_level_t ngtcp2_crypto_boringssl_from_ngtcp2_crypto_level(
    ngtcp2_crypto_level crypto_level) {
  switch (crypto_level) {
  case NGTCP2_CRYPTO_LEVEL_INITIAL:
    return ssl_encryption_initial;
  case NGTCP2_CRYPTO_LEVEL_HANDSHAKE:
    return ssl_encryption_handshake;
  case NGTCP2_CRYPTO_LEVEL_APPLICATION:
    return ssl_encryption_application;
  case NGTCP2_CRYPTO_LEVEL_EARLY:
    return ssl_encryption_early_data;
  default:
    assert(0);
  }
}
