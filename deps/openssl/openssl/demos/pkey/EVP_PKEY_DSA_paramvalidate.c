/*-
 * Copyright 2022-2024 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * Example showing how to validate DSA parameters.
 *
 * Proper FIPS 186-4 DSA (FFC) parameter validation requires that all
 * the parameters used during parameter generation are supplied
 * when doing the validation. Unfortunately saving DSA parameters as
 * a PEM or DER file does not write out all required fields. Because
 * of this the default provider normally only does a partial
 * validation. The FIPS provider will however try to do a full
 * validation. To force the default provider to use full
 * validation the 'seed' that is output during generation must be
 * added to the key. See doc/man7/EVP_PKEY-FFC for more information.
 */

#include <openssl/evp.h>
#include <openssl/core_names.h>
#include <openssl/pem.h>
#include "dsa.inc"

/* The following values were output from the EVP_PKEY_DSA_paramgen demo */
static const char dsapem[] =
    "-----BEGIN DSA PARAMETERS-----\n"
    "MIICLAKCAQEA1pobSR1FJ3+Tvi0J6Tk1PSV2owZey1Nuo847hGw/59VCS6RPQEqr\n"
    "vp5fhbvBjupBeVGA/AMH6rI4i4h6jlhurrqH1CqUHVcDhJzxV668bMLiP3mIxg5o\n"
    "9Yq8x6BnSOtH5Je0tpeE0/fEvvLjCwBUbwnwWxzjANcvDUEt9XYeRrtB2v52fr56\n"
    "hVYz3wMMNog4CEDOLTvx7/84eVPuUeWDRQFH1EaHMdulP34KBcatEEpEZapkepng\n"
    "nohm9sFSPQhq2utpkH7pNXdG0EILBtRDCvUpF5720a48LYofdggh2VEZfgElAGFk\n"
    "dW/CkvyBDmGIzil5aTz4MMsdudaVYgzt6wIhAPsSGC42Qa+X0AFGvonb5nmfUVm/\n"
    "8aC+tHk7Nb2AYLHXAoIBADx5C0H1+QHsmGKvuOaY+WKUt7aWUrEivD1zBMJAQ6bL\n"
    "Wv9lbCq1CFHvVzojeOVpn872NqDEpkx4HTpvqhxWL5CkbN/HaGItsQzkD59AQg3v\n"
    "4YsLlkesq9Jq6x/aWetJXWO36fszFv1gpD3NY3wliBvMYHx62jfc5suh9D3ZZvu7\n"
    "PLGH4X4kcfzK/R2b0oVbEBjVTe5GMRYZRqnvfSW2f2fA7BzI1OL83UxDDe58cL2M\n"
    "GcAoUYXOBAfZ37qLMm2juf+o5gCrT4CXfRPu6kbapt7V/YIc1nsNgeAOKKoFBHBQ\n"
    "gc5u5G6G/j79FVoSDq9DYwTJcHPsU+eHj1uWHso1AjQ=\n"
    "-----END DSA PARAMETERS-----\n";

static const char hexseed[] =
    "cba30ccd905aa7675a0b81769704bf3c"
    "ccf2ca1892b2eaf6b9e2b38d9bf6affc"
    "42ada55986d8a1772b442770954d0b65";
static const int gindex = 42;
static const int pcounter = 363;
static const char digest[] = "SHA384";

/*
 * Create a new dsa param key that is the combination of an existing param key
 * plus extra parameters.
 */
static EVP_PKEY_CTX *create_merged_key(EVP_PKEY *dsaparams, const OSSL_PARAM *newparams,
                                       OSSL_LIB_CTX *libctx, const char *propq)
{
    EVP_PKEY_CTX *out = NULL;
    EVP_PKEY_CTX *ctx = NULL;
    EVP_PKEY *pkey = NULL;
    OSSL_PARAM *mergedparams = NULL;
    OSSL_PARAM *loadedparams = NULL;

    /* Specify EVP_PKEY_KEY_PUBLIC here if you have a public key */
    if (EVP_PKEY_todata(dsaparams, EVP_PKEY_KEY_PARAMETERS, &loadedparams) <= 0) {
        fprintf(stderr, "EVP_PKEY_todata() failed\n");
        goto cleanup;
    }
    mergedparams = OSSL_PARAM_merge(loadedparams, newparams);
    if (mergedparams == NULL) {
        fprintf(stderr, "OSSL_PARAM_merge() failed\n");
        goto cleanup;
    }

    ctx = EVP_PKEY_CTX_new_from_name(libctx, "DSA", propq);
    if (ctx == NULL) {
        fprintf(stderr, "EVP_PKEY_CTX_new_from_name() failed\n");
        goto cleanup;
    }
    if (EVP_PKEY_fromdata_init(ctx) <= 0
            || EVP_PKEY_fromdata(ctx, &pkey,
                                 EVP_PKEY_KEY_PARAMETERS, mergedparams) <= 0) {
        fprintf(stderr, "EVP_PKEY_fromdata() failed\n");
        goto cleanup;
    }
    out = EVP_PKEY_CTX_new_from_pkey(libctx, pkey, propq);
    if (out == NULL) {
        fprintf(stderr, "EVP_PKEY_CTX_new_from_pkey() failed\n");
        goto cleanup;
    }

cleanup:
    EVP_PKEY_free(pkey);
    OSSL_PARAM_free(loadedparams);
    OSSL_PARAM_free(mergedparams);
    EVP_PKEY_CTX_free(ctx);
    return out;
}

