/* crypto/ec/ectest.c */
/*
 * Originally written by Bodo Moeller for the OpenSSL project.
 */
/* ====================================================================
 * Copyright (c) 1998-2001 The OpenSSL Project.  All rights reserved.
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
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
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
/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 *
 * Portions of the attached software ("Contribution") are developed by
 * SUN MICROSYSTEMS, INC., and are contributed to the OpenSSL project.
 *
 * The Contribution is licensed pursuant to the OpenSSL open source
 * license provided above.
 *
 * The elliptic curve binary polynomial software is originally written by
 * Sheueling Chang Shantz and Douglas Stebila of Sun Microsystems Laboratories.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#ifdef FLAT_INC
# include "e_os.h"
#else
# include "../e_os.h"
#endif
#include <string.h>
#include <time.h>

#ifdef OPENSSL_NO_EC
int main(int argc, char *argv[])
{
    puts("Elliptic curves are disabled.");
    return 0;
}
#else

# include <openssl/ec.h>
# ifndef OPENSSL_NO_ENGINE
#  include <openssl/engine.h>
# endif
# include <openssl/err.h>
# include <openssl/obj_mac.h>
# include <openssl/objects.h>
# include <openssl/rand.h>
# include <openssl/bn.h>
# include <openssl/opensslconf.h>

# if defined(_MSC_VER) && defined(_MIPS_) && (_MSC_VER/100==12)
/* suppress "too big too optimize" warning */
#  pragma warning(disable:4959)
# endif

# define ABORT do { \
        fflush(stdout); \
        fprintf(stderr, "%s:%d: ABORT\n", __FILE__, __LINE__); \
        ERR_print_errors_fp(stderr); \
        EXIT(1); \
} while (0)

# define TIMING_BASE_PT 0
# define TIMING_RAND_PT 1
# define TIMING_SIMUL 2

# if 0
static void timings(EC_GROUP *group, int type, BN_CTX *ctx)
{
    clock_t clck;
    int i, j;
    BIGNUM *s;
    BIGNUM *r[10], *r0[10];
    EC_POINT *P;

    s = BN_new();
    if (s == NULL)
        ABORT;

    fprintf(stdout, "Timings for %d-bit field, ", EC_GROUP_get_degree(group));
    if (!EC_GROUP_get_order(group, s, ctx))
        ABORT;
    fprintf(stdout, "%d-bit scalars ", (int)BN_num_bits(s));
    fflush(stdout);

    P = EC_POINT_new(group);
    if (P == NULL)
        ABORT;
    EC_POINT_copy(P, EC_GROUP_get0_generator(group));

    for (i = 0; i < 10; i++) {
        if ((r[i] = BN_new()) == NULL)
            ABORT;
        if (!BN_pseudo_rand(r[i], BN_num_bits(s), 0, 0))
            ABORT;
        if (type != TIMING_BASE_PT) {
            if ((r0[i] = BN_new()) == NULL)
                ABORT;
            if (!BN_pseudo_rand(r0[i], BN_num_bits(s), 0, 0))
                ABORT;
        }
    }

    clck = clock();
    for (i = 0; i < 10; i++) {
        for (j = 0; j < 10; j++) {
            if (!EC_POINT_mul
                (group, P, (type != TIMING_RAND_PT) ? r[i] : NULL,
                 (type != TIMING_BASE_PT) ? P : NULL,
                 (type != TIMING_BASE_PT) ? r0[i] : NULL, ctx))
                ABORT;
        }
    }
    clck = clock() - clck;

    fprintf(stdout, "\n");

#  ifdef CLOCKS_PER_SEC
    /*
     * "To determine the time in seconds, the value returned by the clock
     * function should be divided by the value of the macro CLOCKS_PER_SEC."
     * -- ISO/IEC 9899
     */
#   define UNIT "s"
#  else
    /*
     * "`CLOCKS_PER_SEC' undeclared (first use this function)" -- cc on
     * NeXTstep/OpenStep
     */
#   define UNIT "units"
#   define CLOCKS_PER_SEC 1
#  endif

    if (type == TIMING_BASE_PT) {
        fprintf(stdout, "%i %s in %.2f " UNIT "\n", i * j,
                "base point multiplications", (double)clck / CLOCKS_PER_SEC);
    } else if (type == TIMING_RAND_PT) {
        fprintf(stdout, "%i %s in %.2f " UNIT "\n", i * j,
                "random point multiplications",
                (double)clck / CLOCKS_PER_SEC);
    } else if (type == TIMING_SIMUL) {
        fprintf(stdout, "%i %s in %.2f " UNIT "\n", i * j,
                "s*P+t*Q operations", (double)clck / CLOCKS_PER_SEC);
    }
    fprintf(stdout, "average: %.4f " UNIT "\n",
            (double)clck / (CLOCKS_PER_SEC * i * j));

    EC_POINT_free(P);
    BN_free(s);
    for (i = 0; i < 10; i++) {
        BN_free(r[i]);
        if (type != TIMING_BASE_PT)
            BN_free(r0[i]);
    }
}
# endif

/* test multiplication with group order, long and negative scalars */
static void group_order_tests(EC_GROUP *group)
{
    BIGNUM *n1, *n2, *order;
    EC_POINT *P = EC_POINT_new(group);
    EC_POINT *Q = EC_POINT_new(group);
    BN_CTX *ctx = BN_CTX_new();
    int i;

    n1 = BN_new();
    n2 = BN_new();
    order = BN_new();
    fprintf(stdout, "verify group order ...");
    fflush(stdout);
    if (!EC_GROUP_get_order(group, order, ctx))
        ABORT;
    if (!EC_POINT_mul(group, Q, order, NULL, NULL, ctx))
        ABORT;
    if (!EC_POINT_is_at_infinity(group, Q))
        ABORT;
    fprintf(stdout, ".");
    fflush(stdout);
    if (!EC_GROUP_precompute_mult(group, ctx))
        ABORT;
    if (!EC_POINT_mul(group, Q, order, NULL, NULL, ctx))
        ABORT;
    if (!EC_POINT_is_at_infinity(group, Q))
        ABORT;
    fprintf(stdout, " ok\n");
    fprintf(stdout, "long/negative scalar tests ");
    for (i = 1; i <= 2; i++) {
        const BIGNUM *scalars[6];
        const EC_POINT *points[6];

        fprintf(stdout, i == 1 ?
                "allowing precomputation ... " :
                "without precomputation ... ");
        if (!BN_set_word(n1, i))
            ABORT;
        /*
         * If i == 1, P will be the predefined generator for which
         * EC_GROUP_precompute_mult has set up precomputation.
         */
        if (!EC_POINT_mul(group, P, n1, NULL, NULL, ctx))
            ABORT;

        if (!BN_one(n1))
            ABORT;
        /* n1 = 1 - order */
        if (!BN_sub(n1, n1, order))
            ABORT;
        if (!EC_POINT_mul(group, Q, NULL, P, n1, ctx))
            ABORT;
        if (0 != EC_POINT_cmp(group, Q, P, ctx))
            ABORT;

        /* n2 = 1 + order */
        if (!BN_add(n2, order, BN_value_one()))
            ABORT;
        if (!EC_POINT_mul(group, Q, NULL, P, n2, ctx))
            ABORT;
        if (0 != EC_POINT_cmp(group, Q, P, ctx))
            ABORT;

        /* n2 = (1 - order) * (1 + order) = 1 - order^2 */
        if (!BN_mul(n2, n1, n2, ctx))
            ABORT;
        if (!EC_POINT_mul(group, Q, NULL, P, n2, ctx))
            ABORT;
        if (0 != EC_POINT_cmp(group, Q, P, ctx))
            ABORT;

        /* n2 = order^2 - 1 */
        BN_set_negative(n2, 0);
        if (!EC_POINT_mul(group, Q, NULL, P, n2, ctx))
            ABORT;
        /* Add P to verify the result. */
        if (!EC_POINT_add(group, Q, Q, P, ctx))
            ABORT;
        if (!EC_POINT_is_at_infinity(group, Q))
            ABORT;

        /* Exercise EC_POINTs_mul, including corner cases. */
        if (EC_POINT_is_at_infinity(group, P))
            ABORT;
        scalars[0] = n1;
        points[0] = Q;          /* => infinity */
        scalars[1] = n2;
        points[1] = P;          /* => -P */
        scalars[2] = n1;
        points[2] = Q;          /* => infinity */
        scalars[3] = n2;
        points[3] = Q;          /* => infinity */
        scalars[4] = n1;
        points[4] = P;          /* => P */
        scalars[5] = n2;
        points[5] = Q;          /* => infinity */
        if (!EC_POINTs_mul(group, P, NULL, 6, points, scalars, ctx))
            ABORT;
        if (!EC_POINT_is_at_infinity(group, P))
            ABORT;
    }
    fprintf(stdout, "ok\n");

    EC_POINT_free(P);
    EC_POINT_free(Q);
    BN_free(n1);
    BN_free(n2);
    BN_free(order);
    BN_CTX_free(ctx);
}

