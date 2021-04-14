/*
 * Copyright 2019-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <string.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/core_names.h>
#include <openssl/param_build.h>
#include "internal/cryptlib.h"
#include "internal/nelem.h"
#include "self_test.h"
#include "self_test_data.inc"

static int self_test_digest(const ST_KAT_DIGEST *t, OSSL_SELF_TEST *st,
                            OSSL_LIB_CTX *libctx)
{
    int ok = 0;
    unsigned char out[EVP_MAX_MD_SIZE];
    unsigned int out_len = 0;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_MD *md = EVP_MD_fetch(libctx, t->algorithm, NULL);

    OSSL_SELF_TEST_onbegin(st, OSSL_SELF_TEST_TYPE_KAT_DIGEST, t->desc);

    if (ctx == NULL
            || md == NULL
            || !EVP_DigestInit_ex(ctx, md, NULL)
            || !EVP_DigestUpdate(ctx, t->pt, t->pt_len)
            || !EVP_DigestFinal(ctx, out, &out_len))
        goto err;

    /* Optional corruption */
    OSSL_SELF_TEST_oncorrupt_byte(st, out);

    if (out_len != t->expected_len
            || memcmp(out, t->expected, out_len) != 0)
        goto err;
    ok = 1;
err:
    EVP_MD_free(md);
    EVP_MD_CTX_free(ctx);
    OSSL_SELF_TEST_onend(st, ok);
    return ok;
}

/*
 * Helper function to setup a EVP_CipherInit
 * Used to hide the complexity of Authenticated ciphers.
 */
static int cipher_init(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher,
                       const ST_KAT_CIPHER *t, int enc)
{
    unsigned char *in_tag = NULL;
    int pad = 0, tmp;

    /* Flag required for Key wrapping */
    EVP_CIPHER_CTX_set_flags(ctx, EVP_CIPHER_CTX_FLAG_WRAP_ALLOW);
    if (t->tag == NULL) {
        /* Use a normal cipher init */
        return EVP_CipherInit_ex(ctx, cipher, NULL, t->key, t->iv, enc)
               && EVP_CIPHER_CTX_set_padding(ctx, pad);
    }

    /* The authenticated cipher init */
    if (!enc)
        in_tag = (unsigned char *)t->tag;

    return EVP_CipherInit_ex(ctx, cipher, NULL, NULL, NULL, enc)
           && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, t->iv_len, NULL)
           && (in_tag == NULL
               || EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, t->tag_len,
                                      in_tag))
           && EVP_CipherInit_ex(ctx, NULL, NULL, t->key, t->iv, enc)
           && EVP_CIPHER_CTX_set_padding(ctx, pad)
           && EVP_CipherUpdate(ctx, NULL, &tmp, t->aad, t->aad_len);
}

