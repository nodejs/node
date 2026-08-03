/*
 * ngtcp2
 *
 * Copyright (c) 2025 ngtcp2 contributors
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
#endif /* defined(HAVE_CONFIG_H) */

#include <assert.h>

#include <ngtcp2/ngtcp2_crypto.h>
#include <ngtcp2/ngtcp2_crypto_ossl.h>

#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <openssl/core_names.h>

#include "ngtcp2_macro.h"
#include "shared.h"

static int crypto_initialized;
static EVP_CIPHER *crypto_aes_128_gcm;
static EVP_CIPHER *crypto_aes_256_gcm;
static EVP_CIPHER *crypto_chacha20_poly1305;
static EVP_CIPHER *crypto_aes_128_ccm;
static EVP_CIPHER *crypto_aes_128_ctr;
static EVP_CIPHER *crypto_aes_256_ctr;
static EVP_CIPHER *crypto_chacha20;
static EVP_MD *crypto_sha256;
static EVP_MD *crypto_sha384;
static EVP_KDF *crypto_hkdf;

int ngtcp2_crypto_ossl_init(void) {
  crypto_aes_128_gcm = EVP_CIPHER_fetch(NULL, "AES-128-GCM", NULL);
  if (crypto_aes_128_gcm == NULL) {
    return -1;
  }

  crypto_aes_256_gcm = EVP_CIPHER_fetch(NULL, "AES-256-GCM", NULL);
  if (crypto_aes_256_gcm == NULL) {
    return -1;
  }

  crypto_chacha20_poly1305 = EVP_CIPHER_fetch(NULL, "ChaCha20-Poly1305", NULL);
  if (crypto_chacha20_poly1305 == NULL) {
    return -1;
  }

  crypto_aes_128_ccm = EVP_CIPHER_fetch(NULL, "AES-128-CCM", NULL);
  if (crypto_aes_128_ccm == NULL) {
    return -1;
  }

  crypto_aes_128_ctr = EVP_CIPHER_fetch(NULL, "AES-128-CTR", NULL);
  if (crypto_aes_128_ctr == NULL) {
    return -1;
  }

  crypto_aes_256_ctr = EVP_CIPHER_fetch(NULL, "AES-256-CTR", NULL);
  if (crypto_aes_256_ctr == NULL) {
    return -1;
  }

  crypto_chacha20 = EVP_CIPHER_fetch(NULL, "ChaCha20", NULL);
  if (crypto_chacha20 == NULL) {
    return -1;
  }

  crypto_sha256 = EVP_MD_fetch(NULL, "sha256", NULL);
  if (crypto_sha256 == NULL) {
    return -1;
  }

  crypto_sha384 = EVP_MD_fetch(NULL, "sha384", NULL);
  if (crypto_sha384 == NULL) {
    return -1;
  }

  crypto_hkdf = EVP_KDF_fetch(NULL, "hkdf", NULL);
  if (crypto_hkdf == NULL) {
    return -1;
  }

  crypto_initialized = 1;

  return 0;
}

static const EVP_CIPHER *crypto_aead_aes_128_gcm(void) {
  if (crypto_aes_128_gcm) {
    return crypto_aes_128_gcm;
  }

  return EVP_aes_128_gcm();
}

static const EVP_CIPHER *crypto_aead_aes_256_gcm(void) {
  if (crypto_aes_256_gcm) {
    return crypto_aes_256_gcm;
  }

  return EVP_aes_256_gcm();
}

static const EVP_CIPHER *crypto_aead_chacha20_poly1305(void) {
  if (crypto_chacha20_poly1305) {
    return crypto_chacha20_poly1305;
  }

  return EVP_chacha20_poly1305();
}

static const EVP_CIPHER *crypto_aead_aes_128_ccm(void) {
  if (crypto_aes_128_ccm) {
    return crypto_aes_128_ccm;
  }

  return EVP_aes_128_ccm();
}

static const EVP_CIPHER *crypto_cipher_aes_128_ctr(void) {
  if (crypto_aes_128_ctr) {
    return crypto_aes_128_ctr;
  }

  return EVP_aes_128_ctr();
}

static const EVP_CIPHER *crypto_cipher_aes_256_ctr(void) {
  if (crypto_aes_256_ctr) {
    return crypto_aes_256_ctr;
  }

  return EVP_aes_256_ctr();
}

