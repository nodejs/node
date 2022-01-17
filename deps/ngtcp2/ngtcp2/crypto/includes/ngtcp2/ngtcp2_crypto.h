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
#ifndef NGTCP2_CRYPTO_H
#define NGTCP2_CRYPTO_H

#include <ngtcp2/ngtcp2.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @macro
 *
 * :macro:`NGTCP2_CRYPTO_INITIAL_SECRETLEN` is the length of secret
 * for Initial packets.
 */
#define NGTCP2_CRYPTO_INITIAL_SECRETLEN 32

/**
 * @macro
 *
 * :macro:`NGTCP2_CRYPTO_INITIAL_KEYLEN` is the length of key for
 * Initial packets.
 */
#define NGTCP2_CRYPTO_INITIAL_KEYLEN 16

/**
 * @macro
 *
 * :macro:`NGTCP2_CRYPTO_INITIAL_IVLEN` is the length of IV for
 * Initial packets.
 */
#define NGTCP2_CRYPTO_INITIAL_IVLEN 12

/**
 * @function
 *
 * `ngtcp2_crypto_ctx_initial` initializes |ctx| for Initial packet
 * encryption and decryption.
 */
NGTCP2_EXTERN ngtcp2_crypto_ctx *
ngtcp2_crypto_ctx_initial(ngtcp2_crypto_ctx *ctx);

/**
 * @function
 *
 * `ngtcp2_crypto_ctx_tls` initializes |ctx| by extracting negotiated
 * ciphers and message digests from native TLS session
 * |tls_native_handle|.  This is used for encrypting/decrypting
 * Handshake and Short packets.
 *
 * If libngtcp2_crypto_openssl is linked, |tls_native_handle| must be
 * a pointer to SSL object.
 */
NGTCP2_EXTERN ngtcp2_crypto_ctx *ngtcp2_crypto_ctx_tls(ngtcp2_crypto_ctx *ctx,
                                                       void *tls_native_handle);

/**
 * @function
 *
 * `ngtcp2_crypto_ctx_tls_early` initializes |ctx| by extracting early
 * ciphers and message digests from native TLS session
 * |tls_native_handle|.  This is used for encrypting/decrypting 0RTT
 * packets.
 *
 * If libngtcp2_crypto_openssl is linked, |tls_native_handle| must be
 * a pointer to SSL object.
 */
NGTCP2_EXTERN ngtcp2_crypto_ctx *
ngtcp2_crypto_ctx_tls_early(ngtcp2_crypto_ctx *ctx, void *tls_native_handle);

/**
 * @function
 *
 * `ngtcp2_crypto_aead_init` initializes |aead| with the provided
 * |aead_native_handle| which is an underlying AEAD object.
 *
 * If libngtcp2_crypto_openssl is linked, |aead_native_handle| must be
 * a pointer to EVP_CIPHER.
 *
 * If libngtcp2_crypto_gnutls is linked, |aead_native_handle| must be
 * gnutls_cipher_algorithm_t casted to ``void *``.
 *
 * If libngtcp2_crypto_boringssl is linked, |aead_native_handle| must
 * be a pointer to EVP_AEAD.
 */
NGTCP2_EXTERN ngtcp2_crypto_aead *
ngtcp2_crypto_aead_init(ngtcp2_crypto_aead *aead, void *aead_native_handle);

/**
 * @function
 *
 * `ngtcp2_crypto_aead_retry` initializes |aead| with the AEAD cipher
 * AEAD_AES_128_GCM for Retry packet integrity protection.
 */
NGTCP2_EXTERN ngtcp2_crypto_aead *
ngtcp2_crypto_aead_retry(ngtcp2_crypto_aead *aead);

/**
 * @function
 *
 * `ngtcp2_crypto_md_init` initializes |md| with the provided
 * |md_native_handle| which is an underlying message digest object.
 *
 * If libngtcp2_crypto_openssl is linked, |md_native_handle| must be a
 * pointer to EVP_MD.
 *
 * If libngtcp2_crypto_gnutls is linked, |md_native_handle| must be
 * gnutls_mac_algorithm_t casted to ``void *``.
 *
 * If libngtcp2_crypto_boringssl is linked, |md_native_handle| must be
 * a pointer to EVP_MD.
 */