/* Test a single KAT for encrypt/decrypt */
static int self_test_cipher(const ST_KAT_CIPHER *t, OSSL_SELF_TEST *st,
                            OSSL_LIB_CTX *libctx)
{
    int ret = 0, encrypt = 1, len = 0, ct_len = 0, pt_len = 0;
    EVP_CIPHER_CTX *ctx = NULL;
    EVP_CIPHER *cipher = NULL;
    unsigned char ct_buf[256] = { 0 };
    unsigned char pt_buf[256] = { 0 };

    OSSL_SELF_TEST_onbegin(st, OSSL_SELF_TEST_TYPE_KAT_CIPHER, t->base.desc);

    ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL)
        goto err;
    cipher = EVP_CIPHER_fetch(libctx, t->base.algorithm, NULL);
    if (cipher == NULL)
        goto err;

    /* Encrypt plain text message */
    if ((t->mode & CIPHER_MODE_ENCRYPT) != 0) {
        if (!cipher_init(ctx, cipher, t, encrypt)
                || !EVP_CipherUpdate(ctx, ct_buf, &len, t->base.pt,
                                     t->base.pt_len)
                || !EVP_CipherFinal_ex(ctx, ct_buf + len, &ct_len))
            goto err;

        OSSL_SELF_TEST_oncorrupt_byte(st, ct_buf);
        ct_len += len;
        if (ct_len != (int)t->base.expected_len
            || memcmp(t->base.expected, ct_buf, ct_len) != 0)
            goto err;

        if (t->tag != NULL) {
            unsigned char tag[16] = { 0 };

            if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, t->tag_len,
                                     tag)
                || memcmp(tag, t->tag, t->tag_len) != 0)
                goto err;
        }
    }

    /* Decrypt cipher text */
    if ((t->mode & CIPHER_MODE_DECRYPT) != 0) {
        if (!(cipher_init(ctx, cipher, t, !encrypt)
              && EVP_CipherUpdate(ctx, pt_buf, &len,
                                  t->base.expected, t->base.expected_len)
              && EVP_CipherFinal_ex(ctx, pt_buf + len, &pt_len)))
            goto err;
        OSSL_SELF_TEST_oncorrupt_byte(st, pt_buf);
        pt_len += len;
        if (pt_len != (int)t->base.pt_len
                || memcmp(pt_buf, t->base.pt, pt_len) != 0)
            goto err;
    }

    ret = 1;
err:
    EVP_CIPHER_free(cipher);
    EVP_CIPHER_CTX_free(ctx);
    OSSL_SELF_TEST_onend(st, ret);
    return ret;
}

static int add_params(OSSL_PARAM_BLD *bld, const ST_KAT_PARAM *params,
                      BN_CTX *ctx)
{
    int ret = 0;
    const ST_KAT_PARAM *p;

    if (params == NULL)
        return 1;
    for (p = params; p->data != NULL; ++p)
    {
        switch (p->type) {
        case OSSL_PARAM_UNSIGNED_INTEGER: {
            BIGNUM *bn = BN_CTX_get(ctx);

            if (bn == NULL
                || (BN_bin2bn(p->data, p->data_len, bn) == NULL)
                || !OSSL_PARAM_BLD_push_BN(bld, p->name, bn))
                goto err;
            break;
        }
        case OSSL_PARAM_UTF8_STRING: {
            if (!OSSL_PARAM_BLD_push_utf8_string(bld, p->name, p->data,
                                                 p->data_len))
                goto err;
            break;
        }
        case OSSL_PARAM_OCTET_STRING: {
            if (!OSSL_PARAM_BLD_push_octet_string(bld, p->name, p->data,
                                                  p->data_len))
                goto err;
            break;
        }
        case OSSL_PARAM_INTEGER: {
            if (!OSSL_PARAM_BLD_push_int(bld, p->name, *(int *)p->data))
                goto err;
            break;
        }
        default:
            break;
        }
    }
    ret = 1;
err:
    return ret;
}

static int self_test_kdf(const ST_KAT_KDF *t, OSSL_SELF_TEST *st,
                         OSSL_LIB_CTX *libctx)
{
    int ret = 0;
    unsigned char out[128];
    EVP_KDF *kdf = NULL;
    EVP_KDF_CTX *ctx = NULL;
    BN_CTX *bnctx = NULL;
    OSSL_PARAM *params  = NULL;
    OSSL_PARAM_BLD *bld = NULL;

    OSSL_SELF_TEST_onbegin(st, OSSL_SELF_TEST_TYPE_KAT_KDF, t->desc);

    bld = OSSL_PARAM_BLD_new();
    if (bld == NULL)
        goto err;

    kdf = EVP_KDF_fetch(libctx, t->algorithm, "");
    if (kdf == NULL)
        goto err;

    ctx = EVP_KDF_CTX_new(kdf);
    if (ctx == NULL)
        goto err;

    bnctx = BN_CTX_new_ex(libctx);
    if (bnctx == NULL)
        goto err;
    if (!add_params(bld, t->params, bnctx))
        goto err;
    params = OSSL_PARAM_BLD_to_param(bld);
    if (params == NULL)
        goto err;

    if (t->expected_len > sizeof(out))
        goto err;
    if (EVP_KDF_derive(ctx, out, t->expected_len, params) <= 0)
        goto err;

    OSSL_SELF_TEST_oncorrupt_byte(st, out);

    if (memcmp(out, t->expected,  t->expected_len) != 0)
        goto err;

    ret = 1;
err:
    EVP_KDF_free(kdf);
    EVP_KDF_CTX_free(ctx);
    BN_CTX_free(bnctx);
    OSSL_PARAM_free(params);
    OSSL_PARAM_BLD_free(bld);
    OSSL_SELF_TEST_onend(st, ret);
    return ret;
}