static const EVP_CIPHER *crypto_cipher_chacha20(void) {
  if (crypto_chacha20) {
    return crypto_chacha20;
  }

  return EVP_chacha20();
}

static const EVP_MD *crypto_md_sha256(void) {
  if (crypto_sha256) {
    return crypto_sha256;
  }

  return EVP_sha256();
}

static const EVP_MD *crypto_md_sha384(void) {
  if (crypto_sha384) {
    return crypto_sha384;
  }

  return EVP_sha384();
}

static EVP_KDF *crypto_kdf_hkdf(void) {
  if (crypto_hkdf) {
    return crypto_hkdf;
  }

  return EVP_KDF_fetch(NULL, "hkdf", NULL);
}

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
    abort(); /* if NDEBUG is set */
  }
}

ngtcp2_crypto_aead *ngtcp2_crypto_aead_aes_128_gcm(ngtcp2_crypto_aead *aead) {
  return ngtcp2_crypto_aead_init(aead, (void *)crypto_aead_aes_128_gcm());
}

ngtcp2_crypto_md *ngtcp2_crypto_md_sha256(ngtcp2_crypto_md *md) {
  md->native_handle = (void *)crypto_md_sha256();
  return md;
}

ngtcp2_crypto_ctx *ngtcp2_crypto_ctx_initial(ngtcp2_crypto_ctx *ctx) {
  ngtcp2_crypto_aead_init(&ctx->aead, (void *)crypto_aead_aes_128_gcm());
  ctx->md.native_handle = (void *)crypto_md_sha256();
  ctx->hp.native_handle = (void *)crypto_cipher_aes_128_ctr();
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
  return ngtcp2_crypto_aead_init(aead, (void *)crypto_aead_aes_128_gcm());
}

static const EVP_CIPHER *crypto_cipher_id_get_aead(uint32_t cipher_id) {
  switch (cipher_id) {
  case TLS1_3_CK_AES_128_GCM_SHA256:
    return crypto_aead_aes_128_gcm();
  case TLS1_3_CK_AES_256_GCM_SHA384:
    return crypto_aead_aes_256_gcm();
  case TLS1_3_CK_CHACHA20_POLY1305_SHA256:
    return crypto_aead_chacha20_poly1305();
  case TLS1_3_CK_AES_128_CCM_SHA256:
    return crypto_aead_aes_128_ccm();
  default:
    return NULL;
  }
}

