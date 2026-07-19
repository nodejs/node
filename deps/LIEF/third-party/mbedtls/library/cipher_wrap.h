/**
 * \file cipher_wrap.h
 *
 * \brief Cipher wrappers.
 *
 * \author Adriaan de Jong <dejong@fox-it.com>
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#ifndef MBEDTLS_CIPHER_WRAP_H
#define MBEDTLS_CIPHER_WRAP_H

#include "mbedtls/build_info.h"

#include "mbedtls/cipher.h"

#if defined(MBEDTLS_USE_PSA_CRYPTO)
#include "psa/crypto.h"
#endif /* MBEDTLS_USE_PSA_CRYPTO */

#ifdef __cplusplus
extern "C" {
#endif

/* Support for GCM either through Mbed TLS SW implementation or PSA */
#if defined(MBEDTLS_GCM_C) || \
    (defined(MBEDTLS_USE_PSA_CRYPTO) && defined(PSA_WANT_ALG_GCM))
#define MBEDTLS_CIPHER_HAVE_GCM_VIA_LEGACY_OR_USE_PSA
#endif

#if (defined(MBEDTLS_GCM_C) && defined(MBEDTLS_AES_C)) || \
    (defined(MBEDTLS_USE_PSA_CRYPTO) && defined(PSA_WANT_ALG_GCM) && defined(PSA_WANT_KEY_TYPE_AES))
#define MBEDTLS_CIPHER_HAVE_GCM_AES_VIA_LEGACY_OR_USE_PSA
#endif

#if defined(MBEDTLS_CCM_C) || \
    (defined(MBEDTLS_USE_PSA_CRYPTO) && defined(PSA_WANT_ALG_CCM))
#define MBEDTLS_CIPHER_HAVE_CCM_VIA_LEGACY_OR_USE_PSA
#endif

#if (defined(MBEDTLS_CCM_C) && defined(MBEDTLS_AES_C)) || \
    (defined(MBEDTLS_USE_PSA_CRYPTO) && defined(PSA_WANT_ALG_CCM) && defined(PSA_WANT_KEY_TYPE_AES))
#define MBEDTLS_CIPHER_HAVE_CCM_AES_VIA_LEGACY_OR_USE_PSA
#endif

#if defined(MBEDTLS_CCM_C) || \
    (defined(MBEDTLS_USE_PSA_CRYPTO) && defined(PSA_WANT_ALG_CCM_STAR_NO_TAG))
#define MBEDTLS_CIPHER_HAVE_CCM_STAR_NO_TAG_VIA_LEGACY_OR_USE_PSA
#endif

#if (defined(MBEDTLS_CCM_C) && defined(MBEDTLS_AES_C)) || \
    (defined(MBEDTLS_USE_PSA_CRYPTO) && defined(PSA_WANT_ALG_CCM_STAR_NO_TAG) && \
    defined(PSA_WANT_KEY_TYPE_AES))
#define MBEDTLS_CIPHER_HAVE_CCM_STAR_NO_TAG_AES_VIA_LEGACY_OR_USE_PSA
#endif

#if defined(MBEDTLS_CHACHAPOLY_C) || \
    (defined(MBEDTLS_USE_PSA_CRYPTO) && defined(PSA_WANT_ALG_CHACHA20_POLY1305))
#define MBEDTLS_CIPHER_HAVE_CHACHAPOLY_VIA_LEGACY_OR_USE_PSA
#endif

#if defined(MBEDTLS_CIPHER_HAVE_GCM_VIA_LEGACY_OR_USE_PSA) || \
    defined(MBEDTLS_CIPHER_HAVE_CCM_VIA_LEGACY_OR_USE_PSA) || \
    defined(MBEDTLS_CIPHER_HAVE_CCM_STAR_NO_TAG_VIA_LEGACY_OR_USE_PSA) || \
    defined(MBEDTLS_CIPHER_HAVE_CHACHAPOLY_VIA_LEGACY_OR_USE_PSA)
#define MBEDTLS_CIPHER_HAVE_SOME_AEAD_VIA_LEGACY_OR_USE_PSA
#endif

/**
 * Base cipher information. The non-mode specific functions and values.
 */
struct mbedtls_cipher_base_t {
    /** Base Cipher type (e.g. MBEDTLS_CIPHER_ID_AES) */
    mbedtls_cipher_id_t cipher;

    /** Encrypt using ECB */
    int (*ecb_func)(void *ctx, mbedtls_operation_t mode,
                    const unsigned char *input, unsigned char *output);

#if defined(MBEDTLS_CIPHER_MODE_CBC)
    /** Encrypt using CBC */
    int (*cbc_func)(void *ctx, mbedtls_operation_t mode, size_t length,
                    unsigned char *iv, const unsigned char *input,
                    unsigned char *output);
#endif

#if defined(MBEDTLS_CIPHER_MODE_CFB)
    /** Encrypt using CFB (Full length) */
    int (*cfb_func)(void *ctx, mbedtls_operation_t mode, size_t length, size_t *iv_off,
                    unsigned char *iv, const unsigned char *input,
                    unsigned char *output);
#endif

#if defined(MBEDTLS_CIPHER_MODE_OFB)
    /** Encrypt using OFB (Full length) */
    int (*ofb_func)(void *ctx, size_t length, size_t *iv_off,
                    unsigned char *iv,
                    const unsigned char *input,
                    unsigned char *output);
#endif

#if defined(MBEDTLS_CIPHER_MODE_CTR)
    /** Encrypt using CTR */
    int (*ctr_func)(void *ctx, size_t length, size_t *nc_off,
                    unsigned char *nonce_counter, unsigned char *stream_block,
                    const unsigned char *input, unsigned char *output);
#endif

#if defined(MBEDTLS_CIPHER_MODE_XTS)
    /** Encrypt or decrypt using XTS. */
    int (*xts_func)(void *ctx, mbedtls_operation_t mode, size_t length,
                    const unsigned char data_unit[16],
                    const unsigned char *input, unsigned char *output);
#endif

#if defined(MBEDTLS_CIPHER_MODE_STREAM)
    /** Encrypt using STREAM */
    int (*stream_func)(void *ctx, size_t length,
                       const unsigned char *input, unsigned char *output);
#endif

    /** Set key for encryption purposes */
    int (*setkey_enc_func)(void *ctx, const unsigned char *key,
                           unsigned int key_bitlen);

#if !defined(MBEDTLS_BLOCK_CIPHER_NO_DECRYPT)
    /** Set key for decryption purposes */
    int (*setkey_dec_func)(void *ctx, const unsigned char *key,
                           unsigned int key_bitlen);
#endif

    /** Allocate a new context */
    void * (*ctx_alloc_func)(void);

    /** Free the given context */
    void (*ctx_free_func)(void *ctx);

};

typedef struct {
    mbedtls_cipher_type_t type;
    const mbedtls_cipher_info_t *info;
} mbedtls_cipher_definition_t;

#if defined(MBEDTLS_USE_PSA_CRYPTO)
typedef enum {
    MBEDTLS_CIPHER_PSA_KEY_UNSET = 0,
    MBEDTLS_CIPHER_PSA_KEY_OWNED, /* Used for PSA-based cipher contexts which */
                                  /* use raw key material internally imported */
                                  /* as a volatile key, and which hence need  */
                                  /* to destroy that key when the context is  */
                                  /* freed.                                   */
    MBEDTLS_CIPHER_PSA_KEY_NOT_OWNED, /* Used for PSA-based cipher contexts   */
                                      /* which use a key provided by the      */
                                      /* user, and which hence will not be    */
                                      /* destroyed when the context is freed. */
} mbedtls_cipher_psa_key_ownership;

typedef struct {
    psa_algorithm_t alg;
    mbedtls_svc_key_id_t slot;
    mbedtls_cipher_psa_key_ownership slot_state;
} mbedtls_cipher_context_psa;
#endif /* MBEDTLS_USE_PSA_CRYPTO */

extern const mbedtls_cipher_definition_t mbedtls_cipher_definitions[];

extern int mbedtls_cipher_supported[];

extern const mbedtls_cipher_base_t * const mbedtls_cipher_base_lookup_table[];

#ifdef __cplusplus
}
#endif

#endif /* MBEDTLS_CIPHER_WRAP_H */
