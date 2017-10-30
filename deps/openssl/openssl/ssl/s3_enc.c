/* ssl/s3_enc.c */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */
/* ====================================================================
 * Copyright (c) 1998-2007 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
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

static unsigned char ssl3_pad_1[48] = {
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36
};

static unsigned char ssl3_pad_2[48] = {
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c
};

static int ssl3_handshake_mac(SSL *s, int md_nid,
                              const char *sender, int len, unsigned char *p);
static int ssl3_generate_key_block(SSL *s, unsigned char *km, int num)
{
    EVP_MD_CTX m5;
    EVP_MD_CTX s1;
    unsigned char buf[16], smd[SHA_DIGEST_LENGTH];
    unsigned char c = 'A';
    unsigned int i, j, k;

#ifdef CHARSET_EBCDIC
    c = os_toascii[c];          /* 'A' in ASCII */
#endif
    k = 0;
    EVP_MD_CTX_init(&m5);
    EVP_MD_CTX_set_flags(&m5, EVP_MD_CTX_FLAG_NON_FIPS_ALLOW);
    EVP_MD_CTX_init(&s1);
    for (i = 0; (int)i < num; i += MD5_DIGEST_LENGTH) {
        k++;
        if (k > sizeof(buf))
            /* bug: 'buf' is too small for this ciphersuite */
            goto err;

        for (j = 0; j < k; j++)
            buf[j] = c;
        c++;
        if (!EVP_DigestInit_ex(&s1, EVP_sha1(), NULL) ||
            !EVP_DigestUpdate(&s1, buf, k) ||
            !EVP_DigestUpdate(&s1, s->session->master_key,
                              s->session->master_key_length) ||
            !EVP_DigestUpdate(&s1, s->s3->server_random, SSL3_RANDOM_SIZE) ||
            !EVP_DigestUpdate(&s1, s->s3->client_random, SSL3_RANDOM_SIZE) ||
            !EVP_DigestFinal_ex(&s1, smd, NULL))
            goto err2;

        if (!EVP_DigestInit_ex(&m5, EVP_md5(), NULL) ||
            !EVP_DigestUpdate(&m5, s->session->master_key,
                              s->session->master_key_length) ||
            !EVP_DigestUpdate(&m5, smd, SHA_DIGEST_LENGTH))
            goto err2;
        if ((int)(i + MD5_DIGEST_LENGTH) > num) {
            if (!EVP_DigestFinal_ex(&m5, smd, NULL))
                goto err2;
            memcpy(km, smd, (num - i));
        } else
            if (!EVP_DigestFinal_ex(&m5, km, NULL))
                goto err2;

        km += MD5_DIGEST_LENGTH;
    }
    OPENSSL_cleanse(smd, SHA_DIGEST_LENGTH);
    EVP_MD_CTX_cleanup(&m5);
    EVP_MD_CTX_cleanup(&s1);
    return 1;
 err:
    SSLerr(SSL_F_SSL3_GENERATE_KEY_BLOCK, ERR_R_INTERNAL_ERROR);
 err2:
    EVP_MD_CTX_cleanup(&m5);
    EVP_MD_CTX_cleanup(&s1);
    return 0;
}

