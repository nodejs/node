/*
 * Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL project
 * 2006.
 */
/* ====================================================================
 * Copyright (c) 2006 The OpenSSL Project.  All rights reserved.
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
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
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

#include <stdio.h>
#include "cryptlib.h"
#include <openssl/asn1t.h>
#include <openssl/x509.h>
#ifndef OPENSSL_NO_ENGINE
# include <openssl/engine.h>
#endif
#include "asn1_locl.h"

extern const EVP_PKEY_ASN1_METHOD rsa_asn1_meths[];
extern const EVP_PKEY_ASN1_METHOD dsa_asn1_meths[];
extern const EVP_PKEY_ASN1_METHOD dh_asn1_meth;
extern const EVP_PKEY_ASN1_METHOD dhx_asn1_meth;
extern const EVP_PKEY_ASN1_METHOD eckey_asn1_meth;
extern const EVP_PKEY_ASN1_METHOD hmac_asn1_meth;
extern const EVP_PKEY_ASN1_METHOD cmac_asn1_meth;

/* Keep this sorted in type order !! */
static const EVP_PKEY_ASN1_METHOD *standard_methods[] = {
#ifndef OPENSSL_NO_RSA
    &rsa_asn1_meths[0],
    &rsa_asn1_meths[1],
#endif
#ifndef OPENSSL_NO_DH
    &dh_asn1_meth,
#endif
#ifndef OPENSSL_NO_DSA
    &dsa_asn1_meths[0],
    &dsa_asn1_meths[1],
    &dsa_asn1_meths[2],
    &dsa_asn1_meths[3],
    &dsa_asn1_meths[4],
#endif
#ifndef OPENSSL_NO_EC
    &eckey_asn1_meth,
#endif
    &hmac_asn1_meth,
    &cmac_asn1_meth,
#ifndef OPENSSL_NO_DH
    &dhx_asn1_meth
#endif
};

typedef int sk_cmp_fn_type(const char *const *a, const char *const *b);
DECLARE_STACK_OF(EVP_PKEY_ASN1_METHOD)
static STACK_OF(EVP_PKEY_ASN1_METHOD) *app_methods = NULL;

#ifdef TEST
void main()
{
    int i;
    for (i = 0;
         i < sizeof(standard_methods) / sizeof(EVP_PKEY_ASN1_METHOD *); i++)
        fprintf(stderr, "Number %d id=%d (%s)\n", i,
                standard_methods[i]->pkey_id,
                OBJ_nid2sn(standard_methods[i]->pkey_id));
}
#endif

DECLARE_OBJ_BSEARCH_CMP_FN(const EVP_PKEY_ASN1_METHOD *,
                           const EVP_PKEY_ASN1_METHOD *, ameth);

static int ameth_cmp(const EVP_PKEY_ASN1_METHOD *const *a,
                     const EVP_PKEY_ASN1_METHOD *const *b)
{
    return ((*a)->pkey_id - (*b)->pkey_id);
}

IMPLEMENT_OBJ_BSEARCH_CMP_FN(const EVP_PKEY_ASN1_METHOD *,
                             const EVP_PKEY_ASN1_METHOD *, ameth);

int EVP_PKEY_asn1_get_count(void)
{
    int num = sizeof(standard_methods) / sizeof(EVP_PKEY_ASN1_METHOD *);
    if (app_methods)
        num += sk_EVP_PKEY_ASN1_METHOD_num(app_methods);
    return num;
}

const EVP_PKEY_ASN1_METHOD *EVP_PKEY_asn1_get0(int idx)
{
    int num = sizeof(standard_methods) / sizeof(EVP_PKEY_ASN1_METHOD *);
    if (idx < 0)
        return NULL;
    if (idx < num)
        return standard_methods[idx];
    idx -= num;
    return sk_EVP_PKEY_ASN1_METHOD_value(app_methods, idx);
}

