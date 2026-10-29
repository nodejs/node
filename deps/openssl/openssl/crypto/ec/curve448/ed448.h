/*
 * Copyright 2017-2026 The OpenSSL Project Authors. All Rights Reserved.
 * Copyright 2015-2016 Cryptography Research, Inc.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 *
 * Originally written by Mike Hamburg
 */

#ifndef OSSL_CRYPTO_EC_CURVE448_ED448_H
#define OSSL_CRYPTO_EC_CURVE448_ED448_H

#include <openssl/types.h>

#include "point_448.h"

/* Number of bytes in an EdDSA public key. */
#define EDDSA_448_PUBLIC_BYTES 57

/* Number of bytes in an EdDSA private key. */
#define EDDSA_448_PRIVATE_BYTES EDDSA_448_PUBLIC_BYTES

/* Number of bytes in an EdDSA signature. */
#define EDDSA_448_SIGNATURE_BYTES (EDDSA_448_PUBLIC_BYTES + EDDSA_448_PRIVATE_BYTES)

/* EdDSA encoding ratio. */
#define C448_EDDSA_ENCODE_RATIO 4

/* EdDSA decoding ratio. */
#define C448_EDDSA_DECODE_RATIO (4 / 4)

/*
 * EdDSA point encoding.  Used internally, exposed externally.
 * Multiplies by C448_EDDSA_ENCODE_RATIO first.
 *
 * The multiplication is required because the EdDSA encoding represents
 * the cofactor information, but the Decaf encoding ignores it (which
 * is the whole point).  So if you decode from EdDSA and re-encode to
 * EdDSA, the cofactor info must get cleared, because the intermediate
 * representation doesn't track it.
 *
 * The way we handle this is to multiply by C448_EDDSA_DECODE_RATIO when
 * decoding, and by C448_EDDSA_ENCODE_RATIO when encoding.  The product of
 * these ratios is always exactly the cofactor 4, so the cofactor ends up
 * cleared one way or another.  But exactly how that shakes out depends on the
 * base points specified in RFC 8032.
 *
 * The upshot is that if you pass the Decaf/Ristretto base point to
 * this function, you will get C448_EDDSA_ENCODE_RATIO times the
 * EdDSA base point.
 *
 * enc (out): The encoded point.
 * p (in): The point.
 */
void ossl_curve448_point_mul_by_ratio_and_encode_like_eddsa(
    uint8_t enc[EDDSA_448_PUBLIC_BYTES],
    const curve448_point_t p);

/*
 * EdDSA point decoding.  Multiplies by C448_EDDSA_DECODE_RATIO, and
 * ignores cofactor information.
 *
 * See notes on curve448_point_mul_by_ratio_and_encode_like_eddsa
 *
 * enc (out): The encoded point.
 * p (in): The point.
 */
c448_error_t
ossl_curve448_point_decode_like_eddsa_and_mul_by_ratio(
    curve448_point_t p,
    const uint8_t enc[EDDSA_448_PUBLIC_BYTES]);

#endif /* OSSL_CRYPTO_EC_CURVE448_ED448_H */
