/*
 * Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../e_os.h"

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/rand.h>
#include <openssl/err.h>

#define NUM_BITS        (BN_BITS2 * 4)

static const char rnd_seed[] =
    "string to make the random number generator think it has entropy";

/*
 * Test that r == 0 in test_exp_mod_zero(). Returns one on success,
 * returns zero and prints debug output otherwise.
 */
static int a_is_zero_mod_one(const char *method, const BIGNUM *r,
                             const BIGNUM *a) {
    if (!BN_is_zero(r)) {
        fprintf(stderr, "%s failed:\n", method);
        fprintf(stderr, "a ** 0 mod 1 = r (should be 0)\n");
        fprintf(stderr, "a = ");
        BN_print_fp(stderr, a);
        fprintf(stderr, "\nr = ");
        BN_print_fp(stderr, r);
        fprintf(stderr, "\n");
        return 0;
    }
    return 1;
}

/*
 * test_exp_mod_zero tests that x**0 mod 1 == 0. It returns zero on success.
 */
static int test_exp_mod_zero()
{
    BIGNUM *a = NULL, *p = NULL, *m = NULL;
    BIGNUM *r = NULL;
    BN_ULONG one_word = 1;
    BN_CTX *ctx = BN_CTX_new();
    int ret = 1, failed = 0;

    m = BN_new();
    if (!m)
        goto err;
    BN_one(m);

    a = BN_new();
    if (!a)
        goto err;
    BN_one(a);

    p = BN_new();
    if (!p)
        goto err;
    BN_zero(p);

    r = BN_new();
    if (!r)
        goto err;

    if (!BN_rand(a, 1024, BN_RAND_TOP_ONE, BN_RAND_BOTTOM_ANY))
        goto err;

    if (!BN_mod_exp(r, a, p, m, ctx))
        goto err;

    if (!a_is_zero_mod_one("BN_mod_exp", r, a))
        failed = 1;

    if (!BN_mod_exp_recp(r, a, p, m, ctx))
        goto err;

    if (!a_is_zero_mod_one("BN_mod_exp_recp", r, a))
        failed = 1;

    if (!BN_mod_exp_simple(r, a, p, m, ctx))
        goto err;

    if (!a_is_zero_mod_one("BN_mod_exp_simple", r, a))
        failed = 1;

    if (!BN_mod_exp_mont(r, a, p, m, ctx, NULL))
        goto err;

    if (!a_is_zero_mod_one("BN_mod_exp_mont", r, a))
        failed = 1;

    if (!BN_mod_exp_mont_consttime(r, a, p, m, ctx, NULL)) {
        goto err;
    }

    if (!a_is_zero_mod_one("BN_mod_exp_mont_consttime", r, a))
        failed = 1;

    /*
     * A different codepath exists for single word multiplication
     * in non-constant-time only.
     */
    if (!BN_mod_exp_mont_word(r, one_word, p, m, ctx, NULL))
        goto err;

    if (!BN_is_zero(r)) {
        fprintf(stderr, "BN_mod_exp_mont_word failed:\n");
        fprintf(stderr, "1 ** 0 mod 1 = r (should be 0)\n");
        fprintf(stderr, "r = ");
        BN_print_fp(stderr, r);
        fprintf(stderr, "\n");
        return 0;
    }

    ret = failed;

 err:
    BN_free(r);
    BN_free(a);
    BN_free(p);
    BN_free(m);
    BN_CTX_free(ctx);

    return ret;
}

