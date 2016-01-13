/* crypto/dsa/dsa_gen.c */
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

#undef GENUINE_DSA

#ifdef GENUINE_DSA
/*
 * Parameter generation follows the original release of FIPS PUB 186,
 * Appendix 2.2 (i.e. use SHA as defined in FIPS PUB 180)
 */
# define HASH    EVP_sha()
#else
/*
 * Parameter generation follows the updated Appendix 2.2 for FIPS PUB 186,
 * also Appendix 2.2 of FIPS PUB 186-1 (i.e. use SHA as defined in FIPS PUB
 * 180-1)
 */
# define HASH    EVP_sha1()
#endif

#include <openssl/opensslconf.h> /* To see if OPENSSL_NO_SHA is defined */

#ifndef OPENSSL_NO_SHA

# include <stdio.h>
# include "cryptlib.h"
# include <openssl/evp.h>
# include <openssl/bn.h>
# include <openssl/rand.h>
# include <openssl/sha.h>
# include "dsa_locl.h"

# ifdef OPENSSL_FIPS
/* Workaround bug in prototype */
#  define fips_dsa_builtin_paramgen2 fips_dsa_paramgen_bad
#  include <openssl/fips.h>
# endif

int DSA_generate_parameters_ex(DSA *ret, int bits,
                               const unsigned char *seed_in, int seed_len,
                               int *counter_ret, unsigned long *h_ret,
                               BN_GENCB *cb)
{
# ifdef OPENSSL_FIPS
    if (FIPS_mode() && !(ret->meth->flags & DSA_FLAG_FIPS_METHOD)
        && !(ret->flags & DSA_FLAG_NON_FIPS_ALLOW)) {
        DSAerr(DSA_F_DSA_GENERATE_PARAMETERS_EX, DSA_R_NON_FIPS_DSA_METHOD);
        return 0;
    }
# endif
    if (ret->meth->dsa_paramgen)
        return ret->meth->dsa_paramgen(ret, bits, seed_in, seed_len,
                                       counter_ret, h_ret, cb);
# ifdef OPENSSL_FIPS
    else if (FIPS_mode()) {
        return FIPS_dsa_generate_parameters_ex(ret, bits,
                                               seed_in, seed_len,
                                               counter_ret, h_ret, cb);
    }
# endif
    else {
        const EVP_MD *evpmd = bits >= 2048 ? EVP_sha256() : EVP_sha1();
        size_t qbits = EVP_MD_size(evpmd) * 8;

        return dsa_builtin_paramgen(ret, bits, qbits, evpmd,
                                    seed_in, seed_len, NULL, counter_ret,
                                    h_ret, cb);
    }
}

