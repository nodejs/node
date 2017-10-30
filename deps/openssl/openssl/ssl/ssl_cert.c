/*
 * ! \file ssl/ssl_cert.c
 */
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
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 * ECC cipher suite support in OpenSSL originally developed by
 * SUN MICROSYSTEMS, INC., and contributed to the OpenSSL project.
 */

#include <stdio.h>

#include "e_os.h"
#ifndef NO_SYS_TYPES_H
# include <sys/types.h>
#endif

#include "o_dir.h"
#include <openssl/objects.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>
#ifndef OPENSSL_NO_DH
# include <openssl/dh.h>
#endif
#include <openssl/bn.h>
#include "ssl_locl.h"

int SSL_get_ex_data_X509_STORE_CTX_idx(void)
{
    static volatile int ssl_x509_store_ctx_idx = -1;
    int got_write_lock = 0;

    if (((size_t)&ssl_x509_store_ctx_idx &
         (sizeof(ssl_x509_store_ctx_idx) - 1))
        == 0) {                 /* check alignment, practically always true */
        int ret;

        if ((ret = ssl_x509_store_ctx_idx) < 0) {
            CRYPTO_w_lock(CRYPTO_LOCK_SSL_CTX);
            if ((ret = ssl_x509_store_ctx_idx) < 0) {
                ret = ssl_x509_store_ctx_idx =
                    X509_STORE_CTX_get_ex_new_index(0,
                                                    "SSL for verify callback",
                                                    NULL, NULL, NULL);
            }
            CRYPTO_w_unlock(CRYPTO_LOCK_SSL_CTX);
        }

        return ret;
    } else {                    /* commonly eliminated */

        CRYPTO_r_lock(CRYPTO_LOCK_SSL_CTX);

        if (ssl_x509_store_ctx_idx < 0) {
            CRYPTO_r_unlock(CRYPTO_LOCK_SSL_CTX);
            CRYPTO_w_lock(CRYPTO_LOCK_SSL_CTX);
            got_write_lock = 1;

            if (ssl_x509_store_ctx_idx < 0) {
                ssl_x509_store_ctx_idx =
                    X509_STORE_CTX_get_ex_new_index(0,
                                                    "SSL for verify callback",
                                                    NULL, NULL, NULL);
            }
        }

        if (got_write_lock)
            CRYPTO_w_unlock(CRYPTO_LOCK_SSL_CTX);
        else
            CRYPTO_r_unlock(CRYPTO_LOCK_SSL_CTX);

        return ssl_x509_store_ctx_idx;
    }
}

void ssl_cert_set_default_md(CERT *cert)
{
    /* Set digest values to defaults */
#ifndef OPENSSL_NO_DSA
    cert->pkeys[SSL_PKEY_DSA_SIGN].digest = EVP_sha1();
#endif
#ifndef OPENSSL_NO_RSA
    cert->pkeys[SSL_PKEY_RSA_SIGN].digest = EVP_sha1();
    cert->pkeys[SSL_PKEY_RSA_ENC].digest = EVP_sha1();
#endif
#ifndef OPENSSL_NO_ECDSA
    cert->pkeys[SSL_PKEY_ECC].digest = EVP_sha1();
#endif
}

CERT *ssl_cert_new(void)
{
    CERT *ret;

    ret = (CERT *)OPENSSL_malloc(sizeof(CERT));
    if (ret == NULL) {
        SSLerr(SSL_F_SSL_CERT_NEW, ERR_R_MALLOC_FAILURE);
        return (NULL);
    }
    memset(ret, 0, sizeof(CERT));

    ret->key = &(ret->pkeys[SSL_PKEY_RSA_ENC]);
    ret->references = 1;
    ssl_cert_set_default_md(ret);
    return (ret);
}