static uint64_t crypto_cipher_id_get_aead_max_encryption(uint32_t cipher_id) {
  switch (cipher_id) {
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

static uint64_t
crypto_cipher_id_get_aead_max_decryption_failure(uint32_t cipher_id) {
  switch (cipher_id) {
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

static const EVP_CIPHER *crypto_cipher_id_get_hp(uint32_t cipher_id) {
  switch (cipher_id) {
  case TLS1_3_CK_AES_128_GCM_SHA256:
  case TLS1_3_CK_AES_128_CCM_SHA256:
    return crypto_cipher_aes_128_ctr();
  case TLS1_3_CK_AES_256_GCM_SHA384:
    return crypto_cipher_aes_256_ctr();
  case TLS1_3_CK_CHACHA20_POLY1305_SHA256:
    return crypto_cipher_chacha20();
  default:
    return NULL;
  }
}

static const EVP_MD *crypto_cipher_id_get_md(uint32_t cipher_id) {
  switch (cipher_id) {
  case TLS1_3_CK_AES_128_GCM_SHA256:
  case TLS1_3_CK_CHACHA20_POLY1305_SHA256:
  case TLS1_3_CK_AES_128_CCM_SHA256:
    return crypto_md_sha256();
  case TLS1_3_CK_AES_256_GCM_SHA384:
    return crypto_md_sha384();
  default:
    return NULL;
  }
}

static int supported_cipher_id(uint32_t cipher_id) {
  switch (cipher_id) {
  case TLS1_3_CK_AES_128_GCM_SHA256:
  case TLS1_3_CK_AES_256_GCM_SHA384:
  case TLS1_3_CK_CHACHA20_POLY1305_SHA256:
  case TLS1_3_CK_AES_128_CCM_SHA256:
    return 1;
  default:
    return 0;
  }
}

typedef struct crypto_buf crypto_buf;

typedef struct crypto_buf {
  uint8_t data[4096];
  uint8_t *pos;
  uint8_t *last;

  crypto_buf *next;
} crypto_buf;

static crypto_buf *crypto_buf_new(void) {
  crypto_buf *cbuf = malloc(sizeof(crypto_buf));

  if (cbuf == NULL) {
    return NULL;
  }

  cbuf->pos = cbuf->last = cbuf->data;
  cbuf->next = NULL;

  return cbuf;
}

static void crypto_buf_del(crypto_buf *cbuf) { free(cbuf); }

static size_t crypto_buf_left(const crypto_buf *cbuf) {
  return (size_t)(cbuf->data + sizeof(cbuf->data) - cbuf->last);
}

static size_t crypto_buf_len(const crypto_buf *cbuf) {
  return (size_t)(cbuf->last - cbuf->pos);
}

static size_t crypto_buf_eof(const crypto_buf *cbuf) {
  return cbuf->pos == cbuf->data + sizeof(cbuf->data);
}

static void crypto_buf_write(crypto_buf *cbuf, const uint8_t *data,
                             size_t datalen) {
  assert(crypto_buf_left(cbuf) >= datalen);

  memcpy(cbuf->last, data, datalen);
  cbuf->last += datalen;
}

struct ngtcp2_crypto_ossl_ctx {
  SSL *ssl;
  ngtcp2_encryption_level tx_level;
  crypto_buf *crypto_head, *crypto_read, *crypto_write;
  size_t crypto_head_released;
  uint8_t *remote_params;
};

int ngtcp2_crypto_ossl_ctx_new(ngtcp2_crypto_ossl_ctx **possl_ctx, SSL *ssl) {
  ngtcp2_crypto_ossl_ctx *ossl_ctx = malloc(sizeof(**possl_ctx));

  if (ossl_ctx == NULL) {
    return NGTCP2_CRYPTO_ERR_NOMEM;
  }

  ossl_ctx->ssl = ssl;
  ossl_ctx->tx_level = NGTCP2_ENCRYPTION_LEVEL_INITIAL;
  ossl_ctx->crypto_head = ossl_ctx->crypto_read = ossl_ctx->crypto_write = NULL;
  ossl_ctx->crypto_head_released = 0;
  ossl_ctx->remote_params = NULL;

  *possl_ctx = ossl_ctx;

  return 0;
}

void ngtcp2_crypto_ossl_ctx_del(ngtcp2_crypto_ossl_ctx *ossl_ctx) {
  crypto_buf *cbuf, *next;

  if (!ossl_ctx) {
    return;
  }

  for (cbuf = ossl_ctx->crypto_head; cbuf;) {
    next = cbuf->next;
    crypto_buf_del(cbuf);
    cbuf = next;
  }

  free(ossl_ctx->remote_params);
  free(ossl_ctx);
}

void ngtcp2_crypto_ossl_ctx_set_ssl(ngtcp2_crypto_ossl_ctx *ossl_ctx,
                                    SSL *ssl) {
  ossl_ctx->ssl = ssl;
}

SSL *ngtcp2_crypto_ossl_ctx_get_ssl(ngtcp2_crypto_ossl_ctx *ossl_ctx) {
  return ossl_ctx->ssl;
}

static int crypto_ossl_ctx_write_crypto_data(ngtcp2_crypto_ossl_ctx *ossl_ctx,
                                             const uint8_t *data,
                                             size_t datalen) {
  crypto_buf *cbuf;
  const uint8_t *end;
  size_t n, left;

  if (datalen == 0) {
    return 0;
  }

  if (ossl_ctx->crypto_write == NULL) {
    ossl_ctx->crypto_head = ossl_ctx->crypto_read = ossl_ctx->crypto_write =
      crypto_buf_new();
    if (ossl_ctx->crypto_head == NULL) {
      return NGTCP2_CRYPTO_ERR_NOMEM;
    }
  }

  for (end = data + datalen; data != end;) {
    left = crypto_buf_left(ossl_ctx->crypto_write);
    if (left == 0) {
      cbuf = crypto_buf_new();
      if (cbuf == NULL) {
        return NGTCP2_CRYPTO_ERR_NOMEM;
      }

      ossl_ctx->crypto_write->next = cbuf;
      ossl_ctx->crypto_write = cbuf;

      left = crypto_buf_left(ossl_ctx->crypto_write);
    }

    n = ngtcp2_min_size((size_t)(end - data), left);
    crypto_buf_write(ossl_ctx->crypto_write, data, n);
    data += n;
  }

  return 0;
}

static void crypto_ossl_ctx_read_crypto_data(ngtcp2_crypto_ossl_ctx *ossl_ctx,
                                             const uint8_t **pbuf,
                                             size_t *pbytes_read) {
  size_t n;

  if (ossl_ctx->crypto_read == NULL) {
    *pbuf = NULL;
    *pbytes_read = 0;
    return;
  }

  n = crypto_buf_len(ossl_ctx->crypto_read);

  *pbuf = ossl_ctx->crypto_read->pos;
  *pbytes_read = n;

  ossl_ctx->crypto_read->pos += n;

  if (crypto_buf_eof(ossl_ctx->crypto_read) &&
      ossl_ctx->crypto_read != ossl_ctx->crypto_write) {
    ossl_ctx->crypto_read = ossl_ctx->crypto_read->next;
  }
}

static void
crypto_ossl_ctx_release_crypto_data(ngtcp2_crypto_ossl_ctx *ossl_ctx,
                                    size_t released) {
  crypto_buf *cbuf = NULL;

  if (released == 0) {
    return;
  }

  assert(ossl_ctx->crypto_head);

  ossl_ctx->crypto_head_released += released;

  for (; ossl_ctx->crypto_head_released >= sizeof(cbuf->data);) {
    assert(ossl_ctx->crypto_head);

    cbuf = ossl_ctx->crypto_head;
    ossl_ctx->crypto_head = cbuf->next;

    crypto_buf_del(cbuf);
    ossl_ctx->crypto_head_released -= sizeof(cbuf->data);
  }

  if (cbuf == ossl_ctx->crypto_read) {
    ossl_ctx->crypto_read = ossl_ctx->crypto_head;

    if (cbuf == ossl_ctx->crypto_write) {
      assert(ossl_ctx->crypto_head == NULL);

      ossl_ctx->crypto_write = NULL;
    }
  }
}

static ngtcp2_crypto_ctx *crypto_ctx_cipher_id(ngtcp2_crypto_ctx *ctx,
                                               uint32_t cipher_id) {
  ngtcp2_crypto_aead_init(&ctx->aead,
                          (void *)crypto_cipher_id_get_aead(cipher_id));
  ctx->md.native_handle = (void *)crypto_cipher_id_get_md(cipher_id);
  ctx->hp.native_handle = (void *)crypto_cipher_id_get_hp(cipher_id);
  ctx->max_encryption = crypto_cipher_id_get_aead_max_encryption(cipher_id);
  ctx->max_decryption_failure =
    crypto_cipher_id_get_aead_max_decryption_failure(cipher_id);

  return ctx;
}

ngtcp2_crypto_ctx *ngtcp2_crypto_ctx_tls(ngtcp2_crypto_ctx *ctx,
                                         void *tls_native_handle) {
  ngtcp2_crypto_ossl_ctx *ossl_ctx = tls_native_handle;
  SSL *ssl = ossl_ctx->ssl;
  const SSL_CIPHER *cipher = SSL_get_current_cipher(ssl);
  uint32_t cipher_id;

  if (cipher == NULL) {
    return NULL;
  }

  cipher_id = (uint32_t)SSL_CIPHER_get_id(cipher);

  if (!supported_cipher_id(cipher_id)) {
    return NULL;
  }

  return crypto_ctx_cipher_id(ctx, cipher_id);
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
  size_t taglen = crypto_aead_max_overhead(cipher);
  OSSL_PARAM params[3];

  actx = EVP_CIPHER_CTX_new();
  if (actx == NULL) {
    return -1;
  }

  params[0] = OSSL_PARAM_construct_size_t(OSSL_CIPHER_PARAM_IVLEN, &noncelen);

  if (cipher_nid == NID_aes_128_ccm) {
    params[1] = OSSL_PARAM_construct_octet_string(OSSL_CIPHER_PARAM_AEAD_TAG,
                                                  NULL, taglen);
    params[2] = OSSL_PARAM_construct_end();
  } else {
    params[1] = OSSL_PARAM_construct_end();
  }

  if (!EVP_EncryptInit_ex(actx, cipher, NULL, NULL, NULL) ||
      !EVP_CIPHER_CTX_set_params(actx, params) ||
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
  size_t taglen = crypto_aead_max_overhead(cipher);
  OSSL_PARAM params[3];

  actx = EVP_CIPHER_CTX_new();
  if (actx == NULL) {
    return -1;
  }

  params[0] = OSSL_PARAM_construct_size_t(OSSL_CIPHER_PARAM_IVLEN, &noncelen);

  if (cipher_nid == NID_aes_128_ccm) {
    params[1] = OSSL_PARAM_construct_octet_string(OSSL_CIPHER_PARAM_AEAD_TAG,
                                                  NULL, taglen);
    params[2] = OSSL_PARAM_construct_end();
  } else {
    params[1] = OSSL_PARAM_construct_end();
  }

  if (!EVP_DecryptInit_ex(actx, cipher, NULL, NULL, NULL) ||
      !EVP_CIPHER_CTX_set_params(actx, params) ||
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
  EVP_KDF *kdf = crypto_kdf_hkdf();
  EVP_KDF_CTX *kctx = EVP_KDF_CTX_new(kdf);
  int mode = EVP_KDF_HKDF_MODE_EXTRACT_ONLY;
  OSSL_PARAM params[] = {
    OSSL_PARAM_construct_int(OSSL_KDF_PARAM_MODE, &mode),
    OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                     (char *)EVP_MD_get0_name(prf), 0),
    OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY, (void *)secret,
                                      secretlen),
    OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT, (void *)salt,
                                      saltlen),
    OSSL_PARAM_construct_end(),
  };
  int rv = 0;

  if (!crypto_initialized) {
    EVP_KDF_free(kdf);
  }

  if (EVP_KDF_derive(kctx, dest, (size_t)EVP_MD_size(prf), params) <= 0) {
    rv = -1;
  }

  EVP_KDF_CTX_free(kctx);

  return rv;
}

int ngtcp2_crypto_hkdf_expand(uint8_t *dest, size_t destlen,
                              const ngtcp2_crypto_md *md, const uint8_t *secret,
                              size_t secretlen, const uint8_t *info,
                              size_t infolen) {
  const EVP_MD *prf = md->native_handle;
  EVP_KDF *kdf = crypto_kdf_hkdf();
  EVP_KDF_CTX *kctx = EVP_KDF_CTX_new(kdf);
  int mode = EVP_KDF_HKDF_MODE_EXPAND_ONLY;
  OSSL_PARAM params[] = {
    OSSL_PARAM_construct_int(OSSL_KDF_PARAM_MODE, &mode),
    OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                     (char *)EVP_MD_get0_name(prf), 0),
    OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY, (void *)secret,
                                      secretlen),
    OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO, (void *)info,
                                      infolen),
    OSSL_PARAM_construct_end(),
  };
  int rv = 0;

  if (!crypto_initialized) {
    EVP_KDF_free(kdf);
  }

  if (EVP_KDF_derive(kctx, dest, destlen, params) <= 0) {
    rv = -1;
  }

  EVP_KDF_CTX_free(kctx);

  return rv;
}

