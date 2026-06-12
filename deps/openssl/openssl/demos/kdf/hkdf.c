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
 * https://datatracker.ietf.org/doc/html/rfc5869
 */

static unsigned char hkdf_salt[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
    0x0c
};

static unsigned char hkdf_ikm[] = {
    0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
    0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b
};

static unsigned char hkdf_info[] = {
    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9
};

/* Expected output keying material */
static unsigned char hkdf_okm[] = {
    0x3c, 0xb2, 0x5f, 0x25, 0xfa, 0xac, 0xd5, 0x7a, 0x90, 0x43, 0x4f, 0x64,
    0xd0, 0x36, 0x2f, 0x2a, 0x2d, 0x2d, 0x0a, 0x90, 0xcf, 0x1a, 0x5a, 0x4c,
    0x5d, 0xb0, 0x2d, 0x56, 0xec, 0xc4, 0xc5, 0xbf, 0x34, 0x00, 0x72, 0x08,
    0xd5, 0xb8, 0x87, 0x18, 0x58, 0x65
};

int main(int argc, char **argv)
{
    int ret = EXIT_FAILURE;
    EVP_KDF *kdf = NULL;
    EVP_KDF_CTX *kctx = NULL;
    unsigned char out[42];
    OSSL_PARAM params[5], *p = params;
    OSSL_LIB_CTX *library_context = NULL;

    library_context = OSSL_LIB_CTX_new();
    if (library_context == NULL) {
        fprintf(stderr, "OSSL_LIB_CTX_new() returned NULL\n");
        goto end;
    }

    /* Fetch the key derivation function implementation */
    kdf = EVP_KDF_fetch(library_context, "HKDF", NULL);
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

    /* Set the underlying hash function used to derive the key */
    *p++ = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                            "SHA256", 0);
    /* Set input keying material */
    *p++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY, hkdf_ikm,
                                             sizeof(hkdf_ikm));
    /* Set application specific information */
    *p++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO, hkdf_info,
                                             sizeof(hkdf_info));
    /* Set salt */
    *p++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT, hkdf_salt,
                                             sizeof(hkdf_salt));
    *p = OSSL_PARAM_construct_end();

    /* Derive the key */
    if (EVP_KDF_derive(kctx, out, sizeof(out), params) != 1) {
        fprintf(stderr, "EVP_KDF_derive() failed\n");
        goto end;
    }

    if (CRYPTO_memcmp(hkdf_okm, out, sizeof(hkdf_okm)) != 0) {
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
