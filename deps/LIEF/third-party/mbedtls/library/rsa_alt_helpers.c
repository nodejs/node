/*
 *  Helper functions for the RSA module
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 *
 */

#include "common.h"

#if defined(MBEDTLS_RSA_C)

#include "mbedtls/rsa.h"
#include "mbedtls/bignum.h"
#include "rsa_alt_helpers.h"

/*
 * Compute RSA prime factors from public and private exponents
 *
 * Summary of algorithm:
 * Setting F := lcm(P-1,Q-1), the idea is as follows:
 *
 * (a) For any 1 <= X < N with gcd(X,N)=1, we have X^F = 1 modulo N, so X^(F/2)
 *     is a square root of 1 in Z/NZ. Since Z/NZ ~= Z/PZ x Z/QZ by CRT and the
 *     square roots of 1 in Z/PZ and Z/QZ are +1 and -1, this leaves the four
 *     possibilities X^(F/2) = (+-1, +-1). If it happens that X^(F/2) = (-1,+1)
 *     or (+1,-1), then gcd(X^(F/2) + 1, N) will be equal to one of the prime
 *     factors of N.
 *
 * (b) If we don't know F/2 but (F/2) * K for some odd (!) K, then the same
 *     construction still applies since (-)^K is the identity on the set of
 *     roots of 1 in Z/NZ.
 *
 * The public and private key primitives (-)^E and (-)^D are mutually inverse
 * bijections on Z/NZ if and only if (-)^(DE) is the identity on Z/NZ, i.e.
 * if and only if DE - 1 is a multiple of F, say DE - 1 = F * L.
 * Splitting L = 2^t * K with K odd, we have
 *
 *   DE - 1 = FL = (F/2) * (2^(t+1)) * K,
 *
 * so (F / 2) * K is among the numbers
 *
 *   (DE - 1) >> 1, (DE - 1) >> 2, ..., (DE - 1) >> ord
 *
 * where ord is the order of 2 in (DE - 1).
 * We can therefore iterate through these numbers apply the construction
 * of (a) and (b) above to attempt to factor N.
 *
 */
