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
#include "shared.h"

#include <string.h>
#include <assert.h>

#include "ngtcp2_macro.h"

int ngtcp2_crypto_hkdf_expand_label(uint8_t *dest, size_t destlen,
                                    const ngtcp2_crypto_md *md,
                                    const uint8_t *secret, size_t secretlen,
                                    const uint8_t *label, size_t labellen) {
  static const uint8_t LABEL[] = "tls13 ";
  uint8_t info[256];
  uint8_t *p = info;

  *p++ = (uint8_t)(destlen / 256);
  *p++ = (uint8_t)(destlen % 256);
  *p++ = (uint8_t)(sizeof(LABEL) - 1 + labellen);
  memcpy(p, LABEL, sizeof(LABEL) - 1);
  p += sizeof(LABEL) - 1;
  memcpy(p, label, labellen);
  p += labellen;
  *p++ = 0;

  return ngtcp2_crypto_hkdf_expand(dest, destlen, md, secret, secretlen, info,
                                   (size_t)(p - info));
}

#define NGTCP2_CRYPTO_INITIAL_SECRETLEN 32

int ngtcp2_crypto_derive_initial_secrets(uint8_t *rx_secret, uint8_t *tx_secret,
                                         uint8_t *initial_secret,
                                         const ngtcp2_cid *client_dcid,
                                         ngtcp2_crypto_side side) {
  static const uint8_t CLABEL[] = "client in";
  static const uint8_t SLABEL[] = "server in";
  uint8_t initial_secret_buf[NGTCP2_CRYPTO_INITIAL_SECRETLEN];
  uint8_t *client_secret;
  uint8_t *server_secret;
  ngtcp2_crypto_ctx ctx;

  if (!initial_secret) {
    initial_secret = initial_secret_buf;
  }

  ngtcp2_crypto_ctx_initial(&ctx);

  if (ngtcp2_crypto_hkdf_extract(initial_secret, &ctx.md, client_dcid->data,
                                 client_dcid->datalen,
                                 (const uint8_t *)NGTCP2_INITIAL_SALT,
                                 sizeof(NGTCP2_INITIAL_SALT) - 1) != 0) {
    return -1;
  }

  if (side == NGTCP2_CRYPTO_SIDE_SERVER) {
    client_secret = rx_secret;
    server_secret = tx_secret;
  } else {
    client_secret = tx_secret;
    server_secret = rx_secret;
  }

  if (ngtcp2_crypto_hkdf_expand_label(
          client_secret, NGTCP2_CRYPTO_INITIAL_SECRETLEN, &ctx.md,
          initial_secret, NGTCP2_CRYPTO_INITIAL_SECRETLEN, CLABEL,
          sizeof(CLABEL) - 1) != 0 ||
      ngtcp2_crypto_hkdf_expand_label(
          server_secret, NGTCP2_CRYPTO_INITIAL_SECRETLEN, &ctx.md,
          initial_secret, NGTCP2_CRYPTO_INITIAL_SECRETLEN, SLABEL,
          sizeof(SLABEL) - 1) != 0) {
    return -1;
  }

  return 0;
}

size_t ngtcp2_crypto_packet_protection_ivlen(const ngtcp2_crypto_aead *aead) {
  size_t noncelen = ngtcp2_crypto_aead_noncelen(aead);
  return ngtcp2_max(8, noncelen);
}

int ngtcp2_crypto_derive_packet_protection_key(
    uint8_t *key, uint8_t *iv, uint8_t *hp_key, const ngtcp2_crypto_aead *aead,
    const ngtcp2_crypto_md *md, const uint8_t *secret, size_t secretlen) {
  static const uint8_t KEY_LABEL[] = "quic key";
  static const uint8_t IV_LABEL[] = "quic iv";
  static const uint8_t HP_KEY_LABEL[] = "quic hp";
  size_t keylen = ngtcp2_crypto_aead_keylen(aead);
  size_t ivlen = ngtcp2_crypto_packet_protection_ivlen(aead);

  if (ngtcp2_crypto_hkdf_expand_label(key, keylen, md, secret, secretlen,
                                      KEY_LABEL, sizeof(KEY_LABEL) - 1) != 0) {
    return -1;
  }

  if (ngtcp2_crypto_hkdf_expand_label(iv, ivlen, md, secret, secretlen,
                                      IV_LABEL, sizeof(IV_LABEL) - 1) != 0) {
    return -1;
  }

  if (hp_key != NULL && ngtcp2_crypto_hkdf_expand_label(
                            hp_key, keylen, md, secret, secretlen, HP_KEY_LABEL,
                            sizeof(HP_KEY_LABEL) - 1) != 0) {
    return -1;
  }

  return 0;
}

