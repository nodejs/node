/*-
 * Copyright 2022-2023 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * Example showing how to generate DSA params using
 * FIPS 186-4 DSA FFC parameter generation.
 */

#include <openssl/evp.h>
#include "dsa.inc"

int main(int argc, char **argv)
{
    int ret = EXIT_FAILURE;
    OSSL_LIB_CTX *libctx = NULL;
    const char *propq = NULL;
    EVP_PKEY_CTX *ctx = NULL;
    EVP_PKEY *dsaparamkey = NULL;
    OSSL_PARAM params[7];
    unsigned int pbits = 2048;
    unsigned int qbits = 256;
    int gindex = 42;

    ctx = EVP_PKEY_CTX_new_from_name(libctx, "DSA", propq);
    if (ctx == NULL)
        goto cleanup;

    /*
     * Demonstrate how to set optional DSA fields as params.
     * See doc/man7/EVP_PKEY-FFC.pod and doc/man7/EVP_PKEY-DSA.pod
     * for more information.
     */
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_FFC_TYPE,
                                                 "fips186_4", 0);
    params[1] = OSSL_PARAM_construct_uint(OSSL_PKEY_PARAM_FFC_PBITS, &pbits);
    params[2] = OSSL_PARAM_construct_uint(OSSL_PKEY_PARAM_FFC_QBITS, &qbits);
    params[3] = OSSL_PARAM_construct_int(OSSL_PKEY_PARAM_FFC_GINDEX, &gindex);
    params[4] = OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_FFC_DIGEST,
                                                 "SHA384", 0);
    params[5] = OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_FFC_DIGEST_PROPS,
                                                 "provider=default", 0);
    params[6] = OSSL_PARAM_construct_end();

    /* Generate a dsa param key using optional params */
    if (EVP_PKEY_paramgen_init(ctx) <= 0
            || EVP_PKEY_CTX_set_params(ctx, params) <= 0
            || EVP_PKEY_paramgen(ctx, &dsaparamkey) <= 0) {
        fprintf(stderr, "DSA paramgen failed\n");
        goto cleanup;
    }

    if (!dsa_print_key(dsaparamkey, 0, libctx, propq))
        goto cleanup;

    ret = EXIT_SUCCESS;
cleanup:
    EVP_PKEY_free(dsaparamkey);
    EVP_PKEY_CTX_free(ctx);
    return ret;
}
