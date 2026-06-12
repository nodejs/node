/*
 * Copyright 2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/core_names.h>
#include <openssl/decoder.h>
#include <openssl/evp.h>
#include "crypto/lms.h"
#include "internal/nelem.h"
#include "testutil.h"
#include "lms.inc"

typedef enum OPTION_choice {
    OPT_ERR = -1,
    OPT_EOF = 0,
    OPT_CONFIG_FILE,
    OPT_TEST_ENUM
} OPTION_CHOICE;

static OSSL_LIB_CTX *libctx = NULL;
static char *propq = NULL;
static OSSL_PROVIDER *nullprov = NULL;
static OSSL_PROVIDER *libprov = NULL;

static EVP_PKEY *lms_pubkey_from_data(const unsigned char *data, size_t datalen)
{
    int ret;
    EVP_PKEY_CTX *ctx = NULL;
    EVP_PKEY *key = NULL;
    OSSL_PARAM params[2];

    params[0] = OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PUB_KEY,
                                                  (unsigned char *)data, datalen);
    params[1] = OSSL_PARAM_construct_end();
    ret = TEST_ptr(ctx = EVP_PKEY_CTX_new_from_name(libctx, "LMS", propq))
        && TEST_int_eq(EVP_PKEY_fromdata_init(ctx), 1)
        && (EVP_PKEY_fromdata(ctx, &key, EVP_PKEY_PUBLIC_KEY, params) == 1);
    if (ret == 0) {
        EVP_PKEY_free(key);
        key = NULL;
    }
    EVP_PKEY_CTX_free(ctx);
    return key;
}

static int lms_bad_pub_len_test(void)
{
    int ret = 0;
    LMS_ACVP_TEST_DATA *td = &lms_testdata[1];
    EVP_PKEY *pkey = NULL;
    size_t publen = 0;
    unsigned char pubdata[128];

    if (!TEST_size_t_le(td->publen + 16, sizeof(pubdata)))
        goto end;

    OPENSSL_cleanse(pubdata, sizeof(pubdata));
    memcpy(pubdata, td->pub, td->publen);

    for (publen = 0; publen <= td->publen + 16; publen += 3) {
        if (publen == td->publen)
            continue;
        if (!TEST_ptr_null(pkey = lms_pubkey_from_data(pubdata, publen)))
            goto end;
    }
    ret = 1;
end:
    if (ret == 0)
        TEST_note("Incorrectly accepted public key of length %u (expected %u)",
                  (unsigned)publen, (unsigned)td->publen);
    EVP_PKEY_free(pkey);

    return ret == 1;
}

static int lms_pubkey_decoder_fail_test(void)
{
    int ret = 0;
    EVP_PKEY *pkey = NULL;
    OSSL_DECODER_CTX *dctx = NULL;
    int selection = 0;
    LMS_ACVP_TEST_DATA *td = &lms_testdata[0];
    const unsigned char *pdata;
    size_t pdatalen;
    static const unsigned char pub_bad_LMSType[] = {
        0x00, 0x00, 0x00, 0xAA
    };
    static const unsigned char pub_bad_OTSType[] = {
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xAA
    };

    if (!TEST_ptr(dctx = OSSL_DECODER_CTX_new_for_pkey(&pkey, NULL, NULL, "LMS",
                                                       selection,
                                                       libctx, NULL)))
        return 0;

    pdata = td->pub;
    pdatalen = 3;
    if (!TEST_false(OSSL_DECODER_from_data(dctx, &pdata, &pdatalen)))
        goto end;

    pdatalen = SIZE_MAX;
    if (!TEST_false(OSSL_DECODER_from_data(dctx, &pdata, &pdatalen)))
        goto end;

    pdata = pub_bad_LMSType;
    pdatalen = sizeof(pub_bad_LMSType);
    if (!TEST_false(OSSL_DECODER_from_data(dctx, &pdata, &pdatalen)))
        goto end;

    pdata = pub_bad_OTSType;
    pdatalen = sizeof(pub_bad_OTSType);
    if (!TEST_false(OSSL_DECODER_from_data(dctx, &pdata, &pdatalen)))
        goto end;

    ret = 1;
end:
    EVP_PKEY_free(pkey);
    OSSL_DECODER_CTX_free(dctx);
    return ret;
}

static EVP_PKEY *key_decode_from_bio(BIO *bio, const char *keytype)
{
    EVP_PKEY *pkey = NULL;
    OSSL_DECODER_CTX *dctx = NULL;
    int selection = 0;

    if (!TEST_ptr(dctx = OSSL_DECODER_CTX_new_for_pkey(&pkey, NULL, NULL,
                                                       keytype,
                                                       selection,
                                                       libctx, NULL)))
        return NULL;

    if (!TEST_true(OSSL_DECODER_from_bio(dctx, bio))) {
        EVP_PKEY_free(pkey);
        pkey = NULL;
    }
    OSSL_DECODER_CTX_free(dctx);
    return pkey;
}

static EVP_PKEY *key_decode_from_data(const unsigned char *data, size_t datalen,
                                      const char *keytype)
{
    BIO *bio;
    EVP_PKEY *key = NULL;

    if (!TEST_ptr(bio = BIO_new_mem_buf(data, (int)datalen)))
        return NULL;
    key = key_decode_from_bio(bio, keytype);
    BIO_free(bio);
    return key;
}

static int lms_key_decode_test(void)
{
    int ret = 0;
    LMS_ACVP_TEST_DATA *td1 = &lms_testdata[0];
    EVP_PKEY *key = NULL;

    ret = TEST_ptr(key = key_decode_from_data(td1->pub, td1->publen, NULL));
    EVP_PKEY_free(key);
    return ret;
}

static int lms_pubkey_decoder_test(void)
{
    int ret = 0;
    LMS_ACVP_TEST_DATA *td = &lms_testdata[0];
    EVP_PKEY *pub = NULL;
    OSSL_DECODER_CTX *dctx = NULL;
    const unsigned char *data;
    size_t data_len;

    if (!TEST_ptr(dctx = OSSL_DECODER_CTX_new_for_pkey(&pub, "xdr", NULL,
                                                       "LMS",
                                                       OSSL_KEYMGMT_SELECT_PUBLIC_KEY,
                                                       libctx, NULL)))
        goto err;
    data = td->pub;
    data_len = td->publen;
    if (!TEST_true(OSSL_DECODER_from_data(dctx, &data, &data_len)))
        goto err;
    ret = 1;
err:
    EVP_PKEY_free(pub);
    OSSL_DECODER_CTX_free(dctx);
    return ret;
}

static int lms_key_eq_test(void)
{
    int ret = 0;
    EVP_PKEY *key[4];
    LMS_ACVP_TEST_DATA *td1 = &lms_testdata[0];
    LMS_ACVP_TEST_DATA *td2 = &lms_testdata[1];
    size_t i;
#ifndef OPENSSL_NO_EC
    EVP_PKEY *eckey = NULL;
#endif

    for (i = 0; i < OSSL_NELEM(key); i++)
        key[i] = NULL;

    if (!TEST_ptr(key[0] = lms_pubkey_from_data(td1->pub, td1->publen))
            || !TEST_ptr(key[1] = lms_pubkey_from_data(td1->pub, td1->publen))
            || !TEST_ptr(key[2] = lms_pubkey_from_data(td2->pub, td2->publen))
            || !TEST_ptr(key[3] = key_decode_from_data(td1->pub, td1->publen,
                                                       NULL)))
        goto end;

    ret = TEST_int_eq(EVP_PKEY_eq(key[0], key[1]), 1)
        && TEST_int_ne(EVP_PKEY_eq(key[0], key[2]), 1)
        && TEST_int_eq(EVP_PKEY_eq(key[0], key[3]), 1);
    if (ret == 0)
        goto end;

#ifndef OPENSSL_NO_EC
    if (!TEST_ptr(eckey = EVP_PKEY_Q_keygen(libctx, NULL, "EC", "P-256")))
        goto end;
    ret = TEST_int_ne(EVP_PKEY_eq(key[0], eckey), 1);
    EVP_PKEY_free(eckey);
#endif
end:
    EVP_PKEY_free(key[3]);
    EVP_PKEY_free(key[2]);
    EVP_PKEY_free(key[1]);
    EVP_PKEY_free(key[0]);
    return ret;
}

static int lms_key_validate_test(void)
{
    int ret = 0;
    LMS_ACVP_TEST_DATA *td = &lms_testdata[0];
    EVP_PKEY_CTX *vctx = NULL;
    EVP_PKEY *key = NULL;

    if (!TEST_ptr(key = lms_pubkey_from_data(td->pub, td->publen)))
        return 0;
    if (!TEST_ptr(vctx = EVP_PKEY_CTX_new_from_pkey(libctx, key, NULL)))
        goto end;
    ret = TEST_int_eq(EVP_PKEY_check(vctx), 1);
    EVP_PKEY_CTX_free(vctx);
end:
    EVP_PKEY_free(key);
    return ret;
}

static int lms_verify_test(int tst)
{
    int ret = 0;
    LMS_ACVP_TEST_DATA *td = &lms_testdata[tst];
    EVP_PKEY_CTX *ctx = NULL;
    EVP_PKEY *pkey = NULL;
    EVP_SIGNATURE *sig = NULL;

    ret = TEST_ptr(pkey = lms_pubkey_from_data(td->pub, td->publen))
        && TEST_ptr(sig = EVP_SIGNATURE_fetch(libctx, "LMS", NULL))
        && TEST_ptr(ctx = EVP_PKEY_CTX_new_from_pkey(libctx, pkey, NULL))
        && TEST_int_eq(EVP_PKEY_verify_message_init(ctx, sig, NULL), 1)
        && TEST_int_eq(EVP_PKEY_verify(ctx, td->sig, td->siglen,
                                       td->msg, td->msglen), 1);

    EVP_PKEY_free(pkey);
    EVP_PKEY_CTX_free(ctx);
    EVP_SIGNATURE_free(sig);
    return ret;
}

static int lms_digest_verify_fail_test(void)
{
    int ret = 0;
    LMS_ACVP_TEST_DATA *td = &lms_testdata[0];
    EVP_PKEY *pub = NULL;
    EVP_MD_CTX *vctx = NULL;

    if (!TEST_ptr(pub = lms_pubkey_from_data(td->pub, td->publen)))
        return 0;
    if (!TEST_ptr(vctx = EVP_MD_CTX_new()))
        goto err;
    /* Only one shot mode is supported, streaming fails to initialise */
    if (!TEST_int_eq(EVP_DigestVerifyInit_ex(vctx, NULL, NULL, libctx, NULL,
                                             pub, NULL), 0))
        goto err;
    ret = 1;
 err:
    EVP_PKEY_free(pub);
    EVP_MD_CTX_free(vctx);
    return ret;
}

