/*
 * Copyright 1995-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include "internal/cryptlib.h"
#include "dh_local.h"
#include "crypto/bn.h"

static int generate_key(DH *dh);
static int compute_key(unsigned char *key, const BIGNUM *pub_key, DH *dh);
static int dh_bn_mod_exp(const DH *dh, BIGNUM *r,
                         const BIGNUM *a, const BIGNUM *p,
                         const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *m_ctx);
static int dh_init(DH *dh);
static int dh_finish(DH *dh);

int DH_generate_key(DH *dh)
{
    return dh->meth->generate_key(dh);
}

/*-
 * NB: This function is inherently not constant time due to the
 * RFC 5246 (8.1.2) padding style that strips leading zero bytes.
 */
int DH_compute_key(unsigned char *key, const BIGNUM *pub_key, DH *dh)
{
    int ret = 0, i;
    volatile size_t npad = 0, mask = 1;

    /* compute the key; ret is constant unless compute_key is external */
    if ((ret = dh->meth->compute_key(key, pub_key, dh)) <= 0)
        return ret;

    /* count leading zero bytes, yet still touch all bytes */
    for (i = 0; i < ret; i++) {
        mask &= !key[i];
        npad += mask;
    }

    /* unpad key */
    ret -= npad;
    /* key-dependent memory access, potentially leaking npad / ret */
    memmove(key, key + npad, ret);
    /* key-dependent memory access, potentially leaking npad / ret */
    memset(key + ret, 0, npad);

    return ret;
}

int DH_compute_key_padded(unsigned char *key, const BIGNUM *pub_key, DH *dh)
{
    int rv, pad;

    /* rv is constant unless compute_key is external */
    rv = dh->meth->compute_key(key, pub_key, dh);
    if (rv <= 0)
        return rv;
    pad = BN_num_bytes(dh->p) - rv;
    /* pad is constant (zero) unless compute_key is external */
    if (pad > 0) {
        memmove(key + pad, key, rv);
        memset(key, 0, pad);
    }
    return rv + pad;
}

static DH_METHOD dh_ossl = {
    "OpenSSL DH Method",
    generate_key,
    compute_key,
    dh_bn_mod_exp,
    dh_init,
    dh_finish,
    DH_FLAG_FIPS_METHOD,
    NULL,
    NULL
};

static const DH_METHOD *default_DH_method = &dh_ossl;

const DH_METHOD *DH_OpenSSL(void)
{
    return &dh_ossl;
}

void DH_set_default_method(const DH_METHOD *meth)
{
    default_DH_method = meth;
}

const DH_METHOD *DH_get_default_method(void)
{
    return default_DH_method;
}

