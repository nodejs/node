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
#ifndef NGTCP2_SHARED_H
#define NGTCP2_SHARED_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <ngtcp2/ngtcp2_crypto.h>

/**
 * @macro
 *
 * :macro:`NGTCP2_INITIAL_SALT_DRAFT` is a salt value which is used to
 * derive initial secret.  It is used for QUIC draft versions.
 */
#define NGTCP2_INITIAL_SALT_DRAFT                                              \
  "\xaf\xbf\xec\x28\x99\x93\xd2\x4c\x9e\x97\x86\xf1\x9c\x61\x11\xe0\x43\x90"   \
  "\xa8\x99"

/**
 * @macro
 *
 * :macro:`NGTCP2_INITIAL_SALT_V1` is a salt value which is used to
 * derive initial secret.  It is used for QUIC v1.
 */
#define NGTCP2_INITIAL_SALT_V1                                                 \
  "\x38\x76\x2c\xf7\xf5\x59\x34\xb3\x4d\x17\x9a\xe6\xa4\xc8\x0c\xad\xcc\xbb"   \
  "\x7f\x0a"

/* Maximum key usage (encryption) limits */
#define NGTCP2_CRYPTO_MAX_ENCRYPTION_AES_GCM (1ULL << 23)
#define NGTCP2_CRYPTO_MAX_ENCRYPTION_CHACHA20_POLY1305 (1ULL << 62)
#define NGTCP2_CRYPTO_MAX_ENCRYPTION_AES_CCM (2965820ULL)

/* Maximum authentication failure (decryption) limits during the
   lifetime of a connection. */
#define NGTCP2_CRYPTO_MAX_DECRYPTION_FAILURE_AES_GCM (1ULL << 52)
#define NGTCP2_CRYPTO_MAX_DECRYPTION_FAILURE_CHACHA20_POLY1305 (1ULL << 36)
#define NGTCP2_CRYPTO_MAX_DECRYPTION_FAILURE_AES_CCM (2965820ULL)

/**
 * @function
 *
 * `ngtcp2_crypto_derive_initial_secrets` derives initial secrets.
 * |rx_secret| and |tx_secret| must point to the buffer of at least 32
 * bytes capacity.  rx for read and tx for write.  This function
 * writes rx and tx secrets into |rx_secret| and |tx_secret|
 * respectively.  The length of secret is 32 bytes long.
 * |client_dcid| is the destination connection ID in first Initial
 * packet of client.  If |initial_secret| is not NULL, the initial
 * secret is written to it.  It must point to the buffer which has at
 * least 32 bytes capacity.  The initial secret is 32 bytes long.
 * |side| specifies the side of application.
 *
 * This function returns 0 if it succeeds, or -1.
 */
int ngtcp2_crypto_derive_initial_secrets(uint32_t version, uint8_t *rx_secret,
                                         uint8_t *tx_secret,
                                         uint8_t *initial_secret,
                                         const ngtcp2_cid *client_dcid,
                                         ngtcp2_crypto_side side);

/**
 * @function
 *
 * `ngtcp2_crypto_update_traffic_secret` derives the next generation
 * of the traffic secret.  |secret| specifies the current secret and
 * its length is given in |secretlen|.  The length of new key is the
 * same as the current key.  This function writes new key into the
 * buffer pointed by |dest|.  |dest| must have the enough capacity to
 * store the new key.
 *
 * This function returns 0 if it succeeds, or -1.
 */
int ngtcp2_crypto_update_traffic_secret(uint8_t *dest,
                                        const ngtcp2_crypto_md *md,
                                        const uint8_t *secret,
                                        size_t secretlen);

/**
 * @function
 *
 * `ngtcp2_crypto_set_local_transport_params` sets QUIC transport
 * parameter, which is encoded in wire format and stored in the buffer
 * pointed by |buf| of length |len|, to the native handle |tls|.
 *
 * |tls| points to a implementation dependent TLS session object.  If
 * libngtcp2_crypto_openssl is linked, |tls| must be a pointer to SSL
 * object.
 *
 * This function returns 0 if it succeeds, or -1.
 */
int ngtcp2_crypto_set_local_transport_params(void *tls, const uint8_t *buf,
                                             size_t len);

