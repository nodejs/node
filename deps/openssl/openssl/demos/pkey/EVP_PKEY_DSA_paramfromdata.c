/*-
 * Copyright 2022 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * Example showing how to load DSA params from raw data
 * using EVP_PKEY_fromdata()
 */

#include <openssl/param_build.h>
#include <openssl/evp.h>
#include <openssl/core_names.h>
#include "dsa.inc"

int main(int argc, char **argv)
{
    int rv = EXIT_FAILURE;
    OSSL_LIB_CTX *libctx = NULL;
    const char *propq = NULL;
    EVP_PKEY_CTX *ctx = NULL;
    EVP_PKEY *dsaparamkey = NULL;
    OSSL_PARAM_BLD *bld = NULL;
    OSSL_PARAM *params = NULL;
    BIGNUM *p = NULL, *q = NULL, *g = NULL;

    p = BN_bin2bn(dsa_p, sizeof(dsa_p), NULL);
    q = BN_bin2bn(dsa_q, sizeof(dsa_q), NULL);
    g = BN_bin2bn(dsa_g, sizeof(dsa_g), NULL);
    if (p == NULL || q == NULL || g == NULL)
        goto cleanup;

    /* Use OSSL_PARAM_BLD if you need to handle BIGNUM Parameters */
    bld = OSSL_PARAM_BLD_new();
    if (bld == NULL)
        goto cleanup;
    if (!OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_FFC_P, p)
            || !OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_FFC_Q, q)
            || !OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_FFC_G, g))
        goto cleanup;
    params = OSSL_PARAM_BLD_to_param(bld);
    if (params == NULL)
        goto cleanup;

    ctx = EVP_PKEY_CTX_new_from_name(libctx, "DSA", propq);
    if (ctx == NULL) {
        fprintf(stderr, "EVP_PKEY_CTX_new_from_name() failed\n");
        goto cleanup;
    }

    if (EVP_PKEY_fromdata_init(ctx) <= 0
            || EVP_PKEY_fromdata(ctx, &dsaparamkey, EVP_PKEY_KEY_PARAMETERS, params) <= 0) {
        fprintf(stderr, "EVP_PKEY_fromdata() failed\n");
        goto cleanup;
    }

    if (!dsa_print_key(dsaparamkey, 0, libctx, propq))
        goto cleanup;

    rv = EXIT_SUCCESS;
cleanup:
    EVP_PKEY_free(dsaparamkey);
    EVP_PKEY_CTX_free(ctx);
    OSSL_PARAM_free(params);
    OSSL_PARAM_BLD_free(bld);
    BN_free(g);
    BN_free(q);
    BN_free(p);

    return rv;
}
