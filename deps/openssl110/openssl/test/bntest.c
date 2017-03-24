/*
 * Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 *
 * Portions of the attached software ("Contribution") are developed by
 * SUN MICROSYSTEMS, INC., and are contributed to the OpenSSL project.
 *
 * The Contribution is licensed pursuant to the Eric Young open source
 * license provided above.
 *
 * The binary polynomial arithmetic software is originally written by
 * Sheueling Chang Shantz and Douglas Stebila of Sun Microsystems Laboratories.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "e_os.h"

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/rand.h>
#include <openssl/x509.h>
#include <openssl/err.h>

/*
 * In bn_lcl.h, bn_expand() is defined as a static ossl_inline function.
 * This is fine in itself, it will end up as an unused static function in
 * the worst case.  However, it referenses bn_expand2(), which is a private
 * function in libcrypto and therefore unavailable on some systems.  This
 * may result in a linker error because of unresolved symbols.
 *
 * To avoid this, we define a dummy variant of bn_expand2() here, and to
 * avoid possible clashes with libcrypto, we rename it first, using a macro.
 */
#define bn_expand2 dummy_bn_expand2
BIGNUM *bn_expand2(BIGNUM *b, int words);
BIGNUM *bn_expand2(BIGNUM *b, int words) { return NULL; }

#include "../crypto/bn/bn_lcl.h"

static const int num0 = 100;           /* number of tests */
static const int num1 = 50;            /* additional tests for some functions */
static const int num2 = 5;             /* number of tests for slow functions */

int test_add(BIO *bp);
int test_sub(BIO *bp);
int test_lshift1(BIO *bp);
int test_lshift(BIO *bp, BN_CTX *ctx, BIGNUM *a_);
int test_rshift1(BIO *bp);
int test_rshift(BIO *bp, BN_CTX *ctx);
int test_div(BIO *bp, BN_CTX *ctx);
int test_div_word(BIO *bp);
int test_div_recp(BIO *bp, BN_CTX *ctx);
int test_mul(BIO *bp);
int test_sqr(BIO *bp, BN_CTX *ctx);
int test_mont(BIO *bp, BN_CTX *ctx);
int test_mod(BIO *bp, BN_CTX *ctx);
int test_mod_mul(BIO *bp, BN_CTX *ctx);
int test_mod_exp(BIO *bp, BN_CTX *ctx);
int test_mod_exp_mont_consttime(BIO *bp, BN_CTX *ctx);
int test_mod_exp_mont5(BIO *bp, BN_CTX *ctx);
int test_exp(BIO *bp, BN_CTX *ctx);
int test_gf2m_add(BIO *bp);
int test_gf2m_mod(BIO *bp);
int test_gf2m_mod_mul(BIO *bp, BN_CTX *ctx);
int test_gf2m_mod_sqr(BIO *bp, BN_CTX *ctx);
int test_gf2m_mod_inv(BIO *bp, BN_CTX *ctx);
int test_gf2m_mod_div(BIO *bp, BN_CTX *ctx);
int test_gf2m_mod_exp(BIO *bp, BN_CTX *ctx);
int test_gf2m_mod_sqrt(BIO *bp, BN_CTX *ctx);
int test_gf2m_mod_solve_quad(BIO *bp, BN_CTX *ctx);
int test_kron(BIO *bp, BN_CTX *ctx);
int test_sqrt(BIO *bp, BN_CTX *ctx);
int test_small_prime(BIO *bp, BN_CTX *ctx);
int test_bn2dec(BIO *bp);
int rand_neg(void);
static int results = 0;

static unsigned char lst[] =
    "\xC6\x4F\x43\x04\x2A\xEA\xCA\x6E\x58\x36\x80\x5B\xE8\xC9"
    "\x9B\x04\x5D\x48\x36\xC2\xFD\x16\xC9\x64\xF0";

static const char rnd_seed[] =
    "string to make the random number generator think it has entropy";

static void message(BIO *out, char *m)
{
    fprintf(stderr, "test %s\n", m);
    BIO_puts(out, "print \"test ");
    BIO_puts(out, m);
    BIO_puts(out, "\\n\"\n");
}

int main(int argc, char *argv[])
{
    BN_CTX *ctx;
    BIO *out;
    char *outfile = NULL;

    CRYPTO_set_mem_debug(1);
    CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);

    results = 0;

    RAND_seed(rnd_seed, sizeof rnd_seed); /* or BN_generate_prime may fail */

    argc--;
    argv++;
    while (argc >= 1) {
        if (strcmp(*argv, "-results") == 0)
            results = 1;
        else if (strcmp(*argv, "-out") == 0) {
            if (--argc < 1)
                break;
            outfile = *(++argv);
        }
        argc--;
        argv++;
    }

    ctx = BN_CTX_new();
    if (ctx == NULL)
        EXIT(1);

    out = BIO_new(BIO_s_file());
    if (out == NULL)
        EXIT(1);
    if (outfile == NULL) {
        BIO_set_fp(out, stdout, BIO_NOCLOSE | BIO_FP_TEXT);
    } else {
        if (!BIO_write_filename(out, outfile)) {
            perror(outfile);
            EXIT(1);
        }
    }
#ifdef OPENSSL_SYS_VMS
    {
        BIO *tmpbio = BIO_new(BIO_f_linebuffer());
        out = BIO_push(tmpbio, out);
    }
#endif

    if (!results)
        BIO_puts(out, "obase=16\nibase=16\n");

    message(out, "BN_add");
    if (!test_add(out))
        goto err;
    (void)BIO_flush(out);

    message(out, "BN_sub");
    if (!test_sub(out))
        goto err;
    (void)BIO_flush(out);

    message(out, "BN_lshift1");
    if (!test_lshift1(out))
        goto err;
    (void)BIO_flush(out);

    message(out, "BN_lshift (fixed)");
    if (!test_lshift(out, ctx, BN_bin2bn(lst, sizeof(lst) - 1, NULL)))
        goto err;
    (void)BIO_flush(out);

    message(out, "BN_lshift");
    if (!test_lshift(out, ctx, NULL))
        goto err;
    (void)BIO_flush(out);

    message(out, "BN_rshift1");
    if (!test_rshift1(out))
        goto err;
    (void)BIO_flush(out);

    message(out, "BN_rshift");
    if (!test_rshift(out, ctx))
        goto err;
    (void)BIO_flush(out);

    message(out, "BN_sqr");
    if (!test_sqr(out, ctx))
        goto err;
    (void)BIO_flush(out);

    message(out, "BN_mul");
    if (!test_mul(out))
        goto err;
    (void)BIO_flush(out);

    message(out, "BN_div");
    if (!test_div(out, ctx))
        goto err;
    (void)BIO_flush(out);

    message(out, "BN_div_word");
    if (!test_div_word(out))
        goto err;
    (void)BIO_flush(out);

    message(out, "BN_div_recp");
    if (!test_div_recp(out, ctx))
        goto err;
    (void)BIO_flush(out);

    message(out, "BN_mod");
    if (!test_mod(out, ctx))
        goto err;
    (void)BIO_flush(out);

    message(out, "BN_mod_mul");
    if (!test_mod_mul(out, ctx))
        goto err;
    (void)BIO_flush(out);

    message(out, "BN_mont");
    if (!test_mont(out, ctx))
        goto err;
    (void)BIO_flush(out);

    message(out, "BN_mod_exp");
    if (!test_mod_exp(out, ctx))
        goto err;
    (void)BIO_flush(out);

    message(out, "BN_mod_exp_mont_consttime");
    if (!test_mod_exp_mont_consttime(out, ctx))
        goto err;
    if (!test_mod_exp_mont5(out, ctx))
        goto err;
    (void)BIO_flush(out);

    message(out, "BN_exp");
    if (!test_exp(out, ctx))
        goto err;
    (void)BIO_flush(out);

    message(out, "BN_kronecker");
    if (!test_kron(out, ctx))
        goto err;
    (void)BIO_flush(out);

    message(out, "BN_mod_sqrt");
    if (!test_sqrt(out, ctx))
        goto err;
    (void)BIO_flush(out);

    message(out, "Small prime generation");
    if (!test_small_prime(out, ctx))
        goto err;
    (void)BIO_flush(out);

    message(out, "BN_bn2dec");
    if (!test_bn2dec(out))
        goto err;
    (void)BIO_flush(out);

