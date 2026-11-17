/*
 * Copyright 2025-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef OSSL_INTERNAL_FIPS_H
#define OSSL_INTERNAL_FIPS_H
#pragma once

#include <openssl/types.h>

#ifdef FIPS_MODULE

/* Return 1 if the FIPS self tests are running and 0 otherwise */
int ossl_fips_self_testing(void);

/*
 * Each enum here corresponds to a test in the st_all_tests array
 * in self_test_data.c, any change done here requires tests to be
 * adjusted accordingly.
 */
typedef enum {
    ST_ID_DRBG_HASH,
    ST_ID_DRBG_CTR,
    ST_ID_DRBG_HMAC,
    ST_ID_CIPHER_AES_256_GCM,
    ST_ID_CIPHER_AES_128_ECB,
#ifndef OPENSSL_NO_DES
    ST_ID_CIPHER_DES_EDE3_ECB,
#endif
#ifndef OPENSSL_NO_ML_KEM
    ST_ID_ASYM_KEYGEN_ML_KEM,
#endif
#ifndef OPENSSL_NO_ML_DSA
    ST_ID_ASYM_KEYGEN_ML_DSA,
#endif
#ifndef OPENSSL_NO_SLH_DSA
    ST_ID_ASYM_KEYGEN_SLH_DSA,
#endif
    ST_ID_SIG_RSA_SHA256,
#ifndef OPENSSL_NO_EC
    ST_ID_SIG_ECDSA_SHA256,
#ifndef OPENSSL_NO_HMAC_DRBG_KDF
    ST_ID_SIG_DET_ECDSA_SHA256,
#endif
#ifndef OPENSSL_NO_EC2M
    ST_ID_SIG_E2CM_ECDSA_SHA256,
#endif
#ifndef OPENSSL_NO_ECX
    ST_ID_SIG_ED448,
    ST_ID_SIG_ED25519,
#endif
#endif
#ifndef OPENSSL_NO_DSA
    ST_ID_SIG_DSA_SHA256,
#endif
#ifndef OPENSSL_NO_ML_DSA
    ST_ID_SIG_ML_DSA_65,
#endif
#ifndef OPENSSL_NO_SLH_DSA
    ST_ID_SIG_SLH_DSA_SHA2_128F,
    ST_ID_SIG_SLH_DSA_SHAKE_128F,
#endif /* OPENSSL_NO_SLH_DSA */
#ifndef OPENSSL_NO_LMS
    ST_ID_SIG_LMS,
#endif
#ifndef OPENSSL_NO_ML_KEM
    ST_ID_KEM_ML_KEM,
#endif
    ST_ID_ASYM_CIPHER_RSA_ENC,
    ST_ID_ASYM_CIPHER_RSA_DEC,
    ST_ID_ASYM_CIPHER_RSA_DEC_CRT,
#ifndef OPENSSL_NO_DH
    ST_ID_KA_DH,
#endif
#ifndef OPENSSL_NO_EC
    ST_ID_KA_ECDH,
#endif
    ST_ID_KDF_TLS13_EXTRACT,
    ST_ID_KDF_TLS13_EXPAND,
    ST_ID_KDF_TLS12_PRF,
    ST_ID_KDF_PBKDF2,
#ifndef OPENSSL_NO_KBKDF
    ST_ID_KDF_KBKDF,
    ST_ID_KDF_KBKDF_KMAC,
#endif
    ST_ID_KDF_HKDF,
#ifndef OPENSSL_NO_IKEV2KDF
    ST_ID_KDF_IKEV2KDF_GEN,
    ST_ID_KDF_IKEV2KDF_DKM1,
    ST_ID_KDF_IKEV2KDF_DKM2,
    ST_ID_KDF_IKEV2KDF_DKM3,
    ST_ID_KDF_IKEV2KDF_REKEY,
#endif
#ifndef OPENSSL_NO_SNMPKDF
    ST_ID_KDF_SNMPKDF,
#endif
#ifndef OPENSSL_NO_SRTPKDF
    ST_ID_KDF_SRTPKDF,
#endif
#ifndef OPENSSL_NO_SSKDF
    ST_ID_KDF_SSKDF,
#endif
#ifndef OPENSSL_NO_X963KDF
    ST_ID_KDF_X963KDF,
#endif
#ifndef OPENSSL_NO_X942KDF
    ST_ID_KDF_X942KDF,
#endif
    ST_ID_MAC_HMAC,
    ST_ID_DIGEST_SHA1,
    ST_ID_DIGEST_SHA256,
    ST_ID_DIGEST_SHA512,
    ST_ID_DIGEST_SHA3_256,
    ST_ID_MAX
} self_test_id_t;

int ossl_deferred_self_test(OSSL_LIB_CTX *libctx, self_test_id_t id);
int ossl_self_test_in_progress(self_test_id_t id);

/* Helper definitions to keep some of the ciphercommon.h macros simple */
#define ST_ID_CIPHER_aes ST_ID_CIPHER_AES_128_ECB
#define ST_ID_CIPHER_AES_128_CCM ST_ID_CIPHER_AES_128_ECB
#define ST_ID_CIPHER_AES_128_OCB ST_ID_CIPHER_AES_128_ECB
#define ST_ID_CIPHER_AES_128_WRP ST_ID_CIPHER_AES_128_ECB
#define ST_ID_CIPHER_AES_128_XTS ST_ID_CIPHER_AES_128_ECB
/* Helper definitions to keep some of the digestcommon.h macros simple */
#define ST_ID_DIGEST_sha1 ST_ID_DIGEST_SHA1
#define ST_ID_DIGEST_sha224 ST_ID_DIGEST_SHA256
#define ST_ID_DIGEST_sha256 ST_ID_DIGEST_SHA256
#define ST_ID_DIGEST_sha256_192_internal ST_ID_DIGEST_SHA256
#define ST_ID_DIGEST_sha384 ST_ID_DIGEST_SHA512
#define ST_ID_DIGEST_sha512 ST_ID_DIGEST_SHA512
#define ST_ID_DIGEST_sha512_224 ST_ID_DIGEST_SHA512
#define ST_ID_DIGEST_sha512_256 ST_ID_DIGEST_SHA512

#endif /* FIPS_MODULE */

#endif
