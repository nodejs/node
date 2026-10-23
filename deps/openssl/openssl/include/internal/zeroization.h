/*
 * Copyright 2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * Utility functions for handling OPENSSL_PEDANTIC_ZEROIZATION.
 *
 * ISO 19790:2012/Cor.1:2015 7.9 requires cryptographic module to provide
 * methods to zeroise all unprotected security sensitive parameters
 * (which includes both Critical/Private and Public security parameters).
 *
 * To comply with these (arguably, unnecessarily onerous) requirements,
 * freeing of public parameters is done via ossl_public_security_param_free()
 * and ossl_public_security_param_bn_free() functions, and those implement
 * the required behaviour if OPENSSL_PEDANTIC_ZEROIZATION is defined.
 */

#ifndef OSSL_INTERNAL_ZEROIZATION_H
#define OSSL_INTERNAL_ZEROIZATION_H

#include <openssl/bn.h>
#include <openssl/crypto.h>
#include <openssl/e_os2.h>

static ossl_unused ossl_inline void
ossl_public_param_free(void *ptr, size_t size)
{
#ifdef OPENSSL_PEDANTIC_ZEROIZATION
    OPENSSL_clear_free(ptr, size);
#else
    OPENSSL_free(ptr);
#endif
}

static ossl_unused ossl_inline void
ossl_public_bn_free(BIGNUM *bn)
{
#ifdef OPENSSL_PEDANTIC_ZEROIZATION
    BN_clear_free(bn);
#else
    BN_free(bn);
#endif
}

#endif /* OSSL_INTERNAL_ZEROIZATION_H */