CERT *ssl_cert_dup(CERT *cert)
{
    CERT *ret;
    int i;

    ret = (CERT *)OPENSSL_malloc(sizeof(CERT));
    if (ret == NULL) {
        SSLerr(SSL_F_SSL_CERT_DUP, ERR_R_MALLOC_FAILURE);
        return (NULL);
    }

    memset(ret, 0, sizeof(CERT));

    ret->references = 1;
    ret->key = &ret->pkeys[cert->key - &cert->pkeys[0]];
    /*
     * or ret->key = ret->pkeys + (cert->key - cert->pkeys), if you find that
     * more readable
     */

    ret->valid = cert->valid;
    ret->mask_k = cert->mask_k;
    ret->mask_a = cert->mask_a;
    ret->export_mask_k = cert->export_mask_k;
    ret->export_mask_a = cert->export_mask_a;

#ifndef OPENSSL_NO_RSA
    if (cert->rsa_tmp != NULL) {
        RSA_up_ref(cert->rsa_tmp);
        ret->rsa_tmp = cert->rsa_tmp;
    }
    ret->rsa_tmp_cb = cert->rsa_tmp_cb;
#endif

#ifndef OPENSSL_NO_DH
    if (cert->dh_tmp != NULL) {
        ret->dh_tmp = DHparams_dup(cert->dh_tmp);
        if (ret->dh_tmp == NULL) {
            SSLerr(SSL_F_SSL_CERT_DUP, ERR_R_DH_LIB);
            goto err;
        }
        if (cert->dh_tmp->priv_key) {
            BIGNUM *b = BN_dup(cert->dh_tmp->priv_key);
            if (!b) {
                SSLerr(SSL_F_SSL_CERT_DUP, ERR_R_BN_LIB);
                goto err;
            }
            ret->dh_tmp->priv_key = b;
        }
        if (cert->dh_tmp->pub_key) {
            BIGNUM *b = BN_dup(cert->dh_tmp->pub_key);
            if (!b) {
                SSLerr(SSL_F_SSL_CERT_DUP, ERR_R_BN_LIB);
                goto err;
            }
            ret->dh_tmp->pub_key = b;
        }
    }
    ret->dh_tmp_cb = cert->dh_tmp_cb;
#endif

#ifndef OPENSSL_NO_ECDH
    if (cert->ecdh_tmp) {
        ret->ecdh_tmp = EC_KEY_dup(cert->ecdh_tmp);
        if (ret->ecdh_tmp == NULL) {
            SSLerr(SSL_F_SSL_CERT_DUP, ERR_R_EC_LIB);
            goto err;
        }
    }
    ret->ecdh_tmp_cb = cert->ecdh_tmp_cb;
    ret->ecdh_tmp_auto = cert->ecdh_tmp_auto;
#endif

    for (i = 0; i < SSL_PKEY_NUM; i++) {
        CERT_PKEY *cpk = cert->pkeys + i;
        CERT_PKEY *rpk = ret->pkeys + i;
        if (cpk->x509 != NULL) {
            rpk->x509 = cpk->x509;
            CRYPTO_add(&rpk->x509->references, 1, CRYPTO_LOCK_X509);
        }

        if (cpk->privatekey != NULL) {
            rpk->privatekey = cpk->privatekey;
            CRYPTO_add(&cpk->privatekey->references, 1, CRYPTO_LOCK_EVP_PKEY);
        }

        if (cpk->chain) {
            rpk->chain = X509_chain_up_ref(cpk->chain);
            if (!rpk->chain) {
                SSLerr(SSL_F_SSL_CERT_DUP, ERR_R_MALLOC_FAILURE);
                goto err;
            }
        }
        rpk->valid_flags = 0;
#ifndef OPENSSL_NO_TLSEXT
        if (cert->pkeys[i].serverinfo != NULL) {
            /* Just copy everything. */
            ret->pkeys[i].serverinfo =
                OPENSSL_malloc(cert->pkeys[i].serverinfo_length);
            if (ret->pkeys[i].serverinfo == NULL) {
                SSLerr(SSL_F_SSL_CERT_DUP, ERR_R_MALLOC_FAILURE);
                goto err;
            }
            ret->pkeys[i].serverinfo_length =
                cert->pkeys[i].serverinfo_length;
            memcpy(ret->pkeys[i].serverinfo,
                   cert->pkeys[i].serverinfo,
                   cert->pkeys[i].serverinfo_length);
        }
#endif
    }

    /*
     * Set digests to defaults. NB: we don't copy existing values as they
     * will be set during handshake.
     */
    ssl_cert_set_default_md(ret);
    /* Peer sigalgs set to NULL as we get these from handshake too */
    ret->peer_sigalgs = NULL;
    ret->peer_sigalgslen = 0;
    /* Configured sigalgs however we copy across */

    if (cert->conf_sigalgs) {
        ret->conf_sigalgs = OPENSSL_malloc(cert->conf_sigalgslen);
        if (!ret->conf_sigalgs)
            goto err;
        memcpy(ret->conf_sigalgs, cert->conf_sigalgs, cert->conf_sigalgslen);
        ret->conf_sigalgslen = cert->conf_sigalgslen;
    } else
        ret->conf_sigalgs = NULL;

    if (cert->client_sigalgs) {
        ret->client_sigalgs = OPENSSL_malloc(cert->client_sigalgslen);
        if (!ret->client_sigalgs)
            goto err;
        memcpy(ret->client_sigalgs, cert->client_sigalgs,
               cert->client_sigalgslen);
        ret->client_sigalgslen = cert->client_sigalgslen;
    } else
        ret->client_sigalgs = NULL;
    /* Shared sigalgs also NULL */
    ret->shared_sigalgs = NULL;
    /* Copy any custom client certificate types */
    if (cert->ctypes) {
        ret->ctypes = OPENSSL_malloc(cert->ctype_num);
        if (!ret->ctypes)
            goto err;
        memcpy(ret->ctypes, cert->ctypes, cert->ctype_num);
        ret->ctype_num = cert->ctype_num;
    }

    ret->cert_flags = cert->cert_flags;

    ret->cert_cb = cert->cert_cb;
    ret->cert_cb_arg = cert->cert_cb_arg;

    if (cert->verify_store) {
        CRYPTO_add(&cert->verify_store->references, 1,
                   CRYPTO_LOCK_X509_STORE);
        ret->verify_store = cert->verify_store;
    }

    if (cert->chain_store) {
        CRYPTO_add(&cert->chain_store->references, 1, CRYPTO_LOCK_X509_STORE);
        ret->chain_store = cert->chain_store;
    }

    ret->ciphers_raw = NULL;

#ifndef OPENSSL_NO_TLSEXT
    if (!custom_exts_copy(&ret->cli_ext, &cert->cli_ext))
        goto err;
    if (!custom_exts_copy(&ret->srv_ext, &cert->srv_ext))
        goto err;
#endif

    return (ret);

 err:
#ifndef OPENSSL_NO_RSA
    if (ret->rsa_tmp != NULL)
        RSA_free(ret->rsa_tmp);
#endif
#ifndef OPENSSL_NO_DH
    if (ret->dh_tmp != NULL)
        DH_free(ret->dh_tmp);
#endif
#ifndef OPENSSL_NO_ECDH
    if (ret->ecdh_tmp != NULL)
        EC_KEY_free(ret->ecdh_tmp);
#endif

#ifndef OPENSSL_NO_TLSEXT
    custom_exts_free(&ret->cli_ext);
    custom_exts_free(&ret->srv_ext);
#endif

    ssl_cert_clear_certs(ret);
    OPENSSL_free(ret);

    return NULL;
}