static int generate_key(DH *dh)
{
    int ok = 0;
    int generate_new_key = 0;
    unsigned l;
    BN_CTX *ctx = NULL;
    BN_MONT_CTX *mont = NULL;
    BIGNUM *pub_key = NULL, *priv_key = NULL;

    if (BN_num_bits(dh->p) > OPENSSL_DH_MAX_MODULUS_BITS) {
        DHerr(DH_F_GENERATE_KEY, DH_R_MODULUS_TOO_LARGE);
        return 0;
    }

    ctx = BN_CTX_new();
    if (ctx == NULL)
        goto err;

    if (dh->priv_key == NULL) {
        priv_key = BN_secure_new();
        if (priv_key == NULL)
            goto err;
        generate_new_key = 1;
    } else
        priv_key = dh->priv_key;

    if (dh->pub_key == NULL) {
        pub_key = BN_new();
        if (pub_key == NULL)
            goto err;
    } else
        pub_key = dh->pub_key;

    if (dh->flags & DH_FLAG_CACHE_MONT_P) {
        mont = BN_MONT_CTX_set_locked(&dh->method_mont_p,
                                      dh->lock, dh->p, ctx);
        if (!mont)
            goto err;
    }

    if (generate_new_key) {
        if (dh->q) {
            do {
                if (!BN_priv_rand_range(priv_key, dh->q))
                    goto err;
            }
            while (BN_is_zero(priv_key) || BN_is_one(priv_key));
        } else {
            /* secret exponent length */
            l = dh->length ? dh->length : BN_num_bits(dh->p) - 1;
            if (!BN_priv_rand(priv_key, l, BN_RAND_TOP_ONE, BN_RAND_BOTTOM_ANY))
                goto err;
            /*
             * We handle just one known case where g is a quadratic non-residue:
             * for g = 2: p % 8 == 3
             */
            if (BN_is_word(dh->g, DH_GENERATOR_2) && !BN_is_bit_set(dh->p, 2)) {
                /* clear bit 0, since it won't be a secret anyway */
                if (!BN_clear_bit(priv_key, 0))
                    goto err;
            }
        }
    }

    {
        BIGNUM *prk = BN_new();

        if (prk == NULL)
            goto err;
        BN_with_flags(prk, priv_key, BN_FLG_CONSTTIME);

        if (!dh->meth->bn_mod_exp(dh, pub_key, dh->g, prk, dh->p, ctx, mont)) {
            BN_clear_free(prk);
            goto err;
        }
        /* We MUST free prk before any further use of priv_key */
        BN_clear_free(prk);
    }

    dh->pub_key = pub_key;
    dh->priv_key = priv_key;
    ok = 1;
 err:
    if (ok != 1)
        DHerr(DH_F_GENERATE_KEY, ERR_R_BN_LIB);

    if (pub_key != dh->pub_key)
        BN_free(pub_key);
    if (priv_key != dh->priv_key)
        BN_free(priv_key);
    BN_CTX_free(ctx);
    return ok;
}

static int compute_key(unsigned char *key, const BIGNUM *pub_key, DH *dh)
{
    BN_CTX *ctx = NULL;
    BN_MONT_CTX *mont = NULL;
    BIGNUM *tmp;
    int ret = -1;
    int check_result;

    if (BN_num_bits(dh->p) > OPENSSL_DH_MAX_MODULUS_BITS) {
        DHerr(DH_F_COMPUTE_KEY, DH_R_MODULUS_TOO_LARGE);
        goto err;
    }

    ctx = BN_CTX_new();
    if (ctx == NULL)
        goto err;
    BN_CTX_start(ctx);
    tmp = BN_CTX_get(ctx);
    if (tmp == NULL)
        goto err;

    if (dh->priv_key == NULL) {
        DHerr(DH_F_COMPUTE_KEY, DH_R_NO_PRIVATE_VALUE);
        goto err;
    }

    if (dh->flags & DH_FLAG_CACHE_MONT_P) {
        mont = BN_MONT_CTX_set_locked(&dh->method_mont_p,
                                      dh->lock, dh->p, ctx);
        BN_set_flags(dh->priv_key, BN_FLG_CONSTTIME);
        if (!mont)
            goto err;
    }

    if (!DH_check_pub_key(dh, pub_key, &check_result) || check_result) {
        DHerr(DH_F_COMPUTE_KEY, DH_R_INVALID_PUBKEY);
        goto err;
    }

    if (!dh->
        meth->bn_mod_exp(dh, tmp, pub_key, dh->priv_key, dh->p, ctx, mont)) {
        DHerr(DH_F_COMPUTE_KEY, ERR_R_BN_LIB);
        goto err;
    }

    ret = BN_bn2binpad(tmp, key, BN_num_bytes(dh->p));
 err:
    BN_CTX_end(ctx);
    BN_CTX_free(ctx);
    return ret;
}

static int dh_bn_mod_exp(const DH *dh, BIGNUM *r,
                         const BIGNUM *a, const BIGNUM *p,
                         const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *m_ctx)
{
    return BN_mod_exp_mont(r, a, p, m, ctx, m_ctx);
}

static int dh_init(DH *dh)
{
    dh->flags |= DH_FLAG_CACHE_MONT_P;
    return 1;
}

static int dh_finish(DH *dh)
{
    BN_MONT_CTX_free(dh->method_mont_p);
    return 1;
}