#ifndef OPENSSL_NO_EC2M
    message(out, "BN_GF2m_add");
    if (!test_gf2m_add(out))
        goto err;
    (void)BIO_flush(out);

    message(out, "BN_GF2m_mod");
    if (!test_gf2m_mod(out))
        goto err;
    (void)BIO_flush(out);

    message(out, "BN_GF2m_mod_mul");
    if (!test_gf2m_mod_mul(out, ctx))
        goto err;
    (void)BIO_flush(out);

    message(out, "BN_GF2m_mod_sqr");
    if (!test_gf2m_mod_sqr(out, ctx))
        goto err;
    (void)BIO_flush(out);

    message(out, "BN_GF2m_mod_inv");
    if (!test_gf2m_mod_inv(out, ctx))
        goto err;
    (void)BIO_flush(out);

    message(out, "BN_GF2m_mod_div");
    if (!test_gf2m_mod_div(out, ctx))
        goto err;
    (void)BIO_flush(out);

    message(out, "BN_GF2m_mod_exp");
    if (!test_gf2m_mod_exp(out, ctx))
        goto err;
    (void)BIO_flush(out);

    message(out, "BN_GF2m_mod_sqrt");
    if (!test_gf2m_mod_sqrt(out, ctx))
        goto err;
    (void)BIO_flush(out);

    message(out, "BN_GF2m_mod_solve_quad");
    if (!test_gf2m_mod_solve_quad(out, ctx))
        goto err;
    (void)BIO_flush(out);
#endif
    BN_CTX_free(ctx);
    BIO_free(out);

    ERR_print_errors_fp(stderr);

#ifndef OPENSSL_NO_CRYPTO_MDEBUG
    if (CRYPTO_mem_leaks_fp(stderr) <= 0)
        EXIT(1);
#endif
    EXIT(0);
 err:
    BIO_puts(out, "1\n");       /* make sure the Perl script fed by bc
                                 * notices the failure, see test_bn in
                                 * test/Makefile.ssl */
    (void)BIO_flush(out);
    BN_CTX_free(ctx);
    BIO_free(out);

    ERR_print_errors_fp(stderr);
    EXIT(1);
}

int test_add(BIO *bp)
{
    BIGNUM *a, *b, *c;
    int i;

    a = BN_new();
    b = BN_new();
    c = BN_new();

    BN_bntest_rand(a, 512, 0, 0);
    for (i = 0; i < num0; i++) {
        BN_bntest_rand(b, 450 + i, 0, 0);
        a->neg = rand_neg();
        b->neg = rand_neg();
        BN_add(c, a, b);
        if (bp != NULL) {
            if (!results) {
                BN_print(bp, a);
                BIO_puts(bp, " + ");
                BN_print(bp, b);
                BIO_puts(bp, " - ");
            }
            BN_print(bp, c);
            BIO_puts(bp, "\n");
        }
        a->neg = !a->neg;
        b->neg = !b->neg;
        BN_add(c, c, b);
        BN_add(c, c, a);
        if (!BN_is_zero(c)) {
            fprintf(stderr, "Add test failed!\n");
            return 0;
        }
    }
    BN_free(a);
    BN_free(b);
    BN_free(c);
    return (1);
}

int test_sub(BIO *bp)
{
    BIGNUM *a, *b, *c;
    int i;

    a = BN_new();
    b = BN_new();
    c = BN_new();

    for (i = 0; i < num0 + num1; i++) {
        if (i < num1) {
            BN_bntest_rand(a, 512, 0, 0);
            BN_copy(b, a);
            if (BN_set_bit(a, i) == 0)
                return (0);
            BN_add_word(b, i);
        } else {
            BN_bntest_rand(b, 400 + i - num1, 0, 0);
            a->neg = rand_neg();
            b->neg = rand_neg();
        }
        BN_sub(c, a, b);
        if (bp != NULL) {
            if (!results) {
                BN_print(bp, a);
                BIO_puts(bp, " - ");
                BN_print(bp, b);
                BIO_puts(bp, " - ");
            }
            BN_print(bp, c);
            BIO_puts(bp, "\n");
        }
        BN_add(c, c, b);
        BN_sub(c, c, a);
        if (!BN_is_zero(c)) {
            fprintf(stderr, "Subtract test failed!\n");
            return 0;
        }
    }
    BN_free(a);
    BN_free(b);
    BN_free(c);
    return (1);
}

int test_div(BIO *bp, BN_CTX *ctx)
{
    BIGNUM *a, *b, *c, *d, *e;
    int i;

    a = BN_new();
    b = BN_new();
    c = BN_new();
    d = BN_new();
    e = BN_new();

    BN_one(a);
    BN_zero(b);

    if (BN_div(d, c, a, b, ctx)) {
        fprintf(stderr, "Division by zero succeeded!\n");
        return 0;
    }

    for (i = 0; i < num0 + num1; i++) {
        if (i < num1) {
            BN_bntest_rand(a, 400, 0, 0);
            BN_copy(b, a);
            BN_lshift(a, a, i);
            BN_add_word(a, i);
        } else
            BN_bntest_rand(b, 50 + 3 * (i - num1), 0, 0);
        a->neg = rand_neg();
        b->neg = rand_neg();
        BN_div(d, c, a, b, ctx);
        if (bp != NULL) {
            if (!results) {
                BN_print(bp, a);
                BIO_puts(bp, " / ");
                BN_print(bp, b);
                BIO_puts(bp, " - ");
            }
            BN_print(bp, d);
            BIO_puts(bp, "\n");

            if (!results) {
                BN_print(bp, a);
                BIO_puts(bp, " % ");
                BN_print(bp, b);
                BIO_puts(bp, " - ");
            }
            BN_print(bp, c);
            BIO_puts(bp, "\n");
        }
        BN_mul(e, d, b, ctx);
        BN_add(d, e, c);
        BN_sub(d, d, a);
        if (!BN_is_zero(d)) {
            fprintf(stderr, "Division test failed!\n");
            return 0;
        }
    }
    BN_free(a);
    BN_free(b);
    BN_free(c);
    BN_free(d);
    BN_free(e);
    return (1);
}

static void print_word(BIO *bp, BN_ULONG w)
{
    int i = sizeof(w) * 8;
    char *fmt = NULL;
    unsigned char byte;

    do {
        i -= 8;
        byte = (unsigned char)(w >> i);
        if (fmt == NULL)
            fmt = byte ? "%X" : NULL;
        else
            fmt = "%02X";

        if (fmt != NULL)
            BIO_printf(bp, fmt, byte);
    } while (i);

    /* If we haven't printed anything, at least print a zero! */
    if (fmt == NULL)
        BIO_printf(bp, "0");
}

int test_div_word(BIO *bp)
{
    BIGNUM *a, *b;
    BN_ULONG r, rmod, s;
    int i;

    a = BN_new();
    b = BN_new();

    for (i = 0; i < num0; i++) {
        do {
            BN_bntest_rand(a, 512, -1, 0);
            BN_bntest_rand(b, BN_BITS2, -1, 0);
        } while (BN_is_zero(b));

        s = b->d[0];
        BN_copy(b, a);
        rmod = BN_mod_word(b, s);
        r = BN_div_word(b, s);

        if (rmod != r) {
            fprintf(stderr, "Mod (word) test failed!\n");
            return 0;
        }

        if (bp != NULL) {
            if (!results) {
                BN_print(bp, a);
                BIO_puts(bp, " / ");
                print_word(bp, s);
                BIO_puts(bp, " - ");
            }
            BN_print(bp, b);
            BIO_puts(bp, "\n");

            if (!results) {
                BN_print(bp, a);
                BIO_puts(bp, " % ");
                print_word(bp, s);
                BIO_puts(bp, " - ");
            }
            print_word(bp, r);
            BIO_puts(bp, "\n");
        }
        BN_mul_word(b, s);
        BN_add_word(b, r);
        BN_sub(b, a, b);
        if (!BN_is_zero(b)) {
            fprintf(stderr, "Division (word) test failed!\n");
            return 0;
        }
    }
    BN_free(a);
    BN_free(b);
    return (1);
}

