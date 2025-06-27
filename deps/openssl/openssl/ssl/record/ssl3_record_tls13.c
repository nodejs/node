/*
 * Copyright 2016-2022 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "../ssl_local.h"
#include "record_local.h"
#include "internal/cryptlib.h"

/*-
 * tls13_enc encrypts/decrypts |n_recs| in |recs|. Calls SSLfatal on internal
 * error, but not otherwise. It is the responsibility of the caller to report
 * a bad_record_mac.
 *
 * Returns:
 *    0: On failure
 *    1: if the record encryption/decryption was successful.
 */
int tls13_enc(SSL *s, SSL3_RECORD *recs, size_t n_recs, int sending,
              ossl_unused SSL_MAC_BUF *mac, ossl_unused size_t macsize)
{
    EVP_CIPHER_CTX *ctx;
    unsigned char iv[EVP_MAX_IV_LENGTH], recheader[SSL3_RT_HEADER_LENGTH];
    size_t taglen, offset, loop, hdrlen;
    int ivlen;
    unsigned char *staticiv;
    unsigned char *seq;
    int lenu, lenf;
    SSL3_RECORD *rec = &recs[0];
    uint32_t alg_enc;
    WPACKET wpkt;

    if (n_recs != 1) {
        /* Should not happen */
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }

    if (sending) {
        ctx = s->enc_write_ctx;
        staticiv = s->write_iv;
        seq = RECORD_LAYER_get_write_sequence(&s->rlayer);
    } else {
        ctx = s->enc_read_ctx;
        staticiv = s->read_iv;
        seq = RECORD_LAYER_get_read_sequence(&s->rlayer);
    }

    /*
     * If we're sending an alert and ctx != NULL then we must be forcing
     * plaintext alerts. If we're reading and ctx != NULL then we allow
     * plaintext alerts at certain points in the handshake. If we've got this
     * far then we have already validated that a plaintext alert is ok here.
     */
    if (ctx == NULL || rec->type == SSL3_RT_ALERT) {
        memmove(rec->data, rec->input, rec->length);
        rec->input = rec->data;
        return 1;
    }

    ivlen = EVP_CIPHER_CTX_get_iv_length(ctx);
    if (ivlen < 0) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }

    if (s->early_data_state == SSL_EARLY_DATA_WRITING
            || s->early_data_state == SSL_EARLY_DATA_WRITE_RETRY) {
        if (s->session != NULL && s->session->ext.max_early_data > 0) {
            alg_enc = s->session->cipher->algorithm_enc;
        } else {
            if (!ossl_assert(s->psksession != NULL
                             && s->psksession->ext.max_early_data > 0)) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                return 0;
            }
            alg_enc = s->psksession->cipher->algorithm_enc;
        }
    } else {
        /*
         * To get here we must have selected a ciphersuite - otherwise ctx would
         * be NULL
         */
        if (!ossl_assert(s->s3.tmp.new_cipher != NULL)) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return 0;
        }
        alg_enc = s->s3.tmp.new_cipher->algorithm_enc;
    }

    if (alg_enc & SSL_AESCCM) {
        if (alg_enc & (SSL_AES128CCM8 | SSL_AES256CCM8))
            taglen = EVP_CCM8_TLS_TAG_LEN;
         else
            taglen = EVP_CCM_TLS_TAG_LEN;
         if (sending && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, taglen,
                                         NULL) <= 0) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return 0;
        }
    } else if (alg_enc & SSL_AESGCM) {
        taglen = EVP_GCM_TLS_TAG_LEN;
    } else if (alg_enc & SSL_CHACHA20) {
        taglen = EVP_CHACHAPOLY_TLS_TAG_LEN;
    } else {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }

    if (!sending) {
        /*
         * Take off tag. There must be at least one byte of content type as
         * well as the tag
         */
        if (rec->length < taglen + 1)
            return 0;
        rec->length -= taglen;
    }

    /* Set up IV */
    if (ivlen < SEQ_NUM_SIZE) {
        /* Should not happen */
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }
    offset = ivlen - SEQ_NUM_SIZE;
    memcpy(iv, staticiv, offset);
    for (loop = 0; loop < SEQ_NUM_SIZE; loop++)
        iv[offset + loop] = staticiv[offset + loop] ^ seq[loop];

    /* Increment the sequence counter */
    for (loop = SEQ_NUM_SIZE; loop > 0; loop--) {
        ++seq[loop - 1];
        if (seq[loop - 1] != 0)
            break;
    }
    if (loop == 0) {
        /* Sequence has wrapped */
        return 0;
    }

    if (EVP_CipherInit_ex(ctx, NULL, NULL, NULL, iv, sending) <= 0
            || (!sending && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG,
                                             taglen,
                                             rec->data + rec->length) <= 0)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }

    /* Set up the AAD */
    if (!WPACKET_init_static_len(&wpkt, recheader, sizeof(recheader), 0)
            || !WPACKET_put_bytes_u8(&wpkt, rec->type)
            || !WPACKET_put_bytes_u16(&wpkt, rec->rec_version)
            || !WPACKET_put_bytes_u16(&wpkt, rec->length + taglen)
            || !WPACKET_get_total_written(&wpkt, &hdrlen)
            || hdrlen != SSL3_RT_HEADER_LENGTH
            || !WPACKET_finish(&wpkt)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        WPACKET_cleanup(&wpkt);
        return 0;
    }

    /*
     * For CCM we must explicitly set the total plaintext length before we add
     * any AAD.
     */
    if (((alg_enc & SSL_AESCCM) != 0
                 && EVP_CipherUpdate(ctx, NULL, &lenu, NULL,
                                     (unsigned int)rec->length) <= 0)
            || EVP_CipherUpdate(ctx, NULL, &lenu, recheader,
                                sizeof(recheader)) <= 0
            || EVP_CipherUpdate(ctx, rec->data, &lenu, rec->input,
                                (unsigned int)rec->length) <= 0
            || EVP_CipherFinal_ex(ctx, rec->data + lenu, &lenf) <= 0
            || (size_t)(lenu + lenf) != rec->length) {
        return 0;
    }
    if (sending) {
        /* Add the tag */
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, taglen,
                                rec->data + rec->length) <= 0) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return 0;
        }
        rec->length += taglen;
    }

    return 1;
}