NGTCP2_EXTERN ngtcp2_crypto_md *ngtcp2_crypto_md_init(ngtcp2_crypto_md *md,
                                                      void *md_native_handle);

/**
 * @function
 *
 * `ngtcp2_crypto_md_hashlen` returns the length of |md| output.
 */
NGTCP2_EXTERN size_t ngtcp2_crypto_md_hashlen(const ngtcp2_crypto_md *md);

/**
 * @function
 *
 * `ngtcp2_crypto_aead_keylen` returns the length of key for |aead|.
 */
NGTCP2_EXTERN size_t ngtcp2_crypto_aead_keylen(const ngtcp2_crypto_aead *aead);

/**
 * @function
 *
 * `ngtcp2_crypto_aead_noncelen` returns the length of nonce for
 * |aead|.
 */
NGTCP2_EXTERN size_t
ngtcp2_crypto_aead_noncelen(const ngtcp2_crypto_aead *aead);

/**
 * @function
 *
 * `ngtcp2_crypto_hkdf_extract` performs HKDF extract operation.  The
 * result is the length of |md| and is stored to the buffer pointed by
 * |dest|.  The caller is responsible to specify the buffer that can
 * store the output.
 *
 * This function returns 0 if it succeeds, or -1.
 */
NGTCP2_EXTERN int
ngtcp2_crypto_hkdf_extract(uint8_t *dest, const ngtcp2_crypto_md *md,
                           const uint8_t *secret, size_t secretlen,
                           const uint8_t *salt, size_t saltlen);

/**
 * @function
 *
 * `ngtcp2_crypto_hkdf_expand` performs HKDF expand operation.  The
 * result is |destlen| bytes long and is stored to the buffer pointed
 * by |dest|.
 *
 * This function returns 0 if it succeeds, or -1.
 */
NGTCP2_EXTERN int ngtcp2_crypto_hkdf_expand(uint8_t *dest, size_t destlen,
                                            const ngtcp2_crypto_md *md,
                                            const uint8_t *secret,
                                            size_t secretlen,
                                            const uint8_t *info,
                                            size_t infolen);

/**
 * @function
 *
 * `ngtcp2_crypto_hkdf_expand_label` performs HKDF expand label.  The
 * result is |destlen| bytes long and is stored to the buffer pointed
 * by |dest|.
 *
 * This function returns 0 if it succeeds, or -1.
 */
NGTCP2_EXTERN int ngtcp2_crypto_hkdf_expand_label(uint8_t *dest, size_t destlen,
                                                  const ngtcp2_crypto_md *md,
                                                  const uint8_t *secret,
                                                  size_t secretlen,
                                                  const uint8_t *label,
                                                  size_t labellen);

/**
 * @enum
 *
 * :type:`ngtcp2_crypto_side` indicates which side the application
 * implements; client or server.
 */
typedef enum ngtcp2_crypto_side {
  /**
   * :enum:`NGTCP2_CRYPTO_SIDE_CLIENT` indicates that the application
   * is client.
   */
  NGTCP2_CRYPTO_SIDE_CLIENT,
  /**
   * :enum:`NGTCP2_CRYPTO_SIDE_SERVER` indicates that the application
   * is server.
   */
  NGTCP2_CRYPTO_SIDE_SERVER
} ngtcp2_crypto_side;

/**
 * @function
 *
 * `ngtcp2_crypto_packet_protection_ivlen` returns the length of IV
 * used to encrypt QUIC packet.
 */
NGTCP2_EXTERN size_t
ngtcp2_crypto_packet_protection_ivlen(const ngtcp2_crypto_aead *aead);