int ssl3_change_cipher_state(SSL *s, int which)
{
    unsigned char *p, *mac_secret;
    unsigned char exp_key[EVP_MAX_KEY_LENGTH];
    unsigned char exp_iv[EVP_MAX_IV_LENGTH];
    unsigned char *ms, *key, *iv, *er1, *er2;
    EVP_CIPHER_CTX *dd;
    const EVP_CIPHER *c;
#ifndef OPENSSL_NO_COMP
    COMP_METHOD *comp;
#endif
    const EVP_MD *m;
    EVP_MD_CTX md;
    int is_exp, n, i, j, k, cl;
    int reuse_dd = 0;

    is_exp = SSL_C_IS_EXPORT(s->s3->tmp.new_cipher);
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
        else if ((s->enc_read_ctx =
                  OPENSSL_malloc(sizeof(EVP_CIPHER_CTX))) == NULL)
            goto err;
        else
            /*
             * make sure it's intialized in case we exit later with an error
             */
            EVP_CIPHER_CTX_init(s->enc_read_ctx);
        dd = s->enc_read_ctx;

        if (ssl_replace_hash(&s->read_hash, m) == NULL) {
                SSLerr(SSL_F_SSL3_CHANGE_CIPHER_STATE, ERR_R_INTERNAL_ERROR);
                goto err2;
        }
#ifndef OPENSSL_NO_COMP
        /* COMPRESS */
        if (s->expand != NULL) {
            COMP_CTX_free(s->expand);
            s->expand = NULL;
        }
        if (comp != NULL) {
            s->expand = COMP_CTX_new(comp);
            if (s->expand == NULL) {
                SSLerr(SSL_F_SSL3_CHANGE_CIPHER_STATE,
                       SSL_R_COMPRESSION_LIBRARY_ERROR);
                goto err2;
            }
            if (s->s3->rrec.comp == NULL)
                s->s3->rrec.comp = (unsigned char *)
                    OPENSSL_malloc(SSL3_RT_MAX_PLAIN_LENGTH);
            if (s->s3->rrec.comp == NULL)
                goto err;
        }
#endif
        memset(&(s->s3->read_sequence[0]), 0, 8);
        mac_secret = &(s->s3->read_mac_secret[0]);
    } else {
        if (s->enc_write_ctx != NULL)
            reuse_dd = 1;
        else if ((s->enc_write_ctx =
                  OPENSSL_malloc(sizeof(EVP_CIPHER_CTX))) == NULL)
            goto err;
        else
            /*
             * make sure it's intialized in case we exit later with an error
             */
            EVP_CIPHER_CTX_init(s->enc_write_ctx);
        dd = s->enc_write_ctx;
        if (ssl_replace_hash(&s->write_hash, m) == NULL) {
                SSLerr(SSL_F_SSL3_CHANGE_CIPHER_STATE, ERR_R_INTERNAL_ERROR);
                goto err2;
        }
#ifndef OPENSSL_NO_COMP
        /* COMPRESS */
        if (s->compress != NULL) {
            COMP_CTX_free(s->compress);
            s->compress = NULL;
        }
        if (comp != NULL) {
            s->compress = COMP_CTX_new(comp);
            if (s->compress == NULL) {
                SSLerr(SSL_F_SSL3_CHANGE_CIPHER_STATE,
                       SSL_R_COMPRESSION_LIBRARY_ERROR);
                goto err2;
            }
        }
#endif
        memset(&(s->s3->write_sequence[0]), 0, 8);
        mac_secret = &(s->s3->write_mac_secret[0]);
    }

    if (reuse_dd)
        EVP_CIPHER_CTX_cleanup(dd);

    p = s->s3->tmp.key_block;
    i = EVP_MD_size(m);
    if (i < 0)
        goto err2;
    cl = EVP_CIPHER_key_length(c);
    j = is_exp ? (cl < SSL_C_EXPORT_KEYLENGTH(s->s3->tmp.new_cipher) ?
                  cl : SSL_C_EXPORT_KEYLENGTH(s->s3->tmp.new_cipher)) : cl;
    /* Was j=(is_exp)?5:EVP_CIPHER_key_length(c); */
    k = EVP_CIPHER_iv_length(c);
    if ((which == SSL3_CHANGE_CIPHER_CLIENT_WRITE) ||
        (which == SSL3_CHANGE_CIPHER_SERVER_READ)) {
        ms = &(p[0]);
        n = i + i;
        key = &(p[n]);
        n += j + j;
        iv = &(p[n]);
        n += k + k;
        er1 = &(s->s3->client_random[0]);
        er2 = &(s->s3->server_random[0]);
    } else {
        n = i;
        ms = &(p[n]);
        n += i + j;
        key = &(p[n]);
        n += j + k;
        iv = &(p[n]);
        n += k;
        er1 = &(s->s3->server_random[0]);
        er2 = &(s->s3->client_random[0]);
    }

    if (n > s->s3->tmp.key_block_length) {
        SSLerr(SSL_F_SSL3_CHANGE_CIPHER_STATE, ERR_R_INTERNAL_ERROR);
        goto err2;
    }

    EVP_MD_CTX_init(&md);
    memcpy(mac_secret, ms, i);
    if (is_exp) {
        /*
         * In here I set both the read and write key/iv to the same value
         * since only the correct one will be used :-).
         */
        if (!EVP_DigestInit_ex(&md, EVP_md5(), NULL) ||
            !EVP_DigestUpdate(&md, key, j) ||
            !EVP_DigestUpdate(&md, er1, SSL3_RANDOM_SIZE) ||
            !EVP_DigestUpdate(&md, er2, SSL3_RANDOM_SIZE) ||
            !EVP_DigestFinal_ex(&md, &(exp_key[0]), NULL)) {
            EVP_MD_CTX_cleanup(&md);
            goto err2;
        }
        key = &(exp_key[0]);

        if (k > 0) {
            if (!EVP_DigestInit_ex(&md, EVP_md5(), NULL) ||
                !EVP_DigestUpdate(&md, er1, SSL3_RANDOM_SIZE) ||
                !EVP_DigestUpdate(&md, er2, SSL3_RANDOM_SIZE) ||
                !EVP_DigestFinal_ex(&md, &(exp_iv[0]), NULL)) {
                EVP_MD_CTX_cleanup(&md);
                goto err2;
            }
            iv = &(exp_iv[0]);
        }
    }
    EVP_MD_CTX_cleanup(&md);

    s->session->key_arg_length = 0;

    if (!EVP_CipherInit_ex(dd, c, NULL, key, iv, (which & SSL3_CC_WRITE)))
        goto err2;