int mbedtls_rsa_deduce_primes(mbedtls_mpi const *N,
                              mbedtls_mpi const *E, mbedtls_mpi const *D,
                              mbedtls_mpi *P, mbedtls_mpi *Q)
{
    int ret = 0;

    uint16_t attempt;  /* Number of current attempt  */
    uint16_t iter;     /* Number of squares computed in the current attempt */

    uint16_t order;    /* Order of 2 in DE - 1 */

    mbedtls_mpi T;  /* Holds largest odd divisor of DE - 1     */
    mbedtls_mpi K;  /* Temporary holding the current candidate */

    const unsigned char primes[] = { 2,
                                     3,    5,    7,   11,   13,   17,   19,   23,
                                     29,   31,   37,   41,   43,   47,   53,   59,
                                     61,   67,   71,   73,   79,   83,   89,   97,
                                     101,  103,  107,  109,  113,  127,  131,  137,
                                     139,  149,  151,  157,  163,  167,  173,  179,
                                     181,  191,  193,  197,  199,  211,  223,  227,
                                     229,  233,  239,  241,  251 };

    const size_t num_primes = sizeof(primes) / sizeof(*primes);

    if (P == NULL || Q == NULL || P->p != NULL || Q->p != NULL) {
        return MBEDTLS_ERR_MPI_BAD_INPUT_DATA;
    }

    if (mbedtls_mpi_cmp_int(N, 0) <= 0 ||
        mbedtls_mpi_cmp_int(D, 1) <= 0 ||
        mbedtls_mpi_cmp_mpi(D, N) >= 0 ||
        mbedtls_mpi_cmp_int(E, 1) <= 0 ||
        mbedtls_mpi_cmp_mpi(E, N) >= 0) {
        return MBEDTLS_ERR_MPI_BAD_INPUT_DATA;
    }

    /*
     * Initializations and temporary changes
     */

    mbedtls_mpi_init(&K);
    mbedtls_mpi_init(&T);

    /* T := DE - 1 */
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&T, D,  E));
    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_int(&T, &T, 1));

    if ((order = (uint16_t) mbedtls_mpi_lsb(&T)) == 0) {
        ret = MBEDTLS_ERR_MPI_BAD_INPUT_DATA;
        goto cleanup;
    }

    /* After this operation, T holds the largest odd divisor of DE - 1. */
    MBEDTLS_MPI_CHK(mbedtls_mpi_shift_r(&T, order));

    /*
     * Actual work
     */

    /* Skip trying 2 if N == 1 mod 8 */
    attempt = 0;
    if (N->p[0] % 8 == 1) {
        attempt = 1;
    }

    for (; attempt < num_primes; ++attempt) {
        MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&K, primes[attempt]));

        /* Check if gcd(K,N) = 1 */
        MBEDTLS_MPI_CHK(mbedtls_mpi_gcd(P, &K, N));
        if (mbedtls_mpi_cmp_int(P, 1) != 0) {
            continue;
        }

        /* Go through K^T + 1, K^(2T) + 1, K^(4T) + 1, ...
         * and check whether they have nontrivial GCD with N. */
        MBEDTLS_MPI_CHK(mbedtls_mpi_exp_mod(&K, &K, &T, N,
                                            Q /* temporarily use Q for storing Montgomery
                                               * multiplication helper values */));

        for (iter = 1; iter <= order; ++iter) {
            /* If we reach 1 prematurely, there's no point
             * in continuing to square K */
            if (mbedtls_mpi_cmp_int(&K, 1) == 0) {
                break;
            }

            MBEDTLS_MPI_CHK(mbedtls_mpi_add_int(&K, &K, 1));
            MBEDTLS_MPI_CHK(mbedtls_mpi_gcd(P, &K, N));

            if (mbedtls_mpi_cmp_int(P, 1) ==  1 &&
                mbedtls_mpi_cmp_mpi(P, N) == -1) {
                /*
                 * Have found a nontrivial divisor P of N.
                 * Set Q := N / P.
                 */

                MBEDTLS_MPI_CHK(mbedtls_mpi_div_mpi(Q, NULL, N, P));
                goto cleanup;
            }

            MBEDTLS_MPI_CHK(mbedtls_mpi_sub_int(&K, &K, 1));
            MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&K, &K, &K));
            MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&K, &K, N));
        }

        /*
         * If we get here, then either we prematurely aborted the loop because
         * we reached 1, or K holds primes[attempt]^(DE - 1) mod N, which must
         * be 1 if D,E,N were consistent.
         * Check if that's the case and abort if not, to avoid very long,
         * yet eventually failing, computations if N,D,E were not sane.
         */
        if (mbedtls_mpi_cmp_int(&K, 1) != 0) {
            break;
        }
    }

    ret = MBEDTLS_ERR_MPI_BAD_INPUT_DATA;

cleanup:

    mbedtls_mpi_free(&K);
    mbedtls_mpi_free(&T);
    return ret;
}

/*
 * Given P, Q and the public exponent E, deduce D.
 * This is essentially a modular inversion.
 */
int mbedtls_rsa_deduce_private_exponent(mbedtls_mpi const *P,
                                        mbedtls_mpi const *Q,
                                        mbedtls_mpi const *E,
                                        mbedtls_mpi *D)
{
    int ret = 0;
    mbedtls_mpi K, L;

    if (D == NULL || mbedtls_mpi_cmp_int(D, 0) != 0) {
        return MBEDTLS_ERR_MPI_BAD_INPUT_DATA;
    }

    if (mbedtls_mpi_cmp_int(P, 1) <= 0 ||
        mbedtls_mpi_cmp_int(Q, 1) <= 0 ||
        mbedtls_mpi_cmp_int(E, 0) == 0) {
        return MBEDTLS_ERR_MPI_BAD_INPUT_DATA;
    }

    mbedtls_mpi_init(&K);
    mbedtls_mpi_init(&L);

    /* Temporarily put K := P-1 and L := Q-1 */
    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_int(&K, P, 1));
    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_int(&L, Q, 1));

    /* Temporarily put D := gcd(P-1, Q-1) */
    MBEDTLS_MPI_CHK(mbedtls_mpi_gcd(D, &K, &L));

    /* K := LCM(P-1, Q-1) */
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&K, &K, &L));
    MBEDTLS_MPI_CHK(mbedtls_mpi_div_mpi(&K, NULL, &K, D));

    /* Compute modular inverse of E in LCM(P-1, Q-1) */
    MBEDTLS_MPI_CHK(mbedtls_mpi_inv_mod(D, E, &K));

