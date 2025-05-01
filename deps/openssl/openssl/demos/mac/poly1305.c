/*
 * Copyright 2021-2023 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/params.h>
#include <openssl/err.h>

/*
 * This is a demonstration of how to compute Poly1305-AES using the OpenSSL
 * Poly1305 and AES providers and the EVP API.
 *
 * Please note that:
 *
 *   - Poly1305 must never be used alone and must be used in conjunction with
 *     another primitive which processes the input nonce to be secure;
 *
 *   - you must never pass a nonce to the Poly1305 primitive directly;
 *
 *   - Poly1305 exhibits catastrophic failure (that is, can be broken) if a
 *     nonce is ever reused for a given key.
 *
 * If you are looking for a general purpose MAC, you should consider using a
 * different MAC and looking at one of the other examples, unless you have a
 * good familiarity with the details and caveats of Poly1305.
 *
 * This example uses AES, as described in the original paper, "The Poly1305-AES
 * message authentication code":
 *   https://cr.yp.to/mac/poly1305-20050329.pdf
 *
 * The test vectors below are from that paper.
 */

/*
 * Hard coding the key into an application is very bad.
 * It is done here solely for educational purposes.
 * These are the "r" and "k" inputs to Poly1305-AES.
 */
static const unsigned char test_r[] = {
    0x85, 0x1f, 0xc4, 0x0c, 0x34, 0x67, 0xac, 0x0b,
    0xe0, 0x5c, 0xc2, 0x04, 0x04, 0xf3, 0xf7, 0x00
};

static const unsigned char test_k[] = {
    0xec, 0x07, 0x4c, 0x83, 0x55, 0x80, 0x74, 0x17,
    0x01, 0x42, 0x5b, 0x62, 0x32, 0x35, 0xad, 0xd6
};

/*
 * Hard coding a nonce must not be done under any circumstances and is done here
 * purely for demonstration purposes. Please note that Poly1305 exhibits
 * catastrophic failure (that is, can be broken) if a nonce is ever reused for a
 * given key.
 */
static const unsigned char test_n[] = {
    0xfb, 0x44, 0x73, 0x50, 0xc4, 0xe8, 0x68, 0xc5,
    0x2a, 0xc3, 0x27, 0x5c, 0xf9, 0xd4, 0x32, 0x7e
};

/* Input message. */
static const unsigned char test_m[] = {
    0xf3, 0xf6
};

static const unsigned char expected_output[] = {
    0xf4, 0xc6, 0x33, 0xc3, 0x04, 0x4f, 0xc1, 0x45,
    0xf8, 0x4f, 0x33, 0x5c, 0xb8, 0x19, 0x53, 0xde
};

/*
 * A property query used for selecting the POLY1305 implementation.
 */
static char *propq = NULL;

int main(int argc, char **argv)
{
    int ret = EXIT_FAILURE;
    EVP_CIPHER *aes = NULL;
    EVP_CIPHER_CTX *aesctx = NULL;
    EVP_MAC *mac = NULL;
    EVP_MAC_CTX *mctx = NULL;
    unsigned char composite_key[32];
    unsigned char out[16];
    OSSL_LIB_CTX *library_context = NULL;
    size_t out_len = 0;
    int aes_len = 0;

    library_context = OSSL_LIB_CTX_new();
    if (library_context == NULL) {
        fprintf(stderr, "OSSL_LIB_CTX_new() returned NULL\n");
        goto end;
    }

    /* Fetch the Poly1305 implementation */
    mac = EVP_MAC_fetch(library_context, "POLY1305", propq);
    if (mac == NULL) {
        fprintf(stderr, "EVP_MAC_fetch() returned NULL\n");
        goto end;
    }

    /* Create a context for the Poly1305 operation */
    mctx = EVP_MAC_CTX_new(mac);
    if (mctx == NULL) {
        fprintf(stderr, "EVP_MAC_CTX_new() returned NULL\n");
        goto end;
    }

    /* Fetch the AES implementation */
    aes = EVP_CIPHER_fetch(library_context, "AES-128-ECB", propq);
    if (aes == NULL) {
        fprintf(stderr, "EVP_CIPHER_fetch() returned NULL\n");
        goto end;
    }

    /* Create a context for AES */
    aesctx = EVP_CIPHER_CTX_new();
    if (aesctx == NULL) {
        fprintf(stderr, "EVP_CIPHER_CTX_new() returned NULL\n");
        goto end;
    }

    /* Initialize the AES cipher with the 128-bit key k */
    if (!EVP_EncryptInit_ex(aesctx, aes, NULL, test_k, NULL)) {
        fprintf(stderr, "EVP_EncryptInit_ex() failed\n");
        goto end;
    }

    /*
     * Disable padding for the AES cipher. We do not strictly need to do this as
     * we are encrypting a single block and thus there are no alignment or
     * padding concerns, but this ensures that the operation below fails if
     * padding would be required for some reason, which in this circumstance
     * would indicate an implementation bug.
     */
    if (!EVP_CIPHER_CTX_set_padding(aesctx, 0)) {
        fprintf(stderr, "EVP_CIPHER_CTX_set_padding() failed\n");
        goto end;
    }

    /*
     * Computes the value AES_k(n) which we need for our Poly1305-AES
     * computation below.
     */
    if (!EVP_EncryptUpdate(aesctx, composite_key + 16, &aes_len,
                           test_n, sizeof(test_n))) {
        fprintf(stderr, "EVP_EncryptUpdate() failed\n");
        goto end;
    }

    /*
     * The Poly1305 provider expects the key r to be passed as the first 16
     * bytes of the "key" and the processed nonce (that is, AES_k(n)) to be
     * passed as the second 16 bytes of the "key". We already put the processed
     * nonce in the correct place above, so copy r into place.
     */
    memcpy(composite_key, test_r, 16);

    /* Initialise the Poly1305 operation */
    if (!EVP_MAC_init(mctx, composite_key, sizeof(composite_key), NULL)) {
        fprintf(stderr, "EVP_MAC_init() failed\n");
        goto end;
    }

    /* Make one or more calls to process the data to be authenticated */
    if (!EVP_MAC_update(mctx, test_m, sizeof(test_m))) {
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

    ret = EXIT_SUCCESS;
end:
    EVP_CIPHER_CTX_free(aesctx);
    EVP_CIPHER_free(aes);
    EVP_MAC_CTX_free(mctx);
    EVP_MAC_free(mac);
    OSSL_LIB_CTX_free(library_context);
    if (ret != EXIT_SUCCESS)
        ERR_print_errors_fp(stderr);
    return ret;
}