#ifdef OPENSSL_SSL_TRACE_CRYPTO
    if (s->msg_callback) {

        int wh = which & SSL3_CC_WRITE ?
            TLS1_RT_CRYPTO_WRITE : TLS1_RT_CRYPTO_READ;
        s->msg_callback(2, s->version, wh | TLS1_RT_CRYPTO_MAC,
                        mac_secret, EVP_MD_size(m), s, s->msg_callback_arg);
        if (c->key_len)
            s->msg_callback(2, s->version, wh | TLS1_RT_CRYPTO_KEY,
                            key, c->key_len, s, s->msg_callback_arg);
        if (k) {
            s->msg_callback(2, s->version, wh | TLS1_RT_CRYPTO_IV,
                            iv, k, s, s->msg_callback_arg);
        }
    }
#endif

    OPENSSL_cleanse(&(exp_key[0]), sizeof(exp_key));
    OPENSSL_cleanse(&(exp_iv[0]), sizeof(exp_iv));
    return (1);
 err:
    SSLerr(SSL_F_SSL3_CHANGE_CIPHER_STATE, ERR_R_MALLOC_FAILURE);
 err2:
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

    if (!ssl_cipher_get_evp(s->session, &c, &hash, NULL, NULL, &comp)) {
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
    if (s->s3->tmp.key_block != NULL) {
        OPENSSL_cleanse(s->s3->tmp.key_block, s->s3->tmp.key_block_length);
        OPENSSL_free(s->s3->tmp.key_block);
        s->s3->tmp.key_block = NULL;
    }
    s->s3->tmp.key_block_length = 0;
}

/*-
 * ssl3_enc encrypts/decrypts the record in |s->wrec| / |s->rrec|, respectively.
 *
 * Returns:
 *   0: (in non-constant time) if the record is publically invalid (i.e. too
 *       short etc).
 *   1: if the record's padding is valid / the encryption was successful.
 *   -1: if the record's padding is invalid or, if sending, an internal error
 *       occured.
 */
