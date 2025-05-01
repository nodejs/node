/*-
 * Copyright 2022-2023 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * An example that uses the EVP_PKEY*, EVP_DigestSign* and EVP_DigestVerify*
 * methods to calculate public/private DSA keypair and to sign and verify
 * two static buffers.
 */

#include <string.h>
#include <stdio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/decoder.h>
#include <openssl/dsa.h>

/*
 * This demonstration will calculate and verify a signature of data using
 * the soliloquy from Hamlet scene 1 act 3
 */

static const char *hamlet_1 =
    "To be, or not to be, that is the question,\n"
    "Whether tis nobler in the minde to suffer\n"
    "The slings and arrowes of outragious fortune,\n"
    "Or to take Armes again in a sea of troubles,\n"
;
static const char *hamlet_2 =
    "And by opposing, end them, to die to sleep;\n"
    "No more, and by a sleep, to say we end\n"
    "The heart-ache, and the thousand natural shocks\n"
    "That flesh is heir to? tis a consumation\n"
;

static const char ALG[] = "DSA";
static const char DIGEST[] = "SHA256";
static const int NUMBITS = 2048;
static const char * const PROPQUERY = NULL;

static int generate_dsa_params(OSSL_LIB_CTX *libctx,
                               EVP_PKEY **p_params)
{
    int ret = 0;

    EVP_PKEY_CTX *pkey_ctx = NULL;
    EVP_PKEY *params = NULL;

    pkey_ctx = EVP_PKEY_CTX_new_from_name(libctx, ALG, PROPQUERY);
    if (pkey_ctx == NULL)
        goto end;

    if (EVP_PKEY_paramgen_init(pkey_ctx) <= 0)
        goto end;

    if (EVP_PKEY_CTX_set_dsa_paramgen_bits(pkey_ctx, NUMBITS) <= 0)
        goto end;
    if (EVP_PKEY_paramgen(pkey_ctx, &params) <= 0)
        goto end;
    if (params == NULL)
        goto end;

    ret = 1;
end:
    if(ret != 1) {
        EVP_PKEY_free(params);
        params = NULL;
    }
    EVP_PKEY_CTX_free(pkey_ctx);
    *p_params = params;
    fprintf(stdout, "Params:\n");
    EVP_PKEY_print_params_fp(stdout, params, 4, NULL);
    fprintf(stdout, "\n");

    return ret;
}

static int generate_dsa_key(OSSL_LIB_CTX *libctx,
                            EVP_PKEY *params,
                            EVP_PKEY **p_pkey)
{
    int ret = 0;

    EVP_PKEY_CTX *ctx = NULL;
    EVP_PKEY *pkey = NULL;

    ctx = EVP_PKEY_CTX_new_from_pkey(libctx, params,
                                     NULL);
    if (ctx == NULL)
        goto end;
    if (EVP_PKEY_keygen_init(ctx) <= 0)
        goto end;

    if (EVP_PKEY_keygen(ctx, &pkey) <= 0)
        goto end;
    if (pkey == NULL)
        goto end;

    ret = 1;
end:
    if(ret != 1) {
        EVP_PKEY_free(pkey);
        pkey = NULL;
    }
    EVP_PKEY_CTX_free(ctx);
    *p_pkey = pkey;
    fprintf(stdout, "Generating public/private key pair:\n");
    EVP_PKEY_print_public_fp(stdout, pkey, 4, NULL);
    fprintf(stdout, "\n");
    EVP_PKEY_print_private_fp(stdout, pkey, 4, NULL);
    fprintf(stdout, "\n");
    EVP_PKEY_print_params_fp(stdout, pkey, 4, NULL);
    fprintf(stdout, "\n");

    return ret;
}

static int extract_public_key(const EVP_PKEY *pkey,
                              OSSL_PARAM **p_public_key)
{
    int ret = 0;
    OSSL_PARAM *public_key = NULL;

    if (EVP_PKEY_todata(pkey, EVP_PKEY_PUBLIC_KEY, &public_key) != 1)
        goto end;

    ret = 1;
end:
    if (ret != 1) {
        OSSL_PARAM_free(public_key);
        public_key = NULL;
    }
    *p_public_key = public_key;

    return ret;
}

static int extract_keypair(const EVP_PKEY *pkey,
                           OSSL_PARAM **p_keypair)
{
    int ret = 0;
    OSSL_PARAM *keypair = NULL;

    if (EVP_PKEY_todata(pkey, EVP_PKEY_KEYPAIR, &keypair) != 1)
        goto end;

    ret = 1;
end:
    if (ret != 1) {
        OSSL_PARAM_free(keypair);
        keypair = NULL;
    }
    *p_keypair = keypair;

    return ret;
}