int ngtcp2_crypto_hkdf(uint8_t *dest, size_t destlen,
                       const ngtcp2_crypto_md *md, const uint8_t *secret,
                       size_t secretlen, const uint8_t *salt, size_t saltlen,
                       const uint8_t *info, size_t infolen) {
  const EVP_MD *prf = md->native_handle;
  EVP_KDF *kdf = crypto_kdf_hkdf();
  EVP_KDF_CTX *kctx = EVP_KDF_CTX_new(kdf);
  OSSL_PARAM params[] = {
    OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                     (char *)EVP_MD_get0_name(prf), 0),
    OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY, (void *)secret,
                                      secretlen),
    OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT, (void *)salt,
                                      saltlen),
    OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO, (void *)info,
                                      infolen),
    OSSL_PARAM_construct_end(),
  };
  int rv = 0;

  if (!crypto_initialized) {
    EVP_KDF_free(kdf);
  }

  if (EVP_KDF_derive(kctx, dest, destlen, params) <= 0) {
    rv = -1;
  }

  EVP_KDF_CTX_free(kctx);

  return rv;
}

int ngtcp2_crypto_encrypt(uint8_t *dest, const ngtcp2_crypto_aead *aead,
                          const ngtcp2_crypto_aead_ctx *aead_ctx,
                          const uint8_t *plaintext, size_t plaintextlen,
                          const uint8_t *nonce, size_t noncelen,
                          const uint8_t *aad, size_t aadlen) {
  const EVP_CIPHER *cipher = aead->native_handle;
  size_t taglen = crypto_aead_max_overhead(cipher);
  int cipher_nid = EVP_CIPHER_nid(cipher);
  EVP_CIPHER_CTX *actx = aead_ctx->native_handle;
  int len;
  OSSL_PARAM params[] = {
    OSSL_PARAM_construct_octet_string(OSSL_CIPHER_PARAM_AEAD_TAG,
                                      dest + plaintextlen, taglen),
    OSSL_PARAM_construct_end(),
  };

  (void)noncelen;

  if (!EVP_EncryptInit_ex(actx, NULL, NULL, NULL, nonce) ||
      (cipher_nid == NID_aes_128_ccm &&
       !EVP_EncryptUpdate(actx, NULL, &len, NULL, (int)plaintextlen)) ||
      !EVP_EncryptUpdate(actx, NULL, &len, aad, (int)aadlen) ||
      !EVP_EncryptUpdate(actx, dest, &len, plaintext, (int)plaintextlen) ||
      !EVP_EncryptFinal_ex(actx, dest + len, &len) ||
      !EVP_CIPHER_CTX_get_params(actx, params)) {
    return -1;
  }

  return 0;
}