int ssl3_enc(SSL *s, int send)
{
    SSL3_RECORD *rec;
    EVP_CIPHER_CTX *ds;
    unsigned long l;
    int bs, i, mac_size = 0;
    const EVP_CIPHER *enc;

    if (send) {
        ds = s->enc_write_ctx;
        rec = &(s->s3->wrec);
        if (s->enc_write_ctx == NULL)
            enc = NULL;
        else
            enc = EVP_CIPHER_CTX_cipher(s->enc_write_ctx);
    } else {
        ds = s->enc_read_ctx;
        rec = &(s->s3->rrec);
        if (s->enc_read_ctx == NULL)
            enc = NULL;
        else
            enc = EVP_CIPHER_CTX_cipher(s->enc_read_ctx);
    }

    if ((s->session == NULL) || (ds == NULL) || (enc == NULL)) {
        memmove(rec->data, rec->input, rec->length);
        rec->input = rec->data;
    } else {
        l = rec->length;
        bs = EVP_CIPHER_block_size(ds->cipher);

        /* COMPRESS */

        if ((bs != 1) && send) {
            i = bs - ((int)l % bs);

            /* we need to add 'i-1' padding bytes */
            l += i;
            /*
             * the last of these zero bytes will be overwritten with the
             * padding length.
             */
            memset(&rec->input[rec->length], 0, i);
            rec->length += i;
            rec->input[l - 1] = (i - 1);
        }

        if (!send) {
            if (l == 0 || l % bs != 0)
                return 0;
            /* otherwise, rec->length >= bs */
        }

        if (EVP_Cipher(ds, rec->data, rec->input, l) < 1)
            return -1;

        if (EVP_MD_CTX_md(s->read_hash) != NULL)
            mac_size = EVP_MD_CTX_size(s->read_hash);
        if ((bs != 1) && !send)
            return ssl3_cbc_remove_padding(s, rec, bs, mac_size);
    }
    return 1;
}

int ssl3_init_finished_mac(SSL *s)
{
    if (s->s3->handshake_buffer)
        BIO_free(s->s3->handshake_buffer);
    if (s->s3->handshake_dgst)
        ssl3_free_digest_list(s);
    s->s3->handshake_buffer = BIO_new(BIO_s_mem());
    if (s->s3->handshake_buffer == NULL)
        return 0;
    (void)BIO_set_close(s->s3->handshake_buffer, BIO_CLOSE);
    return 1;
}

void ssl3_free_digest_list(SSL *s)
{
    int i;
    if (!s->s3->handshake_dgst)
        return;
    for (i = 0; i < SSL_MAX_DIGEST; i++) {
        if (s->s3->handshake_dgst[i])
            EVP_MD_CTX_destroy(s->s3->handshake_dgst[i]);
    }
    OPENSSL_free(s->s3->handshake_dgst);
    s->s3->handshake_dgst = NULL;
}

void ssl3_finish_mac(SSL *s, const unsigned char *buf, int len)
{
    if (s->s3->handshake_buffer
        && !(s->s3->flags & TLS1_FLAGS_KEEP_HANDSHAKE)) {
        BIO_write(s->s3->handshake_buffer, (void *)buf, len);
    } else {
        int i;
        for (i = 0; i < SSL_MAX_DIGEST; i++) {
            if (s->s3->handshake_dgst[i] != NULL)
                EVP_DigestUpdate(s->s3->handshake_dgst[i], buf, len);
        }
    }
}