static int lms_digest_signing_fail_test(void)
{
    int ret = 0;
    LMS_ACVP_TEST_DATA *td = &lms_testdata[0];
    EVP_PKEY *pub = NULL;
    EVP_MD_CTX *vctx = NULL;

    if (!TEST_ptr(pub = lms_pubkey_from_data(td->pub, td->publen)))
        return 0;
    if (!TEST_ptr(vctx = EVP_MD_CTX_new()))
        goto err;
    if (!TEST_int_eq(EVP_DigestSignInit_ex(vctx, NULL, NULL, libctx, NULL,
                                           pub, NULL), 0))
        goto err;
    ret = 1;
 err:
    EVP_PKEY_free(pub);
    EVP_MD_CTX_free(vctx);
    return ret;
}

static int lms_message_signing_fail_test(void)
{
    int ret = 0;
    LMS_ACVP_TEST_DATA *td = &lms_testdata[0];
    EVP_PKEY_CTX *ctx = NULL;
    EVP_PKEY *pkey = NULL;
    EVP_SIGNATURE *sig = NULL;

    ret = TEST_ptr(pkey = lms_pubkey_from_data(td->pub, td->publen))
        && TEST_ptr(sig = EVP_SIGNATURE_fetch(libctx, "LMS", NULL))
        && TEST_ptr(ctx = EVP_PKEY_CTX_new_from_pkey(libctx, pkey, NULL))
        && TEST_int_eq(EVP_PKEY_sign_message_init(ctx, sig, NULL), -2);

    EVP_PKEY_free(pkey);
    EVP_PKEY_CTX_free(ctx);
    EVP_SIGNATURE_free(sig);
    return ret;
}