int dsa_builtin_paramgen(DSA *ret, size_t bits, size_t qbits,
                         const EVP_MD *evpmd, const unsigned char *seed_in,
                         size_t seed_len, unsigned char *seed_out,
                         int *counter_ret, unsigned long *h_ret, BN_GENCB *cb)
{
    int ok = 0;
    unsigned char seed[SHA256_DIGEST_LENGTH];
    unsigned char md[SHA256_DIGEST_LENGTH];
    unsigned char buf[SHA256_DIGEST_LENGTH], buf2[SHA256_DIGEST_LENGTH];
    BIGNUM *r0, *W, *X, *c, *test;
    BIGNUM *g = NULL, *q = NULL, *p = NULL;
    BN_MONT_CTX *mont = NULL;
    int i, k, n = 0, m = 0, qsize = qbits >> 3;
    int counter = 0;
    int r = 0;
    BN_CTX *ctx = NULL;
    unsigned int h = 2;

    if (qsize != SHA_DIGEST_LENGTH && qsize != SHA224_DIGEST_LENGTH &&
        qsize != SHA256_DIGEST_LENGTH)
        /* invalid q size */
        return 0;

    if (evpmd == NULL)
        /* use SHA1 as default */
        evpmd = EVP_sha1();

    if (bits < 512)
        bits = 512;

    bits = (bits + 63) / 64 * 64;

    /*
     * NB: seed_len == 0 is special case: copy generated seed to seed_in if
     * it is not NULL.
     */
    if (seed_len && (seed_len < (size_t)qsize))
        seed_in = NULL;         /* seed buffer too small -- ignore */
    if (seed_len > (size_t)qsize)
        seed_len = qsize;       /* App. 2.2 of FIPS PUB 186 allows larger
                                 * SEED, but our internal buffers are
                                 * restricted to 160 bits */
    if (seed_in != NULL)
        memcpy(seed, seed_in, seed_len);

    if ((mont = BN_MONT_CTX_new()) == NULL)
        goto err;

    if ((ctx = BN_CTX_new()) == NULL)
        goto err;

    BN_CTX_start(ctx);

    r0 = BN_CTX_get(ctx);
    g = BN_CTX_get(ctx);
    W = BN_CTX_get(ctx);
    q = BN_CTX_get(ctx);
    X = BN_CTX_get(ctx);
    c = BN_CTX_get(ctx);
    p = BN_CTX_get(ctx);
    test = BN_CTX_get(ctx);

    if (!BN_lshift(test, BN_value_one(), bits - 1))
        goto err;

    for (;;) {
        for (;;) {              /* find q */
            int seed_is_random;

            /* step 1 */
            if (!BN_GENCB_call(cb, 0, m++))
                goto err;

            if (!seed_len || !seed_in) {
                if (RAND_pseudo_bytes(seed, qsize) < 0)
                    goto err;
                seed_is_random = 1;
            } else {
                seed_is_random = 0;
                seed_len = 0;   /* use random seed if 'seed_in' turns out to
                                 * be bad */
            }
            memcpy(buf, seed, qsize);
            memcpy(buf2, seed, qsize);
            /* precompute "SEED + 1" for step 7: */
            for (i = qsize - 1; i >= 0; i--) {
                buf[i]++;
                if (buf[i] != 0)
                    break;
            }

            /* step 2 */
            if (!EVP_Digest(seed, qsize, md, NULL, evpmd, NULL))
                goto err;
            if (!EVP_Digest(buf, qsize, buf2, NULL, evpmd, NULL))
                goto err;
            for (i = 0; i < qsize; i++)
                md[i] ^= buf2[i];

            /* step 3 */
            md[0] |= 0x80;
            md[qsize - 1] |= 0x01;
            if (!BN_bin2bn(md, qsize, q))
                goto err;

            /* step 4 */
            r = BN_is_prime_fasttest_ex(q, DSS_prime_checks, ctx,
                                        seed_is_random, cb);
            if (r > 0)
                break;
            if (r != 0)
                goto err;

            /* do a callback call */
            /* step 5 */
        }

        if (!BN_GENCB_call(cb, 2, 0))
            goto err;
        if (!BN_GENCB_call(cb, 3, 0))
            goto err;

        /* step 6 */
        counter = 0;
        /* "offset = 2" */

        n = (bits - 1) / 160;

        for (;;) {
            if ((counter != 0) && !BN_GENCB_call(cb, 0, counter))
                goto err;

            /* step 7 */
            BN_zero(W);
            /* now 'buf' contains "SEED + offset - 1" */
            for (k = 0; k <= n; k++) {
                /*
                 * obtain "SEED + offset + k" by incrementing:
                 */
                for (i = qsize - 1; i >= 0; i--) {
                    buf[i]++;
                    if (buf[i] != 0)
                        break;
                }

                if (!EVP_Digest(buf, qsize, md, NULL, evpmd, NULL))
                    goto err;

                /* step 8 */
                if (!BN_bin2bn(md, qsize, r0))
                    goto err;
                if (!BN_lshift(r0, r0, (qsize << 3) * k))
                    goto err;
                if (!BN_add(W, W, r0))
                    goto err;
            }

            /* more of step 8 */
            if (!BN_mask_bits(W, bits - 1))
                goto err;
            if (!BN_copy(X, W))
                goto err;
            if (!BN_add(X, X, test))
                goto err;

            /* step 9 */
            if (!BN_lshift1(r0, q))
                goto err;
            if (!BN_mod(c, X, r0, ctx))
                goto err;
            if (!BN_sub(r0, c, BN_value_one()))
                goto err;
            if (!BN_sub(p, X, r0))
                goto err;

            /* step 10 */
            if (BN_cmp(p, test) >= 0) {
                /* step 11 */
                r = BN_is_prime_fasttest_ex(p, DSS_prime_checks, ctx, 1, cb);
                if (r > 0)
                    goto end;   /* found it */
                if (r != 0)
                    goto err;
            }

            /* step 13 */
            counter++;
            /* "offset = offset + n + 1" */

            /* step 14 */
            if (counter >= 4096)
                break;
        }
    }
 end:
    if (!BN_GENCB_call(cb, 2, 1))
        goto err;

    /* We now need to generate g */
    /* Set r0=(p-1)/q */
    if (!BN_sub(test, p, BN_value_one()))
        goto err;
    if (!BN_div(r0, NULL, test, q, ctx))
        goto err;

    if (!BN_set_word(test, h))
        goto err;
    if (!BN_MONT_CTX_set(mont, p, ctx))
        goto err;

    for (;;) {
        /* g=test^r0%p */
        if (!BN_mod_exp_mont(g, test, r0, p, ctx, mont))
            goto err;
        if (!BN_is_one(g))
            break;
        if (!BN_add(test, test, BN_value_one()))
            goto err;
        h++;
    }

    if (!BN_GENCB_call(cb, 3, 1))
        goto err;

    ok = 1;
 err:
    if (ok) {
        if (ret->p)
            BN_free(ret->p);
        if (ret->q)
            BN_free(ret->q);
        if (ret->g)
            BN_free(ret->g);
        ret->p = BN_dup(p);
        ret->q = BN_dup(q);
        ret->g = BN_dup(g);
        if (ret->p == NULL || ret->q == NULL || ret->g == NULL) {
            ok = 0;
            goto err;
        }
        if (counter_ret != NULL)
            *counter_ret = counter;
        if (h_ret != NULL)
            *h_ret = h;
        if (seed_out)
            memcpy(seed_out, seed, qsize);
    }
    if (ctx) {
        BN_CTX_end(ctx);
        BN_CTX_free(ctx);
    }
    if (mont != NULL)
        BN_MONT_CTX_free(mont);
    return ok;
}

