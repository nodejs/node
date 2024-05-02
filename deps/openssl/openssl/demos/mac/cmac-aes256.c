/*-
 * Copyright 2022 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * Example of using EVP_MAC_ methods to calculate
 * a CMAC of static buffers
 */

#include <string.h>
#include <stdio.h>
#include <openssl/crypto.h>
#include <openssl/core_names.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/cmac.h>
#include <openssl/params.h>

/*
 * Hard coding the key into an application is very bad.
 * It is done here solely for educational purposes.
 */
static unsigned char key[] = {
    0x6c, 0xde, 0x14, 0xf5, 0xd5, 0x2a, 0x4a, 0xdf,
    0x12, 0x39, 0x1e, 0xbf, 0x36, 0xf9, 0x6a, 0x46,
    0x48, 0xd0, 0xb6, 0x51, 0x89, 0xfc, 0x24, 0x85,
    0xa8, 0x8d, 0xdf, 0x7e, 0x80, 0x14, 0xc8, 0xce,
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

/* The known value of the CMAC/AES256 MAC of the above soliloqy */
static const unsigned char expected_output[] = {
    0x67, 0x92, 0x32, 0x23, 0x50, 0x3d, 0xc5, 0xba,
    0x78, 0xd4, 0x6d, 0x63, 0xf2, 0x2b, 0xe9, 0x56,
};

/*
 * A property query used for selecting the MAC implementation.
 */
static const char *propq = NULL;

int main(void)
{
    int rv = EXIT_FAILURE;
    OSSL_LIB_CTX *library_context = NULL;
    EVP_MAC *mac = NULL;
    EVP_MAC_CTX *mctx = NULL;
    unsigned char *out = NULL;
    size_t out_len = 0;
    OSSL_PARAM params[4], *p = params;
    char cipher_name[] = "aes256";

    library_context = OSSL_LIB_CTX_new();
    if (library_context == NULL) {
        fprintf(stderr, "OSSL_LIB_CTX_new() returned NULL\n");
        goto end;
    }

    /* Fetch the CMAC implementation */
    mac = EVP_MAC_fetch(library_context, "CMAC", propq);
    if (mac == NULL) {
        fprintf(stderr, "EVP_MAC_fetch() returned NULL\n");
        goto end;
    }

    /* Create a context for the CMAC operation */
    mctx = EVP_MAC_CTX_new(mac);
    if (mctx == NULL) {
        fprintf(stderr, "EVP_MAC_CTX_new() returned NULL\n");
        goto end;
    }

    /* The underlying cipher to be used */
    *p++ = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_CIPHER, cipher_name,
                                            sizeof(cipher_name));
    *p = OSSL_PARAM_construct_end();

    /* Initialise the CMAC operation */
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

    rv = EXIT_SUCCESS;
end:
    if (rv != EXIT_SUCCESS)
        ERR_print_errors_fp(stderr);
    /* OpenSSL free functions will ignore NULL arguments */
    OPENSSL_free(out);
    EVP_MAC_CTX_free(mctx);
    EVP_MAC_free(mac);
    OSSL_LIB_CTX_free(library_context);
    return rv;
}