/* Free up and clear all certificates and chains */

void ssl_cert_clear_certs(CERT *c)
{
    int i;
    if (c == NULL)
        return;
    for (i = 0; i < SSL_PKEY_NUM; i++) {
        CERT_PKEY *cpk = c->pkeys + i;
        if (cpk->x509) {
            X509_free(cpk->x509);
            cpk->x509 = NULL;
        }
        if (cpk->privatekey) {
            EVP_PKEY_free(cpk->privatekey);
            cpk->privatekey = NULL;
        }
        if (cpk->chain) {
            sk_X509_pop_free(cpk->chain, X509_free);
            cpk->chain = NULL;
        }
#ifndef OPENSSL_NO_TLSEXT
        if (cpk->serverinfo) {
            OPENSSL_free(cpk->serverinfo);
            cpk->serverinfo = NULL;
            cpk->serverinfo_length = 0;
        }
#endif
        /* Clear all flags apart from explicit sign */
        cpk->valid_flags &= CERT_PKEY_EXPLICIT_SIGN;
    }
}

void ssl_cert_free(CERT *c)
{
    int i;

    if (c == NULL)
        return;

    i = CRYPTO_add(&c->references, -1, CRYPTO_LOCK_SSL_CERT);
#ifdef REF_PRINT
    REF_PRINT("CERT", c);
#endif
    if (i > 0)
        return;
#ifdef REF_CHECK
    if (i < 0) {
        fprintf(stderr, "ssl_cert_free, bad reference count\n");
        abort();                /* ok */
    }
#endif

#ifndef OPENSSL_NO_RSA
    if (c->rsa_tmp)
        RSA_free(c->rsa_tmp);
#endif
#ifndef OPENSSL_NO_DH
    if (c->dh_tmp)
        DH_free(c->dh_tmp);
#endif
#ifndef OPENSSL_NO_ECDH
    if (c->ecdh_tmp)
        EC_KEY_free(c->ecdh_tmp);
#endif

    ssl_cert_clear_certs(c);
    if (c->peer_sigalgs)
        OPENSSL_free(c->peer_sigalgs);
    if (c->conf_sigalgs)
        OPENSSL_free(c->conf_sigalgs);
    if (c->client_sigalgs)
        OPENSSL_free(c->client_sigalgs);
    if (c->shared_sigalgs)
        OPENSSL_free(c->shared_sigalgs);
    if (c->ctypes)
        OPENSSL_free(c->ctypes);
    if (c->verify_store)
        X509_STORE_free(c->verify_store);
    if (c->chain_store)
        X509_STORE_free(c->chain_store);
    if (c->ciphers_raw)
        OPENSSL_free(c->ciphers_raw);
#ifndef OPENSSL_NO_TLSEXT
    custom_exts_free(&c->cli_ext);
    custom_exts_free(&c->srv_ext);
    if (c->alpn_proposed)
        OPENSSL_free(c->alpn_proposed);
#endif
    OPENSSL_free(c);
}

int ssl_cert_inst(CERT **o)
{
    /*
     * Create a CERT if there isn't already one (which cannot really happen,
     * as it is initially created in SSL_CTX_new; but the earlier code
     * usually allows for that one being non-existant, so we follow that
     * behaviour, as it might turn out that there actually is a reason for it
     * -- but I'm not sure that *all* of the existing code could cope with
     * s->cert being NULL, otherwise we could do without the initialization
     * in SSL_CTX_new).
     */

    if (o == NULL) {
        SSLerr(SSL_F_SSL_CERT_INST, ERR_R_PASSED_NULL_PARAMETER);
        return (0);
    }
    if (*o == NULL) {
        if ((*o = ssl_cert_new()) == NULL) {
            SSLerr(SSL_F_SSL_CERT_INST, ERR_R_MALLOC_FAILURE);
            return (0);
        }
    }
    return (1);
}