int main(int argc, char **argv)
{
    int ret = EXIT_FAILURE;
    OSSL_LIB_CTX *libctx = NULL;
    const char *propq = NULL;
    EVP_PKEY *dsaparamskey = NULL;
    EVP_PKEY_CTX *ctx = NULL;
    EVP_PKEY_CTX *ctx1 = NULL;
    EVP_PKEY_CTX *ctx2 = NULL;
    BIO *in = NULL;
    OSSL_PARAM params[6];
    unsigned char seed[64];
    size_t seedlen;

    if (!OPENSSL_hexstr2buf_ex(seed, sizeof(seed), &seedlen, hexseed, '\0'))
        goto cleanup;
    /*
     * This example loads the PEM data from a memory buffer
     * Use BIO_new_fp() to load a PEM file instead
     */
    in = BIO_new_mem_buf(dsapem, strlen(dsapem));
    if (in == NULL) {
        fprintf(stderr, "BIO_new_mem_buf() failed\n");
        goto cleanup;
    }

    /* Load DSA params from pem data */
    dsaparamskey = PEM_read_bio_Parameters_ex(in, NULL, libctx, propq);
    if (dsaparamskey == NULL) {
        fprintf(stderr, "Failed to load dsa params\n");
        goto cleanup;
    }

    ctx = EVP_PKEY_CTX_new_from_pkey(libctx, dsaparamskey, propq);
    if (ctx == NULL) {
        fprintf(stderr, "EVP_PKEY_CTX_new_from_pkey() failed\n");
        goto cleanup;
    }
    /*
     * When using the default provider this only does a partial check to
     * make sure that the values of p, q and g are ok.
     * This will fail however if the FIPS provider is used since it does
     * a proper FIPS 186-4 key validation which requires extra parameters
     */
    if (EVP_PKEY_param_check(ctx) <= 0) {
        fprintf(stderr, "Simple EVP_PKEY_param_check() failed\n");
        goto cleanup;
    }

    /*
     * Setup parameters that we want to add.
     * For illustration purposes it deliberately omits a required parameter.
     */
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_FFC_TYPE,
                                                "fips186_4", 0);
    /* Force it to do a proper validation by setting the seed */
    params[1] = OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_FFC_SEED,
                                                  (void *)seed, seedlen);
    params[2] = OSSL_PARAM_construct_int(OSSL_PKEY_PARAM_FFC_GINDEX, (int *)&gindex);
    params[3] = OSSL_PARAM_construct_int(OSSL_PKEY_PARAM_FFC_PCOUNTER, (int *)&pcounter);
    params[4] = OSSL_PARAM_construct_end();

    /* generate a new key that is the combination of the existing key and the new params */
    ctx1 = create_merged_key(dsaparamskey, params, libctx, propq);
    if (ctx1 == NULL)
        goto cleanup;
    /* This will fail since not all the parameters used for key generation are added */
    if (EVP_PKEY_param_check(ctx1) > 0) {
        fprintf(stderr, "EVP_PKEY_param_check() should fail\n");
        goto cleanup;
    }

    /*
     * Add the missing parameters onto the end of the existing list of params
     * If the default was used for the generation then this parameter is not
     * needed
     */
    params[4] = OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_FFC_DIGEST,
                                                 (char *)digest, 0);
    params[5] = OSSL_PARAM_construct_end();
    ctx2 = create_merged_key(dsaparamskey, params, libctx, propq);
    if (ctx2 == NULL)
        goto cleanup;
    if (EVP_PKEY_param_check(ctx2) <= 0) {
        fprintf(stderr, "EVP_PKEY_param_check() failed\n");
        goto cleanup;
    }

    if (!dsa_print_key(EVP_PKEY_CTX_get0_pkey(ctx2), 0, libctx, propq))
        goto cleanup;

    ret = EXIT_SUCCESS;
cleanup:
    EVP_PKEY_free(dsaparamskey);
    EVP_PKEY_CTX_free(ctx2);
    EVP_PKEY_CTX_free(ctx1);
    EVP_PKEY_CTX_free(ctx);
    BIO_free(in);
    return ret;
}