static int lms_paramgen_fail_test(void)
{
    int ret;
    EVP_PKEY_CTX *ctx = NULL;

    ret = TEST_ptr(ctx = EVP_PKEY_CTX_new_from_name(libctx, "LMS", NULL))
        && TEST_int_eq(EVP_PKEY_paramgen_init(ctx), -2);

    EVP_PKEY_CTX_free(ctx);
    return ret;
}

static int lms_keygen_fail_test(void)
{
    int ret;
    EVP_PKEY_CTX *ctx = NULL;

    ret = TEST_ptr(ctx = EVP_PKEY_CTX_new_from_name(libctx, "LMS", NULL))
        && TEST_int_eq(EVP_PKEY_keygen_init(ctx), -2);

    EVP_PKEY_CTX_free(ctx);
    return ret;
}

static int lms_verify_fail_test(void)
{
    int ret = 0;
    LMS_ACVP_TEST_DATA *td = &lms_testdata[0];
    EVP_PKEY_CTX *ctx = NULL;
    EVP_PKEY *pkey = NULL;

    if (!TEST_ptr(pkey = lms_pubkey_from_data(td->pub, td->publen))
            || !TEST_ptr(ctx = EVP_PKEY_CTX_new_from_pkey(libctx, pkey, NULL)))
        goto end;
    /* Only one shot mode is supported, streaming fails to initialise */
    if (!TEST_int_eq(EVP_PKEY_verify_init(ctx), -2))
        goto end;
    ret = 1;
end:
    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    return ret;
}