# ifdef OPENSSL_FIPS
#  undef fips_dsa_builtin_paramgen2
extern int fips_dsa_builtin_paramgen2(DSA *ret, size_t L, size_t N,
                                      const EVP_MD *evpmd,
                                      const unsigned char *seed_in,
                                      size_t seed_len, int idx,
                                      unsigned char *seed_out,
                                      int *counter_ret, unsigned long *h_ret,
                                      BN_GENCB *cb);
# endif

/*
 * This is a parameter generation algorithm for the DSA2 algorithm as
 * described in FIPS 186-3.
 */

int dsa_builtin_paramgen2(DSA *ret, size_t L, size_t N,
                          const EVP_MD *evpmd, const unsigned char *seed_in,
                          size_t seed_len, int idx, unsigned char *seed_out,
                          int *counter_ret, unsigned long *h_ret,
                          BN_GENCB *cb)
{
    int ok = -1;
    unsigned char *seed = NULL, *seed_tmp = NULL;
    unsigned char md[EVP_MAX_MD_SIZE];
    int mdsize;
    BIGNUM *r0, *W, *X, *c, *test;
    BIGNUM *g = NULL, *q = NULL, *p = NULL;
    BN_MONT_CTX *mont = NULL;
    int i, k, n = 0, m = 0, qsize = N >> 3;
    int counter = 0;
    int r = 0;
    BN_CTX *ctx = NULL;
    EVP_MD_CTX mctx;
    unsigned int h = 2;

# ifdef OPENSSL_FIPS

    if (FIPS_mode())
        return fips_dsa_builtin_paramgen2(ret, L, N, evpmd,
                                          seed_in, seed_len, idx,
                                          seed_out, counter_ret, h_ret, cb);
# endif

    EVP_MD_CTX_init(&mctx);

    if (evpmd == NULL) {
        if (N == 160)
            evpmd = EVP_sha1();
        else if (N == 224)
            evpmd = EVP_sha224();
        else
            evpmd = EVP_sha256();
    }

    mdsize = EVP_MD_size(evpmd);
    /* If unverificable g generation only don't need seed */
    if (!ret->p || !ret->q || idx >= 0) {
        if (seed_len == 0)
            seed_len = mdsize;

        seed = OPENSSL_malloc(seed_len);

        if (seed_out)
            seed_tmp = seed_out;
        else
            seed_tmp = OPENSSL_malloc(seed_len);

        if (!seed || !seed_tmp)
            goto err;

        if (seed_in)
            memcpy(seed, seed_in, seed_len);

    }

    if ((ctx = BN_CTX_new()) == NULL)
        goto err;

    if ((mont = BN_MONT_CTX_new()) == NULL)
        goto err;

    BN_CTX_start(ctx);
    r0 = BN_CTX_get(ctx);
    g = BN_CTX_get(ctx);
    W = BN_CTX_get(ctx);
    X = BN_CTX_get(ctx);
    c = BN_CTX_get(ctx);
    test = BN_CTX_get(ctx);

    /* if p, q already supplied generate g only */
    if (ret->p && ret->q) {
        p = ret->p;
        q = ret->q;
        if (idx >= 0)
            memcpy(seed_tmp, seed, seed_len);
        goto g_only;
    } else {
        p = BN_CTX_get(ctx);
        q = BN_CTX_get(ctx);
    }

    if (!BN_lshift(test, BN_value_one(), L - 1))
        goto err;
    for (;;) {
        for (;;) {              /* find q */
            unsigned char *pmd;
            /* step 1 */
            if (!BN_GENCB_call(cb, 0, m++))
                goto err;

            if (!seed_in) {
                if (RAND_pseudo_bytes(seed, seed_len) < 0)
                    goto err;
            }
            /* step 2 */
            if (!EVP_Digest(seed, seed_len, md, NULL, evpmd, NULL))
                goto err;
            /* Take least significant bits of md */
            if (mdsize > qsize)
                pmd = md + mdsize - qsize;
            else
                pmd = md;

            if (mdsize < qsize)
                memset(md + mdsize, 0, qsize - mdsize);

            /* step 3 */
            pmd[0] |= 0x80;
            pmd[qsize - 1] |= 0x01;
            if (!BN_bin2bn(pmd, qsize, q))
                goto err;

            /* step 4 */
            r = BN_is_prime_fasttest_ex(q, DSS_prime_checks, ctx,
                                        seed_in ? 1 : 0, cb);
            if (r > 0)
                break;
            if (r != 0)
                goto err;
            /* Provided seed didn't produce a prime: error */
            if (seed_in) {
                ok = 0;
                DSAerr(DSA_F_DSA_BUILTIN_PARAMGEN2, DSA_R_Q_NOT_PRIME);
                goto err;
            }

            /* do a callback call */
            /* step 5 */
        }
        /* Copy seed to seed_out before we mess with it */
        if (seed_out)
            memcpy(seed_out, seed, seed_len);

        if (!BN_GENCB_call(cb, 2, 0))
            goto err;
        if (!BN_GENCB_call(cb, 3, 0))
            goto err;

        /* step 6 */
        counter = 0;
        /* "offset = 1" */

        n = (L - 1) / (mdsize << 3);

        for (;;) {
            if ((counter != 0) && !BN_GENCB_call(cb, 0, counter))
                goto err;

            /* step 7 */
            BN_zero(W);
            /* now 'buf' contains "SEED + offset - 1" */
            for (k = 0; k <= n; k++) {
                /*
                 * obtain "SEED + offset + k" by incrementing:
                 */
                for (i = seed_len - 1; i >= 0; i--) {
                    seed[i]++;
                    if (seed[i] != 0)
                        break;
                }

                if (!EVP_Digest(seed, seed_len, md, NULL, evpmd, NULL))
                    goto err;

                /* step 8 */
                if (!BN_bin2bn(md, mdsize, r0))
                    goto err;
                if (!BN_lshift(r0, r0, (mdsize << 3) * k))
                    goto err;
                if (!BN_add(W, W, r0))
                    goto err;
            }

            /* more of step 8 */
            if (!BN_mask_bits(W, L - 1))
                goto err;
            if (!BN_copy(X, W))
                goto err;
            if (!BN_add(X, X, test))
                goto err;

            /* step 9 */
            if (!BN_lshift1(r0, q))
                goto err;
            if (!BN_mod(c, X, r0, ctx))
                goto err;
            if (!BN_sub(r0, c, BN_value_one()))
                goto err;
            if (!BN_sub(p, X, r0))
                goto err;

            /* step 10 */
            if (BN_cmp(p, test) >= 0) {
                /* step 11 */
                r = BN_is_prime_fasttest_ex(p, DSS_prime_checks, ctx, 1, cb);
                if (r > 0)
                    goto end;   /* found it */
                if (r != 0)
                    goto err;
            }

            /* step 13 */
            counter++;
            /* "offset = offset + n + 1" */

            /* step 14 */
            if (counter >= (int)(4 * L))
                break;
        }
        if (seed_in) {
            ok = 0;
            DSAerr(DSA_F_DSA_BUILTIN_PARAMGEN2, DSA_R_INVALID_PARAMETERS);
            goto err;
        }
    }
 end:
    if (!BN_GENCB_call(cb, 2, 1))
        goto err;

 g_only:

    /* We now need to generate g */
    /* Set r0=(p-1)/q */
    if (!BN_sub(test, p, BN_value_one()))
        goto err;
    if (!BN_div(r0, NULL, test, q, ctx))
        goto err;

    if (idx < 0) {
        if (!BN_set_word(test, h))
            goto err;
    } else
        h = 1;
    if (!BN_MONT_CTX_set(mont, p, ctx))
        goto err;

    for (;;) {
        static const unsigned char ggen[4] = { 0x67, 0x67, 0x65, 0x6e };
        if (idx >= 0) {
            md[0] = idx & 0xff;
            md[1] = (h >> 8) & 0xff;
            md[2] = h & 0xff;
            if (!EVP_DigestInit_ex(&mctx, evpmd, NULL))
                goto err;
            if (!EVP_DigestUpdate(&mctx, seed_tmp, seed_len))
                goto err;
            if (!EVP_DigestUpdate(&mctx, ggen, sizeof(ggen)))
                goto err;
            if (!EVP_DigestUpdate(&mctx, md, 3))
                goto err;
            if (!EVP_DigestFinal_ex(&mctx, md, NULL))
                goto err;
            if (!BN_bin2bn(md, mdsize, test))
                goto err;
        }
        /* g=test^r0%p */
        if (!BN_mod_exp_mont(g, test, r0, p, ctx, mont))
            goto err;
        if (!BN_is_one(g))
            break;
        if (idx < 0 && !BN_add(test, test, BN_value_one()))
            goto err;
        h++;
        if (idx >= 0 && h > 0xffff)
            goto err;
    }

    if (!BN_GENCB_call(cb, 3, 1))
        goto err;

    ok = 1;
 err:
    if (ok == 1) {
        if (p != ret->p) {
            if (ret->p)
                BN_free(ret->p);
            ret->p = BN_dup(p);
        }
        if (q != ret->q) {
            if (ret->q)
                BN_free(ret->q);
            ret->q = BN_dup(q);
        }
        if (ret->g)
            BN_free(ret->g);
        ret->g = BN_dup(g);
        if (ret->p == NULL || ret->q == NULL || ret->g == NULL) {
            ok = -1;
            goto err;
        }
        if (counter_ret != NULL)
            *counter_ret = counter;
        if (h_ret != NULL)
            *h_ret = h;
    }
    if (seed)
        OPENSSL_free(seed);
    if (seed_out != seed_tmp)
        OPENSSL_free(seed_tmp);
    if (ctx) {
        BN_CTX_end(ctx);
        BN_CTX_free(ctx);
    }
    if (mont != NULL)
        BN_MONT_CTX_free(mont);
    EVP_MD_CTX_cleanup(&mctx);
    return ok;
}

int dsa_paramgen_check_g(DSA *dsa)
{
    BN_CTX *ctx;
    BIGNUM *tmp;
    BN_MONT_CTX *mont = NULL;
    int rv = -1;
    ctx = BN_CTX_new();
    if (!ctx)
        return -1;
    BN_CTX_start(ctx);
    if (BN_cmp(dsa->g, BN_value_one()) <= 0)
        return 0;
    if (BN_cmp(dsa->g, dsa->p) >= 0)
        return 0;
    tmp = BN_CTX_get(ctx);
    if (!tmp)
        goto err;
    if ((mont = BN_MONT_CTX_new()) == NULL)
        goto err;
    if (!BN_MONT_CTX_set(mont, dsa->p, ctx))
        goto err;
    /* Work out g^q mod p */
    if (!BN_mod_exp_mont(tmp, dsa->g, dsa->q, dsa->p, ctx, mont))
        goto err;
    if (!BN_cmp(tmp, BN_value_one()))
        rv = 1;
    else
        rv = 0;
 err:
    BN_CTX_end(ctx);
    if (mont)
        BN_MONT_CTX_free(mont);
    BN_CTX_free(ctx);
    return rv;

}
#endif