int ssl_cert_set0_chain(CERT *c, STACK_OF(X509) *chain)
{
    CERT_PKEY *cpk = c->key;
    if (!cpk)
        return 0;
    if (cpk->chain)
        sk_X509_pop_free(cpk->chain, X509_free);
    cpk->chain = chain;
    return 1;
}

int ssl_cert_set1_chain(CERT *c, STACK_OF(X509) *chain)
{
    STACK_OF(X509) *dchain;
    if (!chain)
        return ssl_cert_set0_chain(c, NULL);
    dchain = X509_chain_up_ref(chain);
    if (!dchain)
        return 0;
    if (!ssl_cert_set0_chain(c, dchain)) {
        sk_X509_pop_free(dchain, X509_free);
        return 0;
    }
    return 1;
}

int ssl_cert_add0_chain_cert(CERT *c, X509 *x)
{
    CERT_PKEY *cpk = c->key;
    if (!cpk)
        return 0;
    if (!cpk->chain)
        cpk->chain = sk_X509_new_null();
    if (!cpk->chain || !sk_X509_push(cpk->chain, x))
        return 0;
    return 1;
}

int ssl_cert_add1_chain_cert(CERT *c, X509 *x)
{
    if (!ssl_cert_add0_chain_cert(c, x))
        return 0;
    CRYPTO_add(&x->references, 1, CRYPTO_LOCK_X509);
    return 1;
}

int ssl_cert_select_current(CERT *c, X509 *x)
{
    int i;
    if (x == NULL)
        return 0;
    for (i = 0; i < SSL_PKEY_NUM; i++) {
        CERT_PKEY *cpk = c->pkeys + i;
        if (cpk->x509 == x && cpk->privatekey) {
            c->key = cpk;
            return 1;
        }
    }

    for (i = 0; i < SSL_PKEY_NUM; i++) {
        CERT_PKEY *cpk = c->pkeys + i;
        if (cpk->privatekey && cpk->x509 && !X509_cmp(cpk->x509, x)) {
            c->key = cpk;
            return 1;
        }
    }
    return 0;
}

int ssl_cert_set_current(CERT *c, long op)
{
    int i, idx;
    if (!c)
        return 0;
    if (op == SSL_CERT_SET_FIRST)
        idx = 0;
    else if (op == SSL_CERT_SET_NEXT) {
        idx = (int)(c->key - c->pkeys + 1);
        if (idx >= SSL_PKEY_NUM)
            return 0;
    } else
        return 0;
    for (i = idx; i < SSL_PKEY_NUM; i++) {
        CERT_PKEY *cpk = c->pkeys + i;
        if (cpk->x509 && cpk->privatekey) {
            c->key = cpk;
            return 1;
        }
    }
    return 0;
}

void ssl_cert_set_cert_cb(CERT *c, int (*cb) (SSL *ssl, void *arg), void *arg)
{
    c->cert_cb = cb;
    c->cert_cb_arg = arg;
}

SESS_CERT *ssl_sess_cert_new(void)
{
    SESS_CERT *ret;

    ret = OPENSSL_malloc(sizeof *ret);
    if (ret == NULL) {
        SSLerr(SSL_F_SSL_SESS_CERT_NEW, ERR_R_MALLOC_FAILURE);
        return NULL;
    }

    memset(ret, 0, sizeof *ret);
    ret->peer_key = &(ret->peer_pkeys[SSL_PKEY_RSA_ENC]);
    ret->references = 1;

    return ret;
}

void ssl_sess_cert_free(SESS_CERT *sc)
{
    int i;

    if (sc == NULL)
        return;

    i = CRYPTO_add(&sc->references, -1, CRYPTO_LOCK_SSL_SESS_CERT);
#ifdef REF_PRINT
    REF_PRINT("SESS_CERT", sc);
#endif
    if (i > 0)
        return;
#ifdef REF_CHECK
    if (i < 0) {
        fprintf(stderr, "ssl_sess_cert_free, bad reference count\n");
        abort();                /* ok */
    }
#endif

    /* i == 0 */
    if (sc->cert_chain != NULL)
        sk_X509_pop_free(sc->cert_chain, X509_free);
    for (i = 0; i < SSL_PKEY_NUM; i++) {
        if (sc->peer_pkeys[i].x509 != NULL)
            X509_free(sc->peer_pkeys[i].x509);
#if 0                           /* We don't have the peer's private key.
                                 * These lines are just * here as a reminder
                                 * that we're still using a
                                 * not-quite-appropriate * data structure. */
        if (sc->peer_pkeys[i].privatekey != NULL)
            EVP_PKEY_free(sc->peer_pkeys[i].privatekey);
#endif
    }

#ifndef OPENSSL_NO_RSA
    if (sc->peer_rsa_tmp != NULL)
        RSA_free(sc->peer_rsa_tmp);
#endif
#ifndef OPENSSL_NO_DH
    if (sc->peer_dh_tmp != NULL)
        DH_free(sc->peer_dh_tmp);
#endif
#ifndef OPENSSL_NO_ECDH
    if (sc->peer_ecdh_tmp != NULL)
        EC_KEY_free(sc->peer_ecdh_tmp);
#endif

    OPENSSL_free(sc);
}

