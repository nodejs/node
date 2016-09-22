/* crypto/dsa/dsa_ossl.c */
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

/* Original version from Steven Schoch <schoch@sheba.arc.nasa.gov> */

#include <stdio.h>
#include "cryptlib.h"
#include <openssl/bn.h>
#include <openssl/sha.h>
#include <openssl/dsa.h>
#include <openssl/rand.h>
#include <openssl/asn1.h>

static DSA_SIG *dsa_do_sign(const unsigned char *dgst, int dlen, DSA *dsa);
static int dsa_sign_setup(DSA *dsa, BN_CTX *ctx_in, BIGNUM **kinvp,
                          BIGNUM **rp);
static int dsa_do_verify(const unsigned char *dgst, int dgst_len,
                         DSA_SIG *sig, DSA *dsa);
static int dsa_init(DSA *dsa);
static int dsa_finish(DSA *dsa);

static DSA_METHOD openssl_dsa_meth = {
    "OpenSSL DSA method",
    dsa_do_sign,
    dsa_sign_setup,
    dsa_do_verify,
    NULL,                       /* dsa_mod_exp, */
    NULL,                       /* dsa_bn_mod_exp, */
    dsa_init,
    dsa_finish,
    0,
    NULL,
    NULL,
    NULL
};

/*-
 * These macro wrappers replace attempts to use the dsa_mod_exp() and
 * bn_mod_exp() handlers in the DSA_METHOD structure. We avoid the problem of
 * having a the macro work as an expression by bundling an "err_instr". So;
 *
 *     if (!dsa->meth->bn_mod_exp(dsa, r,dsa->g,&k,dsa->p,ctx,
 *                 dsa->method_mont_p)) goto err;
 *
 * can be replaced by;
 *
 *     DSA_BN_MOD_EXP(goto err, dsa, r, dsa->g, &k, dsa->p, ctx,
 *                 dsa->method_mont_p);
 */

#define DSA_MOD_EXP(err_instr,dsa,rr,a1,p1,a2,p2,m,ctx,in_mont) \
        do { \
        int _tmp_res53; \
        if ((dsa)->meth->dsa_mod_exp) \
                _tmp_res53 = (dsa)->meth->dsa_mod_exp((dsa), (rr), (a1), (p1), \
                                (a2), (p2), (m), (ctx), (in_mont)); \
        else \
                _tmp_res53 = BN_mod_exp2_mont((rr), (a1), (p1), (a2), (p2), \
                                (m), (ctx), (in_mont)); \
        if (!_tmp_res53) err_instr; \
        } while(0)
#define DSA_BN_MOD_EXP(err_instr,dsa,r,a,p,m,ctx,m_ctx) \
        do { \
        int _tmp_res53; \
        if ((dsa)->meth->bn_mod_exp) \
                _tmp_res53 = (dsa)->meth->bn_mod_exp((dsa), (r), (a), (p), \
                                (m), (ctx), (m_ctx)); \
        else \
                _tmp_res53 = BN_mod_exp_mont((r), (a), (p), (m), (ctx), (m_ctx)); \
        if (!_tmp_res53) err_instr; \
        } while(0)

const DSA_METHOD *DSA_OpenSSL(void)
{
    return &openssl_dsa_meth;
}