static const EVP_PKEY_ASN1_METHOD *pkey_asn1_find(int type)
{
    EVP_PKEY_ASN1_METHOD tmp;
    const EVP_PKEY_ASN1_METHOD *t = &tmp, **ret;
    tmp.pkey_id = type;
    if (app_methods) {
        int idx;
        idx = sk_EVP_PKEY_ASN1_METHOD_find(app_methods, &tmp);
        if (idx >= 0)
            return sk_EVP_PKEY_ASN1_METHOD_value(app_methods, idx);
    }
    ret = OBJ_bsearch_ameth(&t, standard_methods, sizeof(standard_methods)
                            / sizeof(EVP_PKEY_ASN1_METHOD *));
    if (!ret || !*ret)
        return NULL;
    return *ret;
}

/*
 * Find an implementation of an ASN1 algorithm. If 'pe' is not NULL also
 * search through engines and set *pe to a functional reference to the engine
 * implementing 'type' or NULL if no engine implements it.
 */

const EVP_PKEY_ASN1_METHOD *EVP_PKEY_asn1_find(ENGINE **pe, int type)
{
    const EVP_PKEY_ASN1_METHOD *t;

    for (;;) {
        t = pkey_asn1_find(type);
        if (!t || !(t->pkey_flags & ASN1_PKEY_ALIAS))
            break;
        type = t->pkey_base_id;
    }
    if (pe) {
#ifndef OPENSSL_NO_ENGINE
        ENGINE *e;
        /* type will contain the final unaliased type */
        e = ENGINE_get_pkey_asn1_meth_engine(type);
        if (e) {
            *pe = e;
            return ENGINE_get_pkey_asn1_meth(e, type);
        }
#endif
        *pe = NULL;
    }
    return t;
}

const EVP_PKEY_ASN1_METHOD *EVP_PKEY_asn1_find_str(ENGINE **pe,
                                                   const char *str, int len)
{
    int i;
    const EVP_PKEY_ASN1_METHOD *ameth;
    if (len == -1)
        len = strlen(str);
    if (pe) {
#ifndef OPENSSL_NO_ENGINE
        ENGINE *e;
        ameth = ENGINE_pkey_asn1_find_str(&e, str, len);
        if (ameth) {
            /*
             * Convert structural into functional reference
             */
            if (!ENGINE_init(e))
                ameth = NULL;
            ENGINE_free(e);
            *pe = e;
            return ameth;
        }
#endif
        *pe = NULL;
    }
    for (i = 0; i < EVP_PKEY_asn1_get_count(); i++) {
        ameth = EVP_PKEY_asn1_get0(i);
        if (ameth->pkey_flags & ASN1_PKEY_ALIAS)
            continue;
        if (((int)strlen(ameth->pem_str) == len) &&
            !strncasecmp(ameth->pem_str, str, len))
            return ameth;
    }
    return NULL;
}

int EVP_PKEY_asn1_add0(const EVP_PKEY_ASN1_METHOD *ameth)
{
    if (app_methods == NULL) {
        app_methods = sk_EVP_PKEY_ASN1_METHOD_new(ameth_cmp);
        if (!app_methods)
            return 0;
    }
    if (!sk_EVP_PKEY_ASN1_METHOD_push(app_methods, ameth))
        return 0;
    sk_EVP_PKEY_ASN1_METHOD_sort(app_methods);
    return 1;
}

int EVP_PKEY_asn1_add_alias(int to, int from)
{
    EVP_PKEY_ASN1_METHOD *ameth;
    ameth = EVP_PKEY_asn1_new(from, ASN1_PKEY_ALIAS, NULL, NULL);
    if (!ameth)
        return 0;
    ameth->pkey_base_id = to;
    if (!EVP_PKEY_asn1_add0(ameth)) {
        EVP_PKEY_asn1_free(ameth);
        return 0;
    }
    return 1;
}