static int demo_sign(OSSL_LIB_CTX *libctx,
                     size_t *p_sig_len, unsigned char **p_sig_value,
                     OSSL_PARAM keypair[])
{
    int ret = 0;
    size_t sig_len = 0;
    unsigned char *sig_value = NULL;
    EVP_MD_CTX *ctx = NULL;
    EVP_PKEY_CTX *pkey_ctx = NULL;
    EVP_PKEY *pkey = NULL;

    pkey_ctx = EVP_PKEY_CTX_new_from_name(libctx, ALG, PROPQUERY);
    if (pkey_ctx == NULL)
        goto end;
    if (EVP_PKEY_fromdata_init(pkey_ctx) != 1)
        goto end;
    if (EVP_PKEY_fromdata(pkey_ctx, &pkey, EVP_PKEY_KEYPAIR, keypair) != 1)
        goto end;

    ctx = EVP_MD_CTX_create();
    if (ctx == NULL)
        goto end;

    if (EVP_DigestSignInit_ex(ctx, NULL, DIGEST, libctx, NULL, pkey, NULL) != 1)
        goto end;

    if (EVP_DigestSignUpdate(ctx, hamlet_1, sizeof(hamlet_1)) != 1)
        goto end;

    if (EVP_DigestSignUpdate(ctx, hamlet_2, sizeof(hamlet_2)) != 1)
        goto end;

    /* Calculate the signature size */
    if (EVP_DigestSignFinal(ctx, NULL, &sig_len) != 1)
        goto end;
    if (sig_len == 0)
        goto end;

    sig_value = OPENSSL_malloc(sig_len);
    if (sig_value == NULL)
        goto end;

    /* Calculate the signature */
    if (EVP_DigestSignFinal(ctx, sig_value, &sig_len) != 1)
        goto end;

    ret = 1;
end:
    EVP_MD_CTX_free(ctx);
    if (ret != 1) {
        OPENSSL_free(sig_value);
        sig_len = 0;
        sig_value = NULL;
    }
    *p_sig_len = sig_len;
    *p_sig_value = sig_value;
    EVP_PKEY_free(pkey);
    EVP_PKEY_CTX_free(pkey_ctx);

    fprintf(stdout, "Generating signature:\n");
    BIO_dump_indent_fp(stdout, sig_value, sig_len, 2);
    fprintf(stdout, "\n");
    return ret;
}

static int demo_verify(OSSL_LIB_CTX *libctx,
                       size_t sig_len, unsigned char *sig_value,
                       OSSL_PARAM public_key[])
{
    int ret = 0;
    EVP_MD_CTX *ctx = NULL;
    EVP_PKEY_CTX *pkey_ctx = NULL;
    EVP_PKEY *pkey = NULL;

    pkey_ctx = EVP_PKEY_CTX_new_from_name(libctx, ALG, PROPQUERY);
    if (pkey_ctx == NULL)
        goto end;
    if (EVP_PKEY_fromdata_init(pkey_ctx) != 1)
        goto end;
    if (EVP_PKEY_fromdata(pkey_ctx, &pkey, EVP_PKEY_PUBLIC_KEY, public_key) != 1)
        goto end;

    ctx = EVP_MD_CTX_create();
    if(ctx == NULL)
        goto end;

    if (EVP_DigestVerifyInit_ex(ctx, NULL, DIGEST, libctx, NULL, pkey, NULL) != 1)
        goto end;

    if (EVP_DigestVerifyUpdate(ctx, hamlet_1, sizeof(hamlet_1)) != 1)
        goto end;

    if (EVP_DigestVerifyUpdate(ctx, hamlet_2, sizeof(hamlet_2)) != 1)
        goto end;

    if (EVP_DigestVerifyFinal(ctx, sig_value, sig_len) != 1)
        goto end;

    ret = 1;
end:
    EVP_PKEY_free(pkey);
    EVP_PKEY_CTX_free(pkey_ctx);
    EVP_MD_CTX_free(ctx);
    return ret;
}

int main(void)
{
    int ret = EXIT_FAILURE;
    OSSL_LIB_CTX *libctx = NULL;
    EVP_PKEY *params = NULL;
    EVP_PKEY *pkey = NULL;
    OSSL_PARAM *public_key = NULL;
    OSSL_PARAM *keypair = NULL;
    size_t sig_len = 0;
    unsigned char *sig_value = NULL;

    libctx = OSSL_LIB_CTX_new();
    if (libctx == NULL)
        goto end;

    if (generate_dsa_params(libctx, &params) != 1)
        goto end;

    if (generate_dsa_key(libctx, params, &pkey) != 1)
        goto end;

    if (extract_public_key(pkey, &public_key) != 1)
        goto end;

    if (extract_keypair(pkey, &keypair) != 1)
        goto end;

    /* The signer signs with his private key, and distributes his public key */
    if (demo_sign(libctx, &sig_len, &sig_value, keypair) != 1)
        goto end;

    /* A verifier uses the signers public key to verify the signature */
    if (demo_verify(libctx, sig_len, sig_value, public_key) != 1)
        goto end;

    ret = EXIT_SUCCESS;
end:
    if (ret != EXIT_SUCCESS)
        ERR_print_errors_fp(stderr);

    OPENSSL_free(sig_value);
    EVP_PKEY_free(params);
    EVP_PKEY_free(pkey);
    OSSL_PARAM_free(public_key);
    OSSL_PARAM_free(keypair);
    OSSL_LIB_CTX_free(libctx);

    return ret;
}
