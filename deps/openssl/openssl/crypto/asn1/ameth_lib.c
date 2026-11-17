/*
 * Copyright 2006-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "internal/cryptlib.h"
#include <stdio.h>
#include <openssl/asn1t.h>
#include <openssl/x509.h>
#include "crypto/asn1.h"
#include "crypto/evp.h"

#include "standard_methods.h"

typedef int sk_cmp_fn_type(const char *const *a, const char *const *b);

DECLARE_OBJ_BSEARCH_CMP_FN(const EVP_PKEY_ASN1_METHOD *,
    const EVP_PKEY_ASN1_METHOD *, ameth);

static int ameth_cmp(const EVP_PKEY_ASN1_METHOD *const *a,
    const EVP_PKEY_ASN1_METHOD *const *b)
{
    return ((*a)->pkey_id - (*b)->pkey_id);
}

IMPLEMENT_OBJ_BSEARCH_CMP_FN(const EVP_PKEY_ASN1_METHOD *,
    const EVP_PKEY_ASN1_METHOD *, ameth);

int evp_pkey_asn1_get_count(void)
{
    int num = OSSL_NELEM(standard_methods);
    return num;
}

const EVP_PKEY_ASN1_METHOD *evp_pkey_asn1_get0(int idx)
{
    int num = OSSL_NELEM(standard_methods);

    if (idx < 0 || idx >= num)
        return NULL;

    return standard_methods[idx];
}

static const EVP_PKEY_ASN1_METHOD *pkey_asn1_find(int type)
{
    EVP_PKEY_ASN1_METHOD tmp;
    const EVP_PKEY_ASN1_METHOD *t = &tmp, **ret;

    tmp.pkey_id = type;
    ret = OBJ_bsearch_ameth(&t, standard_methods, OSSL_NELEM(standard_methods));
    if (ret == NULL || *ret == NULL)
        return NULL;
    return *ret;
}

/*
 * Return ASN1 method for desired `type`, returns NULL if no method is found for
 * `type`. If pe is not NULL, the function will set *pe to NULL to indicate no
 * engine is used.
 */
const EVP_PKEY_ASN1_METHOD *evp_pkey_asn1_find(int type)
{
    const EVP_PKEY_ASN1_METHOD *t;

    for (;;) {
        t = pkey_asn1_find(type);
        if (!t || !(t->pkey_flags & ASN1_PKEY_ALIAS))
            break;
        type = t->pkey_base_id;
    }
    return t;
}

const EVP_PKEY_ASN1_METHOD *evp_pkey_asn1_find_str(const char *str, int len)
{
    int i;
    const EVP_PKEY_ASN1_METHOD *ameth = NULL;

    if (len == -1)
        len = (int)strlen(str);
    for (i = evp_pkey_asn1_get_count(); i-- > 0;) {
        ameth = evp_pkey_asn1_get0(i);
        if (ameth->pkey_flags & ASN1_PKEY_ALIAS)
            continue;
        if ((int)strlen(ameth->pem_str) == len
            && OPENSSL_strncasecmp(ameth->pem_str, str, len) == 0)
            return ameth;
    }
    return NULL;
}

int evp_pkey_asn1_get0_info(int *ppkey_id, int *ppkey_base_id,
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

const EVP_PKEY_ASN1_METHOD *evp_pkey_get0_asn1(const EVP_PKEY *pkey)
{
    return pkey->ameth;
}