int EVP_PKEY_asn1_get0_info(int *ppkey_id, int *ppkey_base_id,
                            int *ppkey_flags, const char **pinfo,
                            const char **ppem_str,
                            const EVP_PKEY_ASN1_METHOD *ameth)
{
    if (!ameth)
        return 0;
    if (ppkey_id)
        *ppkey_id = ameth->pkey_id;
    if (ppkey_base_id)
        *ppkey_base_id = ameth->pkey_base_id;
    if (ppkey_flags)
        *ppkey_flags = ameth->pkey_flags;
    if (pinfo)
        *pinfo = ameth->info;
    if (ppem_str)
        *ppem_str = ameth->pem_str;
    return 1;
}

const EVP_PKEY_ASN1_METHOD *EVP_PKEY_get0_asn1(EVP_PKEY *pkey)
{
    return pkey->ameth;
}

EVP_PKEY_ASN1_METHOD *EVP_PKEY_asn1_new(int id, int flags,
                                        const char *pem_str, const char *info)
{
    EVP_PKEY_ASN1_METHOD *ameth;
    ameth = OPENSSL_malloc(sizeof(EVP_PKEY_ASN1_METHOD));
    if (!ameth)
        return NULL;

    memset(ameth, 0, sizeof(EVP_PKEY_ASN1_METHOD));

    ameth->pkey_id = id;
    ameth->pkey_base_id = id;
    ameth->pkey_flags = flags | ASN1_PKEY_DYNAMIC;

    if (info) {
        ameth->info = BUF_strdup(info);
        if (!ameth->info)
            goto err;
    } else
        ameth->info = NULL;

    if (pem_str) {
        ameth->pem_str = BUF_strdup(pem_str);
        if (!ameth->pem_str)
            goto err;
    } else
        ameth->pem_str = NULL;

    ameth->pub_decode = 0;
    ameth->pub_encode = 0;
    ameth->pub_cmp = 0;
    ameth->pub_print = 0;

    ameth->priv_decode = 0;
    ameth->priv_encode = 0;
    ameth->priv_print = 0;

    ameth->old_priv_encode = 0;
    ameth->old_priv_decode = 0;

    ameth->item_verify = 0;
    ameth->item_sign = 0;

    ameth->pkey_size = 0;
    ameth->pkey_bits = 0;

    ameth->param_decode = 0;
    ameth->param_encode = 0;
    ameth->param_missing = 0;
    ameth->param_copy = 0;
    ameth->param_cmp = 0;
    ameth->param_print = 0;

    ameth->pkey_free = 0;
    ameth->pkey_ctrl = 0;

    return ameth;

 err:

    EVP_PKEY_asn1_free(ameth);
    return NULL;

}

void EVP_PKEY_asn1_copy(EVP_PKEY_ASN1_METHOD *dst,
                        const EVP_PKEY_ASN1_METHOD *src)
{

    dst->pub_decode = src->pub_decode;
    dst->pub_encode = src->pub_encode;
    dst->pub_cmp = src->pub_cmp;
    dst->pub_print = src->pub_print;

    dst->priv_decode = src->priv_decode;
    dst->priv_encode = src->priv_encode;
    dst->priv_print = src->priv_print;

    dst->old_priv_encode = src->old_priv_encode;
    dst->old_priv_decode = src->old_priv_decode;

    dst->pkey_size = src->pkey_size;
    dst->pkey_bits = src->pkey_bits;

    dst->param_decode = src->param_decode;
    dst->param_encode = src->param_encode;
    dst->param_missing = src->param_missing;
    dst->param_copy = src->param_copy;
    dst->param_cmp = src->param_cmp;
    dst->param_print = src->param_print;

    dst->pkey_free = src->pkey_free;
    dst->pkey_ctrl = src->pkey_ctrl;

    dst->item_sign = src->item_sign;
    dst->item_verify = src->item_verify;

}

void EVP_PKEY_asn1_free(EVP_PKEY_ASN1_METHOD *ameth)
{
    if (ameth && (ameth->pkey_flags & ASN1_PKEY_DYNAMIC)) {
        if (ameth->pem_str)
            OPENSSL_free(ameth->pem_str);
        if (ameth->info)
            OPENSSL_free(ameth->info);
        OPENSSL_free(ameth);
    }
}

