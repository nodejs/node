/*
 * Copyright 2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <stdlib.h>
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/params.h>
#include <openssl/err.h>

/*
 * Taken from NIST's GCM Test Vectors
 * http://csrc.nist.gov/groups/STM/cavp/
 */

/*
 * Hard coding the key into an application is very bad.
 * It is done here solely for educational purposes.
 */
static unsigned char key[] = {
    0x77, 0xbe, 0x63, 0x70, 0x89, 0x71, 0xc4, 0xe2,
    0x40, 0xd1, 0xcb, 0x79, 0xe8, 0xd7, 0x7f, 0xeb
};

/*
 * The initialisation vector (IV) is better not being hard coded too.
 * Repeating password/IV pairs compromises the integrity of GMAC.
 * The IV is not considered secret information and is safe to store with
 * an encrypted password.
 */
static unsigned char iv[] = {
    0xe0, 0xe0, 0x0f, 0x19, 0xfe, 0xd7, 0xba,
    0x01, 0x36, 0xa7, 0x97, 0xf3
};

static unsigned char data[] = {
    0x7a, 0x43, 0xec, 0x1d, 0x9c, 0x0a, 0x5a, 0x78,
    0xa0, 0xb1, 0x65, 0x33, 0xa6, 0x21, 0x3c, 0xab
};

static const unsigned char expected_output[] = {
    0x20, 0x9f, 0xcc, 0x8d, 0x36, 0x75, 0xed, 0x93,
    0x8e, 0x9c, 0x71, 0x66, 0x70, 0x9d, 0xd9, 0x46
};

/*
 * A property query used for selecting the GMAC implementation and the
 * underlying GCM mode cipher.
 */
static char *propq = NULL;

int main(int argc, char **argv)
{
    int rv = EXIT_FAILURE;
    EVP_MAC *mac = NULL;
    EVP_MAC_CTX *mctx = NULL;
    unsigned char out[16];
    OSSL_PARAM params[4], *p = params;
    OSSL_LIB_CTX *library_context = NULL;
    size_t out_len = 0;

    library_context = OSSL_LIB_CTX_new();
    if (library_context == NULL) {
        fprintf(stderr, "OSSL_LIB_CTX_new() returned NULL\n");
        goto end;
    }

    /* Fetch the GMAC implementation */
    mac = EVP_MAC_fetch(library_context, "GMAC", propq);
    if (mac == NULL) {
        fprintf(stderr, "EVP_MAC_fetch() returned NULL\n");
        goto end;
    }

    /* Create a context for the GMAC operation */
    mctx = EVP_MAC_CTX_new(mac);
    if (mctx == NULL) {
        fprintf(stderr, "EVP_MAC_CTX_new() returned NULL\n");
        goto end;
    }

    /* GMAC requries a GCM mode cipher to be specified */
    *p++ = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_CIPHER,
                                            "AES-128-GCM", 0);

    /*
     * If a non-default property query is required when fetching the GCM mode
     * cipher, it needs to be specified too.
     */
    if (propq != NULL)
        *p++ = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_PROPERTIES,
                                                propq, 0);

    /* Set the initialisation vector (IV) */
    *p++ = OSSL_PARAM_construct_octet_string(OSSL_CIPHER_PARAM_IV,
                                             iv, sizeof(iv));
    *p = OSSL_PARAM_construct_end();

    /* Initialise the GMAC operation */
    if (!EVP_MAC_init(mctx, key, sizeof(key), params)) {
        fprintf(stderr, "EVP_MAC_init() failed\n");
        goto end;
    }

    /* Make one or more calls to process the data to be authenticated */
    if (!EVP_MAC_update(mctx, data, sizeof(data))) {
        fprintf(stderr, "EVP_MAC_update() failed\n");
        goto end;
    }

    /* Make one call to the final to get the MAC */
    if (!EVP_MAC_final(mctx, out, &out_len, sizeof(out))) {
        fprintf(stderr, "EVP_MAC_final() failed\n");
        goto end;
    }

    printf("Generated MAC:\n");
    BIO_dump_indent_fp(stdout, out, out_len, 2);
    putchar('\n');

    if (out_len != sizeof(expected_output)) {
        fprintf(stderr, "Generated MAC has an unexpected length\n");
        goto end;
    }

    if (CRYPTO_memcmp(expected_output, out, sizeof(expected_output)) != 0) {
        fprintf(stderr, "Generated MAC does not match expected value\n");
        goto end;
    }

    rv = EXIT_SUCCESS;
end:
    EVP_MAC_CTX_free(mctx);
    EVP_MAC_free(mac);
    OSSL_LIB_CTX_free(library_context);
    if (rv != EXIT_SUCCESS)
        ERR_print_errors_fp(stderr);
    return rv;
}
