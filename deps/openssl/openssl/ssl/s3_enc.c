/*
 * Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* ====================================================================
 * Copyright 2005 Nokia. All rights reserved.
 *
 * The portions of the attached software ("Contribution") is developed by
 * Nokia Corporation and is licensed pursuant to the OpenSSL open source
 * license.
 *
 * The Contribution, originally written by Mika Kousa and Pasi Eronen of
 * Nokia Corporation, consists of the "PSK" (Pre-Shared Key) ciphersuites
 * support (see RFC 4279) to OpenSSL.
 *
 * No patent licenses or other rights except those expressly stated in
 * the OpenSSL open source license shall be deemed granted or received
 * expressly, by implication, estoppel, or otherwise.
 *
 * No assurances are provided by Nokia that the Contribution does not
 * infringe the patent or other intellectual property rights of any third
 * party or that the license provides you with all the necessary rights
 * to make use of the Contribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND. IN
 * ADDITION TO THE DISCLAIMERS INCLUDED IN THE LICENSE, NOKIA
 * SPECIFICALLY DISCLAIMS ANY LIABILITY FOR CLAIMS BROUGHT BY YOU OR ANY
 * OTHER ENTITY BASED ON INFRINGEMENT OF INTELLECTUAL PROPERTY RIGHTS OR
 * OTHERWISE.
 */

#include <stdio.h>
#include "ssl_locl.h"
#include <openssl/evp.h>
#include <openssl/md5.h>

static int ssl3_generate_key_block(SSL *s, unsigned char *km, int num)
{
    EVP_MD_CTX *m5;
    EVP_MD_CTX *s1;
    unsigned char buf[16], smd[SHA_DIGEST_LENGTH];
    unsigned char c = 'A';
    unsigned int i, j, k;
    int ret = 0;

#ifdef CHARSET_EBCDIC
    c = os_toascii[c];          /* 'A' in ASCII */
#endif
    k = 0;
    m5 = EVP_MD_CTX_new();
    s1 = EVP_MD_CTX_new();
    if (m5 == NULL || s1 == NULL) {
        SSLerr(SSL_F_SSL3_GENERATE_KEY_BLOCK, ERR_R_MALLOC_FAILURE);
        goto err;
    }
    EVP_MD_CTX_set_flags(m5, EVP_MD_CTX_FLAG_NON_FIPS_ALLOW);
    for (i = 0; (int)i < num; i += MD5_DIGEST_LENGTH) {
        k++;
        if (k > sizeof(buf)) {
            /* bug: 'buf' is too small for this ciphersuite */
            SSLerr(SSL_F_SSL3_GENERATE_KEY_BLOCK, ERR_R_INTERNAL_ERROR);
            goto err;
        }

        for (j = 0; j < k; j++)
            buf[j] = c;
        c++;
        if (!EVP_DigestInit_ex(s1, EVP_sha1(), NULL)
            || !EVP_DigestUpdate(s1, buf, k)
            || !EVP_DigestUpdate(s1, s->session->master_key,
                                 s->session->master_key_length)
            || !EVP_DigestUpdate(s1, s->s3->server_random, SSL3_RANDOM_SIZE)
            || !EVP_DigestUpdate(s1, s->s3->client_random, SSL3_RANDOM_SIZE)
            || !EVP_DigestFinal_ex(s1, smd, NULL)
            || !EVP_DigestInit_ex(m5, EVP_md5(), NULL)
            || !EVP_DigestUpdate(m5, s->session->master_key,
                                 s->session->master_key_length)
            || !EVP_DigestUpdate(m5, smd, SHA_DIGEST_LENGTH))
            goto err;
        if ((int)(i + MD5_DIGEST_LENGTH) > num) {
            if (!EVP_DigestFinal_ex(m5, smd, NULL))
                goto err;
            memcpy(km, smd, (num - i));
        } else {
            if (!EVP_DigestFinal_ex(m5, km, NULL))
                goto err;
        }

        km += MD5_DIGEST_LENGTH;
    }
    OPENSSL_cleanse(smd, sizeof(smd));
    ret = 1;
 err:
    EVP_MD_CTX_free(m5);
    EVP_MD_CTX_free(s1);
    return ret;
}

