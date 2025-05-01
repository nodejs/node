/*-
 * Copyright 2021-2023 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * Example showing how to generate an EC key and extract values from the
 * generated key.
 */

#include <string.h>
#include <stdio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/core_names.h>

static int get_key_values(EVP_PKEY *pkey);

/*
 * The following code shows how to generate an EC key from a curve name
 * with additional parameters. If only the curve name is required then the
 * simple helper can be used instead i.e. Either
 * pkey = EVP_EC_gen(curvename); OR
 * pkey = EVP_PKEY_Q_keygen(libctx, propq, "EC", curvename);
 */
static EVP_PKEY *do_ec_keygen(void)
{
    /*
     * The libctx and propq can be set if required, they are included here
     * to show how they are passed to EVP_PKEY_CTX_new_from_name().
     */
    OSSL_LIB_CTX *libctx = NULL;
    const char *propq = NULL;
    EVP_PKEY *key = NULL;
    OSSL_PARAM params[3];
    EVP_PKEY_CTX *genctx = NULL;
    const char *curvename = "P-256";
    int use_cofactordh = 1;

    genctx = EVP_PKEY_CTX_new_from_name(libctx, "EC", propq);
    if (genctx == NULL) {
        fprintf(stderr, "EVP_PKEY_CTX_new_from_name() failed\n");
        goto cleanup;
    }

    if (EVP_PKEY_keygen_init(genctx) <= 0) {
        fprintf(stderr, "EVP_PKEY_keygen_init() failed\n");
        goto cleanup;
    }

    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME,
                                                 (char *)curvename, 0);
    /*
     * This is an optional parameter.
     * For many curves where the cofactor is 1, setting this has no effect.
     */
    params[1] = OSSL_PARAM_construct_int(OSSL_PKEY_PARAM_USE_COFACTOR_ECDH,
                                         &use_cofactordh);
    params[2] = OSSL_PARAM_construct_end();
    if (!EVP_PKEY_CTX_set_params(genctx, params)) {
        fprintf(stderr, "EVP_PKEY_CTX_set_params() failed\n");
        goto cleanup;
    }

    fprintf(stdout, "Generating EC key\n\n");
    if (EVP_PKEY_generate(genctx, &key) <= 0) {
        fprintf(stderr, "EVP_PKEY_generate() failed\n");
        goto cleanup;
    }
cleanup:
    EVP_PKEY_CTX_free(genctx);
    return key;
}

/*
 * The following code shows how retrieve key data from the generated
 * EC key. See doc/man7/EVP_PKEY-EC.pod for more information.
 *
 * EVP_PKEY_print_private() could also be used to display the values.
 */
static int get_key_values(EVP_PKEY *pkey)
{
    int ret = 0;
    char out_curvename[80];
    unsigned char out_pubkey[80];
    unsigned char out_privkey[80];
    BIGNUM *out_priv = NULL;
    size_t out_pubkey_len, out_privkey_len = 0;

    if (!EVP_PKEY_get_utf8_string_param(pkey, OSSL_PKEY_PARAM_GROUP_NAME,
                                        out_curvename, sizeof(out_curvename),
                                        NULL)) {
        fprintf(stderr, "Failed to get curve name\n");
        goto cleanup;
    }

    if (!EVP_PKEY_get_octet_string_param(pkey, OSSL_PKEY_PARAM_PUB_KEY,
                                        out_pubkey, sizeof(out_pubkey),
                                        &out_pubkey_len)) {
        fprintf(stderr, "Failed to get public key\n");
        goto cleanup;
    }

    if (!EVP_PKEY_get_bn_param(pkey, OSSL_PKEY_PARAM_PRIV_KEY, &out_priv)) {
        fprintf(stderr, "Failed to get private key\n");
        goto cleanup;
    }

    out_privkey_len = BN_bn2bin(out_priv, out_privkey);
    if (out_privkey_len <= 0 || out_privkey_len > sizeof(out_privkey)) {
        fprintf(stderr, "BN_bn2bin failed\n");
        goto cleanup;
    }

    fprintf(stdout, "Curve name: %s\n", out_curvename);
    fprintf(stdout, "Public key:\n");
    BIO_dump_indent_fp(stdout, out_pubkey, out_pubkey_len, 2);
    fprintf(stdout, "Private Key:\n");
    BIO_dump_indent_fp(stdout, out_privkey, out_privkey_len, 2);

    ret = 1;
cleanup:
    /* Zeroize the private key data when we free it */
    BN_clear_free(out_priv);
    return ret;
}

int main(void)
{
    int ret = EXIT_FAILURE;
    EVP_PKEY *pkey;

    pkey = do_ec_keygen();
    if (pkey == NULL)
        goto cleanup;

    if (!get_key_values(pkey))
        goto cleanup;

    /*
     * At this point we can write out the generated key using
     * i2d_PrivateKey() and i2d_PublicKey() if required.
     */
    ret = EXIT_SUCCESS;
cleanup:
    if (ret != EXIT_SUCCESS)
        ERR_print_errors_fp(stderr);

    EVP_PKEY_free(pkey);
    return ret;
}
