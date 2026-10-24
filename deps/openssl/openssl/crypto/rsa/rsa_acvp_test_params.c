/*
 * Copyright 2020-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <string.h> /* memcpy */
#include <openssl/core_names.h>
#include <openssl/param_build.h>
#include "crypto/rsa.h"
#include "crypto/rsa_params.h"
#include "rsa_local.h"

int ossl_rsa_acvp_test_gen_params_new_parsed(OSSL_PARAM **dst,
    const RSA_PARAMS *params)
{
    OSSL_PARAM *d, *alloc = NULL;
    size_t i;
    int ret = 1;

    static const OSSL_PARAM settable[] = {
        OSSL_PARAM_BN(OSSL_PKEY_PARAM_RSA_TEST_XP, NULL, 0),
        OSSL_PARAM_BN(OSSL_PKEY_PARAM_RSA_TEST_XP1, NULL, 0),
        OSSL_PARAM_BN(OSSL_PKEY_PARAM_RSA_TEST_XP2, NULL, 0),
        OSSL_PARAM_BN(OSSL_PKEY_PARAM_RSA_TEST_XQ, NULL, 0),
        OSSL_PARAM_BN(OSSL_PKEY_PARAM_RSA_TEST_XQ1, NULL, 0),
        OSSL_PARAM_BN(OSSL_PKEY_PARAM_RSA_TEST_XQ2, NULL, 0),
        OSSL_PARAM_END
    };

    if (dst == NULL || params == NULL)
        return 0;

    const OSSL_PARAM *src[] = {
        params->fips.xp, params->fips.xp1, params->fips.xp2,
        params->fips.xq, params->fips.xq1, params->fips.xq2
    };

    /* Xp is required whenever the ACVP test interface is used. */
    if (src[0] == NULL)
        return 1;

    /* Zeroing here means the terminator is always set at the end */
    alloc = OPENSSL_zalloc(sizeof(settable));
    if (alloc == NULL)
        return 0;

    d = alloc;
    for (i = 0; i < OSSL_NELEM(src); i++) {
        if (src[i] != NULL) {
            *d = settable[i];
            d->data_size = src[i]->data_size;
            d->data = OPENSSL_memdup(src[i]->data, src[i]->data_size);
            if (d->data == NULL) {
                ret = 0;
                break;
            }
            d++;
        }
    }
    if (ret == 0) {
        ossl_rsa_acvp_test_gen_params_free(alloc);
        alloc = NULL;
    }
    if (*dst != NULL)
        ossl_rsa_acvp_test_gen_params_free(*dst);
    *dst = alloc;
    return ret;
}

int ossl_rsa_acvp_test_gen_params_new(OSSL_PARAM **dst,
    const OSSL_PARAM src[])
{
    RSA_PARAMS params;

    if (!rsa_acvp_input_decoder(src, &params))
        return 0;
    return ossl_rsa_acvp_test_gen_params_new_parsed(dst, &params);
}

void ossl_rsa_acvp_test_gen_params_free(OSSL_PARAM *dst)
{
    OSSL_PARAM *p;

    if (dst == NULL)
        return;

    for (p = dst; p->key != NULL; ++p) {
        OPENSSL_free(p->data);
        p->data = NULL;
    }
    OPENSSL_free(dst);
}

static int rsa_acvp_test_set_params_parsed(RSA *r, const RSA_PARAMS *p)
{
    RSA_ACVP_TEST *t;

    if (r == NULL || p == NULL)
        return 0;

    if (r->acvp_test != NULL) {
        ossl_rsa_acvp_test_free(r->acvp_test);
        r->acvp_test = NULL;
    }

    t = OPENSSL_zalloc(sizeof(*t));
    if (t == NULL)
        return 0;

    if (p->fips.xp1 != NULL && !OSSL_PARAM_get_BN(p->fips.xp1, &t->Xp1))
        goto err;
    if (p->fips.xp2 != NULL && !OSSL_PARAM_get_BN(p->fips.xp2, &t->Xp2))
        goto err;
    if (p->fips.xp != NULL && !OSSL_PARAM_get_BN(p->fips.xp, &t->Xp))
        goto err;
    if (p->fips.xq1 != NULL && !OSSL_PARAM_get_BN(p->fips.xq1, &t->Xq1))
        goto err;
    if (p->fips.xq2 != NULL && !OSSL_PARAM_get_BN(p->fips.xq2, &t->Xq2))
        goto err;
    if (p->fips.xq != NULL && !OSSL_PARAM_get_BN(p->fips.xq, &t->Xq))
        goto err;

    t->p1 = BN_new();
    t->p2 = BN_new();
    t->q1 = BN_new();
    t->q2 = BN_new();
    if (t->p1 == NULL || t->p2 == NULL || t->q1 == NULL || t->q2 == NULL)
        goto err;
    r->acvp_test = t;
    return 1;
err:
    ossl_rsa_acvp_test_free(t);
    return 0;
}

int ossl_rsa_acvp_test_set_params(RSA *r, const OSSL_PARAM params[])
{
    RSA_PARAMS p;

    if (!rsa_acvp_input_decoder(params, &p))
        return 0;
    return rsa_acvp_test_set_params_parsed(r, &p);
}

int ossl_rsa_acvp_test_get_params_parsed(RSA *r, const RSA_PARAMS *p)
{
    RSA_ACVP_TEST *t;

    if (r == NULL || p == NULL)
        return 0;

    t = r->acvp_test;
    if (t != NULL) {
        if (p->fips.p1 != NULL && !OSSL_PARAM_set_BN(p->fips.p1, t->p1))
            return 0;
        if (p->fips.p2 != NULL && !OSSL_PARAM_set_BN(p->fips.p2, t->p2))
            return 0;
        if (p->fips.q1 != NULL && !OSSL_PARAM_set_BN(p->fips.q1, t->q1))
            return 0;
        if (p->fips.q2 != NULL && !OSSL_PARAM_set_BN(p->fips.q2, t->q2))
            return 0;
    }
    return 1;
}

int ossl_rsa_acvp_test_get_params(RSA *r, OSSL_PARAM params[])
{
    RSA_PARAMS p;

    if (!rsa_acvp_output_decoder(params, &p))
        return 0;
    return ossl_rsa_acvp_test_get_params_parsed(r, &p);
}

void ossl_rsa_acvp_test_free(RSA_ACVP_TEST *t)
{
    if (t != NULL) {
        BN_free(t->Xp1);
        BN_free(t->Xp2);
        BN_free(t->Xp);
        BN_free(t->Xq1);
        BN_free(t->Xq2);
        BN_free(t->Xq);
        BN_free(t->p1);
        BN_free(t->p2);
        BN_free(t->q1);
        BN_free(t->q2);
        OPENSSL_free(t);
    }
}