int test_div_recp(BIO *bp, BN_CTX *ctx)
{
    BIGNUM *a, *b, *c, *d, *e;
    BN_RECP_CTX *recp;
    int i;

    recp = BN_RECP_CTX_new();
    a = BN_new();
    b = BN_new();
    c = BN_new();
    d = BN_new();
    e = BN_new();

    for (i = 0; i < num0 + num1; i++) {
        if (i < num1) {
            BN_bntest_rand(a, 400, 0, 0);
            BN_copy(b, a);
            BN_lshift(a, a, i);
            BN_add_word(a, i);
        } else
            BN_bntest_rand(b, 50 + 3 * (i - num1), 0, 0);
        a->neg = rand_neg();
        b->neg = rand_neg();
        BN_RECP_CTX_set(recp, b, ctx);
        BN_div_recp(d, c, a, recp, ctx);
        if (bp != NULL) {
            if (!results) {
                BN_print(bp, a);
                BIO_puts(bp, " / ");
                BN_print(bp, b);
                BIO_puts(bp, " - ");
            }
            BN_print(bp, d);
            BIO_puts(bp, "\n");

            if (!results) {
                BN_print(bp, a);
                BIO_puts(bp, " % ");
                BN_print(bp, b);
                BIO_puts(bp, " - ");
            }
            BN_print(bp, c);
            BIO_puts(bp, "\n");
        }
        BN_mul(e, d, b, ctx);
        BN_add(d, e, c);
        BN_sub(d, d, a);
        if (!BN_is_zero(d)) {
            fprintf(stderr, "Reciprocal division test failed!\n");
            fprintf(stderr, "a=");
            BN_print_fp(stderr, a);
            fprintf(stderr, "\nb=");
            BN_print_fp(stderr, b);
            fprintf(stderr, "\n");
            return 0;
        }
    }
    BN_free(a);
    BN_free(b);
    BN_free(c);
    BN_free(d);
    BN_free(e);
    BN_RECP_CTX_free(recp);
    return (1);
}

int test_mul(BIO *bp)
{
    BIGNUM *a, *b, *c, *d, *e;
    int i;
    BN_CTX *ctx;

    ctx = BN_CTX_new();
    if (ctx == NULL)
        EXIT(1);

    a = BN_new();
    b = BN_new();
    c = BN_new();
    d = BN_new();
    e = BN_new();

    for (i = 0; i < num0 + num1; i++) {
        if (i <= num1) {
            BN_bntest_rand(a, 100, 0, 0);
            BN_bntest_rand(b, 100, 0, 0);
        } else
            BN_bntest_rand(b, i - num1, 0, 0);
        a->neg = rand_neg();
        b->neg = rand_neg();
        BN_mul(c, a, b, ctx);
        if (bp != NULL) {
            if (!results) {
                BN_print(bp, a);
                BIO_puts(bp, " * ");
                BN_print(bp, b);
                BIO_puts(bp, " - ");
            }
            BN_print(bp, c);
            BIO_puts(bp, "\n");
        }
        BN_div(d, e, c, a, ctx);
        BN_sub(d, d, b);
        if (!BN_is_zero(d) || !BN_is_zero(e)) {
            fprintf(stderr, "Multiplication test failed!\n");
            return 0;
        }
    }
    BN_free(a);
    BN_free(b);
    BN_free(c);
    BN_free(d);
    BN_free(e);
    BN_CTX_free(ctx);
    return (1);
}

int test_sqr(BIO *bp, BN_CTX *ctx)
{
    BIGNUM *a, *c, *d, *e;
    int i, ret = 0;

    a = BN_new();
    c = BN_new();
    d = BN_new();
    e = BN_new();
    if (a == NULL || c == NULL || d == NULL || e == NULL) {
        goto err;
    }

    for (i = 0; i < num0; i++) {
        BN_bntest_rand(a, 40 + i * 10, 0, 0);
        a->neg = rand_neg();
        BN_sqr(c, a, ctx);
        if (bp != NULL) {
            if (!results) {
                BN_print(bp, a);
                BIO_puts(bp, " * ");
                BN_print(bp, a);
                BIO_puts(bp, " - ");
            }
            BN_print(bp, c);
            BIO_puts(bp, "\n");
        }
        BN_div(d, e, c, a, ctx);
        BN_sub(d, d, a);
        if (!BN_is_zero(d) || !BN_is_zero(e)) {
            fprintf(stderr, "Square test failed!\n");
            goto err;
        }
    }

    /* Regression test for a BN_sqr overflow bug. */
    BN_hex2bn(&a,
              "80000000000000008000000000000001"
              "FFFFFFFFFFFFFFFE0000000000000000");
    BN_sqr(c, a, ctx);
    if (bp != NULL) {
        if (!results) {
            BN_print(bp, a);
            BIO_puts(bp, " * ");
            BN_print(bp, a);
            BIO_puts(bp, " - ");
        }
        BN_print(bp, c);
        BIO_puts(bp, "\n");
    }
    BN_mul(d, a, a, ctx);
    if (BN_cmp(c, d)) {
        fprintf(stderr, "Square test failed: BN_sqr and BN_mul produce "
                "different results!\n");
        goto err;
    }

    /* Regression test for a BN_sqr overflow bug. */
    BN_hex2bn(&a,
              "80000000000000000000000080000001"
              "FFFFFFFE000000000000000000000000");
    BN_sqr(c, a, ctx);
    if (bp != NULL) {
        if (!results) {
            BN_print(bp, a);
            BIO_puts(bp, " * ");
            BN_print(bp, a);
            BIO_puts(bp, " - ");
        }
        BN_print(bp, c);
        BIO_puts(bp, "\n");
    }
    BN_mul(d, a, a, ctx);
    if (BN_cmp(c, d)) {
        fprintf(stderr, "Square test failed: BN_sqr and BN_mul produce "
                "different results!\n");
        goto err;
    }
    ret = 1;
 err:
    BN_free(a);
    BN_free(c);
    BN_free(d);
    BN_free(e);
    return ret;
}