/**
 * @function
 *
 * `ngtcp2_crypto_derive_packet_protection_key` derives packet
 * protection key.  This function writes packet protection key into
 * the buffer pointed by |key|.  |key| must point to the buffer which
 * is at least ngtcp2_crypto_aead_keylen(aead) bytes long.  This
 * function writes packet protection IV into |iv|.  |iv| must point to
 * the buffer which is at least
 * ngtcp2_crypto_packet_protection_ivlen(aead).  |key| is
 * ngtcp2_crypto_aead_keylen(aead) bytes long.  |iv| is
 * ngtcp2_crypto_packet_protection_ivlen(aead) bytes long.
 *
 * If |hp| is not NULL, this function also derives packet header
 * protection key and writes the key into the buffer pointed by |hp|.
 * The length of key is ngtcp2_crypto_aead_keylen(aead) bytes long.
 * |hp|, if not NULL, must have enough capacity to store the key.
 *
 * This function returns 0 if it succeeds, or -1.
 */
NGTCP2_EXTERN int ngtcp2_crypto_derive_packet_protection_key(
    uint8_t *key, uint8_t *iv, uint8_t *hp, const ngtcp2_crypto_aead *aead,
    const ngtcp2_crypto_md *md, const uint8_t *secret, size_t secretlen);

/**
 * @function
 *
 * `ngtcp2_crypto_encrypt` encrypts |plaintext| of length
 * |plaintextlen| and writes the ciphertext into the buffer pointed by
 * |dest|.  The length of ciphertext is plaintextlen +
 * ngtcp2_crypto_aead_max_overhead(aead) bytes long.  |dest| must have
 * enough capacity to store the ciphertext.  It is allowed to specify
 * the same value to |dest| and |plaintext|.
 *
 * This function returns 0 if it succeeds, or -1.
 */
NGTCP2_EXTERN int ngtcp2_crypto_encrypt(uint8_t *dest,
                                        const ngtcp2_crypto_aead *aead,
                                        const ngtcp2_crypto_aead_ctx *aead_ctx,
                                        const uint8_t *plaintext,
                                        size_t plaintextlen,
                                        const uint8_t *nonce, size_t noncelen,
                                        const uint8_t *ad, size_t adlen);

/**
 * @function
 *
 * `ngtcp2_crypto_encrypt_cb` is a wrapper function around
 * `ngtcp2_crypto_encrypt`.  It can be directly passed to
 * :member:`ngtcp2_conn_callbacks.encrypt` field.
 *
 * This function returns 0 if it succeeds, or
 * :macro:`NGTCP2_ERR_CALLBACK_FAILURE`.
 */
NGTCP2_EXTERN int
ngtcp2_crypto_encrypt_cb(uint8_t *dest, const ngtcp2_crypto_aead *aead,
                         const ngtcp2_crypto_aead_ctx *aead_ctx,
                         const uint8_t *plaintext, size_t plaintextlen,
                         const uint8_t *nonce, size_t noncelen,
                         const uint8_t *ad, size_t adlen);

/**
 * @function
 *
 * `ngtcp2_crypto_decrypt` decrypts |ciphertext| of length
 * |ciphertextlen| and writes the plaintext into the buffer pointed by
 * |dest|.  The length of plaintext is ciphertextlen -
 * ngtcp2_crypto_aead_max_overhead(aead) bytes long.  |dest| must have
 * enough capacity to store the plaintext.  It is allowed to specify
 * the same value to |dest| and |ciphertext|.
 *
 * This function returns 0 if it succeeds, or -1.
 */
NGTCP2_EXTERN int ngtcp2_crypto_decrypt(uint8_t *dest,
                                        const ngtcp2_crypto_aead *aead,
                                        const ngtcp2_crypto_aead_ctx *aead_ctx,
                                        const uint8_t *ciphertext,
                                        size_t ciphertextlen,
                                        const uint8_t *nonce, size_t noncelen,
                                        const uint8_t *ad, size_t adlen);

/**
 * @function
 *
 * `ngtcp2_crypto_decrypt_cb` is a wrapper function around
 * `ngtcp2_crypto_decrypt`.  It can be directly passed to
 * :member:`ngtcp2_conn_callbacks.decrypt` field.
 *
 * This function returns 0 if it succeeds, or
 * :macro:`NGTCP2_ERR_TLS_DECRYPT`.
 */