int ssl_set_peer_cert_type(SESS_CERT *sc, int type)
{
    sc->peer_cert_type = type;
    return (1);
}

int ssl_verify_cert_chain(SSL *s, STACK_OF(X509) *sk)
{
    X509 *x;
    int i;
    X509_STORE *verify_store;
    X509_STORE_CTX ctx;

    if (s->cert->verify_store)
        verify_store = s->cert->verify_store;
    else
        verify_store = s->ctx->cert_store;

    if ((sk == NULL) || (sk_X509_num(sk) == 0))
        return (0);

    x = sk_X509_value(sk, 0);
    if (!X509_STORE_CTX_init(&ctx, verify_store, x, sk)) {
        SSLerr(SSL_F_SSL_VERIFY_CERT_CHAIN, ERR_R_X509_LIB);
        return (0);
    }
    /* Set suite B flags if needed */
    X509_STORE_CTX_set_flags(&ctx, tls1_suiteb(s));
#if 0
    if (SSL_get_verify_depth(s) >= 0)
        X509_STORE_CTX_set_depth(&ctx, SSL_get_verify_depth(s));
#endif
    X509_STORE_CTX_set_ex_data(&ctx, SSL_get_ex_data_X509_STORE_CTX_idx(), s);

    /*
     * We need to inherit the verify parameters. These can be determined by
     * the context: if its a server it will verify SSL client certificates or
     * vice versa.
     */

    X509_STORE_CTX_set_default(&ctx, s->server ? "ssl_client" : "ssl_server");
    /*
     * Anything non-default in "param" should overwrite anything in the ctx.
     */
    X509_VERIFY_PARAM_set1(X509_STORE_CTX_get0_param(&ctx), s->param);

    if (s->verify_callback)
        X509_STORE_CTX_set_verify_cb(&ctx, s->verify_callback);

    if (s->ctx->app_verify_callback != NULL)
#if 1                           /* new with OpenSSL 0.9.7 */
        i = s->ctx->app_verify_callback(&ctx, s->ctx->app_verify_arg);
#else
        i = s->ctx->app_verify_callback(&ctx); /* should pass app_verify_arg */
#endif
    else {
#ifndef OPENSSL_NO_X509_VERIFY
        i = X509_verify_cert(&ctx);
#else
        i = 0;
        ctx.error = X509_V_ERR_APPLICATION_VERIFICATION;
        SSLerr(SSL_F_SSL_VERIFY_CERT_CHAIN, SSL_R_NO_VERIFY_CALLBACK);
#endif
    }

    s->verify_result = ctx.error;
    X509_STORE_CTX_cleanup(&ctx);

    return (i);
}

static void set_client_CA_list(STACK_OF(X509_NAME) **ca_list,
                               STACK_OF(X509_NAME) *name_list)
{
    if (*ca_list != NULL)
        sk_X509_NAME_pop_free(*ca_list, X509_NAME_free);

    *ca_list = name_list;
}

STACK_OF(X509_NAME) *SSL_dup_CA_list(STACK_OF(X509_NAME) *sk)
{
    int i;
    STACK_OF(X509_NAME) *ret;
    X509_NAME *name;

    ret = sk_X509_NAME_new_null();
    for (i = 0; i < sk_X509_NAME_num(sk); i++) {
        name = X509_NAME_dup(sk_X509_NAME_value(sk, i));
        if ((name == NULL) || !sk_X509_NAME_push(ret, name)) {
            sk_X509_NAME_pop_free(ret, X509_NAME_free);
            return (NULL);
        }
    }
    return (ret);
}

void SSL_set_client_CA_list(SSL *s, STACK_OF(X509_NAME) *name_list)
{
    set_client_CA_list(&(s->client_CA), name_list);
}

void SSL_CTX_set_client_CA_list(SSL_CTX *ctx, STACK_OF(X509_NAME) *name_list)
{
    set_client_CA_list(&(ctx->client_CA), name_list);
}

STACK_OF(X509_NAME) *SSL_CTX_get_client_CA_list(const SSL_CTX *ctx)
{
    return (ctx->client_CA);
}

STACK_OF(X509_NAME) *SSL_get_client_CA_list(const SSL *s)
{
    if (s->type == SSL_ST_CONNECT) { /* we are in the client */
        if (((s->version >> 8) == SSL3_VERSION_MAJOR) && (s->s3 != NULL))
            return (s->s3->tmp.ca_names);
        else
            return (NULL);
    } else {
        if (s->client_CA != NULL)
            return (s->client_CA);
        else
            return (s->ctx->client_CA);
    }
}

