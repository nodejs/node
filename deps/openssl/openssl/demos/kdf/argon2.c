/*
 * Copyright 2023 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <openssl/core_names.h>
#include <openssl/crypto.h>
#include <openssl/kdf.h>
#include <openssl/params.h>
#include <openssl/thread.h>

/*
 * Example showing how to use Argon2 KDF.
 * See man EVP_KDF-ARGON2 for more information.
 *
 * test vector from
 * https://datatracker.ietf.org/doc/html/rfc9106
 */

/*
 * Hard coding a password into an application is very bad.
 * It is done here solely for educational purposes.
 */
static unsigned char password[] = {
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01
};

/*
 * The salt is better not being hard coded too.  Each password should have a
 * different salt if possible.  The salt is not considered secret information
 * and is safe to store with an encrypted password.
 */
static unsigned char salt[] = {
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02
};

/*
 * Optional secret for KDF
 */
static unsigned char secret[] = {
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03
};

/*
 * Optional additional data
 */
static unsigned char ad[] = {
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x04, 0x04, 0x04, 0x04
};

/*
 * Argon2 cost parameters
 */
static uint32_t memory_cost    = 32;
static uint32_t iteration_cost =  3;
static uint32_t parallel_cost  =  4;

static const unsigned char expected_output[] = {
   0x0d, 0x64, 0x0d, 0xf5, 0x8d, 0x78, 0x76, 0x6c,
   0x08, 0xc0, 0x37, 0xa3, 0x4a, 0x8b, 0x53, 0xc9,
   0xd0, 0x1e, 0xf0, 0x45, 0x2d, 0x75, 0xb6, 0x5e,
   0xb5, 0x25, 0x20, 0xe9, 0x6b, 0x01, 0xe6, 0x59
};

int main(int argc, char **argv)
{
    int rv = EXIT_FAILURE;
    EVP_KDF *kdf = NULL;
    EVP_KDF_CTX *kctx = NULL;
    unsigned char out[32];
    OSSL_PARAM params[9], *p = params;
    OSSL_LIB_CTX *library_context = NULL;
    unsigned int threads;

    library_context = OSSL_LIB_CTX_new();
    if (library_context == NULL) {
        fprintf(stderr, "OSSL_LIB_CTX_new() returned NULL\n");
        goto end;
    }

    /* Fetch the key derivation function implementation */
    kdf = EVP_KDF_fetch(library_context, "argon2id", NULL);
    if (kdf == NULL) {
        fprintf(stderr, "EVP_KDF_fetch() returned NULL\n");
        goto end;
    }

    /* Create a context for the key derivation operation */
    kctx = EVP_KDF_CTX_new(kdf);
    if (kctx == NULL) {
        fprintf(stderr, "EVP_KDF_CTX_new() returned NULL\n");
        goto end;
    }

    /*
     * Thread support can be turned off; use serialization if we cannot
     * set requested number of threads.
     */
    threads = parallel_cost;
    if (OSSL_set_max_threads(library_context, parallel_cost) != 1) {
        uint64_t max_threads = OSSL_get_max_threads(library_context);

        if (max_threads == 0)
            threads = 1;
        else if (max_threads < parallel_cost)
            threads = max_threads;
    }

    /* Set password */
    *p++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_PASSWORD, password, sizeof(password));
    /* Set salt */
    *p++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT, salt, sizeof(salt));
    /* Set optional additional data */
    *p++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_ARGON2_AD, ad, sizeof(ad));
    /* Set optional secret */
    *p++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SECRET, secret, sizeof(secret));
    /* Set iteration count */
    *p++ = OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ITER, &iteration_cost);
    /* Set threads performing derivation (can be decreased) */
    *p++ = OSSL_PARAM_construct_uint(OSSL_KDF_PARAM_THREADS, &threads);
    /* Set parallel cost */
    *p++ = OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ARGON2_LANES, &parallel_cost);
    /* Set memory requirement */
    *p++ = OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ARGON2_MEMCOST, &memory_cost);
    *p = OSSL_PARAM_construct_end();

    /* Derive the key */
    if (EVP_KDF_derive(kctx, out, sizeof(out), params) != 1) {
        fprintf(stderr, "EVP_KDF_derive() failed\n");
        goto end;
    }

    if (CRYPTO_memcmp(expected_output, out, sizeof(expected_output)) != 0) {
        fprintf(stderr, "Generated key does not match expected value\n");
        goto end;
    }

    printf("Success\n");

    rv = EXIT_SUCCESS;
end:
    EVP_KDF_CTX_free(kctx);
    EVP_KDF_free(kdf);
    OSSL_LIB_CTX_free(library_context);
    return rv;
}