int test_mont(BIO *bp, BN_CTX *ctx)
{
    BIGNUM *a, *b, *c, *d, *A, *B;
    BIGNUM *n;
    int i;
    BN_MONT_CTX *mont;

    a = BN_new();
    b = BN_new();
    c = BN_new();
    d = BN_new();
    A = BN_new();
    B = BN_new();
    n = BN_new();

    mont = BN_MONT_CTX_new();
    if (mont == NULL)
        return 0;

    BN_zero(n);
    if (BN_MONT_CTX_set(mont, n, ctx)) {
        fprintf(stderr, "BN_MONT_CTX_set succeeded for zero modulus!\n");
        return 0;
    }

    BN_set_word(n, 16);
    if (BN_MONT_CTX_set(mont, n, ctx)) {
        fprintf(stderr, "BN_MONT_CTX_set succeeded for even modulus!\n");
        return 0;
    }

    BN_bntest_rand(a, 100, 0, 0);
    BN_bntest_rand(b, 100, 0, 0);
    for (i = 0; i < num2; i++) {
        int bits = (200 * (i + 1)) / num2;

        if (bits == 0)
            continue;
        BN_bntest_rand(n, bits, 0, 1);
        BN_MONT_CTX_set(mont, n, ctx);

        BN_nnmod(a, a, n, ctx);
        BN_nnmod(b, b, n, ctx);

        BN_to_montgomery(A, a, mont, ctx);
        BN_to_montgomery(B, b, mont, ctx);

        BN_mod_mul_montgomery(c, A, B, mont, ctx);
        BN_from_montgomery(A, c, mont, ctx);
        if (bp != NULL) {
            if (!results) {
                BN_print(bp, a);
                BIO_puts(bp, " * ");
                BN_print(bp, b);
                BIO_puts(bp, " % ");
                BN_print(bp, &mont->N);
                BIO_puts(bp, " - ");
            }
            BN_print(bp, A);
            BIO_puts(bp, "\n");
        }
        BN_mod_mul(d, a, b, n, ctx);
        BN_sub(d, d, A);
        if (!BN_is_zero(d)) {
            fprintf(stderr, "Montgomery multiplication test failed!\n");
            return 0;
        }
    }

    /* Regression test for carry bug in mulx4x_mont */
    BN_hex2bn(&a,
        "7878787878787878787878787878787878787878787878787878787878787878"
        "7878787878787878787878787878787878787878787878787878787878787878"
        "7878787878787878787878787878787878787878787878787878787878787878"
        "7878787878787878787878787878787878787878787878787878787878787878");
    BN_hex2bn(&b,
        "095D72C08C097BA488C5E439C655A192EAFB6380073D8C2664668EDDB4060744"
        "E16E57FB4EDB9AE10A0CEFCDC28A894F689A128379DB279D48A2E20849D68593"
        "9B7803BCF46CEBF5C533FB0DD35B080593DE5472E3FE5DB951B8BFF9B4CB8F03"
        "9CC638A5EE8CDD703719F8000E6A9F63BEED5F2FCD52FF293EA05A251BB4AB81");
    BN_hex2bn(&n,
        "D78AF684E71DB0C39CFF4E64FB9DB567132CB9C50CC98009FEB820B26F2DED9B"
        "91B9B5E2B83AE0AE4EB4E0523CA726BFBE969B89FD754F674CE99118C3F2D1C5"
        "D81FDC7C54E02B60262B241D53C040E99E45826ECA37A804668E690E1AFC1CA4"
        "2C9A15D84D4954425F0B7642FC0BD9D7B24E2618D2DCC9B729D944BADACFDDAF");
    BN_MONT_CTX_set(mont, n, ctx);
    BN_mod_mul_montgomery(c, a, b, mont, ctx);
    BN_mod_mul_montgomery(d, b, a, mont, ctx);
    if (BN_cmp(c, d)) {
        fprintf(stderr, "Montgomery multiplication test failed:"
                        " a*b != b*a.\n");
        return 0;
    }

    BN_MONT_CTX_free(mont);
    BN_free(a);
    BN_free(b);
    BN_free(c);
    BN_free(d);
    BN_free(A);
    BN_free(B);
    BN_free(n);
    return (1);
}

int test_mod(BIO *bp, BN_CTX *ctx)
{
    BIGNUM *a, *b, *c, *d, *e;
    int i;

    a = BN_new();
    b = BN_new();
    c = BN_new();
    d = BN_new();
    e = BN_new();

    BN_bntest_rand(a, 1024, 0, 0);
    for (i = 0; i < num0; i++) {
        BN_bntest_rand(b, 450 + i * 10, 0, 0);
        a->neg = rand_neg();
        b->neg = rand_neg();
        BN_mod(c, a, b, ctx);
        if (bp != NULL) {
            if (!results) {
                BN_print(bp, a);
                BIO_puts(bp, " % ");
                BN_print(bp, b);
                BIO_puts(bp, " - ");
            }
            BN_print(bp, c);
            BIO_puts(bp, "\n");
        }
        BN_div(d, e, a, b, ctx);
        BN_sub(e, e, c);
        if (!BN_is_zero(e)) {
            fprintf(stderr, "Modulo test failed!\n");
            return 0;
        }
    }
    BN_free(a);
    BN_free(b);
    BN_free(c);
    BN_free(d);
    BN_free(e);
    return (1);
}

int test_mod_mul(BIO *bp, BN_CTX *ctx)
{
    BIGNUM *a, *b, *c, *d, *e;
    int i, j;

    a = BN_new();
    b = BN_new();
    c = BN_new();
    d = BN_new();
    e = BN_new();

    BN_one(a);
    BN_one(b);
    BN_zero(c);
    if (BN_mod_mul(e, a, b, c, ctx)) {
        fprintf(stderr, "BN_mod_mul with zero modulus succeeded!\n");
        return 0;
    }

    for (j = 0; j < 3; j++) {
        BN_bntest_rand(c, 1024, 0, 0);
        for (i = 0; i < num0; i++) {
            BN_bntest_rand(a, 475 + i * 10, 0, 0);
            BN_bntest_rand(b, 425 + i * 11, 0, 0);
            a->neg = rand_neg();
            b->neg = rand_neg();
            if (!BN_mod_mul(e, a, b, c, ctx)) {
                unsigned long l;

                while ((l = ERR_get_error()))
                    fprintf(stderr, "ERROR:%s\n", ERR_error_string(l, NULL));
                EXIT(1);
            }
            if (bp != NULL) {
                if (!results) {
                    BN_print(bp, a);
                    BIO_puts(bp, " * ");
                    BN_print(bp, b);
                    BIO_puts(bp, " % ");
                    BN_print(bp, c);
                    if ((a->neg ^ b->neg) && !BN_is_zero(e)) {
                        /*
                         * If (a*b) % c is negative, c must be added in order
                         * to obtain the normalized remainder (new with
                         * OpenSSL 0.9.7, previous versions of BN_mod_mul
                         * could generate negative results)
                         */
                        BIO_puts(bp, " + ");
                        BN_print(bp, c);
                    }
                    BIO_puts(bp, " - ");
                }
                BN_print(bp, e);
                BIO_puts(bp, "\n");
            }
            BN_mul(d, a, b, ctx);
            BN_sub(d, d, e);
            BN_div(a, b, d, c, ctx);
            if (!BN_is_zero(b)) {
                fprintf(stderr, "Modulo multiply test failed!\n");
                ERR_print_errors_fp(stderr);
                return 0;
            }
        }
    }
    BN_free(a);
    BN_free(b);
    BN_free(c);
    BN_free(d);
    BN_free(e);
    return (1);
}

int test_mod_exp(BIO *bp, BN_CTX *ctx)
{
    BIGNUM *a, *b, *c, *d, *e;
    int i;

    a = BN_new();
    b = BN_new();
    c = BN_new();
    d = BN_new();
    e = BN_new();

    BN_one(a);
    BN_one(b);
    BN_zero(c);
    if (BN_mod_exp(d, a, b, c, ctx)) {
        fprintf(stderr, "BN_mod_exp with zero modulus succeeded!\n");
        return 0;
    }

    BN_bntest_rand(c, 30, 0, 1); /* must be odd for montgomery */
    for (i = 0; i < num2; i++) {
        BN_bntest_rand(a, 20 + i * 5, 0, 0);
        BN_bntest_rand(b, 2 + i, 0, 0);

        if (!BN_mod_exp(d, a, b, c, ctx))
            return (0);

        if (bp != NULL) {
            if (!results) {
                BN_print(bp, a);
                BIO_puts(bp, " ^ ");
                BN_print(bp, b);
                BIO_puts(bp, " % ");
                BN_print(bp, c);
                BIO_puts(bp, " - ");
            }
            BN_print(bp, d);
            BIO_puts(bp, "\n");
        }
        BN_exp(e, a, b, ctx);
        BN_sub(e, e, d);
        BN_div(a, b, e, c, ctx);
        if (!BN_is_zero(b)) {
            fprintf(stderr, "Modulo exponentiation test failed!\n");
            return 0;
        }
    }

    /* Regression test for carry propagation bug in sqr8x_reduction */
    BN_hex2bn(&a, "050505050505");
    BN_hex2bn(&b, "02");
    BN_hex2bn(&c,
        "4141414141414141414141274141414141414141414141414141414141414141"
        "4141414141414141414141414141414141414141414141414141414141414141"
        "4141414141414141414141800000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000001");
    BN_mod_exp(d, a, b, c, ctx);
    BN_mul(e, a, a, ctx);
    if (BN_cmp(d, e)) {
        fprintf(stderr, "BN_mod_exp and BN_mul produce different results!\n");
        return 0;
    }

    BN_free(a);
    BN_free(b);
    BN_free(c);
    BN_free(d);
    BN_free(e);
    return (1);
}