static int add_client_CA(STACK_OF(X509_NAME) **sk, X509 *x)
{
    X509_NAME *name;

    if (x == NULL)
        return (0);
    if ((*sk == NULL) && ((*sk = sk_X509_NAME_new_null()) == NULL))
        return (0);

    if ((name = X509_NAME_dup(X509_get_subject_name(x))) == NULL)
        return (0);

    if (!sk_X509_NAME_push(*sk, name)) {
        X509_NAME_free(name);
        return (0);
    }
    return (1);
}

int SSL_add_client_CA(SSL *ssl, X509 *x)
{
    return (add_client_CA(&(ssl->client_CA), x));
}

int SSL_CTX_add_client_CA(SSL_CTX *ctx, X509 *x)
{
    return (add_client_CA(&(ctx->client_CA), x));
}

static int xname_cmp(const X509_NAME *const *a, const X509_NAME *const *b)
{
    return (X509_NAME_cmp(*a, *b));
}

#ifndef OPENSSL_NO_STDIO
/**
 * Load CA certs from a file into a ::STACK. Note that it is somewhat misnamed;
 * it doesn't really have anything to do with clients (except that a common use
 * for a stack of CAs is to send it to the client). Actually, it doesn't have
 * much to do with CAs, either, since it will load any old cert.
 * \param file the file containing one or more certs.
 * \return a ::STACK containing the certs.
 */
