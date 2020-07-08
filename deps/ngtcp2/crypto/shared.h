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

/* Maximum key usage (encryption) limits */
#define NGTCP2_CRYPTO_MAX_ENCRYPTION_AES_GCM (23726566ULL)
#define NGTCP2_CRYPTO_MAX_ENCRYPTION_CHACHA20_POLY1305 (1ULL << 62)
#define NGTCP2_CRYPTO_MAX_ENCRYPTION_AES_CCM (1ULL << 23)

/* Maximum authentication failure (decryption) limits */
#define NGTCP2_CRYPTO_MAX_DECRYPTION_FAILURE_AES_GCM (1ULL << 36)
#define NGTCP2_CRYPTO_MAX_DECRYPTION_FAILURE_CHACHA20_POLY1305 (1ULL << 36)
#define NGTCP2_CRYPTO_MAX_DECRYPTION_FAILURE_AES_CCM (11863283ULL)

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
int ngtcp2_crypto_derive_initial_secrets(uint8_t *rx_secret, uint8_t *tx_secret,
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

#endif /* NGTCP2_SHARED_H */
