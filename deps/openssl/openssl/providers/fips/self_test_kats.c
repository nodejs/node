/*
 * Copyright 2019-2026 The OpenSSL Project Authors. All Rights Reserved.
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
#include <openssl/proverr.h>
#include <openssl/rand.h>
#include "crypto/ml_dsa.h"
#include "crypto/rand.h"
#include "internal/cryptlib.h"
#include "self_test.h"

#if !defined(OPENSSL_NO_DH) || !defined(OPENSSL_NO_EC)
#define SELF_TEST_KA_ENABLED 1
#endif

#if !defined(OPENSSL_NO_ML_DSA) || !defined(OPENSSL_NO_SLH_DSA)
#define SELF_TEST_ASYM_KEYGEN_ENABLED 1
#endif

#if !defined(OPENSSL_NO_ML_KEM)
#define SELF_TEST_KEM_ENABLED 1
#endif

static int set_kat_drbg(OSSL_LIB_CTX *ctx,
    const unsigned char *entropy, size_t entropy_len,
    const unsigned char *nonce, size_t nonce_len,
    const unsigned char *persstr, size_t persstr_len);
static int reset_main_drbg(OSSL_LIB_CTX *ctx);

static int self_test_digest(const ST_DEFINITION *t, OSSL_SELF_TEST *st,
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
        || !EVP_DigestUpdate(ctx, t->pt.buf, t->pt.len)
        || !EVP_DigestFinal(ctx, out, &out_len))
        goto err;

    /* Optional corruption */
    OSSL_SELF_TEST_oncorrupt_byte(st, out);

    if (out_len != t->expected.len
        || memcmp(out, t->expected.buf, out_len) != 0)
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
    if (t->tag.buf == NULL) {
        /* Use a normal cipher init */
        return EVP_CipherInit_ex(ctx, cipher, NULL, t->key.buf, t->iv.buf, enc)
            && EVP_CIPHER_CTX_set_padding(ctx, pad);
    }

    /* The authenticated cipher init */
    if (!enc)
        in_tag = (unsigned char *)t->tag.buf;

    return EVP_CipherInit_ex(ctx, cipher, NULL, NULL, NULL, enc)
        && (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, (int)t->iv.len, NULL) > 0)
        && (in_tag == NULL
            || EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, (int)t->tag.len,
                   in_tag)
                > 0)
        && EVP_CipherInit_ex(ctx, NULL, NULL, t->key.buf, t->iv.buf, enc)
        && EVP_CIPHER_CTX_set_padding(ctx, pad)
        && EVP_CipherUpdate(ctx, NULL, &tmp, t->aad.buf, (int)t->aad.len);
}