int ssl3_digest_cached_records(SSL *s)
{
    int i;
    long mask;
    const EVP_MD *md;
    long hdatalen;
    void *hdata;

    /* Allocate handshake_dgst array */
    ssl3_free_digest_list(s);
    s->s3->handshake_dgst =
        OPENSSL_malloc(SSL_MAX_DIGEST * sizeof(EVP_MD_CTX *));
    if (s->s3->handshake_dgst == NULL) {
        SSLerr(SSL_F_SSL3_DIGEST_CACHED_RECORDS, ERR_R_MALLOC_FAILURE);
        return 0;
    }
    memset(s->s3->handshake_dgst, 0, SSL_MAX_DIGEST * sizeof(EVP_MD_CTX *));
    hdatalen = BIO_get_mem_data(s->s3->handshake_buffer, &hdata);
    if (hdatalen <= 0) {
        SSLerr(SSL_F_SSL3_DIGEST_CACHED_RECORDS, SSL_R_BAD_HANDSHAKE_LENGTH);
        return 0;
    }

    /* Loop through bitso of algorithm2 field and create MD_CTX-es */
    for (i = 0; ssl_get_handshake_digest(i, &mask, &md); i++) {
        if ((mask & ssl_get_algorithm2(s)) && md) {
            s->s3->handshake_dgst[i] = EVP_MD_CTX_create();
            if (s->s3->handshake_dgst[i] == NULL) {
                SSLerr(SSL_F_SSL3_DIGEST_CACHED_RECORDS, ERR_R_MALLOC_FAILURE);
                return 0;
            }
#ifdef OPENSSL_FIPS
            if (EVP_MD_nid(md) == NID_md5) {
                EVP_MD_CTX_set_flags(s->s3->handshake_dgst[i],
                                     EVP_MD_CTX_FLAG_NON_FIPS_ALLOW);
            }
#endif
            if (!EVP_DigestInit_ex(s->s3->handshake_dgst[i], md, NULL)
                || !EVP_DigestUpdate(s->s3->handshake_dgst[i], hdata,
                                     hdatalen)) {
                SSLerr(SSL_F_SSL3_DIGEST_CACHED_RECORDS, ERR_R_INTERNAL_ERROR);
                return 0;
            }
        } else {
            s->s3->handshake_dgst[i] = NULL;
        }
    }
    if (!(s->s3->flags & TLS1_FLAGS_KEEP_HANDSHAKE)) {
        /* Free handshake_buffer BIO */
        BIO_free(s->s3->handshake_buffer);
        s->s3->handshake_buffer = NULL;
    }

    return 1;
}

int ssl3_cert_verify_mac(SSL *s, int md_nid, unsigned char *p)
{
    return (ssl3_handshake_mac(s, md_nid, NULL, 0, p));
}

int ssl3_final_finish_mac(SSL *s,
                          const char *sender, int len, unsigned char *p)
{
    int ret, sha1len;
    ret = ssl3_handshake_mac(s, NID_md5, sender, len, p);
    if (ret == 0)
        return 0;

    p += ret;

    sha1len = ssl3_handshake_mac(s, NID_sha1, sender, len, p);
    if (sha1len == 0)
        return 0;

    ret += sha1len;
    return (ret);
}

static int ssl3_handshake_mac(SSL *s, int md_nid,
                              const char *sender, int len, unsigned char *p)
{
    unsigned int ret;
    int npad, n;
    unsigned int i;
    unsigned char md_buf[EVP_MAX_MD_SIZE];
    EVP_MD_CTX ctx, *d = NULL;

    if (s->s3->handshake_buffer)
        if (!ssl3_digest_cached_records(s))
            return 0;

    /*
     * Search for digest of specified type in the handshake_dgst array
     */
    for (i = 0; i < SSL_MAX_DIGEST; i++) {
        if (s->s3->handshake_dgst[i]
            && EVP_MD_CTX_type(s->s3->handshake_dgst[i]) == md_nid) {
            d = s->s3->handshake_dgst[i];
            break;
        }
    }
    if (!d) {
        SSLerr(SSL_F_SSL3_HANDSHAKE_MAC, SSL_R_NO_REQUIRED_DIGEST);
        return 0;
    }
    EVP_MD_CTX_init(&ctx);
    EVP_MD_CTX_set_flags(&ctx, EVP_MD_CTX_FLAG_NON_FIPS_ALLOW);
    EVP_MD_CTX_copy_ex(&ctx, d);
    n = EVP_MD_CTX_size(&ctx);
    if (n < 0)
        return 0;

    npad = (48 / n) * n;
    if ((sender != NULL && EVP_DigestUpdate(&ctx, sender, len) <= 0)
            || EVP_DigestUpdate(&ctx, s->session->master_key,
                                s->session->master_key_length) <= 0
            || EVP_DigestUpdate(&ctx, ssl3_pad_1, npad) <= 0
            || EVP_DigestFinal_ex(&ctx, md_buf, &i) <= 0

            || EVP_DigestInit_ex(&ctx, EVP_MD_CTX_md(&ctx), NULL) <= 0
            || EVP_DigestUpdate(&ctx, s->session->master_key,
                                s->session->master_key_length) <= 0
            || EVP_DigestUpdate(&ctx, ssl3_pad_2, npad) <= 0
            || EVP_DigestUpdate(&ctx, md_buf, i) <= 0
            || EVP_DigestFinal_ex(&ctx, p, &ret) <= 0) {
        SSLerr(SSL_F_SSL3_HANDSHAKE_MAC, ERR_R_INTERNAL_ERROR);
        ret = 0;
    }

    EVP_MD_CTX_cleanup(&ctx);

    return ((int)ret);
}