NGTCP2_EXTERN int
ngtcp2_crypto_decrypt_cb(uint8_t *dest, const ngtcp2_crypto_aead *aead,
                         const ngtcp2_crypto_aead_ctx *aead_ctx,
                         const uint8_t *ciphertext, size_t ciphertextlen,
                         const uint8_t *nonce, size_t noncelen,
                         const uint8_t *ad, size_t adlen);

/**
 * @function
 *
 * `ngtcp2_crypto_hp_mask` generates mask which is used in packet
 * header encryption.  The mask is written to the buffer pointed by
 * |dest|.  The length of mask is 5 bytes.  |dest| must have enough
 * capacity to store the mask.
 *
 * This function returns 0 if it succeeds, or -1.
 */
NGTCP2_EXTERN int ngtcp2_crypto_hp_mask(uint8_t *dest,
                                        const ngtcp2_crypto_cipher *hp,
                                        const ngtcp2_crypto_cipher_ctx *hp_ctx,
                                        const uint8_t *sample);

/**
 * @function
 *
 * `ngtcp2_crypto_hp_mask_cb` is a wrapper function around
 * `ngtcp2_crypto_hp_mask`.  It can be directly passed to
 * :member:`ngtcp2_conn_callbacks.hp_mask` field.
 *
 * This function returns 0 if it succeeds, or
 * :macro:`NGTCP2_ERR_CALLBACK_FAILURE`.
 */
NGTCP2_EXTERN int
ngtcp2_crypto_hp_mask_cb(uint8_t *dest, const ngtcp2_crypto_cipher *hp,
                         const ngtcp2_crypto_cipher_ctx *hp_ctx,
                         const uint8_t *sample);

/**
 * @function
 *
 * `ngtcp2_crypto_derive_and_install_rx_key` derives the rx keys from
 * |secret| and installs new keys to |conn|.
 *
 * If |key| is not NULL, the derived packet protection key for
 * decryption is written to the buffer pointed by |key|.  If |iv| is
 * not NULL, the derived packet protection IV for decryption is
 * written to the buffer pointed by |iv|.  If |hp| is not NULL, the
 * derived header protection key for decryption is written to the
 * buffer pointed by |hp|.
 *
 * |secretlen| specifies the length of |secret|.
 *
 * The length of packet protection key and header protection key is
 * ngtcp2_crypto_aead(ctx->aead), and the length of packet protection
 * IV is ngtcp2_crypto_packet_protection_ivlen(ctx->aead) where ctx
 * can be obtained by `ngtcp2_crypto_ctx_tls`.
 *
 * In the first call of this function, it calls
 * `ngtcp2_conn_set_crypto_ctx` to set negotiated AEAD and message
 * digest algorithm.  After the successful call of this function,
 * application can use `ngtcp2_conn_get_crypto_ctx` to get the object.
 * It also calls `ngtcp2_conn_set_aead_overhead` to set AEAD tag
 * length.
 *
 * This function returns 0 if it succeeds, or -1.
 */
NGTCP2_EXTERN int ngtcp2_crypto_derive_and_install_rx_key(
    ngtcp2_conn *conn, uint8_t *key, uint8_t *iv, uint8_t *hp,
    ngtcp2_crypto_level level, const uint8_t *secret, size_t secretlen);

