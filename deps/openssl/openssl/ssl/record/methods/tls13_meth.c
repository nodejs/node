/*
 * Copyright 2022-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/evp.h>
#include <openssl/core_names.h>
#include "../../ssl_local.h"
#include "../record_local.h"
#include "recmethod_local.h"

static int tls13_set_crypto_state(OSSL_RECORD_LAYER *rl, int level,
    unsigned char *snkey,
    unsigned char *key, size_t keylen,
    unsigned char *iv, size_t ivlen,
    unsigned char *mackey, size_t mackeylen,
    const EVP_CIPHER *snciph,
    const EVP_CIPHER *ciph,
    size_t taglen,
    int mactype,
    const EVP_MD *md,
    COMP_METHOD *comp)
{
    EVP_CIPHER_CTX *ciph_ctx;
    EVP_MAC_CTX *mac_ctx;
    EVP_MAC *mac;
    OSSL_PARAM params[2], *p = params;
    int mode;
    int enc = (rl->direction == OSSL_RECORD_DIRECTION_WRITE) ? 1 : 0;

    rl->iv = OPENSSL_malloc(ivlen);
    if (rl->iv == NULL)
        return OSSL_RECORD_RETURN_FATAL;

    rl->nonce = OPENSSL_malloc(ivlen);
    if (rl->nonce == NULL)
        return OSSL_RECORD_RETURN_FATAL;

    memcpy(rl->iv, iv, ivlen);

    /* Integrity only */
    if (EVP_CIPHER_is_a(ciph, "NULL") && mactype == NID_hmac && md != NULL) {
        mac = EVP_MAC_fetch(rl->libctx, "HMAC", rl->propq);
        if (mac == NULL
            || (mac_ctx = rl->mac_ctx = EVP_MAC_CTX_new(mac)) == NULL) {
            EVP_MAC_free(mac);
            ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
            return OSSL_RECORD_RETURN_FATAL;
        }
        EVP_MAC_free(mac);
        *p++ = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST,
            (char *)EVP_MD_name(md), 0);
        *p = OSSL_PARAM_construct_end();
        if (!EVP_MAC_init(mac_ctx, key, keylen, params)) {
            ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
            return OSSL_RECORD_RETURN_FATAL;
        }
        goto end;
    }

    ciph_ctx = rl->enc_ctx = EVP_CIPHER_CTX_new();
    if (ciph_ctx == NULL) {
        ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
        return OSSL_RECORD_RETURN_FATAL;
    }

    mode = EVP_CIPHER_get_mode(ciph);

    if (EVP_CipherInit_ex(ciph_ctx, ciph, NULL, NULL, NULL, enc) <= 0
        || EVP_CIPHER_CTX_ctrl(ciph_ctx, EVP_CTRL_AEAD_SET_IVLEN, (int)ivlen,
               NULL)
            <= 0
        || (mode == EVP_CIPH_CCM_MODE
            && EVP_CIPHER_CTX_ctrl(ciph_ctx, EVP_CTRL_AEAD_SET_TAG, (int)taglen,
                   NULL)
                <= 0)
        || EVP_CipherInit_ex(ciph_ctx, NULL, NULL, key, NULL, enc) <= 0) {
        ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
        return OSSL_RECORD_RETURN_FATAL;
    }

    if (rl->isdtls && snciph != NULL) {
        EVP_CIPHER_CTX *sn_ciph_ctx;

        sn_ciph_ctx = rl->sn_enc_ctx = EVP_CIPHER_CTX_new();

        if (sn_ciph_ctx == NULL) {
            ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
            return OSSL_RECORD_RETURN_FATAL;
        }

        if (EVP_CIPHER_CTX_set_padding(sn_ciph_ctx, 0)
            || EVP_CipherInit_ex(sn_ciph_ctx, snciph, NULL,
                   snkey, NULL, 1)
                <= 0) {
            ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
            return OSSL_RECORD_RETURN_FATAL;
        }
    }

end:
    return OSSL_RECORD_RETURN_SUCCESS;
}