int ssl3_change_cipher_state(SSL *s, int which)
{
    unsigned char *p, *mac_secret;
    unsigned char exp_key[EVP_MAX_KEY_LENGTH];
    unsigned char exp_iv[EVP_MAX_IV_LENGTH];
    unsigned char *ms, *key, *iv;
    EVP_CIPHER_CTX *dd;
    const EVP_CIPHER *c;
#ifndef OPENSSL_NO_COMP
    COMP_METHOD *comp;
#endif
    const EVP_MD *m;
    int n, i, j, k, cl;
    int reuse_dd = 0;

    c = s->s3->tmp.new_sym_enc;
    m = s->s3->tmp.new_hash;
    /* m == NULL will lead to a crash later */
    OPENSSL_assert(m);
#ifndef OPENSSL_NO_COMP
    if (s->s3->tmp.new_compression == NULL)
        comp = NULL;
    else
        comp = s->s3->tmp.new_compression->method;
#endif

    if (which & SSL3_CC_READ) {
        if (s->enc_read_ctx != NULL)
            reuse_dd = 1;
        else if ((s->enc_read_ctx = EVP_CIPHER_CTX_new()) == NULL)
            goto err;
        else
            /*
             * make sure it's initialised in case we exit later with an error
             */
            EVP_CIPHER_CTX_reset(s->enc_read_ctx);
        dd = s->enc_read_ctx;

        if (ssl_replace_hash(&s->read_hash, m) == NULL) {
            SSLerr(SSL_F_SSL3_CHANGE_CIPHER_STATE, ERR_R_INTERNAL_ERROR);
            goto err2;
        }
#ifndef OPENSSL_NO_COMP
        /* COMPRESS */
        COMP_CTX_free(s->expand);
        s->expand = NULL;
        if (comp != NULL) {
            s->expand = COMP_CTX_new(comp);
            if (s->expand == NULL) {
                SSLerr(SSL_F_SSL3_CHANGE_CIPHER_STATE,
                       SSL_R_COMPRESSION_LIBRARY_ERROR);
                goto err2;
            }
        }
#endif
        RECORD_LAYER_reset_read_sequence(&s->rlayer);
        mac_secret = &(s->s3->read_mac_secret[0]);
    } else {
        if (s->enc_write_ctx != NULL)
            reuse_dd = 1;
        else if ((s->enc_write_ctx = EVP_CIPHER_CTX_new()) == NULL)
            goto err;
        else
            /*
             * make sure it's initialised in case we exit later with an error
             */
            EVP_CIPHER_CTX_reset(s->enc_write_ctx);
        dd = s->enc_write_ctx;
        if (ssl_replace_hash(&s->write_hash, m) == NULL) {
            SSLerr(SSL_F_SSL3_CHANGE_CIPHER_STATE, ERR_R_INTERNAL_ERROR);
            goto err2;
        }
#ifndef OPENSSL_NO_COMP
        /* COMPRESS */
        COMP_CTX_free(s->compress);
        s->compress = NULL;
        if (comp != NULL) {
            s->compress = COMP_CTX_new(comp);
            if (s->compress == NULL) {
                SSLerr(SSL_F_SSL3_CHANGE_CIPHER_STATE,
                       SSL_R_COMPRESSION_LIBRARY_ERROR);
                goto err2;
            }
        }
#endif
        RECORD_LAYER_reset_write_sequence(&s->rlayer);
        mac_secret = &(s->s3->write_mac_secret[0]);
    }

    if (reuse_dd)
        EVP_CIPHER_CTX_reset(dd);

    p = s->s3->tmp.key_block;
    i = EVP_MD_size(m);
    if (i < 0)
        goto err2;
    cl = EVP_CIPHER_key_length(c);
    j = cl;
    k = EVP_CIPHER_iv_length(c);
    if ((which == SSL3_CHANGE_CIPHER_CLIENT_WRITE) ||
        (which == SSL3_CHANGE_CIPHER_SERVER_READ)) {
        ms = &(p[0]);
        n = i + i;
        key = &(p[n]);
        n += j + j;
        iv = &(p[n]);
        n += k + k;
    } else {
        n = i;
        ms = &(p[n]);
        n += i + j;
        key = &(p[n]);
        n += j + k;
        iv = &(p[n]);
        n += k;
    }

    if (n > s->s3->tmp.key_block_length) {
        SSLerr(SSL_F_SSL3_CHANGE_CIPHER_STATE, ERR_R_INTERNAL_ERROR);
        goto err2;
    }

    memcpy(mac_secret, ms, i);

    if (!EVP_CipherInit_ex(dd, c, NULL, key, iv, (which & SSL3_CC_WRITE)))
        goto err2;

    OPENSSL_cleanse(exp_key, sizeof(exp_key));
    OPENSSL_cleanse(exp_iv, sizeof(exp_iv));
    return (1);
 err:
    SSLerr(SSL_F_SSL3_CHANGE_CIPHER_STATE, ERR_R_MALLOC_FAILURE);
 err2:
    OPENSSL_cleanse(exp_key, sizeof(exp_key));
    OPENSSL_cleanse(exp_iv, sizeof(exp_iv));
    return (0);
}

