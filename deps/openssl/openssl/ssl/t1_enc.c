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
#include <openssl/comp.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>

/* seed1 through seed5 are concatenated */
static int tls1_PRF(SSL *s,
                    const void *seed1, int seed1_len,
                    const void *seed2, int seed2_len,
                    const void *seed3, int seed3_len,
                    const void *seed4, int seed4_len,
                    const void *seed5, int seed5_len,
                    const unsigned char *sec, int slen,
                    unsigned char *out, int olen)
{
    const EVP_MD *md = ssl_prf_md(s);
    EVP_PKEY_CTX *pctx = NULL;

    int ret = 0;
    size_t outlen = olen;

    if (md == NULL) {
        /* Should never happen */
        SSLerr(SSL_F_TLS1_PRF, ERR_R_INTERNAL_ERROR);
        return 0;
    }
    pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_TLS1_PRF, NULL);
    if (pctx == NULL || EVP_PKEY_derive_init(pctx) <= 0
        || EVP_PKEY_CTX_set_tls1_prf_md(pctx, md) <= 0
        || EVP_PKEY_CTX_set1_tls1_prf_secret(pctx, sec, slen) <= 0)
        goto err;

    if (EVP_PKEY_CTX_add1_tls1_prf_seed(pctx, seed1, seed1_len) <= 0)
        goto err;
    if (EVP_PKEY_CTX_add1_tls1_prf_seed(pctx, seed2, seed2_len) <= 0)
        goto err;
    if (EVP_PKEY_CTX_add1_tls1_prf_seed(pctx, seed3, seed3_len) <= 0)
        goto err;
    if (EVP_PKEY_CTX_add1_tls1_prf_seed(pctx, seed4, seed4_len) <= 0)
        goto err;
    if (EVP_PKEY_CTX_add1_tls1_prf_seed(pctx, seed5, seed5_len) <= 0)
        goto err;

    if (EVP_PKEY_derive(pctx, out, &outlen) <= 0)
        goto err;
    ret = 1;

 err:
    EVP_PKEY_CTX_free(pctx);
    return ret;
}

static int tls1_generate_key_block(SSL *s, unsigned char *km, int num)
{
    int ret;
    ret = tls1_PRF(s,
                   TLS_MD_KEY_EXPANSION_CONST,
                   TLS_MD_KEY_EXPANSION_CONST_SIZE, s->s3->server_random,
                   SSL3_RANDOM_SIZE, s->s3->client_random, SSL3_RANDOM_SIZE,
                   NULL, 0, NULL, 0, s->session->master_key,
                   s->session->master_key_length, km, num);

    return ret;
}

