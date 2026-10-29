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
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include "internal/ssl3_cbc.h"
#include "../../ssl_local.h"
#include "../record_local.h"
#include "recmethod_local.h"

static int tls1_set_crypto_state(OSSL_RECORD_LAYER *rl, int level,
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
    EVP_PKEY *mac_key;
    OSSL_PARAM params[2], *p = params;
    int enc = (rl->direction == OSSL_RECORD_DIRECTION_WRITE) ? 1 : 0;

    if (level != OSSL_RECORD_PROTECTION_LEVEL_APPLICATION)
        return OSSL_RECORD_RETURN_FATAL;

    if ((rl->enc_ctx = EVP_CIPHER_CTX_new()) == NULL) {
        RLAYERfatal(rl, SSL_AD_INTERNAL_ERROR, ERR_R_EVP_LIB);
        return OSSL_RECORD_RETURN_FATAL;
    }

    ciph_ctx = rl->enc_ctx;

    rl->md_ctx = EVP_MD_CTX_new();
    if (rl->md_ctx == NULL) {
        RLAYERfatal(rl, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return OSSL_RECORD_RETURN_FATAL;
    }
#ifndef OPENSSL_NO_COMP
    if (comp != NULL) {
        rl->compctx = COMP_CTX_new(comp);
        if (rl->compctx == NULL) {
            ERR_raise(ERR_LIB_SSL, SSL_R_COMPRESSION_LIBRARY_ERROR);
            return OSSL_RECORD_RETURN_FATAL;
        }
    }
#endif

    /*
     * If we have an AEAD Cipher, then there is no separate MAC, so we can skip
     * setting up the MAC key.
     */
    if ((EVP_CIPHER_get_flags(ciph) & EVP_CIPH_FLAG_AEAD_CIPHER) == 0) {
        if (mactype == EVP_PKEY_HMAC) {
            mac_key = EVP_PKEY_new_raw_private_key_ex(rl->libctx, "HMAC",
                rl->propq, mackey,
                mackeylen);
        } else {
            /*
             * If its not HMAC then the only other types of MAC we support are
             * the GOST MACs, so we need to use the old style way of creating
             * a MAC key.
             */
            mac_key = EVP_PKEY_new_mac_key(mactype, NULL, mackey,
                (int)mackeylen);
        }

        /*
         * We want the underlying mac to use our passed property query when allocating
         * its internal digest as well
         */
        if (rl->propq != NULL)
            *p++ = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_PROPERTIES,
                (char *)rl->propq, 0);

        *p = OSSL_PARAM_construct_end();

        if (mac_key == NULL
            || EVP_DigestSignInit_ex(rl->md_ctx, NULL, EVP_MD_get0_name(md),
                   rl->libctx, rl->propq, mac_key,
                   params)
                <= 0) {
            EVP_PKEY_free(mac_key);
            ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
            return OSSL_RECORD_RETURN_FATAL;
        }
        EVP_PKEY_free(mac_key);
    }

    if (EVP_CIPHER_get_mode(ciph) == EVP_CIPH_GCM_MODE) {
        if (!EVP_CipherInit_ex(ciph_ctx, ciph, NULL, key, NULL, enc)
            || EVP_CIPHER_CTX_ctrl(ciph_ctx, EVP_CTRL_GCM_SET_IV_FIXED,
                   (int)ivlen, iv)
                <= 0) {
            ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
            return OSSL_RECORD_RETURN_FATAL;
        }
    } else if (EVP_CIPHER_get_mode(ciph) == EVP_CIPH_CCM_MODE) {
        if (!EVP_CipherInit_ex(ciph_ctx, ciph, NULL, NULL, NULL, enc)
            || EVP_CIPHER_CTX_ctrl(ciph_ctx, EVP_CTRL_AEAD_SET_IVLEN, 12,
                   NULL)
                <= 0
            || EVP_CIPHER_CTX_ctrl(ciph_ctx, EVP_CTRL_AEAD_SET_TAG,
                   (int)taglen, NULL)
                <= 0
            || EVP_CIPHER_CTX_ctrl(ciph_ctx, EVP_CTRL_CCM_SET_IV_FIXED,
                   (int)ivlen, iv)
                <= 0
            || !EVP_CipherInit_ex(ciph_ctx, NULL, NULL, key, NULL, enc)) {
            ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
            return OSSL_RECORD_RETURN_FATAL;
        }
    } else {
        if (!EVP_CipherInit_ex(ciph_ctx, ciph, NULL, key, iv, enc)) {
            ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
            return OSSL_RECORD_RETURN_FATAL;
        }
    }
    /* Needed for "composite" AEADs, such as RC4-HMAC-MD5 */
    if ((EVP_CIPHER_get_flags(ciph) & EVP_CIPH_FLAG_AEAD_CIPHER) != 0
        && mackeylen != 0
        && EVP_CIPHER_CTX_ctrl(ciph_ctx, EVP_CTRL_AEAD_SET_MAC_KEY,
               (int)mackeylen, mackey)
            <= 0) {
        ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
        return OSSL_RECORD_RETURN_FATAL;
    }

    if (EVP_CIPHER_get0_provider(EVP_CIPHER_CTX_get0_cipher(ciph_ctx)) != NULL
        && !ossl_set_tls_provider_parameters(rl, ciph_ctx, ciph, md)) {
        /* ERR_raise already called */
        return OSSL_RECORD_RETURN_FATAL;
    }

    /* Calculate the explicit IV length */
    if (RLAYER_USE_EXPLICIT_IV(rl)) {
        int mode = EVP_CIPHER_CTX_get_mode(ciph_ctx);
        int eivlen = 0;

        if (mode == EVP_CIPH_CBC_MODE) {
            eivlen = EVP_CIPHER_CTX_get_iv_length(ciph_ctx);
            if (eivlen < 0) {
                RLAYERfatal(rl, SSL_AD_INTERNAL_ERROR, SSL_R_LIBRARY_BUG);
                return OSSL_RECORD_RETURN_FATAL;
            }
            if (eivlen <= 1)
                eivlen = 0;
        } else if (mode == EVP_CIPH_GCM_MODE) {
            /* Need explicit part of IV for GCM mode */
            eivlen = EVP_GCM_TLS_EXPLICIT_IV_LEN;
        } else if (mode == EVP_CIPH_CCM_MODE) {
            eivlen = EVP_CCM_TLS_EXPLICIT_IV_LEN;
        }
        rl->eivlen = (size_t)eivlen;
    }

    return OSSL_RECORD_RETURN_SUCCESS;
}

