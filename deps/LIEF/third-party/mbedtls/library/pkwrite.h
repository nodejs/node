/**
 * \file pkwrite.h
 *
 * \brief Internal defines shared by the PK write module
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_PK_WRITE_H
#define MBEDTLS_PK_WRITE_H

#include "mbedtls/build_info.h"

#include "mbedtls/pk.h"

#if defined(MBEDTLS_USE_PSA_CRYPTO)
#include "psa/crypto.h"
#endif /* MBEDTLS_USE_PSA_CRYPTO */

/*
 * Max sizes of key per types. Shown as tag + len (+ content).
 */

#if defined(MBEDTLS_RSA_C)
/*
 * RSA public keys:
 *  SubjectPublicKeyInfo  ::=  SEQUENCE  {          1 + 3
 *       algorithm            AlgorithmIdentifier,  1 + 1 (sequence)
 *                                                + 1 + 1 + 9 (rsa oid)
 *                                                + 1 + 1 (params null)
 *       subjectPublicKey     BIT STRING }          1 + 3 + (1 + below)
 *  RSAPublicKey ::= SEQUENCE {                     1 + 3
 *      modulus           INTEGER,  -- n            1 + 3 + MPI_MAX + 1
 *      publicExponent    INTEGER   -- e            1 + 3 + MPI_MAX + 1
 *  }
 */
#define MBEDTLS_PK_RSA_PUB_DER_MAX_BYTES    (38 + 2 * MBEDTLS_MPI_MAX_SIZE)

/*
 * RSA private keys:
 *  RSAPrivateKey ::= SEQUENCE {                    1 + 3
 *      version           Version,                  1 + 1 + 1
 *      modulus           INTEGER,                  1 + 3 + MPI_MAX + 1
 *      publicExponent    INTEGER,                  1 + 3 + MPI_MAX + 1
 *      privateExponent   INTEGER,                  1 + 3 + MPI_MAX + 1
 *      prime1            INTEGER,                  1 + 3 + MPI_MAX / 2 + 1
 *      prime2            INTEGER,                  1 + 3 + MPI_MAX / 2 + 1
 *      exponent1         INTEGER,                  1 + 3 + MPI_MAX / 2 + 1
 *      exponent2         INTEGER,                  1 + 3 + MPI_MAX / 2 + 1
 *      coefficient       INTEGER,                  1 + 3 + MPI_MAX / 2 + 1
 *      otherPrimeInfos   OtherPrimeInfos OPTIONAL  0 (not supported)
 *  }
 */
#define MBEDTLS_MPI_MAX_SIZE_2  (MBEDTLS_MPI_MAX_SIZE / 2 + \
                                 MBEDTLS_MPI_MAX_SIZE % 2)
#define MBEDTLS_PK_RSA_PRV_DER_MAX_BYTES    (47 + 3 * MBEDTLS_MPI_MAX_SIZE \
                                             + 5 * MBEDTLS_MPI_MAX_SIZE_2)

#else /* MBEDTLS_RSA_C */

#define MBEDTLS_PK_RSA_PUB_DER_MAX_BYTES   0
#define MBEDTLS_PK_RSA_PRV_DER_MAX_BYTES   0

#endif /* MBEDTLS_RSA_C */

#if defined(MBEDTLS_PK_HAVE_ECC_KEYS)

/* Find the maximum number of bytes necessary to store an EC point. When USE_PSA
 * is defined this means looking for the maximum between PSA and built-in
 * supported curves. */
#if defined(MBEDTLS_USE_PSA_CRYPTO)
#define MBEDTLS_PK_MAX_ECC_BYTES   (PSA_BITS_TO_BYTES(PSA_VENDOR_ECC_MAX_CURVE_BITS) > \
                                    MBEDTLS_ECP_MAX_BYTES ? \
                                    PSA_BITS_TO_BYTES(PSA_VENDOR_ECC_MAX_CURVE_BITS) : \
                                    MBEDTLS_ECP_MAX_BYTES)
#else /* MBEDTLS_USE_PSA_CRYPTO */
#define MBEDTLS_PK_MAX_ECC_BYTES   MBEDTLS_ECP_MAX_BYTES
#endif /* MBEDTLS_USE_PSA_CRYPTO */

/*
 * EC public keys:
 *  SubjectPublicKeyInfo  ::=  SEQUENCE  {      1 + 2
 *    algorithm         AlgorithmIdentifier,    1 + 1 (sequence)
 *                                            + 1 + 1 + 7 (ec oid)
 *                                            + 1 + 1 + 9 (namedCurve oid)
 *    subjectPublicKey  BIT STRING              1 + 2 + 1               [1]
 *                                            + 1 (point format)        [1]
 *                                            + 2 * ECP_MAX (coords)    [1]
 *  }
 */
#define MBEDTLS_PK_ECP_PUB_DER_MAX_BYTES    (30 + 2 * MBEDTLS_PK_MAX_ECC_BYTES)

/*
 * EC private keys:
 * ECPrivateKey ::= SEQUENCE {                  1 + 2
 *      version        INTEGER ,                1 + 1 + 1
 *      privateKey     OCTET STRING,            1 + 1 + ECP_MAX
 *      parameters [0] ECParameters OPTIONAL,   1 + 1 + (1 + 1 + 9)
 *      publicKey  [1] BIT STRING OPTIONAL      1 + 2 + [1] above
 *    }
 */
#define MBEDTLS_PK_ECP_PRV_DER_MAX_BYTES    (29 + 3 * MBEDTLS_PK_MAX_ECC_BYTES)

#else /* MBEDTLS_PK_HAVE_ECC_KEYS */

#define MBEDTLS_PK_ECP_PUB_DER_MAX_BYTES   0
#define MBEDTLS_PK_ECP_PRV_DER_MAX_BYTES   0

#endif /* MBEDTLS_PK_HAVE_ECC_KEYS */

/* Define the maximum available public key DER length based on the supported
 * key types (EC and/or RSA). */
#if (MBEDTLS_PK_RSA_PUB_DER_MAX_BYTES > MBEDTLS_PK_ECP_PUB_DER_MAX_BYTES)
#define MBEDTLS_PK_WRITE_PUBKEY_MAX_SIZE    MBEDTLS_PK_RSA_PUB_DER_MAX_BYTES
#else
#define MBEDTLS_PK_WRITE_PUBKEY_MAX_SIZE    MBEDTLS_PK_ECP_PUB_DER_MAX_BYTES
#endif

#endif /* MBEDTLS_PK_WRITE_H */
