/* crypto/dh/dh_key.c */
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
#include <openssl/rand.h>
#include <openssl/dh.h>

static int generate_key(DH *dh);
static int compute_key(unsigned char *key, const BIGNUM *pub_key, DH *dh);
static int dh_bn_mod_exp(const DH *dh, BIGNUM *r,
                         const BIGNUM *a, const BIGNUM *p,
                         const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *m_ctx);
static int dh_init(DH *dh);
static int dh_finish(DH *dh);

int DH_generate_key(DH *dh)
{
#ifdef OPENSSL_FIPS
    if (FIPS_mode() && !(dh->meth->flags & DH_FLAG_FIPS_METHOD)
        && !(dh->flags & DH_FLAG_NON_FIPS_ALLOW)) {
        DHerr(DH_F_DH_GENERATE_KEY, DH_R_NON_FIPS_METHOD);
        return 0;
    }
#endif
    return dh->meth->generate_key(dh);
}

int DH_compute_key(unsigned char *key, const BIGNUM *pub_key, DH *dh)
{
#ifdef OPENSSL_FIPS
    if (FIPS_mode() && !(dh->meth->flags & DH_FLAG_FIPS_METHOD)
        && !(dh->flags & DH_FLAG_NON_FIPS_ALLOW)) {
        DHerr(DH_F_DH_COMPUTE_KEY, DH_R_NON_FIPS_METHOD);
        return 0;
    }
#endif
    return dh->meth->compute_key(key, pub_key, dh);
}

int DH_compute_key_padded(unsigned char *key, const BIGNUM *pub_key, DH *dh)
{
    int rv, pad;
    rv = dh->meth->compute_key(key, pub_key, dh);
    if (rv <= 0)
        return rv;
    pad = BN_num_bytes(dh->p) - rv;
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
    0,
    NULL,
    NULL
};

const DH_METHOD *DH_OpenSSL(void)
{
    return &dh_ossl;
}

static int generate_key(DH *dh)
{
    int ok = 0;
    int generate_new_key = 0;
    unsigned l;
    BN_CTX *ctx;
    BN_MONT_CTX *mont = NULL;
    BIGNUM *pub_key = NULL, *priv_key = NULL;

    ctx = BN_CTX_new();
    if (ctx == NULL)
        goto err;

    if (dh->priv_key == NULL) {
        priv_key = BN_new();
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
                                      CRYPTO_LOCK_DH, dh->p, ctx);
        if (!mont)
            goto err;
    }

    if (generate_new_key) {
        if (dh->q) {
            do {
                if (!BN_rand_range(priv_key, dh->q))
                    goto err;
            }
            while (BN_is_zero(priv_key) || BN_is_one(priv_key));
        } else {
            /* secret exponent length */
            l = dh->length ? dh->length : BN_num_bits(dh->p) - 1;
            if (!BN_rand(priv_key, l, 0, 0))
                goto err;
        }
    }

    {
        BIGNUM local_prk;
        BIGNUM *prk;

        if ((dh->flags & DH_FLAG_NO_EXP_CONSTTIME) == 0) {
            BN_init(&local_prk);
            prk = &local_prk;
            BN_with_flags(prk, priv_key, BN_FLG_CONSTTIME);
        } else
            prk = priv_key;

        if (!dh->meth->bn_mod_exp(dh, pub_key, dh->g, prk, dh->p, ctx, mont))
            goto err;
    }

    dh->pub_key = pub_key;
    dh->priv_key = priv_key;
    ok = 1;
 err:
    if (ok != 1)
        DHerr(DH_F_GENERATE_KEY, ERR_R_BN_LIB);

    if ((pub_key != NULL) && (dh->pub_key == NULL))
        BN_free(pub_key);
    if ((priv_key != NULL) && (dh->priv_key == NULL))
        BN_free(priv_key);
    BN_CTX_free(ctx);
    return (ok);
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

    if (dh->priv_key == NULL) {
        DHerr(DH_F_COMPUTE_KEY, DH_R_NO_PRIVATE_VALUE);
        goto err;
    }

    if (dh->flags & DH_FLAG_CACHE_MONT_P) {
        mont = BN_MONT_CTX_set_locked(&dh->method_mont_p,
                                      CRYPTO_LOCK_DH, dh->p, ctx);
        if ((dh->flags & DH_FLAG_NO_EXP_CONSTTIME) == 0) {
            /* XXX */
            BN_set_flags(dh->priv_key, BN_FLG_CONSTTIME);
        }
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

    ret = BN_bn2bin(tmp, key);
 err:
    if (ctx != NULL) {
        BN_CTX_end(ctx);
        BN_CTX_free(ctx);
    }
    return (ret);
}

static int dh_bn_mod_exp(const DH *dh, BIGNUM *r,
                         const BIGNUM *a, const BIGNUM *p,
                         const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *m_ctx)
{
    /*
     * If a is only one word long and constant time is false, use the faster
     * exponenentiation function.
     */
    if (a->top == 1 && ((dh->flags & DH_FLAG_NO_EXP_CONSTTIME) != 0)) {
        BN_ULONG A = a->d[0];
        return BN_mod_exp_mont_word(r, A, p, m, ctx, m_ctx);
    } else
        return BN_mod_exp_mont(r, a, p, m, ctx, m_ctx);
}

static int dh_init(DH *dh)
{
    dh->flags |= DH_FLAG_CACHE_MONT_P;
    return (1);
}

static int dh_finish(DH *dh)
{
    if (dh->method_mont_p)
        BN_MONT_CTX_free(dh->method_mont_p);
    return (1);
}