static int lms_verify_bad_sig_test(void)
{
    int ret = 0, i = 0;
    LMS_ACVP_TEST_DATA *td = &lms_testdata[1];
    EVP_PKEY *pkey = NULL;
    EVP_SIGNATURE *sig = NULL;
    EVP_PKEY_CTX *ctx = NULL;
    unsigned char *sig_data = NULL, corrupt_mask = 0x01;
    /*
     * Corrupt every 3rd byte to run less tests. The smallest element of an XDR
     * encoding is 4 bytes, so this will corrupt every element.
     * Memory sanitisation is slow, so a larger step size is used for this.
     */
#if defined(OSSL_SANITIZE_MEMORY)
    const int step = (int)(td->siglen >> 1);
#else
    const int step = 3;
#endif

    /* Copy the signature so that we can corrupt it */
    sig_data = OPENSSL_memdup(td->sig, td->siglen);
    if (sig_data == NULL)
        return 0;

    if (!TEST_ptr(pkey = lms_pubkey_from_data(td->pub, td->publen))
            || !TEST_ptr(sig = EVP_SIGNATURE_fetch(libctx, "LMS", NULL))
            || !TEST_ptr(ctx = EVP_PKEY_CTX_new_from_pkey(libctx, pkey, NULL)))
        goto end;

    if (!TEST_int_eq(EVP_PKEY_verify_message_init(ctx, sig, NULL), 1))
        goto end;

    for (i = 0; i < (int)td->siglen; i += step) {
        sig_data[i] ^= corrupt_mask; /* corrupt a byte */
        if (i > 0)
            sig_data[i - step] ^= corrupt_mask; /* Reset the previously corrupt byte */

        if (!TEST_int_eq(EVP_PKEY_verify(ctx, sig_data, td->siglen,
                                         td->msg, td->msglen), 0)) {
            TEST_note("Incorrectly passed when %dth byte of signature"
                      " was corrupted", i);
            goto end;
        }
    }

    ret = 1;
end:
    EVP_SIGNATURE_free(sig);
    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    OPENSSL_free(sig_data);

    return ret;
}

/*
 * Test that using the incorrect signature lengths (both shorter and longer)
 * fail.
 * NOTE: It does not get an out of bounds read due to the signature
 * knowing how large it should be
 */
static int lms_verify_bad_sig_len_test(void)
{
    int ret = 0;
    LMS_ACVP_TEST_DATA *td = &lms_testdata[1];
    EVP_PKEY *pkey = NULL;
    EVP_SIGNATURE *sig = NULL;
    EVP_PKEY_CTX *ctx = NULL;
    size_t siglen = 0;
    const int step = 3;
    unsigned char sigdata[4096];

    if (!TEST_ptr(pkey = lms_pubkey_from_data(td->pub, td->publen))
            || !TEST_ptr(sig = EVP_SIGNATURE_fetch(libctx, "LMS", NULL))
            || !TEST_ptr(ctx = EVP_PKEY_CTX_new_from_pkey(libctx, pkey, NULL)))
        goto end;

    if (!TEST_size_t_le(td->siglen + 16, sizeof(sigdata)))
        goto end;

    OPENSSL_cleanse(sigdata, sizeof(sigdata));
    memcpy(sigdata, td->sig, td->siglen);

    ret = 0;
    for (siglen = 0; siglen < td->siglen + 16; siglen += step) {
        if (siglen == td->siglen)   /* ignore the size that should pass */
            continue;
        if (!TEST_int_eq(EVP_PKEY_verify_message_init(ctx, sig, NULL), 1))
            goto end;
        if (!TEST_int_eq(EVP_PKEY_verify(ctx, sigdata, siglen,
                                         td->msg, td->msglen), 0)) {
            TEST_note("Incorrectly accepted signature key of length"
                      " %u (expected %u)", (unsigned)siglen, (unsigned)td->siglen);
            goto end;
        }
    }

    ret = 1;
end:
    EVP_SIGNATURE_free(sig);
    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(pkey);

    return ret;
}