static void prime_field_tests(void)
{
    BN_CTX *ctx = NULL;
    BIGNUM *p, *a, *b;
    EC_GROUP *group;
    EC_GROUP *P_160 = NULL, *P_192 = NULL, *P_224 = NULL, *P_256 =
        NULL, *P_384 = NULL, *P_521 = NULL;
    EC_POINT *P, *Q, *R;
    BIGNUM *x, *y, *z;
    unsigned char buf[100];
    size_t i, len;
    int k;

# if 1                          /* optional */
    ctx = BN_CTX_new();
    if (!ctx)
        ABORT;
# endif

    p = BN_new();
    a = BN_new();
    b = BN_new();
    if (!p || !a || !b)
        ABORT;

    if (!BN_hex2bn(&p, "17"))
        ABORT;
    if (!BN_hex2bn(&a, "1"))
        ABORT;
    if (!BN_hex2bn(&b, "1"))
        ABORT;

    group = EC_GROUP_new(EC_GFp_mont_method()); /* applications should use
                                                 * EC_GROUP_new_curve_GFp so
                                                 * that the library gets to
                                                 * choose the EC_METHOD */
    if (!group)
        ABORT;

    if (!EC_GROUP_set_curve_GFp(group, p, a, b, ctx))
        ABORT;

    {
        EC_GROUP *tmp;
        tmp = EC_GROUP_new(EC_GROUP_method_of(group));
        if (!tmp)
            ABORT;
        if (!EC_GROUP_copy(tmp, group))
            ABORT;
        EC_GROUP_free(group);
        group = tmp;
    }

    if (!EC_GROUP_get_curve_GFp(group, p, a, b, ctx))
        ABORT;

    fprintf(stdout,
            "Curve defined by Weierstrass equation\n     y^2 = x^3 + a*x + b  (mod 0x");
    BN_print_fp(stdout, p);
    fprintf(stdout, ")\n     a = 0x");
    BN_print_fp(stdout, a);
    fprintf(stdout, "\n     b = 0x");
    BN_print_fp(stdout, b);
    fprintf(stdout, "\n");

    P = EC_POINT_new(group);
    Q = EC_POINT_new(group);
    R = EC_POINT_new(group);
    if (!P || !Q || !R)
        ABORT;

    if (!EC_POINT_set_to_infinity(group, P))
        ABORT;
    if (!EC_POINT_is_at_infinity(group, P))
        ABORT;

    buf[0] = 0;
    if (!EC_POINT_oct2point(group, Q, buf, 1, ctx))
        ABORT;

    if (!EC_POINT_add(group, P, P, Q, ctx))
        ABORT;
    if (!EC_POINT_is_at_infinity(group, P))
        ABORT;

    x = BN_new();
    y = BN_new();
    z = BN_new();
    if (!x || !y || !z)
        ABORT;

    if (!BN_hex2bn(&x, "D"))
        ABORT;
    if (!EC_POINT_set_compressed_coordinates_GFp(group, Q, x, 1, ctx))
        ABORT;
    if (EC_POINT_is_on_curve(group, Q, ctx) <= 0) {
        if (!EC_POINT_get_affine_coordinates_GFp(group, Q, x, y, ctx))
            ABORT;
        fprintf(stderr, "Point is not on curve: x = 0x");
        BN_print_fp(stderr, x);
        fprintf(stderr, ", y = 0x");
        BN_print_fp(stderr, y);
        fprintf(stderr, "\n");
        ABORT;
    }

    fprintf(stdout, "A cyclic subgroup:\n");
    k = 100;
    do {
        if (k-- == 0)
            ABORT;

        if (EC_POINT_is_at_infinity(group, P))
            fprintf(stdout, "     point at infinity\n");
        else {
            if (!EC_POINT_get_affine_coordinates_GFp(group, P, x, y, ctx))
                ABORT;

            fprintf(stdout, "     x = 0x");
            BN_print_fp(stdout, x);
            fprintf(stdout, ", y = 0x");
            BN_print_fp(stdout, y);
            fprintf(stdout, "\n");
        }

        if (!EC_POINT_copy(R, P))
            ABORT;
        if (!EC_POINT_add(group, P, P, Q, ctx))
            ABORT;

# if 0                          /* optional */
        {
            EC_POINT *points[3];

            points[0] = R;
            points[1] = Q;
            points[2] = P;
            if (!EC_POINTs_make_affine(group, 2, points, ctx))
                ABORT;
        }
# endif

    }
    while (!EC_POINT_is_at_infinity(group, P));

    if (!EC_POINT_add(group, P, Q, R, ctx))
        ABORT;
    if (!EC_POINT_is_at_infinity(group, P))
        ABORT;

    len =
        EC_POINT_point2oct(group, Q, POINT_CONVERSION_COMPRESSED, buf,
                           sizeof buf, ctx);
    if (len == 0)
        ABORT;
    if (!EC_POINT_oct2point(group, P, buf, len, ctx))
        ABORT;
    if (0 != EC_POINT_cmp(group, P, Q, ctx))
        ABORT;
    fprintf(stdout, "Generator as octet string, compressed form:\n     ");
    for (i = 0; i < len; i++)
        fprintf(stdout, "%02X", buf[i]);

    len =
        EC_POINT_point2oct(group, Q, POINT_CONVERSION_UNCOMPRESSED, buf,
                           sizeof buf, ctx);
    if (len == 0)
        ABORT;
    if (!EC_POINT_oct2point(group, P, buf, len, ctx))
        ABORT;
    if (0 != EC_POINT_cmp(group, P, Q, ctx))
        ABORT;
    fprintf(stdout, "\nGenerator as octet string, uncompressed form:\n     ");
    for (i = 0; i < len; i++)
        fprintf(stdout, "%02X", buf[i]);

    len =
        EC_POINT_point2oct(group, Q, POINT_CONVERSION_HYBRID, buf, sizeof buf,
                           ctx);
    if (len == 0)
        ABORT;
    if (!EC_POINT_oct2point(group, P, buf, len, ctx))
        ABORT;
    if (0 != EC_POINT_cmp(group, P, Q, ctx))
        ABORT;
    fprintf(stdout, "\nGenerator as octet string, hybrid form:\n     ");
    for (i = 0; i < len; i++)
        fprintf(stdout, "%02X", buf[i]);

    if (!EC_POINT_get_Jprojective_coordinates_GFp(group, R, x, y, z, ctx))
        ABORT;
    fprintf(stdout,
            "\nA representation of the inverse of that generator in\nJacobian projective coordinates:\n     X = 0x");
    BN_print_fp(stdout, x);
    fprintf(stdout, ", Y = 0x");
    BN_print_fp(stdout, y);
    fprintf(stdout, ", Z = 0x");
    BN_print_fp(stdout, z);
    fprintf(stdout, "\n");

    if (!EC_POINT_invert(group, P, ctx))
        ABORT;
    if (0 != EC_POINT_cmp(group, P, R, ctx))
        ABORT;

    /*
     * Curve secp160r1 (Certicom Research SEC 2 Version 1.0, section 2.4.2,
     * 2000) -- not a NIST curve, but commonly used
     */

    if (!BN_hex2bn(&p, "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7FFFFFFF"))
        ABORT;
    if (1 != BN_is_prime_ex(p, BN_prime_checks, ctx, NULL))
        ABORT;
    if (!BN_hex2bn(&a, "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7FFFFFFC"))
        ABORT;
    if (!BN_hex2bn(&b, "1C97BEFC54BD7A8B65ACF89F81D4D4ADC565FA45"))
        ABORT;
    if (!EC_GROUP_set_curve_GFp(group, p, a, b, ctx))
        ABORT;

    if (!BN_hex2bn(&x, "4A96B5688EF573284664698968C38BB913CBFC82"))
        ABORT;
    if (!BN_hex2bn(&y, "23a628553168947d59dcc912042351377ac5fb32"))
        ABORT;
    if (!EC_POINT_set_affine_coordinates_GFp(group, P, x, y, ctx))
        ABORT;
    if (EC_POINT_is_on_curve(group, P, ctx) <= 0)
        ABORT;
    if (!BN_hex2bn(&z, "0100000000000000000001F4C8F927AED3CA752257"))
        ABORT;
    if (!EC_GROUP_set_generator(group, P, z, BN_value_one()))
        ABORT;

    if (!EC_POINT_get_affine_coordinates_GFp(group, P, x, y, ctx))
        ABORT;
    fprintf(stdout, "\nSEC2 curve secp160r1 -- Generator:\n     x = 0x");
    BN_print_fp(stdout, x);
    fprintf(stdout, "\n     y = 0x");
    BN_print_fp(stdout, y);
    fprintf(stdout, "\n");
    /* G_y value taken from the standard: */
    if (!BN_hex2bn(&z, "23a628553168947d59dcc912042351377ac5fb32"))
        ABORT;
    if (0 != BN_cmp(y, z))
        ABORT;

    fprintf(stdout, "verify degree ...");
    if (EC_GROUP_get_degree(group) != 160)
        ABORT;
    fprintf(stdout, " ok\n");

    group_order_tests(group);

    if (!(P_160 = EC_GROUP_new(EC_GROUP_method_of(group))))
        ABORT;
    if (!EC_GROUP_copy(P_160, group))
        ABORT;

    /* Curve P-192 (FIPS PUB 186-2, App. 6) */

    if (!BN_hex2bn(&p, "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFFFFFFFFFFFF"))
        ABORT;
    if (1 != BN_is_prime_ex(p, BN_prime_checks, ctx, NULL))
        ABORT;
    if (!BN_hex2bn(&a, "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFFFFFFFFFFFC"))
        ABORT;
    if (!BN_hex2bn(&b, "64210519E59C80E70FA7E9AB72243049FEB8DEECC146B9B1"))
        ABORT;
    if (!EC_GROUP_set_curve_GFp(group, p, a, b, ctx))
        ABORT;

    if (!BN_hex2bn(&x, "188DA80EB03090F67CBF20EB43A18800F4FF0AFD82FF1012"))
        ABORT;
    if (!EC_POINT_set_compressed_coordinates_GFp(group, P, x, 1, ctx))
        ABORT;
    if (EC_POINT_is_on_curve(group, P, ctx) <= 0)
        ABORT;
    if (!BN_hex2bn(&z, "FFFFFFFFFFFFFFFFFFFFFFFF99DEF836146BC9B1B4D22831"))
        ABORT;
    if (!EC_GROUP_set_generator(group, P, z, BN_value_one()))
        ABORT;

    if (!EC_POINT_get_affine_coordinates_GFp(group, P, x, y, ctx))
        ABORT;
    fprintf(stdout, "\nNIST curve P-192 -- Generator:\n     x = 0x");
    BN_print_fp(stdout, x);
    fprintf(stdout, "\n     y = 0x");
    BN_print_fp(stdout, y);
    fprintf(stdout, "\n");
    /* G_y value taken from the standard: */
    if (!BN_hex2bn(&z, "07192B95FFC8DA78631011ED6B24CDD573F977A11E794811"))
        ABORT;
    if (0 != BN_cmp(y, z))
        ABORT;

    fprintf(stdout, "verify degree ...");
    if (EC_GROUP_get_degree(group) != 192)
        ABORT;
    fprintf(stdout, " ok\n");

    group_order_tests(group);

    if (!(P_192 = EC_GROUP_new(EC_GROUP_method_of(group))))
        ABORT;
    if (!EC_GROUP_copy(P_192, group))
        ABORT;

    /* Curve P-224 (FIPS PUB 186-2, App. 6) */

    if (!BN_hex2bn
        (&p, "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF000000000000000000000001"))
        ABORT;
    if (1 != BN_is_prime_ex(p, BN_prime_checks, ctx, NULL))
        ABORT;
    if (!BN_hex2bn
        (&a, "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFFFFFFFFFFFFFFFFFFFE"))
        ABORT;
    if (!BN_hex2bn
        (&b, "B4050A850C04B3ABF54132565044B0B7D7BFD8BA270B39432355FFB4"))
        ABORT;
    if (!EC_GROUP_set_curve_GFp(group, p, a, b, ctx))
        ABORT;

    if (!BN_hex2bn
        (&x, "B70E0CBD6BB4BF7F321390B94A03C1D356C21122343280D6115C1D21"))
        ABORT;
    if (!EC_POINT_set_compressed_coordinates_GFp(group, P, x, 0, ctx))
        ABORT;
    if (EC_POINT_is_on_curve(group, P, ctx) <= 0)
        ABORT;
    if (!BN_hex2bn
        (&z, "FFFFFFFFFFFFFFFFFFFFFFFFFFFF16A2E0B8F03E13DD29455C5C2A3D"))
        ABORT;
    if (!EC_GROUP_set_generator(group, P, z, BN_value_one()))
        ABORT;

    if (!EC_POINT_get_affine_coordinates_GFp(group, P, x, y, ctx))
        ABORT;
    fprintf(stdout, "\nNIST curve P-224 -- Generator:\n     x = 0x");
    BN_print_fp(stdout, x);
    fprintf(stdout, "\n     y = 0x");
    BN_print_fp(stdout, y);
    fprintf(stdout, "\n");
    /* G_y value taken from the standard: */
    if (!BN_hex2bn
        (&z, "BD376388B5F723FB4C22DFE6CD4375A05A07476444D5819985007E34"))
        ABORT;
    if (0 != BN_cmp(y, z))
        ABORT;

    fprintf(stdout, "verify degree ...");
    if (EC_GROUP_get_degree(group) != 224)
        ABORT;
    fprintf(stdout, " ok\n");

    group_order_tests(group);

    if (!(P_224 = EC_GROUP_new(EC_GROUP_method_of(group))))
        ABORT;
    if (!EC_GROUP_copy(P_224, group))
        ABORT;

    /* Curve P-256 (FIPS PUB 186-2, App. 6) */

    if (!BN_hex2bn
        (&p,
         "FFFFFFFF00000001000000000000000000000000FFFFFFFFFFFFFFFFFFFFFFFF"))
        ABORT;
    if (1 != BN_is_prime_ex(p, BN_prime_checks, ctx, NULL))
        ABORT;
    if (!BN_hex2bn
        (&a,
         "FFFFFFFF00000001000000000000000000000000FFFFFFFFFFFFFFFFFFFFFFFC"))
        ABORT;
    if (!BN_hex2bn
        (&b,
         "5AC635D8AA3A93E7B3EBBD55769886BC651D06B0CC53B0F63BCE3C3E27D2604B"))
        ABORT;
    if (!EC_GROUP_set_curve_GFp(group, p, a, b, ctx))
        ABORT;

    if (!BN_hex2bn
        (&x,
         "6B17D1F2E12C4247F8BCE6E563A440F277037D812DEB33A0F4A13945D898C296"))
        ABORT;
    if (!EC_POINT_set_compressed_coordinates_GFp(group, P, x, 1, ctx))
        ABORT;
    if (EC_POINT_is_on_curve(group, P, ctx) <= 0)
        ABORT;
    if (!BN_hex2bn(&z, "FFFFFFFF00000000FFFFFFFFFFFFFFFFBCE6FAADA7179E"
                   "84F3B9CAC2FC632551"))
        ABORT;
    if (!EC_GROUP_set_generator(group, P, z, BN_value_one()))
        ABORT;

    if (!EC_POINT_get_affine_coordinates_GFp(group, P, x, y, ctx))
        ABORT;
    fprintf(stdout, "\nNIST curve P-256 -- Generator:\n     x = 0x");
    BN_print_fp(stdout, x);
    fprintf(stdout, "\n     y = 0x");
    BN_print_fp(stdout, y);
    fprintf(stdout, "\n");
    /* G_y value taken from the standard: */
    if (!BN_hex2bn
        (&z,
         "4FE342E2FE1A7F9B8EE7EB4A7C0F9E162BCE33576B315ECECBB6406837BF51F5"))
        ABORT;
    if (0 != BN_cmp(y, z))
        ABORT;

    fprintf(stdout, "verify degree ...");
    if (EC_GROUP_get_degree(group) != 256)
        ABORT;
    fprintf(stdout, " ok\n");

    group_order_tests(group);

    if (!(P_256 = EC_GROUP_new(EC_GROUP_method_of(group))))
        ABORT;
    if (!EC_GROUP_copy(P_256, group))
        ABORT;

    /* Curve P-384 (FIPS PUB 186-2, App. 6) */

    if (!BN_hex2bn(&p, "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                   "FFFFFFFFFFFFFFFFFEFFFFFFFF0000000000000000FFFFFFFF"))
        ABORT;
    if (1 != BN_is_prime_ex(p, BN_prime_checks, ctx, NULL))
        ABORT;
    if (!BN_hex2bn(&a, "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                   "FFFFFFFFFFFFFFFFFEFFFFFFFF0000000000000000FFFFFFFC"))
        ABORT;
    if (!BN_hex2bn(&b, "B3312FA7E23EE7E4988E056BE3F82D19181D9C6EFE8141"
                   "120314088F5013875AC656398D8A2ED19D2A85C8EDD3EC2AEF"))
        ABORT;
    if (!EC_GROUP_set_curve_GFp(group, p, a, b, ctx))
        ABORT;

    if (!BN_hex2bn(&x, "AA87CA22BE8B05378EB1C71EF320AD746E1D3B628BA79B"
                   "9859F741E082542A385502F25DBF55296C3A545E3872760AB7"))
        ABORT;
    if (!EC_POINT_set_compressed_coordinates_GFp(group, P, x, 1, ctx))
        ABORT;
    if (EC_POINT_is_on_curve(group, P, ctx) <= 0)
        ABORT;
    if (!BN_hex2bn(&z, "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                   "FFC7634D81F4372DDF581A0DB248B0A77AECEC196ACCC52973"))
        ABORT;
    if (!EC_GROUP_set_generator(group, P, z, BN_value_one()))
        ABORT;

    if (!EC_POINT_get_affine_coordinates_GFp(group, P, x, y, ctx))
        ABORT;
    fprintf(stdout, "\nNIST curve P-384 -- Generator:\n     x = 0x");
    BN_print_fp(stdout, x);
    fprintf(stdout, "\n     y = 0x");
    BN_print_fp(stdout, y);
    fprintf(stdout, "\n");
    /* G_y value taken from the standard: */
    if (!BN_hex2bn(&z, "3617DE4A96262C6F5D9E98BF9292DC29F8F41DBD289A14"
                   "7CE9DA3113B5F0B8C00A60B1CE1D7E819D7A431D7C90EA0E5F"))
        ABORT;
    if (0 != BN_cmp(y, z))
        ABORT;

    fprintf(stdout, "verify degree ...");
    if (EC_GROUP_get_degree(group) != 384)
        ABORT;
    fprintf(stdout, " ok\n");

    group_order_tests(group);

    if (!(P_384 = EC_GROUP_new(EC_GROUP_method_of(group))))
        ABORT;
    if (!EC_GROUP_copy(P_384, group))
        ABORT;

    /* Curve P-521 (FIPS PUB 186-2, App. 6) */

    if (!BN_hex2bn(&p, "1FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                   "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                   "FFFFFFFFFFFFFFFFFFFFFFFFFFFF"))
        ABORT;
    if (1 != BN_is_prime_ex(p, BN_prime_checks, ctx, NULL))
        ABORT;
    if (!BN_hex2bn(&a, "1FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                   "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                   "FFFFFFFFFFFFFFFFFFFFFFFFFFFC"))
        ABORT;
    if (!BN_hex2bn(&b, "051953EB9618E1C9A1F929A21A0B68540EEA2DA725B99B"
                   "315F3B8B489918EF109E156193951EC7E937B1652C0BD3BB1BF073573"
                   "DF883D2C34F1EF451FD46B503F00"))
        ABORT;
    if (!EC_GROUP_set_curve_GFp(group, p, a, b, ctx))
        ABORT;

    if (!BN_hex2bn(&x, "C6858E06B70404E9CD9E3ECB662395B4429C648139053F"
                   "B521F828AF606B4D3DBAA14B5E77EFE75928FE1DC127A2FFA8DE3348B"
                   "3C1856A429BF97E7E31C2E5BD66"))
        ABORT;
    if (!EC_POINT_set_compressed_coordinates_GFp(group, P, x, 0, ctx))
        ABORT;
    if (EC_POINT_is_on_curve(group, P, ctx) <= 0)
        ABORT;
    if (!BN_hex2bn(&z, "1FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                   "FFFFFFFFFFFFFFFFFFFFA51868783BF2F966B7FCC0148F709A5D03BB5"
                   "C9B8899C47AEBB6FB71E91386409"))
        ABORT;
    if (!EC_GROUP_set_generator(group, P, z, BN_value_one()))
        ABORT;

    if (!EC_POINT_get_affine_coordinates_GFp(group, P, x, y, ctx))
        ABORT;
    fprintf(stdout, "\nNIST curve P-521 -- Generator:\n     x = 0x");
    BN_print_fp(stdout, x);
    fprintf(stdout, "\n     y = 0x");
    BN_print_fp(stdout, y);
    fprintf(stdout, "\n");
    /* G_y value taken from the standard: */
    if (!BN_hex2bn(&z, "11839296A789A3BC0045C8A5FB42C7D1BD998F54449579"
                   "B446817AFBD17273E662C97EE72995EF42640C550B9013FAD0761353C"
                   "7086A272C24088BE94769FD16650"))
        ABORT;
    if (0 != BN_cmp(y, z))
        ABORT;

    fprintf(stdout, "verify degree ...");
    if (EC_GROUP_get_degree(group) != 521)
        ABORT;
    fprintf(stdout, " ok\n");

    group_order_tests(group);

    if (!(P_521 = EC_GROUP_new(EC_GROUP_method_of(group))))
        ABORT;
    if (!EC_GROUP_copy(P_521, group))
        ABORT;

    /* more tests using the last curve */

    if (!EC_POINT_copy(Q, P))
        ABORT;
    if (EC_POINT_is_at_infinity(group, Q))
        ABORT;
    if (!EC_POINT_dbl(group, P, P, ctx))
        ABORT;
    if (EC_POINT_is_on_curve(group, P, ctx) <= 0)
        ABORT;
    if (!EC_POINT_invert(group, Q, ctx))
        ABORT;                  /* P = -2Q */

    if (!EC_POINT_add(group, R, P, Q, ctx))
        ABORT;
    if (!EC_POINT_add(group, R, R, Q, ctx))
        ABORT;
    if (!EC_POINT_is_at_infinity(group, R))
        ABORT;                  /* R = P + 2Q */

    {
        const EC_POINT *points[4];
        const BIGNUM *scalars[4];
        BIGNUM scalar3;

        if (EC_POINT_is_at_infinity(group, Q))
            ABORT;
        points[0] = Q;
        points[1] = Q;
        points[2] = Q;
        points[3] = Q;

        if (!EC_GROUP_get_order(group, z, ctx))
            ABORT;
        if (!BN_add(y, z, BN_value_one()))
            ABORT;
        if (BN_is_odd(y))
            ABORT;
        if (!BN_rshift1(y, y))
            ABORT;
        scalars[0] = y;         /* (group order + 1)/2, so y*Q + y*Q = Q */
        scalars[1] = y;

        fprintf(stdout, "combined multiplication ...");
        fflush(stdout);

        /* z is still the group order */
        if (!EC_POINTs_mul(group, P, NULL, 2, points, scalars, ctx))
            ABORT;
        if (!EC_POINTs_mul(group, R, z, 2, points, scalars, ctx))
            ABORT;
        if (0 != EC_POINT_cmp(group, P, R, ctx))
            ABORT;
        if (0 != EC_POINT_cmp(group, R, Q, ctx))
            ABORT;

        fprintf(stdout, ".");
        fflush(stdout);

        if (!BN_pseudo_rand(y, BN_num_bits(y), 0, 0))
            ABORT;
        if (!BN_add(z, z, y))
            ABORT;
        BN_set_negative(z, 1);
        scalars[0] = y;
        scalars[1] = z;         /* z = -(order + y) */

        if (!EC_POINTs_mul(group, P, NULL, 2, points, scalars, ctx))
            ABORT;
        if (!EC_POINT_is_at_infinity(group, P))
            ABORT;

        fprintf(stdout, ".");
        fflush(stdout);

        if (!BN_pseudo_rand(x, BN_num_bits(y) - 1, 0, 0))
            ABORT;
        if (!BN_add(z, x, y))
            ABORT;
        BN_set_negative(z, 1);
        scalars[0] = x;
        scalars[1] = y;
        scalars[2] = z;         /* z = -(x+y) */

        BN_init(&scalar3);
        BN_zero(&scalar3);
        scalars[3] = &scalar3;

        if (!EC_POINTs_mul(group, P, NULL, 4, points, scalars, ctx))
            ABORT;
        if (!EC_POINT_is_at_infinity(group, P))
            ABORT;

        fprintf(stdout, " ok\n\n");

        BN_free(&scalar3);
    }

# if 0
    timings(P_160, TIMING_BASE_PT, ctx);
    timings(P_160, TIMING_RAND_PT, ctx);
    timings(P_160, TIMING_SIMUL, ctx);
    timings(P_192, TIMING_BASE_PT, ctx);
    timings(P_192, TIMING_RAND_PT, ctx);
    timings(P_192, TIMING_SIMUL, ctx);
    timings(P_224, TIMING_BASE_PT, ctx);
    timings(P_224, TIMING_RAND_PT, ctx);
    timings(P_224, TIMING_SIMUL, ctx);
    timings(P_256, TIMING_BASE_PT, ctx);
    timings(P_256, TIMING_RAND_PT, ctx);
    timings(P_256, TIMING_SIMUL, ctx);
    timings(P_384, TIMING_BASE_PT, ctx);
    timings(P_384, TIMING_RAND_PT, ctx);
    timings(P_384, TIMING_SIMUL, ctx);
    timings(P_521, TIMING_BASE_PT, ctx);
    timings(P_521, TIMING_RAND_PT, ctx);
    timings(P_521, TIMING_SIMUL, ctx);
# endif

    if (ctx)
        BN_CTX_free(ctx);
    BN_free(p);
    BN_free(a);
    BN_free(b);
    EC_GROUP_free(group);
    EC_POINT_free(P);
    EC_POINT_free(Q);
    EC_POINT_free(R);
    BN_free(x);
    BN_free(y);
    BN_free(z);

    if (P_160)
        EC_GROUP_free(P_160);
    if (P_192)
        EC_GROUP_free(P_192);
    if (P_224)
        EC_GROUP_free(P_224);
    if (P_256)
        EC_GROUP_free(P_256);
    if (P_384)
        EC_GROUP_free(P_384);
    if (P_521)
        EC_GROUP_free(P_521);

}

/* Change test based on whether binary point compression is enabled or not. */
# ifdef OPENSSL_EC_BIN_PT_COMP
#  define CHAR2_CURVE_TEST_INTERNAL(_name, _p, _a, _b, _x, _y, _y_bit, _order, _cof, _degree, _variable) \
        if (!BN_hex2bn(&x, _x)) ABORT; \
        if (!EC_POINT_set_compressed_coordinates_GF2m(group, P, x, _y_bit, ctx)) ABORT; \
        if (EC_POINT_is_on_curve(group, P, ctx) <= 0) ABORT; \
        if (!BN_hex2bn(&z, _order)) ABORT; \
        if (!BN_hex2bn(&cof, _cof)) ABORT; \
        if (!EC_GROUP_set_generator(group, P, z, cof)) ABORT; \
        if (!EC_POINT_get_affine_coordinates_GF2m(group, P, x, y, ctx)) ABORT; \
        fprintf(stdout, "\n%s -- Generator:\n     x = 0x", _name); \
        BN_print_fp(stdout, x); \
        fprintf(stdout, "\n     y = 0x"); \
        BN_print_fp(stdout, y); \
        fprintf(stdout, "\n"); \
        /* G_y value taken from the standard: */ \
        if (!BN_hex2bn(&z, _y)) ABORT; \
        if (0 != BN_cmp(y, z)) ABORT;
# else
#  define CHAR2_CURVE_TEST_INTERNAL(_name, _p, _a, _b, _x, _y, _y_bit, _order, _cof, _degree, _variable) \
        if (!BN_hex2bn(&x, _x)) ABORT; \
        if (!BN_hex2bn(&y, _y)) ABORT; \
        if (!EC_POINT_set_affine_coordinates_GF2m(group, P, x, y, ctx)) ABORT; \
        if (EC_POINT_is_on_curve(group, P, ctx) <= 0) ABORT; \
        if (!BN_hex2bn(&z, _order)) ABORT; \
        if (!BN_hex2bn(&cof, _cof)) ABORT; \
        if (!EC_GROUP_set_generator(group, P, z, cof)) ABORT; \
        fprintf(stdout, "\n%s -- Generator:\n     x = 0x", _name); \
        BN_print_fp(stdout, x); \
        fprintf(stdout, "\n     y = 0x"); \
        BN_print_fp(stdout, y); \
        fprintf(stdout, "\n");
# endif

# define CHAR2_CURVE_TEST(_name, _p, _a, _b, _x, _y, _y_bit, _order, _cof, _degree, _variable) \
        if (!BN_hex2bn(&p, _p)) ABORT; \
        if (!BN_hex2bn(&a, _a)) ABORT; \
        if (!BN_hex2bn(&b, _b)) ABORT; \
        if (!EC_GROUP_set_curve_GF2m(group, p, a, b, ctx)) ABORT; \
        CHAR2_CURVE_TEST_INTERNAL(_name, _p, _a, _b, _x, _y, _y_bit, _order, _cof, _degree, _variable) \
        fprintf(stdout, "verify degree ..."); \
        if (EC_GROUP_get_degree(group) != _degree) ABORT; \
        fprintf(stdout, " ok\n"); \
        group_order_tests(group); \
        if (!(_variable = EC_GROUP_new(EC_GROUP_method_of(group)))) ABORT; \
        if (!EC_GROUP_copy(_variable, group)) ABORT; \

# ifndef OPENSSL_NO_EC2M

static void char2_field_tests(void)
{
    BN_CTX *ctx = NULL;
    BIGNUM *p, *a, *b;
    EC_GROUP *group;
    EC_GROUP *C2_K163 = NULL, *C2_K233 = NULL, *C2_K283 = NULL, *C2_K409 =
        NULL, *C2_K571 = NULL;
    EC_GROUP *C2_B163 = NULL, *C2_B233 = NULL, *C2_B283 = NULL, *C2_B409 =
        NULL, *C2_B571 = NULL;
    EC_POINT *P, *Q, *R;
    BIGNUM *x, *y, *z, *cof;
    unsigned char buf[100];
    size_t i, len;
    int k;

#  if 1                         /* optional */
    ctx = BN_CTX_new();
    if (!ctx)
        ABORT;
#  endif

    p = BN_new();
    a = BN_new();
    b = BN_new();
    if (!p || !a || !b)
        ABORT;

    if (!BN_hex2bn(&p, "13"))
        ABORT;
    if (!BN_hex2bn(&a, "3"))
        ABORT;
    if (!BN_hex2bn(&b, "1"))
        ABORT;

    group = EC_GROUP_new(EC_GF2m_simple_method()); /* applications should use
                                                    * EC_GROUP_new_curve_GF2m
                                                    * so that the library gets
                                                    * to choose the EC_METHOD */
    if (!group)
        ABORT;
    if (!EC_GROUP_set_curve_GF2m(group, p, a, b, ctx))
        ABORT;

    {
        EC_GROUP *tmp;
        tmp = EC_GROUP_new(EC_GROUP_method_of(group));
        if (!tmp)
            ABORT;
        if (!EC_GROUP_copy(tmp, group))
            ABORT;
        EC_GROUP_free(group);
        group = tmp;
    }

    if (!EC_GROUP_get_curve_GF2m(group, p, a, b, ctx))
        ABORT;

    fprintf(stdout,
            "Curve defined by Weierstrass equation\n     y^2 + x*y = x^3 + a*x^2 + b  (mod 0x");
    BN_print_fp(stdout, p);
    fprintf(stdout, ")\n     a = 0x");
    BN_print_fp(stdout, a);
    fprintf(stdout, "\n     b = 0x");
    BN_print_fp(stdout, b);
    fprintf(stdout, "\n(0x... means binary polynomial)\n");

    P = EC_POINT_new(group);
    Q = EC_POINT_new(group);
    R = EC_POINT_new(group);
    if (!P || !Q || !R)
        ABORT;

    if (!EC_POINT_set_to_infinity(group, P))
        ABORT;
    if (!EC_POINT_is_at_infinity(group, P))
        ABORT;

    buf[0] = 0;
    if (!EC_POINT_oct2point(group, Q, buf, 1, ctx))
        ABORT;

    if (!EC_POINT_add(group, P, P, Q, ctx))
        ABORT;
    if (!EC_POINT_is_at_infinity(group, P))
        ABORT;

    x = BN_new();
    y = BN_new();
    z = BN_new();
    cof = BN_new();
    if (!x || !y || !z || !cof)
        ABORT;

    if (!BN_hex2bn(&x, "6"))
        ABORT;
/* Change test based on whether binary point compression is enabled or not. */
#  ifdef OPENSSL_EC_BIN_PT_COMP
    if (!EC_POINT_set_compressed_coordinates_GF2m(group, Q, x, 1, ctx))
        ABORT;
#  else
    if (!BN_hex2bn(&y, "8"))
        ABORT;
    if (!EC_POINT_set_affine_coordinates_GF2m(group, Q, x, y, ctx))
        ABORT;
#  endif
    if (EC_POINT_is_on_curve(group, Q, ctx) <= 0) {
/* Change test based on whether binary point compression is enabled or not. */
#  ifdef OPENSSL_EC_BIN_PT_COMP
        if (!EC_POINT_get_affine_coordinates_GF2m(group, Q, x, y, ctx))
            ABORT;
#  endif
        fprintf(stderr, "Point is not on curve: x = 0x");
        BN_print_fp(stderr, x);
        fprintf(stderr, ", y = 0x");
        BN_print_fp(stderr, y);
        fprintf(stderr, "\n");
        ABORT;
    }

    fprintf(stdout, "A cyclic subgroup:\n");
    k = 100;
    do {
        if (k-- == 0)
            ABORT;

        if (EC_POINT_is_at_infinity(group, P))
            fprintf(stdout, "     point at infinity\n");
        else {
            if (!EC_POINT_get_affine_coordinates_GF2m(group, P, x, y, ctx))
                ABORT;

            fprintf(stdout, "     x = 0x");
            BN_print_fp(stdout, x);
            fprintf(stdout, ", y = 0x");
            BN_print_fp(stdout, y);
            fprintf(stdout, "\n");
        }

        if (!EC_POINT_copy(R, P))
            ABORT;
        if (!EC_POINT_add(group, P, P, Q, ctx))
            ABORT;
    }
    while (!EC_POINT_is_at_infinity(group, P));

    if (!EC_POINT_add(group, P, Q, R, ctx))
        ABORT;
    if (!EC_POINT_is_at_infinity(group, P))
        ABORT;

/* Change test based on whether binary point compression is enabled or not. */
#  ifdef OPENSSL_EC_BIN_PT_COMP
    len =
        EC_POINT_point2oct(group, Q, POINT_CONVERSION_COMPRESSED, buf,
                           sizeof buf, ctx);
    if (len == 0)
        ABORT;
    if (!EC_POINT_oct2point(group, P, buf, len, ctx))
        ABORT;
    if (0 != EC_POINT_cmp(group, P, Q, ctx))
        ABORT;
    fprintf(stdout, "Generator as octet string, compressed form:\n     ");
    for (i = 0; i < len; i++)
        fprintf(stdout, "%02X", buf[i]);
#  endif

    len =
        EC_POINT_point2oct(group, Q, POINT_CONVERSION_UNCOMPRESSED, buf,
                           sizeof buf, ctx);
    if (len == 0)
        ABORT;
    if (!EC_POINT_oct2point(group, P, buf, len, ctx))
        ABORT;
    if (0 != EC_POINT_cmp(group, P, Q, ctx))
        ABORT;
    fprintf(stdout, "\nGenerator as octet string, uncompressed form:\n     ");
    for (i = 0; i < len; i++)
        fprintf(stdout, "%02X", buf[i]);

/* Change test based on whether binary point compression is enabled or not. */
#  ifdef OPENSSL_EC_BIN_PT_COMP
    len =
        EC_POINT_point2oct(group, Q, POINT_CONVERSION_HYBRID, buf, sizeof buf,
                           ctx);
    if (len == 0)
        ABORT;
    if (!EC_POINT_oct2point(group, P, buf, len, ctx))
        ABORT;
    if (0 != EC_POINT_cmp(group, P, Q, ctx))
        ABORT;
    fprintf(stdout, "\nGenerator as octet string, hybrid form:\n     ");
    for (i = 0; i < len; i++)
        fprintf(stdout, "%02X", buf[i]);
#  endif

    fprintf(stdout, "\n");

    if (!EC_POINT_invert(group, P, ctx))
        ABORT;
    if (0 != EC_POINT_cmp(group, P, R, ctx))
        ABORT;

    /* Curve K-163 (FIPS PUB 186-2, App. 6) */
    CHAR2_CURVE_TEST
        ("NIST curve K-163",
         "0800000000000000000000000000000000000000C9",
         "1",
         "1",
         "02FE13C0537BBC11ACAA07D793DE4E6D5E5C94EEE8",
         "0289070FB05D38FF58321F2E800536D538CCDAA3D9",
         1, "04000000000000000000020108A2E0CC0D99F8A5EF", "2", 163, C2_K163);

    /* Curve B-163 (FIPS PUB 186-2, App. 6) */
    CHAR2_CURVE_TEST
        ("NIST curve B-163",
         "0800000000000000000000000000000000000000C9",
         "1",
         "020A601907B8C953CA1481EB10512F78744A3205FD",
         "03F0EBA16286A2D57EA0991168D4994637E8343E36",
         "00D51FBC6C71A0094FA2CDD545B11C5C0C797324F1",
         1, "040000000000000000000292FE77E70C12A4234C33", "2", 163, C2_B163);

    /* Curve K-233 (FIPS PUB 186-2, App. 6) */
    CHAR2_CURVE_TEST
        ("NIST curve K-233",
         "020000000000000000000000000000000000000004000000000000000001",
         "0",
         "1",
         "017232BA853A7E731AF129F22FF4149563A419C26BF50A4C9D6EEFAD6126",
         "01DB537DECE819B7F70F555A67C427A8CD9BF18AEB9B56E0C11056FAE6A3",
         0,
         "008000000000000000000000000000069D5BB915BCD46EFB1AD5F173ABDF",
         "4", 233, C2_K233);

    /* Curve B-233 (FIPS PUB 186-2, App. 6) */
    CHAR2_CURVE_TEST
        ("NIST curve B-233",
         "020000000000000000000000000000000000000004000000000000000001",
         "000000000000000000000000000000000000000000000000000000000001",
         "0066647EDE6C332C7F8C0923BB58213B333B20E9CE4281FE115F7D8F90AD",
         "00FAC9DFCBAC8313BB2139F1BB755FEF65BC391F8B36F8F8EB7371FD558B",
         "01006A08A41903350678E58528BEBF8A0BEFF867A7CA36716F7E01F81052",
         1,
         "01000000000000000000000000000013E974E72F8A6922031D2603CFE0D7",
         "2", 233, C2_B233);

    /* Curve K-283 (FIPS PUB 186-2, App. 6) */
    CHAR2_CURVE_TEST
        ("NIST curve K-283",
         "0800000000000000000000000000000000000000000000000000000000000000000010A1",
         "0",
         "1",
         "0503213F78CA44883F1A3B8162F188E553CD265F23C1567A16876913B0C2AC2458492836",
         "01CCDA380F1C9E318D90F95D07E5426FE87E45C0E8184698E45962364E34116177DD2259",
         0,
         "01FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFE9AE2ED07577265DFF7F94451E061E163C61",
         "4", 283, C2_K283);

    /* Curve B-283 (FIPS PUB 186-2, App. 6) */
    CHAR2_CURVE_TEST
        ("NIST curve B-283",
         "0800000000000000000000000000000000000000000000000000000000000000000010A1",
         "000000000000000000000000000000000000000000000000000000000000000000000001",
         "027B680AC8B8596DA5A4AF8A19A0303FCA97FD7645309FA2A581485AF6263E313B79A2F5",
         "05F939258DB7DD90E1934F8C70B0DFEC2EED25B8557EAC9C80E2E198F8CDBECD86B12053",
         "03676854FE24141CB98FE6D4B20D02B4516FF702350EDDB0826779C813F0DF45BE8112F4",
         1,
         "03FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEF90399660FC938A90165B042A7CEFADB307",
         "2", 283, C2_B283);

    /* Curve K-409 (FIPS PUB 186-2, App. 6) */
    CHAR2_CURVE_TEST
        ("NIST curve K-409",
         "02000000000000000000000000000000000000000000000000000000000000000000000000000000008000000000000000000001",
         "0",
         "1",
         "0060F05F658F49C1AD3AB1890F7184210EFD0987E307C84C27ACCFB8F9F67CC2C460189EB5AAAA62EE222EB1B35540CFE9023746",
         "01E369050B7C4E42ACBA1DACBF04299C3460782F918EA427E6325165E9EA10E3DA5F6C42E9C55215AA9CA27A5863EC48D8E0286B",
         1,
         "007FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFE5F83B2D4EA20400EC4557D5ED3E3E7CA5B4B5C83B8E01E5FCF",
         "4", 409, C2_K409);

    /* Curve B-409 (FIPS PUB 186-2, App. 6) */
    CHAR2_CURVE_TEST
        ("NIST curve B-409",
         "02000000000000000000000000000000000000000000000000000000000000000000000000000000008000000000000000000001",
         "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001",
         "0021A5C2C8EE9FEB5C4B9A753B7B476B7FD6422EF1F3DD674761FA99D6AC27C8A9A197B272822F6CD57A55AA4F50AE317B13545F",
         "015D4860D088DDB3496B0C6064756260441CDE4AF1771D4DB01FFE5B34E59703DC255A868A1180515603AEAB60794E54BB7996A7",
         "0061B1CFAB6BE5F32BBFA78324ED106A7636B9C5A7BD198D0158AA4F5488D08F38514F1FDF4B4F40D2181B3681C364BA0273C706",
         1,
         "010000000000000000000000000000000000000000000000000001E2AAD6A612F33307BE5FA47C3C9E052F838164CD37D9A21173",
         "2", 409, C2_B409);

    /* Curve K-571 (FIPS PUB 186-2, App. 6) */
    CHAR2_CURVE_TEST
        ("NIST curve K-571",
         "80000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000425",
         "0",
         "1",
         "026EB7A859923FBC82189631F8103FE4AC9CA2970012D5D46024804801841CA44370958493B205E647DA304DB4CEB08CBBD1BA39494776FB988B47174DCA88C7E2945283A01C8972",
         "0349DC807F4FBF374F4AEADE3BCA95314DD58CEC9F307A54FFC61EFC006D8A2C9D4979C0AC44AEA74FBEBBB9F772AEDCB620B01A7BA7AF1B320430C8591984F601CD4C143EF1C7A3",
         0,
         "020000000000000000000000000000000000000000000000000000000000000000000000131850E1F19A63E4B391A8DB917F4138B630D84BE5D639381E91DEB45CFE778F637C1001",
         "4", 571, C2_K571);

    /* Curve B-571 (FIPS PUB 186-2, App. 6) */
    CHAR2_CURVE_TEST
        ("NIST curve B-571",
         "80000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000425",
         "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001",
         "02F40E7E2221F295DE297117B7F3D62F5C6A97FFCB8CEFF1CD6BA8CE4A9A18AD84FFABBD8EFA59332BE7AD6756A66E294AFD185A78FF12AA520E4DE739BACA0C7FFEFF7F2955727A",
         "0303001D34B856296C16C0D40D3CD7750A93D1D2955FA80AA5F40FC8DB7B2ABDBDE53950F4C0D293CDD711A35B67FB1499AE60038614F1394ABFA3B4C850D927E1E7769C8EEC2D19",
         "037BF27342DA639B6DCCFFFEB73D69D78C6C27A6009CBBCA1980F8533921E8A684423E43BAB08A576291AF8F461BB2A8B3531D2F0485C19B16E2F1516E23DD3C1A4827AF1B8AC15B",
         1,
         "03FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFE661CE18FF55987308059B186823851EC7DD9CA1161DE93D5174D66E8382E9BB2FE84E47",
         "2", 571, C2_B571);

    /* more tests using the last curve */

    if (!EC_POINT_copy(Q, P))
        ABORT;
    if (EC_POINT_is_at_infinity(group, Q))
        ABORT;
    if (!EC_POINT_dbl(group, P, P, ctx))
        ABORT;
    if (EC_POINT_is_on_curve(group, P, ctx) <= 0)
        ABORT;
    if (!EC_POINT_invert(group, Q, ctx))
        ABORT;                  /* P = -2Q */

    if (!EC_POINT_add(group, R, P, Q, ctx))
        ABORT;
    if (!EC_POINT_add(group, R, R, Q, ctx))
        ABORT;
    if (!EC_POINT_is_at_infinity(group, R))
        ABORT;                  /* R = P + 2Q */

    {
        const EC_POINT *points[3];
        const BIGNUM *scalars[3];

        if (EC_POINT_is_at_infinity(group, Q))
            ABORT;
        points[0] = Q;
        points[1] = Q;
        points[2] = Q;

        if (!BN_add(y, z, BN_value_one()))
            ABORT;
        if (BN_is_odd(y))
            ABORT;
        if (!BN_rshift1(y, y))
            ABORT;
        scalars[0] = y;         /* (group order + 1)/2, so y*Q + y*Q = Q */
        scalars[1] = y;

        fprintf(stdout, "combined multiplication ...");
        fflush(stdout);

        /* z is still the group order */
        if (!EC_POINTs_mul(group, P, NULL, 2, points, scalars, ctx))
            ABORT;
        if (!EC_POINTs_mul(group, R, z, 2, points, scalars, ctx))
            ABORT;
        if (0 != EC_POINT_cmp(group, P, R, ctx))
            ABORT;
        if (0 != EC_POINT_cmp(group, R, Q, ctx))
            ABORT;

        fprintf(stdout, ".");
        fflush(stdout);

        if (!BN_pseudo_rand(y, BN_num_bits(y), 0, 0))
            ABORT;
        if (!BN_add(z, z, y))
            ABORT;
        BN_set_negative(z, 1);
        scalars[0] = y;
        scalars[1] = z;         /* z = -(order + y) */

        if (!EC_POINTs_mul(group, P, NULL, 2, points, scalars, ctx))
            ABORT;
        if (!EC_POINT_is_at_infinity(group, P))
            ABORT;

        fprintf(stdout, ".");
        fflush(stdout);

        if (!BN_pseudo_rand(x, BN_num_bits(y) - 1, 0, 0))
            ABORT;
        if (!BN_add(z, x, y))
            ABORT;
        BN_set_negative(z, 1);
        scalars[0] = x;
        scalars[1] = y;
        scalars[2] = z;         /* z = -(x+y) */

        if (!EC_POINTs_mul(group, P, NULL, 3, points, scalars, ctx))
            ABORT;
        if (!EC_POINT_is_at_infinity(group, P))
            ABORT;

        fprintf(stdout, " ok\n\n");
    }

#  if 0
    timings(C2_K163, TIMING_BASE_PT, ctx);
    timings(C2_K163, TIMING_RAND_PT, ctx);
    timings(C2_K163, TIMING_SIMUL, ctx);
    timings(C2_B163, TIMING_BASE_PT, ctx);
    timings(C2_B163, TIMING_RAND_PT, ctx);
    timings(C2_B163, TIMING_SIMUL, ctx);
    timings(C2_K233, TIMING_BASE_PT, ctx);
    timings(C2_K233, TIMING_RAND_PT, ctx);
    timings(C2_K233, TIMING_SIMUL, ctx);
    timings(C2_B233, TIMING_BASE_PT, ctx);
    timings(C2_B233, TIMING_RAND_PT, ctx);
    timings(C2_B233, TIMING_SIMUL, ctx);
    timings(C2_K283, TIMING_BASE_PT, ctx);
    timings(C2_K283, TIMING_RAND_PT, ctx);
    timings(C2_K283, TIMING_SIMUL, ctx);
    timings(C2_B283, TIMING_BASE_PT, ctx);
    timings(C2_B283, TIMING_RAND_PT, ctx);
    timings(C2_B283, TIMING_SIMUL, ctx);
    timings(C2_K409, TIMING_BASE_PT, ctx);
    timings(C2_K409, TIMING_RAND_PT, ctx);
    timings(C2_K409, TIMING_SIMUL, ctx);
    timings(C2_B409, TIMING_BASE_PT, ctx);
    timings(C2_B409, TIMING_RAND_PT, ctx);
    timings(C2_B409, TIMING_SIMUL, ctx);
    timings(C2_K571, TIMING_BASE_PT, ctx);
    timings(C2_K571, TIMING_RAND_PT, ctx);
    timings(C2_K571, TIMING_SIMUL, ctx);
    timings(C2_B571, TIMING_BASE_PT, ctx);
    timings(C2_B571, TIMING_RAND_PT, ctx);
    timings(C2_B571, TIMING_SIMUL, ctx);
#  endif

    if (ctx)
        BN_CTX_free(ctx);
    BN_free(p);
    BN_free(a);
    BN_free(b);
    EC_GROUP_free(group);
    EC_POINT_free(P);
    EC_POINT_free(Q);
    EC_POINT_free(R);
    BN_free(x);
    BN_free(y);
    BN_free(z);
    BN_free(cof);

    if (C2_K163)
        EC_GROUP_free(C2_K163);
    if (C2_B163)
        EC_GROUP_free(C2_B163);
    if (C2_K233)
        EC_GROUP_free(C2_K233);
    if (C2_B233)
        EC_GROUP_free(C2_B233);
    if (C2_K283)
        EC_GROUP_free(C2_K283);
    if (C2_B283)
        EC_GROUP_free(C2_B283);
    if (C2_K409)
        EC_GROUP_free(C2_K409);
    if (C2_B409)
        EC_GROUP_free(C2_B409);
    if (C2_K571)
        EC_GROUP_free(C2_K571);
    if (C2_B571)
        EC_GROUP_free(C2_B571);

}
# endif

static void internal_curve_test(void)
{
    EC_builtin_curve *curves = NULL;
    size_t crv_len = 0, n = 0;
    int ok = 1;

    crv_len = EC_get_builtin_curves(NULL, 0);

    curves = OPENSSL_malloc(sizeof(EC_builtin_curve) * crv_len);

    if (curves == NULL)
        return;

    if (!EC_get_builtin_curves(curves, crv_len)) {
        OPENSSL_free(curves);
        return;
    }

    fprintf(stdout, "testing internal curves: ");

    for (n = 0; n < crv_len; n++) {
        EC_GROUP *group = NULL;
        int nid = curves[n].nid;
        if ((group = EC_GROUP_new_by_curve_name(nid)) == NULL) {
            ok = 0;
            fprintf(stdout, "\nEC_GROUP_new_curve_name() failed with"
                    " curve %s\n", OBJ_nid2sn(nid));
            /* try next curve */
            continue;
        }
        if (!EC_GROUP_check(group, NULL)) {
            ok = 0;
            fprintf(stdout, "\nEC_GROUP_check() failed with"
                    " curve %s\n", OBJ_nid2sn(nid));
            EC_GROUP_free(group);
            /* try the next curve */
            continue;
        }
        fprintf(stdout, ".");
        fflush(stdout);
        EC_GROUP_free(group);
    }
    if (ok)
        fprintf(stdout, " ok\n\n");
    else {
        fprintf(stdout, " failed\n\n");
        ABORT;
    }
    OPENSSL_free(curves);
    return;
}

# ifndef OPENSSL_NO_EC_NISTP_64_GCC_128
/*
 * nistp_test_params contains magic numbers for testing our optimized
 * implementations of several NIST curves with characteristic > 3.
 */
struct nistp_test_params {
    const EC_METHOD *(*meth) ();
    int degree;
    /*
     * Qx, Qy and D are taken from
     * http://csrc.nist.gov/groups/ST/toolkit/documents/Examples/ECDSA_Prime.pdf
     * Otherwise, values are standard curve parameters from FIPS 180-3
     */
    const char *p, *a, *b, *Qx, *Qy, *Gx, *Gy, *order, *d;
};

static const struct nistp_test_params nistp_tests_params[] = {
    {
     /* P-224 */
     EC_GFp_nistp224_method,
     224,
     /* p */
     "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF000000000000000000000001",
     /* a */
     "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFFFFFFFFFFFFFFFFFFFE",
     /* b */
     "B4050A850C04B3ABF54132565044B0B7D7BFD8BA270B39432355FFB4",
     /* Qx */
     "E84FB0B8E7000CB657D7973CF6B42ED78B301674276DF744AF130B3E",
     /* Qy */
     "4376675C6FC5612C21A0FF2D2A89D2987DF7A2BC52183B5982298555",
     /* Gx */
     "B70E0CBD6BB4BF7F321390B94A03C1D356C21122343280D6115C1D21",
     /* Gy */
     "BD376388B5F723FB4C22DFE6CD4375A05A07476444D5819985007E34",
     /* order */
     "FFFFFFFFFFFFFFFFFFFFFFFFFFFF16A2E0B8F03E13DD29455C5C2A3D",
     /* d */
     "3F0C488E987C80BE0FEE521F8D90BE6034EC69AE11CA72AA777481E8",
     },
    {
     /* P-256 */
     EC_GFp_nistp256_method,
     256,
     /* p */
     "ffffffff00000001000000000000000000000000ffffffffffffffffffffffff",
     /* a */
     "ffffffff00000001000000000000000000000000fffffffffffffffffffffffc",
     /* b */
     "5ac635d8aa3a93e7b3ebbd55769886bc651d06b0cc53b0f63bce3c3e27d2604b",
     /* Qx */
     "b7e08afdfe94bad3f1dc8c734798ba1c62b3a0ad1e9ea2a38201cd0889bc7a19",
     /* Qy */
     "3603f747959dbf7a4bb226e41928729063adc7ae43529e61b563bbc606cc5e09",
     /* Gx */
     "6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296",
     /* Gy */
     "4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5",
     /* order */
     "ffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551",
     /* d */
     "c477f9f65c22cce20657faa5b2d1d8122336f851a508a1ed04e479c34985bf96",
     },
    {
     /* P-521 */
     EC_GFp_nistp521_method,
     521,
     /* p */
     "1ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
     /* a */
     "1fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffc",
     /* b */
     "051953eb9618e1c9a1f929a21a0b68540eea2da725b99b315f3b8b489918ef109e156193951ec7e937b1652c0bd3bb1bf073573df883d2c34f1ef451fd46b503f00",
     /* Qx */
     "0098e91eef9a68452822309c52fab453f5f117c1da8ed796b255e9ab8f6410cca16e59df403a6bdc6ca467a37056b1e54b3005d8ac030decfeb68df18b171885d5c4",
     /* Qy */
     "0164350c321aecfc1cca1ba4364c9b15656150b4b78d6a48d7d28e7f31985ef17be8554376b72900712c4b83ad668327231526e313f5f092999a4632fd50d946bc2e",
     /* Gx */
     "c6858e06b70404e9cd9e3ecb662395b4429c648139053fb521f828af606b4d3dbaa14b5e77efe75928fe1dc127a2ffa8de3348b3c1856a429bf97e7e31c2e5bd66",
     /* Gy */
     "11839296a789a3bc0045c8a5fb42c7d1bd998f54449579b446817afbd17273e662c97ee72995ef42640c550b9013fad0761353c7086a272c24088be94769fd16650",
     /* order */
     "1fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffa51868783bf2f966b7fcc0148f709a5d03bb5c9b8899c47aebb6fb71e91386409",
     /* d */
     "0100085f47b8e1b8b11b7eb33028c0b2888e304bfc98501955b45bba1478dc184eeedf09b86a5f7c21994406072787205e69a63709fe35aa93ba333514b24f961722",
     },
};

static void nistp_single_test(const struct nistp_test_params *test)
{
    BN_CTX *ctx;
    BIGNUM *p, *a, *b, *x, *y, *n, *m, *order;
    EC_GROUP *NISTP;
    EC_POINT *G, *P, *Q, *Q_CHECK;

    fprintf(stdout, "\nNIST curve P-%d (optimised implementation):\n",
            test->degree);
    ctx = BN_CTX_new();
    p = BN_new();
    a = BN_new();
    b = BN_new();
    x = BN_new();
    y = BN_new();
    m = BN_new();
    n = BN_new();
    order = BN_new();

    NISTP = EC_GROUP_new(test->meth());
    if (!NISTP)
        ABORT;
    if (!BN_hex2bn(&p, test->p))
        ABORT;
    if (1 != BN_is_prime_ex(p, BN_prime_checks, ctx, NULL))
        ABORT;
    if (!BN_hex2bn(&a, test->a))
        ABORT;
    if (!BN_hex2bn(&b, test->b))
        ABORT;
    if (!EC_GROUP_set_curve_GFp(NISTP, p, a, b, ctx))
        ABORT;
    G = EC_POINT_new(NISTP);
    P = EC_POINT_new(NISTP);
    Q = EC_POINT_new(NISTP);
    Q_CHECK = EC_POINT_new(NISTP);
    if (!BN_hex2bn(&x, test->Qx))
        ABORT;
    if (!BN_hex2bn(&y, test->Qy))
        ABORT;
    if (!EC_POINT_set_affine_coordinates_GFp(NISTP, Q_CHECK, x, y, ctx))
        ABORT;
    if (!BN_hex2bn(&x, test->Gx))
        ABORT;
    if (!BN_hex2bn(&y, test->Gy))
        ABORT;
    if (!EC_POINT_set_affine_coordinates_GFp(NISTP, G, x, y, ctx))
        ABORT;
    if (!BN_hex2bn(&order, test->order))
        ABORT;
    if (!EC_GROUP_set_generator(NISTP, G, order, BN_value_one()))
        ABORT;

    fprintf(stdout, "verify degree ... ");
    if (EC_GROUP_get_degree(NISTP) != test->degree)
        ABORT;
    fprintf(stdout, "ok\n");

    fprintf(stdout, "NIST test vectors ... ");
    if (!BN_hex2bn(&n, test->d))
        ABORT;
    /* fixed point multiplication */
    EC_POINT_mul(NISTP, Q, n, NULL, NULL, ctx);
    if (0 != EC_POINT_cmp(NISTP, Q, Q_CHECK, ctx))
        ABORT;
    /* random point multiplication */
    EC_POINT_mul(NISTP, Q, NULL, G, n, ctx);
    if (0 != EC_POINT_cmp(NISTP, Q, Q_CHECK, ctx))
        ABORT;

    /* set generator to P = 2*G, where G is the standard generator */
    if (!EC_POINT_dbl(NISTP, P, G, ctx))
        ABORT;
    if (!EC_GROUP_set_generator(NISTP, P, order, BN_value_one()))
        ABORT;
    /* set the scalar to m=n/2, where n is the NIST test scalar */
    if (!BN_rshift(m, n, 1))
        ABORT;

    /* test the non-standard generator */
    /* fixed point multiplication */
    EC_POINT_mul(NISTP, Q, m, NULL, NULL, ctx);
    if (0 != EC_POINT_cmp(NISTP, Q, Q_CHECK, ctx))
        ABORT;
    /* random point multiplication */
    EC_POINT_mul(NISTP, Q, NULL, P, m, ctx);
    if (0 != EC_POINT_cmp(NISTP, Q, Q_CHECK, ctx))
        ABORT;

    /*
     * We have not performed precomputation so have_precompute mult should be
     * false
     */
    if (EC_GROUP_have_precompute_mult(NISTP))
        ABORT;

    /* now repeat all tests with precomputation */
    if (!EC_GROUP_precompute_mult(NISTP, ctx))
        ABORT;
    if (!EC_GROUP_have_precompute_mult(NISTP))
        ABORT;

    /* fixed point multiplication */
    EC_POINT_mul(NISTP, Q, m, NULL, NULL, ctx);
    if (0 != EC_POINT_cmp(NISTP, Q, Q_CHECK, ctx))
        ABORT;
    /* random point multiplication */
    EC_POINT_mul(NISTP, Q, NULL, P, m, ctx);
    if (0 != EC_POINT_cmp(NISTP, Q, Q_CHECK, ctx))
        ABORT;

    /* reset generator */
    if (!EC_GROUP_set_generator(NISTP, G, order, BN_value_one()))
        ABORT;
    /* fixed point multiplication */
    EC_POINT_mul(NISTP, Q, n, NULL, NULL, ctx);
    if (0 != EC_POINT_cmp(NISTP, Q, Q_CHECK, ctx))
        ABORT;
    /* random point multiplication */
    EC_POINT_mul(NISTP, Q, NULL, G, n, ctx);
    if (0 != EC_POINT_cmp(NISTP, Q, Q_CHECK, ctx))
        ABORT;

    fprintf(stdout, "ok\n");
    group_order_tests(NISTP);
#  if 0
    timings(NISTP, TIMING_BASE_PT, ctx);
    timings(NISTP, TIMING_RAND_PT, ctx);
#  endif
    EC_GROUP_free(NISTP);
    EC_POINT_free(G);
    EC_POINT_free(P);
    EC_POINT_free(Q);
    EC_POINT_free(Q_CHECK);
    BN_free(n);
    BN_free(m);
    BN_free(p);
    BN_free(a);
    BN_free(b);
    BN_free(x);
    BN_free(y);
    BN_free(order);
    BN_CTX_free(ctx);
}

static void nistp_tests()
{
    unsigned i;

    for (i = 0;
         i < sizeof(nistp_tests_params) / sizeof(struct nistp_test_params);
         i++) {
        nistp_single_test(&nistp_tests_params[i]);
    }
}
# endif

static const char rnd_seed[] =
    "string to make the random number generator think it has entropy";

int main(int argc, char *argv[])
{

    /* enable memory leak checking unless explicitly disabled */
    if (!((getenv("OPENSSL_DEBUG_MEMORY") != NULL)
          && (0 == strcmp(getenv("OPENSSL_DEBUG_MEMORY"), "off")))) {
        CRYPTO_malloc_debug_init();
        CRYPTO_set_mem_debug_options(V_CRYPTO_MDEBUG_ALL);
    } else {
        /* OPENSSL_DEBUG_MEMORY=off */
        CRYPTO_set_mem_debug_functions(0, 0, 0, 0, 0);
    }
    CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);
    ERR_load_crypto_strings();

    RAND_seed(rnd_seed, sizeof rnd_seed); /* or BN_generate_prime may fail */

    prime_field_tests();
    puts("");
# ifndef OPENSSL_NO_EC2M
    char2_field_tests();
# endif
# ifndef OPENSSL_NO_EC_NISTP_64_GCC_128
    nistp_tests();
# endif
    /* test the internal curves */
    internal_curve_test();

# ifndef OPENSSL_NO_ENGINE
    ENGINE_cleanup();
# endif
    CRYPTO_cleanup_all_ex_data();
    ERR_free_strings();
    ERR_remove_thread_state(NULL);
    CRYPTO_mem_leaks_fp(stderr);

    return 0;
}
#endif
