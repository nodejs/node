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
    'p', 'a', 's', 's', 'w', 'o', 'r', 'd'
};

/*
 * The salt is better not being hard coded too.  Each password should have a
 * different salt if possible.  The salt is not considered secret information
 * and is safe to store with an encrypted password.
 */
static unsigned char scrypt_salt[] = {
    'N', 'a', 'C', 'l'
};

/*
 * The SCRYPT parameters can be variable or hard coded.  The disadvantage with
 * hard coding them is that they cannot easily be adjusted for future
 * technological improvements appear.
 */
static unsigned int scrypt_n = 1024;
static unsigned int scrypt_r = 8;
static unsigned int scrypt_p = 16;

static const unsigned char expected_output[] = {

    0xfd, 0xba, 0xbe, 0x1c, 0x9d, 0x34, 0x72, 0x00,
    0x78, 0x56, 0xe7, 0x19, 0x0d, 0x01, 0xe9, 0xfe,
    0x7c, 0x6a, 0xd7, 0xcb, 0xc8, 0x23, 0x78, 0x30,
    0xe7, 0x73, 0x76, 0x63, 0x4b, 0x37, 0x31, 0x62,
    0x2e, 0xaf, 0x30, 0xd9, 0x2e, 0x22, 0xa3, 0x88,
    0x6f, 0xf1, 0x09, 0x27, 0x9d, 0x98, 0x30, 0xda,
    0xc7, 0x27, 0xaf, 0xb9, 0x4a, 0x83, 0xee, 0x6d,
    0x83, 0x60, 0xcb, 0xdf, 0xa2, 0xcc, 0x06, 0x40
};

int main(int argc, char **argv)
{
    int ret = EXIT_FAILURE;
    EVP_KDF *kdf = NULL;
    EVP_KDF_CTX *kctx = NULL;
    unsigned char out[64];
    OSSL_PARAM params[6], *p = params;
    OSSL_LIB_CTX *library_context = NULL;

    library_context = OSSL_LIB_CTX_new();
    if (library_context == NULL) {
        fprintf(stderr, "OSSL_LIB_CTX_new() returned NULL\n");
        goto end;
    }

    /* Fetch the key derivation function implementation */
    kdf = EVP_KDF_fetch(library_context, "SCRYPT", NULL);
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
    *p++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT, scrypt_salt,
                                             sizeof(scrypt_salt));
    /* Set N (default 1048576) */
    *p++ = OSSL_PARAM_construct_uint(OSSL_KDF_PARAM_SCRYPT_N, &scrypt_n);
    /* Set R (default 8) */
    *p++ = OSSL_PARAM_construct_uint(OSSL_KDF_PARAM_SCRYPT_R, &scrypt_r);
    /* Set P (default 1) */
    *p++ = OSSL_PARAM_construct_uint(OSSL_KDF_PARAM_SCRYPT_P, &scrypt_p);
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