static int self_test_drbg(const ST_KAT_DRBG *t, OSSL_SELF_TEST *st,
                          OSSL_LIB_CTX *libctx)
{
    int ret = 0;
    unsigned char out[256];
    EVP_RAND *rand;
    EVP_RAND_CTX *test = NULL, *drbg = NULL;
    unsigned int strength = 256;
    int prediction_resistance = 1; /* Causes a reseed */
    OSSL_PARAM drbg_params[3] = {
        OSSL_PARAM_END, OSSL_PARAM_END, OSSL_PARAM_END
    };

    OSSL_SELF_TEST_onbegin(st, OSSL_SELF_TEST_TYPE_DRBG, t->desc);

    rand = EVP_RAND_fetch(libctx, "TEST-RAND", NULL);
    if (rand == NULL)
        goto err;

    test = EVP_RAND_CTX_new(rand, NULL);
    EVP_RAND_free(rand);
    if (test == NULL)
        goto err;

    drbg_params[0] = OSSL_PARAM_construct_uint(OSSL_RAND_PARAM_STRENGTH,
                                               &strength);
    if (!EVP_RAND_CTX_set_params(test, drbg_params))
        goto err;

    rand = EVP_RAND_fetch(libctx, t->algorithm, NULL);
    if (rand == NULL)
        goto err;

    drbg = EVP_RAND_CTX_new(rand, test);
    EVP_RAND_free(rand);
    if (drbg == NULL)
        goto err;

    strength = EVP_RAND_get_strength(drbg);

    drbg_params[0] = OSSL_PARAM_construct_utf8_string(t->param_name,
                                                      t->param_value, 0);
    /* This is only used by HMAC-DRBG but it is ignored by the others */
    drbg_params[1] =
        OSSL_PARAM_construct_utf8_string(OSSL_DRBG_PARAM_MAC, "HMAC", 0);
    if (!EVP_RAND_CTX_set_params(drbg, drbg_params))
        goto err;

    drbg_params[0] =
        OSSL_PARAM_construct_octet_string(OSSL_RAND_PARAM_TEST_ENTROPY,
                                          (void *)t->entropyin,
                                          t->entropyinlen);
    drbg_params[1] =
        OSSL_PARAM_construct_octet_string(OSSL_RAND_PARAM_TEST_NONCE,
                                          (void *)t->nonce, t->noncelen);
    if (!EVP_RAND_instantiate(test, strength, 0, NULL, 0, drbg_params))
        goto err;
    if (!EVP_RAND_instantiate(drbg, strength, 0, t->persstr, t->persstrlen,
                              NULL))
        goto err;

    drbg_params[0] =
        OSSL_PARAM_construct_octet_string(OSSL_RAND_PARAM_TEST_ENTROPY,
                                          (void *)t->entropyinpr1,
                                          t->entropyinpr1len);
    if (!EVP_RAND_CTX_set_params(test, drbg_params))
        goto err;

    if (!EVP_RAND_generate(drbg, out, t->expectedlen, strength,
                           prediction_resistance,
                           t->entropyaddin1, t->entropyaddin1len))
        goto err;

    drbg_params[0] =
        OSSL_PARAM_construct_octet_string(OSSL_RAND_PARAM_TEST_ENTROPY,
                                         (void *)t->entropyinpr2,
                                         t->entropyinpr2len);
    if (!EVP_RAND_CTX_set_params(test, drbg_params))
        goto err;

    /*
     * This calls ossl_prov_drbg_reseed() internally when
     * prediction_resistance = 1
     */
    if (!EVP_RAND_generate(drbg, out, t->expectedlen, strength,
                           prediction_resistance,
                           t->entropyaddin2, t->entropyaddin2len))
        goto err;

    OSSL_SELF_TEST_oncorrupt_byte(st, out);

    if (memcmp(out, t->expected, t->expectedlen) != 0)
        goto err;

    if (!EVP_RAND_uninstantiate(drbg))
        goto err;
    /*
     * Check that the DRBG data has been zeroized after
     * ossl_prov_drbg_uninstantiate.
     */
    if (!EVP_RAND_verify_zeroization(drbg))
        goto err;

    ret = 1;
err:
    EVP_RAND_CTX_free(drbg);
    EVP_RAND_CTX_free(test);
    OSSL_SELF_TEST_onend(st, ret);
    return ret;
}