static int tls13_cipher(OSSL_RECORD_LAYER *rl, TLS_RL_RECORD *recs,
    size_t n_recs, int sending, SSL_MAC_BUF *mac,
    size_t macsize)
{
    EVP_CIPHER_CTX *enc_ctx;
    unsigned char recheader[SSL3_RT_HEADER_LENGTH];
    unsigned char tag[EVP_MAX_MD_SIZE];
    size_t nonce_len, offset, loop, hdrlen, taglen, exphdrlen;
    int isdtls, sbit = 0, addlen;
    unsigned char *staticiv;
    unsigned char *nonce;
    unsigned char seq[SEQ_NUM_SIZE], *p_seq = seq;
    uint64_t seqnum = 0;
    int lenu, lenf;
    TLS_RL_RECORD *rec = &recs[0];
    WPACKET wpkt;
    const EVP_CIPHER *cipher;
    EVP_MAC_CTX *mac_ctx = NULL;
    int mode;

    if (n_recs != 1) {
        /* Should not happen */
        RLAYERfatal(rl, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }

    enc_ctx = rl->enc_ctx; /* enc_ctx is ignored when rl->mac_ctx != NULL */
    staticiv = rl->iv;
    nonce = rl->nonce;
    isdtls = rl->isdtls;
    l2n8(rl->sequence, p_seq);

    if (enc_ctx == NULL && rl->mac_ctx == NULL) {
        RLAYERfatal(rl, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }

    /*
     * Plaintext alerts are allowed only when explicitly enabled. DTLS needs
     * this check here because it does not run the TLS header validator.
     */
    if (rec->type == SSL3_RT_ALERT) {
        if (!rl->allow_plain_alerts)
            return 0;
        memmove(rec->data, rec->input, rec->length);
        rec->input = rec->data;
        return 1;
    }

    /* Keyed DTLS 1.3 layers accept only unified headers. */
    if (isdtls && !DTLS13_UNI_HDR_FIX_BITS_IS_SET(rec->type))
        return 0;

    /* For integrity-only ciphers, nonce_len is same as MAC size */
    if (rl->mac_ctx != NULL) {
        nonce_len = EVP_MAC_CTX_get_mac_size(rl->mac_ctx);
    } else {
        int ivlen = EVP_CIPHER_CTX_get_iv_length(enc_ctx);

        if (ivlen < 0) {
            /* Should not happen */
            RLAYERfatal(rl, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return 0;
        }
        nonce_len = (size_t)ivlen;
    }

    if (!sending) {
        /*
         * Take off tag. There must be at least one byte of content type as
         * well as the tag
         */
        if (rec->length < rl->taglen + 1)
            return 0;
        rec->length -= rl->taglen;
    }

    /* Set up nonce: part of static IV followed by sequence number */
    if (nonce_len < SEQ_NUM_SIZE) {
        /* Should not happen */
        RLAYERfatal(rl, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }
    offset = nonce_len - SEQ_NUM_SIZE;
    memcpy(nonce, staticiv, offset);
    for (loop = 0; loop < SEQ_NUM_SIZE; loop++)
        nonce[offset + loop] = staticiv[offset + loop] ^ seq[loop];

    if (!isdtls && !tls_increment_sequence_ctr(rl)) {
        /* RLAYERfatal already called */
        return 0;
    }

    /*-
     * Set up the additional data as described in rfc8446 section 5.2:
     *   "and the additional data input is the record header.
     *   I.e.,
     *      additional_data = TLSCiphertext.opaque_type ||
     *                        TLSCiphertext.legacy_record_version ||
     *                        TLSCiphertext.length"
     * and in rfc1947 section 4:
     *   "The entire header value shown in Figure 4 (but prior to record number
     *   encryption; see Section 4.2.3) is used as the additional data value for
     *   the AEAD function. For instance, if the minimal variant is used, the
     *   Associated Data (AD) is 2 octets long."
     *
     *   For DTLS: at this point rec->type is just the first byte of the variable
     *   header. So it is not an actual record type. The record type is set in
     *   tls13_post_process_record() for incoming records.
     */
    if (isdtls) {
        exphdrlen = dtls_get_rec_header_size(rec->type);
        sbit = DTLS13_UNI_HDR_SEQ_BIT_IS_SET(rec->type);
        addlen = DTLS13_UNI_HDR_LEN_BIT_IS_SET(rec->type);
        /* Match the truncated value encoded in the unified header. */
        seqnum = rl->sequence & DTLS13_UNI_HDR_SEQ_MASK(sbit ? 2 : 1);
    } else {
        exphdrlen = SSL3_RT_HEADER_LENGTH;
        addlen = 1;
    }

    /*
     * Reject unsupported CID before WPACKET setup so cleanup cannot touch
     * an uninitialised packet.
     */
    if (isdtls && !ossl_assert(!DTLS13_UNI_HDR_CID_BIT_IS_SET(rec->type))) {
        RLAYERfatal(rl, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }

    if (!WPACKET_init_static_len(&wpkt, recheader, sizeof(recheader), 0)
        || !WPACKET_put_bytes_u8(&wpkt, rec->type)
        || (isdtls
            && (sbit ? !WPACKET_put_bytes_u16(&wpkt, seqnum)
                     : !WPACKET_put_bytes_u8(&wpkt, seqnum)))
        || (!isdtls && !WPACKET_put_bytes_u16(&wpkt, rec->rec_version))
        || (addlen && !WPACKET_put_bytes_u16(&wpkt, rec->length + rl->taglen))
        || !WPACKET_get_total_written(&wpkt, &hdrlen)
        || hdrlen != exphdrlen
        || !WPACKET_finish(&wpkt)) {
        RLAYERfatal(rl, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        WPACKET_cleanup(&wpkt);
        return 0;
    }

    if (rl->mac_ctx != NULL) {
        int ret = 0;

        if ((mac_ctx = EVP_MAC_CTX_dup(rl->mac_ctx)) == NULL
            || !EVP_MAC_update(mac_ctx, nonce, nonce_len)
            || !EVP_MAC_update(mac_ctx, recheader, hdrlen)
            || !EVP_MAC_update(mac_ctx, rec->input, rec->length)
            || !EVP_MAC_final(mac_ctx, tag, &taglen, rl->taglen)) {
            RLAYERfatal(rl, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            goto end_mac;
        }

        if (sending) {
            memcpy(rec->data + rec->length, tag, rl->taglen);
            rec->length += rl->taglen;
        } else if (CRYPTO_memcmp(tag, rec->data + rec->length,
                       rl->taglen)
            != 0) {
            goto end_mac;
        }
        ret = 1;
    end_mac:
        EVP_MAC_CTX_free(mac_ctx);
        return ret;
    }

    cipher = EVP_CIPHER_CTX_get0_cipher(enc_ctx);
    if (cipher == NULL) {
        RLAYERfatal(rl, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }
    mode = EVP_CIPHER_get_mode(cipher);

    if (EVP_CipherInit_ex(enc_ctx, NULL, NULL, NULL, nonce, sending) <= 0
        || (!sending && EVP_CIPHER_CTX_ctrl(enc_ctx, EVP_CTRL_AEAD_SET_TAG, (int)rl->taglen, rec->data + rec->length) <= 0)) {
        RLAYERfatal(rl, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }

    /*
     * For CCM we must explicitly set the total plaintext length before we add
     * any AAD.
     */
    if ((mode == EVP_CIPH_CCM_MODE
            && EVP_CipherUpdate(enc_ctx, NULL, &lenu, NULL, (int)rec->length) <= 0)
        || EVP_CipherUpdate(enc_ctx, NULL, &lenu, recheader, (int)hdrlen) <= 0
        || EVP_CipherUpdate(enc_ctx, rec->data, &lenu, rec->input, (int)rec->length) <= 0
        || EVP_CipherFinal_ex(enc_ctx, rec->data + lenu, &lenf) <= 0
        || (size_t)lenu + lenf != rec->length) {
        return 0;
    }
    if (sending) {
        /* Add the tag */
        if (EVP_CIPHER_CTX_ctrl(enc_ctx, EVP_CTRL_AEAD_GET_TAG, (int)rl->taglen,
                rec->data + rec->length)
            <= 0) {
            RLAYERfatal(rl, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return 0;
        }
        rec->length += rl->taglen;
    }

    return 1;
}

static int tls13_validate_record_header(OSSL_RECORD_LAYER *rl,
    TLS_RL_RECORD *rec)
{
    if (rec->type != SSL3_RT_APPLICATION_DATA
        && (rec->type != SSL3_RT_CHANGE_CIPHER_SPEC
            || !rl->is_first_handshake)
        && (rec->type != SSL3_RT_ALERT || !rl->allow_plain_alerts)) {
        RLAYERfatal(rl, SSL_AD_UNEXPECTED_MESSAGE, SSL_R_BAD_RECORD_TYPE);
        return 0;
    }

    if (rec->rec_version != TLS1_2_VERSION) {
        RLAYERfatal(rl, SSL_AD_DECODE_ERROR, SSL_R_WRONG_VERSION_NUMBER);
        return 0;
    }

    if (rec->length > SSL3_RT_MAX_TLS13_ENCRYPTED_LENGTH) {
        RLAYERfatal(rl, SSL_AD_RECORD_OVERFLOW,
            SSL_R_ENCRYPTED_LENGTH_TOO_LONG);
        return 0;
    }
    return 1;
}

static int tls13_post_process_record(OSSL_RECORD_LAYER *rl, TLS_RL_RECORD *rec)
{
    /* Skip this if we've received a plaintext alert */
    if (rec->type != SSL3_RT_ALERT) {
        size_t end;

        if (rec->length == 0
            || (rl->isdtls
                    ? !DTLS13_UNI_HDR_FIX_BITS_IS_SET(rec->type)
                    : rec->type != SSL3_RT_APPLICATION_DATA)) {
            RLAYERfatal(rl, SSL_AD_UNEXPECTED_MESSAGE,
                SSL_R_BAD_RECORD_TYPE);
            return 0;
        }

        /* Strip trailing padding */
        for (end = rec->length - 1; end > 0 && rec->data[end] == 0; end--)
            continue;

        rec->length = end;
        rec->type = rec->data[end];
    }

    if (rec->length > SSL3_RT_MAX_PLAIN_LENGTH) {
        RLAYERfatal(rl, SSL_AD_RECORD_OVERFLOW, SSL_R_DATA_LENGTH_TOO_LONG);
        return 0;
    }

    if (!tls13_common_post_process_record(rl, rec)) {
        /* RLAYERfatal already called */
        return 0;
    }

    return 1;
}

static uint8_t tls13_get_record_type(OSSL_RECORD_LAYER *rl,
    OSSL_RECORD_TEMPLATE *template)
{
    if (rl->allow_plain_alerts && template->type == SSL3_RT_ALERT)
        return SSL3_RT_ALERT;

    /*
     * Aside from the above case we always use the application data record type
     * when encrypting in TLSv1.3. The "inner" record type encodes the "real"
     * record type from the template.
     */
    if (rl->isdtls) {
        const unsigned char fixed = DTLS13_UNI_HDR_FIX_BITS;
        const unsigned char sbit = DTLS13_UNI_HDR_SEQ_BIT;
        const unsigned char lbit = DTLS13_UNI_HDR_LEN_BIT;
        const unsigned char epochbits = DTLS13_UNI_HDR_EPOCH_BITS_MASK & rl->epoch;

        return fixed | sbit | lbit | epochbits;
    }

    return SSL3_RT_APPLICATION_DATA;
}

static int tls13_add_record_padding(OSSL_RECORD_LAYER *rl,
    OSSL_RECORD_TEMPLATE *thistempl,
    WPACKET *thispkt,
    TLS_RL_RECORD *thiswr)
{
    size_t rlen;
    size_t max_frag_len = rl->max_frag_len;
    int isdtls = rl->isdtls;
    size_t mac_size = 0;
    size_t taglen = rl->taglen;

    /* Nothing to be done in the case of a plaintext alert */
    if (rl->allow_plain_alerts && thistempl->type != SSL3_RT_ALERT)
        return 1;

    if (!WPACKET_put_bytes_u8(thispkt, thistempl->type)) {
        RLAYERfatal(rl, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }
    TLS_RL_RECORD_add_length(thiswr, 1);

    if (rl->isdtls && rl->curr_mtu != 0 && rl->curr_mtu < max_frag_len)
        max_frag_len = rl->curr_mtu;

    /* Add TLS1.3 padding */
    rlen = TLS_RL_RECORD_get_length(thiswr);
    if (rlen < max_frag_len) {
        size_t padding = 0;
        size_t max_padding = max_frag_len - rlen;

        /*
         * We might want to change the "else if" below so that
         * library-added padding can still happen even if there
         * is an application-layer callback. The reason being
         * the application may not be aware that the effectiveness
         * of ECH could be damaged if the callback e.g. only
         * padded application data. However, doing so would be
         * a change that could break some application that has
         * a client and server that both know what padding they
         * like, and that dislike any other padding. That'd need
         * one of those to have been updated though so the
         * probability may be low enough that we could change
         * the "else if" below to just an "if" and pick the
         * larger of the library and callback's idea of padding.
         * (Still subject to max_padding though.)
         */
        if (rl->padding != NULL) {
            padding = rl->padding(rl->cbarg, thistempl->type, rlen);
        } else if (rl->block_padding > 0 || rl->hs_padding > 0) {
            size_t mask, bp = 0, remainder;

            /*
             * pad handshake or alert messages based on |hs_padding|
             * but application data based on |block_padding|
             */
            if (thistempl->type == SSL3_RT_HANDSHAKE && rl->hs_padding > 0)
                bp = rl->hs_padding;
            else if (thistempl->type == SSL3_RT_ALERT && rl->hs_padding > 0)
                bp = rl->hs_padding;
            else if (thistempl->type == SSL3_RT_APPLICATION_DATA
                && rl->block_padding > 0)
                bp = rl->block_padding;
            if (bp > 0) {
                mask = bp - 1;
                /* optimize for power of 2 */
                if ((bp & mask) == 0)
                    remainder = rlen & mask;
                else
                    remainder = rlen % bp;
                /* don't want to add a block of padding if we don't have to */
                if (remainder == 0)
                    padding = 0;
                else
                    padding = bp - remainder;
            }
        }

        /*
         * DTLS1.3 RFC 9147 Section 4.2.3 says records should be padded
         * if the ciphertext is less than 16 bytes.
         */
        if (isdtls) {
            if (rl->mac_ctx != NULL)
                mac_size = EVP_MAC_CTX_get_mac_size(rl->mac_ctx);

            if (padding + rlen + taglen + mac_size < DTLS13_CIPHERTEXT_MINSIZE)
                padding += DTLS13_CIPHERTEXT_MINSIZE - (padding + rlen + taglen + mac_size);
        }

        if (padding > 0) {
            /* do not allow the record to exceed max plaintext length */
            if (padding > max_padding)
                padding = max_padding;
            if (!WPACKET_memset(thispkt, 0, padding)) {
                RLAYERfatal(rl, SSL_AD_INTERNAL_ERROR,
                    ERR_R_INTERNAL_ERROR);
                return 0;
            }
            TLS_RL_RECORD_add_length(thiswr, padding);
        }
    }

    return 1;
}

const struct record_functions_st tls_1_3_funcs = {
    tls13_set_crypto_state,
    tls13_cipher,
    NULL,
    tls_default_set_protocol_version,
    tls_default_read_n,
    tls_get_more_records,
    tls13_validate_record_header,
    tls13_post_process_record,
    tls_get_max_records_default,
    tls_write_records_default,
    tls_allocate_write_buffers_default,
    tls_initialise_write_packets_default,
    tls13_get_record_type,
    tls_prepare_record_header_default,
    tls13_add_record_padding,
    tls_prepare_for_encryption_default,
    tls_post_encryption_processing_default,
    NULL
};

const struct record_functions_st dtls_1_3_funcs = {
    tls13_set_crypto_state,
    tls13_cipher,
    NULL,
    tls_default_set_protocol_version,
    tls_default_read_n,
    dtls_get_more_records,
    /*
     * Keep this NULL: the TLS validator raises fatal errors, while DTLS must
     * silently discard invalid records (RFC 9147 section 4.5.2).
     */
    NULL,
    tls13_post_process_record,
    NULL,
    tls_write_records_default,
    tls_allocate_write_buffers_default,
    tls_initialise_write_packets_default,
    tls13_get_record_type,
    dtls_prepare_record_header,
    tls13_add_record_padding,
    tls_prepare_for_encryption_default,
    dtls_post_encryption_processing,
    NULL
};