int test_mod_exp_mont_consttime(BIO *bp, BN_CTX *ctx)
{
    BIGNUM *a, *b, *c, *d, *e;
    int i;

    a = BN_new();
    b = BN_new();
    c = BN_new();
    d = BN_new();
    e = BN_new();

    BN_one(a);
    BN_one(b);
    BN_zero(c);
    if (BN_mod_exp_mont_consttime(d, a, b, c, ctx, NULL)) {
        fprintf(stderr, "BN_mod_exp_mont_consttime with zero modulus "
                "succeeded\n");
        return 0;
    }

    BN_set_word(c, 16);
    if (BN_mod_exp_mont_consttime(d, a, b, c, ctx, NULL)) {
        fprintf(stderr, "BN_mod_exp_mont_consttime with even modulus "
                "succeeded\n");
        return 0;
    }

    BN_bntest_rand(c, 30, 0, 1); /* must be odd for montgomery */
    for (i = 0; i < num2; i++) {
        BN_bntest_rand(a, 20 + i * 5, 0, 0);
        BN_bntest_rand(b, 2 + i, 0, 0);

        if (!BN_mod_exp_mont_consttime(d, a, b, c, ctx, NULL))
            return (00);

        if (bp != NULL) {
            if (!results) {
                BN_print(bp, a);
                BIO_puts(bp, " ^ ");
                BN_print(bp, b);
                BIO_puts(bp, " % ");
                BN_print(bp, c);
                BIO_puts(bp, " - ");
            }
            BN_print(bp, d);
            BIO_puts(bp, "\n");
        }
        BN_exp(e, a, b, ctx);
        BN_sub(e, e, d);
        BN_div(a, b, e, c, ctx);
        if (!BN_is_zero(b)) {
            fprintf(stderr, "Modulo exponentiation test failed!\n");
            return 0;
        }
    }
    BN_free(a);
    BN_free(b);
    BN_free(c);
    BN_free(d);
    BN_free(e);
    return (1);
}

/*
 * Test constant-time modular exponentiation with 1024-bit inputs, which on
 * x86_64 cause a different code branch to be taken.
 */
int test_mod_exp_mont5(BIO *bp, BN_CTX *ctx)
{
    BIGNUM *a, *p, *m, *d, *e;
    BN_MONT_CTX *mont;

    a = BN_new();
    p = BN_new();
    m = BN_new();
    d = BN_new();
    e = BN_new();
    mont = BN_MONT_CTX_new();

    BN_bntest_rand(m, 1024, 0, 1); /* must be odd for montgomery */
    /* Zero exponent */
    BN_bntest_rand(a, 1024, 0, 0);
    BN_zero(p);
    if (!BN_mod_exp_mont_consttime(d, a, p, m, ctx, NULL))
        return 0;
    if (!BN_is_one(d)) {
        fprintf(stderr, "Modular exponentiation test failed!\n");
        return 0;
    }
    /* Zero input */
    BN_bntest_rand(p, 1024, 0, 0);
    BN_zero(a);
    if (!BN_mod_exp_mont_consttime(d, a, p, m, ctx, NULL))
        return 0;
    if (!BN_is_zero(d)) {
        fprintf(stderr, "Modular exponentiation test failed!\n");
        return 0;
    }
    /*
     * Craft an input whose Montgomery representation is 1, i.e., shorter
     * than the modulus m, in order to test the const time precomputation
     * scattering/gathering.
     */
    BN_one(a);
    BN_MONT_CTX_set(mont, m, ctx);
    if (!BN_from_montgomery(e, a, mont, ctx))
        return 0;
    if (!BN_mod_exp_mont_consttime(d, e, p, m, ctx, NULL))
        return 0;
    if (!BN_mod_exp_simple(a, e, p, m, ctx))
        return 0;
    if (BN_cmp(a, d) != 0) {
        fprintf(stderr, "Modular exponentiation test failed!\n");
        return 0;
    }
    /* Finally, some regular test vectors. */
    BN_bntest_rand(e, 1024, 0, 0);
    if (!BN_mod_exp_mont_consttime(d, e, p, m, ctx, NULL))
        return 0;
    if (!BN_mod_exp_simple(a, e, p, m, ctx))
        return 0;
    if (BN_cmp(a, d) != 0) {
        fprintf(stderr, "Modular exponentiation test failed!\n");
        return 0;
    }
    BN_MONT_CTX_free(mont);
    BN_free(a);
    BN_free(p);
    BN_free(m);
    BN_free(d);
    BN_free(e);
    return (1);
}

int test_exp(BIO *bp, BN_CTX *ctx)
{
    BIGNUM *a, *b, *d, *e, *one;
    int i;

    a = BN_new();
    b = BN_new();
    d = BN_new();
    e = BN_new();
    one = BN_new();
    BN_one(one);

    for (i = 0; i < num2; i++) {
        BN_bntest_rand(a, 20 + i * 5, 0, 0);
        BN_bntest_rand(b, 2 + i, 0, 0);

        if (BN_exp(d, a, b, ctx) <= 0)
            return (0);

        if (bp != NULL) {
            if (!results) {
                BN_print(bp, a);
                BIO_puts(bp, " ^ ");
                BN_print(bp, b);
                BIO_puts(bp, " - ");
            }
            BN_print(bp, d);
            BIO_puts(bp, "\n");
        }
        BN_one(e);
        for (; !BN_is_zero(b); BN_sub(b, b, one))
            BN_mul(e, e, a, ctx);
        BN_sub(e, e, d);
        if (!BN_is_zero(e)) {
            fprintf(stderr, "Exponentiation test failed!\n");
            return 0;
        }
    }
    BN_free(a);
    BN_free(b);
    BN_free(d);
    BN_free(e);
    BN_free(one);
    return (1);
}

#ifndef OPENSSL_NO_EC2M
int test_gf2m_add(BIO *bp)
{
    BIGNUM *a, *b, *c;
    int i, ret = 0;

    a = BN_new();
    b = BN_new();
    c = BN_new();

    for (i = 0; i < num0; i++) {
        BN_rand(a, 512, BN_RAND_TOP_ONE, BN_RAND_BOTTOM_ANY);
        BN_copy(b, BN_value_one());
        a->neg = rand_neg();
        b->neg = rand_neg();
        BN_GF2m_add(c, a, b);
        /* Test that two added values have the correct parity. */
        if ((BN_is_odd(a) && BN_is_odd(c))
            || (!BN_is_odd(a) && !BN_is_odd(c))) {
            fprintf(stderr, "GF(2^m) addition test (a) failed!\n");
            goto err;
        }
        BN_GF2m_add(c, c, c);
        /* Test that c + c = 0. */
        if (!BN_is_zero(c)) {
            fprintf(stderr, "GF(2^m) addition test (b) failed!\n");
            goto err;
        }
    }
    ret = 1;
 err:
    BN_free(a);
    BN_free(b);
    BN_free(c);
    return ret;
}

