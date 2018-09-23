/*
 * Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <openssl/crypto.h>
#include "internal/cryptlib.h"
#include <openssl/lhash.h>
#include "internal/bn_int.h"
#include <openssl/engine.h>
#include "rsa_locl.h"

RSA *RSA_new(void)
{
    return RSA_new_method(NULL);
}

const RSA_METHOD *RSA_get_method(const RSA *rsa)
{
    return rsa->meth;
}

int RSA_set_method(RSA *rsa, const RSA_METHOD *meth)
{
    /*
     * NB: The caller is specifically setting a method, so it's not up to us
     * to deal with which ENGINE it comes from.
     */
    const RSA_METHOD *mtmp;
    mtmp = rsa->meth;
    if (mtmp->finish)
        mtmp->finish(rsa);
#ifndef OPENSSL_NO_ENGINE
    ENGINE_finish(rsa->engine);
    rsa->engine = NULL;
#endif
    rsa->meth = meth;
    if (meth->init)
        meth->init(rsa);
    return 1;
}

RSA *RSA_new_method(ENGINE *engine)
{
    RSA *ret = OPENSSL_zalloc(sizeof(*ret));

    if (ret == NULL) {
        RSAerr(RSA_F_RSA_NEW_METHOD, ERR_R_MALLOC_FAILURE);
        return NULL;
    }

    ret->references = 1;
    ret->lock = CRYPTO_THREAD_lock_new();
    if (ret->lock == NULL) {
        RSAerr(RSA_F_RSA_NEW_METHOD, ERR_R_MALLOC_FAILURE);
        OPENSSL_free(ret);
        return NULL;
    }

    ret->meth = RSA_get_default_method();
#ifndef OPENSSL_NO_ENGINE
    ret->flags = ret->meth->flags & ~RSA_FLAG_NON_FIPS_ALLOW;
    if (engine) {
        if (!ENGINE_init(engine)) {
            RSAerr(RSA_F_RSA_NEW_METHOD, ERR_R_ENGINE_LIB);
            goto err;
        }
        ret->engine = engine;
    } else
        ret->engine = ENGINE_get_default_RSA();
    if (ret->engine) {
        ret->meth = ENGINE_get_RSA(ret->engine);
        if (ret->meth == NULL) {
            RSAerr(RSA_F_RSA_NEW_METHOD, ERR_R_ENGINE_LIB);
            goto err;
        }
    }
#endif

    ret->flags = ret->meth->flags & ~RSA_FLAG_NON_FIPS_ALLOW;
    if (!CRYPTO_new_ex_data(CRYPTO_EX_INDEX_RSA, ret, &ret->ex_data)) {
        goto err;
    }

    if ((ret->meth->init != NULL) && !ret->meth->init(ret)) {
        RSAerr(RSA_F_RSA_NEW_METHOD, ERR_R_INIT_FAIL);
        goto err;
    }

    return ret;

err:
    RSA_free(ret);
    return NULL;
}

void RSA_free(RSA *r)
{
    int i;

    if (r == NULL)
        return;

    CRYPTO_atomic_add(&r->references, -1, &i, r->lock);
    REF_PRINT_COUNT("RSA", r);
    if (i > 0)
        return;
    REF_ASSERT_ISNT(i < 0);

    if (r->meth->finish)
        r->meth->finish(r);
#ifndef OPENSSL_NO_ENGINE
    ENGINE_finish(r->engine);
#endif

    CRYPTO_free_ex_data(CRYPTO_EX_INDEX_RSA, r, &r->ex_data);

    CRYPTO_THREAD_lock_free(r->lock);

    BN_clear_free(r->n);
    BN_clear_free(r->e);
    BN_clear_free(r->d);
    BN_clear_free(r->p);
    BN_clear_free(r->q);
    BN_clear_free(r->dmp1);
    BN_clear_free(r->dmq1);
    BN_clear_free(r->iqmp);
    BN_BLINDING_free(r->blinding);
    BN_BLINDING_free(r->mt_blinding);
    OPENSSL_free(r->bignum_data);
    OPENSSL_free(r);
}

