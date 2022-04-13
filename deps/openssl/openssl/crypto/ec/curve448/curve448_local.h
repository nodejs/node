/*
 * Copyright 2017-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */
#ifndef OSSL_CRYPTO_EC_CURVE448_LOCAL_H
# define OSSL_CRYPTO_EC_CURVE448_LOCAL_H
# include "curve448utils.h"

int
ossl_ed448ph_sign(OSSL_LIB_CTX *ctx, uint8_t *out_sig, const uint8_t hash[64],
                  const uint8_t public_key[57], const uint8_t private_key[57],
                  const uint8_t *context, size_t context_len, const char *propq);

int
ossl_ed448ph_verify(OSSL_LIB_CTX *ctx, const uint8_t hash[64],
                    const uint8_t signature[114], const uint8_t public_key[57],
                    const uint8_t *context, size_t context_len,
                    const char *propq);

#endif              /* OSSL_CRYPTO_EC_CURVE448_LOCAL_H */