int test_gf2m_mod(BIO *bp)
{
    BIGNUM *a, *b[2], *c, *d, *e;
    int i, j, ret = 0;
    int p0[] = { 163, 7, 6, 3, 0, -1 };
    int p1[] = { 193, 15, 0, -1 };

    a = BN_new();
    b[0] = BN_new();
    b[1] = BN_new();
    c = BN_new();
    d = BN_new();
    e = BN_new();

    BN_GF2m_arr2poly(p0, b[0]);
    BN_GF2m_arr2poly(p1, b[1]);

    for (i = 0; i < num0; i++) {
        BN_bntest_rand(a, 1024, 0, 0);
        for (j = 0; j < 2; j++) {
            BN_GF2m_mod(c, a, b[j]);
            BN_GF2m_add(d, a, c);
            BN_GF2m_mod(e, d, b[j]);
            /* Test that a + (a mod p) mod p == 0. */
            if (!BN_is_zero(e)) {
                fprintf(stderr, "GF(2^m) modulo test failed!\n");
                goto err;
            }
        }
    }
    ret = 1;
 err:
    BN_free(a);
    BN_free(b[0]);
    BN_free(b[1]);
    BN_free(c);
    BN_free(d);
    BN_free(e);
    return ret;
}

int test_gf2m_mod_mul(BIO *bp, BN_CTX *ctx)
{
    BIGNUM *a, *b[2], *c, *d, *e, *f, *g, *h;
    int i, j, ret = 0;
    int p0[] = { 163, 7, 6, 3, 0, -1 };
    int p1[] = { 193, 15, 0, -1 };

    a = BN_new();
    b[0] = BN_new();
    b[1] = BN_new();
    c = BN_new();
    d = BN_new();
    e = BN_new();
    f = BN_new();
    g = BN_new();
    h = BN_new();

    BN_GF2m_arr2poly(p0, b[0]);
    BN_GF2m_arr2poly(p1, b[1]);

    for (i = 0; i < num0; i++) {
        BN_bntest_rand(a, 1024, 0, 0);
        BN_bntest_rand(c, 1024, 0, 0);
        BN_bntest_rand(d, 1024, 0, 0);
        for (j = 0; j < 2; j++) {
            BN_GF2m_mod_mul(e, a, c, b[j], ctx);
            BN_GF2m_add(f, a, d);
            BN_GF2m_mod_mul(g, f, c, b[j], ctx);
            BN_GF2m_mod_mul(h, d, c, b[j], ctx);
            BN_GF2m_add(f, e, g);
            BN_GF2m_add(f, f, h);
            /* Test that (a+d)*c = a*c + d*c. */
            if (!BN_is_zero(f)) {
                fprintf(stderr,
                        "GF(2^m) modular multiplication test failed!\n");
                goto err;
            }
        }
    }
    ret = 1;
 err:
    BN_free(a);
    BN_free(b[0]);
    BN_free(b[1]);
    BN_free(c);
    BN_free(d);
    BN_free(e);
    BN_free(f);
    BN_free(g);
    BN_free(h);
    return ret;
}

int test_gf2m_mod_sqr(BIO *bp, BN_CTX *ctx)
{
    BIGNUM *a, *b[2], *c, *d;
    int i, j, ret = 0;
    int p0[] = { 163, 7, 6, 3, 0, -1 };
    int p1[] = { 193, 15, 0, -1 };

    a = BN_new();
    b[0] = BN_new();
    b[1] = BN_new();
    c = BN_new();
    d = BN_new();

    BN_GF2m_arr2poly(p0, b[0]);
    BN_GF2m_arr2poly(p1, b[1]);

    for (i = 0; i < num0; i++) {
        BN_bntest_rand(a, 1024, 0, 0);
        for (j = 0; j < 2; j++) {
            BN_GF2m_mod_sqr(c, a, b[j], ctx);
            BN_copy(d, a);
            BN_GF2m_mod_mul(d, a, d, b[j], ctx);
            BN_GF2m_add(d, c, d);
            /* Test that a*a = a^2. */
            if (!BN_is_zero(d)) {
                fprintf(stderr, "GF(2^m) modular squaring test failed!\n");
                goto err;
            }
        }
    }
    ret = 1;
 err:
    BN_free(a);
    BN_free(b[0]);
    BN_free(b[1]);
    BN_free(c);
    BN_free(d);
    return ret;
}

int test_gf2m_mod_inv(BIO *bp, BN_CTX *ctx)
{
    BIGNUM *a, *b[2], *c, *d;
    int i, j, ret = 0;
    int p0[] = { 163, 7, 6, 3, 0, -1 };
    int p1[] = { 193, 15, 0, -1 };

    a = BN_new();
    b[0] = BN_new();
    b[1] = BN_new();
    c = BN_new();
    d = BN_new();

    BN_GF2m_arr2poly(p0, b[0]);
    BN_GF2m_arr2poly(p1, b[1]);

    for (i = 0; i < num0; i++) {
        BN_bntest_rand(a, 512, 0, 0);
        for (j = 0; j < 2; j++) {
            BN_GF2m_mod_inv(c, a, b[j], ctx);
            BN_GF2m_mod_mul(d, a, c, b[j], ctx);
            /* Test that ((1/a)*a) = 1. */
            if (!BN_is_one(d)) {
                fprintf(stderr, "GF(2^m) modular inversion test failed!\n");
                goto err;
            }
        }
    }
    ret = 1;
 err:
    BN_free(a);
    BN_free(b[0]);
    BN_free(b[1]);
    BN_free(c);
    BN_free(d);
    return ret;
}

int test_gf2m_mod_div(BIO *bp, BN_CTX *ctx)
{
    BIGNUM *a, *b[2], *c, *d, *e, *f;
    int i, j, ret = 0;
    int p0[] = { 163, 7, 6, 3, 0, -1 };
    int p1[] = { 193, 15, 0, -1 };

    a = BN_new();
    b[0] = BN_new();
    b[1] = BN_new();
    c = BN_new();
    d = BN_new();
    e = BN_new();
    f = BN_new();

    BN_GF2m_arr2poly(p0, b[0]);
    BN_GF2m_arr2poly(p1, b[1]);

    for (i = 0; i < num0; i++) {
        BN_bntest_rand(a, 512, 0, 0);
        BN_bntest_rand(c, 512, 0, 0);
        for (j = 0; j < 2; j++) {
            BN_GF2m_mod_div(d, a, c, b[j], ctx);
            BN_GF2m_mod_mul(e, d, c, b[j], ctx);
            BN_GF2m_mod_div(f, a, e, b[j], ctx);
            /* Test that ((a/c)*c)/a = 1. */
            if (!BN_is_one(f)) {
                fprintf(stderr, "GF(2^m) modular division test failed!\n");
                goto err;
            }
        }
    }
    ret = 1;
 err:
    BN_free(a);
    BN_free(b[0]);
    BN_free(b[1]);
    BN_free(c);
    BN_free(d);
    BN_free(e);
    BN_free(f);
    return ret;
}

int test_gf2m_mod_exp(BIO *bp, BN_CTX *ctx)
{
    BIGNUM *a, *b[2], *c, *d, *e, *f;
    int i, j, ret = 0;
    int p0[] = { 163, 7, 6, 3, 0, -1 };
    int p1[] = { 193, 15, 0, -1 };

    a = BN_new();
    b[0] = BN_new();
    b[1] = BN_new();
    c = BN_new();
    d = BN_new();
    e = BN_new();
    f = BN_new();

    BN_GF2m_arr2poly(p0, b[0]);
    BN_GF2m_arr2poly(p1, b[1]);

    for (i = 0; i < num0; i++) {
        BN_bntest_rand(a, 512, 0, 0);
        BN_bntest_rand(c, 512, 0, 0);
        BN_bntest_rand(d, 512, 0, 0);
        for (j = 0; j < 2; j++) {
            BN_GF2m_mod_exp(e, a, c, b[j], ctx);
            BN_GF2m_mod_exp(f, a, d, b[j], ctx);
            BN_GF2m_mod_mul(e, e, f, b[j], ctx);
            BN_add(f, c, d);
            BN_GF2m_mod_exp(f, a, f, b[j], ctx);
            BN_GF2m_add(f, e, f);
            /* Test that a^(c+d)=a^c*a^d. */
            if (!BN_is_zero(f)) {
                fprintf(stderr,
                        "GF(2^m) modular exponentiation test failed!\n");
                goto err;
            }
        }
    }
    ret = 1;
 err:
    BN_free(a);
    BN_free(b[0]);
    BN_free(b[1]);
    BN_free(c);
    BN_free(d);
    BN_free(e);
    BN_free(f);
    return ret;
}