#if !defined(OPENSSL_NO_DH) || !defined(OPENSSL_NO_EC)
static int self_test_ka(const ST_KAT_KAS *t,
                        OSSL_SELF_TEST *st, OSSL_LIB_CTX *libctx)
{
    int ret = 0;
    EVP_PKEY_CTX *kactx = NULL, *dctx = NULL;
    EVP_PKEY *pkey = NULL, *peerkey = NULL;
    OSSL_PARAM *params = NULL;
    OSSL_PARAM *params_peer = NULL;
    unsigned char secret[256];
    size_t secret_len = sizeof(secret);
    OSSL_PARAM_BLD *bld = NULL;
    BN_CTX *bnctx = NULL;

    OSSL_SELF_TEST_onbegin(st, OSSL_SELF_TEST_TYPE_KAT_KA, t->desc);

    bnctx = BN_CTX_new_ex(libctx);
    if (bnctx == NULL)
        goto err;

    bld = OSSL_PARAM_BLD_new();
    if (bld == NULL)
        goto err;

    if (!add_params(bld, t->key_group, bnctx)
        || !add_params(bld, t->key_host_data, bnctx))
        goto err;
    params = OSSL_PARAM_BLD_to_param(bld);

    if (!add_params(bld, t->key_group, bnctx)
        || !add_params(bld, t->key_peer_data, bnctx))
        goto err;

    params_peer = OSSL_PARAM_BLD_to_param(bld);
    if (params == NULL || params_peer == NULL)
        goto err;

    /* Create a EVP_PKEY_CTX to load the DH keys into */
    kactx = EVP_PKEY_CTX_new_from_name(libctx, t->algorithm, "");
    if (kactx == NULL)
        goto err;
    if (EVP_PKEY_fromdata_init(kactx) <= 0
        || EVP_PKEY_fromdata(kactx, &pkey, EVP_PKEY_KEYPAIR, params) <= 0)
        goto err;
    if (EVP_PKEY_fromdata_init(kactx) <= 0
        || EVP_PKEY_fromdata(kactx, &peerkey, EVP_PKEY_KEYPAIR, params_peer) <= 0)
        goto err;

    /* Create a EVP_PKEY_CTX to perform key derivation */
    dctx = EVP_PKEY_CTX_new_from_pkey(libctx, pkey, NULL);
    if (dctx == NULL)
        goto err;

    if (EVP_PKEY_derive_init(dctx) <= 0
        || EVP_PKEY_derive_set_peer(dctx, peerkey) <= 0
        || EVP_PKEY_derive(dctx, secret, &secret_len) <= 0)
        goto err;

    OSSL_SELF_TEST_oncorrupt_byte(st, secret);

    if (secret_len != t->expected_len
        || memcmp(secret, t->expected, t->expected_len) != 0)
        goto err;
    ret = 1;
err:
    BN_CTX_free(bnctx);
    EVP_PKEY_free(pkey);
    EVP_PKEY_free(peerkey);
    EVP_PKEY_CTX_free(kactx);
    EVP_PKEY_CTX_free(dctx);
    OSSL_PARAM_free(params_peer);
    OSSL_PARAM_free(params);
    OSSL_PARAM_BLD_free(bld);
    OSSL_SELF_TEST_onend(st, ret);
    return ret;
}
#endif /* !defined(OPENSSL_NO_DH) || !defined(OPENSSL_NO_EC) */