static int lms_verify_bad_pub_sig_test(void)
{
    LMS_ACVP_TEST_DATA *td = &lms_testdata[1];
    int ret = 0, i = 0;
    EVP_PKEY *pkey = NULL;
    EVP_SIGNATURE *sig = NULL;
    EVP_PKEY_CTX *ctx = NULL;
    unsigned char *pub = NULL;
    const int step = 1;

    /* Copy the public key data so that we can corrupt it */
    if (!TEST_ptr(pub = OPENSSL_memdup(td->pub, td->publen)))
        return 0;

    if (!TEST_ptr(sig = EVP_SIGNATURE_fetch(libctx, "LMS", NULL)))
        goto end;

    for (i = 0; i < (int)td->publen; i += step) {
        pub[i] ^= 1; /* corrupt a byte */
        /* Corrupting the public key may cause the key load to fail */
        pkey = lms_pubkey_from_data(pub, td->publen);
        if (pkey != NULL) {
            if (!TEST_ptr(ctx = EVP_PKEY_CTX_new_from_pkey(libctx, pkey, NULL)))
                goto end;
            /* We expect the verify to fail */
            if ((EVP_PKEY_verify_message_init(ctx, sig, NULL) == 1)
                    && !TEST_int_eq(EVP_PKEY_verify(ctx, td->sig, td->siglen,
                                                    td->msg, td->msglen), 0)) {
                TEST_note("Incorrectly passed when byte %d of the public key"
                          " was corrupted", i);
                goto end;
            }
            EVP_PKEY_free(pkey);
            pkey = NULL;
            EVP_PKEY_CTX_free(ctx);
            ctx = NULL;
        }
        pub[i] ^= 1; /* restore the corrupted byte */
    }

    ret = 1;
end:
    EVP_SIGNATURE_free(sig);
    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    OPENSSL_free(pub);

    return ret;
}

const OPTIONS *test_get_options(void)
{
    static const OPTIONS options[] = {
        OPT_TEST_OPTIONS_DEFAULT_USAGE,
        { "config", OPT_CONFIG_FILE, '<',
          "The configuration file to use for the libctx" },
        { NULL }
    };
    return options;
}

int setup_tests(void)
{
    OPTION_CHOICE o;
    char *config_file = NULL;
    EVP_PKEY_CTX *ctx = NULL;

    /* Swap the libctx to test non-default context only */
    propq = "provider=default";

    while ((o = opt_next()) != OPT_EOF) {
        switch (o) {
        case OPT_CONFIG_FILE:
            config_file = opt_arg();
            propq = "";
            break;
        case OPT_TEST_CASES:
            break;
        default:
        case OPT_ERR:
            return 0;
        }
    }
    if (!test_get_libctx(&libctx, &nullprov, config_file, &libprov, NULL))
        return 0;

    ctx = EVP_PKEY_CTX_new_from_name(libctx, "LMS", propq);
    if (ctx == NULL && ERR_get_error() == EVP_R_UNSUPPORTED_ALGORITHM)
        return TEST_skip("LMS algorithm is not available in provider");
    EVP_PKEY_CTX_free(ctx);

    ADD_TEST(lms_bad_pub_len_test);
    ADD_TEST(lms_key_validate_test);
    ADD_TEST(lms_key_eq_test);
    ADD_TEST(lms_key_decode_test);
    ADD_TEST(lms_pubkey_decoder_test);
    ADD_TEST(lms_pubkey_decoder_fail_test);
    ADD_ALL_TESTS(lms_verify_test, OSSL_NELEM(lms_testdata));
    ADD_TEST(lms_verify_fail_test);
    ADD_TEST(lms_digest_verify_fail_test);
    ADD_TEST(lms_digest_signing_fail_test);
    ADD_TEST(lms_message_signing_fail_test);
    ADD_TEST(lms_paramgen_fail_test);
    ADD_TEST(lms_keygen_fail_test);
    ADD_TEST(lms_verify_bad_sig_test);
    ADD_TEST(lms_verify_bad_sig_len_test);
    ADD_TEST(lms_verify_bad_pub_sig_test);

    return 1;
}

void cleanup_tests(void)
{
    OSSL_PROVIDER_unload(nullprov);
    OSSL_PROVIDER_unload(libprov);
    OSSL_LIB_CTX_free(libctx);
}
