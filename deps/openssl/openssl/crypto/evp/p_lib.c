/* crypto/evp/p_lib.c */
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

#include <stdio.h>
#include "cryptlib.h"
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/objects.h>
#include <openssl/evp.h>
#include <openssl/asn1_mac.h>
#include <openssl/x509.h>
#ifndef OPENSSL_NO_RSA
# include <openssl/rsa.h>
#endif
#ifndef OPENSSL_NO_DSA
# include <openssl/dsa.h>
#endif
#ifndef OPENSSL_NO_DH
# include <openssl/dh.h>
#endif

#ifndef OPENSSL_NO_ENGINE
# include <openssl/engine.h>
#endif

#include "asn1_locl.h"

static void EVP_PKEY_free_it(EVP_PKEY *x);

int EVP_PKEY_bits(EVP_PKEY *pkey)
{
    if (pkey && pkey->ameth && pkey->ameth->pkey_bits)
        return pkey->ameth->pkey_bits(pkey);
    return 0;
}

int EVP_PKEY_size(EVP_PKEY *pkey)
{
    if (pkey && pkey->ameth && pkey->ameth->pkey_size)
        return pkey->ameth->pkey_size(pkey);
    return 0;
}

int EVP_PKEY_save_parameters(EVP_PKEY *pkey, int mode)
{
#ifndef OPENSSL_NO_DSA
    if (pkey->type == EVP_PKEY_DSA) {
        int ret = pkey->save_parameters;

        if (mode >= 0)
            pkey->save_parameters = mode;
        return (ret);
    }
#endif
#ifndef OPENSSL_NO_EC
    if (pkey->type == EVP_PKEY_EC) {
        int ret = pkey->save_parameters;

        if (mode >= 0)
            pkey->save_parameters = mode;
        return (ret);
    }
#endif
    return (0);
}

int EVP_PKEY_copy_parameters(EVP_PKEY *to, const EVP_PKEY *from)
{
    if (to->type != from->type) {
        EVPerr(EVP_F_EVP_PKEY_COPY_PARAMETERS, EVP_R_DIFFERENT_KEY_TYPES);
        goto err;
    }

    if (EVP_PKEY_missing_parameters(from)) {
        EVPerr(EVP_F_EVP_PKEY_COPY_PARAMETERS, EVP_R_MISSING_PARAMETERS);
        goto err;
    }
    if (from->ameth && from->ameth->param_copy)
        return from->ameth->param_copy(to, from);
 err:
    return 0;
}

int EVP_PKEY_missing_parameters(const EVP_PKEY *pkey)
{
    if (pkey->ameth && pkey->ameth->param_missing)
        return pkey->ameth->param_missing(pkey);
    return 0;
}

int EVP_PKEY_cmp_parameters(const EVP_PKEY *a, const EVP_PKEY *b)
{
    if (a->type != b->type)
        return -1;
    if (a->ameth && a->ameth->param_cmp)
        return a->ameth->param_cmp(a, b);
    return -2;
}

int EVP_PKEY_cmp(const EVP_PKEY *a, const EVP_PKEY *b)
{
    if (a->type != b->type)
        return -1;

    if (a->ameth) {
        int ret;
        /* Compare parameters if the algorithm has them */
        if (a->ameth->param_cmp) {
            ret = a->ameth->param_cmp(a, b);
            if (ret <= 0)
                return ret;
        }

        if (a->ameth->pub_cmp)
            return a->ameth->pub_cmp(a, b);
    }

    return -2;
}

EVP_PKEY *EVP_PKEY_new(void)
{
    EVP_PKEY *ret;

    ret = (EVP_PKEY *)OPENSSL_malloc(sizeof(EVP_PKEY));
    if (ret == NULL) {
        EVPerr(EVP_F_EVP_PKEY_NEW, ERR_R_MALLOC_FAILURE);
        return (NULL);
    }
    ret->type = EVP_PKEY_NONE;
    ret->save_type = EVP_PKEY_NONE;
    ret->references = 1;
    ret->ameth = NULL;
    ret->engine = NULL;
    ret->pkey.ptr = NULL;
    ret->attributes = NULL;
    ret->save_parameters = 1;
    return (ret);
}

/*
 * Setup a public key ASN1 method and ENGINE from a NID or a string. If pkey
 * is NULL just return 1 or 0 if the algorithm exists.
 */