int ngtcp2_crypto_update_traffic_secret(uint8_t *dest,
                                        const ngtcp2_crypto_md *md,
                                        const uint8_t *secret,
                                        size_t secretlen) {
  static const uint8_t LABEL[] = "quic ku";

  if (ngtcp2_crypto_hkdf_expand_label(dest, secretlen, md, secret, secretlen,
                                      LABEL, sizeof(LABEL) - 1) != 0) {
    return -1;
  }

  return 0;
}

int ngtcp2_crypto_derive_and_install_rx_key(ngtcp2_conn *conn, uint8_t *key,
                                            uint8_t *iv, uint8_t *hp_key,
                                            ngtcp2_crypto_level level,
                                            const uint8_t *secret,
                                            size_t secretlen) {
  const ngtcp2_crypto_ctx *ctx;
  const ngtcp2_crypto_aead *aead;
  const ngtcp2_crypto_md *md;
  void *tls = ngtcp2_conn_get_tls_native_handle(conn);
  uint8_t keybuf[64], ivbuf[64], hp_keybuf[64];
  size_t keylen;
  size_t ivlen;
  int rv;

  if (level == NGTCP2_CRYPTO_LEVEL_EARLY && !ngtcp2_conn_is_server(conn)) {
    return 0;
  }

  if (!key) {
    key = keybuf;
  }
  if (!iv) {
    iv = ivbuf;
  }
  if (!hp_key) {
    hp_key = hp_keybuf;
  }

  ctx = ngtcp2_conn_get_crypto_ctx(conn);

  if (!ctx->aead.native_handle) {
    ngtcp2_crypto_ctx cctx;
    ngtcp2_crypto_ctx_tls(&cctx, tls);
    ngtcp2_conn_set_aead_overhead(conn, ngtcp2_crypto_aead_taglen(&cctx.aead));
    ngtcp2_conn_set_crypto_ctx(conn, &cctx);
    ctx = ngtcp2_conn_get_crypto_ctx(conn);
  }

  aead = &ctx->aead;
  md = &ctx->md;
  keylen = ngtcp2_crypto_aead_keylen(aead);
  ivlen = ngtcp2_crypto_packet_protection_ivlen(aead);

  if (ngtcp2_crypto_derive_packet_protection_key(key, iv, hp_key, aead, md,
                                                 secret, secretlen) != 0) {
    return -1;
  }

  switch (level) {
  case NGTCP2_CRYPTO_LEVEL_EARLY:
    rv = ngtcp2_conn_install_early_key(conn, key, iv, hp_key, keylen, ivlen);
    if (rv != 0) {
      return -1;
    }
    break;
  case NGTCP2_CRYPTO_LEVEL_HANDSHAKE:
    rv = ngtcp2_conn_install_rx_handshake_key(conn, key, iv, hp_key, keylen,
                                              ivlen);
    if (rv != 0) {
      return -1;
    }
    break;
  case NGTCP2_CRYPTO_LEVEL_APP:
    if (!ngtcp2_conn_is_server(conn)) {
      rv = ngtcp2_crypto_set_remote_transport_params(conn, tls);
      if (rv != 0) {
        return -1;
      }
    }

    rv = ngtcp2_conn_install_rx_key(conn, secret, key, iv, hp_key, secretlen,
                                    keylen, ivlen);
    if (rv != 0) {
      return -1;
    }

    break;
  default:
    return -1;
  }

  return 0;
}