int ngtcp2_crypto_decrypt(uint8_t *dest, const ngtcp2_crypto_aead *aead,
                          const ngtcp2_crypto_aead_ctx *aead_ctx,
                          const uint8_t *ciphertext, size_t ciphertextlen,
                          const uint8_t *nonce, size_t noncelen,
                          const uint8_t *aad, size_t aadlen) {
  const EVP_CIPHER *cipher = aead->native_handle;
  size_t taglen = crypto_aead_max_overhead(cipher);
  int cipher_nid = EVP_CIPHER_nid(cipher);
  EVP_CIPHER_CTX *actx = aead_ctx->native_handle;
  int len;
  const uint8_t *tag;
  OSSL_PARAM params[2];

  (void)noncelen;

  if (taglen > ciphertextlen) {
    return -1;
  }

  ciphertextlen -= taglen;
  tag = ciphertext + ciphertextlen;

  params[0] = OSSL_PARAM_construct_octet_string(OSSL_CIPHER_PARAM_AEAD_TAG,
                                                (void *)tag, taglen);
  params[1] = OSSL_PARAM_construct_end();

  if (!EVP_DecryptInit_ex(actx, NULL, NULL, NULL, nonce) ||
      !EVP_CIPHER_CTX_set_params(actx, params) ||
      (cipher_nid == NID_aes_128_ccm &&
       !EVP_DecryptUpdate(actx, NULL, &len, NULL, (int)ciphertextlen)) ||
      !EVP_DecryptUpdate(actx, NULL, &len, aad, (int)aadlen) ||
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

int ngtcp2_crypto_read_write_crypto_data(
  ngtcp2_conn *conn, ngtcp2_encryption_level encryption_level,
  const uint8_t *data, size_t datalen) {
  ngtcp2_crypto_ossl_ctx *ossl_ctx = ngtcp2_conn_get_tls_native_handle(conn);
  SSL *ssl = ossl_ctx->ssl;
  int rv;
  int err;
  (void)encryption_level;

  if (crypto_ossl_ctx_write_crypto_data(ossl_ctx, data, datalen) != 0) {
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
        return NGTCP2_CRYPTO_OSSL_ERR_TLS_WANT_CLIENT_HELLO_CB;
      case SSL_ERROR_WANT_X509_LOOKUP:
        return NGTCP2_CRYPTO_OSSL_ERR_TLS_WANT_X509_LOOKUP;
      case SSL_ERROR_SSL:
        return -1;
      default:
        return -1;
      }
    }

    ngtcp2_conn_tls_handshake_completed(conn);
  }

  rv = SSL_read(ssl, NULL, 0);
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
  (void)conn;
  (void)tls;

  return 0;
}