static int pkey_set_type(EVP_PKEY *pkey, int type, const char *str, int len)
{
    const EVP_PKEY_ASN1_METHOD *ameth;
    ENGINE *e = NULL;
    if (pkey) {
        if (pkey->pkey.ptr)
            EVP_PKEY_free_it(pkey);
        /*
         * If key type matches and a method exists then this lookup has
         * succeeded once so just indicate success.
         */
        if ((type == pkey->save_type) && pkey->ameth)
            return 1;
#ifndef OPENSSL_NO_ENGINE
        /* If we have an ENGINE release it */
        if (pkey->engine) {
            ENGINE_finish(pkey->engine);
            pkey->engine = NULL;
        }
#endif
    }
    if (str)
        ameth = EVP_PKEY_asn1_find_str(&e, str, len);
    else
        ameth = EVP_PKEY_asn1_find(&e, type);
#ifndef OPENSSL_NO_ENGINE
    if (!pkey && e)
        ENGINE_finish(e);
#endif
    if (!ameth) {
        EVPerr(EVP_F_PKEY_SET_TYPE, EVP_R_UNSUPPORTED_ALGORITHM);
        return 0;
    }
    if (pkey) {
        pkey->ameth = ameth;
        pkey->engine = e;

        pkey->type = pkey->ameth->pkey_id;
        pkey->save_type = type;
    }
    return 1;
}

int EVP_PKEY_set_type(EVP_PKEY *pkey, int type)
{
    return pkey_set_type(pkey, type, NULL, -1);
}

int EVP_PKEY_set_type_str(EVP_PKEY *pkey, const char *str, int len)
{
    return pkey_set_type(pkey, EVP_PKEY_NONE, str, len);
}

int EVP_PKEY_assign(EVP_PKEY *pkey, int type, void *key)
{
    if (!EVP_PKEY_set_type(pkey, type))
        return 0;
    pkey->pkey.ptr = key;
    return (key != NULL);
}

void *EVP_PKEY_get0(EVP_PKEY *pkey)
{
    return pkey->pkey.ptr;
}

#ifndef OPENSSL_NO_RSA
int EVP_PKEY_set1_RSA(EVP_PKEY *pkey, RSA *key)
{
    int ret = EVP_PKEY_assign_RSA(pkey, key);
    if (ret)
        RSA_up_ref(key);
    return ret;
}

RSA *EVP_PKEY_get1_RSA(EVP_PKEY *pkey)
{
    if (pkey->type != EVP_PKEY_RSA) {
        EVPerr(EVP_F_EVP_PKEY_GET1_RSA, EVP_R_EXPECTING_AN_RSA_KEY);
        return NULL;
    }
    RSA_up_ref(pkey->pkey.rsa);
    return pkey->pkey.rsa;
}
#endif

#ifndef OPENSSL_NO_DSA
int EVP_PKEY_set1_DSA(EVP_PKEY *pkey, DSA *key)
{
    int ret = EVP_PKEY_assign_DSA(pkey, key);
    if (ret)
        DSA_up_ref(key);
    return ret;
}

DSA *EVP_PKEY_get1_DSA(EVP_PKEY *pkey)
{
    if (pkey->type != EVP_PKEY_DSA) {
        EVPerr(EVP_F_EVP_PKEY_GET1_DSA, EVP_R_EXPECTING_A_DSA_KEY);
        return NULL;
    }
    DSA_up_ref(pkey->pkey.dsa);
    return pkey->pkey.dsa;
}
#endif

#ifndef OPENSSL_NO_EC

int EVP_PKEY_set1_EC_KEY(EVP_PKEY *pkey, EC_KEY *key)
{
    int ret = EVP_PKEY_assign_EC_KEY(pkey, key);
    if (ret)
        EC_KEY_up_ref(key);
    return ret;
}

EC_KEY *EVP_PKEY_get1_EC_KEY(EVP_PKEY *pkey)
{
    if (pkey->type != EVP_PKEY_EC) {
        EVPerr(EVP_F_EVP_PKEY_GET1_EC_KEY, EVP_R_EXPECTING_A_EC_KEY);
        return NULL;
    }
    EC_KEY_up_ref(pkey->pkey.ec);
    return pkey->pkey.ec;
}
#endif