static DSA_SIG *dsa_do_sign(const unsigned char *dgst, int dlen, DSA *dsa)
{
    BIGNUM *kinv = NULL, *r = NULL, *s = NULL;
    BIGNUM m;
    BIGNUM xr;
    BN_CTX *ctx = NULL;
    int reason = ERR_R_BN_LIB;
    DSA_SIG *ret = NULL;
    int noredo = 0;

    BN_init(&m);
    BN_init(&xr);

    if (!dsa->p || !dsa->q || !dsa->g) {
        reason = DSA_R_MISSING_PARAMETERS;
        goto err;
    }

    s = BN_new();
    if (s == NULL)
        goto err;
    ctx = BN_CTX_new();
    if (ctx == NULL)
        goto err;
 redo:
    if ((dsa->kinv == NULL) || (dsa->r == NULL)) {
        if (!DSA_sign_setup(dsa, ctx, &kinv, &r))
            goto err;
    } else {
        kinv = dsa->kinv;
        dsa->kinv = NULL;
        r = dsa->r;
        dsa->r = NULL;
        noredo = 1;
    }

    if (dlen > BN_num_bytes(dsa->q))
        /*
         * if the digest length is greater than the size of q use the
         * BN_num_bits(dsa->q) leftmost bits of the digest, see fips 186-3,
         * 4.2
         */
        dlen = BN_num_bytes(dsa->q);
    if (BN_bin2bn(dgst, dlen, &m) == NULL)
        goto err;

    /* Compute  s = inv(k) (m + xr) mod q */
    if (!BN_mod_mul(&xr, dsa->priv_key, r, dsa->q, ctx))
        goto err;               /* s = xr */
    if (!BN_add(s, &xr, &m))
        goto err;               /* s = m + xr */
    if (BN_cmp(s, dsa->q) > 0)
        if (!BN_sub(s, s, dsa->q))
            goto err;
    if (!BN_mod_mul(s, s, kinv, dsa->q, ctx))
        goto err;

    /*
     * Redo if r or s is zero as required by FIPS 186-3: this is very
     * unlikely.
     */
    if (BN_is_zero(r) || BN_is_zero(s)) {
        if (noredo) {
            reason = DSA_R_NEED_NEW_SETUP_VALUES;
            goto err;
        }
        goto redo;
    }
    ret = DSA_SIG_new();
    if (ret == NULL)
        goto err;
    ret->r = r;
    ret->s = s;

 err:
    if (ret == NULL) {
        DSAerr(DSA_F_DSA_DO_SIGN, reason);
        BN_free(r);
        BN_free(s);
    }
    if (ctx != NULL)
        BN_CTX_free(ctx);
    BN_clear_free(&m);
    BN_clear_free(&xr);
    if (kinv != NULL)           /* dsa->kinv is NULL now if we used it */
        BN_clear_free(kinv);
    return (ret);
}

static int dsa_sign_setup(DSA *dsa, BN_CTX *ctx_in, BIGNUM **kinvp,
                          BIGNUM **rp)
{
    BN_CTX *ctx;
    BIGNUM k, kq, *K, *kinv = NULL, *r = NULL;
    int ret = 0;

    if (!dsa->p || !dsa->q || !dsa->g) {
        DSAerr(DSA_F_DSA_SIGN_SETUP, DSA_R_MISSING_PARAMETERS);
        return 0;
    }

    BN_init(&k);
    BN_init(&kq);

    if (ctx_in == NULL) {
        if ((ctx = BN_CTX_new()) == NULL)
            goto err;
    } else
        ctx = ctx_in;

    if ((r = BN_new()) == NULL)
        goto err;

    /* Get random k */
    do
        if (!BN_rand_range(&k, dsa->q))
            goto err;
    while (BN_is_zero(&k));

    if ((dsa->flags & DSA_FLAG_NO_EXP_CONSTTIME) == 0) {
        BN_set_flags(&k, BN_FLG_CONSTTIME);
    }


    if (dsa->flags & DSA_FLAG_CACHE_MONT_P) {
        if (!BN_MONT_CTX_set_locked(&dsa->method_mont_p,
                                    CRYPTO_LOCK_DSA, dsa->p, ctx))
            goto err;
    }

    /* Compute r = (g^k mod p) mod q */

    if ((dsa->flags & DSA_FLAG_NO_EXP_CONSTTIME) == 0) {
        if (!BN_copy(&kq, &k))
            goto err;

        BN_set_flags(&kq, BN_FLG_CONSTTIME);

        /*
         * We do not want timing information to leak the length of k, so we
         * compute g^k using an equivalent exponent of fixed length. (This
         * is a kludge that we need because the BN_mod_exp_mont() does not
         * let us specify the desired timing behaviour.)
         */

        if (!BN_add(&kq, &kq, dsa->q))
            goto err;
        if (BN_num_bits(&kq) <= BN_num_bits(dsa->q)) {
            if (!BN_add(&kq, &kq, dsa->q))
                goto err;
        }

        K = &kq;
    } else {
        K = &k;
    }

    DSA_BN_MOD_EXP(goto err, dsa, r, dsa->g, K, dsa->p, ctx,
                   dsa->method_mont_p);
    if (!BN_mod(r, r, dsa->q, ctx))
        goto err;

    /* Compute  part of 's = inv(k) (m + xr) mod q' */
    if ((kinv = BN_mod_inverse(NULL, &k, dsa->q, ctx)) == NULL)
        goto err;

    if (*kinvp != NULL)
        BN_clear_free(*kinvp);
    *kinvp = kinv;
    kinv = NULL;
    if (*rp != NULL)
        BN_clear_free(*rp);
    *rp = r;
    ret = 1;
 err:
    if (!ret) {
        DSAerr(DSA_F_DSA_SIGN_SETUP, ERR_R_BN_LIB);
        if (r != NULL)
            BN_clear_free(r);
    }
    if (ctx_in == NULL)
        BN_CTX_free(ctx);
    BN_clear_free(&k);
    BN_clear_free(&kq);
    return (ret);
}

