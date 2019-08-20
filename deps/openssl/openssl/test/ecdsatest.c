/*
 * Copyright 2002-2019 The OpenSSL Project Authors. All Rights Reserved.
 * Copyright (c) 2002, Oracle and/or its affiliates. All rights reserved
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/opensslconf.h> /* To see if OPENSSL_NO_EC is defined */
#include "testutil.h"

#ifndef OPENSSL_NO_EC

# include <openssl/evp.h>
# include <openssl/bn.h>
# include <openssl/ec.h>
# include <openssl/rand.h>
# include "internal/nelem.h"
# include "ecdsatest.h"

/* functions to change the RAND_METHOD */
static int fbytes(unsigned char *buf, int num);

static RAND_METHOD fake_rand;
static const RAND_METHOD *old_rand;
static int use_fake = 0;
static const char *numbers[2];
static size_t crv_len = 0;
static EC_builtin_curve *curves = NULL;

static int change_rand(void)
{
    /* save old rand method */
    if (!TEST_ptr(old_rand = RAND_get_rand_method()))
        return 0;

    fake_rand = *old_rand;
    /* use own random function */
    fake_rand.bytes = fbytes;
    /* set new RAND_METHOD */
    if (!TEST_true(RAND_set_rand_method(&fake_rand)))
        return 0;
    return 1;
}

static int restore_rand(void)
{
    if (!TEST_true(RAND_set_rand_method(old_rand)))
        return 0;
    return 1;
}

static int fbytes(unsigned char *buf, int num)
{
    int ret = 0;
    static int fbytes_counter = 0;
    BIGNUM *tmp = NULL;

    if (use_fake == 0)
        return old_rand->bytes(buf, num);

    use_fake = 0;

    if (!TEST_ptr(tmp = BN_new())
        || !TEST_int_lt(fbytes_counter, OSSL_NELEM(numbers))
        || !TEST_true(BN_hex2bn(&tmp, numbers[fbytes_counter]))
        /* tmp might need leading zeros so pad it out */
        || !TEST_int_le(BN_num_bytes(tmp), num)
        || !TEST_true(BN_bn2binpad(tmp, buf, num)))
        goto err;

    fbytes_counter = (fbytes_counter + 1) % OSSL_NELEM(numbers);
    ret = 1;
 err:
    BN_free(tmp);
    return ret;
}

/*-
 * This function hijacks the RNG to feed it the chosen ECDSA key and nonce.
 * The ECDSA KATs are from:
 * - the X9.62 draft (4)
 * - NIST CAVP (720)
 *
 * It uses the low-level ECDSA_sign_setup instead of EVP to control the RNG.
 * NB: This is not how applications should use ECDSA; this is only for testing.
 *
 * Tests the library can successfully:
 * - generate public keys that matches those KATs
 * - create ECDSA signatures that match those KATs
 * - accept those signatures as valid
 */