static int self_test_sign(const ST_KAT_SIGN *t,
                         OSSL_SELF_TEST *st, OSSL_LIB_CTX *libctx)
{
    int ret = 0;
    OSSL_PARAM *params = NULL, *params_sig = NULL;
    OSSL_PARAM_BLD *bld = NULL;
    EVP_PKEY_CTX *sctx = NULL, *kctx = NULL;
    EVP_PKEY *pkey = NULL;
    unsigned char sig[256];
    BN_CTX *bnctx = NULL;
    size_t siglen = 0;
    static const unsigned char dgst[] = {
        0x7f, 0x83, 0xb1, 0x65, 0x7f, 0xf1, 0xfc, 0x53, 0xb9, 0x2d, 0xc1, 0x81,
        0x48, 0xa1, 0xd6, 0x5d, 0xfc, 0x2d, 0x4b, 0x1f, 0xa3, 0xd6, 0x77, 0x28,
        0x4a, 0xdd, 0xd2, 0x00, 0x12, 0x6d, 0x90, 0x69
    };
    const char *typ = OSSL_SELF_TEST_TYPE_KAT_SIGNATURE;

    if (t->sig_expected == NULL)
        typ = OSSL_SELF_TEST_TYPE_PCT_SIGNATURE;

    OSSL_SELF_TEST_onbegin(st, typ, t->desc);

    bnctx = BN_CTX_new_ex(libctx);
    if (bnctx == NULL)
        goto err;

    bld = OSSL_PARAM_BLD_new();
    if (bld == NULL)
        goto err;

    if (!add_params(bld, t->key, bnctx))
        goto err;
    params = OSSL_PARAM_BLD_to_param(bld);

    /* Create a EVP_PKEY_CTX to load the DSA key into */
    kctx = EVP_PKEY_CTX_new_from_name(libctx, t->algorithm, "");
    if (kctx == NULL || params == NULL)
        goto err;
    if (EVP_PKEY_fromdata_init(kctx) <= 0
        || EVP_PKEY_fromdata(kctx, &pkey, EVP_PKEY_KEYPAIR, params) <= 0)
        goto err;

    /* Create a EVP_PKEY_CTX to use for the signing operation */
    sctx = EVP_PKEY_CTX_new_from_pkey(libctx, pkey, NULL);
    if (sctx == NULL
        || EVP_PKEY_sign_init(sctx) <= 0)
        goto err;

    /* set signature parameters */
    if (!OSSL_PARAM_BLD_push_utf8_string(bld, OSSL_SIGNATURE_PARAM_DIGEST,
                                         t->mdalgorithm,
                                         strlen(t->mdalgorithm) + 1))
        goto err;
    params_sig = OSSL_PARAM_BLD_to_param(bld);
    if (EVP_PKEY_CTX_set_params(sctx, params_sig) <= 0)
        goto err;

    if (EVP_PKEY_sign(sctx, sig, &siglen, dgst, sizeof(dgst)) <= 0
        || EVP_PKEY_verify_init(sctx) <= 0
        || EVP_PKEY_CTX_set_params(sctx, params_sig) <= 0)
        goto err;

    /*
     * Used by RSA, for other key types where the signature changes, we
     * can only use the verify.
     */
    if (t->sig_expected != NULL
        && (siglen != t->sig_expected_len
            || memcmp(sig, t->sig_expected, t->sig_expected_len) != 0))
        goto err;

    OSSL_SELF_TEST_oncorrupt_byte(st, sig);
    if (EVP_PKEY_verify(sctx, sig, siglen, dgst, sizeof(dgst)) <= 0)
        goto err;
    ret = 1;
err:
    BN_CTX_free(bnctx);
    EVP_PKEY_free(pkey);
    EVP_PKEY_CTX_free(kctx);
    EVP_PKEY_CTX_free(sctx);
    OSSL_PARAM_free(params);
    OSSL_PARAM_free(params_sig);
    OSSL_PARAM_BLD_free(bld);
    OSSL_SELF_TEST_onend(st, ret);
    return ret;
}

