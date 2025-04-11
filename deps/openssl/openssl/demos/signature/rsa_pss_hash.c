/*
 * Copyright 2022-2023 The OpenSSL Project Authors. All Rights Reserved.
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
#include <openssl/rsa.h>
#include <openssl/params.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include "rsa_pss.h"

/* The data to be signed. This will be hashed. */
static const char test_message[] =
    "This is an example message to be signed.";

/* A property query used for selecting algorithm implementations. */
static const char *propq = NULL;

/*
 * This function demonstrates RSA signing of an arbitrary-length message.
 * Hashing is performed automatically. In this example, SHA-256 is used. If you
 * have already hashed your message and simply want to sign the hash directly,
 * see rsa_pss_direct.c.
 */
static int sign(OSSL_LIB_CTX *libctx, unsigned char **sig, size_t *sig_len)
{
    int ret = 0;
    EVP_PKEY *pkey = NULL;
    EVP_MD_CTX *mctx = NULL;
    OSSL_PARAM params[2], *p = params;
    const unsigned char *ppriv_key = NULL;

    *sig = NULL;

    /* Load DER-encoded RSA private key. */
    ppriv_key = rsa_priv_key;
    pkey = d2i_PrivateKey_ex(EVP_PKEY_RSA, NULL, &ppriv_key,
                             sizeof(rsa_priv_key), libctx, propq);
    if (pkey == NULL) {
        fprintf(stderr, "Failed to load private key\n");
        goto end;
    }

    /* Create MD context used for signing. */
    mctx = EVP_MD_CTX_new();
    if (mctx == NULL) {
        fprintf(stderr, "Failed to create MD context\n");
        goto end;
    }

    /* Initialize MD context for signing. */
    *p++ = OSSL_PARAM_construct_utf8_string(OSSL_SIGNATURE_PARAM_PAD_MODE,
                                            OSSL_PKEY_RSA_PAD_MODE_PSS, 0);
    *p = OSSL_PARAM_construct_end();

    if (EVP_DigestSignInit_ex(mctx, NULL, "SHA256", libctx, propq,
                              pkey, params) == 0) {
        fprintf(stderr, "Failed to initialize signing context\n");
        goto end;
    }

    /*
     * Feed data to be signed into the algorithm. This may
     * be called multiple times.
     */
    if (EVP_DigestSignUpdate(mctx, test_message, sizeof(test_message)) == 0) {
        fprintf(stderr, "Failed to hash message into signing context\n");
        goto end;
    }

    /* Determine signature length. */
    if (EVP_DigestSignFinal(mctx, NULL, sig_len) == 0) {
        fprintf(stderr, "Failed to get signature length\n");
        goto end;
    }

    /* Allocate memory for signature. */
    *sig = OPENSSL_malloc(*sig_len);
    if (*sig == NULL) {
        fprintf(stderr, "Failed to allocate memory for signature\n");
        goto end;
    }

    /* Generate signature. */
    if (EVP_DigestSignFinal(mctx, *sig, sig_len) == 0) {
        fprintf(stderr, "Failed to sign\n");
        goto end;
    }

    ret = 1;
end:
    EVP_MD_CTX_free(mctx);
    EVP_PKEY_free(pkey);

    if (ret == 0)
        OPENSSL_free(*sig);

    return ret;
}

/*
 * This function demonstrates verification of an RSA signature over an
 * arbitrary-length message using the PSS signature scheme. Hashing is performed
 * automatically.
 */
static int verify(OSSL_LIB_CTX *libctx, const unsigned char *sig, size_t sig_len)
{
    int ret = 0;
    EVP_PKEY *pkey = NULL;
    EVP_MD_CTX *mctx = NULL;
    OSSL_PARAM params[2], *p = params;
    const unsigned char *ppub_key = NULL;

    /* Load DER-encoded RSA public key. */
    ppub_key = rsa_pub_key;
    pkey = d2i_PublicKey(EVP_PKEY_RSA, NULL, &ppub_key, sizeof(rsa_pub_key));
    if (pkey == NULL) {
        fprintf(stderr, "Failed to load public key\n");
        goto end;
    }

    /* Create MD context used for verification. */
    mctx = EVP_MD_CTX_new();
    if (mctx == NULL) {
        fprintf(stderr, "Failed to create MD context\n");
        goto end;
    }

    /* Initialize MD context for verification. */
    *p++ = OSSL_PARAM_construct_utf8_string(OSSL_SIGNATURE_PARAM_PAD_MODE,
                                            OSSL_PKEY_RSA_PAD_MODE_PSS, 0);
    *p = OSSL_PARAM_construct_end();

    if (EVP_DigestVerifyInit_ex(mctx, NULL, "SHA256", libctx, propq,
                                pkey, params) == 0) {
        fprintf(stderr, "Failed to initialize signing context\n");
        goto end;
    }

    /*
     * Feed data to be signed into the algorithm. This may
     * be called multiple times.
     */
    if (EVP_DigestVerifyUpdate(mctx, test_message, sizeof(test_message)) == 0) {
        fprintf(stderr, "Failed to hash message into signing context\n");
        goto end;
    }

    /* Verify signature. */
    if (EVP_DigestVerifyFinal(mctx, sig, sig_len) == 0) {
        fprintf(stderr, "Failed to verify signature; "
                "signature may be invalid\n");
        goto end;
    }

    ret = 1;
end:
    EVP_MD_CTX_free(mctx);
    EVP_PKEY_free(pkey);
    return ret;
}

int main(int argc, char **argv)
{
    int ret = EXIT_FAILURE;
    OSSL_LIB_CTX *libctx = NULL;
    unsigned char *sig = NULL;
    size_t sig_len = 0;

    if (sign(libctx, &sig, &sig_len) == 0)
        goto end;

    if (verify(libctx, sig, sig_len) == 0)
        goto end;

    printf("Success\n");

    ret = EXIT_SUCCESS;
end:
    OPENSSL_free(sig);
    OSSL_LIB_CTX_free(libctx);
    return ret;
}