cleanup:

    mbedtls_mpi_free(&K);
    mbedtls_mpi_free(&L);

    return ret;
}

int mbedtls_rsa_deduce_crt(const mbedtls_mpi *P, const mbedtls_mpi *Q,
                           const mbedtls_mpi *D, mbedtls_mpi *DP,
                           mbedtls_mpi *DQ, mbedtls_mpi *QP)
{
    int ret = 0;
    mbedtls_mpi K;
    mbedtls_mpi_init(&K);

    /* DP = D mod P-1 */
    if (DP != NULL) {
        MBEDTLS_MPI_CHK(mbedtls_mpi_sub_int(&K, P, 1));
        MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(DP, D, &K));
    }

    /* DQ = D mod Q-1 */
    if (DQ != NULL) {
        MBEDTLS_MPI_CHK(mbedtls_mpi_sub_int(&K, Q, 1));
        MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(DQ, D, &K));
    }

    /* QP = Q^{-1} mod P */
    if (QP != NULL) {
        MBEDTLS_MPI_CHK(mbedtls_mpi_inv_mod(QP, Q, P));
    }

cleanup:
    mbedtls_mpi_free(&K);

    return ret;
}

/*
 * Check that core RSA parameters are sane.
 */
int mbedtls_rsa_validate_params(const mbedtls_mpi *N, const mbedtls_mpi *P,
                                const mbedtls_mpi *Q, const mbedtls_mpi *D,
                                const mbedtls_mpi *E,
                                int (*f_rng)(void *, unsigned char *, size_t),
                                void *p_rng)
{
    int ret = 0;
    mbedtls_mpi K, L;

    mbedtls_mpi_init(&K);
    mbedtls_mpi_init(&L);

    /*
     * Step 1: If PRNG provided, check that P and Q are prime
     */

#if defined(MBEDTLS_GENPRIME)
    /*
     * When generating keys, the strongest security we support aims for an error
     * rate of at most 2^-100 and we are aiming for the same certainty here as
     * well.
     */
    if (f_rng != NULL && P != NULL &&
        (ret = mbedtls_mpi_is_prime_ext(P, 50, f_rng, p_rng)) != 0) {
        ret = MBEDTLS_ERR_RSA_KEY_CHECK_FAILED;
        goto cleanup;
    }

    if (f_rng != NULL && Q != NULL &&
        (ret = mbedtls_mpi_is_prime_ext(Q, 50, f_rng, p_rng)) != 0) {
        ret = MBEDTLS_ERR_RSA_KEY_CHECK_FAILED;
        goto cleanup;
    }
#else
    ((void) f_rng);
    ((void) p_rng);
#endif /* MBEDTLS_GENPRIME */

    /*
     * Step 2: Check that 1 < N = P * Q
     */

    if (P != NULL && Q != NULL && N != NULL) {
        MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&K, P, Q));
        if (mbedtls_mpi_cmp_int(N, 1)  <= 0 ||
            mbedtls_mpi_cmp_mpi(&K, N) != 0) {
            ret = MBEDTLS_ERR_RSA_KEY_CHECK_FAILED;
            goto cleanup;
        }
    }

    /*
     * Step 3: Check and 1 < D, E < N if present.
     */

    if (N != NULL && D != NULL && E != NULL) {
        if (mbedtls_mpi_cmp_int(D, 1) <= 0 ||
            mbedtls_mpi_cmp_int(E, 1) <= 0 ||
            mbedtls_mpi_cmp_mpi(D, N) >= 0 ||
            mbedtls_mpi_cmp_mpi(E, N) >= 0) {
            ret = MBEDTLS_ERR_RSA_KEY_CHECK_FAILED;
            goto cleanup;
        }
    }

    /*
     * Step 4: Check that D, E are inverse modulo P-1 and Q-1
     */

    if (P != NULL && Q != NULL && D != NULL && E != NULL) {
        if (mbedtls_mpi_cmp_int(P, 1) <= 0 ||
            mbedtls_mpi_cmp_int(Q, 1) <= 0) {
            ret = MBEDTLS_ERR_RSA_KEY_CHECK_FAILED;
            goto cleanup;
        }

        /* Compute DE-1 mod P-1 */
        MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&K, D, E));
        MBEDTLS_MPI_CHK(mbedtls_mpi_sub_int(&K, &K, 1));
        MBEDTLS_MPI_CHK(mbedtls_mpi_sub_int(&L, P, 1));
        MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&K, &K, &L));
        if (mbedtls_mpi_cmp_int(&K, 0) != 0) {
            ret = MBEDTLS_ERR_RSA_KEY_CHECK_FAILED;
            goto cleanup;
        }

        /* Compute DE-1 mod Q-1 */
        MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&K, D, E));
        MBEDTLS_MPI_CHK(mbedtls_mpi_sub_int(&K, &K, 1));
        MBEDTLS_MPI_CHK(mbedtls_mpi_sub_int(&L, Q, 1));
        MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&K, &K, &L));
        if (mbedtls_mpi_cmp_int(&K, 0) != 0) {
            ret = MBEDTLS_ERR_RSA_KEY_CHECK_FAILED;
            goto cleanup;
        }
    }