int n_ssl3_mac(SSL *ssl, unsigned char *md, int send)
{
    SSL3_RECORD *rec;
    unsigned char *mac_sec, *seq;
    EVP_MD_CTX md_ctx;
    const EVP_MD_CTX *hash;
    unsigned char *p, rec_char;
    size_t md_size, orig_len;
    int npad;
    int t;

    if (send) {
        rec = &(ssl->s3->wrec);
        mac_sec = &(ssl->s3->write_mac_secret[0]);
        seq = &(ssl->s3->write_sequence[0]);
        hash = ssl->write_hash;
    } else {
        rec = &(ssl->s3->rrec);
        mac_sec = &(ssl->s3->read_mac_secret[0]);
        seq = &(ssl->s3->read_sequence[0]);
        hash = ssl->read_hash;
    }

    t = EVP_MD_CTX_size(hash);
    if (t < 0)
        return -1;
    md_size = t;
    npad = (48 / md_size) * md_size;

    /*
     * kludge: ssl3_cbc_remove_padding passes padding length in rec->type
     */
    orig_len = rec->length + md_size + ((unsigned int)rec->type >> 8);
    rec->type &= 0xff;

    if (!send &&
        EVP_CIPHER_CTX_mode(ssl->enc_read_ctx) == EVP_CIPH_CBC_MODE &&
        ssl3_cbc_record_digest_supported(hash)) {
        /*
         * This is a CBC-encrypted record. We must avoid leaking any
         * timing-side channel information about how many blocks of data we
         * are hashing because that gives an attacker a timing-oracle.
         */

        /*-
         * npad is, at most, 48 bytes and that's with MD5:
         *   16 + 48 + 8 (sequence bytes) + 1 + 2 = 75.
         *
         * With SHA-1 (the largest hash speced for SSLv3) the hash size
         * goes up 4, but npad goes down by 8, resulting in a smaller
         * total size.
         */
        unsigned char header[75];
        unsigned j = 0;
        memcpy(header + j, mac_sec, md_size);
        j += md_size;
        memcpy(header + j, ssl3_pad_1, npad);
        j += npad;
        memcpy(header + j, seq, 8);
        j += 8;
        header[j++] = rec->type;
        header[j++] = rec->length >> 8;
        header[j++] = rec->length & 0xff;

        /* Final param == is SSLv3 */
        if (ssl3_cbc_digest_record(hash,
                                   md, &md_size,
                                   header, rec->input,
                                   rec->length + md_size, orig_len,
                                   mac_sec, md_size, 1) <= 0)
            return -1;
    } else {
        unsigned int md_size_u;
        /* Chop the digest off the end :-) */
        EVP_MD_CTX_init(&md_ctx);

        rec_char = rec->type;
        p = md;
        s2n(rec->length, p);
        if (EVP_MD_CTX_copy_ex(&md_ctx, hash) <= 0
                || EVP_DigestUpdate(&md_ctx, mac_sec, md_size) <= 0
                || EVP_DigestUpdate(&md_ctx, ssl3_pad_1, npad) <= 0
                || EVP_DigestUpdate(&md_ctx, seq, 8) <= 0
                || EVP_DigestUpdate(&md_ctx, &rec_char, 1) <= 0
                || EVP_DigestUpdate(&md_ctx, md, 2) <= 0
                || EVP_DigestUpdate(&md_ctx, rec->input, rec->length) <= 0
                || EVP_DigestFinal_ex(&md_ctx, md, NULL) <= 0
                || EVP_MD_CTX_copy_ex(&md_ctx, hash) <= 0
                || EVP_DigestUpdate(&md_ctx, mac_sec, md_size) <= 0
                || EVP_DigestUpdate(&md_ctx, ssl3_pad_2, npad) <= 0
                || EVP_DigestUpdate(&md_ctx, md, md_size) <= 0
                || EVP_DigestFinal_ex(&md_ctx, md, &md_size_u) <= 0) {
            EVP_MD_CTX_cleanup(&md_ctx);
            return -1;
        }
        md_size = md_size_u;

        EVP_MD_CTX_cleanup(&md_ctx);
    }

    ssl3_record_sequence_update(seq);
    return (md_size);
}