int ngtcp2_crypto_derive_and_install_tx_key(ngtcp2_conn *conn, uint8_t *key,
                                            uint8_t *iv, uint8_t *hp_key,
                                            ngtcp2_crypto_level level,
                                            const uint8_t *secret,
                                            size_t secretlen) {
  const ngtcp2_crypto_ctx *ctx;
  const ngtcp2_crypto_aead *aead;
  const ngtcp2_crypto_md *md;
  void *tls = ngtcp2_conn_get_tls_native_handle(conn);
  uint8_t keybuf[64], ivbuf[64], hp_keybuf[64];
  size_t keylen;
  size_t ivlen;
  int rv;

  if (level == NGTCP2_CRYPTO_LEVEL_EARLY && ngtcp2_conn_is_server(conn)) {
    return 0;
  }

  if (!key) {
    key = keybuf;
  }
  if (!iv) {
    iv = ivbuf;
  }
  if (!hp_key) {
    hp_key = hp_keybuf;
  }

  ctx = ngtcp2_conn_get_crypto_ctx(conn);

  if (!ctx->aead.native_handle) {
    ngtcp2_crypto_ctx cctx;
    ngtcp2_crypto_ctx_tls(&cctx, tls);
    ngtcp2_conn_set_aead_overhead(conn, ngtcp2_crypto_aead_taglen(&cctx.aead));
    ngtcp2_conn_set_crypto_ctx(conn, &cctx);
    ctx = ngtcp2_conn_get_crypto_ctx(conn);
  }

  aead = &ctx->aead;
  md = &ctx->md;
  keylen = ngtcp2_crypto_aead_keylen(aead);
  ivlen = ngtcp2_crypto_packet_protection_ivlen(aead);

  if (ngtcp2_crypto_derive_packet_protection_key(key, iv, hp_key, aead, md,
                                                 secret, secretlen) != 0) {
    return -1;
  }

  switch (level) {
  case NGTCP2_CRYPTO_LEVEL_EARLY:
    rv = ngtcp2_conn_install_early_key(conn, key, iv, hp_key, keylen, ivlen);
    if (rv != 0) {
      return -1;
    }
    break;
  case NGTCP2_CRYPTO_LEVEL_HANDSHAKE:
    if (ngtcp2_conn_is_server(conn)) {
      rv = ngtcp2_crypto_set_remote_transport_params(conn, tls);
      if (rv != 0) {
        return -1;
      }
    }

    rv = ngtcp2_conn_install_tx_handshake_key(conn, key, iv, hp_key, keylen,
                                              ivlen);
    if (rv != 0) {
      return -1;
    }
    break;
  case NGTCP2_CRYPTO_LEVEL_APP:
    rv = ngtcp2_conn_install_tx_key(conn, secret, key, iv, hp_key, secretlen,
                                    keylen, ivlen);
    if (rv != 0) {
      return -1;
    }

    break;
  default:
    return -1;
  }

  return 0;
}