/**
 * @function
 *
 * `ngtcp2_crypto_set_remote_transport_params` retrieves a remote QUIC
 * transport parameters from |tls| and sets it to |conn| using
 * `ngtcp2_conn_set_remote_transport_params`.
 *
 * |tls| points to a implementation dependent TLS session object.  If
 * libngtcp2_crypto_openssl is linked, |tls| must be a pointer to SSL
 * object.
 *
 * This function returns 0 if it succeeds, or -1.
 */
int ngtcp2_crypto_set_remote_transport_params(ngtcp2_conn *conn, void *tls);

/**
 * @function
 *
 * `ngtcp2_crypto_derive_and_install_initial_key` derives initial
 * keying materials and installs keys to |conn|.
 *
 * If |rx_secret| is not NULL, the secret for decryption is written to
 * the buffer pointed by |rx_secret|.  The length of secret is 32
 * bytes, and |rx_secret| must point to the buffer which has enough
 * capacity.
 *
 * If |tx_secret| is not NULL, the secret for encryption is written to
 * the buffer pointed by |tx_secret|.  The length of secret is 32
 * bytes, and |tx_secret| must point to the buffer which has enough
 * capacity.
 *
 * If |initial_secret| is not NULL, the initial secret is written to
 * the buffer pointed by |initial_secret|.  The length of secret is 32
 * bytes, and |initial_secret| must point to the buffer which has
 * enough capacity.
 *
 * |client_dcid| is the destination connection ID in first Initial
 * packet of client.
 *
 * If |rx_key| is not NULL, the derived packet protection key for
 * decryption is written to the buffer pointed by |rx_key|.  If
 * |rx_iv| is not NULL, the derived packet protection IV for
 * decryption is written to the buffer pointed by |rx_iv|.  If |rx_hp|
 * is not NULL, the derived header protection key for decryption is
 * written to the buffer pointed by |rx_hp|.
 *
 * If |tx_key| is not NULL, the derived packet protection key for
 * encryption is written to the buffer pointed by |tx_key|.  If
 * |tx_iv| is not NULL, the derived packet protection IV for
 * encryption is written to the buffer pointed by |tx_iv|.  If |tx_hp|
 * is not NULL, the derived header protection key for encryption is
 * written to the buffer pointed by |tx_hp|.
 *
 * The length of packet protection key and header protection key is 16
 * bytes long.  The length of packet protection IV is 12 bytes long.
 *
 * This function calls `ngtcp2_conn_set_initial_crypto_ctx` to set
 * initial AEAD and message digest algorithm.  After the successful
 * call of this function, application can use
 * `ngtcp2_conn_get_initial_crypto_ctx` to get the object.
 *
 * This function returns 0 if it succeeds, or -1.
 */
int ngtcp2_crypto_derive_and_install_initial_key(
    ngtcp2_conn *conn, uint8_t *rx_secret, uint8_t *tx_secret,
    uint8_t *initial_secret, uint8_t *rx_key, uint8_t *rx_iv, uint8_t *rx_hp,
    uint8_t *tx_key, uint8_t *tx_iv, uint8_t *tx_hp,
    const ngtcp2_cid *client_dcid);

/**
 * @function
 *
 * `ngtcp2_crypto_cipher_ctx_encrypt_init` initializes |cipher_ctx|
 * with new cipher context object for encryption which is constructed
 * to use |key| as encryption key.  |cipher| specifies cipher to use.
 *
 * This function returns 0 if it succeeds, or -1.
 */
int ngtcp2_crypto_cipher_ctx_encrypt_init(ngtcp2_crypto_cipher_ctx *cipher_ctx,
                                          const ngtcp2_crypto_cipher *cipher,
                                          const uint8_t *key);

/**
 * @function
 *
 * `ngtcp2_crypto_cipher_ctx_free` frees up resources used by
 * |cipher_ctx|.  This function does not free the memory pointed by
 * |cipher_ctx| itself.
 */
void ngtcp2_crypto_cipher_ctx_free(ngtcp2_crypto_cipher_ctx *cipher_ctx);

#endif /* NGTCP2_SHARED_H */
