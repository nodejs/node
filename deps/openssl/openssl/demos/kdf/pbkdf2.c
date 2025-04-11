/*
 * Copyright 2021-2023 The OpenSSL Project Authors. All Rights Reserved.
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
#include <openssl/obj_mac.h>
#include <openssl/params.h>

/*
 * test vector from
 * https://datatracker.ietf.org/doc/html/rfc7914
 */

/*
 * Hard coding a password into an application is very bad.
 * It is done here solely for educational purposes.
 */
static unsigned char password[] = {
    'P', 'a', 's', 's', 'w', 'o', 'r', 'd'
};

/*
 * The salt is better not being hard coded too.  Each password should have a
 * different salt if possible.  The salt is not considered secret information
 * and is safe to store with an encrypted password.
 */
static unsigned char pbkdf2_salt[] = {
    'N', 'a', 'C', 'l'
};

/*
 * The iteration parameter can be variable or hard coded.  The disadvantage with
 * hard coding them is that they cannot easily be adjusted for future
 * technological improvements appear.
 */
static unsigned int pbkdf2_iterations = 80000;

static const unsigned char expected_output[] = {

    0x4d, 0xdc, 0xd8, 0xf6, 0x0b, 0x98, 0xbe, 0x21,
    0x83, 0x0c, 0xee, 0x5e, 0xf2, 0x27, 0x01, 0xf9,
    0x64, 0x1a, 0x44, 0x18, 0xd0, 0x4c, 0x04, 0x14,
    0xae, 0xff, 0x08, 0x87, 0x6b, 0x34, 0xab, 0x56,
    0xa1, 0xd4, 0x25, 0xa1, 0x22, 0x58, 0x33, 0x54,
    0x9a, 0xdb, 0x84, 0x1b, 0x51, 0xc9, 0xb3, 0x17,
    0x6a, 0x27, 0x2b, 0xde, 0xbb, 0xa1, 0xd0, 0x78,
    0x47, 0x8f, 0x62, 0xb3, 0x97, 0xf3, 0x3c, 0x8d
};

int main(int argc, char **argv)
{
    int ret = EXIT_FAILURE;
    EVP_KDF *kdf = NULL;
    EVP_KDF_CTX *kctx = NULL;
    unsigned char out[64];
    OSSL_PARAM params[5], *p = params;
    OSSL_LIB_CTX *library_context = NULL;

    library_context = OSSL_LIB_CTX_new();
    if (library_context == NULL) {
        fprintf(stderr, "OSSL_LIB_CTX_new() returned NULL\n");
        goto end;
    }

    /* Fetch the key derivation function implementation */
    kdf = EVP_KDF_fetch(library_context, "PBKDF2", NULL);
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

    /* Set password */
    *p++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_PASSWORD, password,
                                             sizeof(password));
    /* Set salt */
    *p++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT, pbkdf2_salt,
                                             sizeof(pbkdf2_salt));
    /* Set iteration count (default 2048) */
    *p++ = OSSL_PARAM_construct_uint(OSSL_KDF_PARAM_ITER, &pbkdf2_iterations);
    /* Set the underlying hash function used to derive the key */
    *p++ = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                            "SHA256", 0);
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

    ret = EXIT_SUCCESS;
end:
    EVP_KDF_CTX_free(kctx);
    EVP_KDF_free(kdf);
    OSSL_LIB_CTX_free(library_context);
    return ret;
}