int RSA_up_ref(RSA *r)
{
    int i;

    if (CRYPTO_atomic_add(&r->references, 1, &i, r->lock) <= 0)
        return 0;

    REF_PRINT_COUNT("RSA", r);
    REF_ASSERT_ISNT(i < 2);
    return ((i > 1) ? 1 : 0);
}

int RSA_set_ex_data(RSA *r, int idx, void *arg)
{
    return (CRYPTO_set_ex_data(&r->ex_data, idx, arg));
}

void *RSA_get_ex_data(const RSA *r, int idx)
{
    return (CRYPTO_get_ex_data(&r->ex_data, idx));
}

int RSA_security_bits(const RSA *rsa)
{
    return BN_security_bits(BN_num_bits(rsa->n), -1);
}

int RSA_set0_key(RSA *r, BIGNUM *n, BIGNUM *e, BIGNUM *d)
{
    /* If the fields n and e in r are NULL, the corresponding input
     * parameters MUST be non-NULL for n and e.  d may be
     * left NULL (in case only the public key is used).
     */
    if ((r->n == NULL && n == NULL)
        || (r->e == NULL && e == NULL))
        return 0;

    if (n != NULL) {
        BN_free(r->n);
        r->n = n;
    }
    if (e != NULL) {
        BN_free(r->e);
        r->e = e;
    }
    if (d != NULL) {
        BN_free(r->d);
        r->d = d;
    }

    return 1;
}

int RSA_set0_factors(RSA *r, BIGNUM *p, BIGNUM *q)
{
    /* If the fields p and q in r are NULL, the corresponding input
     * parameters MUST be non-NULL.
     */
    if ((r->p == NULL && p == NULL)
        || (r->q == NULL && q == NULL))
        return 0;

    if (p != NULL) {
        BN_free(r->p);
        r->p = p;
    }
    if (q != NULL) {
        BN_free(r->q);
        r->q = q;
    }

    return 1;
}

int RSA_set0_crt_params(RSA *r, BIGNUM *dmp1, BIGNUM *dmq1, BIGNUM *iqmp)
{
    /* If the fields dmp1, dmq1 and iqmp in r are NULL, the corresponding input
     * parameters MUST be non-NULL.
     */
    if ((r->dmp1 == NULL && dmp1 == NULL)
        || (r->dmq1 == NULL && dmq1 == NULL)
        || (r->iqmp == NULL && iqmp == NULL))
        return 0;

    if (dmp1 != NULL) {
        BN_free(r->dmp1);
        r->dmp1 = dmp1;
    }
    if (dmq1 != NULL) {
        BN_free(r->dmq1);
        r->dmq1 = dmq1;
    }
    if (iqmp != NULL) {
        BN_free(r->iqmp);
        r->iqmp = iqmp;
    }

    return 1;
}

void RSA_get0_key(const RSA *r,
                  const BIGNUM **n, const BIGNUM **e, const BIGNUM **d)
{
    if (n != NULL)
        *n = r->n;
    if (e != NULL)
        *e = r->e;
    if (d != NULL)
        *d = r->d;
}

void RSA_get0_factors(const RSA *r, const BIGNUM **p, const BIGNUM **q)
{
    if (p != NULL)
        *p = r->p;
    if (q != NULL)
        *q = r->q;
}

void RSA_get0_crt_params(const RSA *r,
                         const BIGNUM **dmp1, const BIGNUM **dmq1,
                         const BIGNUM **iqmp)
{
    if (dmp1 != NULL)
        *dmp1 = r->dmp1;
    if (dmq1 != NULL)
        *dmq1 = r->dmq1;
    if (iqmp != NULL)
        *iqmp = r->iqmp;
}

void RSA_clear_flags(RSA *r, int flags)
{
    r->flags &= ~flags;
}

int RSA_test_flags(const RSA *r, int flags)
{
    return r->flags & flags;
}

void RSA_set_flags(RSA *r, int flags)
{
    r->flags |= flags;
}

ENGINE *RSA_get0_engine(const RSA *r)
{
    return r->engine;
}
