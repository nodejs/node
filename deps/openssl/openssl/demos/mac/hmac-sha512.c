/*-
 * Copyright 2022-2023 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * Example of using EVP_MAC_ methods to calculate
 * a HMAC of static buffers
 */

#include <string.h>
#include <stdio.h>
#include <openssl/crypto.h>
#include <openssl/core_names.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/params.h>

/*
 * Hard coding the key into an application is very bad.
 * It is done here solely for educational purposes.
 */
static unsigned char key[] = {
    0x25, 0xfd, 0x12, 0x99, 0xdf, 0xad, 0x1a, 0x03,
    0x0a, 0x81, 0x3c, 0x2d, 0xcc, 0x05, 0xd1, 0x5c,
    0x17, 0x7a, 0x36, 0x73, 0x17, 0xef, 0x41, 0x75,
    0x71, 0x18, 0xe0, 0x1a, 0xda, 0x99, 0xc3, 0x61,
    0x38, 0xb5, 0xb1, 0xe0, 0x82, 0x2c, 0x70, 0xa4,
    0xc0, 0x8e, 0x5e, 0xf9, 0x93, 0x9f, 0xcf, 0xf7,
    0x32, 0x4d, 0x0c, 0xbd, 0x31, 0x12, 0x0f, 0x9a,
    0x15, 0xee, 0x82, 0xdb, 0x8d, 0x29, 0x54, 0x14,
};

static const unsigned char data[] =
    "To be, or not to be, that is the question,\n"
    "Whether tis nobler in the minde to suffer\n"
    "The ſlings and arrowes of outragious fortune,\n"
    "Or to take Armes again in a sea of troubles,\n"
    "And by opposing, end them, to die to sleep;\n"
    "No more, and by a sleep, to say we end\n"
    "The heart-ache, and the thousand natural shocks\n"
    "That flesh is heir to? tis a consumation\n"
    "Devoutly to be wished. To die to sleep,\n"
    "To sleepe, perchance to dreame, Aye, there's the rub,\n"
    "For in that sleep of death what dreams may come\n"
    "When we haue shuffled off this mortal coil\n"
    "Must give us pause. There's the respect\n"
    "That makes calamity of so long life:\n"
    "For who would bear the Ships and Scorns of time,\n"
    "The oppressor's wrong, the proud man's Contumely,\n"
    "The pangs of dispised love, the Law's delay,\n"
;

/* The known value of the HMAC/SHA3-512 MAC of the above soliloqy */
static const unsigned char expected_output[] = {
    0x3b, 0x77, 0x5f, 0xf1, 0x4f, 0x9e, 0xb9, 0x23,
    0x8f, 0xdc, 0xa0, 0x68, 0x15, 0x7b, 0x8a, 0xf1,
    0x96, 0x23, 0xaa, 0x3c, 0x1f, 0xe9, 0xdc, 0x89,
    0x11, 0x7d, 0x58, 0x07, 0xe7, 0x96, 0x17, 0xe3,
    0x44, 0x8b, 0x03, 0x37, 0x91, 0xc0, 0x6e, 0x06,
    0x7c, 0x54, 0xe4, 0xa4, 0xcc, 0xd5, 0x16, 0xbb,
    0x5e, 0x4d, 0x64, 0x7d, 0x88, 0x23, 0xc9, 0xb7,
    0x25, 0xda, 0xbe, 0x4b, 0xe4, 0xd5, 0x34, 0x30,
};

/*
 * A property query used for selecting the MAC implementation.
 */
static const char *propq = NULL;

int main(void)
{
    int ret = EXIT_FAILURE;
    OSSL_LIB_CTX *library_context = NULL;
    EVP_MAC *mac = NULL;
    EVP_MAC_CTX *mctx = NULL;
    EVP_MD_CTX *digest_context = NULL;
    unsigned char *out = NULL;
    size_t out_len = 0;
    OSSL_PARAM params[4], *p = params;
    char digest_name[] = "SHA3-512";

    library_context = OSSL_LIB_CTX_new();
    if (library_context == NULL) {
        fprintf(stderr, "OSSL_LIB_CTX_new() returned NULL\n");
        goto end;
    }

    /* Fetch the HMAC implementation */
    mac = EVP_MAC_fetch(library_context, "HMAC", propq);
    if (mac == NULL) {
        fprintf(stderr, "EVP_MAC_fetch() returned NULL\n");
        goto end;
    }

    /* Create a context for the HMAC operation */
    mctx = EVP_MAC_CTX_new(mac);
    if (mctx == NULL) {
        fprintf(stderr, "EVP_MAC_CTX_new() returned NULL\n");
        goto end;
    }

    /* The underlying digest to be used */
    *p++ = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST, digest_name,
                                            sizeof(digest_name));
    *p = OSSL_PARAM_construct_end();

    /* Initialise the HMAC operation */
    if (!EVP_MAC_init(mctx, key, sizeof(key), params)) {
        fprintf(stderr, "EVP_MAC_init() failed\n");
        goto end;
    }

    /* Make one or more calls to process the data to be authenticated */
    if (!EVP_MAC_update(mctx, data, sizeof(data))) {
        fprintf(stderr, "EVP_MAC_update() failed\n");
        goto end;
    }

    /* Make a call to the final with a NULL buffer to get the length of the MAC */
    if (!EVP_MAC_final(mctx, NULL, &out_len, 0)) {
        fprintf(stderr, "EVP_MAC_final() failed\n");
        goto end;
    }
    out = OPENSSL_malloc(out_len);
    if (out == NULL) {
        fprintf(stderr, "malloc failed\n");
        goto end;
    }
    /* Make one call to the final to get the MAC */
    if (!EVP_MAC_final(mctx, out, &out_len, out_len)) {
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
    if (ret != EXIT_SUCCESS)
        ERR_print_errors_fp(stderr);
    /* OpenSSL free functions will ignore NULL arguments */
    OPENSSL_free(out);
    EVP_MD_CTX_free(digest_context);
    EVP_MAC_CTX_free(mctx);
    EVP_MAC_free(mac);
    OSSL_LIB_CTX_free(library_context);
    return ret;
}