int test_gf2m_mod_sqrt(BIO *bp, BN_CTX *ctx)
{
    BIGNUM *a, *b[2], *c, *d, *e, *f;
    int i, j, ret = 0;
    int p0[] = { 163, 7, 6, 3, 0, -1 };
    int p1[] = { 193, 15, 0, -1 };

    a = BN_new();
    b[0] = BN_new();
    b[1] = BN_new();
    c = BN_new();
    d = BN_new();
    e = BN_new();
    f = BN_new();

    BN_GF2m_arr2poly(p0, b[0]);
    BN_GF2m_arr2poly(p1, b[1]);

    for (i = 0; i < num0; i++) {
        BN_bntest_rand(a, 512, 0, 0);
        for (j = 0; j < 2; j++) {
            BN_GF2m_mod(c, a, b[j]);
            BN_GF2m_mod_sqrt(d, a, b[j], ctx);
            BN_GF2m_mod_sqr(e, d, b[j], ctx);
            BN_GF2m_add(f, c, e);
            /* Test that d^2 = a, where d = sqrt(a). */
            if (!BN_is_zero(f)) {
                fprintf(stderr, "GF(2^m) modular square root test failed!\n");
                goto err;
            }
        }
    }
    ret = 1;
 err:
    BN_free(a);
    BN_free(b[0]);
    BN_free(b[1]);
    BN_free(c);
    BN_free(d);
    BN_free(e);
    BN_free(f);
    return ret;
}

int test_gf2m_mod_solve_quad(BIO *bp, BN_CTX *ctx)
{
    BIGNUM *a, *b[2], *c, *d, *e;
    int i, j, s = 0, t, ret = 0;
    int p0[] = { 163, 7, 6, 3, 0, -1 };
    int p1[] = { 193, 15, 0, -1 };

    a = BN_new();
    b[0] = BN_new();
    b[1] = BN_new();
    c = BN_new();
    d = BN_new();
    e = BN_new();

    BN_GF2m_arr2poly(p0, b[0]);
    BN_GF2m_arr2poly(p1, b[1]);

    for (i = 0; i < num0; i++) {
        BN_bntest_rand(a, 512, 0, 0);
        for (j = 0; j < 2; j++) {
            t = BN_GF2m_mod_solve_quad(c, a, b[j], ctx);
            if (t) {
                s++;
                BN_GF2m_mod_sqr(d, c, b[j], ctx);
                BN_GF2m_add(d, c, d);
                BN_GF2m_mod(e, a, b[j]);
                BN_GF2m_add(e, e, d);
                /*
                 * Test that solution of quadratic c satisfies c^2 + c = a.
                 */
                if (!BN_is_zero(e)) {
                    fprintf(stderr,
                            "GF(2^m) modular solve quadratic test failed!\n");
                    goto err;
                }

            }
        }
    }
    if (s == 0) {
        fprintf(stderr,
                "All %i tests of GF(2^m) modular solve quadratic resulted in no roots;\n",
                num0);
        fprintf(stderr,
                "this is very unlikely and probably indicates an error.\n");
        goto err;
    }
    ret = 1;
 err:
    BN_free(a);
    BN_free(b[0]);
    BN_free(b[1]);
    BN_free(c);
    BN_free(d);
    BN_free(e);
    return ret;
}
#endif
static int genprime_cb(int p, int n, BN_GENCB *arg)
{
    char c = '*';

    if (p == 0)
        c = '.';
    if (p == 1)
        c = '+';
    if (p == 2)
        c = '*';
    if (p == 3)
        c = '\n';
    putc(c, stderr);
    fflush(stderr);
    return 1;
}

int test_kron(BIO *bp, BN_CTX *ctx)
{
    BN_GENCB cb;
    BIGNUM *a, *b, *r, *t;
    int i;
    int legendre, kronecker;
    int ret = 0;

    a = BN_new();
    b = BN_new();
    r = BN_new();
    t = BN_new();
    if (a == NULL || b == NULL || r == NULL || t == NULL)
        goto err;

    BN_GENCB_set(&cb, genprime_cb, NULL);

    /*
     * We test BN_kronecker(a, b, ctx) just for b odd (Jacobi symbol). In
     * this case we know that if b is prime, then BN_kronecker(a, b, ctx) is
     * congruent to $a^{(b-1)/2}$, modulo $b$ (Legendre symbol). So we
     * generate a random prime b and compare these values for a number of
     * random a's.  (That is, we run the Solovay-Strassen primality test to
     * confirm that b is prime, except that we don't want to test whether b
     * is prime but whether BN_kronecker works.)
     */

    if (!BN_generate_prime_ex(b, 512, 0, NULL, NULL, &cb))
        goto err;
    b->neg = rand_neg();
    putc('\n', stderr);

    for (i = 0; i < num0; i++) {
        if (!BN_bntest_rand(a, 512, 0, 0))
            goto err;
        a->neg = rand_neg();

        /* t := (|b|-1)/2  (note that b is odd) */
        if (!BN_copy(t, b))
            goto err;
        t->neg = 0;
        if (!BN_sub_word(t, 1))
            goto err;
        if (!BN_rshift1(t, t))
            goto err;
        /* r := a^t mod b */
        b->neg = 0;

        if (!BN_mod_exp_recp(r, a, t, b, ctx))
            goto err;
        b->neg = 1;

        if (BN_is_word(r, 1))
            legendre = 1;
        else if (BN_is_zero(r))
            legendre = 0;
        else {
            if (!BN_add_word(r, 1))
                goto err;
            if (0 != BN_ucmp(r, b)) {
                fprintf(stderr, "Legendre symbol computation failed\n");
                goto err;
            }
            legendre = -1;
        }

        kronecker = BN_kronecker(a, b, ctx);
        if (kronecker < -1)
            goto err;
        /* we actually need BN_kronecker(a, |b|) */
        if (a->neg && b->neg)
            kronecker = -kronecker;

        if (legendre != kronecker) {
            fprintf(stderr, "legendre != kronecker; a = ");
            BN_print_fp(stderr, a);
            fprintf(stderr, ", b = ");
            BN_print_fp(stderr, b);
            fprintf(stderr, "\n");
            goto err;
        }

        putc('.', stderr);
        fflush(stderr);
    }

    putc('\n', stderr);
    fflush(stderr);
    ret = 1;
 err:
    BN_free(a);
    BN_free(b);
    BN_free(r);
    BN_free(t);
    return ret;
}