cleanup:

    mbedtls_mpi_free(&K);
    mbedtls_mpi_free(&L);

    /* Wrap MPI error codes by RSA check failure error code */
    if (ret != 0 && ret != MBEDTLS_ERR_RSA_KEY_CHECK_FAILED) {
        ret += MBEDTLS_ERR_RSA_KEY_CHECK_FAILED;
    }

    return ret;
}

/*
 * Check that RSA CRT parameters are in accordance with core parameters.
 */
int mbedtls_rsa_validate_crt(const mbedtls_mpi *P,  const mbedtls_mpi *Q,
                             const mbedtls_mpi *D,  const mbedtls_mpi *DP,
                             const mbedtls_mpi *DQ, const mbedtls_mpi *QP)
{
    int ret = 0;

    mbedtls_mpi K, L;
    mbedtls_mpi_init(&K);
    mbedtls_mpi_init(&L);

    /* Check that DP - D == 0 mod P - 1 */
    if (DP != NULL) {
        if (P == NULL) {
            ret = MBEDTLS_ERR_RSA_BAD_INPUT_DATA;
            goto cleanup;
        }

        MBEDTLS_MPI_CHK(mbedtls_mpi_sub_int(&K, P, 1));
        MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mpi(&L, DP, D));
        MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&L, &L, &K));

        if (mbedtls_mpi_cmp_int(&L, 0) != 0) {
            ret = MBEDTLS_ERR_RSA_KEY_CHECK_FAILED;
            goto cleanup;
        }
    }

    /* Check that DQ - D == 0 mod Q - 1 */
    if (DQ != NULL) {
        if (Q == NULL) {
            ret = MBEDTLS_ERR_RSA_BAD_INPUT_DATA;
            goto cleanup;
        }

        MBEDTLS_MPI_CHK(mbedtls_mpi_sub_int(&K, Q, 1));
        MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mpi(&L, DQ, D));
        MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&L, &L, &K));

        if (mbedtls_mpi_cmp_int(&L, 0) != 0) {
            ret = MBEDTLS_ERR_RSA_KEY_CHECK_FAILED;
            goto cleanup;
        }
    }

    /* Check that QP * Q - 1 == 0 mod P */
    if (QP != NULL) {
        if (P == NULL || Q == NULL) {
            ret = MBEDTLS_ERR_RSA_BAD_INPUT_DATA;
            goto cleanup;
        }

        MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&K, QP, Q));
        MBEDTLS_MPI_CHK(mbedtls_mpi_sub_int(&K, &K, 1));
        MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&K, &K, P));
        if (mbedtls_mpi_cmp_int(&K, 0) != 0) {
            ret = MBEDTLS_ERR_RSA_KEY_CHECK_FAILED;
            goto cleanup;
        }
    }

cleanup:

    /* Wrap MPI error codes by RSA check failure error code */
    if (ret != 0 &&
        ret != MBEDTLS_ERR_RSA_KEY_CHECK_FAILED &&
        ret != MBEDTLS_ERR_RSA_BAD_INPUT_DATA) {
        ret += MBEDTLS_ERR_RSA_KEY_CHECK_FAILED;
    }

    mbedtls_mpi_free(&K);
    mbedtls_mpi_free(&L);

    return ret;
}

#endif /* MBEDTLS_RSA_C */
