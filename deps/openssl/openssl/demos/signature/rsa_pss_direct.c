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

/*
 * The digest to be signed. This should be the output of a hash function.
 * Here we sign an all-zeroes digest for demonstration purposes.
 */
static const unsigned char test_digest[32] = {0};

/* A property query used for selecting algorithm implementations. */
static const char *propq = NULL;

/*
 * This function demonstrates RSA signing of a SHA-256 digest using the PSS
 * padding scheme. You must already have hashed the data you want to sign.
 * For a higher-level demonstration which does the hashing for you, see
 * rsa_pss_hash.c.
 *
 * For more information, see RFC 8017 section 9.1. The digest passed in
 * (test_digest above) corresponds to the 'mHash' value.
 */
static int sign(OSSL_LIB_CTX *libctx, unsigned char **sig, size_t *sig_len)
{
    int ret = 0;
    EVP_PKEY *pkey = NULL;
    EVP_PKEY_CTX *ctx = NULL;
    EVP_MD *md = NULL;
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

    /* Fetch hash algorithm we want to use. */
    md = EVP_MD_fetch(libctx, "SHA256", propq);
    if (md == NULL) {
        fprintf(stderr, "Failed to fetch hash algorithm\n");
        goto end;
    }

    /* Create signing context. */
    ctx = EVP_PKEY_CTX_new_from_pkey(libctx, pkey, propq);
    if (ctx == NULL) {
        fprintf(stderr, "Failed to create signing context\n");
        goto end;
    }

    /* Initialize context for signing and set options. */
    if (EVP_PKEY_sign_init(ctx) == 0) {
        fprintf(stderr, "Failed to initialize signing context\n");
        goto end;
    }

    if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PSS_PADDING) == 0) {
        fprintf(stderr, "Failed to configure padding\n");
        goto end;
    }

    if (EVP_PKEY_CTX_set_signature_md(ctx, md) == 0) {
        fprintf(stderr, "Failed to configure digest type\n");
        goto end;
    }

    /* Determine length of signature. */
    if (EVP_PKEY_sign(ctx, NULL, sig_len,
                      test_digest, sizeof(test_digest)) == 0) {
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
    if (EVP_PKEY_sign(ctx, *sig, sig_len,
                      test_digest, sizeof(test_digest)) != 1) {
        fprintf(stderr, "Failed to sign\n");
        goto end;
    }

    ret = 1;
end:
    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    EVP_MD_free(md);

    if (ret == 0)
        OPENSSL_free(*sig);

    return ret;
}

/*
 * This function demonstrates verification of an RSA signature over a SHA-256
 * digest using the PSS signature scheme.
 */
static int verify(OSSL_LIB_CTX *libctx, const unsigned char *sig, size_t sig_len)
{
    int ret = 0;
    const unsigned char *ppub_key = NULL;
    EVP_PKEY *pkey = NULL;
    EVP_PKEY_CTX *ctx = NULL;
    EVP_MD *md = NULL;

    /* Load DER-encoded RSA public key. */
    ppub_key = rsa_pub_key;
    pkey = d2i_PublicKey(EVP_PKEY_RSA, NULL, &ppub_key, sizeof(rsa_pub_key));
    if (pkey == NULL) {
        fprintf(stderr, "Failed to load public key\n");
        goto end;
    }

    /* Fetch hash algorithm we want to use. */
    md = EVP_MD_fetch(libctx, "SHA256", propq);
    if (md == NULL) {
        fprintf(stderr, "Failed to fetch hash algorithm\n");
        goto end;
    }

    /* Create verification context. */
    ctx = EVP_PKEY_CTX_new_from_pkey(libctx, pkey, propq);
    if (ctx == NULL) {
        fprintf(stderr, "Failed to create verification context\n");
        goto end;
    }

    /* Initialize context for verification and set options. */
    if (EVP_PKEY_verify_init(ctx) == 0) {
        fprintf(stderr, "Failed to initialize verification context\n");
        goto end;
    }

    if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PSS_PADDING) == 0) {
        fprintf(stderr, "Failed to configure padding\n");
        goto end;
    }

    if (EVP_PKEY_CTX_set_signature_md(ctx, md) == 0) {
        fprintf(stderr, "Failed to configure digest type\n");
        goto end;
    }

    /* Verify signature. */
    if (EVP_PKEY_verify(ctx, sig, sig_len,
                        test_digest, sizeof(test_digest)) == 0) {
        fprintf(stderr, "Failed to verify signature; "
                "signature may be invalid\n");
        goto end;
    }

    ret = 1;
end:
    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    EVP_MD_free(md);
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
