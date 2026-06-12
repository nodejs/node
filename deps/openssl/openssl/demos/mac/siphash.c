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
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/params.h>
#include <openssl/err.h>

/*
 * Taken from the test vector from the paper "SipHash: a fast short-input PRF".
 * https://www.aumasson.jp/siphash/siphash.pdf
 */

/*
 * Hard coding the key into an application is very bad.
 * It is done here solely for educational purposes.
 */
static unsigned char key[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
};

static unsigned char data[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e
};

static const unsigned char expected_output[] = {
    0xe5, 0x45, 0xbe, 0x49, 0x61, 0xca, 0x29, 0xa1
};

/*
 * A property query used for selecting the SIPHASH implementation.
 */
static char *propq = NULL;

int main(int argc, char **argv)
{
    int ret = EXIT_FAILURE;
    EVP_MAC *mac = NULL;
    EVP_MAC_CTX *mctx = NULL;
    unsigned char out[8];
    OSSL_PARAM params[4], *p = params;
    OSSL_LIB_CTX *library_context = NULL;
    unsigned int digest_len = 8, c_rounds = 2, d_rounds = 4;
    size_t out_len = 0;

    library_context = OSSL_LIB_CTX_new();
    if (library_context == NULL) {
        fprintf(stderr, "OSSL_LIB_CTX_new() returned NULL\n");
        goto end;
    }

    /* Fetch the SipHash implementation */
    mac = EVP_MAC_fetch(library_context, "SIPHASH", propq);
    if (mac == NULL) {
        fprintf(stderr, "EVP_MAC_fetch() returned NULL\n");
        goto end;
    }

    /* Create a context for the SipHash operation */
    mctx = EVP_MAC_CTX_new(mac);
    if (mctx == NULL) {
        fprintf(stderr, "EVP_MAC_CTX_new() returned NULL\n");
        goto end;
    }

    /* SipHash can support either 8 or 16-byte digests. */
    *p++ = OSSL_PARAM_construct_uint(OSSL_MAC_PARAM_SIZE, &digest_len);

    /*
     * The number of C-rounds and D-rounds is configurable. Standard SipHash
     * uses values of 2 and 4 respectively. The following lines are unnecessary
     * as they set the default, but demonstrate how to change these values.
     */
    *p++ = OSSL_PARAM_construct_uint(OSSL_MAC_PARAM_C_ROUNDS, &c_rounds);
    *p++ = OSSL_PARAM_construct_uint(OSSL_MAC_PARAM_D_ROUNDS, &d_rounds);

    *p = OSSL_PARAM_construct_end();

    /* Initialise the SIPHASH operation */
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

    ret = EXIT_SUCCESS;
end:
    EVP_MAC_CTX_free(mctx);
    EVP_MAC_free(mac);
    OSSL_LIB_CTX_free(library_context);
    if (ret != EXIT_SUCCESS)
        ERR_print_errors_fp(stderr);
    return ret;
}
