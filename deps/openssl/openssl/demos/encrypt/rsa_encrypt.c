/*-
 * Copyright 2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * An example that uses EVP_PKEY_encrypt and EVP_PKEY_decrypt methods
 * to encrypt and decrypt data using an RSA keypair.
 * RSA encryption produces different encrypted output each time it is run,
 * hence this is not a known answer test.
 */

#include <stdio.h>
#include <stdlib.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/decoder.h>
#include <openssl/core_names.h>
#include "rsa_encrypt.h"

/* Input data to encrypt */
static const unsigned char msg[] =
    "To be, or not to be, that is the question,\n"
    "Whether tis nobler in the minde to suffer\n"
    "The slings and arrowes of outragious fortune,\n"
    "Or to take Armes again in a sea of troubles";

/*
 * For do_encrypt(), load an RSA public key from pub_key_der[].
 * For do_decrypt(), load an RSA private key from priv_key_der[].
 */
static EVP_PKEY *get_key(OSSL_LIB_CTX *libctx, const char *propq, int public)
{
    OSSL_DECODER_CTX *dctx = NULL;
    EVP_PKEY *pkey = NULL;
    int selection;
    const unsigned char *data;
    size_t data_len;

    if (public) {
        selection = EVP_PKEY_PUBLIC_KEY;
        data = pub_key_der;
        data_len = sizeof(pub_key_der);
    } else {
        selection = EVP_PKEY_KEYPAIR;
        data = priv_key_der;
        data_len = sizeof(priv_key_der);
    }
    dctx = OSSL_DECODER_CTX_new_for_pkey(&pkey, "DER", NULL, "RSA",
                                         selection, libctx, propq);
    (void)OSSL_DECODER_from_data(dctx, &data, &data_len);
    OSSL_DECODER_CTX_free(dctx);
    return pkey;
}

/* Set optional parameters for RSA OAEP Padding */
static void set_optional_params(OSSL_PARAM *p, const char *propq)
{
    static unsigned char label[] = "label";

    /* "pkcs1" is used by default if the padding mode is not set */
    *p++ = OSSL_PARAM_construct_utf8_string(OSSL_ASYM_CIPHER_PARAM_PAD_MODE,
                                            OSSL_PKEY_RSA_PAD_MODE_OAEP, 0);
    /* No oaep_label is used if this is not set */
    *p++ = OSSL_PARAM_construct_octet_string(OSSL_ASYM_CIPHER_PARAM_OAEP_LABEL,
                                             label, sizeof(label));
    /* "SHA1" is used if this is not set */
    *p++ = OSSL_PARAM_construct_utf8_string(OSSL_ASYM_CIPHER_PARAM_OAEP_DIGEST,
                                            "SHA256", 0);
    /*
     * If a non default property query needs to be specified when fetching the
     * OAEP digest then it needs to be specified here.
     */
    if (propq != NULL)
        *p++ = OSSL_PARAM_construct_utf8_string(OSSL_ASYM_CIPHER_PARAM_OAEP_DIGEST_PROPS,
                                                (char *)propq, 0);

    /*
     * OSSL_ASYM_CIPHER_PARAM_MGF1_DIGEST and
     * OSSL_ASYM_CIPHER_PARAM_MGF1_DIGEST_PROPS can also be optionally added
     * here if the MGF1 digest differs from the OAEP digest.
     */

    *p = OSSL_PARAM_construct_end();
}

/*
 * The length of the input data that can be encrypted is limited by the
 * RSA key length minus some additional bytes that depends on the padding mode.
 *
 */