int tls1_change_cipher_state(SSL *s, int which)
{
    unsigned char *p, *mac_secret;
    unsigned char tmp1[EVP_MAX_KEY_LENGTH];
    unsigned char tmp2[EVP_MAX_KEY_LENGTH];
    unsigned char iv1[EVP_MAX_IV_LENGTH * 2];
    unsigned char iv2[EVP_MAX_IV_LENGTH * 2];
    unsigned char *ms, *key, *iv;
    EVP_CIPHER_CTX *dd;
    const EVP_CIPHER *c;
#ifndef OPENSSL_NO_COMP
    const SSL_COMP *comp;
#endif
    const EVP_MD *m;
    int mac_type;
    int *mac_secret_size;
    EVP_MD_CTX *mac_ctx;
    EVP_PKEY *mac_key;
    int n, i, j, k, cl;
    int reuse_dd = 0;

    c = s->s3->tmp.new_sym_enc;
    m = s->s3->tmp.new_hash;
    mac_type = s->s3->tmp.new_mac_pkey_type;
#ifndef OPENSSL_NO_COMP
    comp = s->s3->tmp.new_compression;
#endif

    if (which & SSL3_CC_READ) {
        if (s->tlsext_use_etm)
            s->s3->flags |= TLS1_FLAGS_ENCRYPT_THEN_MAC_READ;
        else
            s->s3->flags &= ~TLS1_FLAGS_ENCRYPT_THEN_MAC_READ;

        if (s->s3->tmp.new_cipher->algorithm2 & TLS1_STREAM_MAC)
            s->mac_flags |= SSL_MAC_FLAG_READ_MAC_STREAM;
        else
            s->mac_flags &= ~SSL_MAC_FLAG_READ_MAC_STREAM;

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
        mac_ctx = ssl_replace_hash(&s->read_hash, NULL);
        if (mac_ctx == NULL)
            goto err;
#ifndef OPENSSL_NO_COMP
        COMP_CTX_free(s->expand);
        s->expand = NULL;
        if (comp != NULL) {
            s->expand = COMP_CTX_new(comp->method);
            if (s->expand == NULL) {
                SSLerr(SSL_F_TLS1_CHANGE_CIPHER_STATE,
                       SSL_R_COMPRESSION_LIBRARY_ERROR);
                goto err2;
            }
        }
#endif
        /*
         * this is done by dtls1_reset_seq_numbers for DTLS
         */
        if (!SSL_IS_DTLS(s))
            RECORD_LAYER_reset_read_sequence(&s->rlayer);
        mac_secret = &(s->s3->read_mac_secret[0]);
        mac_secret_size = &(s->s3->read_mac_secret_size);
    } else {
        if (s->tlsext_use_etm)
            s->s3->flags |= TLS1_FLAGS_ENCRYPT_THEN_MAC_WRITE;
        else
            s->s3->flags &= ~TLS1_FLAGS_ENCRYPT_THEN_MAC_WRITE;

        if (s->s3->tmp.new_cipher->algorithm2 & TLS1_STREAM_MAC)
            s->mac_flags |= SSL_MAC_FLAG_WRITE_MAC_STREAM;
        else
            s->mac_flags &= ~SSL_MAC_FLAG_WRITE_MAC_STREAM;
        if (s->enc_write_ctx != NULL && !SSL_IS_DTLS(s))
            reuse_dd = 1;
        else if ((s->enc_write_ctx = EVP_CIPHER_CTX_new()) == NULL)
            goto err;
        dd = s->enc_write_ctx;
        if (SSL_IS_DTLS(s)) {
            mac_ctx = EVP_MD_CTX_new();
            if (mac_ctx == NULL)
                goto err;
            s->write_hash = mac_ctx;
        } else {
            mac_ctx = ssl_replace_hash(&s->write_hash, NULL);
            if (mac_ctx == NULL)
                goto err;
        }
#ifndef OPENSSL_NO_COMP
        COMP_CTX_free(s->compress);
        s->compress = NULL;
        if (comp != NULL) {
            s->compress = COMP_CTX_new(comp->method);
            if (s->compress == NULL) {
                SSLerr(SSL_F_TLS1_CHANGE_CIPHER_STATE,
                       SSL_R_COMPRESSION_LIBRARY_ERROR);
                goto err2;
            }
        }
#endif
        /*
         * this is done by dtls1_reset_seq_numbers for DTLS
         */
        if (!SSL_IS_DTLS(s))
            RECORD_LAYER_reset_write_sequence(&s->rlayer);
        mac_secret = &(s->s3->write_mac_secret[0]);
        mac_secret_size = &(s->s3->write_mac_secret_size);
    }

    if (reuse_dd)
        EVP_CIPHER_CTX_reset(dd);

    p = s->s3->tmp.key_block;
    i = *mac_secret_size = s->s3->tmp.new_mac_secret_size;

    cl = EVP_CIPHER_key_length(c);
    j = cl;
    /* Was j=(exp)?5:EVP_CIPHER_key_length(c); */
    /* If GCM/CCM mode only part of IV comes from PRF */
    if (EVP_CIPHER_mode(c) == EVP_CIPH_GCM_MODE)
        k = EVP_GCM_TLS_FIXED_IV_LEN;
    else if (EVP_CIPHER_mode(c) == EVP_CIPH_CCM_MODE)
        k = EVP_CCM_TLS_FIXED_IV_LEN;
    else
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
        SSLerr(SSL_F_TLS1_CHANGE_CIPHER_STATE, ERR_R_INTERNAL_ERROR);
        goto err2;
    }

    memcpy(mac_secret, ms, i);

    if (!(EVP_CIPHER_flags(c) & EVP_CIPH_FLAG_AEAD_CIPHER)) {
        mac_key = EVP_PKEY_new_mac_key(mac_type, NULL,
                                       mac_secret, *mac_secret_size);
        if (mac_key == NULL
            || EVP_DigestSignInit(mac_ctx, NULL, m, NULL, mac_key) <= 0) {
            EVP_PKEY_free(mac_key);
            SSLerr(SSL_F_TLS1_CHANGE_CIPHER_STATE, ERR_R_INTERNAL_ERROR);
            goto err2;
        }
        EVP_PKEY_free(mac_key);
    }