void ssl3_record_sequence_update(unsigned char *seq)
{
    int i;

    for (i = 7; i >= 0; i--) {
        ++seq[i];
        if (seq[i] != 0)
            break;
    }
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
    EVP_MD_CTX ctx;
    int i, ret = 0;
    unsigned int n;
#ifdef OPENSSL_SSL_TRACE_CRYPTO
    unsigned char *tmpout = out;
#endif

    EVP_MD_CTX_init(&ctx);
    for (i = 0; i < 3; i++) {
        if (EVP_DigestInit_ex(&ctx, s->ctx->sha1, NULL) <= 0
                || EVP_DigestUpdate(&ctx, salt[i],
                                    strlen((const char *)salt[i])) <= 0
                || EVP_DigestUpdate(&ctx, p, len) <= 0
                || EVP_DigestUpdate(&ctx, &(s->s3->client_random[0]),
                                    SSL3_RANDOM_SIZE) <= 0
                || EVP_DigestUpdate(&ctx, &(s->s3->server_random[0]),
                                    SSL3_RANDOM_SIZE) <= 0
                || EVP_DigestFinal_ex(&ctx, buf, &n) <= 0

                || EVP_DigestInit_ex(&ctx, s->ctx->md5, NULL) <= 0
                || EVP_DigestUpdate(&ctx, p, len) <= 0
                || EVP_DigestUpdate(&ctx, buf, n) <= 0
                || EVP_DigestFinal_ex(&ctx, out, &n) <= 0) {
            SSLerr(SSL_F_SSL3_GENERATE_MASTER_SECRET, ERR_R_INTERNAL_ERROR);
            ret = 0;
            break;
        }
        out += n;
        ret += n;
    }
    EVP_MD_CTX_cleanup(&ctx);

#ifdef OPENSSL_SSL_TRACE_CRYPTO
    if (ret > 0 && s->msg_callback) {
        s->msg_callback(2, s->version, TLS1_RT_CRYPTO_PREMASTER,
                        p, len, s, s->msg_callback_arg);
        s->msg_callback(2, s->version, TLS1_RT_CRYPTO_CLIENT_RANDOM,
                        s->s3->client_random, SSL3_RANDOM_SIZE,
                        s, s->msg_callback_arg);
        s->msg_callback(2, s->version, TLS1_RT_CRYPTO_SERVER_RANDOM,
                        s->s3->server_random, SSL3_RANDOM_SIZE,
                        s, s->msg_callback_arg);
        s->msg_callback(2, s->version, TLS1_RT_CRYPTO_MASTER,
                        tmpout, SSL3_MASTER_SECRET_SIZE,
                        s, s->msg_callback_arg);
    }
#endif
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
    default:
        return (-1);
    }
}
