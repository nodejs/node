/* ====================================================================
 * Copyright (c) 2006-2018 The OpenSSL Project.  All rights reserved.
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

#include "eng_int.h"
#include "asn1_locl.h"
#include <openssl/evp.h>

/*
 * If this symbol is defined then ENGINE_get_pkey_asn1_meth_engine(), the
 * function that is used by EVP to hook in pkey_asn1_meth code and cache
 * defaults (etc), will display brief debugging summaries to stderr with the
 * 'nid'.
 */
/* #define ENGINE_PKEY_ASN1_METH_DEBUG */

static ENGINE_TABLE *pkey_asn1_meth_table = NULL;

void ENGINE_unregister_pkey_asn1_meths(ENGINE *e)
{
    engine_table_unregister(&pkey_asn1_meth_table, e);
}

static void engine_unregister_all_pkey_asn1_meths(void)
{
    engine_table_cleanup(&pkey_asn1_meth_table);
}

int ENGINE_register_pkey_asn1_meths(ENGINE *e)
{
    if (e->pkey_asn1_meths) {
        const int *nids;
        int num_nids = e->pkey_asn1_meths(e, NULL, &nids, 0);
        if (num_nids > 0)
            return engine_table_register(&pkey_asn1_meth_table,
                                         engine_unregister_all_pkey_asn1_meths,
                                         e, nids, num_nids, 0);
    }
    return 1;
}

void ENGINE_register_all_pkey_asn1_meths(void)
{
    ENGINE *e;

    for (e = ENGINE_get_first(); e; e = ENGINE_get_next(e))
        ENGINE_register_pkey_asn1_meths(e);
}

int ENGINE_set_default_pkey_asn1_meths(ENGINE *e)
{
    if (e->pkey_asn1_meths) {
        const int *nids;
        int num_nids = e->pkey_asn1_meths(e, NULL, &nids, 0);
        if (num_nids > 0)
            return engine_table_register(&pkey_asn1_meth_table,
                                         engine_unregister_all_pkey_asn1_meths,
                                         e, nids, num_nids, 1);
    }
    return 1;
}

/*
 * Exposed API function to get a functional reference from the implementation
 * table (ie. try to get a functional reference from the tabled structural
 * references) for a given pkey_asn1_meth 'nid'
 */
ENGINE *ENGINE_get_pkey_asn1_meth_engine(int nid)
{
    return engine_table_select(&pkey_asn1_meth_table, nid);
}

/*
 * Obtains a pkey_asn1_meth implementation from an ENGINE functional
 * reference
 */
const EVP_PKEY_ASN1_METHOD *ENGINE_get_pkey_asn1_meth(ENGINE *e, int nid)
{
    EVP_PKEY_ASN1_METHOD *ret;
    ENGINE_PKEY_ASN1_METHS_PTR fn = ENGINE_get_pkey_asn1_meths(e);
    if (!fn || !fn(e, &ret, NULL, nid)) {
        ENGINEerr(ENGINE_F_ENGINE_GET_PKEY_ASN1_METH,
                  ENGINE_R_UNIMPLEMENTED_PUBLIC_KEY_METHOD);
        return NULL;
    }
    return ret;
}

/* Gets the pkey_asn1_meth callback from an ENGINE structure */
ENGINE_PKEY_ASN1_METHS_PTR ENGINE_get_pkey_asn1_meths(const ENGINE *e)
{
    return e->pkey_asn1_meths;
}

/* Sets the pkey_asn1_meth callback in an ENGINE structure */
int ENGINE_set_pkey_asn1_meths(ENGINE *e, ENGINE_PKEY_ASN1_METHS_PTR f)
{
    e->pkey_asn1_meths = f;
    return 1;
}

/*
 * Internal function to free up EVP_PKEY_ASN1_METHOD structures before an
 * ENGINE is destroyed
 */

void engine_pkey_asn1_meths_free(ENGINE *e)
{
    int i;
    EVP_PKEY_ASN1_METHOD *pkm;
    if (e->pkey_asn1_meths) {
        const int *pknids;
        int npknids;
        npknids = e->pkey_asn1_meths(e, NULL, &pknids, 0);
        for (i = 0; i < npknids; i++) {
            if (e->pkey_asn1_meths(e, &pkm, NULL, pknids[i])) {
                EVP_PKEY_asn1_free(pkm);
            }
        }
    }
}

/*
 * Find a method based on a string. This does a linear search through all
 * implemented algorithms. This is OK in practice because only a small number
 * of algorithms are likely to be implemented in an engine and it is not used
 * for speed critical operations.
 */

const EVP_PKEY_ASN1_METHOD *ENGINE_get_pkey_asn1_meth_str(ENGINE *e,
                                                          const char *str,
                                                          int len)
{
    int i, nidcount;
    const int *nids;
    EVP_PKEY_ASN1_METHOD *ameth;
    if (!e->pkey_asn1_meths)
        return NULL;
    if (len == -1)
        len = strlen(str);
    nidcount = e->pkey_asn1_meths(e, NULL, &nids, 0);
    for (i = 0; i < nidcount; i++) {
        e->pkey_asn1_meths(e, &ameth, NULL, nids[i]);
        if (((int)strlen(ameth->pem_str) == len) &&
            !strncasecmp(ameth->pem_str, str, len))
            return ameth;
    }
    return NULL;
}

typedef struct {
    ENGINE *e;
    const EVP_PKEY_ASN1_METHOD *ameth;
    const char *str;
    int len;
} ENGINE_FIND_STR;

static void look_str_cb(int nid, STACK_OF(ENGINE) *sk, ENGINE *def, void *arg)
{
    ENGINE_FIND_STR *lk = arg;
    int i;
    if (lk->ameth)
        return;
    for (i = 0; i < sk_ENGINE_num(sk); i++) {
        ENGINE *e = sk_ENGINE_value(sk, i);
        EVP_PKEY_ASN1_METHOD *ameth;
        e->pkey_asn1_meths(e, &ameth, NULL, nid);
        if (ameth != NULL && ((int)strlen(ameth->pem_str) == lk->len) &&
            !strncasecmp(ameth->pem_str, lk->str, lk->len)) {
            lk->e = e;
            lk->ameth = ameth;
            return;
        }
    }
}

const EVP_PKEY_ASN1_METHOD *ENGINE_pkey_asn1_find_str(ENGINE **pe,
                                                      const char *str,
                                                      int len)
{
    ENGINE_FIND_STR fstr;
    fstr.e = NULL;
    fstr.ameth = NULL;
    fstr.str = str;
    fstr.len = len;
    CRYPTO_w_lock(CRYPTO_LOCK_ENGINE);
    engine_table_doall(pkey_asn1_meth_table, look_str_cb, &fstr);
    /* If found obtain a structural reference to engine */
    if (fstr.e) {
        fstr.e->struct_ref++;
        engine_ref_debug(fstr.e, 0, 1)
    }
    *pe = fstr.e;
    CRYPTO_w_unlock(CRYPTO_LOCK_ENGINE);
    return fstr.ameth;
}