static int setup_record_header(const OSSL_RECORD_LAYER *rl, TLS_RL_RECORD *rec,
    unsigned char *buf, size_t buflen)
{
    WPACKET hdr;
    size_t hdrsize;

    if (buflen < EVP_AEAD_TLS1_AAD_LEN)
        return 0;
    if (!WPACKET_init_static_len(&hdr, buf, EVP_AEAD_TLS1_AAD_LEN, 0)
        || (rl->isdtls && !WPACKET_put_bytes_u16(&hdr, rl->epoch))
        || (rl->isdtls ? !WPACKET_put_bytes_u48(&hdr, rl->sequence)
                       : !WPACKET_put_bytes_u64(&hdr, rl->sequence))
        || !WPACKET_put_bytes_u8(&hdr, rec->type)
        || !WPACKET_put_bytes_u16(&hdr, rl->version)
        || !WPACKET_put_bytes_u16(&hdr, rec->length)
        || !WPACKET_finish(&hdr)
        || !WPACKET_get_total_written(&hdr, &hdrsize)
        || hdrsize != EVP_AEAD_TLS1_AAD_LEN) {
        WPACKET_cleanup(&hdr);
        return 0;
    }

    return 1;
}

#define MAX_PADDING 256
/*-
 * tls1_cipher encrypts/decrypts |n_recs| in |recs|. Calls RLAYERfatal on
 * internal error, but not otherwise. It is the responsibility of the caller to
 * report a bad_record_mac - if appropriate (DTLS just drops the record).
 *
 * Returns:
 *    0: if the record is publicly invalid, or an internal error, or AEAD
 *       decryption failed, or Encrypt-then-mac decryption failed.
 *    1: Success or Mac-then-encrypt decryption failed (MAC will be randomised)
 */