/*
 * Test an encrypt or decrypt KAT..
 *
 * FIPS 140-2 IG D.9 states that separate KAT tests are needed for encrypt
 * and decrypt..
 */
static int self_test_asym_cipher(const ST_KAT_ASYM_CIPHER *t, OSSL_SELF_TEST *st,
                                 OSSL_LIB_CTX *libctx)
{
    int ret = 0;
    OSSL_PARAM *keyparams = NULL, *initparams = NULL;
    OSSL_PARAM_BLD *keybld = NULL, *initbld = NULL;
    EVP_PKEY_CTX *encctx = NULL, *keyctx = NULL;
    EVP_PKEY *key = NULL;
    BN_CTX *bnctx = NULL;
    unsigned char out[256];
    size_t outlen = sizeof(out);

    OSSL_SELF_TEST_onbegin(st, OSSL_SELF_TEST_TYPE_KAT_ASYM_CIPHER, t->desc);

    bnctx = BN_CTX_new_ex(libctx);
    if (bnctx == NULL)
        goto err;

    /* Load a public or private key from data */
    keybld = OSSL_PARAM_BLD_new();
    if (keybld == NULL
        || !add_params(keybld, t->key, bnctx))
        goto err;
    keyparams = OSSL_PARAM_BLD_to_param(keybld);
    keyctx = EVP_PKEY_CTX_new_from_name(libctx, t->algorithm, NULL);
    if (keyctx == NULL || keyparams == NULL)
        goto err;
    if (EVP_PKEY_fromdata_init(keyctx) <= 0
        || EVP_PKEY_fromdata(keyctx, &key, EVP_PKEY_KEYPAIR, keyparams) <= 0)
        goto err;

    /* Create a EVP_PKEY_CTX to use for the encrypt or decrypt operation */
    encctx = EVP_PKEY_CTX_new_from_pkey(libctx, key, NULL);
    if (encctx == NULL
        || (t->encrypt && EVP_PKEY_encrypt_init(encctx) <= 0)
        || (!t->encrypt && EVP_PKEY_decrypt_init(encctx) <= 0))
        goto err;

    /* Add any additional parameters such as padding */
    if (t->postinit != NULL) {
        initbld = OSSL_PARAM_BLD_new();
        if (initbld == NULL)
            goto err;
        if (!add_params(initbld, t->postinit, bnctx))
            goto err;
        initparams = OSSL_PARAM_BLD_to_param(initbld);
        if (initparams == NULL)
            goto err;
        if (EVP_PKEY_CTX_set_params(encctx, initparams) <= 0)
            goto err;
    }

    if (t->encrypt) {
        if (EVP_PKEY_encrypt(encctx, out, &outlen,
                             t->in, t->in_len) <= 0)
            goto err;
    } else {
        if (EVP_PKEY_decrypt(encctx, out, &outlen,
                             t->in, t->in_len) <= 0)
            goto err;
    }
    /* Check the KAT */
    OSSL_SELF_TEST_oncorrupt_byte(st, out);
    if (outlen != t->expected_len
        || memcmp(out, t->expected, t->expected_len) != 0)
        goto err;

    ret = 1;
err:
    BN_CTX_free(bnctx);
    EVP_PKEY_free(key);
    EVP_PKEY_CTX_free(encctx);
    EVP_PKEY_CTX_free(keyctx);
    OSSL_PARAM_free(keyparams);
    OSSL_PARAM_BLD_free(keybld);
    OSSL_PARAM_free(initparams);
    OSSL_PARAM_BLD_free(initbld);
    OSSL_SELF_TEST_onend(st, ret);
    return ret;
}