#ifndef OPENSSL_NO_DH

int EVP_PKEY_set1_DH(EVP_PKEY *pkey, DH *key)
{
    int ret = EVP_PKEY_assign_DH(pkey, key);
    if (ret)
        DH_up_ref(key);
    return ret;
}

DH *EVP_PKEY_get1_DH(EVP_PKEY *pkey)
{
    if (pkey->type != EVP_PKEY_DH && pkey->type != EVP_PKEY_DHX) {
        EVPerr(EVP_F_EVP_PKEY_GET1_DH, EVP_R_EXPECTING_A_DH_KEY);
        return NULL;
    }
    DH_up_ref(pkey->pkey.dh);
    return pkey->pkey.dh;
}
#endif

int EVP_PKEY_type(int type)
{
    int ret;
    const EVP_PKEY_ASN1_METHOD *ameth;
    ENGINE *e;
    ameth = EVP_PKEY_asn1_find(&e, type);
    if (ameth)
        ret = ameth->pkey_id;
    else
        ret = NID_undef;
#ifndef OPENSSL_NO_ENGINE
    if (e)
        ENGINE_finish(e);
#endif
    return ret;
}

int EVP_PKEY_id(const EVP_PKEY *pkey)
{
    return pkey->type;
}

int EVP_PKEY_base_id(const EVP_PKEY *pkey)
{
    return EVP_PKEY_type(pkey->type);
}

void EVP_PKEY_free(EVP_PKEY *x)
{
    int i;

    if (x == NULL)
        return;

    i = CRYPTO_add(&x->references, -1, CRYPTO_LOCK_EVP_PKEY);
#ifdef REF_PRINT
    REF_PRINT("EVP_PKEY", x);
#endif
    if (i > 0)
        return;
#ifdef REF_CHECK
    if (i < 0) {
        fprintf(stderr, "EVP_PKEY_free, bad reference count\n");
        abort();
    }
#endif
    EVP_PKEY_free_it(x);
    if (x->attributes)
        sk_X509_ATTRIBUTE_pop_free(x->attributes, X509_ATTRIBUTE_free);
    OPENSSL_free(x);
}

static void EVP_PKEY_free_it(EVP_PKEY *x)
{
    if (x->ameth && x->ameth->pkey_free) {
        x->ameth->pkey_free(x);
        x->pkey.ptr = NULL;
    }
#ifndef OPENSSL_NO_ENGINE
    if (x->engine) {
        ENGINE_finish(x->engine);
        x->engine = NULL;
    }
#endif
}

static int unsup_alg(BIO *out, const EVP_PKEY *pkey, int indent,
                     const char *kstr)
{
    BIO_indent(out, indent, 128);
    BIO_printf(out, "%s algorithm \"%s\" unsupported\n",
               kstr, OBJ_nid2ln(pkey->type));
    return 1;
}

int EVP_PKEY_print_public(BIO *out, const EVP_PKEY *pkey,
                          int indent, ASN1_PCTX *pctx)
{
    if (pkey->ameth && pkey->ameth->pub_print)
        return pkey->ameth->pub_print(out, pkey, indent, pctx);

    return unsup_alg(out, pkey, indent, "Public Key");
}

int EVP_PKEY_print_private(BIO *out, const EVP_PKEY *pkey,
                           int indent, ASN1_PCTX *pctx)
{
    if (pkey->ameth && pkey->ameth->priv_print)
        return pkey->ameth->priv_print(out, pkey, indent, pctx);

    return unsup_alg(out, pkey, indent, "Private Key");
}

int EVP_PKEY_print_params(BIO *out, const EVP_PKEY *pkey,
                          int indent, ASN1_PCTX *pctx)
{
    if (pkey->ameth && pkey->ameth->param_print)
        return pkey->ameth->param_print(out, pkey, indent, pctx);
    return unsup_alg(out, pkey, indent, "Parameters");
}

int EVP_PKEY_get_default_digest_nid(EVP_PKEY *pkey, int *pnid)
{
    if (!pkey->ameth || !pkey->ameth->pkey_ctrl)
        return -2;
    return pkey->ameth->pkey_ctrl(pkey, ASN1_PKEY_CTRL_DEFAULT_MD_NID,
                                  0, pnid);
}
