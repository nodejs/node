/*
 * Copyright 1995-2019 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * NB: These functions have been upgraded - the previous prototypes are in
 * dh_depr.c as wrappers to these ones.  - Geoff
 */

#include <stdio.h>
#include "internal/cryptlib.h"
#include <openssl/bn.h>
#include "dh_locl.h"

static int dh_builtin_genparams(DH *ret, int prime_len, int generator,
                                BN_GENCB *cb);

int DH_generate_parameters_ex(DH *ret, int prime_len, int generator,
                              BN_GENCB *cb)
{
    if (ret->meth->generate_params)
        return ret->meth->generate_params(ret, prime_len, generator, cb);
    return dh_builtin_genparams(ret, prime_len, generator, cb);
}

/*-
 * We generate DH parameters as follows
 * find a prime p which is prime_len bits long,
 * where q=(p-1)/2 is also prime.
 * In the following we assume that g is not 0, 1 or p-1, since it
 * would generate only trivial subgroups.
 * For this case, g is a generator of the order-q subgroup if
 * g^q mod p == 1.
 * Or in terms of the Legendre symbol: (g/p) == 1.
 *
 * Having said all that,
 * there is another special case method for the generators 2, 3 and 5.
 * Using the quadratic reciprocity law it is possible to solve
 * (g/p) == 1 for the special values 2, 3, 5:
 * (2/p) == 1 if p mod 8 == 1 or 7.
 * (3/p) == 1 if p mod 12 == 1 or 11.
 * (5/p) == 1 if p mod 5 == 1 or 4.
 * See for instance: https://en.wikipedia.org/wiki/Legendre_symbol
 *
 * Since all safe primes > 7 must satisfy p mod 12 == 11
 * and all safe primes > 11 must satisfy p mod 5 != 1
 * we can further improve the condition for g = 2, 3 and 5:
 * for 2, p mod 24 == 23
 * for 3, p mod 12 == 11
 * for 5, p mod 60 == 59
 *
 * However for compatibilty with previous versions we use:
 * for 2, p mod 24 == 11
 * for 5, p mod 60 == 23
 */
static int dh_builtin_genparams(DH *ret, int prime_len, int generator,
                                BN_GENCB *cb)
{
    BIGNUM *t1, *t2;
    int g, ok = -1;
    BN_CTX *ctx = NULL;

    ctx = BN_CTX_new();
    if (ctx == NULL)
        goto err;
    BN_CTX_start(ctx);
    t1 = BN_CTX_get(ctx);
    t2 = BN_CTX_get(ctx);
    if (t2 == NULL)
        goto err;

    /* Make sure 'ret' has the necessary elements */
    if (!ret->p && ((ret->p = BN_new()) == NULL))
        goto err;
    if (!ret->g && ((ret->g = BN_new()) == NULL))
        goto err;

    if (generator <= 1) {
        DHerr(DH_F_DH_BUILTIN_GENPARAMS, DH_R_BAD_GENERATOR);
        goto err;
    }
    if (generator == DH_GENERATOR_2) {
        if (!BN_set_word(t1, 24))
            goto err;
        if (!BN_set_word(t2, 11))
            goto err;
        g = 2;
    } else if (generator == DH_GENERATOR_5) {
        if (!BN_set_word(t1, 60))
            goto err;
        if (!BN_set_word(t2, 23))
            goto err;
        g = 5;
    } else {
        /*
         * in the general case, don't worry if 'generator' is a generator or
         * not: since we are using safe primes, it will generate either an
         * order-q or an order-2q group, which both is OK
         */
        if (!BN_set_word(t1, 12))
            goto err;
        if (!BN_set_word(t2, 11))
            goto err;
        g = generator;
    }

    if (!BN_generate_prime_ex(ret->p, prime_len, 1, t1, t2, cb))
        goto err;
    if (!BN_GENCB_call(cb, 3, 0))
        goto err;
    if (!BN_set_word(ret->g, g))
        goto err;
    ok = 1;
 err:
    if (ok == -1) {
        DHerr(DH_F_DH_BUILTIN_GENPARAMS, ERR_R_BN_LIB);
        ok = 0;
    }

    BN_CTX_end(ctx);
    BN_CTX_free(ctx);
    return ok;
}