#ifdef SSL_DEBUG
    printf("which = %04X\nmac key=", which);
    {
        int z;
        for (z = 0; z < i; z++)
            printf("%02X%c", ms[z], ((z + 1) % 16) ? ' ' : '\n');
    }
#endif

    if (EVP_CIPHER_mode(c) == EVP_CIPH_GCM_MODE) {
        if (!EVP_CipherInit_ex(dd, c, NULL, key, NULL, (which & SSL3_CC_WRITE))
            || !EVP_CIPHER_CTX_ctrl(dd, EVP_CTRL_GCM_SET_IV_FIXED, k, iv)) {
            SSLerr(SSL_F_TLS1_CHANGE_CIPHER_STATE, ERR_R_INTERNAL_ERROR);
            goto err2;
        }
    } else if (EVP_CIPHER_mode(c) == EVP_CIPH_CCM_MODE) {
        int taglen;
        if (s->s3->tmp.
            new_cipher->algorithm_enc & (SSL_AES128CCM8 | SSL_AES256CCM8))
            taglen = 8;
        else
            taglen = 16;
        if (!EVP_CipherInit_ex(dd, c, NULL, NULL, NULL, (which & SSL3_CC_WRITE))
            || !EVP_CIPHER_CTX_ctrl(dd, EVP_CTRL_AEAD_SET_IVLEN, 12, NULL)
            || !EVP_CIPHER_CTX_ctrl(dd, EVP_CTRL_AEAD_SET_TAG, taglen, NULL)
            || !EVP_CIPHER_CTX_ctrl(dd, EVP_CTRL_CCM_SET_IV_FIXED, k, iv)
            || !EVP_CipherInit_ex(dd, NULL, NULL, key, NULL, -1)) {
            SSLerr(SSL_F_TLS1_CHANGE_CIPHER_STATE, ERR_R_INTERNAL_ERROR);
            goto err2;
        }
    } else {
        if (!EVP_CipherInit_ex(dd, c, NULL, key, iv, (which & SSL3_CC_WRITE))) {
            SSLerr(SSL_F_TLS1_CHANGE_CIPHER_STATE, ERR_R_INTERNAL_ERROR);
            goto err2;
        }
    }
    /* Needed for "composite" AEADs, such as RC4-HMAC-MD5 */
    if ((EVP_CIPHER_flags(c) & EVP_CIPH_FLAG_AEAD_CIPHER) && *mac_secret_size
        && !EVP_CIPHER_CTX_ctrl(dd, EVP_CTRL_AEAD_SET_MAC_KEY,
                                *mac_secret_size, mac_secret)) {
        SSLerr(SSL_F_TLS1_CHANGE_CIPHER_STATE, ERR_R_INTERNAL_ERROR);
        goto err2;
    }

#ifdef SSL_DEBUG
    printf("which = %04X\nkey=", which);
    {
        int z;
        for (z = 0; z < EVP_CIPHER_key_length(c); z++)
            printf("%02X%c", key[z], ((z + 1) % 16) ? ' ' : '\n');
    }
    printf("\niv=");
    {
        int z;
        for (z = 0; z < k; z++)
            printf("%02X%c", iv[z], ((z + 1) % 16) ? ' ' : '\n');
    }
    printf("\n");
