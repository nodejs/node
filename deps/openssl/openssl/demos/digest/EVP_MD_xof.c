/*-
 * Copyright 2022-2024 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <string.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/core_names.h>

/*
 * Example of using an extendable-output hash function (XOF). A XOF is a hash
 * function with configurable output length and which can generate an
 * arbitrarily large output.
 *
 * This example uses SHAKE256, an extendable output variant of SHA3 (Keccak).
 *
 * To generate different output lengths, you can pass a single integer argument
 * on the command line, which is the output size in bytes. By default, a 20-byte
 * output is generated and (for this length only) a known answer test is
 * performed.
 */

/* Our input to the XOF hash function. */
static const char message[] = "This is a test message.";

/* Expected output when an output length of 20 bytes is used. */
static const unsigned char known_answer[] = {
  0x52, 0x97, 0x93, 0x78, 0x27, 0x58, 0x7d, 0x62,
  0x8b, 0x00, 0x25, 0xb5, 0xec, 0x39, 0x5e, 0x2d,
  0x7f, 0x3e, 0xd4, 0x19
};

/*
 * A property query used for selecting the SHAKE256 implementation.
 */
static const char *propq = NULL;

int main(int argc, char **argv)
{
    int ret = EXIT_FAILURE;
    OSSL_LIB_CTX *libctx = NULL;
    EVP_MD *md = NULL;
    EVP_MD_CTX *ctx = NULL;
    unsigned int digest_len = 20;
    int digest_len_i;
    unsigned char *digest = NULL;

    /* Allow digest length to be changed for demonstration purposes. */
    if (argc > 1) {
        digest_len_i = atoi(argv[1]);
        if (digest_len_i <= 0) {
            fprintf(stderr, "Specify a non-negative digest length\n");
            goto end;
        }

        digest_len = (unsigned int)digest_len_i;
    }

    /*
     * Retrieve desired algorithm. This must be a hash algorithm which supports
     * XOF.
     */
    md = EVP_MD_fetch(libctx, "SHAKE256", propq);
    if (md == NULL) {
        fprintf(stderr, "Failed to retrieve SHAKE256 algorithm\n");
        goto end;
    }

    /* Create context. */
    ctx = EVP_MD_CTX_new();
    if (ctx == NULL) {
        fprintf(stderr, "Failed to create digest context\n");
        goto end;
    }

    /* Initialize digest context. */
    if (EVP_DigestInit(ctx, md) == 0) {
        fprintf(stderr, "Failed to initialize digest\n");
        goto end;
    }

    /*
     * Feed our message into the digest function.
     * This may be called multiple times.
     */
    if (EVP_DigestUpdate(ctx, message, sizeof(message)) == 0) {
        fprintf(stderr, "Failed to hash input message\n");
        goto end;
    }

    /* Allocate enough memory for our digest length. */
    digest = OPENSSL_malloc(digest_len);
    if (digest == NULL) {
        fprintf(stderr, "Failed to allocate memory for digest\n");
        goto end;
    }

    /* Get computed digest. The digest will be of whatever length we specify. */
    if (EVP_DigestFinalXOF(ctx, digest, digest_len) == 0) {
        fprintf(stderr, "Failed to finalize hash\n");
        goto end;
    }

    printf("Output digest:\n");
    BIO_dump_indent_fp(stdout, digest, digest_len, 2);

    /* If digest length is 20 bytes, check it matches our known answer. */
    if (digest_len == 20) {
        /*
         * Always use a constant-time function such as CRYPTO_memcmp
         * when comparing cryptographic values. Do not use memcmp(3).
         */
        if (CRYPTO_memcmp(digest, known_answer, sizeof(known_answer)) != 0) {
            fprintf(stderr, "Output does not match expected result\n");
            goto end;
        }
    }

    ret = EXIT_SUCCESS;
end:
    OPENSSL_free(digest);
    EVP_MD_CTX_free(ctx);
    EVP_MD_free(md);
    OSSL_LIB_CTX_free(libctx);
    return ret;
}