int ngtcp2_crypto_set_local_transport_params(void *tls, const uint8_t *buf,
                                             size_t len) {
  ngtcp2_crypto_ossl_ctx *ossl_ctx = tls;

  if (len) {
    assert(!ossl_ctx->remote_params);

    ossl_ctx->remote_params = malloc(len);
    if (!ossl_ctx->remote_params) {
      return -1;
    }

    memcpy(ossl_ctx->remote_params, buf, len);
  }

  if (SSL_set_quic_tls_transport_params(ossl_ctx->ssl, ossl_ctx->remote_params,
                                        len) != 1) {
    return -1;
  }

  return 0;
}

ngtcp2_encryption_level
ngtcp2_crypto_ossl_from_ossl_encryption_level(uint32_t ossl_level) {
  switch (ossl_level) {
  case OSSL_RECORD_PROTECTION_LEVEL_NONE:
    return NGTCP2_ENCRYPTION_LEVEL_INITIAL;
  case OSSL_RECORD_PROTECTION_LEVEL_EARLY:
    return NGTCP2_ENCRYPTION_LEVEL_0RTT;
  case OSSL_RECORD_PROTECTION_LEVEL_HANDSHAKE:
    return NGTCP2_ENCRYPTION_LEVEL_HANDSHAKE;
  case OSSL_RECORD_PROTECTION_LEVEL_APPLICATION:
    return NGTCP2_ENCRYPTION_LEVEL_1RTT;
  default:
    assert(0);
    abort(); /* if NDEBUG is set */
  }
}