#endif

    OPENSSL_cleanse(tmp1, sizeof(tmp1));
    OPENSSL_cleanse(tmp2, sizeof(tmp1));
    OPENSSL_cleanse(iv1, sizeof(iv1));
    OPENSSL_cleanse(iv2, sizeof(iv2));
    return (1);
 err:
    SSLerr(SSL_F_TLS1_CHANGE_CIPHER_STATE, ERR_R_MALLOC_FAILURE);
 err2:
    OPENSSL_cleanse(tmp1, sizeof(tmp1));
    OPENSSL_cleanse(tmp2, sizeof(tmp1));
    OPENSSL_cleanse(iv1, sizeof(iv1));
    OPENSSL_cleanse(iv2, sizeof(iv2));
    return (0);
}

int tls1_setup_key_block(SSL *s)
{
    unsigned char *p;
    const EVP_CIPHER *c;
    const EVP_MD *hash;
    int num;
    SSL_COMP *comp;
    int mac_type = NID_undef, mac_secret_size = 0;
    int ret = 0;

    if (s->s3->tmp.key_block_length != 0)
        return (1);

    if (!ssl_cipher_get_evp(s->session, &c, &hash, &mac_type, &mac_secret_size,
                            &comp, s->tlsext_use_etm)) {
        SSLerr(SSL_F_TLS1_SETUP_KEY_BLOCK, SSL_R_CIPHER_OR_HASH_UNAVAILABLE);
        return (0);
    }

    s->s3->tmp.new_sym_enc = c;
    s->s3->tmp.new_hash = hash;
    s->s3->tmp.new_mac_pkey_type = mac_type;
    s->s3->tmp.new_mac_secret_size = mac_secret_size;
    num = EVP_CIPHER_key_length(c) + mac_secret_size + EVP_CIPHER_iv_length(c);
    num *= 2;

    ssl3_cleanup_key_block(s);

    if ((p = OPENSSL_malloc(num)) == NULL) {
        SSLerr(SSL_F_TLS1_SETUP_KEY_BLOCK, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    s->s3->tmp.key_block_length = num;
    s->s3->tmp.key_block = p;

#ifdef SSL_DEBUG
    printf("client random\n");
    {
        int z;
        for (z = 0; z < SSL3_RANDOM_SIZE; z++)
            printf("%02X%c", s->s3->client_random[z],
                   ((z + 1) % 16) ? ' ' : '\n');
    }
    printf("server random\n");
    {
        int z;
        for (z = 0; z < SSL3_RANDOM_SIZE; z++)
            printf("%02X%c", s->s3->server_random[z],
                   ((z + 1) % 16) ? ' ' : '\n');
    }
    printf("master key\n");
    {
        int z;
        for (z = 0; z < s->session->master_key_length; z++)
            printf("%02X%c", s->session->master_key[z],
                   ((z + 1) % 16) ? ' ' : '\n');
    }
#endif
    if (!tls1_generate_key_block(s, p, num))
        goto err;
#ifdef SSL_DEBUG
    printf("\nkey block\n");
    {
        int z;
        for (z = 0; z < num; z++)
            printf("%02X%c", p[z], ((z + 1) % 16) ? ' ' : '\n');
    }
#endif

    if (!(s->options & SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS)
        && s->method->version <= TLS1_VERSION) {
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

    ret = 1;
 err:
    return (ret);
}

int tls1_final_finish_mac(SSL *s, const char *str, int slen, unsigned char *out)
{
    int hashlen;
    unsigned char hash[EVP_MAX_MD_SIZE];

    if (!ssl3_digest_cached_records(s, 0))
        return 0;

    hashlen = ssl_handshake_hash(s, hash, sizeof(hash));

    if (hashlen == 0)
        return 0;

    if (!tls1_PRF(s, str, slen, hash, hashlen, NULL, 0, NULL, 0, NULL, 0,
                  s->session->master_key, s->session->master_key_length,
                  out, TLS1_FINISH_MAC_LENGTH))
        return 0;
    OPENSSL_cleanse(hash, hashlen);
    return TLS1_FINISH_MAC_LENGTH;
}

int tls1_generate_master_secret(SSL *s, unsigned char *out, unsigned char *p,
                                int len)
{
    if (s->session->flags & SSL_SESS_FLAG_EXTMS) {
        unsigned char hash[EVP_MAX_MD_SIZE * 2];
        int hashlen;
        /*
         * Digest cached records keeping record buffer (if present): this wont
         * affect client auth because we're freezing the buffer at the same
         * point (after client key exchange and before certificate verify)
         */
        if (!ssl3_digest_cached_records(s, 1))
            return -1;
        hashlen = ssl_handshake_hash(s, hash, sizeof(hash));
#ifdef SSL_DEBUG
        fprintf(stderr, "Handshake hashes:\n");
        BIO_dump_fp(stderr, (char *)hash, hashlen);
#endif
        tls1_PRF(s,
                 TLS_MD_EXTENDED_MASTER_SECRET_CONST,
                 TLS_MD_EXTENDED_MASTER_SECRET_CONST_SIZE,
                 hash, hashlen,
                 NULL, 0,
                 NULL, 0,
                 NULL, 0, p, len, s->session->master_key,
                 SSL3_MASTER_SECRET_SIZE);
        OPENSSL_cleanse(hash, hashlen);
    } else {
        tls1_PRF(s,
                 TLS_MD_MASTER_SECRET_CONST,
                 TLS_MD_MASTER_SECRET_CONST_SIZE,
                 s->s3->client_random, SSL3_RANDOM_SIZE,
                 NULL, 0,
                 s->s3->server_random, SSL3_RANDOM_SIZE,
                 NULL, 0, p, len, s->session->master_key,
                 SSL3_MASTER_SECRET_SIZE);
    }
#ifdef SSL_DEBUG
    fprintf(stderr, "Premaster Secret:\n");
    BIO_dump_fp(stderr, (char *)p, len);
    fprintf(stderr, "Client Random:\n");
    BIO_dump_fp(stderr, (char *)s->s3->client_random, SSL3_RANDOM_SIZE);
    fprintf(stderr, "Server Random:\n");
    BIO_dump_fp(stderr, (char *)s->s3->server_random, SSL3_RANDOM_SIZE);
    fprintf(stderr, "Master Secret:\n");
    BIO_dump_fp(stderr, (char *)s->session->master_key,
                SSL3_MASTER_SECRET_SIZE);
#endif

    return (SSL3_MASTER_SECRET_SIZE);
}

int tls1_export_keying_material(SSL *s, unsigned char *out, size_t olen,
                                const char *label, size_t llen,
                                const unsigned char *context,
                                size_t contextlen, int use_context)
{
    unsigned char *val = NULL;
    size_t vallen = 0, currentvalpos;
    int rv;

    /*
     * construct PRF arguments we construct the PRF argument ourself rather
     * than passing separate values into the TLS PRF to ensure that the
     * concatenation of values does not create a prohibited label.
     */
    vallen = llen + SSL3_RANDOM_SIZE * 2;
    if (use_context) {
        vallen += 2 + contextlen;
    }

    val = OPENSSL_malloc(vallen);
    if (val == NULL)
        goto err2;
    currentvalpos = 0;
    memcpy(val + currentvalpos, (unsigned char *)label, llen);
    currentvalpos += llen;
    memcpy(val + currentvalpos, s->s3->client_random, SSL3_RANDOM_SIZE);
    currentvalpos += SSL3_RANDOM_SIZE;
    memcpy(val + currentvalpos, s->s3->server_random, SSL3_RANDOM_SIZE);
    currentvalpos += SSL3_RANDOM_SIZE;

    if (use_context) {
        val[currentvalpos] = (contextlen >> 8) & 0xff;
        currentvalpos++;
        val[currentvalpos] = contextlen & 0xff;
        currentvalpos++;
        if ((contextlen > 0) || (context != NULL)) {
            memcpy(val + currentvalpos, context, contextlen);
        }
    }

    /*
     * disallow prohibited labels note that SSL3_RANDOM_SIZE > max(prohibited
     * label len) = 15, so size of val > max(prohibited label len) = 15 and
     * the comparisons won't have buffer overflow
     */
    if (memcmp(val, TLS_MD_CLIENT_FINISH_CONST,
               TLS_MD_CLIENT_FINISH_CONST_SIZE) == 0)
        goto err1;
    if (memcmp(val, TLS_MD_SERVER_FINISH_CONST,
               TLS_MD_SERVER_FINISH_CONST_SIZE) == 0)
        goto err1;
    if (memcmp(val, TLS_MD_MASTER_SECRET_CONST,
               TLS_MD_MASTER_SECRET_CONST_SIZE) == 0)
        goto err1;
    if (memcmp(val, TLS_MD_EXTENDED_MASTER_SECRET_CONST,
               TLS_MD_EXTENDED_MASTER_SECRET_CONST_SIZE) == 0)
        goto err1;
    if (memcmp(val, TLS_MD_KEY_EXPANSION_CONST,
               TLS_MD_KEY_EXPANSION_CONST_SIZE) == 0)
        goto err1;

    rv = tls1_PRF(s,
                  val, vallen,
                  NULL, 0,
                  NULL, 0,
                  NULL, 0,
                  NULL, 0,
                  s->session->master_key, s->session->master_key_length,
                  out, olen);

    goto ret;
 err1:
    SSLerr(SSL_F_TLS1_EXPORT_KEYING_MATERIAL, SSL_R_TLS_ILLEGAL_EXPORTER_LABEL);
    rv = 0;
    goto ret;
 err2:
    SSLerr(SSL_F_TLS1_EXPORT_KEYING_MATERIAL, ERR_R_MALLOC_FAILURE);
    rv = 0;
 ret:
    OPENSSL_clear_free(val, vallen);
    return (rv);
}

int tls1_alert_code(int code)
{
    switch (code) {
    case SSL_AD_CLOSE_NOTIFY:
        return (SSL3_AD_CLOSE_NOTIFY);
    case SSL_AD_UNEXPECTED_MESSAGE:
        return (SSL3_AD_UNEXPECTED_MESSAGE);
    case SSL_AD_BAD_RECORD_MAC:
        return (SSL3_AD_BAD_RECORD_MAC);
    case SSL_AD_DECRYPTION_FAILED:
        return (TLS1_AD_DECRYPTION_FAILED);
    case SSL_AD_RECORD_OVERFLOW:
        return (TLS1_AD_RECORD_OVERFLOW);
    case SSL_AD_DECOMPRESSION_FAILURE:
        return (SSL3_AD_DECOMPRESSION_FAILURE);
    case SSL_AD_HANDSHAKE_FAILURE:
        return (SSL3_AD_HANDSHAKE_FAILURE);
    case SSL_AD_NO_CERTIFICATE:
        return (-1);
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
        return (TLS1_AD_UNKNOWN_CA);
    case SSL_AD_ACCESS_DENIED:
        return (TLS1_AD_ACCESS_DENIED);
    case SSL_AD_DECODE_ERROR:
        return (TLS1_AD_DECODE_ERROR);
    case SSL_AD_DECRYPT_ERROR:
        return (TLS1_AD_DECRYPT_ERROR);
    case SSL_AD_EXPORT_RESTRICTION:
        return (TLS1_AD_EXPORT_RESTRICTION);
    case SSL_AD_PROTOCOL_VERSION:
        return (TLS1_AD_PROTOCOL_VERSION);
    case SSL_AD_INSUFFICIENT_SECURITY:
        return (TLS1_AD_INSUFFICIENT_SECURITY);
    case SSL_AD_INTERNAL_ERROR:
        return (TLS1_AD_INTERNAL_ERROR);
    case SSL_AD_USER_CANCELLED:
        return (TLS1_AD_USER_CANCELLED);
    case SSL_AD_NO_RENEGOTIATION:
        return (TLS1_AD_NO_RENEGOTIATION);
    case SSL_AD_UNSUPPORTED_EXTENSION:
        return (TLS1_AD_UNSUPPORTED_EXTENSION);
    case SSL_AD_CERTIFICATE_UNOBTAINABLE:
        return (TLS1_AD_CERTIFICATE_UNOBTAINABLE);
    case SSL_AD_UNRECOGNIZED_NAME:
        return (TLS1_AD_UNRECOGNIZED_NAME);
    case SSL_AD_BAD_CERTIFICATE_STATUS_RESPONSE:
        return (TLS1_AD_BAD_CERTIFICATE_STATUS_RESPONSE);
    case SSL_AD_BAD_CERTIFICATE_HASH_VALUE:
        return (TLS1_AD_BAD_CERTIFICATE_HASH_VALUE);
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