int ssl3_setup_key_block(SSL *s)
{
    unsigned char *p;
    const EVP_CIPHER *c;
    const EVP_MD *hash;
    int num;
    int ret = 0;
    SSL_COMP *comp;

    if (s->s3->tmp.key_block_length != 0)
        return (1);

    if (!ssl_cipher_get_evp(s->session, &c, &hash, NULL, NULL, &comp, 0)) {
        SSLerr(SSL_F_SSL3_SETUP_KEY_BLOCK, SSL_R_CIPHER_OR_HASH_UNAVAILABLE);
        return (0);
    }

    s->s3->tmp.new_sym_enc = c;
    s->s3->tmp.new_hash = hash;
#ifdef OPENSSL_NO_COMP
    s->s3->tmp.new_compression = NULL;
#else
    s->s3->tmp.new_compression = comp;
#endif

    num = EVP_MD_size(hash);
    if (num < 0)
        return 0;

    num = EVP_CIPHER_key_length(c) + num + EVP_CIPHER_iv_length(c);
    num *= 2;

    ssl3_cleanup_key_block(s);

    if ((p = OPENSSL_malloc(num)) == NULL)
        goto err;

    s->s3->tmp.key_block_length = num;
    s->s3->tmp.key_block = p;

    ret = ssl3_generate_key_block(s, p, num);

    if (!(s->options & SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS)) {
        /*
         * enable vulnerability countermeasure for CBC ciphers with known-IV
         * problem (http://www.openssl.org/~bodo/tls-cbc.txt)
         */
        s->s3->need_empty_fragments = 1;

        if (s->session->cipher != NULL) {
            if (s->session->cipher->algorithm_enc == SSL_eNULL)
                s->s3->need_empty_fragments = 0;

#ifndef OPENSSL_NO_RC4
            if (s->session->cipher->algorithm_enc == SSL_RC4)
                s->s3->need_empty_fragments = 0;
#endif
        }
    }

    return ret;

 err:
    SSLerr(SSL_F_SSL3_SETUP_KEY_BLOCK, ERR_R_MALLOC_FAILURE);
    return (0);
}

void ssl3_cleanup_key_block(SSL *s)
{
    OPENSSL_clear_free(s->s3->tmp.key_block, s->s3->tmp.key_block_length);
    s->s3->tmp.key_block = NULL;
    s->s3->tmp.key_block_length = 0;
}

int ssl3_init_finished_mac(SSL *s)
{
    BIO *buf = BIO_new(BIO_s_mem());

    if (buf == NULL) {
        SSLerr(SSL_F_SSL3_INIT_FINISHED_MAC, ERR_R_MALLOC_FAILURE);
        return 0;
    }
    ssl3_free_digest_list(s);
    s->s3->handshake_buffer = buf;
    (void)BIO_set_close(s->s3->handshake_buffer, BIO_CLOSE);
    return 1;
}

/*
 * Free digest list. Also frees handshake buffer since they are always freed
 * together.
 */

void ssl3_free_digest_list(SSL *s)
{
    BIO_free(s->s3->handshake_buffer);
    s->s3->handshake_buffer = NULL;
    EVP_MD_CTX_free(s->s3->handshake_dgst);
    s->s3->handshake_dgst = NULL;
}

int ssl3_finish_mac(SSL *s, const unsigned char *buf, int len)
{
    if (s->s3->handshake_dgst == NULL)
        /* Note: this writes to a memory BIO so a failure is a fatal error */
        return BIO_write(s->s3->handshake_buffer, (void *)buf, len) == len;
    else
        return EVP_DigestUpdate(s->s3->handshake_dgst, buf, len);
}