int test_sqrt(BIO *bp, BN_CTX *ctx)
{
    BN_GENCB cb;
    BIGNUM *a, *p, *r;
    int i, j;
    int ret = 0;

    a = BN_new();
    p = BN_new();
    r = BN_new();
    if (a == NULL || p == NULL || r == NULL)
        goto err;

    BN_GENCB_set(&cb, genprime_cb, NULL);

    for (i = 0; i < 16; i++) {
        if (i < 8) {
            unsigned primes[8] = { 2, 3, 5, 7, 11, 13, 17, 19 };

            if (!BN_set_word(p, primes[i]))
                goto err;
        } else {
            if (!BN_set_word(a, 32))
                goto err;
            if (!BN_set_word(r, 2 * i + 1))
                goto err;

            if (!BN_generate_prime_ex(p, 256, 0, a, r, &cb))
                goto err;
            putc('\n', stderr);
        }
        p->neg = rand_neg();

        for (j = 0; j < num2; j++) {
            /*
             * construct 'a' such that it is a square modulo p, but in
             * general not a proper square and not reduced modulo p
             */
            if (!BN_bntest_rand(r, 256, 0, 3))
                goto err;
            if (!BN_nnmod(r, r, p, ctx))
                goto err;
            if (!BN_mod_sqr(r, r, p, ctx))
                goto err;
            if (!BN_bntest_rand(a, 256, 0, 3))
                goto err;
            if (!BN_nnmod(a, a, p, ctx))
                goto err;
            if (!BN_mod_sqr(a, a, p, ctx))
                goto err;
            if (!BN_mul(a, a, r, ctx))
                goto err;
            if (rand_neg())
                if (!BN_sub(a, a, p))
                    goto err;

            if (!BN_mod_sqrt(r, a, p, ctx))
                goto err;
            if (!BN_mod_sqr(r, r, p, ctx))
                goto err;

            if (!BN_nnmod(a, a, p, ctx))
                goto err;

            if (BN_cmp(a, r) != 0) {
                fprintf(stderr, "BN_mod_sqrt failed: a = ");
                BN_print_fp(stderr, a);
                fprintf(stderr, ", r = ");
                BN_print_fp(stderr, r);
                fprintf(stderr, ", p = ");
                BN_print_fp(stderr, p);
                fprintf(stderr, "\n");
                goto err;
            }

            putc('.', stderr);
            fflush(stderr);
        }

        putc('\n', stderr);
        fflush(stderr);
    }
    ret = 1;
 err:
    BN_free(a);
    BN_free(p);
    BN_free(r);
    return ret;
}

int test_small_prime(BIO *bp, BN_CTX *ctx)
{
    static const int bits = 10;
    int ret = 0;
    BIGNUM *r;

    r = BN_new();
    if (!BN_generate_prime_ex(r, bits, 0, NULL, NULL, NULL))
        goto err;
    if (BN_num_bits(r) != bits) {
        BIO_printf(bp, "Expected %d bit prime, got %d bit number\n", bits,
                   BN_num_bits(r));
        goto err;
    }

    ret = 1;

 err:
    BN_clear_free(r);
    return ret;
}

int test_bn2dec(BIO *bp)
{
    static const char *bn2dec_tests[] = {
        "0",
        "1",
        "-1",
        "100",
        "-100",
        "123456789012345678901234567890",
        "-123456789012345678901234567890",
        "123456789012345678901234567890123456789012345678901234567890",
        "-123456789012345678901234567890123456789012345678901234567890",
    };
    int ret = 0;
    size_t i;
    BIGNUM *bn = NULL;
    char *dec = NULL;

    for (i = 0; i < OSSL_NELEM(bn2dec_tests); i++) {
        if (!BN_dec2bn(&bn, bn2dec_tests[i]))
            goto err;

        dec = BN_bn2dec(bn);
        if (dec == NULL) {
            fprintf(stderr, "BN_bn2dec failed on %s.\n", bn2dec_tests[i]);
            goto err;
        }

        if (strcmp(dec, bn2dec_tests[i]) != 0) {
            fprintf(stderr, "BN_bn2dec gave %s, wanted %s.\n", dec,
                    bn2dec_tests[i]);
            goto err;
        }

        OPENSSL_free(dec);
        dec = NULL;
    }

    ret = 1;

err:
    BN_free(bn);
    OPENSSL_free(dec);
    return ret;
}

int test_lshift(BIO *bp, BN_CTX *ctx, BIGNUM *a_)
{
    BIGNUM *a, *b, *c, *d;
    int i;

    b = BN_new();
    c = BN_new();
    d = BN_new();
    BN_one(c);

    if (a_)
        a = a_;
    else {
        a = BN_new();
        BN_bntest_rand(a, 200, 0, 0);
        a->neg = rand_neg();
    }
    for (i = 0; i < num0; i++) {
        BN_lshift(b, a, i + 1);
        BN_add(c, c, c);
        if (bp != NULL) {
            if (!results) {
                BN_print(bp, a);
                BIO_puts(bp, " * ");
                BN_print(bp, c);
                BIO_puts(bp, " - ");
            }
            BN_print(bp, b);
            BIO_puts(bp, "\n");
        }
        BN_mul(d, a, c, ctx);
        BN_sub(d, d, b);
        if (!BN_is_zero(d)) {
            fprintf(stderr, "Left shift test failed!\n");
            fprintf(stderr, "a=");
            BN_print_fp(stderr, a);
            fprintf(stderr, "\nb=");
            BN_print_fp(stderr, b);
            fprintf(stderr, "\nc=");
            BN_print_fp(stderr, c);
            fprintf(stderr, "\nd=");
            BN_print_fp(stderr, d);
            fprintf(stderr, "\n");
            return 0;
        }
    }
    BN_free(a);
    BN_free(b);
    BN_free(c);
    BN_free(d);
    return (1);
}

int test_lshift1(BIO *bp)
{
    BIGNUM *a, *b, *c;
    int i;

    a = BN_new();
    b = BN_new();
    c = BN_new();

    BN_bntest_rand(a, 200, 0, 0);
    a->neg = rand_neg();
    for (i = 0; i < num0; i++) {
        BN_lshift1(b, a);
        if (bp != NULL) {
            if (!results) {
                BN_print(bp, a);
                BIO_puts(bp, " * 2");
                BIO_puts(bp, " - ");
            }
            BN_print(bp, b);
            BIO_puts(bp, "\n");
        }
        BN_add(c, a, a);
        BN_sub(a, b, c);
        if (!BN_is_zero(a)) {
            fprintf(stderr, "Left shift one test failed!\n");
            return 0;
        }

        BN_copy(a, b);
    }
    BN_free(a);
    BN_free(b);
    BN_free(c);
    return (1);
}

int test_rshift(BIO *bp, BN_CTX *ctx)
{
    BIGNUM *a, *b, *c, *d, *e;
    int i;

    a = BN_new();
    b = BN_new();
    c = BN_new();
    d = BN_new();
    e = BN_new();
    BN_one(c);

    BN_bntest_rand(a, 200, 0, 0);
    a->neg = rand_neg();
    for (i = 0; i < num0; i++) {
        BN_rshift(b, a, i + 1);
        BN_add(c, c, c);
        if (bp != NULL) {
            if (!results) {
                BN_print(bp, a);
                BIO_puts(bp, " / ");
                BN_print(bp, c);
                BIO_puts(bp, " - ");
            }
            BN_print(bp, b);
            BIO_puts(bp, "\n");
        }
        BN_div(d, e, a, c, ctx);
        BN_sub(d, d, b);
        if (!BN_is_zero(d)) {
            fprintf(stderr, "Right shift test failed!\n");
            return 0;
        }
    }
    BN_free(a);
    BN_free(b);
    BN_free(c);
    BN_free(d);
    BN_free(e);
    return (1);
}

int test_rshift1(BIO *bp)
{
    BIGNUM *a, *b, *c;
    int i;

    a = BN_new();
    b = BN_new();
    c = BN_new();

    BN_bntest_rand(a, 200, 0, 0);
    a->neg = rand_neg();
    for (i = 0; i < num0; i++) {
        BN_rshift1(b, a);
        if (bp != NULL) {
            if (!results) {
                BN_print(bp, a);
                BIO_puts(bp, " / 2");
                BIO_puts(bp, " - ");
            }
            BN_print(bp, b);
            BIO_puts(bp, "\n");
        }
        BN_sub(c, a, b);
        BN_sub(c, c, b);
        if (!BN_is_zero(c) && !BN_abs_is_word(c, 1)) {
            fprintf(stderr, "Right shift one test failed!\n");
            return 0;
        }
        BN_copy(a, b);
    }
    BN_free(a);
    BN_free(b);
    BN_free(c);
    return (1);
}

int rand_neg(void)
{
    static unsigned int neg = 0;
    static int sign[8] = { 0, 0, 0, 1, 1, 0, 1, 1 };

    return (sign[(neg++) % 8]);
}