int main(int argc, char *argv[])
{
    BN_CTX *ctx;
    BIO *out = NULL;
    int i, ret;
    unsigned char c;
    BIGNUM *r_mont, *r_mont_const, *r_recp, *r_simple, *a, *b, *m;

    /*
     * Seed or BN_rand may fail, and we don't even check its return
     * value (which we should)
     */
    RAND_seed(rnd_seed, sizeof(rnd_seed));

    ctx = BN_CTX_new();
    if (ctx == NULL)
        EXIT(1);
    r_mont = BN_new();
    r_mont_const = BN_new();
    r_recp = BN_new();
    r_simple = BN_new();
    a = BN_new();
    b = BN_new();
    m = BN_new();
    if ((r_mont == NULL) || (r_recp == NULL) || (a == NULL) || (b == NULL))
        goto err;

    out = BIO_new(BIO_s_file());

    if (out == NULL)
        EXIT(1);
    BIO_set_fp(out, stdout, BIO_NOCLOSE | BIO_FP_TEXT);

    for (i = 0; i < 200; i++) {
        RAND_bytes(&c, 1);
        c = (c % BN_BITS) - BN_BITS2;
        BN_rand(a, NUM_BITS + c, BN_RAND_TOP_ONE, BN_RAND_BOTTOM_ANY);

        RAND_bytes(&c, 1);
        c = (c % BN_BITS) - BN_BITS2;
        BN_rand(b, NUM_BITS + c, BN_RAND_TOP_ONE, BN_RAND_BOTTOM_ANY);

        RAND_bytes(&c, 1);
        c = (c % BN_BITS) - BN_BITS2;
        BN_rand(m, NUM_BITS + c, BN_RAND_TOP_ONE, BN_RAND_BOTTOM_ODD);

        BN_mod(a, a, m, ctx);
        BN_mod(b, b, m, ctx);

        ret = BN_mod_exp_mont(r_mont, a, b, m, ctx, NULL);
        if (ret <= 0) {
            printf("BN_mod_exp_mont() problems\n");
            ERR_print_errors(out);
            EXIT(1);
        }

        ret = BN_mod_exp_recp(r_recp, a, b, m, ctx);
        if (ret <= 0) {
            printf("BN_mod_exp_recp() problems\n");
            ERR_print_errors(out);
            EXIT(1);
        }

        ret = BN_mod_exp_simple(r_simple, a, b, m, ctx);
        if (ret <= 0) {
            printf("BN_mod_exp_simple() problems\n");
            ERR_print_errors(out);
            EXIT(1);
        }

        ret = BN_mod_exp_mont_consttime(r_mont_const, a, b, m, ctx, NULL);
        if (ret <= 0) {
            printf("BN_mod_exp_mont_consttime() problems\n");
            ERR_print_errors(out);
            EXIT(1);
        }

        if (BN_cmp(r_simple, r_mont) == 0
            && BN_cmp(r_simple, r_recp) == 0
            && BN_cmp(r_simple, r_mont_const) == 0) {
            printf(".");
            fflush(stdout);
        } else {
            if (BN_cmp(r_simple, r_mont) != 0)
                printf("\nsimple and mont results differ\n");
            if (BN_cmp(r_simple, r_mont_const) != 0)
                printf("\nsimple and mont const time results differ\n");
            if (BN_cmp(r_simple, r_recp) != 0)
                printf("\nsimple and recp results differ\n");

            printf("a (%3d) = ", BN_num_bits(a));
            BN_print(out, a);
            printf("\nb (%3d) = ", BN_num_bits(b));
            BN_print(out, b);
            printf("\nm (%3d) = ", BN_num_bits(m));
            BN_print(out, m);
            printf("\nsimple   =");
            BN_print(out, r_simple);
            printf("\nrecp     =");
            BN_print(out, r_recp);
            printf("\nmont     =");
            BN_print(out, r_mont);
            printf("\nmont_ct  =");
            BN_print(out, r_mont_const);
            printf("\n");
            EXIT(1);
        }
    }
    BN_free(r_mont);
    BN_free(r_mont_const);
    BN_free(r_recp);
    BN_free(r_simple);
    BN_free(a);
    BN_free(b);
    BN_free(m);
    BN_CTX_free(ctx);

    if (test_exp_mod_zero() != 0)
        goto err;

#ifndef OPENSSL_NO_CRYPTO_MDEBUG
    if (CRYPTO_mem_leaks(out) <= 0)
        goto err;
#endif
    BIO_free(out);
    printf("\n");

    printf("done\n");

    EXIT(0);
 err:
    ERR_print_errors(out);
    EXIT(1);
}