static int x9_62_tests(int n)
{
    int nid, md_nid, ret = 0;
    const char *r_in = NULL, *s_in = NULL, *tbs = NULL;
    unsigned char *pbuf = NULL, *qbuf = NULL, *message = NULL;
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int dgst_len = 0;
    long q_len, msg_len = 0;
    size_t p_len;
    EVP_MD_CTX *mctx = NULL;
    EC_KEY *key = NULL;
    ECDSA_SIG *signature = NULL;
    BIGNUM *r = NULL, *s = NULL;
    BIGNUM *kinv = NULL, *rp = NULL;
    const BIGNUM *sig_r = NULL, *sig_s = NULL;

    nid = ecdsa_cavs_kats[n].nid;
    md_nid = ecdsa_cavs_kats[n].md_nid;
    r_in = ecdsa_cavs_kats[n].r;
    s_in = ecdsa_cavs_kats[n].s;
    tbs = ecdsa_cavs_kats[n].msg;
    numbers[0] = ecdsa_cavs_kats[n].d;
    numbers[1] = ecdsa_cavs_kats[n].k;

    TEST_info("ECDSA KATs for curve %s", OBJ_nid2sn(nid));

    if (!TEST_ptr(mctx = EVP_MD_CTX_new())
        /* get the message digest */
        || !TEST_ptr(message = OPENSSL_hexstr2buf(tbs, &msg_len))
        || !TEST_true(EVP_DigestInit_ex(mctx, EVP_get_digestbynid(md_nid), NULL))
        || !TEST_true(EVP_DigestUpdate(mctx, message, msg_len))
        || !TEST_true(EVP_DigestFinal_ex(mctx, digest, &dgst_len))
        /* create the key */
        || !TEST_ptr(key = EC_KEY_new_by_curve_name(nid))
        /* load KAT variables */
        || !TEST_ptr(r = BN_new())
        || !TEST_ptr(s = BN_new())
        || !TEST_true(BN_hex2bn(&r, r_in))
        || !TEST_true(BN_hex2bn(&s, s_in))
        /* swap the RNG source */
        || !TEST_true(change_rand()))
        goto err;

    /* public key must match KAT */
    use_fake = 1;
    if (!TEST_true(EC_KEY_generate_key(key))
        || !TEST_true(p_len = EC_KEY_key2buf(key, POINT_CONVERSION_UNCOMPRESSED,
                                             &pbuf, NULL))
        || !TEST_ptr(qbuf = OPENSSL_hexstr2buf(ecdsa_cavs_kats[n].Q, &q_len))
        || !TEST_int_eq(q_len, p_len)
        || !TEST_mem_eq(qbuf, q_len, pbuf, p_len))
        goto err;

    /* create the signature via ECDSA_sign_setup to avoid use of ECDSA nonces */
    use_fake = 1;
    if (!TEST_true(ECDSA_sign_setup(key, NULL, &kinv, &rp))
        || !TEST_ptr(signature = ECDSA_do_sign_ex(digest, dgst_len,
                                                  kinv, rp, key))
        /* verify the signature */
        || !TEST_int_eq(ECDSA_do_verify(digest, dgst_len, signature, key), 1))
        goto err;

    /* compare the created signature with the expected signature */
    ECDSA_SIG_get0(signature, &sig_r, &sig_s);
    if (!TEST_BN_eq(sig_r, r)
        || !TEST_BN_eq(sig_s, s))
        goto err;

    ret = 1;

 err:
    /* restore the RNG source */
    if (!TEST_true(restore_rand()))
        ret = 0;

    OPENSSL_free(message);
    OPENSSL_free(pbuf);
    OPENSSL_free(qbuf);
    EC_KEY_free(key);
    ECDSA_SIG_free(signature);
    BN_free(r);
    BN_free(s);
    EVP_MD_CTX_free(mctx);
    BN_clear_free(kinv);
    BN_clear_free(rp);
    return ret;
}

/*-
 * Positive and negative ECDSA testing through EVP interface:
 * - EVP_DigestSign (this is the one-shot version)
 * - EVP_DigestVerify
 *
 * Tests the library can successfully:
 * - create a key
 * - create a signature
 * - accept that signature
 * - reject that signature with a different public key
 * - reject that signature if its length is not correct
 * - reject that signature after modifying the message
 * - accept that signature after un-modifying the message
 * - reject that signature after modifying the signature
 * - accept that signature after un-modifying the signature
 */