static int dsa_do_verify(const unsigned char *dgst, int dgst_len,
                         DSA_SIG *sig, DSA *dsa)
{
    BN_CTX *ctx;
    BIGNUM u1, u2, t1;
    BN_MONT_CTX *mont = NULL;
    int ret = -1, i;
    if (!dsa->p || !dsa->q || !dsa->g) {
        DSAerr(DSA_F_DSA_DO_VERIFY, DSA_R_MISSING_PARAMETERS);
        return -1;
    }

    i = BN_num_bits(dsa->q);
    /* fips 186-3 allows only different sizes for q */
    if (i != 160 && i != 224 && i != 256) {
        DSAerr(DSA_F_DSA_DO_VERIFY, DSA_R_BAD_Q_VALUE);
        return -1;
    }

    if (BN_num_bits(dsa->p) > OPENSSL_DSA_MAX_MODULUS_BITS) {
        DSAerr(DSA_F_DSA_DO_VERIFY, DSA_R_MODULUS_TOO_LARGE);
        return -1;
    }
    BN_init(&u1);
    BN_init(&u2);
    BN_init(&t1);

    if ((ctx = BN_CTX_new()) == NULL)
        goto err;

    if (BN_is_zero(sig->r) || BN_is_negative(sig->r) ||
        BN_ucmp(sig->r, dsa->q) >= 0) {
        ret = 0;
        goto err;
    }
    if (BN_is_zero(sig->s) || BN_is_negative(sig->s) ||
        BN_ucmp(sig->s, dsa->q) >= 0) {
        ret = 0;
        goto err;
    }

    /*
     * Calculate W = inv(S) mod Q save W in u2
     */
    if ((BN_mod_inverse(&u2, sig->s, dsa->q, ctx)) == NULL)
        goto err;

    /* save M in u1 */
    if (dgst_len > (i >> 3))
        /*
         * if the digest length is greater than the size of q use the
         * BN_num_bits(dsa->q) leftmost bits of the digest, see fips 186-3,
         * 4.2
         */
        dgst_len = (i >> 3);
    if (BN_bin2bn(dgst, dgst_len, &u1) == NULL)
        goto err;

    /* u1 = M * w mod q */
    if (!BN_mod_mul(&u1, &u1, &u2, dsa->q, ctx))
        goto err;

    /* u2 = r * w mod q */
    if (!BN_mod_mul(&u2, sig->r, &u2, dsa->q, ctx))
        goto err;

    if (dsa->flags & DSA_FLAG_CACHE_MONT_P) {
        mont = BN_MONT_CTX_set_locked(&dsa->method_mont_p,
                                      CRYPTO_LOCK_DSA, dsa->p, ctx);
        if (!mont)
            goto err;
    }

    DSA_MOD_EXP(goto err, dsa, &t1, dsa->g, &u1, dsa->pub_key, &u2, dsa->p,
                ctx, mont);
    /* BN_copy(&u1,&t1); */
    /* let u1 = u1 mod q */
    if (!BN_mod(&u1, &t1, dsa->q, ctx))
        goto err;

    /*
     * V is now in u1.  If the signature is correct, it will be equal to R.
     */
    ret = (BN_ucmp(&u1, sig->r) == 0);

 err:
    /*
     * XXX: surely this is wrong - if ret is 0, it just didn't verify; there
     * is no error in BN. Test should be ret == -1 (Ben)
     */
    if (ret != 1)
        DSAerr(DSA_F_DSA_DO_VERIFY, ERR_R_BN_LIB);
    if (ctx != NULL)
        BN_CTX_free(ctx);
    BN_free(&u1);
    BN_free(&u2);
    BN_free(&t1);
    return (ret);
}

static int dsa_init(DSA *dsa)
{
    dsa->flags |= DSA_FLAG_CACHE_MONT_P;
    return (1);
}

static int dsa_finish(DSA *dsa)
{
    if (dsa->method_mont_p)
        BN_MONT_CTX_free(dsa->method_mont_p);
    return (1);
}