void EVP_PKEY_asn1_set_public(EVP_PKEY_ASN1_METHOD *ameth,
                              int (*pub_decode) (EVP_PKEY *pk,
                                                 X509_PUBKEY *pub),
                              int (*pub_encode) (X509_PUBKEY *pub,
                                                 const EVP_PKEY *pk),
                              int (*pub_cmp) (const EVP_PKEY *a,
                                              const EVP_PKEY *b),
                              int (*pub_print) (BIO *out,
                                                const EVP_PKEY *pkey,
                                                int indent, ASN1_PCTX *pctx),
                              int (*pkey_size) (const EVP_PKEY *pk),
                              int (*pkey_bits) (const EVP_PKEY *pk))
{
    ameth->pub_decode = pub_decode;
    ameth->pub_encode = pub_encode;
    ameth->pub_cmp = pub_cmp;
    ameth->pub_print = pub_print;
    ameth->pkey_size = pkey_size;
    ameth->pkey_bits = pkey_bits;
}

void EVP_PKEY_asn1_set_private(EVP_PKEY_ASN1_METHOD *ameth,
                               int (*priv_decode) (EVP_PKEY *pk,
                                                   PKCS8_PRIV_KEY_INFO
                                                   *p8inf),
                               int (*priv_encode) (PKCS8_PRIV_KEY_INFO *p8,
                                                   const EVP_PKEY *pk),
                               int (*priv_print) (BIO *out,
                                                  const EVP_PKEY *pkey,
                                                  int indent,
                                                  ASN1_PCTX *pctx))
{
    ameth->priv_decode = priv_decode;
    ameth->priv_encode = priv_encode;
    ameth->priv_print = priv_print;
}

void EVP_PKEY_asn1_set_param(EVP_PKEY_ASN1_METHOD *ameth,
                             int (*param_decode) (EVP_PKEY *pkey,
                                                  const unsigned char **pder,
                                                  int derlen),
                             int (*param_encode) (const EVP_PKEY *pkey,
                                                  unsigned char **pder),
                             int (*param_missing) (const EVP_PKEY *pk),
                             int (*param_copy) (EVP_PKEY *to,
                                                const EVP_PKEY *from),
                             int (*param_cmp) (const EVP_PKEY *a,
                                               const EVP_PKEY *b),
                             int (*param_print) (BIO *out,
                                                 const EVP_PKEY *pkey,
                                                 int indent, ASN1_PCTX *pctx))
{
    ameth->param_decode = param_decode;
    ameth->param_encode = param_encode;
    ameth->param_missing = param_missing;
    ameth->param_copy = param_copy;
    ameth->param_cmp = param_cmp;
    ameth->param_print = param_print;
}

void EVP_PKEY_asn1_set_free(EVP_PKEY_ASN1_METHOD *ameth,
                            void (*pkey_free) (EVP_PKEY *pkey))
{
    ameth->pkey_free = pkey_free;
}

void EVP_PKEY_asn1_set_ctrl(EVP_PKEY_ASN1_METHOD *ameth,
                            int (*pkey_ctrl) (EVP_PKEY *pkey, int op,
                                              long arg1, void *arg2))
{
    ameth->pkey_ctrl = pkey_ctrl;
}

void EVP_PKEY_asn1_set_item(EVP_PKEY_ASN1_METHOD *ameth,
                            int (*item_verify) (EVP_MD_CTX *ctx,
                                                const ASN1_ITEM *it,
                                                void *asn,
                                                X509_ALGOR *a,
                                                ASN1_BIT_STRING *sig,
                                                EVP_PKEY *pkey),
                            int (*item_sign) (EVP_MD_CTX *ctx,
                                              const ASN1_ITEM *it,
                                              void *asn,
                                              X509_ALGOR *alg1,
                                              X509_ALGOR *alg2,
                                              ASN1_BIT_STRING *sig))
{
    ameth->item_sign = item_sign;
    ameth->item_verify = item_verify;
}