/**
 * @function
 *
 * `ngtcp2_crypto_derive_and_install_tx_key` derives the tx keys from
 * |secret| and installs new keys to |conn|.
 *
 * If |key| is not NULL, the derived packet protection key for
 * encryption is written to the buffer pointed by |key|.  If |iv| is
 * not NULL, the derived packet protection IV for encryption is
 * written to the buffer pointed by |iv|.  If |hp| is not NULL, the
 * derived header protection key for encryption is written to the
 * buffer pointed by |hp|.
 *
 * |secretlen| specifies the length of |secret|.
 *
 * The length of packet protection key and header protection key is
 * ngtcp2_crypto_aead(ctx->aead), and the length of packet protection
 * IV is ngtcp2_crypto_packet_protection_ivlen(ctx->aead) where ctx
 * can be obtained by `ngtcp2_crypto_ctx_tls`.
 *
 * In the first call of this function, it calls
 * `ngtcp2_conn_set_crypto_ctx` to set negotiated AEAD and message
 * digest algorithm.  After the successful call of this function,
 * application can use `ngtcp2_conn_get_crypto_ctx` to get the object.
 * It also calls `ngtcp2_conn_set_aead_overhead` to set AEAD tag
 * length.
 *
 * If |level| is NGTCP2_CRYPTO_LEVEL_APP, this function retrieves a
 * remote QUIC transport parameters extension from |tls| and sets it
 * to |conn|.
 *
 * This function returns 0 if it succeeds, or -1.
 */
NGTCP2_EXTERN int ngtcp2_crypto_derive_and_install_tx_key(
    ngtcp2_conn *conn, uint8_t *key, uint8_t *iv, uint8_t *hp,
    ngtcp2_crypto_level level, const uint8_t *secret, size_t secretlen);

/**
 * @function
 *
 * `ngtcp2_crypto_update_key` updates traffic keying materials.
 *
 * The new traffic secret for decryption is written to the buffer
 * pointed by |rx_secret|.  The length of secret is |secretlen| bytes,
 * and |rx_secret| must point to the buffer which has enough capacity.
 *
 * The new traffic secret for encryption is written to the buffer
 * pointed by |tx_secret|.  The length of secret is |secretlen| bytes,
 * and |tx_secret| must point to the buffer which has enough capacity.
 *
 * The derived packet protection key for decryption is written to the
 * buffer pointed by |rx_key|.  The derived packet protection IV for
 * decryption is written to the buffer pointed by |rx_iv|.
 * |rx_aead_ctx| must be constructed with |rx_key|.
 *
 * The derived packet protection key for encryption is written to the
 * buffer pointed by |tx_key|.  The derived packet protection IV for
 * encryption is written to the buffer pointed by |tx_iv|.
 * |tx_aead_ctx| must be constructed with |rx_key|.
 *
 * |current_rx_secret| and |current_tx_secret| are the current traffic
 * secrets for decryption and encryption.  |secretlen| specifies the
 * length of |rx_secret| and |tx_secret|.
 *
 * The length of packet protection key and header protection key is
 * ngtcp2_crypto_aead(ctx->aead), and the length of packet protection
 * IV is ngtcp2_crypto_packet_protection_ivlen(ctx->aead) where ctx
 * can be obtained by `ngtcp2_conn_get_crypto_ctx`.
 *
 * This function returns 0 if it succeeds, or -1.
 */
NGTCP2_EXTERN int ngtcp2_crypto_update_key(
    ngtcp2_conn *conn, uint8_t *rx_secret, uint8_t *tx_secret,
    ngtcp2_crypto_aead_ctx *rx_aead_ctx, uint8_t *rx_key, uint8_t *rx_iv,
    ngtcp2_crypto_aead_ctx *tx_aead_ctx, uint8_t *tx_key, uint8_t *tx_iv,
    const uint8_t *current_rx_secret, const uint8_t *current_tx_secret,
    size_t secretlen);

/**
 * @function
 *
 * `ngtcp2_crypto_update_key_cb` is a wrapper function around
 * `ngtcp2_crypto_update_key`.  It can be directly passed to
 * :member:`ngtcp2_conn_callbacks.update_key` field.
 *
 * This function returns 0 if it succeeds, or
 * :macro:`NGTCP2_ERR_CALLBACK_FAILURE`.
 */
NGTCP2_EXTERN int ngtcp2_crypto_update_key_cb(
    ngtcp2_conn *conn, uint8_t *rx_secret, uint8_t *tx_secret,
    ngtcp2_crypto_aead_ctx *rx_aead_ctx, uint8_t *rx_iv,
    ngtcp2_crypto_aead_ctx *tx_aead_ctx, uint8_t *tx_iv,
    const uint8_t *current_rx_secret, const uint8_t *current_tx_secret,
    size_t secretlen, void *user_data);