int ngtcp2_crypto_derive_and_install_initial_key(
    ngtcp2_conn *conn, uint8_t *rx_secret, uint8_t *tx_secret,
    uint8_t *initial_secret, uint8_t *rx_key, uint8_t *rx_iv,
    uint8_t *rx_hp_key, uint8_t *tx_key, uint8_t *tx_iv, uint8_t *tx_hp_key,
    const ngtcp2_cid *client_dcid) {
  uint8_t rx_secretbuf[NGTCP2_CRYPTO_INITIAL_SECRETLEN];
  uint8_t tx_secretbuf[NGTCP2_CRYPTO_INITIAL_SECRETLEN];
  uint8_t initial_secretbuf[NGTCP2_CRYPTO_INITIAL_SECRETLEN];
  uint8_t rx_keybuf[NGTCP2_CRYPTO_INITIAL_KEYLEN];
  uint8_t rx_ivbuf[NGTCP2_CRYPTO_INITIAL_IVLEN];
  uint8_t rx_hp_keybuf[NGTCP2_CRYPTO_INITIAL_KEYLEN];
  uint8_t tx_keybuf[NGTCP2_CRYPTO_INITIAL_KEYLEN];
  uint8_t tx_ivbuf[NGTCP2_CRYPTO_INITIAL_IVLEN];
  uint8_t tx_hp_keybuf[NGTCP2_CRYPTO_INITIAL_KEYLEN];
  ngtcp2_crypto_ctx ctx;
  ngtcp2_crypto_aead retry_aead;
  int rv;

  ngtcp2_crypto_ctx_initial(&ctx);

  if (!rx_secret) {
    rx_secret = rx_secretbuf;
  }
  if (!tx_secret) {
    tx_secret = tx_secretbuf;
  }
  if (!initial_secret) {
    initial_secret = initial_secretbuf;
  }

  if (!rx_key) {
    rx_key = rx_keybuf;
  }
  if (!rx_iv) {
    rx_iv = rx_ivbuf;
  }
  if (!rx_hp_key) {
    rx_hp_key = rx_hp_keybuf;
  }
  if (!tx_key) {
    tx_key = tx_keybuf;
  }
  if (!tx_iv) {
    tx_iv = tx_ivbuf;
  }
  if (!tx_hp_key) {
    tx_hp_key = tx_hp_keybuf;
  }

  ngtcp2_conn_set_initial_crypto_ctx(conn, &ctx);

  if (ngtcp2_crypto_derive_initial_secrets(
          rx_secret, tx_secret, initial_secret, client_dcid,
          ngtcp2_conn_is_server(conn) ? NGTCP2_CRYPTO_SIDE_SERVER
                                      : NGTCP2_CRYPTO_SIDE_CLIENT) != 0) {
    return -1;
  }

  if (ngtcp2_crypto_derive_packet_protection_key(
          rx_key, rx_iv, rx_hp_key, &ctx.aead, &ctx.md, rx_secret,
          NGTCP2_CRYPTO_INITIAL_SECRETLEN) != 0) {
    return -1;
  }

  if (ngtcp2_crypto_derive_packet_protection_key(
          tx_key, tx_iv, tx_hp_key, &ctx.aead, &ctx.md, tx_secret,
          NGTCP2_CRYPTO_INITIAL_SECRETLEN) != 0) {
    return -1;
  }

  rv = ngtcp2_conn_install_initial_key(
      conn, rx_key, rx_iv, rx_hp_key, tx_key, tx_iv, tx_hp_key,
      NGTCP2_CRYPTO_INITIAL_KEYLEN, NGTCP2_CRYPTO_INITIAL_IVLEN);
  if (rv != 0) {
    return -1;
  }

  ngtcp2_conn_set_retry_aead(conn, ngtcp2_crypto_aead_retry(&retry_aead));

  return 0;
}

int ngtcp2_crypto_update_key(ngtcp2_conn *conn, uint8_t *rx_secret,
                             uint8_t *tx_secret, uint8_t *rx_key,
                             uint8_t *rx_iv, uint8_t *tx_key, uint8_t *tx_iv,
                             const uint8_t *current_rx_secret,
                             const uint8_t *current_tx_secret,
                             size_t secretlen) {
  const ngtcp2_crypto_ctx *ctx = ngtcp2_conn_get_crypto_ctx(conn);
  const ngtcp2_crypto_aead *aead = &ctx->aead;
  const ngtcp2_crypto_md *md = &ctx->md;

  if (ngtcp2_crypto_update_traffic_secret(rx_secret, md, current_rx_secret,
                                          secretlen) != 0) {
    return -1;
  }

  if (ngtcp2_crypto_derive_packet_protection_key(rx_key, rx_iv, NULL, aead, md,
                                                 rx_secret, secretlen) != 0) {
    return -1;
  }

  if (ngtcp2_crypto_update_traffic_secret(tx_secret, md, current_tx_secret,
                                          secretlen) != 0) {
    return -1;
  }

  if (ngtcp2_crypto_derive_packet_protection_key(tx_key, tx_iv, NULL, aead, md,
                                                 tx_secret, secretlen) != 0) {
    return -1;
  }

  return 0;
}