int ssl3_digest_cached_records(SSL *s, int keep)
{
    const EVP_MD *md;
    long hdatalen;
    void *hdata;

    if (s->s3->handshake_dgst == NULL) {
        hdatalen = BIO_get_mem_data(s->s3->handshake_buffer, &hdata);
        if (hdatalen <= 0) {
            SSLerr(SSL_F_SSL3_DIGEST_CACHED_RECORDS,
                   SSL_R_BAD_HANDSHAKE_LENGTH);
            return 0;
        }

        s->s3->handshake_dgst = EVP_MD_CTX_new();
        if (s->s3->handshake_dgst == NULL) {
            SSLerr(SSL_F_SSL3_DIGEST_CACHED_RECORDS, ERR_R_MALLOC_FAILURE);
            return 0;
        }

        md = ssl_handshake_md(s);
        if (md == NULL || !EVP_DigestInit_ex(s->s3->handshake_dgst, md, NULL)
            || !EVP_DigestUpdate(s->s3->handshake_dgst, hdata, hdatalen)) {
            SSLerr(SSL_F_SSL3_DIGEST_CACHED_RECORDS, ERR_R_INTERNAL_ERROR);
            return 0;
        }
    }
    if (keep == 0) {
        BIO_free(s->s3->handshake_buffer);
        s->s3->handshake_buffer = NULL;
    }

    return 1;
}

int ssl3_final_finish_mac(SSL *s, const char *sender, int len, unsigned char *p)
{
    int ret;
    EVP_MD_CTX *ctx = NULL;

    if (!ssl3_digest_cached_records(s, 0))
        return 0;

    if (EVP_MD_CTX_type(s->s3->handshake_dgst) != NID_md5_sha1) {
        SSLerr(SSL_F_SSL3_FINAL_FINISH_MAC, SSL_R_NO_REQUIRED_DIGEST);
        return 0;
    }

    ctx = EVP_MD_CTX_new();
    if (ctx == NULL) {
        SSLerr(SSL_F_SSL3_FINAL_FINISH_MAC, ERR_R_MALLOC_FAILURE);
        return 0;
    }
    if (!EVP_MD_CTX_copy_ex(ctx, s->s3->handshake_dgst)) {
        SSLerr(SSL_F_SSL3_FINAL_FINISH_MAC, ERR_R_INTERNAL_ERROR);
        return 0;
    }

    ret = EVP_MD_CTX_size(ctx);
    if (ret < 0) {
        EVP_MD_CTX_reset(ctx);
        return 0;
    }

    if ((sender != NULL && EVP_DigestUpdate(ctx, sender, len) <= 0)
        || EVP_MD_CTX_ctrl(ctx, EVP_CTRL_SSL3_MASTER_SECRET,
                           s->session->master_key_length,
                           s->session->master_key) <= 0
        || EVP_DigestFinal_ex(ctx, p, NULL) <= 0) {
        SSLerr(SSL_F_SSL3_FINAL_FINISH_MAC, ERR_R_INTERNAL_ERROR);
        ret = 0;
    }

    EVP_MD_CTX_free(ctx);

    return ret;
}

int ssl3_generate_master_secret(SSL *s, unsigned char *out, unsigned char *p,
                                int len)
{
    static const unsigned char *salt[3] = {
#ifndef CHARSET_EBCDIC
        (const unsigned char *)"A",
        (const unsigned char *)"BB",
        (const unsigned char *)"CCC",
#else
        (const unsigned char *)"\x41",
        (const unsigned char *)"\x42\x42",
        (const unsigned char *)"\x43\x43\x43",
#endif
    };
    unsigned char buf[EVP_MAX_MD_SIZE];
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    int i, ret = 0;
    unsigned int n;

    if (ctx == NULL) {
        SSLerr(SSL_F_SSL3_GENERATE_MASTER_SECRET, ERR_R_MALLOC_FAILURE);
        return 0;
    }
    for (i = 0; i < 3; i++) {
        if (EVP_DigestInit_ex(ctx, s->ctx->sha1, NULL) <= 0
            || EVP_DigestUpdate(ctx, salt[i],
                                strlen((const char *)salt[i])) <= 0
            || EVP_DigestUpdate(ctx, p, len) <= 0
            || EVP_DigestUpdate(ctx, &(s->s3->client_random[0]),
                                SSL3_RANDOM_SIZE) <= 0
            || EVP_DigestUpdate(ctx, &(s->s3->server_random[0]),
                                SSL3_RANDOM_SIZE) <= 0
            || EVP_DigestFinal_ex(ctx, buf, &n) <= 0
            || EVP_DigestInit_ex(ctx, s->ctx->md5, NULL) <= 0
            || EVP_DigestUpdate(ctx, p, len) <= 0
            || EVP_DigestUpdate(ctx, buf, n) <= 0
            || EVP_DigestFinal_ex(ctx, out, &n) <= 0) {
            SSLerr(SSL_F_SSL3_GENERATE_MASTER_SECRET, ERR_R_INTERNAL_ERROR);
            ret = 0;
            break;
        }
        out += n;
        ret += n;
    }
    EVP_MD_CTX_free(ctx);

    OPENSSL_cleanse(buf, sizeof(buf));
    return (ret);
}