/**
 * @function
 *
 * `ngtcp2_crypto_client_initial_cb` installs initial secrets and
 * encryption keys and sets QUIC transport parameters.
 *
 * This function can be directly passed to
 * :member:`ngtcp2_conn_callbacks.client_initial` field.  It is only
 * used by client.
 *
 * This function returns 0 if it succeeds, or
 * :macro:`NGTCP2_ERR_CALLBACK_FAILURE`.
 */
NGTCP2_EXTERN int ngtcp2_crypto_client_initial_cb(ngtcp2_conn *conn,
                                                  void *user_data);

/**
 * @function
 *
 * `ngtcp2_crypto_recv_retry_cb` re-installs initial secrets in
 * response to incoming Retry packet.
 *
 * This function can be directly passed to
 * :member:`ngtcp2_conn_callbacks.recv_retry` field.  It is only used
 * by client.
 *
 * This function returns 0 if it succeeds, or
 * :macro:`NGTCP2_ERR_CALLBACK_FAILURE`.
 */
NGTCP2_EXTERN int ngtcp2_crypto_recv_retry_cb(ngtcp2_conn *conn,
                                              const ngtcp2_pkt_hd *hd,
                                              void *user_data);

/**
 * @function
 *
 * `ngtcp2_crypto_recv_client_initial_cb` installs initial secrets in
 * response to an incoming Initial packet from client, and sets QUIC
 * transport parameters.
 *
 * This function can be directly passed to
 * :member:`ngtcp2_conn_callbacks.recv_client_initial` field.  It is
 * only used by server.
 *
 * This function returns 0 if it succeeds, or
 * :macro:`NGTCP2_ERR_CALLBACK_FAILURE`.
 */
NGTCP2_EXTERN int ngtcp2_crypto_recv_client_initial_cb(ngtcp2_conn *conn,
                                                       const ngtcp2_cid *dcid,
                                                       void *user_data);

/**
 * @function
 *
 * `ngtcp2_crypto_read_write_crypto_data` reads CRYPTO data |data| of
 * length |datalen| in encryption level |crypto_level| and may feed
 * outgoing CRYPTO data to |conn|.  This function can drive handshake.
 * This function can be also used after handshake completes.  It is
 * allowed to call this function with datalen == 0.  In this case, no
 * additional read operation is done.
 *
 * This function returns 0 if it succeeds, or a negative error code.
 * The generic error code is -1 if a specific error code is not
 * suitable.  The error codes less than -10000 are specific to
 * underlying TLS implementation.  For OpenSSL, the error codes are
 * defined in *ngtcp2_crypto_openssl.h*.
 */
NGTCP2_EXTERN int
ngtcp2_crypto_read_write_crypto_data(ngtcp2_conn *conn,
                                     ngtcp2_crypto_level crypto_level,
                                     const uint8_t *data, size_t datalen);

/**
 * @function
 *
 *  `ngtcp2_crypto_generate_stateless_reset_token` generates a
 *  stateless reset token using HKDF extraction with |md| using the
 *  given |cid| and static key |secret| as input.  The token will be
 *  written to the buffer pointed by |token| and it must have a
 *  capacity of at least :macro:`NGTCP2_STATELESS_RESET_TOKENLEN`
 *  bytes.
 *
 * This function returns 0 if it succeeds, or -1.
 */
NGTCP2_EXTERN int ngtcp2_crypto_generate_stateless_reset_token(
    uint8_t *token, const ngtcp2_crypto_md *md, const uint8_t *secret,
    size_t secretlen, const ngtcp2_cid *cid);

/**
 * @function
 *
 * `ngtcp2_crypto_write_connection_close` writes Initial packet
 * containing CONNECTION_CLOSE with the given |error_code| to the
 * buffer pointed by |dest| of length |destlen|.  This function is
 * designed for server to close connection without committing the
 * state when validating Retry token fails.  This function must not be
 * used by client.  The |dcid| must be the Source Connection ID in
 * Initial packet from client.  The |scid| must be the Destination
 * Connection ID in Initial packet from client.  |scid| is used to
 * derive initial keying materials.
 *
 * This function wraps around `ngtcp2_pkt_write_connection_close` for
 * easier use.
 *
 * This function returns 0 if it succeeds, or -1.
 */