int ngtcp2_crypto_encrypt_cb(uint8_t *dest, const ngtcp2_crypto_aead *aead,
                             const uint8_t *plaintext, size_t plaintextlen,
                             const uint8_t *key, const uint8_t *nonce,
                             size_t noncelen, const uint8_t *ad, size_t adlen) {
  if (ngtcp2_crypto_encrypt(dest, aead, plaintext, plaintextlen, key, nonce,
                            noncelen, ad, adlen) != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }
  return 0;
}

int ngtcp2_crypto_decrypt_cb(uint8_t *dest, const ngtcp2_crypto_aead *aead,
                             const uint8_t *ciphertext, size_t ciphertextlen,
                             const uint8_t *key, const uint8_t *nonce,
                             size_t noncelen, const uint8_t *ad, size_t adlen) {
  if (ngtcp2_crypto_decrypt(dest, aead, ciphertext, ciphertextlen, key, nonce,
                            noncelen, ad, adlen) != 0) {
    return NGTCP2_ERR_TLS_DECRYPT;
  }
  return 0;
}

int ngtcp2_crypto_hp_mask_cb(uint8_t *dest, const ngtcp2_crypto_cipher *hp,
                             const uint8_t *hp_key, const uint8_t *sample) {
  if (ngtcp2_crypto_hp_mask(dest, hp, hp_key, sample) != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }
  return 0;
}

int ngtcp2_crypto_update_key_cb(ngtcp2_conn *conn, uint8_t *rx_secret,
                                uint8_t *tx_secret, uint8_t *rx_key,
                                uint8_t *rx_iv, uint8_t *tx_key, uint8_t *tx_iv,
                                const uint8_t *current_rx_secret,
                                const uint8_t *current_tx_secret,
                                size_t secretlen, void *user_data) {
  (void)conn;
  (void)user_data;

  if (ngtcp2_crypto_update_key(conn, rx_secret, tx_secret, rx_key, rx_iv,
                               tx_key, tx_iv, current_rx_secret,
                               current_tx_secret, secretlen) != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }
  return 0;
}

int ngtcp2_crypto_generate_stateless_reset_token(uint8_t *token,
                                                 const ngtcp2_crypto_md *md,
                                                 const uint8_t *secret,
                                                 size_t secretlen,
                                                 const ngtcp2_cid *cid) {
  uint8_t buf[64];
  int rv;

  assert(ngtcp2_crypto_md_hashlen(md) <= sizeof(buf));
  assert(NGTCP2_STATELESS_RESET_TOKENLEN <= sizeof(buf));

  rv = ngtcp2_crypto_hkdf_extract(buf, md, secret, secretlen, cid->data,
                                  cid->datalen);
  if (rv != 0) {
    return -1;
  }

  memcpy(token, buf, NGTCP2_STATELESS_RESET_TOKENLEN);

  return 0;
}

ngtcp2_ssize ngtcp2_crypto_write_connection_close(uint8_t *dest, size_t destlen,
                                                  const ngtcp2_cid *dcid,
                                                  const ngtcp2_cid *scid,
                                                  uint64_t error_code) {
  uint8_t rx_secret[NGTCP2_CRYPTO_INITIAL_SECRETLEN];
  uint8_t tx_secret[NGTCP2_CRYPTO_INITIAL_SECRETLEN];
  uint8_t initial_secret[NGTCP2_CRYPTO_INITIAL_SECRETLEN];
  uint8_t tx_key[NGTCP2_CRYPTO_INITIAL_KEYLEN];
  uint8_t tx_iv[NGTCP2_CRYPTO_INITIAL_IVLEN];
  uint8_t tx_hp_key[NGTCP2_CRYPTO_INITIAL_KEYLEN];
  ngtcp2_crypto_ctx ctx;
  ngtcp2_ssize spktlen;

  ngtcp2_crypto_ctx_initial(&ctx);

  if (ngtcp2_crypto_derive_initial_secrets(rx_secret, tx_secret, initial_secret,
                                           scid,
                                           NGTCP2_CRYPTO_SIDE_SERVER) != 0) {
    return -1;
  }

  if (ngtcp2_crypto_derive_packet_protection_key(
          tx_key, tx_iv, tx_hp_key, &ctx.aead, &ctx.md, tx_secret,
          NGTCP2_CRYPTO_INITIAL_SECRETLEN) != 0) {
    return -1;
  }

  spktlen = ngtcp2_pkt_write_connection_close(
      dest, destlen, dcid, scid, error_code, ngtcp2_crypto_encrypt_cb,
      &ctx.aead, tx_key, tx_iv, ngtcp2_crypto_hp_mask_cb, &ctx.hp, tx_hp_key);
  if (spktlen < 0) {
    return -1;
  }

  return spktlen;
}