int ssl3_alert_code(int code)
{
    switch (code) {
    case SSL_AD_CLOSE_NOTIFY:
        return (SSL3_AD_CLOSE_NOTIFY);
    case SSL_AD_UNEXPECTED_MESSAGE:
        return (SSL3_AD_UNEXPECTED_MESSAGE);
    case SSL_AD_BAD_RECORD_MAC:
        return (SSL3_AD_BAD_RECORD_MAC);
    case SSL_AD_DECRYPTION_FAILED:
        return (SSL3_AD_BAD_RECORD_MAC);
    case SSL_AD_RECORD_OVERFLOW:
        return (SSL3_AD_BAD_RECORD_MAC);
    case SSL_AD_DECOMPRESSION_FAILURE:
        return (SSL3_AD_DECOMPRESSION_FAILURE);
    case SSL_AD_HANDSHAKE_FAILURE:
        return (SSL3_AD_HANDSHAKE_FAILURE);
    case SSL_AD_NO_CERTIFICATE:
        return (SSL3_AD_NO_CERTIFICATE);
    case SSL_AD_BAD_CERTIFICATE:
        return (SSL3_AD_BAD_CERTIFICATE);
    case SSL_AD_UNSUPPORTED_CERTIFICATE:
        return (SSL3_AD_UNSUPPORTED_CERTIFICATE);
    case SSL_AD_CERTIFICATE_REVOKED:
        return (SSL3_AD_CERTIFICATE_REVOKED);
    case SSL_AD_CERTIFICATE_EXPIRED:
        return (SSL3_AD_CERTIFICATE_EXPIRED);
    case SSL_AD_CERTIFICATE_UNKNOWN:
        return (SSL3_AD_CERTIFICATE_UNKNOWN);
    case SSL_AD_ILLEGAL_PARAMETER:
        return (SSL3_AD_ILLEGAL_PARAMETER);
    case SSL_AD_UNKNOWN_CA:
        return (SSL3_AD_BAD_CERTIFICATE);
    case SSL_AD_ACCESS_DENIED:
        return (SSL3_AD_HANDSHAKE_FAILURE);
    case SSL_AD_DECODE_ERROR:
        return (SSL3_AD_HANDSHAKE_FAILURE);
    case SSL_AD_DECRYPT_ERROR:
        return (SSL3_AD_HANDSHAKE_FAILURE);
    case SSL_AD_EXPORT_RESTRICTION:
        return (SSL3_AD_HANDSHAKE_FAILURE);
    case SSL_AD_PROTOCOL_VERSION:
        return (SSL3_AD_HANDSHAKE_FAILURE);
    case SSL_AD_INSUFFICIENT_SECURITY:
        return (SSL3_AD_HANDSHAKE_FAILURE);
    case SSL_AD_INTERNAL_ERROR:
        return (SSL3_AD_HANDSHAKE_FAILURE);
    case SSL_AD_USER_CANCELLED:
        return (SSL3_AD_HANDSHAKE_FAILURE);
    case SSL_AD_NO_RENEGOTIATION:
        return (-1);            /* Don't send it :-) */
    case SSL_AD_UNSUPPORTED_EXTENSION:
        return (SSL3_AD_HANDSHAKE_FAILURE);
    case SSL_AD_CERTIFICATE_UNOBTAINABLE:
        return (SSL3_AD_HANDSHAKE_FAILURE);
    case SSL_AD_UNRECOGNIZED_NAME:
        return (SSL3_AD_HANDSHAKE_FAILURE);
    case SSL_AD_BAD_CERTIFICATE_STATUS_RESPONSE:
        return (SSL3_AD_HANDSHAKE_FAILURE);
    case SSL_AD_BAD_CERTIFICATE_HASH_VALUE:
        return (SSL3_AD_HANDSHAKE_FAILURE);
    case SSL_AD_UNKNOWN_PSK_IDENTITY:
        return (TLS1_AD_UNKNOWN_PSK_IDENTITY);
    case SSL_AD_INAPPROPRIATE_FALLBACK:
        return (TLS1_AD_INAPPROPRIATE_FALLBACK);
    case SSL_AD_NO_APPLICATION_PROTOCOL:
        return (TLS1_AD_NO_APPLICATION_PROTOCOL);
    default:
        return (-1);
    }
}
