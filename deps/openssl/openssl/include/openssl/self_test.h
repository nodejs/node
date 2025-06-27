/*
 * Copyright 2019-2022 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef OPENSSL_SELF_TEST_H
# define OPENSSL_SELF_TEST_H
# pragma once

# include <openssl/core.h> /* OSSL_CALLBACK */

# ifdef __cplusplus
extern "C" {
# endif

/* The test event phases */
# define OSSL_SELF_TEST_PHASE_NONE     "None"
# define OSSL_SELF_TEST_PHASE_START    "Start"
# define OSSL_SELF_TEST_PHASE_CORRUPT  "Corrupt"
# define OSSL_SELF_TEST_PHASE_PASS     "Pass"
# define OSSL_SELF_TEST_PHASE_FAIL     "Fail"

/* Test event categories */
# define OSSL_SELF_TEST_TYPE_NONE               "None"
# define OSSL_SELF_TEST_TYPE_MODULE_INTEGRITY   "Module_Integrity"
# define OSSL_SELF_TEST_TYPE_INSTALL_INTEGRITY  "Install_Integrity"
# define OSSL_SELF_TEST_TYPE_CRNG               "Continuous_RNG_Test"
# define OSSL_SELF_TEST_TYPE_PCT                "Conditional_PCT"
# define OSSL_SELF_TEST_TYPE_KAT_CIPHER         "KAT_Cipher"
# define OSSL_SELF_TEST_TYPE_KAT_ASYM_CIPHER    "KAT_AsymmetricCipher"
# define OSSL_SELF_TEST_TYPE_KAT_DIGEST         "KAT_Digest"
# define OSSL_SELF_TEST_TYPE_KAT_SIGNATURE      "KAT_Signature"
# define OSSL_SELF_TEST_TYPE_PCT_SIGNATURE      "PCT_Signature"
# define OSSL_SELF_TEST_TYPE_KAT_KDF            "KAT_KDF"
# define OSSL_SELF_TEST_TYPE_KAT_KA             "KAT_KA"
# define OSSL_SELF_TEST_TYPE_DRBG               "DRBG"

/* Test event sub categories */
# define OSSL_SELF_TEST_DESC_NONE           "None"
# define OSSL_SELF_TEST_DESC_INTEGRITY_HMAC "HMAC"
# define OSSL_SELF_TEST_DESC_PCT_RSA_PKCS1  "RSA"
# define OSSL_SELF_TEST_DESC_PCT_ECDSA      "ECDSA"
# define OSSL_SELF_TEST_DESC_PCT_DSA        "DSA"
# define OSSL_SELF_TEST_DESC_CIPHER_AES_GCM "AES_GCM"
# define OSSL_SELF_TEST_DESC_CIPHER_AES_ECB "AES_ECB_Decrypt"
# define OSSL_SELF_TEST_DESC_CIPHER_TDES    "TDES"
# define OSSL_SELF_TEST_DESC_ASYM_RSA_ENC   "RSA_Encrypt"
# define OSSL_SELF_TEST_DESC_ASYM_RSA_DEC   "RSA_Decrypt"
# define OSSL_SELF_TEST_DESC_MD_SHA1        "SHA1"
# define OSSL_SELF_TEST_DESC_MD_SHA2        "SHA2"
# define OSSL_SELF_TEST_DESC_MD_SHA3        "SHA3"
# define OSSL_SELF_TEST_DESC_SIGN_DSA       "DSA"
# define OSSL_SELF_TEST_DESC_SIGN_RSA       "RSA"
# define OSSL_SELF_TEST_DESC_SIGN_ECDSA     "ECDSA"
# define OSSL_SELF_TEST_DESC_DRBG_CTR       "CTR"
# define OSSL_SELF_TEST_DESC_DRBG_HASH      "HASH"
# define OSSL_SELF_TEST_DESC_DRBG_HMAC      "HMAC"
# define OSSL_SELF_TEST_DESC_KA_DH          "DH"
# define OSSL_SELF_TEST_DESC_KA_ECDH        "ECDH"
# define OSSL_SELF_TEST_DESC_KDF_HKDF       "HKDF"
# define OSSL_SELF_TEST_DESC_KDF_SSKDF      "SSKDF"
# define OSSL_SELF_TEST_DESC_KDF_X963KDF    "X963KDF"
# define OSSL_SELF_TEST_DESC_KDF_X942KDF    "X942KDF"
# define OSSL_SELF_TEST_DESC_KDF_PBKDF2     "PBKDF2"
# define OSSL_SELF_TEST_DESC_KDF_SSHKDF     "SSHKDF"
# define OSSL_SELF_TEST_DESC_KDF_TLS12_PRF  "TLS12_PRF"
# define OSSL_SELF_TEST_DESC_KDF_KBKDF      "KBKDF"
# define OSSL_SELF_TEST_DESC_KDF_TLS13_EXTRACT  "TLS13_KDF_EXTRACT"
# define OSSL_SELF_TEST_DESC_KDF_TLS13_EXPAND   "TLS13_KDF_EXPAND"
# define OSSL_SELF_TEST_DESC_RNG            "RNG"

void OSSL_SELF_TEST_set_callback(OSSL_LIB_CTX *libctx, OSSL_CALLBACK *cb,
                                 void *cbarg);
void OSSL_SELF_TEST_get_callback(OSSL_LIB_CTX *libctx, OSSL_CALLBACK **cb,
                                 void **cbarg);

OSSL_SELF_TEST *OSSL_SELF_TEST_new(OSSL_CALLBACK *cb, void *cbarg);
void OSSL_SELF_TEST_free(OSSL_SELF_TEST *st);

void OSSL_SELF_TEST_onbegin(OSSL_SELF_TEST *st, const char *type,
                            const char *desc);
int OSSL_SELF_TEST_oncorrupt_byte(OSSL_SELF_TEST *st, unsigned char *bytes);
void OSSL_SELF_TEST_onend(OSSL_SELF_TEST *st, int ret);

# ifdef __cplusplus
}
# endif
#endif /* OPENSSL_SELF_TEST_H */