NGTCP2_EXTERN ngtcp2_ssize ngtcp2_crypto_write_connection_close(
    uint8_t *dest, size_t destlen, uint32_t version, const ngtcp2_cid *dcid,
    const ngtcp2_cid *scid, uint64_t error_code);

/**
 * @function
 *
 * `ngtcp2_crypto_write_retry` writes Retry packet to the buffer
 * pointed by |dest| of length |destlen|.  |odcid| specifies Original
 * Destination Connection ID.  |token| specifies Retry Token, and
 * |tokenlen| specifies its length.
 *
 * This function wraps around `ngtcp2_pkt_write_retry` for easier use.
 *
 * This function returns 0 if it succeeds, or -1.
 */
NGTCP2_EXTERN ngtcp2_ssize ngtcp2_crypto_write_retry(
    uint8_t *dest, size_t destlen, uint32_t version, const ngtcp2_cid *dcid,
    const ngtcp2_cid *scid, const ngtcp2_cid *odcid, const uint8_t *token,
    size_t tokenlen);

/**
 * @function
 *
 * `ngtcp2_crypto_aead_ctx_encrypt_init` initializes |aead_ctx| with
 * new AEAD cipher context object for encryption which is constructed
 * to use |key| as encryption key.  |aead| specifies AEAD cipher to
 * use.  |noncelen| is the length of nonce.
 *
 * This function returns 0 if it succeeds, or -1.
 */
NGTCP2_EXTERN int
ngtcp2_crypto_aead_ctx_encrypt_init(ngtcp2_crypto_aead_ctx *aead_ctx,
                                    const ngtcp2_crypto_aead *aead,
                                    const uint8_t *key, size_t noncelen);

/**
 * @function
 *
 * `ngtcp2_crypto_aead_ctx_decrypt_init` initializes |aead_ctx| with
 * new AEAD cipher context object for decryption which is constructed
 * to use |key| as encryption key.  |aead| specifies AEAD cipher to
 * use.  |noncelen| is the length of nonce.
 *
 * This function returns 0 if it succeeds, or -1.
 */
NGTCP2_EXTERN int
ngtcp2_crypto_aead_ctx_decrypt_init(ngtcp2_crypto_aead_ctx *aead_ctx,
                                    const ngtcp2_crypto_aead *aead,
                                    const uint8_t *key, size_t noncelen);

/**
 * @function
 *
 * `ngtcp2_crypto_aead_ctx_free` frees up resources used by
 * |aead_ctx|.  This function does not free the memory pointed by
 * |aead_ctx| itself.
 */
NGTCP2_EXTERN void
ngtcp2_crypto_aead_ctx_free(ngtcp2_crypto_aead_ctx *aead_ctx);

/**
 * @function
 *
 * `ngtcp2_crypto_delete_crypto_aead_ctx_cb` deletes the given |aead_ctx|.
 *
 * This function can be directly passed to
 * :member:`ngtcp2_conn_callbacks.delete_crypto_aead_ctx` field.
 */
NGTCP2_EXTERN void ngtcp2_crypto_delete_crypto_aead_ctx_cb(
    ngtcp2_conn *conn, ngtcp2_crypto_aead_ctx *aead_ctx, void *user_data);

/**
 * @function
 *
 * `ngtcp2_crypto_delete_crypto_cipher_ctx_cb` deletes the given
 * |cipher_ctx|.
 *
 * This function can be directly passed to
 * :member:`ngtcp2_conn_callbacks.delete_crypto_cipher_ctx` field.
 */
NGTCP2_EXTERN void ngtcp2_crypto_delete_crypto_cipher_ctx_cb(
    ngtcp2_conn *conn, ngtcp2_crypto_cipher_ctx *cipher_ctx, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* NGTCP2_CRYPTO_H */