/*
 * Test a data driven list of KAT's for digest algorithms.
 * All tests are run regardless of if they fail or not.
 * Return 0 if any test fails.
 */
static int self_test_digests(OSSL_SELF_TEST *st, OSSL_LIB_CTX *libctx)
{
    int i, ret = 1;

    for (i = 0; i < (int)OSSL_NELEM(st_kat_digest_tests); ++i) {
        if (!self_test_digest(&st_kat_digest_tests[i], st, libctx))
            ret = 0;
    }
    return ret;
}

static int self_test_ciphers(OSSL_SELF_TEST *st, OSSL_LIB_CTX *libctx)
{
    int i, ret = 1;

    for (i = 0; i < (int)OSSL_NELEM(st_kat_cipher_tests); ++i) {
        if (!self_test_cipher(&st_kat_cipher_tests[i], st, libctx))
            ret = 0;
    }
    return ret;
}

static int self_test_asym_ciphers(OSSL_SELF_TEST *st, OSSL_LIB_CTX *libctx)
{
    int i, ret = 1;

    for (i = 0; i < (int)OSSL_NELEM(st_kat_asym_cipher_tests); ++i) {
        if (!self_test_asym_cipher(&st_kat_asym_cipher_tests[i], st, libctx))
            ret = 0;
    }
    return ret;
}

static int self_test_kdfs(OSSL_SELF_TEST *st, OSSL_LIB_CTX *libctx)
{
    int i, ret = 1;

    for (i = 0; i < (int)OSSL_NELEM(st_kat_kdf_tests); ++i) {
        if (!self_test_kdf(&st_kat_kdf_tests[i], st, libctx))
            ret = 0;
    }
    return ret;
}

static int self_test_drbgs(OSSL_SELF_TEST *st, OSSL_LIB_CTX *libctx)
{
    int i, ret = 1;

    for (i = 0; i < (int)OSSL_NELEM(st_kat_drbg_tests); ++i) {
        if (!self_test_drbg(&st_kat_drbg_tests[i], st, libctx))
            ret = 0;
    }
    return ret;
}

static int self_test_kas(OSSL_SELF_TEST *st, OSSL_LIB_CTX *libctx)
{
    int ret = 1;
#if !defined(OPENSSL_NO_DH) || !defined(OPENSSL_NO_EC)
    int i;

    for (i = 0; i < (int)OSSL_NELEM(st_kat_kas_tests); ++i) {
        if (!self_test_ka(&st_kat_kas_tests[i], st, libctx))
            ret = 0;
    }
#endif

    return ret;
}

static int self_test_signatures(OSSL_SELF_TEST *st, OSSL_LIB_CTX *libctx)
{
    int i, ret = 1;

    for (i = 0; i < (int)OSSL_NELEM(st_kat_sign_tests); ++i) {
        if (!self_test_sign(&st_kat_sign_tests[i], st, libctx))
            ret = 0;
    }
    return ret;
}

/*
 * Run the algorithm KAT's.
 * Return 1 is successful, otherwise return 0.
 * This runs all the tests regardless of if any fail.
 */
int SELF_TEST_kats(OSSL_SELF_TEST *st, OSSL_LIB_CTX *libctx)
{
    int ret = 1;

    if (!self_test_digests(st, libctx))
        ret = 0;
    if (!self_test_ciphers(st, libctx))
        ret = 0;
    if (!self_test_signatures(st, libctx))
        ret = 0;
    if (!self_test_kdfs(st, libctx))
        ret = 0;
    if (!self_test_drbgs(st, libctx))
        ret = 0;
    if (!self_test_kas(st, libctx))
        ret = 0;
    if (!self_test_asym_ciphers(st, libctx))
        ret = 0;

    return ret;
}