ngtcp2_ssize ngtcp2_crypto_write_retry(uint8_t *dest, size_t destlen,
                                       const ngtcp2_cid *dcid,
                                       const ngtcp2_cid *scid,
                                       const ngtcp2_cid *odcid,
                                       const uint8_t *token, size_t tokenlen) {
  ngtcp2_crypto_aead aead;
  ngtcp2_ssize spktlen;

  ngtcp2_crypto_aead_retry(&aead);

  spktlen = ngtcp2_pkt_write_retry(dest, destlen, dcid, scid, odcid, token,
                                   tokenlen, ngtcp2_crypto_encrypt_cb, &aead);
  if (spktlen < 0) {
    return -1;
  }

  return spktlen;
}

/*
 * crypto_set_local_transport_params gets local QUIC transport
 * parameters from |conn| and sets it to |tls|.
 *
 * This function returns 0 if it succeeds, or -1.
 */
static int crypto_set_local_transport_params(ngtcp2_conn *conn, void *tls) {
  ngtcp2_transport_params_type exttype =
      ngtcp2_conn_is_server(conn)
          ? NGTCP2_TRANSPORT_PARAMS_TYPE_ENCRYPTED_EXTENSIONS
          : NGTCP2_TRANSPORT_PARAMS_TYPE_CLIENT_HELLO;
  ngtcp2_transport_params params;
  ngtcp2_ssize nwrite;
  uint8_t buf[256];

  ngtcp2_conn_get_local_transport_params(conn, &params);

  nwrite = ngtcp2_encode_transport_params(buf, sizeof(buf), exttype, &params);
  if (nwrite < 0) {
    return -1;
  }

  if (ngtcp2_crypto_set_local_transport_params(tls, buf, (size_t)nwrite) != 0) {
    return -1;
  }

  return 0;
}

/*
 * crypto_setup_initial_crypto establishes the initial secrets and
 * encryption keys, and prepares local QUIC transport parameters.
 */
static int crypto_setup_initial_crypto(ngtcp2_conn *conn,
                                       const ngtcp2_cid *dcid) {
  void *tls = ngtcp2_conn_get_tls_native_handle(conn);

  if (ngtcp2_crypto_derive_and_install_initial_key(conn, NULL, NULL, NULL, NULL,
                                                   NULL, NULL, NULL, NULL, NULL,
                                                   dcid) != 0) {
    return -1;
  }

  return crypto_set_local_transport_params(conn, tls);
}

int ngtcp2_crypto_client_initial_cb(ngtcp2_conn *conn, void *user_data) {
  const ngtcp2_cid *dcid = ngtcp2_conn_get_dcid(conn);
  (void)user_data;

  if (crypto_setup_initial_crypto(conn, dcid) != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  if (ngtcp2_crypto_read_write_crypto_data(conn, NGTCP2_CRYPTO_LEVEL_INITIAL,
                                           NULL, 0) != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

int ngtcp2_crypto_recv_retry_cb(ngtcp2_conn *conn, const ngtcp2_pkt_hd *hd,
                                void *user_data) {
  (void)user_data;

  if (ngtcp2_crypto_derive_and_install_initial_key(conn, NULL, NULL, NULL, NULL,
                                                   NULL, NULL, NULL, NULL, NULL,
                                                   &hd->scid) != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

int ngtcp2_crypto_recv_client_initial_cb(ngtcp2_conn *conn,
                                         const ngtcp2_cid *dcid,
                                         void *user_data) {
  (void)user_data;

  if (crypto_setup_initial_crypto(conn, dcid) != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  return 0;
}
