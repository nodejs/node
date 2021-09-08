/*
 * Copyright 2019-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/crypto.h>
#include "prov/blake2.h"
#include "prov/digestcommon.h"
#include "prov/implementations.h"

int ossl_blake2s256_init(void *ctx)
{
    BLAKE2S_PARAM P;

    ossl_blake2s_param_init(&P);
    return ossl_blake2s_init((BLAKE2S_CTX *)ctx, &P);
}

int ossl_blake2b512_init(void *ctx)
{
    BLAKE2B_PARAM P;

    ossl_blake2b_param_init(&P);
    return ossl_blake2b_init((BLAKE2B_CTX *)ctx, &P);
}

/* ossl_blake2s256_functions */
IMPLEMENT_digest_functions(blake2s256, BLAKE2S_CTX,
                           BLAKE2S_BLOCKBYTES, BLAKE2S_DIGEST_LENGTH, 0,
                           ossl_blake2s256_init, ossl_blake2s_update,
                           ossl_blake2s_final)

/* ossl_blake2b512_functions */
IMPLEMENT_digest_functions(blake2b512, BLAKE2B_CTX,
                           BLAKE2B_BLOCKBYTES, BLAKE2B_DIGEST_LENGTH, 0,
                           ossl_blake2b512_init, ossl_blake2b_update,
                           ossl_blake2b_final)