STACK_OF(X509_NAME) *SSL_load_client_CA_file(const char *file)
{
    BIO *in;
    X509 *x = NULL;
    X509_NAME *xn = NULL;
    STACK_OF(X509_NAME) *ret = NULL, *sk;

    sk = sk_X509_NAME_new(xname_cmp);

    in = BIO_new(BIO_s_file_internal());

    if ((sk == NULL) || (in == NULL)) {
        SSLerr(SSL_F_SSL_LOAD_CLIENT_CA_FILE, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    if (!BIO_read_filename(in, file))
        goto err;

    for (;;) {
        if (PEM_read_bio_X509(in, &x, NULL, NULL) == NULL)
            break;
        if (ret == NULL) {
            ret = sk_X509_NAME_new_null();
            if (ret == NULL) {
                SSLerr(SSL_F_SSL_LOAD_CLIENT_CA_FILE, ERR_R_MALLOC_FAILURE);
                goto err;
            }
        }
        if ((xn = X509_get_subject_name(x)) == NULL)
            goto err;
        /* check for duplicates */
        xn = X509_NAME_dup(xn);
        if (xn == NULL)
            goto err;
        if (sk_X509_NAME_find(sk, xn) >= 0)
            X509_NAME_free(xn);
        else {
            sk_X509_NAME_push(sk, xn);
            sk_X509_NAME_push(ret, xn);
        }
    }

    if (0) {
 err:
        if (ret != NULL)
            sk_X509_NAME_pop_free(ret, X509_NAME_free);
        ret = NULL;
    }
    if (sk != NULL)
        sk_X509_NAME_free(sk);
    if (in != NULL)
        BIO_free(in);
    if (x != NULL)
        X509_free(x);
    if (ret != NULL)
        ERR_clear_error();
    return (ret);
}
#endif

/**
 * Add a file of certs to a stack.
 * \param stack the stack to add to.
 * \param file the file to add from. All certs in this file that are not
 * already in the stack will be added.
 * \return 1 for success, 0 for failure. Note that in the case of failure some
 * certs may have been added to \c stack.
 */

int SSL_add_file_cert_subjects_to_stack(STACK_OF(X509_NAME) *stack,
                                        const char *file)
{
    BIO *in;
    X509 *x = NULL;
    X509_NAME *xn = NULL;
    int ret = 1;
    int (*oldcmp) (const X509_NAME *const *a, const X509_NAME *const *b);

    oldcmp = sk_X509_NAME_set_cmp_func(stack, xname_cmp);

    in = BIO_new(BIO_s_file_internal());

    if (in == NULL) {
        SSLerr(SSL_F_SSL_ADD_FILE_CERT_SUBJECTS_TO_STACK,
               ERR_R_MALLOC_FAILURE);
        goto err;
    }

    if (!BIO_read_filename(in, file))
        goto err;

    for (;;) {
        if (PEM_read_bio_X509(in, &x, NULL, NULL) == NULL)
            break;
        if ((xn = X509_get_subject_name(x)) == NULL)
            goto err;
        xn = X509_NAME_dup(xn);
        if (xn == NULL)
            goto err;
        if (sk_X509_NAME_find(stack, xn) >= 0)
            X509_NAME_free(xn);
        else
            sk_X509_NAME_push(stack, xn);
    }

    ERR_clear_error();

    if (0) {
 err:
        ret = 0;
    }
    if (in != NULL)
        BIO_free(in);
    if (x != NULL)
        X509_free(x);

    (void)sk_X509_NAME_set_cmp_func(stack, oldcmp);

    return ret;
}

/**
 * Add a directory of certs to a stack.
 * \param stack the stack to append to.
 * \param dir the directory to append from. All files in this directory will be
 * examined as potential certs. Any that are acceptable to
 * SSL_add_dir_cert_subjects_to_stack() that are not already in the stack will be
 * included.
 * \return 1 for success, 0 for failure. Note that in the case of failure some
 * certs may have been added to \c stack.
 */

int SSL_add_dir_cert_subjects_to_stack(STACK_OF(X509_NAME) *stack,
                                       const char *dir)
{
    OPENSSL_DIR_CTX *d = NULL;
    const char *filename;
    int ret = 0;

    CRYPTO_w_lock(CRYPTO_LOCK_READDIR);

    /* Note that a side effect is that the CAs will be sorted by name */

    while ((filename = OPENSSL_DIR_read(&d, dir))) {
        char buf[1024];
        int r;

        if (strlen(dir) + strlen(filename) + 2 > sizeof buf) {
            SSLerr(SSL_F_SSL_ADD_DIR_CERT_SUBJECTS_TO_STACK,
                   SSL_R_PATH_TOO_LONG);
            goto err;
        }
#ifdef OPENSSL_SYS_VMS
        r = BIO_snprintf(buf, sizeof buf, "%s%s", dir, filename);
#else
        r = BIO_snprintf(buf, sizeof buf, "%s/%s", dir, filename);
#endif
        if (r <= 0 || r >= (int)sizeof(buf))
            goto err;
        if (!SSL_add_file_cert_subjects_to_stack(stack, buf))
            goto err;
    }

    if (errno) {
        SYSerr(SYS_F_OPENDIR, get_last_sys_error());
        ERR_add_error_data(3, "OPENSSL_DIR_read(&ctx, '", dir, "')");
        SSLerr(SSL_F_SSL_ADD_DIR_CERT_SUBJECTS_TO_STACK, ERR_R_SYS_LIB);
        goto err;
    }

    ret = 1;

 err:
    if (d)
        OPENSSL_DIR_end(&d);
    CRYPTO_w_unlock(CRYPTO_LOCK_READDIR);
    return ret;
}

/* Add a certificate to a BUF_MEM structure */

static int ssl_add_cert_to_buf(BUF_MEM *buf, unsigned long *l, X509 *x)
{
    int n;
    unsigned char *p;

    n = i2d_X509(x, NULL);
    if (n < 0 || !BUF_MEM_grow_clean(buf, (int)(n + (*l) + 3))) {
        SSLerr(SSL_F_SSL_ADD_CERT_TO_BUF, ERR_R_BUF_LIB);
        return 0;
    }
    p = (unsigned char *)&(buf->data[*l]);
    l2n3(n, p);
    n = i2d_X509(x, &p);
    if (n < 0) {
        /* Shouldn't happen */
        SSLerr(SSL_F_SSL_ADD_CERT_TO_BUF, ERR_R_BUF_LIB);
        return 0;
    }
    *l += n + 3;

    return 1;
}

/* Add certificate chain to internal SSL BUF_MEM strcuture */
int ssl_add_cert_chain(SSL *s, CERT_PKEY *cpk, unsigned long *l)
{
    BUF_MEM *buf = s->init_buf;
    int no_chain;
    int i;

    X509 *x;
    STACK_OF(X509) *extra_certs;
    X509_STORE *chain_store;

    if (cpk)
        x = cpk->x509;
    else
        x = NULL;

    if (s->cert->chain_store)
        chain_store = s->cert->chain_store;
    else
        chain_store = s->ctx->cert_store;

    /*
     * If we have a certificate specific chain use it, else use parent ctx.
     */
    if (cpk && cpk->chain)
        extra_certs = cpk->chain;
    else
        extra_certs = s->ctx->extra_certs;

    if ((s->mode & SSL_MODE_NO_AUTO_CHAIN) || extra_certs)
        no_chain = 1;
    else
        no_chain = 0;

    /* TLSv1 sends a chain with nothing in it, instead of an alert */
    if (!BUF_MEM_grow_clean(buf, 10)) {
        SSLerr(SSL_F_SSL_ADD_CERT_CHAIN, ERR_R_BUF_LIB);
        return 0;
    }
    if (x != NULL) {
        if (no_chain) {
            if (!ssl_add_cert_to_buf(buf, l, x))
                return 0;
        } else {
            X509_STORE_CTX xs_ctx;

            if (!X509_STORE_CTX_init(&xs_ctx, chain_store, x, NULL)) {
                SSLerr(SSL_F_SSL_ADD_CERT_CHAIN, ERR_R_X509_LIB);
                return (0);
            }
            X509_verify_cert(&xs_ctx);
            /* Don't leave errors in the queue */
            ERR_clear_error();
            for (i = 0; i < sk_X509_num(xs_ctx.chain); i++) {
                x = sk_X509_value(xs_ctx.chain, i);

                if (!ssl_add_cert_to_buf(buf, l, x)) {
                    X509_STORE_CTX_cleanup(&xs_ctx);
                    return 0;
                }
            }
            X509_STORE_CTX_cleanup(&xs_ctx);
        }
    }
    for (i = 0; i < sk_X509_num(extra_certs); i++) {
        x = sk_X509_value(extra_certs, i);
        if (!ssl_add_cert_to_buf(buf, l, x))
            return 0;
    }

    return 1;
}

/* Build a certificate chain for current certificate */
int ssl_build_cert_chain(CERT *c, X509_STORE *chain_store, int flags)
{
    CERT_PKEY *cpk = c->key;
    X509_STORE_CTX xs_ctx;
    STACK_OF(X509) *chain = NULL, *untrusted = NULL;
    X509 *x;
    int i, rv = 0;
    unsigned long error;

    if (!cpk->x509) {
        SSLerr(SSL_F_SSL_BUILD_CERT_CHAIN, SSL_R_NO_CERTIFICATE_SET);
        goto err;
    }
    /* Rearranging and check the chain: add everything to a store */
    if (flags & SSL_BUILD_CHAIN_FLAG_CHECK) {
        chain_store = X509_STORE_new();
        if (!chain_store)
            goto err;
        for (i = 0; i < sk_X509_num(cpk->chain); i++) {
            x = sk_X509_value(cpk->chain, i);
            if (!X509_STORE_add_cert(chain_store, x)) {
                error = ERR_peek_last_error();
                if (ERR_GET_LIB(error) != ERR_LIB_X509 ||
                    ERR_GET_REASON(error) !=
                    X509_R_CERT_ALREADY_IN_HASH_TABLE)
                    goto err;
                ERR_clear_error();
            }
        }
        /* Add EE cert too: it might be self signed */
        if (!X509_STORE_add_cert(chain_store, cpk->x509)) {
            error = ERR_peek_last_error();
            if (ERR_GET_LIB(error) != ERR_LIB_X509 ||
                ERR_GET_REASON(error) != X509_R_CERT_ALREADY_IN_HASH_TABLE)
                goto err;
            ERR_clear_error();
        }
    } else {
        if (c->chain_store)
            chain_store = c->chain_store;

        if (flags & SSL_BUILD_CHAIN_FLAG_UNTRUSTED)
            untrusted = cpk->chain;
    }

    if (!X509_STORE_CTX_init(&xs_ctx, chain_store, cpk->x509, untrusted)) {
        SSLerr(SSL_F_SSL_BUILD_CERT_CHAIN, ERR_R_X509_LIB);
        goto err;
    }
    /* Set suite B flags if needed */
    X509_STORE_CTX_set_flags(&xs_ctx,
                             c->cert_flags & SSL_CERT_FLAG_SUITEB_128_LOS);

    i = X509_verify_cert(&xs_ctx);
    if (i <= 0 && flags & SSL_BUILD_CHAIN_FLAG_IGNORE_ERROR) {
        if (flags & SSL_BUILD_CHAIN_FLAG_CLEAR_ERROR)
            ERR_clear_error();
        i = 1;
        rv = 2;
    }
    if (i > 0)
        chain = X509_STORE_CTX_get1_chain(&xs_ctx);
    if (i <= 0) {
        SSLerr(SSL_F_SSL_BUILD_CERT_CHAIN, SSL_R_CERTIFICATE_VERIFY_FAILED);
        i = X509_STORE_CTX_get_error(&xs_ctx);
        ERR_add_error_data(2, "Verify error:",
                           X509_verify_cert_error_string(i));

        X509_STORE_CTX_cleanup(&xs_ctx);
        goto err;
    }
    X509_STORE_CTX_cleanup(&xs_ctx);
    if (cpk->chain)
        sk_X509_pop_free(cpk->chain, X509_free);
    /* Remove EE certificate from chain */
    x = sk_X509_shift(chain);
    X509_free(x);
    if (flags & SSL_BUILD_CHAIN_FLAG_NO_ROOT) {
        if (sk_X509_num(chain) > 0) {
            /* See if last cert is self signed */
            x = sk_X509_value(chain, sk_X509_num(chain) - 1);
            X509_check_purpose(x, -1, 0);
            if (x->ex_flags & EXFLAG_SS) {
                x = sk_X509_pop(chain);
                X509_free(x);
            }
        }
    }
    cpk->chain = chain;
    if (rv == 0)
        rv = 1;
 err:
    if (flags & SSL_BUILD_CHAIN_FLAG_CHECK)
        X509_STORE_free(chain_store);

    return rv;
}

int ssl_cert_set_cert_store(CERT *c, X509_STORE *store, int chain, int ref)
{
    X509_STORE **pstore;
    if (chain)
        pstore = &c->chain_store;
    else
        pstore = &c->verify_store;
    if (*pstore)
        X509_STORE_free(*pstore);
    *pstore = store;
    if (ref && store)
        CRYPTO_add(&store->references, 1, CRYPTO_LOCK_X509_STORE);
    return 1;
}
