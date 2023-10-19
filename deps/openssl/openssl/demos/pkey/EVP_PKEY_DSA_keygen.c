/*-
 * Copyright 2022 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * Example showing how to generate an DSA key pair.
 */

#include <openssl/evp.h>
#include "dsa.inc"

/*
 * Generate dsa params using default values.
 * See the EVP_PKEY_DSA_param_fromdata demo if you need
 * to load DSA params from raw values.
 * See the EVP_PKEY_DSA_paramgen demo if you need to
 * use non default parameters.
 */
EVP_PKEY *dsa_genparams(OSSL_LIB_CTX *libctx, const char *propq)
{
    EVP_PKEY *dsaparamkey = NULL;
    EVP_PKEY_CTX *ctx = NULL;

    /* Use the dsa params in a EVP_PKEY ctx */
    ctx = EVP_PKEY_CTX_new_from_name(libctx, "DSA", propq);
    if (ctx == NULL) {
        fprintf(stderr, "EVP_PKEY_CTX_new_from_name() failed\n");
        return NULL;
    }

    if (EVP_PKEY_paramgen_init(ctx) <= 0
            || EVP_PKEY_paramgen(ctx, &dsaparamkey) <= 0) {
        fprintf(stderr, "DSA paramgen failed\n");
        goto cleanup;
    }
cleanup:
    EVP_PKEY_CTX_free(ctx);
    return dsaparamkey;
}

int main(int argc, char **argv)
{
    int rv = EXIT_FAILURE;
    OSSL_LIB_CTX *libctx = NULL;
    const char *propq = NULL;
    EVP_PKEY *dsaparamskey = NULL;
    EVP_PKEY *dsakey = NULL;
    EVP_PKEY_CTX *ctx = NULL;

    /* Generate random dsa params */
    dsaparamskey = dsa_genparams(libctx, propq);
    if (dsaparamskey == NULL)
        goto cleanup;

    /* Use the dsa params in a EVP_PKEY ctx */
    ctx = EVP_PKEY_CTX_new_from_pkey(libctx, dsaparamskey, propq);
    if (ctx == NULL) {
        fprintf(stderr, "EVP_PKEY_CTX_new_from_pkey() failed\n");
        goto cleanup;
    }

    /* Generate a key using the dsa params */
    if (EVP_PKEY_keygen_init(ctx) <= 0
            || EVP_PKEY_keygen(ctx, &dsakey) <= 0) {
        fprintf(stderr, "DSA keygen failed\n");
        goto cleanup;
    }

    if (!dsa_print_key(dsakey, 1, libctx, propq))
        goto cleanup;

    rv = EXIT_SUCCESS;
cleanup:
    EVP_PKEY_free(dsakey);
    EVP_PKEY_free(dsaparamskey);
    EVP_PKEY_CTX_free(ctx);
    return rv;
}