/* Test a single KAT for encrypt/decrypt */
static int self_test_cipher(const ST_DEFINITION *t, OSSL_SELF_TEST *st,
    OSSL_LIB_CTX *libctx)
{
    int ret = 0, encrypt = 1, len = 0, ct_len = 0, pt_len = 0;
    EVP_CIPHER_CTX *ctx = NULL;
    EVP_CIPHER *cipher = NULL;
    unsigned char ct_buf[256] = { 0 };
    unsigned char pt_buf[256] = { 0 };

    OSSL_SELF_TEST_onbegin(st, OSSL_SELF_TEST_TYPE_KAT_CIPHER, t->desc);

    ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL)
        goto err;
    cipher = EVP_CIPHER_fetch(libctx, t->algorithm, NULL);
    if (cipher == NULL)
        goto err;

    /* Encrypt plain text message */
    if ((t->u.cipher.mode & CIPHER_MODE_ENCRYPT) != 0) {
        if (!cipher_init(ctx, cipher, &t->u.cipher, encrypt)
            || !EVP_CipherUpdate(ctx, ct_buf, &len, t->pt.buf,
                (int)t->pt.len)
            || !EVP_CipherFinal_ex(ctx, ct_buf + len, &ct_len))
            goto err;

        OSSL_SELF_TEST_oncorrupt_byte(st, ct_buf);
        ct_len += len;
        if (ct_len != (int)t->expected.len
            || memcmp(t->expected.buf, ct_buf, ct_len) != 0)
            goto err;

        if (t->u.cipher.tag.buf != NULL) {
            unsigned char tag[16] = { 0 };

            if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG,
                    (int)t->u.cipher.tag.len, tag)
                    <= 0
                || memcmp(tag, t->u.cipher.tag.buf, t->u.cipher.tag.len) != 0)
                goto err;
        }
    }

    /* Decrypt cipher text */
    if ((t->u.cipher.mode & CIPHER_MODE_DECRYPT) != 0) {
        if (!(cipher_init(ctx, cipher, &t->u.cipher, !encrypt)
                && EVP_CipherUpdate(ctx, pt_buf, &len,
                    t->expected.buf, (int)t->expected.len)
                && EVP_CipherFinal_ex(ctx, pt_buf + len, &pt_len)))
            goto err;
        OSSL_SELF_TEST_oncorrupt_byte(st, pt_buf);
        pt_len += len;
        if (pt_len != (int)t->pt.len
            || memcmp(pt_buf, t->pt.buf, pt_len) != 0)
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
    for (p = params; p->data != NULL; ++p) {
        switch (p->type) {
        case OSSL_PARAM_UNSIGNED_INTEGER: {
            BIGNUM *bn = BN_CTX_get(ctx);

            if (bn == NULL
                || (BN_bin2bn(p->data, (int)p->data_len, bn) == NULL)
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

#if defined(__GNUC__)
#define SENTINEL __attribute__((sentinel))
#endif

#if !defined(SENTINEL) && defined(__clang_major__) && __clang_major__ > 14
#define SENTINEL __attribute__((sentinel))
#endif

#ifndef SENTINEL
#define SENTINEL
#endif

static SENTINEL OSSL_PARAM *kat_params_to_ossl_params(OSSL_LIB_CTX *libctx, ...)
{
    BN_CTX *bnc = NULL;
    OSSL_PARAM *params = NULL;
    OSSL_PARAM_BLD *bld = NULL;
    const ST_KAT_PARAM *pms;
    va_list ap;

    bnc = BN_CTX_new_ex(libctx);
    if (bnc == NULL)
        goto err;
    bld = OSSL_PARAM_BLD_new();
    if (bld == NULL)
        goto err;

    va_start(ap, libctx);
    while ((pms = va_arg(ap, const ST_KAT_PARAM *)) != NULL)
        if (!add_params(bld, pms, bnc)) {
            va_end(ap);
            goto err;
        }
    va_end(ap);

    params = OSSL_PARAM_BLD_to_param(bld);

err:
    OSSL_PARAM_BLD_free(bld);
    BN_CTX_free(bnc);
    return params;
}

static int self_test_kdf(const ST_DEFINITION *t, OSSL_SELF_TEST *st,
    OSSL_LIB_CTX *libctx)
{
    int ret = 0;
    unsigned char out[256];
    EVP_KDF *kdf = NULL;
    EVP_KDF_CTX *ctx = NULL;
    OSSL_PARAM *params = NULL;

    OSSL_SELF_TEST_onbegin(st, OSSL_SELF_TEST_TYPE_KAT_KDF, t->desc);

    kdf = EVP_KDF_fetch(libctx, t->algorithm, "");
    if (kdf == NULL)
        goto err;

    ctx = EVP_KDF_CTX_new(kdf);
    if (ctx == NULL)
        goto err;

    params = kat_params_to_ossl_params(libctx, t->u.kdf.params, NULL);
    if (params == NULL)
        goto err;

    if (t->expected.len > sizeof(out))
        goto err;
    if (EVP_KDF_derive(ctx, out, t->expected.len, params) <= 0)
        goto err;

    OSSL_SELF_TEST_oncorrupt_byte(st, out);

    if (memcmp(out, t->expected.buf, t->expected.len) != 0)
        goto err;

    ret = 1;
err:
    EVP_KDF_free(kdf);
    EVP_KDF_CTX_free(ctx);
    OSSL_PARAM_free(params);
#ifdef OPENSSL_PEDANTIC_ZEROIZATION
    OPENSSL_cleanse(out, 256);
#endif
    OSSL_SELF_TEST_onend(st, ret);
    return ret;
}

static int self_test_drbg(const ST_DEFINITION *t, OSSL_SELF_TEST *st,
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

    drbg_params[0] = OSSL_PARAM_construct_utf8_string(t->u.drbg.param_name,
        (char *)t->u.drbg.param_value, 0);
    if (!EVP_RAND_CTX_set_params(drbg, drbg_params))
        goto err;

    drbg_params[0] = OSSL_PARAM_construct_octet_string(OSSL_RAND_PARAM_TEST_ENTROPY,
        (void *)t->u.drbg.entropyin.buf,
        t->u.drbg.entropyin.len);
    drbg_params[1] = OSSL_PARAM_construct_octet_string(OSSL_RAND_PARAM_TEST_NONCE,
        (void *)t->u.drbg.nonce.buf,
        t->u.drbg.nonce.len);
    if (!EVP_RAND_instantiate(test, strength, 0, NULL, 0, drbg_params))
        goto err;
    if (!EVP_RAND_instantiate(drbg, strength, 0, t->u.drbg.persstr.buf,
            t->u.drbg.persstr.len, NULL))
        goto err;

    drbg_params[0] = OSSL_PARAM_construct_octet_string(OSSL_RAND_PARAM_TEST_ENTROPY,
        (void *)t->u.drbg.entropyinpr1.buf,
        t->u.drbg.entropyinpr1.len);
    if (!EVP_RAND_CTX_set_params(test, drbg_params))
        goto err;

    if (!EVP_RAND_generate(drbg, out, t->expected.len, strength,
            prediction_resistance,
            t->u.drbg.entropyaddin1.buf,
            t->u.drbg.entropyaddin1.len))
        goto err;

    drbg_params[0] = OSSL_PARAM_construct_octet_string(OSSL_RAND_PARAM_TEST_ENTROPY,
        (void *)t->u.drbg.entropyinpr2.buf,
        t->u.drbg.entropyinpr2.len);
    if (!EVP_RAND_CTX_set_params(test, drbg_params))
        goto err;

    /*
     * This calls ossl_prov_drbg_reseed() internally when
     * prediction_resistance = 1
     */
    if (!EVP_RAND_generate(drbg, out, t->expected.len, strength,
            prediction_resistance,
            t->u.drbg.entropyaddin2.buf,
            t->u.drbg.entropyaddin2.len))
        goto err;

    OSSL_SELF_TEST_oncorrupt_byte(st, out);

    if (memcmp(out, t->expected.buf, t->expected.len) != 0)
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

#if defined(SELF_TEST_KA_ENABLED)
static int self_test_ka(const ST_DEFINITION *t,
    OSSL_SELF_TEST *st, OSSL_LIB_CTX *libctx)
{
    int ret = 0;
    EVP_PKEY_CTX *kactx = NULL, *dctx = NULL;
    EVP_PKEY *pkey = NULL, *peerkey = NULL;
    OSSL_PARAM *params = NULL;
    OSSL_PARAM *params_peer = NULL;
    unsigned char secret[256];
    size_t secret_len = t->expected.len;

    OSSL_SELF_TEST_onbegin(st, OSSL_SELF_TEST_TYPE_KAT_KA, t->desc);

    if (secret_len > sizeof(secret))
        goto err;

    params = kat_params_to_ossl_params(libctx, t->u.kas.key_group,
        t->u.kas.key_host_data, NULL);
    params_peer = kat_params_to_ossl_params(libctx, t->u.kas.key_group,
        t->u.kas.key_peer_data, NULL);
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

    if (secret_len != t->expected.len
        || memcmp(secret, t->expected.buf, t->expected.len) != 0)
        goto err;
    ret = 1;
err:
    EVP_PKEY_free(pkey);
    EVP_PKEY_free(peerkey);
    EVP_PKEY_CTX_free(kactx);
    EVP_PKEY_CTX_free(dctx);
    OSSL_PARAM_free(params_peer);
    OSSL_PARAM_free(params);
    OSSL_SELF_TEST_onend(st, ret);
    return ret;
}
#endif /* defined(SELF_TEST_KA_ENABLED) */

static int digest_signature(const uint8_t *sig, size_t sig_len,
    uint8_t *out, size_t *out_len,
    OSSL_LIB_CTX *lib_ctx)
{
    int ret;
    unsigned int len = 0;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_MD *md = EVP_MD_fetch(lib_ctx, "SHA256", NULL);

    ret = ctx != NULL
        && md != NULL
        && EVP_DigestInit_ex(ctx, md, NULL) == 1
        && EVP_DigestUpdate(ctx, sig, sig_len) == 1
        && EVP_DigestFinal(ctx, out, &len) == 1;
    EVP_MD_free(md);
    EVP_MD_CTX_free(ctx);
    *out_len = len;
    return ret;
}

static int self_test_digest_sign(const ST_DEFINITION *t,
    OSSL_SELF_TEST *st, OSSL_LIB_CTX *libctx)
{
    int ret = 0;
    OSSL_PARAM *paramskey = NULL, *paramsinit = NULL, *paramsverify = NULL;
    EVP_SIGNATURE *sigalg = NULL;
    EVP_PKEY_CTX *ctx = NULL;
    EVP_PKEY_CTX *fromctx = NULL;
    EVP_PKEY *pkey = NULL;
    unsigned char sig[MAX_ML_DSA_SIG_LEN], *psig = sig;
    size_t siglen;
    int digested = 0;
    const char *typ = OSSL_SELF_TEST_TYPE_KAT_SIGNATURE;

    if (t->expected.len > sizeof(sig))
        goto err;

    if (t->expected.buf == NULL)
        typ = OSSL_SELF_TEST_TYPE_PCT_SIGNATURE;

    OSSL_SELF_TEST_onbegin(st, typ, t->desc);

    if (t->u.sig.entropy.buf != NULL) {
        if (!set_kat_drbg(libctx, t->u.sig.entropy.buf, t->u.sig.entropy.len,
                t->u.sig.nonce.buf, t->u.sig.nonce.len,
                t->u.sig.persstr.buf, t->u.sig.persstr.len))
            goto err;
    }

    paramskey = kat_params_to_ossl_params(libctx, t->u.sig.key, NULL);
    paramsinit = kat_params_to_ossl_params(libctx, t->u.sig.init, NULL);
    paramsverify = kat_params_to_ossl_params(libctx, t->u.sig.verify, NULL);

    fromctx = EVP_PKEY_CTX_new_from_name(libctx, t->u.sig.keytype, NULL);
    if (fromctx == NULL
        || paramskey == NULL
        || paramsinit == NULL
        || paramsverify == NULL)
        goto err;
    if (EVP_PKEY_fromdata_init(fromctx) <= 0
        || EVP_PKEY_fromdata(fromctx, &pkey, EVP_PKEY_KEYPAIR, paramskey) <= 0)
        goto err;

    sigalg = EVP_SIGNATURE_fetch(libctx, t->algorithm, NULL);
    if (sigalg == NULL)
        goto err;
    ctx = EVP_PKEY_CTX_new_from_pkey(libctx, pkey, NULL);
    if (ctx == NULL)
        goto err;

    digested = ((t->u.sig.mode & SIGNATURE_MODE_DIGESTED) != 0);

    if ((t->u.sig.mode & SIGNATURE_MODE_VERIFY_ONLY) != 0) {
        siglen = t->expected.len;
        memcpy(psig, t->expected.buf, siglen);
    } else {
        if (digested) {
            if (EVP_PKEY_sign_init_ex2(ctx, sigalg, paramsinit) <= 0)
                goto err;
        } else {
            if (EVP_PKEY_sign_message_init(ctx, sigalg, paramsinit) <= 0)
                goto err;
        }
        siglen = sizeof(sig);
        if ((t->u.sig.mode & SIGNATURE_MODE_SIG_DIGESTED) != 0) {
            if (EVP_PKEY_sign(ctx, NULL, &siglen, t->pt.buf, t->pt.len) <= 0)
                goto err;
            if (siglen > sizeof(sig)) {
                psig = OPENSSL_malloc(siglen);
                if (psig == NULL)
                    goto err;
            }
        }
        if (EVP_PKEY_sign(ctx, psig, &siglen, t->pt.buf, t->pt.len) <= 0)
            goto err;

        if (t->expected.buf != NULL) {
            if ((t->u.sig.mode & SIGNATURE_MODE_SIG_DIGESTED) != 0) {
                uint8_t digested_sig[EVP_MAX_MD_SIZE];
                size_t digested_sig_len = 0;

                if (!digest_signature(psig, siglen, digested_sig,
                        &digested_sig_len, libctx)
                    || digested_sig_len != t->expected.len
                    || memcmp(digested_sig, t->expected.buf, t->expected.len) != 0)
                    goto err;
            } else {
                if (siglen != t->expected.len
                    || memcmp(psig, t->expected.buf, t->expected.len) != 0)
                    goto err;
            }
        }
    }

    if ((t->u.sig.mode & SIGNATURE_MODE_SIGN_ONLY) == 0) {
        if (digested) {
            if (EVP_PKEY_verify_init_ex2(ctx, sigalg, paramsverify) <= 0)
                goto err;
        } else {
            if (EVP_PKEY_verify_message_init(ctx, sigalg, paramsverify) <= 0)
                goto err;
        }
        OSSL_SELF_TEST_oncorrupt_byte(st, psig);
        if (EVP_PKEY_verify(ctx, psig, siglen, t->pt.buf, t->pt.len) <= 0)
            goto err;
    }
    ret = 1;
err:
    if (psig != sig)
        OPENSSL_free(psig);
    EVP_PKEY_free(pkey);
    EVP_PKEY_CTX_free(fromctx);
    EVP_PKEY_CTX_free(ctx);
    EVP_SIGNATURE_free(sigalg);
    OSSL_PARAM_free(paramskey);
    OSSL_PARAM_free(paramsinit);
    OSSL_PARAM_free(paramsverify);
    if (t->u.sig.entropy.buf != NULL) {
        if (!reset_main_drbg(libctx))
            ret = 0;
    }
    OSSL_SELF_TEST_onend(st, ret);
    return ret;
}

#if defined(SELF_TEST_ASYM_KEYGEN_ENABLED)
/*
 * Test that a deterministic key generation produces the correct key
 */
static int self_test_asym_keygen(const ST_DEFINITION *t, OSSL_SELF_TEST *st,
    OSSL_LIB_CTX *libctx)
{
    int ret = 0;
    const ST_KAT_PARAM *expected;
    OSSL_PARAM *key_params = NULL;
    EVP_PKEY_CTX *key_ctx = NULL;
    EVP_PKEY *key = NULL;
    uint8_t out[MAX_ML_DSA_PRIV_LEN];
    size_t out_len = 0;

    OSSL_SELF_TEST_onbegin(st, OSSL_SELF_TEST_TYPE_KAT_ASYM_KEYGEN, t->desc);

    key_ctx = EVP_PKEY_CTX_new_from_name(libctx, t->algorithm, NULL);
    if (key_ctx == NULL)
        goto err;
    if (t->u.akgen.keygen_params != NULL) {
        key_params = kat_params_to_ossl_params(libctx, t->u.akgen.keygen_params,
            NULL);
        if (key_params == NULL)
            goto err;
    }
    if (EVP_PKEY_keygen_init(key_ctx) != 1
        || EVP_PKEY_CTX_set_params(key_ctx, key_params) != 1
        || EVP_PKEY_generate(key_ctx, &key) != 1)
        goto err;

    for (expected = t->u.akgen.expected_params; expected->data != NULL; ++expected) {
        if (expected->type != OSSL_PARAM_OCTET_STRING
            || !EVP_PKEY_get_octet_string_param(key, expected->name,
                out, sizeof(out), &out_len))
            goto err;
        OSSL_SELF_TEST_oncorrupt_byte(st, out);
        /* Check the KAT */
        if (out_len != expected->data_len
            || memcmp(out, expected->data, expected->data_len) != 0)
            goto err;
    }
    ret = 1;
err:
    EVP_PKEY_free(key);
    EVP_PKEY_CTX_free(key_ctx);
    OSSL_PARAM_free(key_params);
    OSSL_SELF_TEST_onend(st, ret);
    return ret;
}
#endif /* defined(SELF_TEST_ASYM_KEYGEN_ENABLED) */

#if defined(SELF_TEST_KEM_ENABLED)
/*
 * FIPS 140-3 IG 10.3.A resolution 14 mandates a CAST for ML-KEM
 * encapsulation.
 */
static int self_test_kem_encapsulate(const ST_KAT_KEM *t, OSSL_SELF_TEST *st,
    OSSL_LIB_CTX *libctx, EVP_PKEY *pkey)
{
    int ret = 0;
    EVP_PKEY_CTX *ctx;
    unsigned char *wrapped = NULL, *secret = NULL;
    size_t wrappedlen = t->cipher_text.len, secretlen = t->secret.len;
    OSSL_PARAM params[2] = { OSSL_PARAM_END, OSSL_PARAM_END };

    OSSL_SELF_TEST_onbegin(st, OSSL_SELF_TEST_TYPE_KAT_KEM,
        OSSL_SELF_TEST_DESC_ENCAP_KEM);

    ctx = EVP_PKEY_CTX_new_from_pkey(libctx, pkey, "");
    if (ctx == NULL)
        goto err;

    *params = OSSL_PARAM_construct_octet_string(OSSL_KEM_PARAM_IKME,
        (unsigned char *)t->entropy.buf,
        t->entropy.len);
    if (EVP_PKEY_encapsulate_init(ctx, params) <= 0)
        goto err;

    /* Allocate output buffers */
    wrapped = OPENSSL_malloc(wrappedlen);
    secret = OPENSSL_malloc(secretlen);
    if (wrapped == NULL || secret == NULL)
        goto err;

    /* Encapsulate */
    if (EVP_PKEY_encapsulate(ctx, wrapped, &wrappedlen, secret, &secretlen) <= 0)
        goto err;

    /* Compare outputs */
    OSSL_SELF_TEST_oncorrupt_byte(st, wrapped);
    if (wrappedlen != t->cipher_text.len
        || memcmp(wrapped, t->cipher_text.buf, t->cipher_text.len) != 0)
        goto err;

    OSSL_SELF_TEST_oncorrupt_byte(st, secret);
    if (secretlen != t->secret.len
        || memcmp(secret, t->secret.buf, t->secret.len) != 0)
        goto err;

    ret = 1;
err:
    OPENSSL_free(wrapped);
    OPENSSL_free(secret);
    EVP_PKEY_CTX_free(ctx);
    OSSL_SELF_TEST_onend(st, ret);
    return ret;
}

/*
 * FIPS 140-3 IG 10.3.A resolution 14 mandates a CAST for ML-KEM
 * decapsulation both for the rejection path and the normal path.
 */
static int self_test_kem_decapsulate(const ST_KAT_KEM *t, OSSL_SELF_TEST *st,
    OSSL_LIB_CTX *libctx, EVP_PKEY *pkey,
    int reject)
{
    int ret = 0;
    EVP_PKEY_CTX *ctx = NULL;
    unsigned char *secret = NULL, *alloced = NULL;
    const unsigned char *test_secret = t->secret.buf;
    const unsigned char *cipher_text = t->cipher_text.buf;
    size_t secretlen = t->secret.len;

    OSSL_SELF_TEST_onbegin(st, OSSL_SELF_TEST_TYPE_KAT_KEM,
        reject ? OSSL_SELF_TEST_DESC_DECAP_KEM_FAIL
               : OSSL_SELF_TEST_DESC_DECAP_KEM);

    if (reject) {
        cipher_text = alloced = OPENSSL_zalloc(t->cipher_text.len);
        if (alloced == NULL)
            goto err;
        test_secret = t->reject_secret.buf;
    }

    ctx = EVP_PKEY_CTX_new_from_pkey(libctx, pkey, "");
    if (ctx == NULL)
        goto err;

    if (EVP_PKEY_decapsulate_init(ctx, NULL) <= 0)
        goto err;

    /* Allocate output buffer */
    secret = OPENSSL_malloc(secretlen);
    if (secret == NULL)
        goto err;

    /* Decapsulate */
    if (EVP_PKEY_decapsulate(ctx, secret, &secretlen,
            cipher_text, t->cipher_text.len)
        <= 0)
        goto err;

    /* Compare output */
    OSSL_SELF_TEST_oncorrupt_byte(st, secret);
    if (secretlen != t->secret.len
        || memcmp(secret, test_secret, t->secret.len) != 0)
        goto err;

    ret = 1;
err:
    OPENSSL_free(alloced);
    OPENSSL_free(secret);
    EVP_PKEY_CTX_free(ctx);
    OSSL_SELF_TEST_onend(st, ret);
    return ret;
}

/*
 * Test encapsulation, decapsulation for KEM.
 *
 * FIPS 140-3 IG 10.3.A resolution 14 mandates a CAST for:
 * 1   ML-KEM encapsulation
 * 2a  ML-KEM decapsulation non-rejection path
 * 2b  ML-KEM decapsulation implicit rejection path
 * 3   ML-KEM key generation
 */
static int self_test_kem(const ST_DEFINITION *t, OSSL_SELF_TEST *st,
    OSSL_LIB_CTX *libctx)
{
    int ret = 0;
    EVP_PKEY *pkey = NULL;
    EVP_PKEY_CTX *ctx;
    OSSL_PARAM *params = NULL;

    ctx = EVP_PKEY_CTX_new_from_name(libctx, t->algorithm, NULL);
    if (ctx == NULL)
        goto err;
    params = kat_params_to_ossl_params(libctx, t->u.kem.key, NULL);
    if (params == NULL)
        goto err;

    if (EVP_PKEY_fromdata_init(ctx) <= 0
        || EVP_PKEY_fromdata(ctx, &pkey, EVP_PKEY_KEYPAIR, params) <= 0)
        goto err;

    if (!self_test_kem_encapsulate(&t->u.kem, st, libctx, pkey)
        || !self_test_kem_decapsulate(&t->u.kem, st, libctx, pkey, 0)
        || !self_test_kem_decapsulate(&t->u.kem, st, libctx, pkey, 1))
        goto err;

    ret = 1;
err:
    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    OSSL_PARAM_free(params);
    return ret;
}
#endif /* defined(SELF_TEST_KEM_ENABLED) */

/*
 * Test an encrypt or decrypt KAT..
 *
 * FIPS 140-2 IG D.9 states that separate KAT tests are needed for encrypt
 * and decrypt..
 */
static int self_test_asym_cipher(const ST_DEFINITION *t, OSSL_SELF_TEST *st,
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
        || !add_params(keybld, t->u.ac.key, bnctx))
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
        || (t->u.ac.encrypt && EVP_PKEY_encrypt_init(encctx) <= 0)
        || (!t->u.ac.encrypt && EVP_PKEY_decrypt_init(encctx) <= 0))
        goto err;

    /* Add any additional parameters such as padding */
    if (t->u.ac.postinit != NULL) {
        initbld = OSSL_PARAM_BLD_new();
        if (initbld == NULL)
            goto err;
        if (!add_params(initbld, t->u.ac.postinit, bnctx))
            goto err;
        initparams = OSSL_PARAM_BLD_to_param(initbld);
        if (initparams == NULL)
            goto err;
        if (EVP_PKEY_CTX_set_params(encctx, initparams) <= 0)
            goto err;
    }

    if (t->u.ac.encrypt) {
        if (EVP_PKEY_encrypt(encctx, out, &outlen,
                t->pt.buf, t->pt.len)
            <= 0)
            goto err;
    } else {
        if (EVP_PKEY_decrypt(encctx, out, &outlen,
                t->pt.buf, t->pt.len)
            <= 0)
            goto err;
    }
    /* Check the KAT */
    OSSL_SELF_TEST_oncorrupt_byte(st, out);
    if (outlen != t->expected.len
        || memcmp(out, t->expected.buf, t->expected.len) != 0)
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

/* Test MAC algorithms */
static int self_test_mac(const ST_DEFINITION *t, OSSL_SELF_TEST *st,
    OSSL_LIB_CTX *libctx)
{
    int ret = 0;
    unsigned char out[EVP_MAX_MD_SIZE];
    size_t out_len = 0;
    EVP_MAC *mac = NULL;
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM *params = NULL;

    /* Currently used for integrity */
    OSSL_SELF_TEST_onbegin(st, OSSL_SELF_TEST_TYPE_KAT_MAC, t->desc);

    mac = EVP_MAC_fetch(libctx, t->algorithm, "");
    if (mac == NULL)
        goto err;

    ctx = EVP_MAC_CTX_new(mac);
    if (ctx == NULL)
        goto err;

    params = kat_params_to_ossl_params(libctx, t->u.mac.params, NULL);
    if (params == NULL)
        goto err;

    if (t->expected.len > sizeof(out))
        goto err;

    if (!EVP_MAC_init(ctx, NULL, 0, params)
        || !EVP_MAC_update(ctx, t->pt.buf, t->pt.len)
        || !EVP_MAC_final(ctx, out, &out_len, EVP_MAX_MD_SIZE))
        goto err;

    OSSL_SELF_TEST_oncorrupt_byte(st, out);

    if ((out_len != t->expected.len)
        || memcmp(out, t->expected.buf, t->expected.len) != 0)
        goto err;

    ret = 1;
err:
    EVP_MAC_free(mac);
    EVP_MAC_CTX_free(ctx);
    OSSL_PARAM_free(params);
    OSSL_SELF_TEST_onend(st, ret);
    return ret;
}

/*
 * Swap the library context DRBG for KAT testing
 *
 * In FIPS 140-3, the asymmetric POST must be a KAT, not a PCT.  For DSA and ECDSA,
 * the sign operation includes the random value 'k'.  For a KAT to work, we
 * have to have control of the DRBG to make sure it is in a "test" state, where
 * its output is truly deterministic.
 *
 */

/*
 * Replacement "random" sources
 * main_rand is used for most tests and it's set to generate mode.
 * kat_rand is used for KATs where specific input is mandated.
 */
static EVP_RAND_CTX *kat_rand = NULL;
static EVP_RAND_CTX *main_rand = NULL;

static int set_kat_drbg(OSSL_LIB_CTX *ctx,
    const unsigned char *entropy, size_t entropy_len,
    const unsigned char *nonce, size_t nonce_len,
    const unsigned char *persstr, size_t persstr_len)
{
    EVP_RAND *rand;
    unsigned int strength = 256;
    EVP_RAND_CTX *parent_rand = NULL;
    int reseed_time_interval = 0;
    unsigned int reseed_requests = 0;
    OSSL_PARAM drbg_params[3] = {
        OSSL_PARAM_END, OSSL_PARAM_END, OSSL_PARAM_END
    };

    /* If not NULL, we didn't cleanup from last call: BAD */
    if (kat_rand != NULL)
        return 0;

    rand = EVP_RAND_fetch(ctx, "TEST-RAND", NULL);
    if (rand == NULL)
        return 0;

    parent_rand = EVP_RAND_CTX_new(rand, NULL);
    EVP_RAND_free(rand);
    if (parent_rand == NULL)
        goto err;

    drbg_params[0] = OSSL_PARAM_construct_uint(OSSL_RAND_PARAM_STRENGTH,
        &strength);
    if (!EVP_RAND_CTX_set_params(parent_rand, drbg_params))
        goto err;

    rand = EVP_RAND_fetch(ctx, "HASH-DRBG", NULL);
    if (rand == NULL)
        goto err;

    kat_rand = EVP_RAND_CTX_new(rand, parent_rand);
    EVP_RAND_free(rand);
    if (kat_rand == NULL)
        goto err;

    drbg_params[0] = OSSL_PARAM_construct_utf8_string("digest", "SHA256", 0);
    if (!EVP_RAND_CTX_set_params(kat_rand, drbg_params))
        goto err;

    /* Instantiate the RNGs */
    drbg_params[0] = OSSL_PARAM_construct_octet_string(OSSL_RAND_PARAM_TEST_ENTROPY,
        (void *)entropy, entropy_len);
    drbg_params[1] = OSSL_PARAM_construct_octet_string(OSSL_RAND_PARAM_TEST_NONCE,
        (void *)nonce, nonce_len);
    if (!EVP_RAND_instantiate(parent_rand, strength, 0, NULL, 0, drbg_params))
        goto err;

    EVP_RAND_CTX_free(parent_rand);
    parent_rand = NULL;

    /* Disable time/request based reseeding to make selftests deterministic */
    drbg_params[0] = OSSL_PARAM_construct_int(OSSL_DRBG_PARAM_RESEED_TIME_INTERVAL,
        &reseed_time_interval);
    drbg_params[1] = OSSL_PARAM_construct_uint(OSSL_DRBG_PARAM_RESEED_REQUESTS,
        &reseed_requests);
    if (!EVP_RAND_instantiate(kat_rand, strength, 0, persstr, persstr_len, drbg_params))
        goto err;

    /* When we set the new private generator this one is freed, so upref it */
    if (!EVP_RAND_CTX_up_ref(main_rand))
        goto err;

    /* Update the library context DRBG */
    if (RAND_set0_private(ctx, kat_rand) > 0) {
        /* Keeping a copy to verify zeroization */
        if (EVP_RAND_CTX_up_ref(kat_rand))
            return 1;
        RAND_set0_private(ctx, main_rand);
    }

err:
    EVP_RAND_CTX_free(parent_rand);
    EVP_RAND_CTX_free(kat_rand);
    kat_rand = NULL;
    return 0;
}

static int reset_main_drbg(OSSL_LIB_CTX *ctx)
{
    int ret = 1;

    if (!RAND_set0_private(ctx, main_rand))
        ret = 0;
    if (kat_rand != NULL) {
        if (!EVP_RAND_uninstantiate(kat_rand)
            || !EVP_RAND_verify_zeroization(kat_rand))
            ret = 0;
        EVP_RAND_CTX_free(kat_rand);
        kat_rand = NULL;
    }
    return ret;
}

static int setup_main_random(OSSL_LIB_CTX *libctx)
{
    OSSL_PARAM drbg_params[3] = {
        OSSL_PARAM_END, OSSL_PARAM_END, OSSL_PARAM_END
    };
    unsigned int strength = 256, generate = 1;
    EVP_RAND *rand;

    rand = EVP_RAND_fetch(libctx, "TEST-RAND", NULL);
    if (rand == NULL)
        return 0;

    main_rand = EVP_RAND_CTX_new(rand, NULL);
    EVP_RAND_free(rand);
    if (main_rand == NULL)
        goto err;

    drbg_params[0] = OSSL_PARAM_construct_uint(OSSL_RAND_PARAM_GENERATE,
        &generate);
    drbg_params[1] = OSSL_PARAM_construct_uint(OSSL_RAND_PARAM_STRENGTH,
        &strength);

    if (!EVP_RAND_instantiate(main_rand, strength, 0, NULL, 0, drbg_params))
        goto err;
    return 1;
err:
    EVP_RAND_CTX_free(main_rand);
    /* Ensure this global variable does not reference freed memory */
    main_rand = NULL;
    return 0;
}

static int SELF_TEST_kats_single(OSSL_SELF_TEST *st, OSSL_LIB_CTX *libctx,
    self_test_id_t id)
{
    int ret;

    switch (st_all_tests[id].category) {
    case SELF_TEST_KAT_DIGEST:
        ret = self_test_digest(&st_all_tests[id], st, libctx);
        break;
    case SELF_TEST_KAT_CIPHER:
        ret = self_test_cipher(&st_all_tests[id], st, libctx);
        break;
    case SELF_TEST_KAT_SIGNATURE:
        ret = self_test_digest_sign(&st_all_tests[id], st, libctx);
        break;
    case SELF_TEST_KAT_KDF:
        ret = self_test_kdf(&st_all_tests[id], st, libctx);
        break;
    case SELF_TEST_DRBG:
        ret = self_test_drbg(&st_all_tests[id], st, libctx);
        break;
#if defined(SELF_TEST_KA_ENABLED)
    case SELF_TEST_KAT_KAS:
        ret = self_test_ka(&st_all_tests[id], st, libctx);
        break;
#endif
#if defined(SELF_TEST_ASYM_KEYGEN_ENABLED)
    case SELF_TEST_KAT_ASYM_KEYGEN:
        ret = self_test_asym_keygen(&st_all_tests[id], st, libctx);
        break;
#endif
#if defined(SELF_TEST_KEM_ENABLED)
    case SELF_TEST_KAT_KEM:
        ret = self_test_kem(&st_all_tests[id], st, libctx);
        break;
#endif
    case SELF_TEST_KAT_ASYM_CIPHER:
        ret = self_test_asym_cipher(&st_all_tests[id], st, libctx);
        break;
    case SELF_TEST_KAT_MAC:
        ret = self_test_mac(&st_all_tests[id], st, libctx);
        break;
    default:
        ret = 0;
        break;
    }
    if (ret) {
        if (!ossl_set_self_test_state(id, SELF_TEST_STATE_PASSED))
            return 0;
    } else {
        ossl_set_self_test_state(id, SELF_TEST_STATE_FAILED);
        ERR_raise(ERR_LIB_PROV, PROV_R_SELF_TEST_KAT_FAILURE);
    }

    return ret;
}

static int SELF_TEST_kat_deps(OSSL_SELF_TEST *st, OSSL_LIB_CTX *libctx,
    ST_DEFINITION *test)
{
    if (test->depends_on == NULL)
        return 0;

    for (int i = 0; test->depends_on[i] != ST_ID_MAX; i++)
        if (!SELF_TEST_kats_execute(st, libctx, test->depends_on[i], 0))
            return 0;

    return 1;
}

/*
 * Run a single algorithm KAT, and its dependencies.
 * Return 1 if successful, otherwise return 0.
 */
int SELF_TEST_kats_execute(OSSL_SELF_TEST *st, OSSL_LIB_CTX *libctx,
    self_test_id_t id, int switch_rand)
{
    EVP_RAND_CTX *saved_rand = NULL;
    int ret;

    if (id >= ST_ID_MAX || st_all_tests[id].id != id) {
        ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_CONFIG_DATA);
        return 0;
    }

    /*
     * Dependency chains may cause a test to be referenced multiple times,
     * immediately return if not in initial state.
     * NOTE: In this function state can be read w/o atomics because this
     * function is always executed under lock. However we need to use
     * atomics to set the state so that other threads reading state always
     * read a correct value.
     */
    switch (st_all_tests[id].state) {
    case SELF_TEST_STATE_INIT:
    case SELF_TEST_STATE_DEFER:
        break;
    case SELF_TEST_STATE_FAILED:
        return 0;
    case SELF_TEST_STATE_IN_PROGRESS:
    case SELF_TEST_STATE_PASSED:
    case SELF_TEST_STATE_IMPLICIT:
        return 1;
    }

    if (switch_rand) {
        saved_rand = ossl_rand_get0_private_noncreating(libctx);
        if (saved_rand != NULL && !EVP_RAND_CTX_up_ref(saved_rand))
            return 0;
        if (!setup_main_random(libctx)
            || !RAND_set0_private(libctx, main_rand)) {
            /* Decrement saved_rand reference counter */
            EVP_RAND_CTX_free(saved_rand);
            EVP_RAND_CTX_free(main_rand);
            /* Ensure this global variable does not reference freed memory */
            main_rand = NULL;
            return 0;
        }
    }

    /* Mark test as in progress */
    if (!ossl_set_self_test_state(id, SELF_TEST_STATE_IN_PROGRESS))
        return 0;

    /* check if there are dependent tests to run */
    if (st_all_tests[id].depends_on) {
        if (!SELF_TEST_kat_deps(st, libctx, &st_all_tests[id])) {
            ret = 0;
            goto done;
        }
    }

    /* may have already been run through dependency chains */
    switch (st_all_tests[id].state) {
    case SELF_TEST_STATE_IN_PROGRESS:
        ret = SELF_TEST_kats_single(st, libctx, id);
        break;
    case SELF_TEST_STATE_PASSED:
        ret = 1;
        break;
    default:
        /* ensure all states are set to failed if we get here */
        ossl_set_self_test_state(id, SELF_TEST_STATE_FAILED);
        ret = 0;
    }

    /*
     * if an implicit algorithm has explicit dependencies we want to
     * ensure they are all executed as well otherwise we could not
     * mark it as passed.
     */
    if (st_all_tests[id].state == SELF_TEST_STATE_PASSED)
        for (int i = 0; i < ST_ID_MAX; i++) {
            if (st_all_tests[i].state == SELF_TEST_STATE_IMPLICIT
                && st_all_tests[i].depends_on != NULL)
                if (!(ret = SELF_TEST_kat_deps(st, libctx, &st_all_tests[i])))
                    break;
        }

done:
    /*
     * now mark (pass or fail) all the algorithm tests that have been marked
     * by this test implicitly tested.
     */
    for (int i = 0; i < ST_ID_MAX; i++) {
        if (st_all_tests[i].state == SELF_TEST_STATE_IMPLICIT)
            ossl_set_self_test_state(i, st_all_tests[id].state);
    }

    if (switch_rand) {
        RAND_set0_private(libctx, saved_rand);
        /* The above call will cause main_rand to be freed */
        main_rand = NULL;
    }
    return ret;
}

/*
 * Run the algorithm KAT's.
 * Return 1 is successful, otherwise return 0.
 * This runs all the tests regardless of if any fail, but it will not forcibly
 * run tests that have been implicitly satisfied.
 */
int SELF_TEST_kats(OSSL_SELF_TEST *st, OSSL_LIB_CTX *libctx)
{
    EVP_RAND_CTX *saved_rand = ossl_rand_get0_private_noncreating(libctx);
    int i, ret = 1;

    if (saved_rand != NULL && !EVP_RAND_CTX_up_ref(saved_rand))
        return 0;
    if (!setup_main_random(libctx)
        || !RAND_set0_private(libctx, main_rand)) {
        /* Decrement saved_rand reference counter */
        EVP_RAND_CTX_free(saved_rand);
        EVP_RAND_CTX_free(main_rand);
        /* Ensure this global variable does not reference freed memory */
        main_rand = NULL;
        return 0;
    }

    for (i = 0; i < ST_ID_MAX; i++) {
        if (st_all_tests[i].state == SELF_TEST_STATE_INIT)
            if (!SELF_TEST_kats_execute(st, libctx, i, 0))
                ret = 0;
    }

    RAND_set0_private(libctx, saved_rand);
    /* The above call will cause main_rand to be freed */
    main_rand = NULL;
    return ret;
}

int ossl_self_test_in_progress(self_test_id_t id)
{
    enum st_test_state state;

    if (id >= ST_ID_MAX)
        return 0;

    if (!ossl_get_self_test_state(id, &state))
        return 0;

    if (state == SELF_TEST_STATE_IN_PROGRESS)
        return 1;
    return 0;
}