uint32_t ngtcp2_crypto_ossl_from_ngtcp2_encryption_level(
  ngtcp2_encryption_level encryption_level) {
  switch (encryption_level) {
  case NGTCP2_ENCRYPTION_LEVEL_INITIAL:
    return OSSL_RECORD_PROTECTION_LEVEL_NONE;
  case NGTCP2_ENCRYPTION_LEVEL_HANDSHAKE:
    return OSSL_RECORD_PROTECTION_LEVEL_HANDSHAKE;
  case NGTCP2_ENCRYPTION_LEVEL_1RTT:
    return OSSL_RECORD_PROTECTION_LEVEL_APPLICATION;
  case NGTCP2_ENCRYPTION_LEVEL_0RTT:
    return OSSL_RECORD_PROTECTION_LEVEL_EARLY;
  default:
    assert(0);
    abort(); /* if NDEBUG is set */
  }
}

int ngtcp2_crypto_get_path_challenge_data_cb(ngtcp2_conn *conn, uint8_t *data,
                                             void *user_data) {
  (void)conn;
  (void)user_data;

  if (RAND_bytes(data, NGTCP2_PATH_CHALLENGE_DATALEN) != 1) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

int ngtcp2_crypto_random(uint8_t *data, size_t datalen) {
  if (RAND_bytes(data, (int)datalen) != 1) {
    return -1;
  }

  return 0;
}

static int ossl_yield_secret(SSL *ssl, uint32_t ossl_level, int direction,
                             const unsigned char *secret, size_t secretlen,
                             void *arg) {
  ngtcp2_crypto_conn_ref *conn_ref = SSL_get_app_data(ssl);
  ngtcp2_conn *conn;
  ngtcp2_crypto_ossl_ctx *ossl_ctx;
  ngtcp2_encryption_level level =
    ngtcp2_crypto_ossl_from_ossl_encryption_level(ossl_level);
  (void)arg;

  if (!conn_ref) {
    return 1;
  }

  conn = conn_ref->get_conn(conn_ref);
  ossl_ctx = ngtcp2_conn_get_tls_native_handle(conn);

  if (direction) {
    if (ngtcp2_crypto_derive_and_install_tx_key(conn, NULL, NULL, NULL, level,
                                                secret, secretlen) != 0) {
      return 0;
    }

    ossl_ctx->tx_level = level;

    return 1;
  }

  if (ngtcp2_crypto_derive_and_install_rx_key(conn, NULL, NULL, NULL, level,
                                              secret, secretlen) != 0) {
    return 0;
  }

  return 1;
}

static int ossl_crypto_send(SSL *ssl, const unsigned char *buf, size_t buflen,
                            size_t *consumed, void *arg) {
  ngtcp2_crypto_conn_ref *conn_ref = SSL_get_app_data(ssl);
  ngtcp2_conn *conn;
  ngtcp2_crypto_ossl_ctx *ossl_ctx;
  int rv;
  (void)arg;

  if (!conn_ref) {
    return 1;
  }

  conn = conn_ref->get_conn(conn_ref);
  ossl_ctx = ngtcp2_conn_get_tls_native_handle(conn);

  rv = ngtcp2_conn_submit_crypto_data(conn, ossl_ctx->tx_level, buf, buflen);
  if (rv != 0) {
    ngtcp2_conn_set_tls_error(conn, rv);
    return 0;
  }

  *consumed = buflen;

  return 1;
}

static int ossl_crypto_recv_rcd(SSL *ssl, const unsigned char **buf,
                                size_t *bytes_read, void *arg) {
  ngtcp2_crypto_conn_ref *conn_ref = SSL_get_app_data(ssl);
  ngtcp2_conn *conn;
  ngtcp2_crypto_ossl_ctx *ossl_ctx;
  (void)arg;

  if (!conn_ref) {
    *buf = NULL;
    *bytes_read = 0;
    return 1;
  }

  conn = conn_ref->get_conn(conn_ref);
  ossl_ctx = ngtcp2_conn_get_tls_native_handle(conn);

  crypto_ossl_ctx_read_crypto_data(ossl_ctx, buf, bytes_read);

  return 1;
}

static int ossl_crypto_release_rcd(SSL *ssl, size_t released, void *arg) {
  ngtcp2_crypto_conn_ref *conn_ref = SSL_get_app_data(ssl);
  ngtcp2_conn *conn;
  ngtcp2_crypto_ossl_ctx *ossl_ctx;
  (void)arg;

  /* It is sometimes a bit hard or tedious to keep ngtcp2_conn alive
     until SSL_free is called.  Instead, we require application to
     call SSL_set_app_data(ssl, NULL) before SSL_free(ssl) so that
     ngtcp2_conn is never used in this function. */
  if (!conn_ref) {
    return 1;
  }

  conn = conn_ref->get_conn(conn_ref);
  ossl_ctx = ngtcp2_conn_get_tls_native_handle(conn);

  crypto_ossl_ctx_release_crypto_data(ossl_ctx, released);

  return 1;
}

static int ossl_got_transport_params(SSL *ssl, const unsigned char *params,
                                     size_t paramslen, void *arg) {
  ngtcp2_crypto_conn_ref *conn_ref = SSL_get_app_data(ssl);
  ngtcp2_conn *conn;
  int rv;
  (void)arg;

  if (!conn_ref) {
    return 1;
  }

  conn = conn_ref->get_conn(conn_ref);

  rv =
    ngtcp2_conn_decode_and_set_remote_transport_params(conn, params, paramslen);
  if (rv != 0) {
    ngtcp2_conn_set_tls_error(conn, rv);
    return 0;
  }

  return 1;
}

static int ossl_alert(SSL *ssl, uint8_t alert_code, void *arg) {
  ngtcp2_crypto_conn_ref *conn_ref = SSL_get_app_data(ssl);
  ngtcp2_conn *conn;
  (void)arg;

  if (!conn_ref) {
    return 1;
  }

  conn = conn_ref->get_conn(conn_ref);

  ngtcp2_conn_set_tls_alert(conn, alert_code);

  return 1;
}

static const OSSL_DISPATCH qtdis[] = {
  {
    OSSL_FUNC_SSL_QUIC_TLS_CRYPTO_SEND,
    (void (*)(void))ossl_crypto_send,
  },
  {
    OSSL_FUNC_SSL_QUIC_TLS_CRYPTO_RECV_RCD,
    (void (*)(void))ossl_crypto_recv_rcd,
  },
  {
    OSSL_FUNC_SSL_QUIC_TLS_CRYPTO_RELEASE_RCD,
    (void (*)(void))ossl_crypto_release_rcd,
  },
  {
    OSSL_FUNC_SSL_QUIC_TLS_YIELD_SECRET,
    (void (*)(void))ossl_yield_secret,
  },
  {
    OSSL_FUNC_SSL_QUIC_TLS_GOT_TRANSPORT_PARAMS,
    (void (*)(void))ossl_got_transport_params,
  },
  {
    OSSL_FUNC_SSL_QUIC_TLS_ALERT,
    (void (*)(void))ossl_alert,
  },
  OSSL_DISPATCH_END,
};

static int crypto_ossl_configure_session(SSL *ssl) {
  if (!SSL_set_quic_tls_cbs(ssl, qtdis, NULL)) {
    return -1;
  }

  return 0;
}

int ngtcp2_crypto_ossl_configure_server_session(SSL *ssl) {
  return crypto_ossl_configure_session(ssl);
}

int ngtcp2_crypto_ossl_configure_client_session(SSL *ssl) {
  return crypto_ossl_configure_session(ssl);
}