static int test_builtin(int n)
{
    EC_KEY *eckey_neg = NULL, *eckey = NULL;
    unsigned char dirt, offset, tbs[128];
    unsigned char *sig = NULL;
    EVP_PKEY *pkey_neg = NULL, *pkey = NULL;
    EVP_MD_CTX *mctx = NULL;
    size_t sig_len;
    int nid, ret = 0;

    nid = curves[n].nid;

    /* skip built-in curves where ord(G) is not prime */
    if (nid == NID_ipsec4 || nid == NID_ipsec3) {
        TEST_info("skipped: ECDSA unsupported for curve %s", OBJ_nid2sn(nid));
        return 1;
    }

    TEST_info("testing ECDSA for curve %s", OBJ_nid2sn(nid));

    if (!TEST_ptr(mctx = EVP_MD_CTX_new())
        /* get some random message data */
        || !TEST_true(RAND_bytes(tbs, sizeof(tbs)))
        /* real key */
        || !TEST_ptr(eckey = EC_KEY_new_by_curve_name(nid))
        || !TEST_true(EC_KEY_generate_key(eckey))
        || !TEST_ptr(pkey = EVP_PKEY_new())
        || !TEST_true(EVP_PKEY_assign_EC_KEY(pkey, eckey))
        /* fake key for negative testing */
        || !TEST_ptr(eckey_neg = EC_KEY_new_by_curve_name(nid))
        || !TEST_true(EC_KEY_generate_key(eckey_neg))
        || !TEST_ptr(pkey_neg = EVP_PKEY_new())
        || !TEST_true(EVP_PKEY_assign_EC_KEY(pkey_neg, eckey_neg)))
        goto err;

    sig_len = ECDSA_size(eckey);

    if (!TEST_ptr(sig = OPENSSL_malloc(sig_len))
        /* create a signature */
        || !TEST_true(EVP_DigestSignInit(mctx, NULL, NULL, NULL, pkey))
        || !TEST_true(EVP_DigestSign(mctx, sig, &sig_len, tbs, sizeof(tbs)))
        || !TEST_int_le(sig_len, ECDSA_size(eckey))
        /* negative test, verify with wrong key, 0 return */
        || !TEST_true(EVP_MD_CTX_reset(mctx))
        || !TEST_true(EVP_DigestVerifyInit(mctx, NULL, NULL, NULL, pkey_neg))
        || !TEST_int_eq(EVP_DigestVerify(mctx, sig, sig_len, tbs, sizeof(tbs)), 0)
        /* negative test, verify with wrong signature length, -1 return */
        || !TEST_true(EVP_MD_CTX_reset(mctx))
        || !TEST_true(EVP_DigestVerifyInit(mctx, NULL, NULL, NULL, pkey))
        || !TEST_int_eq(EVP_DigestVerify(mctx, sig, sig_len - 1, tbs, sizeof(tbs)), -1)
        /* positive test, verify with correct key, 1 return */
        || !TEST_true(EVP_MD_CTX_reset(mctx))
        || !TEST_true(EVP_DigestVerifyInit(mctx, NULL, NULL, NULL, pkey))
        || !TEST_int_eq(EVP_DigestVerify(mctx, sig, sig_len, tbs, sizeof(tbs)), 1))
        goto err;

    /* muck with the message, test it fails with 0 return */
    tbs[0] ^= 1;
    if (!TEST_true(EVP_MD_CTX_reset(mctx))
        || !TEST_true(EVP_DigestVerifyInit(mctx, NULL, NULL, NULL, pkey))
        || !TEST_int_eq(EVP_DigestVerify(mctx, sig, sig_len, tbs, sizeof(tbs)), 0))
        goto err;
    /* un-muck and test it verifies */
    tbs[0] ^= 1;
    if (!TEST_true(EVP_MD_CTX_reset(mctx))
        || !TEST_true(EVP_DigestVerifyInit(mctx, NULL, NULL, NULL, pkey))
        || !TEST_int_eq(EVP_DigestVerify(mctx, sig, sig_len, tbs, sizeof(tbs)), 1))
        goto err;

    /*-
     * Muck with the ECDSA signature. The DER encoding is one of:
     * - 30 LL 02 ..
     * - 30 81 LL 02 ..
     *
     * - Sometimes this mucks with the high level DER sequence wrapper:
     *   in that case, DER-parsing of the whole signature should fail.
     *
     * - Sometimes this mucks with the DER-encoding of ECDSA.r:
     *   in that case, DER-parsing of ECDSA.r should fail.
     *
     * - Sometimes this mucks with the DER-encoding of ECDSA.s:
     *   in that case, DER-parsing of ECDSA.s should fail.
     *
     * - Sometimes this mucks with ECDSA.r:
     *   in that case, the signature verification should fail.
     *
     * - Sometimes this mucks with ECDSA.s:
     *   in that case, the signature verification should fail.
     *
     * The usual case is changing the integer value of ECDSA.r or ECDSA.s.
     * Because the ratio of DER overhead to signature bytes is small.
     * So most of the time it will be one of the last two cases.
     *
     * In any case, EVP_PKEY_verify should not return 1 for valid.
     */
    offset = tbs[0] % sig_len;
    dirt = tbs[1] ? tbs[1] : 1;
    sig[offset] ^= dirt;
    if (!TEST_true(EVP_MD_CTX_reset(mctx))
        || !TEST_true(EVP_DigestVerifyInit(mctx, NULL, NULL, NULL, pkey))
        || !TEST_int_ne(EVP_DigestVerify(mctx, sig, sig_len, tbs, sizeof(tbs)), 1))
        goto err;
    /* un-muck and test it verifies */
    sig[offset] ^= dirt;
    if (!TEST_true(EVP_MD_CTX_reset(mctx))
        || !TEST_true(EVP_DigestVerifyInit(mctx, NULL, NULL, NULL, pkey))
        || !TEST_int_eq(EVP_DigestVerify(mctx, sig, sig_len, tbs, sizeof(tbs)), 1))
        goto err;

    ret = 1;
 err:
    EVP_PKEY_free(pkey);
    EVP_PKEY_free(pkey_neg);
    EVP_MD_CTX_free(mctx);
    OPENSSL_free(sig);
    return ret;
}
#endif

int setup_tests(void)
{
#ifdef OPENSSL_NO_EC
    TEST_note("Elliptic curves are disabled.");
#else
    /* get a list of all internal curves */
    crv_len = EC_get_builtin_curves(NULL, 0);
    if (!TEST_ptr(curves = OPENSSL_malloc(sizeof(*curves) * crv_len))
        || !TEST_true(EC_get_builtin_curves(curves, crv_len)))
        return 0;
    ADD_ALL_TESTS(test_builtin, crv_len);
    ADD_ALL_TESTS(x9_62_tests, OSSL_NELEM(ecdsa_cavs_kats));
#endif
    return 1;
}

void cleanup_tests(void)
{
#ifndef OPENSSL_NO_EC
    OPENSSL_free(curves);
#endif
}
