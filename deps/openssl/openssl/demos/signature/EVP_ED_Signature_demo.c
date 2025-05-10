/*-
 * Copyright 2023 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * This demonstration will calculate and verify an ED25519 signature of
 * a message using  EVP_DigestSign() and EVP_DigestVerify().
 */

#include <string.h>
#include <stdio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/core_names.h>

/* A test message to be signed (TBS) */
static const unsigned char hamlet[] =
    "To be, or not to be, that is the question,\n"
    "Whether tis nobler in the minde to suffer\n"
    "The slings and arrowes of outragious fortune,\n"
    "Or to take Armes again in a sea of troubles,\n";

static int demo_sign(EVP_PKEY *priv,
                     const unsigned char *tbs, size_t tbs_len,
                     OSSL_LIB_CTX *libctx,
                     unsigned char **sig_out_value,
                     size_t *sig_out_len)
{
    int ret = 0;
    size_t sig_len;
    unsigned char *sig_value = NULL;
    EVP_MD_CTX *sign_context = NULL;

    /* Create a signature context */
    sign_context = EVP_MD_CTX_new();
    if (sign_context == NULL) {
        fprintf(stderr, "EVP_MD_CTX_new failed.\n");
        goto cleanup;
    }

    /*
     * Initialize the sign context using an ED25519 private key
     * Notice that the digest name must NOT be used.
     * In this demo we don't specify any additional parameters via
     * OSSL_PARAM, which means it will use default values.
     * For more information, refer to doc/man7/EVP_SIGNATURE-ED25519.pod
     * "ED25519 and ED448 Signature Parameters"
     */
    if (!EVP_DigestSignInit_ex(sign_context, NULL, NULL, libctx, NULL, priv, NULL)) {
        fprintf(stderr, "EVP_DigestSignInit_ex failed.\n");
        goto cleanup;
    }

    /* Calculate the required size for the signature by passing a NULL buffer. */
    if (!EVP_DigestSign(sign_context, NULL, &sig_len, tbs, tbs_len)) {
        fprintf(stderr, "EVP_DigestSign using NULL buffer failed.\n");
        goto cleanup;
    }
    sig_value = OPENSSL_malloc(sig_len);
    if (sig_value == NULL) {
        fprintf(stderr, "OPENSSL_malloc failed.\n");
        goto cleanup;
    }
    fprintf(stdout, "Generating signature:\n");
    if (!EVP_DigestSign(sign_context, sig_value, &sig_len, tbs, tbs_len)) {
        fprintf(stderr, "EVP_DigestSign failed.\n");
        goto cleanup;
    }
    *sig_out_len = sig_len;
    *sig_out_value = sig_value;
    BIO_dump_indent_fp(stdout, sig_value, sig_len, 2);
    fprintf(stdout, "\n");
    ret = 1;

cleanup:
    if (!ret)
        OPENSSL_free(sig_value);
    EVP_MD_CTX_free(sign_context);
    return ret;
}

static int demo_verify(EVP_PKEY *pub,
                       const unsigned char *tbs, size_t tbs_len,
                       const unsigned char *sig_value, size_t sig_len,
                       OSSL_LIB_CTX *libctx)
{
    int ret = 0;
    EVP_MD_CTX *verify_context = NULL;

    /*
     * Make a verify signature context to hold temporary state
     * during signature verification
     */
    verify_context = EVP_MD_CTX_new();
    if (verify_context == NULL) {
        fprintf(stderr, "EVP_MD_CTX_new failed.\n");
        goto cleanup;
    }
    /* Initialize the verify context with a ED25519 public key */
    if (!EVP_DigestVerifyInit_ex(verify_context, NULL, NULL,
                                 libctx, NULL, pub, NULL)) {
        fprintf(stderr, "EVP_DigestVerifyInit_ex failed.\n");
        goto cleanup;
    }
    /*
     * ED25519 only supports the one shot interface using EVP_DigestVerify()
     * The streaming EVP_DigestVerifyUpdate() API is not supported.
     */
    if (!EVP_DigestVerify(verify_context, sig_value, sig_len,
                          tbs, tbs_len)) {
        fprintf(stderr, "EVP_DigestVerify() failed.\n");
        goto cleanup;
    }
    fprintf(stdout, "Signature verified.\n");
    ret = 1;

cleanup:
    EVP_MD_CTX_free(verify_context);
    return ret;
}

static int create_key(OSSL_LIB_CTX *libctx,
                      EVP_PKEY **privout, EVP_PKEY **pubout)
{
    int ret = 0;
    EVP_PKEY *priv = NULL, *pub = NULL;
    unsigned char pubdata[32];
    size_t pubdata_len = 0;

    /*
     * In this demo we just create a keypair, and extract the
     * public key. We could also use EVP_PKEY_new_raw_private_key_ex()
     * to create a key from raw data.
     */
    priv = EVP_PKEY_Q_keygen(libctx, NULL, "ED25519");
    if (priv == NULL) {
        fprintf(stderr, "EVP_PKEY_Q_keygen() failed\n");
        goto end;
    }

    if (!EVP_PKEY_get_octet_string_param(priv,
                                         OSSL_PKEY_PARAM_PUB_KEY,
                                         pubdata,
                                         sizeof(pubdata),
                                         &pubdata_len)) {
        fprintf(stderr, "EVP_PKEY_get_octet_string_param() failed\n");
        goto end;
    }
    pub = EVP_PKEY_new_raw_public_key_ex(libctx, "ED25519", NULL, pubdata, pubdata_len);
    if (pub == NULL) {
        fprintf(stderr, "EVP_PKEY_new_raw_public_key_ex() failed\n");
        goto end;
    }
    ret = 1;
end:
    if (ret) {
        *pubout = pub;
        *privout = priv;
    } else {
        EVP_PKEY_free(priv);
    }
    return ret;
}

int main(void)
{
    OSSL_LIB_CTX *libctx = NULL;
    size_t sig_len = 0;
    unsigned char *sig_value = NULL;
    int ret = EXIT_FAILURE;
    EVP_PKEY *priv = NULL, *pub = NULL;

    libctx = OSSL_LIB_CTX_new();
    if (libctx == NULL) {
        fprintf(stderr, "OSSL_LIB_CTX_new() returned NULL\n");
        goto cleanup;
    }
    if (!create_key(libctx, &priv, &pub)) {
        fprintf(stderr, "Failed to create key.\n");
        goto cleanup;
    }

    if (!demo_sign(priv, hamlet, sizeof(hamlet), libctx,
                   &sig_value, &sig_len)) {
        fprintf(stderr, "demo_sign failed.\n");
        goto cleanup;
    }
    if (!demo_verify(pub, hamlet, sizeof(hamlet),
                     sig_value, sig_len, libctx)) {
        fprintf(stderr, "demo_verify failed.\n");
        goto cleanup;
    }
    ret = EXIT_SUCCESS;

cleanup:
    if (ret != EXIT_SUCCESS)
        ERR_print_errors_fp(stderr);
    EVP_PKEY_free(pub);
    EVP_PKEY_free(priv);
    OSSL_LIB_CTX_free(libctx);
    OPENSSL_free(sig_value);
    return ret;
}