static int do_encrypt(OSSL_LIB_CTX *libctx,
                      const unsigned char *in, size_t in_len,
                      unsigned char **out, size_t *out_len)
{
    int ret = 0, public = 1;
    size_t buf_len = 0;
    unsigned char *buf = NULL;
    const char *propq = NULL;
    EVP_PKEY_CTX *ctx = NULL;
    EVP_PKEY *pub_key = NULL;
    OSSL_PARAM params[5];

    /* Get public key */
    pub_key = get_key(libctx, propq, public);
    if (pub_key == NULL) {
        fprintf(stderr, "Get public key failed.\n");
        goto cleanup;
    }
    ctx = EVP_PKEY_CTX_new_from_pkey(libctx, pub_key, propq);
    if (ctx == NULL) {
        fprintf(stderr, "EVP_PKEY_CTX_new_from_pkey() failed.\n");
        goto cleanup;
    }
    set_optional_params(params, propq);
    /* If no optional parameters are required then NULL can be passed */
    if (EVP_PKEY_encrypt_init_ex(ctx, params) <= 0) {
        fprintf(stderr, "EVP_PKEY_encrypt_init_ex() failed.\n");
        goto cleanup;
    }
    /* Calculate the size required to hold the encrypted data */
    if (EVP_PKEY_encrypt(ctx, NULL, &buf_len, in, in_len) <= 0) {
        fprintf(stderr, "EVP_PKEY_encrypt() failed.\n");
        goto cleanup;
    }
    buf = OPENSSL_zalloc(buf_len);
    if (buf  == NULL) {
        fprintf(stderr, "Malloc failed.\n");
        goto cleanup;
    }
    if (EVP_PKEY_encrypt(ctx, buf, &buf_len, in, in_len) <= 0) {
        fprintf(stderr, "EVP_PKEY_encrypt() failed.\n");
        goto cleanup;
    }
    *out_len = buf_len;
    *out = buf;
    fprintf(stdout, "Encrypted:\n");
    BIO_dump_indent_fp(stdout, buf, buf_len, 2);
    fprintf(stdout, "\n");
    ret = 1;

cleanup:
    if (!ret)
        OPENSSL_free(buf);
    EVP_PKEY_free(pub_key);
    EVP_PKEY_CTX_free(ctx);
    return ret;
}

static int do_decrypt(OSSL_LIB_CTX *libctx, const unsigned char *in, size_t in_len,
                      unsigned char **out, size_t *out_len)
{
    int ret = 0, public = 0;
    size_t buf_len = 0;
    unsigned char *buf = NULL;
    const char *propq = NULL;
    EVP_PKEY_CTX *ctx = NULL;
    EVP_PKEY *priv_key = NULL;
    OSSL_PARAM params[5];

    /* Get private key */
    priv_key = get_key(libctx, propq, public);
    if (priv_key == NULL) {
        fprintf(stderr, "Get private key failed.\n");
        goto cleanup;
    }
    ctx = EVP_PKEY_CTX_new_from_pkey(libctx, priv_key, propq);
    if (ctx == NULL) {
        fprintf(stderr, "EVP_PKEY_CTX_new_from_pkey() failed.\n");
        goto cleanup;
    }

    /* The parameters used for encryption must also be used for decryption */
    set_optional_params(params, propq);
    /* If no optional parameters are required then NULL can be passed */
    if (EVP_PKEY_decrypt_init_ex(ctx, params) <= 0) {
        fprintf(stderr, "EVP_PKEY_decrypt_init_ex() failed.\n");
        goto cleanup;
    }
    /* Calculate the size required to hold the decrypted data */
    if (EVP_PKEY_decrypt(ctx, NULL, &buf_len, in, in_len) <= 0) {
        fprintf(stderr, "EVP_PKEY_decrypt() failed.\n");
        goto cleanup;
    }
    buf = OPENSSL_zalloc(buf_len);
    if (buf == NULL) {
        fprintf(stderr, "Malloc failed.\n");
        goto cleanup;
    }
    if (EVP_PKEY_decrypt(ctx, buf, &buf_len, in, in_len) <= 0) {
        fprintf(stderr, "EVP_PKEY_decrypt() failed.\n");
        goto cleanup;
    }
    *out_len = buf_len;
    *out = buf;
    fprintf(stdout, "Decrypted:\n");
    BIO_dump_indent_fp(stdout, buf, buf_len, 2);
    fprintf(stdout, "\n");
    ret = 1;

cleanup:
    if (!ret)
        OPENSSL_free(buf);
    EVP_PKEY_free(priv_key);
    EVP_PKEY_CTX_free(ctx);
    return ret;
}

int main(void)
{
    int ret = EXIT_FAILURE;
    size_t msg_len = sizeof(msg) - 1;
    size_t encrypted_len = 0, decrypted_len = 0;
    unsigned char *encrypted = NULL, *decrypted = NULL;
    OSSL_LIB_CTX *libctx = NULL;

    if (!do_encrypt(libctx, msg, msg_len, &encrypted, &encrypted_len)) {
        fprintf(stderr, "encryption failed.\n");
        goto cleanup;
    }
    if (!do_decrypt(libctx, encrypted, encrypted_len,
                    &decrypted, &decrypted_len)) {
        fprintf(stderr, "decryption failed.\n");
        goto cleanup;
    }
    if (CRYPTO_memcmp(msg, decrypted, decrypted_len) != 0) {
        fprintf(stderr, "Decrypted data does not match expected value\n");
        goto cleanup;
    }
    ret = EXIT_SUCCESS;

cleanup:
    OPENSSL_free(decrypted);
    OPENSSL_free(encrypted);
    OSSL_LIB_CTX_free(libctx);
    if (ret != EXIT_SUCCESS)
        ERR_print_errors_fp(stderr);
    return ret;
}