static int tls1_cipher(OSSL_RECORD_LAYER *rl, TLS_RL_RECORD *recs,
    size_t n_recs, int sending, SSL_MAC_BUF *macs,
    size_t macsize)
{
    EVP_CIPHER_CTX *ds;
    size_t reclen[SSL_MAX_PIPELINES];
    unsigned char buf[SSL_MAX_PIPELINES][EVP_AEAD_TLS1_AAD_LEN];
    unsigned char *data[SSL_MAX_PIPELINES];
    int pad = 0;
    size_t bs, ctr;
    const EVP_CIPHER *enc;
    int outlen;

    if (n_recs == 0) {
        RLAYERfatal(rl, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }

    if (EVP_MD_CTX_get0_md(rl->md_ctx)) {
        int n = EVP_MD_CTX_get_size(rl->md_ctx);

        if (!ossl_assert(n >= 0)) {
            RLAYERfatal(rl, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return 0;
        }
    }
    ds = rl->enc_ctx;
    if (!ossl_assert(rl->enc_ctx != NULL)) {
        RLAYERfatal(rl, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }

    enc = EVP_CIPHER_CTX_get0_cipher(rl->enc_ctx);

    if (sending) {
        int ivlen;

        /* For TLSv1.1 and later explicit IV */
        if (RLAYER_USE_EXPLICIT_IV(rl)
            && EVP_CIPHER_get_mode(enc) == EVP_CIPH_CBC_MODE)
            ivlen = EVP_CIPHER_get_iv_length(enc);
        else
            ivlen = 0;
        if (ivlen > 1) {
            for (ctr = 0; ctr < n_recs; ctr++) {
                if (recs[ctr].data != recs[ctr].input) {
                    RLAYERfatal(rl, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                    return 0;
                } else if (RAND_bytes_ex(rl->libctx, recs[ctr].input,
                               ivlen, 0)
                    <= 0) {
                    RLAYERfatal(rl, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                    return 0;
                }
            }
        }
    }
    if (!ossl_assert(enc != NULL)) {
        RLAYERfatal(rl, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }

    bs = EVP_CIPHER_get_block_size(EVP_CIPHER_CTX_get0_cipher(ds));

    if (bs == 0) {
        RLAYERfatal(rl, SSL_AD_INTERNAL_ERROR, SSL_R_BAD_CIPHER);
        return 0;
    }

    if (n_recs > 1) {
        if ((EVP_CIPHER_get_flags(EVP_CIPHER_CTX_get0_cipher(ds))
                & EVP_CIPH_FLAG_PIPELINE)
            == 0) {
            /*
             * We shouldn't have been called with pipeline data if the
             * cipher doesn't support pipelining
             */
            RLAYERfatal(rl, SSL_AD_INTERNAL_ERROR, SSL_R_PIPELINE_FAILURE);
            return 0;
        }
    }
    for (ctr = 0; ctr < n_recs; ctr++) {
        reclen[ctr] = recs[ctr].length;

        if ((EVP_CIPHER_get_flags(EVP_CIPHER_CTX_get0_cipher(ds))
                & EVP_CIPH_FLAG_AEAD_CIPHER)
            != 0) {
            /*
             * Publicly invalid: the record is shorter than the mandatory
             * AEAD overhead (explicit IV plus authentication tag). Leave
             * alert handling to the caller so TLS reports bad_record_mac
             * and DTLS silently discards the record.
             */
            if (!sending && reclen[ctr] < rl->eivlen + rl->taglen)
                return 0;

            if (!setup_record_header(rl, &recs[ctr], buf[ctr], sizeof(buf[ctr]))) {
                RLAYERfatal(rl, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                return 0;
            }

            if (!rl->isdtls && !tls_increment_sequence_ctr(rl)) {
                /* RLAYERfatal already called */
                return 0;
            }

            pad = EVP_CIPHER_CTX_ctrl(ds, EVP_CTRL_AEAD_TLS1_AAD,
                EVP_AEAD_TLS1_AAD_LEN, buf[ctr]);
            if (pad <= 0) {
                RLAYERfatal(rl, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                return 0;
            }

            if (sending) {
                reclen[ctr] += pad;
                recs[ctr].length += pad;
            }
        }

        if (!sending) {
            if (reclen[ctr] == 0 || reclen[ctr] % bs != 0) {
                /* Publicly invalid */
                return 0;
            }
        }
    }
    if (n_recs > 1) {
        /* Set the output buffers */
        for (ctr = 0; ctr < n_recs; ctr++)
            data[ctr] = recs[ctr].data;

        if (EVP_CIPHER_CTX_ctrl(ds, EVP_CTRL_SET_PIPELINE_OUTPUT_BUFS,
                (int)n_recs, data)
            <= 0) {
            RLAYERfatal(rl, SSL_AD_INTERNAL_ERROR, SSL_R_PIPELINE_FAILURE);
            return 0;
        }
        /* Set the input buffers */
        for (ctr = 0; ctr < n_recs; ctr++)
            data[ctr] = recs[ctr].input;

        if (EVP_CIPHER_CTX_ctrl(ds, EVP_CTRL_SET_PIPELINE_INPUT_BUFS,
                (int)n_recs, data)
                <= 0
            || EVP_CIPHER_CTX_ctrl(ds, EVP_CTRL_SET_PIPELINE_INPUT_LENS,
                   (int)n_recs, reclen)
                <= 0) {
            RLAYERfatal(rl, SSL_AD_INTERNAL_ERROR, SSL_R_PIPELINE_FAILURE);
            return 0;
        }
    }

    if (!rl->isdtls && rl->tlstree) {
        int decrement_seq = 0;
        unsigned char recseq[SEQ_NUM_SIZE], *p_recseq = recseq;

        l2n8(rl->sequence, p_recseq);

        /*
         * When sending, seq is incremented after MAC calculation.
         * So if we are in ETM mode, we use seq 'as is' in the ctrl-function.
         * Otherwise we have to decrease it in the implementation
         */
        if (sending && !rl->use_etm)
            decrement_seq = 1;

        if (EVP_CIPHER_CTX_ctrl(ds, EVP_CTRL_TLSTREE, decrement_seq, recseq) <= 0) {
            RLAYERfatal(rl, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return 0;
        }
    }

    /* Provided cipher - we do not support pipelining on this path */
    if (n_recs > 1) {
        RLAYERfatal(rl, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }

    if (!EVP_CipherUpdate(ds, recs[0].data, &outlen, recs[0].input,
            (int)reclen[0]))
        return 0;
    recs[0].length = outlen;

    /*
     * The length returned from EVP_CipherUpdate above is the actual
     * payload length. We need to adjust the data/input ptr to skip over
     * any explicit IV
     */
    if (!sending) {
        if (EVP_CIPHER_get_mode(enc) == EVP_CIPH_GCM_MODE) {
            recs[0].data += EVP_GCM_TLS_EXPLICIT_IV_LEN;
            recs[0].input += EVP_GCM_TLS_EXPLICIT_IV_LEN;
        } else if (EVP_CIPHER_get_mode(enc) == EVP_CIPH_CCM_MODE) {
            recs[0].data += EVP_CCM_TLS_EXPLICIT_IV_LEN;
            recs[0].input += EVP_CCM_TLS_EXPLICIT_IV_LEN;
        } else if (bs != 1 && RLAYER_USE_EXPLICIT_IV(rl)) {
            recs[0].data += bs;
            recs[0].input += bs;
            recs[0].orig_len -= bs;
        }

        /* Now get a pointer to the MAC (if applicable) */
        if (macs != NULL) {
            OSSL_PARAM params[2], *p = params;

            /* Get the MAC */
            macs[0].alloced = 0;

            *p++ = OSSL_PARAM_construct_octet_ptr(OSSL_CIPHER_PARAM_TLS_MAC,
                (void **)&macs[0].mac,
                macsize);
            *p = OSSL_PARAM_construct_end();

            if (!EVP_CIPHER_CTX_get_params(ds, params)) {
                /* Shouldn't normally happen */
                RLAYERfatal(rl, SSL_AD_INTERNAL_ERROR,
                    ERR_R_INTERNAL_ERROR);
                return 0;
            }
        }
    }

    return 1;
}

static int tls1_mac(OSSL_RECORD_LAYER *rl, TLS_RL_RECORD *rec, unsigned char *md,
    int sending)
{
    unsigned char seq[SEQ_NUM_SIZE], *p_seq = seq;
    EVP_MD_CTX *hash;
    size_t md_size;
    EVP_MD_CTX *hmac = NULL, *mac_ctx;
    EVP_PKEY_CTX *pkctx;
    unsigned char header[13];
    int t;
    int ret = 0;

    hash = rl->md_ctx;
    l2n8(rl->sequence, p_seq);

    t = EVP_MD_CTX_get_size(hash);
    if (!ossl_assert(t >= 0))
        return 0;
    md_size = t;

    if (rl->stream_mac) {
        mac_ctx = hash;
    } else {
        hmac = EVP_MD_CTX_new();
        if (hmac == NULL || !EVP_MD_CTX_copy(hmac, hash)) {
            goto end;
        }
        mac_ctx = hmac;
    }

    if (!rl->isdtls
        && rl->tlstree
        && EVP_MD_CTX_ctrl(mac_ctx, EVP_MD_CTRL_TLSTREE, 0, seq) <= 0)
        goto end;

    if (!setup_record_header(rl, rec, header, sizeof(header)))
        goto end;

    if (!sending && !rl->use_etm
        && EVP_CIPHER_CTX_get_mode(rl->enc_ctx) == EVP_CIPH_CBC_MODE
        && ssl3_cbc_record_digest_supported(mac_ctx)) {
        OSSL_PARAM tls_hmac_params[2], *p = tls_hmac_params;

        *p++ = OSSL_PARAM_construct_size_t(OSSL_MAC_PARAM_TLS_DATA_SIZE,
            &rec->orig_len);
        *p++ = OSSL_PARAM_construct_end();

        pkctx = EVP_MD_CTX_get_pkey_ctx(mac_ctx);
        if (pkctx == NULL) {
            RLAYERfatal(rl, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            goto end;
        }

        if (!EVP_PKEY_CTX_set_params(pkctx, tls_hmac_params))
            goto end;
    }

    if (EVP_DigestSignUpdate(mac_ctx, header, sizeof(header)) <= 0
        || EVP_DigestSignUpdate(mac_ctx, rec->input, rec->length) <= 0
        || EVP_DigestSignFinal(mac_ctx, md, &md_size) <= 0)
        goto end;

    OSSL_TRACE_BEGIN(TLS)
    {
        BIO_printf(trc_out, "seq:\n");
        BIO_dump_indent(trc_out, seq, 8, 4);
        BIO_printf(trc_out, "rec:\n");
        BIO_dump_indent(trc_out, rec->data, (int)rec->length, 4);
    }
    OSSL_TRACE_END(TLS);

    if (!rl->isdtls && !tls_increment_sequence_ctr(rl)) {
        /* RLAYERfatal already called */
        goto end;
    }

    OSSL_TRACE_BEGIN(TLS)
    {
        BIO_printf(trc_out, "md:\n");
        BIO_dump_indent(trc_out, md, (int)md_size, 4);
    }
    OSSL_TRACE_END(TLS);
    ret = 1;
end:
    EVP_MD_CTX_free(hmac);
    return ret;
}

#if defined(SSL3_ALIGN_PAYLOAD) && SSL3_ALIGN_PAYLOAD != 0
#ifndef OPENSSL_NO_COMP
#define MAX_PREFIX_LEN ((SSL3_ALIGN_PAYLOAD - 1) \
    + SSL3_RT_SEND_MAX_ENCRYPTED_OVERHEAD        \
    + SSL3_RT_HEADER_LENGTH                      \
    + SSL3_RT_MAX_COMPRESSED_OVERHEAD)
#else
#define MAX_PREFIX_LEN ((SSL3_ALIGN_PAYLOAD - 1) \
    + SSL3_RT_SEND_MAX_ENCRYPTED_OVERHEAD        \
    + SSL3_RT_HEADER_LENGTH)
#endif /* OPENSSL_NO_COMP */
#else
#ifndef OPENSSL_NO_COMP
#define MAX_PREFIX_LEN (SSL3_RT_SEND_MAX_ENCRYPTED_OVERHEAD \
    + SSL3_RT_HEADER_LENGTH                                 \
    + SSL3_RT_MAX_COMPRESSED_OVERHEAD)
#else
#define MAX_PREFIX_LEN (SSL3_RT_SEND_MAX_ENCRYPTED_OVERHEAD \
    + SSL3_RT_HEADER_LENGTH)
#endif /* OPENSSL_NO_COMP */
#endif

int tls1_allocate_write_buffers(OSSL_RECORD_LAYER *rl,
    OSSL_RECORD_TEMPLATE *templates,
    size_t numtempl, size_t *prefix)
{
    /* Do we need to add an empty record prefix? */
    *prefix = rl->need_empty_fragments
        && templates[0].type == SSL3_RT_APPLICATION_DATA;

    /*
     * In the prefix case we can allocate a much smaller buffer. Otherwise we
     * just allocate the default buffer size
     */
    if (!tls_setup_write_buffer(rl, numtempl + *prefix,
            *prefix ? MAX_PREFIX_LEN : 0, 0)) {
        /* RLAYERfatal() already called */
        return 0;
    }

    return 1;
}

int tls1_initialise_write_packets(OSSL_RECORD_LAYER *rl,
    OSSL_RECORD_TEMPLATE *templates,
    size_t numtempl,
    OSSL_RECORD_TEMPLATE *prefixtempl,
    WPACKET *pkt,
    TLS_BUFFER *bufs,
    size_t *wpinited)
{
    size_t align = 0;
    TLS_BUFFER *wb;
    size_t prefix;

    /* Do we need to add an empty record prefix? */
    prefix = rl->need_empty_fragments
        && templates[0].type == SSL3_RT_APPLICATION_DATA;

    if (prefix) {
        /*
         * countermeasure against known-IV weakness in CBC ciphersuites (see
         * http://www.openssl.org/~bodo/tls-cbc.txt)
         */
        prefixtempl->buf = NULL;
        prefixtempl->version = templates[0].version;
        prefixtempl->buflen = 0;
        prefixtempl->type = SSL3_RT_APPLICATION_DATA;

        wb = &bufs[0];
        align = tls_get_record_body_alignment_offset(rl, TLS_BUFFER_get_buf(wb));
        TLS_BUFFER_set_offset(wb, align);

        if (!WPACKET_init_static_len(&pkt[0], TLS_BUFFER_get_buf(wb),
                TLS_BUFFER_get_len(wb), 0)) {
            RLAYERfatal(rl, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return 0;
        }
        *wpinited = 1;
        if (!WPACKET_allocate_bytes(&pkt[0], align, NULL)) {
            RLAYERfatal(rl, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return 0;
        }
    }

    return tls_initialise_write_packets_default(rl, templates, numtempl,
        NULL,
        pkt + prefix, bufs + prefix,
        wpinited);
}

/* TLSv1.0, TLSv1.1 and TLSv1.2 all use the same funcs */
const struct record_functions_st tls_1_funcs = {
    tls1_set_crypto_state,
    tls1_cipher,
    tls1_mac,
    tls_default_set_protocol_version,
    tls_default_read_n,
    tls_get_more_records,
    tls_default_validate_record_header,
    tls_default_post_process_record,
    tls_get_max_records_multiblock,
    tls_write_records_multiblock, /* Defined in tls_multib.c */
    tls1_allocate_write_buffers,
    tls1_initialise_write_packets,
    NULL,
    tls_prepare_record_header_default,
    NULL,
    tls_prepare_for_encryption_default,
    tls_post_encryption_processing_default,
    NULL
};

const struct record_functions_st dtls_1_funcs = {
    tls1_set_crypto_state,
    tls1_cipher,
    tls1_mac,
    tls_default_set_protocol_version,
    tls_default_read_n,
    dtls_get_more_records,
    NULL,
    NULL,
    NULL,
    tls_write_records_default,
    /*
     * Don't use tls1_allocate_write_buffers since that handles empty fragment
     * records which aren't needed in DTLS. We just use the default allocation
     * instead.
     */
    tls_allocate_write_buffers_default,
    /* Don't use tls1_initialise_write_packets for same reason as above */
    tls_initialise_write_packets_default,
    NULL,
    dtls_prepare_record_header,
    NULL,
    tls_prepare_for_encryption_default,
    dtls_post_encryption_processing,
    NULL
};
