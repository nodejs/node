/*
 * ngtcp2
 *
 * Copyright (c) 2019 ngtcp2 contributors
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

#include <ngtcp2/ngtcp2_crypto.h>
#include <ngtcp2/ngtcp2_crypto_openssl.h>

#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>

#include "shared.h"

static size_t crypto_aead_max_overhead(const EVP_CIPHER *aead) {
  switch (EVP_CIPHER_nid(aead)) {
  case NID_aes_128_gcm:
  case NID_aes_256_gcm:
    return EVP_GCM_TLS_TAG_LEN;
  case NID_chacha20_poly1305:
    return EVP_CHACHAPOLY_TLS_TAG_LEN;
  case NID_aes_128_ccm:
    return EVP_CCM_TLS_TAG_LEN;
  default:
    assert(0);
  }
}

ngtcp2_crypto_ctx *ngtcp2_crypto_ctx_initial(ngtcp2_crypto_ctx *ctx) {
  ngtcp2_crypto_aead_init(&ctx->aead, (void *)EVP_aes_128_gcm());
  ctx->md.native_handle = (void *)EVP_sha256();
  ctx->hp.native_handle = (void *)EVP_aes_128_ctr();
  ctx->max_encryption = 0;
  ctx->max_decryption_failure = 0;
  return ctx;
}

ngtcp2_crypto_aead *ngtcp2_crypto_aead_init(ngtcp2_crypto_aead *aead,
                                            void *aead_native_handle) {
  aead->native_handle = aead_native_handle;
  aead->max_overhead = crypto_aead_max_overhead(aead_native_handle);
  return aead;
}

ngtcp2_crypto_aead *ngtcp2_crypto_aead_retry(ngtcp2_crypto_aead *aead) {
  return ngtcp2_crypto_aead_init(aead, (void *)EVP_aes_128_gcm());
}

static const EVP_CIPHER *crypto_ssl_get_aead(SSL *ssl) {
  switch (SSL_CIPHER_get_id(SSL_get_current_cipher(ssl))) {
  case TLS1_3_CK_AES_128_GCM_SHA256:
    return EVP_aes_128_gcm();
  case TLS1_3_CK_AES_256_GCM_SHA384:
    return EVP_aes_256_gcm();
  case TLS1_3_CK_CHACHA20_POLY1305_SHA256:
    return EVP_chacha20_poly1305();
  case TLS1_3_CK_AES_128_CCM_SHA256:
    return EVP_aes_128_ccm();
  default:
    return NULL;
  }
}

static uint64_t crypto_ssl_get_aead_max_encryption(SSL *ssl) {
  switch (SSL_CIPHER_get_id(SSL_get_current_cipher(ssl))) {
  case TLS1_3_CK_AES_128_GCM_SHA256:
  case TLS1_3_CK_AES_256_GCM_SHA384:
    return NGTCP2_CRYPTO_MAX_ENCRYPTION_AES_GCM;
  case TLS1_3_CK_CHACHA20_POLY1305_SHA256:
    return NGTCP2_CRYPTO_MAX_ENCRYPTION_CHACHA20_POLY1305;
  case TLS1_3_CK_AES_128_CCM_SHA256:
    return NGTCP2_CRYPTO_MAX_ENCRYPTION_AES_CCM;
  default:
    return 0;
  }
}

static uint64_t crypto_ssl_get_aead_max_decryption_failure(SSL *ssl) {
  switch (SSL_CIPHER_get_id(SSL_get_current_cipher(ssl))) {
  case TLS1_3_CK_AES_128_GCM_SHA256:
  case TLS1_3_CK_AES_256_GCM_SHA384:
    return NGTCP2_CRYPTO_MAX_DECRYPTION_FAILURE_AES_GCM;
  case TLS1_3_CK_CHACHA20_POLY1305_SHA256:
    return NGTCP2_CRYPTO_MAX_DECRYPTION_FAILURE_CHACHA20_POLY1305;
  case TLS1_3_CK_AES_128_CCM_SHA256:
    return NGTCP2_CRYPTO_MAX_DECRYPTION_FAILURE_AES_CCM;
  default:
    return 0;
  }
}

static const EVP_CIPHER *crypto_ssl_get_hp(SSL *ssl) {
  switch (SSL_CIPHER_get_id(SSL_get_current_cipher(ssl))) {
  case TLS1_3_CK_AES_128_GCM_SHA256:
  case TLS1_3_CK_AES_128_CCM_SHA256:
    return EVP_aes_128_ctr();
  case TLS1_3_CK_AES_256_GCM_SHA384:
    return EVP_aes_256_ctr();
  case TLS1_3_CK_CHACHA20_POLY1305_SHA256:
    return EVP_chacha20();
  default:
    return NULL;
  }
}

static const EVP_MD *crypto_ssl_get_md(SSL *ssl) {
  switch (SSL_CIPHER_get_id(SSL_get_current_cipher(ssl))) {
  case TLS1_3_CK_AES_128_GCM_SHA256:
  case TLS1_3_CK_CHACHA20_POLY1305_SHA256:
  case TLS1_3_CK_AES_128_CCM_SHA256:
    return EVP_sha256();
  case TLS1_3_CK_AES_256_GCM_SHA384:
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

static size_t crypto_aead_keylen(const EVP_CIPHER *aead) {
  return (size_t)EVP_CIPHER_key_length(aead);
}

size_t ngtcp2_crypto_aead_keylen(const ngtcp2_crypto_aead *aead) {
  return crypto_aead_keylen(aead->native_handle);
}

static size_t crypto_aead_noncelen(const EVP_CIPHER *aead) {
  return (size_t)EVP_CIPHER_iv_length(aead);
}

size_t ngtcp2_crypto_aead_noncelen(const ngtcp2_crypto_aead *aead) {
  return crypto_aead_noncelen(aead->native_handle);
}

int ngtcp2_crypto_aead_ctx_encrypt_init(ngtcp2_crypto_aead_ctx *aead_ctx,
                                        const ngtcp2_crypto_aead *aead,
                                        const uint8_t *key, size_t noncelen) {
  const EVP_CIPHER *cipher = aead->native_handle;
  int cipher_nid = EVP_CIPHER_nid(cipher);
  EVP_CIPHER_CTX *actx;

  actx = EVP_CIPHER_CTX_new();
  if (actx == NULL) {
    return -1;
  }

  if (!EVP_EncryptInit_ex(actx, cipher, NULL, NULL, NULL) ||
      !EVP_CIPHER_CTX_ctrl(actx, EVP_CTRL_AEAD_SET_IVLEN, (int)noncelen,
                           NULL) ||
      (cipher_nid == NID_aes_128_ccm &&
       !EVP_CIPHER_CTX_ctrl(actx, EVP_CTRL_AEAD_SET_TAG,
                            (int)crypto_aead_max_overhead(cipher), NULL)) ||
      !EVP_EncryptInit_ex(actx, NULL, NULL, key, NULL)) {
    EVP_CIPHER_CTX_free(actx);
    return -1;
  }

  aead_ctx->native_handle = actx;

  return 0;
}

int ngtcp2_crypto_aead_ctx_decrypt_init(ngtcp2_crypto_aead_ctx *aead_ctx,
                                        const ngtcp2_crypto_aead *aead,
                                        const uint8_t *key, size_t noncelen) {
  const EVP_CIPHER *cipher = aead->native_handle;
  int cipher_nid = EVP_CIPHER_nid(cipher);
  EVP_CIPHER_CTX *actx;

  actx = EVP_CIPHER_CTX_new();
  if (actx == NULL) {
    return -1;
  }

  if (!EVP_DecryptInit_ex(actx, cipher, NULL, NULL, NULL) ||
      !EVP_CIPHER_CTX_ctrl(actx, EVP_CTRL_AEAD_SET_IVLEN, (int)noncelen,
                           NULL) ||
      (cipher_nid == NID_aes_128_ccm &&
       !EVP_CIPHER_CTX_ctrl(actx, EVP_CTRL_AEAD_SET_TAG,
                            (int)crypto_aead_max_overhead(cipher), NULL)) ||
      !EVP_DecryptInit_ex(actx, NULL, NULL, key, NULL)) {
    EVP_CIPHER_CTX_free(actx);
    return -1;
  }

  aead_ctx->native_handle = actx;

  return 0;
}

void ngtcp2_crypto_aead_ctx_free(ngtcp2_crypto_aead_ctx *aead_ctx) {
  if (aead_ctx->native_handle) {
    EVP_CIPHER_CTX_free(aead_ctx->native_handle);
  }
}

int ngtcp2_crypto_cipher_ctx_encrypt_init(ngtcp2_crypto_cipher_ctx *cipher_ctx,
                                          const ngtcp2_crypto_cipher *cipher,
                                          const uint8_t *key) {
  EVP_CIPHER_CTX *actx;

  actx = EVP_CIPHER_CTX_new();
  if (actx == NULL) {
    return -1;
  }

  if (!EVP_EncryptInit_ex(actx, cipher->native_handle, NULL, key, NULL)) {
    EVP_CIPHER_CTX_free(actx);
    return -1;
  }

  cipher_ctx->native_handle = actx;

  return 0;
}

void ngtcp2_crypto_cipher_ctx_free(ngtcp2_crypto_cipher_ctx *cipher_ctx) {
  if (cipher_ctx->native_handle) {
    EVP_CIPHER_CTX_free(cipher_ctx->native_handle);
  }
}

int ngtcp2_crypto_hkdf_extract(uint8_t *dest, const ngtcp2_crypto_md *md,
                               const uint8_t *secret, size_t secretlen,
                               const uint8_t *salt, size_t saltlen) {
  const EVP_MD *prf = md->native_handle;
  int rv = 0;
  EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, NULL);
  size_t destlen = (size_t)EVP_MD_size(prf);

  if (pctx == NULL) {
    return -1;
  }

  if (EVP_PKEY_derive_init(pctx) != 1 ||
      EVP_PKEY_CTX_hkdf_mode(pctx, EVP_PKEY_HKDEF_MODE_EXTRACT_ONLY) != 1 ||
      EVP_PKEY_CTX_set_hkdf_md(pctx, prf) != 1 ||
      EVP_PKEY_CTX_set1_hkdf_salt(pctx, salt, (int)saltlen) != 1 ||
      EVP_PKEY_CTX_set1_hkdf_key(pctx, secret, (int)secretlen) != 1 ||
      EVP_PKEY_derive(pctx, dest, &destlen) != 1) {
    rv = -1;
  }

  EVP_PKEY_CTX_free(pctx);

  return rv;
}

int ngtcp2_crypto_hkdf_expand(uint8_t *dest, size_t destlen,
                              const ngtcp2_crypto_md *md, const uint8_t *secret,
                              size_t secretlen, const uint8_t *info,
                              size_t infolen) {
  const EVP_MD *prf = md->native_handle;
  int rv = 0;
  EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, NULL);
  if (pctx == NULL) {
    return -1;
  }

  if (EVP_PKEY_derive_init(pctx) != 1 ||
      EVP_PKEY_CTX_hkdf_mode(pctx, EVP_PKEY_HKDEF_MODE_EXPAND_ONLY) != 1 ||
      EVP_PKEY_CTX_set_hkdf_md(pctx, prf) != 1 ||
      EVP_PKEY_CTX_set1_hkdf_salt(pctx, "", 0) != 1 ||
      EVP_PKEY_CTX_set1_hkdf_key(pctx, secret, (int)secretlen) != 1 ||
      EVP_PKEY_CTX_add1_hkdf_info(pctx, info, (int)infolen) != 1 ||
      EVP_PKEY_derive(pctx, dest, &destlen) != 1) {
    rv = -1;
  }

  EVP_PKEY_CTX_free(pctx);

  return rv;
}

int ngtcp2_crypto_encrypt(uint8_t *dest, const ngtcp2_crypto_aead *aead,
                          const ngtcp2_crypto_aead_ctx *aead_ctx,
                          const uint8_t *plaintext, size_t plaintextlen,
                          const uint8_t *nonce, size_t noncelen,
                          const uint8_t *ad, size_t adlen) {
  const EVP_CIPHER *cipher = aead->native_handle;
  size_t taglen = crypto_aead_max_overhead(cipher);
  int cipher_nid = EVP_CIPHER_nid(cipher);
  EVP_CIPHER_CTX *actx = aead_ctx->native_handle;
  int len;

  (void)noncelen;

  if (!EVP_EncryptInit_ex(actx, NULL, NULL, NULL, nonce) ||
      (cipher_nid == NID_aes_128_ccm &&
       !EVP_EncryptUpdate(actx, NULL, &len, NULL, (int)plaintextlen)) ||
      !EVP_EncryptUpdate(actx, NULL, &len, ad, (int)adlen) ||
      !EVP_EncryptUpdate(actx, dest, &len, plaintext, (int)plaintextlen) ||
      !EVP_EncryptFinal_ex(actx, dest + len, &len) ||
      !EVP_CIPHER_CTX_ctrl(actx, EVP_CTRL_AEAD_GET_TAG, (int)taglen,
                           dest + plaintextlen)) {
    return -1;
  }

  return 0;
}

int ngtcp2_crypto_decrypt(uint8_t *dest, const ngtcp2_crypto_aead *aead,
                          const ngtcp2_crypto_aead_ctx *aead_ctx,
                          const uint8_t *ciphertext, size_t ciphertextlen,
                          const uint8_t *nonce, size_t noncelen,
                          const uint8_t *ad, size_t adlen) {
  const EVP_CIPHER *cipher = aead->native_handle;
  size_t taglen = crypto_aead_max_overhead(cipher);
  int cipher_nid = EVP_CIPHER_nid(cipher);
  EVP_CIPHER_CTX *actx = aead_ctx->native_handle;
  int len;
  const uint8_t *tag;

  (void)noncelen;

  if (taglen > ciphertextlen) {
    return -1;
  }

  ciphertextlen -= taglen;
  tag = ciphertext + ciphertextlen;

  if (!EVP_DecryptInit_ex(actx, NULL, NULL, NULL, nonce) ||
      !EVP_CIPHER_CTX_ctrl(actx, EVP_CTRL_AEAD_SET_TAG, (int)taglen,
                           (uint8_t *)tag) ||
      (cipher_nid == NID_aes_128_ccm &&
       !EVP_DecryptUpdate(actx, NULL, &len, NULL, (int)ciphertextlen)) ||
      !EVP_DecryptUpdate(actx, NULL, &len, ad, (int)adlen) ||
      !EVP_DecryptUpdate(actx, dest, &len, ciphertext, (int)ciphertextlen) ||
      (cipher_nid != NID_aes_128_ccm &&
       !EVP_DecryptFinal_ex(actx, dest + ciphertextlen, &len))) {
    return -1;
  }

  return 0;
}

int ngtcp2_crypto_hp_mask(uint8_t *dest, const ngtcp2_crypto_cipher *hp,
                          const ngtcp2_crypto_cipher_ctx *hp_ctx,
                          const uint8_t *sample) {
  static const uint8_t PLAINTEXT[] = "\x00\x00\x00\x00\x00";
  EVP_CIPHER_CTX *actx = hp_ctx->native_handle;
  int len;

  (void)hp;

  if (!EVP_EncryptInit_ex(actx, NULL, NULL, NULL, sample) ||
      !EVP_EncryptUpdate(actx, dest, &len, PLAINTEXT, sizeof(PLAINTEXT) - 1) ||
      !EVP_EncryptFinal_ex(actx, dest + sizeof(PLAINTEXT) - 1, &len)) {
    return -1;
  }

  return 0;
}

int ngtcp2_crypto_read_write_crypto_data(ngtcp2_conn *conn,
                                         ngtcp2_crypto_level crypto_level,
                                         const uint8_t *data, size_t datalen) {
  SSL *ssl = ngtcp2_conn_get_tls_native_handle(conn);
  int rv;
  int err;

  if (SSL_provide_quic_data(
          ssl, ngtcp2_crypto_openssl_from_ngtcp2_crypto_level(crypto_level),
          data, datalen) != 1) {
    return -1;
  }

  if (!ngtcp2_conn_get_handshake_completed(conn)) {
    rv = SSL_do_handshake(ssl);
    if (rv <= 0) {
      err = SSL_get_error(ssl, rv);
      switch (err) {
      case SSL_ERROR_WANT_READ:
      case SSL_ERROR_WANT_WRITE:
        return 0;
      case SSL_ERROR_WANT_CLIENT_HELLO_CB:
        return NGTCP2_CRYPTO_OPENSSL_ERR_TLS_WANT_CLIENT_HELLO_CB;
      case SSL_ERROR_WANT_X509_LOOKUP:
        return NGTCP2_CRYPTO_OPENSSL_ERR_TLS_WANT_X509_LOOKUP;
      case SSL_ERROR_SSL:
        return -1;
      default:
        return -1;
      }
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

ngtcp2_crypto_level ngtcp2_crypto_openssl_from_ossl_encryption_level(
    OSSL_ENCRYPTION_LEVEL ossl_level) {
  switch (ossl_level) {
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

OSSL_ENCRYPTION_LEVEL
ngtcp2_crypto_openssl_from_ngtcp2_crypto_level(
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
