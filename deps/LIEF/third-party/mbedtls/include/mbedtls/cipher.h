/**
 * \file cipher.h
 *
 * \brief This file contains an abstraction interface for use with the cipher
 * primitives provided by the library. It provides a common interface to all of
 * the available cipher operations.
 *
 * \author Adriaan de Jong <dejong@fox-it.com>
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_CIPHER_H
#define MBEDTLS_CIPHER_H
#include "mbedtls/private_access.h"

#include "mbedtls/build_info.h"

#include <stddef.h>
#include "mbedtls/platform_util.h"

#if defined(MBEDTLS_GCM_C) || defined(MBEDTLS_CCM_C) || defined(MBEDTLS_CHACHAPOLY_C)
#define MBEDTLS_CIPHER_MODE_AEAD
#endif

#if defined(MBEDTLS_CIPHER_MODE_CBC)
#define MBEDTLS_CIPHER_MODE_WITH_PADDING
#endif

#if defined(MBEDTLS_CIPHER_NULL_CIPHER) || \
    defined(MBEDTLS_CHACHA20_C)
#define MBEDTLS_CIPHER_MODE_STREAM
#endif

/** The selected feature is not available. */
#define MBEDTLS_ERR_CIPHER_FEATURE_UNAVAILABLE  -0x6080
/** Bad input parameters. */
#define MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA       -0x6100
/** Failed to allocate memory. */
#define MBEDTLS_ERR_CIPHER_ALLOC_FAILED         -0x6180
/** Input data contains invalid padding and is rejected. */
#define MBEDTLS_ERR_CIPHER_INVALID_PADDING      -0x6200
/** Decryption of block requires a full block. */
#define MBEDTLS_ERR_CIPHER_FULL_BLOCK_EXPECTED  -0x6280
/** Authentication failed (for AEAD modes). */
#define MBEDTLS_ERR_CIPHER_AUTH_FAILED          -0x6300
/** The context is invalid. For example, because it was freed. */
#define MBEDTLS_ERR_CIPHER_INVALID_CONTEXT      -0x6380

#define MBEDTLS_CIPHER_VARIABLE_IV_LEN     0x01    /**< Cipher accepts IVs of variable length. */
#define MBEDTLS_CIPHER_VARIABLE_KEY_LEN    0x02    /**< Cipher accepts keys of variable length. */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief     Supported cipher types.
 *
 * \warning   DES/3DES are considered weak ciphers and their use
 *            constitutes a security risk. We recommend considering stronger
 *            ciphers instead.
 */
typedef enum {
    MBEDTLS_CIPHER_ID_NONE = 0,  /**< Placeholder to mark the end of cipher ID lists. */
    MBEDTLS_CIPHER_ID_NULL,      /**< The identity cipher, treated as a stream cipher. */
    MBEDTLS_CIPHER_ID_AES,       /**< The AES cipher. */
    MBEDTLS_CIPHER_ID_DES,       /**< The DES cipher. \warning DES is considered weak. */
    MBEDTLS_CIPHER_ID_3DES,      /**< The Triple DES cipher. \warning 3DES is considered weak. */
    MBEDTLS_CIPHER_ID_CAMELLIA,  /**< The Camellia cipher. */
    MBEDTLS_CIPHER_ID_ARIA,      /**< The Aria cipher. */
    MBEDTLS_CIPHER_ID_CHACHA20,  /**< The ChaCha20 cipher. */
} mbedtls_cipher_id_t;

/**
 * \brief     Supported {cipher type, cipher mode} pairs.
 *
 * \warning   DES/3DES are considered weak ciphers and their use
 *            constitutes a security risk. We recommend considering stronger
 *            ciphers instead.
 */
typedef enum {
    MBEDTLS_CIPHER_NONE = 0,             /**< Placeholder to mark the end of cipher-pair lists. */
    MBEDTLS_CIPHER_NULL,                 /**< The identity stream cipher. */
    MBEDTLS_CIPHER_AES_128_ECB,          /**< AES cipher with 128-bit ECB mode. */
    MBEDTLS_CIPHER_AES_192_ECB,          /**< AES cipher with 192-bit ECB mode. */
    MBEDTLS_CIPHER_AES_256_ECB,          /**< AES cipher with 256-bit ECB mode. */
    MBEDTLS_CIPHER_AES_128_CBC,          /**< AES cipher with 128-bit CBC mode. */
    MBEDTLS_CIPHER_AES_192_CBC,          /**< AES cipher with 192-bit CBC mode. */
    MBEDTLS_CIPHER_AES_256_CBC,          /**< AES cipher with 256-bit CBC mode. */
    MBEDTLS_CIPHER_AES_128_CFB128,       /**< AES cipher with 128-bit CFB128 mode. */
    MBEDTLS_CIPHER_AES_192_CFB128,       /**< AES cipher with 192-bit CFB128 mode. */
    MBEDTLS_CIPHER_AES_256_CFB128,       /**< AES cipher with 256-bit CFB128 mode. */
    MBEDTLS_CIPHER_AES_128_CTR,          /**< AES cipher with 128-bit CTR mode. */
    MBEDTLS_CIPHER_AES_192_CTR,          /**< AES cipher with 192-bit CTR mode. */
    MBEDTLS_CIPHER_AES_256_CTR,          /**< AES cipher with 256-bit CTR mode. */
    MBEDTLS_CIPHER_AES_128_GCM,          /**< AES cipher with 128-bit GCM mode. */
    MBEDTLS_CIPHER_AES_192_GCM,          /**< AES cipher with 192-bit GCM mode. */
    MBEDTLS_CIPHER_AES_256_GCM,          /**< AES cipher with 256-bit GCM mode. */
    MBEDTLS_CIPHER_CAMELLIA_128_ECB,     /**< Camellia cipher with 128-bit ECB mode. */
    MBEDTLS_CIPHER_CAMELLIA_192_ECB,     /**< Camellia cipher with 192-bit ECB mode. */
    MBEDTLS_CIPHER_CAMELLIA_256_ECB,     /**< Camellia cipher with 256-bit ECB mode. */
    MBEDTLS_CIPHER_CAMELLIA_128_CBC,     /**< Camellia cipher with 128-bit CBC mode. */
    MBEDTLS_CIPHER_CAMELLIA_192_CBC,     /**< Camellia cipher with 192-bit CBC mode. */
    MBEDTLS_CIPHER_CAMELLIA_256_CBC,     /**< Camellia cipher with 256-bit CBC mode. */
    MBEDTLS_CIPHER_CAMELLIA_128_CFB128,  /**< Camellia cipher with 128-bit CFB128 mode. */
    MBEDTLS_CIPHER_CAMELLIA_192_CFB128,  /**< Camellia cipher with 192-bit CFB128 mode. */
    MBEDTLS_CIPHER_CAMELLIA_256_CFB128,  /**< Camellia cipher with 256-bit CFB128 mode. */
    MBEDTLS_CIPHER_CAMELLIA_128_CTR,     /**< Camellia cipher with 128-bit CTR mode. */
    MBEDTLS_CIPHER_CAMELLIA_192_CTR,     /**< Camellia cipher with 192-bit CTR mode. */
    MBEDTLS_CIPHER_CAMELLIA_256_CTR,     /**< Camellia cipher with 256-bit CTR mode. */
    MBEDTLS_CIPHER_CAMELLIA_128_GCM,     /**< Camellia cipher with 128-bit GCM mode. */
    MBEDTLS_CIPHER_CAMELLIA_192_GCM,     /**< Camellia cipher with 192-bit GCM mode. */
    MBEDTLS_CIPHER_CAMELLIA_256_GCM,     /**< Camellia cipher with 256-bit GCM mode. */
    MBEDTLS_CIPHER_DES_ECB,              /**< DES cipher with ECB mode. \warning DES is considered weak. */
    MBEDTLS_CIPHER_DES_CBC,              /**< DES cipher with CBC mode. \warning DES is considered weak. */
    MBEDTLS_CIPHER_DES_EDE_ECB,          /**< DES cipher with EDE ECB mode. \warning 3DES is considered weak. */
    MBEDTLS_CIPHER_DES_EDE_CBC,          /**< DES cipher with EDE CBC mode. \warning 3DES is considered weak. */
    MBEDTLS_CIPHER_DES_EDE3_ECB,         /**< DES cipher with EDE3 ECB mode. \warning 3DES is considered weak. */
    MBEDTLS_CIPHER_DES_EDE3_CBC,         /**< DES cipher with EDE3 CBC mode. \warning 3DES is considered weak. */
    MBEDTLS_CIPHER_AES_128_CCM,          /**< AES cipher with 128-bit CCM mode. */
    MBEDTLS_CIPHER_AES_192_CCM,          /**< AES cipher with 192-bit CCM mode. */
    MBEDTLS_CIPHER_AES_256_CCM,          /**< AES cipher with 256-bit CCM mode. */
    MBEDTLS_CIPHER_AES_128_CCM_STAR_NO_TAG, /**< AES cipher with 128-bit CCM_STAR_NO_TAG mode. */
    MBEDTLS_CIPHER_AES_192_CCM_STAR_NO_TAG, /**< AES cipher with 192-bit CCM_STAR_NO_TAG mode. */
    MBEDTLS_CIPHER_AES_256_CCM_STAR_NO_TAG, /**< AES cipher with 256-bit CCM_STAR_NO_TAG mode. */
    MBEDTLS_CIPHER_CAMELLIA_128_CCM,     /**< Camellia cipher with 128-bit CCM mode. */
    MBEDTLS_CIPHER_CAMELLIA_192_CCM,     /**< Camellia cipher with 192-bit CCM mode. */
    MBEDTLS_CIPHER_CAMELLIA_256_CCM,     /**< Camellia cipher with 256-bit CCM mode. */
    MBEDTLS_CIPHER_CAMELLIA_128_CCM_STAR_NO_TAG, /**< Camellia cipher with 128-bit CCM_STAR_NO_TAG mode. */
    MBEDTLS_CIPHER_CAMELLIA_192_CCM_STAR_NO_TAG, /**< Camellia cipher with 192-bit CCM_STAR_NO_TAG mode. */
    MBEDTLS_CIPHER_CAMELLIA_256_CCM_STAR_NO_TAG, /**< Camellia cipher with 256-bit CCM_STAR_NO_TAG mode. */
    MBEDTLS_CIPHER_ARIA_128_ECB,         /**< Aria cipher with 128-bit key and ECB mode. */
    MBEDTLS_CIPHER_ARIA_192_ECB,         /**< Aria cipher with 192-bit key and ECB mode. */
    MBEDTLS_CIPHER_ARIA_256_ECB,         /**< Aria cipher with 256-bit key and ECB mode. */
    MBEDTLS_CIPHER_ARIA_128_CBC,         /**< Aria cipher with 128-bit key and CBC mode. */
    MBEDTLS_CIPHER_ARIA_192_CBC,         /**< Aria cipher with 192-bit key and CBC mode. */
    MBEDTLS_CIPHER_ARIA_256_CBC,         /**< Aria cipher with 256-bit key and CBC mode. */
    MBEDTLS_CIPHER_ARIA_128_CFB128,      /**< Aria cipher with 128-bit key and CFB-128 mode. */
    MBEDTLS_CIPHER_ARIA_192_CFB128,      /**< Aria cipher with 192-bit key and CFB-128 mode. */
    MBEDTLS_CIPHER_ARIA_256_CFB128,      /**< Aria cipher with 256-bit key and CFB-128 mode. */
    MBEDTLS_CIPHER_ARIA_128_CTR,         /**< Aria cipher with 128-bit key and CTR mode. */
    MBEDTLS_CIPHER_ARIA_192_CTR,         /**< Aria cipher with 192-bit key and CTR mode. */
    MBEDTLS_CIPHER_ARIA_256_CTR,         /**< Aria cipher with 256-bit key and CTR mode. */
    MBEDTLS_CIPHER_ARIA_128_GCM,         /**< Aria cipher with 128-bit key and GCM mode. */
    MBEDTLS_CIPHER_ARIA_192_GCM,         /**< Aria cipher with 192-bit key and GCM mode. */
    MBEDTLS_CIPHER_ARIA_256_GCM,         /**< Aria cipher with 256-bit key and GCM mode. */
    MBEDTLS_CIPHER_ARIA_128_CCM,         /**< Aria cipher with 128-bit key and CCM mode. */
    MBEDTLS_CIPHER_ARIA_192_CCM,         /**< Aria cipher with 192-bit key and CCM mode. */
    MBEDTLS_CIPHER_ARIA_256_CCM,         /**< Aria cipher with 256-bit key and CCM mode. */
    MBEDTLS_CIPHER_ARIA_128_CCM_STAR_NO_TAG, /**< Aria cipher with 128-bit key and CCM_STAR_NO_TAG mode. */
    MBEDTLS_CIPHER_ARIA_192_CCM_STAR_NO_TAG, /**< Aria cipher with 192-bit key and CCM_STAR_NO_TAG mode. */
    MBEDTLS_CIPHER_ARIA_256_CCM_STAR_NO_TAG, /**< Aria cipher with 256-bit key and CCM_STAR_NO_TAG mode. */
    MBEDTLS_CIPHER_AES_128_OFB,          /**< AES 128-bit cipher in OFB mode. */
    MBEDTLS_CIPHER_AES_192_OFB,          /**< AES 192-bit cipher in OFB mode. */
    MBEDTLS_CIPHER_AES_256_OFB,          /**< AES 256-bit cipher in OFB mode. */
    MBEDTLS_CIPHER_AES_128_XTS,          /**< AES 128-bit cipher in XTS block mode. */
    MBEDTLS_CIPHER_AES_256_XTS,          /**< AES 256-bit cipher in XTS block mode. */
    MBEDTLS_CIPHER_CHACHA20,             /**< ChaCha20 stream cipher. */
    MBEDTLS_CIPHER_CHACHA20_POLY1305,    /**< ChaCha20-Poly1305 AEAD cipher. */
    MBEDTLS_CIPHER_AES_128_KW,           /**< AES cipher with 128-bit NIST KW mode. */
    MBEDTLS_CIPHER_AES_192_KW,           /**< AES cipher with 192-bit NIST KW mode. */
    MBEDTLS_CIPHER_AES_256_KW,           /**< AES cipher with 256-bit NIST KW mode. */
    MBEDTLS_CIPHER_AES_128_KWP,          /**< AES cipher with 128-bit NIST KWP mode. */
    MBEDTLS_CIPHER_AES_192_KWP,          /**< AES cipher with 192-bit NIST KWP mode. */
    MBEDTLS_CIPHER_AES_256_KWP,          /**< AES cipher with 256-bit NIST KWP mode. */
} mbedtls_cipher_type_t;

/** Supported cipher modes. */
typedef enum {
    MBEDTLS_MODE_NONE = 0,               /**< None.                        */
    MBEDTLS_MODE_ECB,                    /**< The ECB cipher mode.         */
    MBEDTLS_MODE_CBC,                    /**< The CBC cipher mode.         */
    MBEDTLS_MODE_CFB,                    /**< The CFB cipher mode.         */
    MBEDTLS_MODE_OFB,                    /**< The OFB cipher mode.         */
    MBEDTLS_MODE_CTR,                    /**< The CTR cipher mode.         */
    MBEDTLS_MODE_GCM,                    /**< The GCM cipher mode.         */
    MBEDTLS_MODE_STREAM,                 /**< The stream cipher mode.      */
    MBEDTLS_MODE_CCM,                    /**< The CCM cipher mode.         */
    MBEDTLS_MODE_CCM_STAR_NO_TAG,        /**< The CCM*-no-tag cipher mode. */
    MBEDTLS_MODE_XTS,                    /**< The XTS cipher mode.         */
    MBEDTLS_MODE_CHACHAPOLY,             /**< The ChaCha-Poly cipher mode. */
    MBEDTLS_MODE_KW,                     /**< The SP800-38F KW mode */
    MBEDTLS_MODE_KWP,                    /**< The SP800-38F KWP mode */
} mbedtls_cipher_mode_t;

/** Supported cipher padding types. */
typedef enum {
    MBEDTLS_PADDING_PKCS7 = 0,     /**< PKCS7 padding (default).        */
    MBEDTLS_PADDING_ONE_AND_ZEROS, /**< ISO/IEC 7816-4 padding.         */
    MBEDTLS_PADDING_ZEROS_AND_LEN, /**< ANSI X.923 padding.             */
    MBEDTLS_PADDING_ZEROS,         /**< Zero padding (not reversible). */
    MBEDTLS_PADDING_NONE,          /**< Never pad (full blocks only).   */
} mbedtls_cipher_padding_t;

/** Type of operation. */
typedef enum {
    MBEDTLS_OPERATION_NONE = -1,
    MBEDTLS_DECRYPT = 0,
    MBEDTLS_ENCRYPT,
} mbedtls_operation_t;

enum {
    /** Undefined key length. */
    MBEDTLS_KEY_LENGTH_NONE = 0,
    /** Key length, in bits (including parity), for DES keys. \warning DES is considered weak. */
    MBEDTLS_KEY_LENGTH_DES  = 64,
    /** Key length in bits, including parity, for DES in two-key EDE. \warning 3DES is considered weak. */
    MBEDTLS_KEY_LENGTH_DES_EDE = 128,
    /** Key length in bits, including parity, for DES in three-key EDE. \warning 3DES is considered weak. */
    MBEDTLS_KEY_LENGTH_DES_EDE3 = 192,
};

/** Maximum length of any IV, in Bytes. */
/* This should ideally be derived automatically from list of ciphers.
 * This should be kept in sync with MBEDTLS_SSL_MAX_IV_LENGTH defined
 * in library/ssl_misc.h. */
#define MBEDTLS_MAX_IV_LENGTH      16

/** Maximum block size of any cipher, in Bytes. */
/* This should ideally be derived automatically from list of ciphers.
 * This should be kept in sync with MBEDTLS_SSL_MAX_BLOCK_LENGTH defined
 * in library/ssl_misc.h. */
#define MBEDTLS_MAX_BLOCK_LENGTH   16

/** Maximum key length, in Bytes. */
/* This should ideally be derived automatically from list of ciphers.
 * For now, only check whether XTS is enabled which uses 64 Byte keys,
 * and use 32 Bytes as an upper bound for the maximum key length otherwise.
 * This should be kept in sync with MBEDTLS_SSL_MAX_BLOCK_LENGTH defined
 * in library/ssl_misc.h, which however deliberately ignores the case of XTS
 * since the latter isn't used in SSL/TLS. */
#if defined(MBEDTLS_CIPHER_MODE_XTS)
#define MBEDTLS_MAX_KEY_LENGTH     64
#else
#define MBEDTLS_MAX_KEY_LENGTH     32
#endif /* MBEDTLS_CIPHER_MODE_XTS */

/**
 * Base cipher information (opaque struct).
 */
typedef struct mbedtls_cipher_base_t mbedtls_cipher_base_t;

/**
 * CMAC context (opaque struct).
 */
typedef struct mbedtls_cmac_context_t mbedtls_cmac_context_t;

/**
 * Cipher information. Allows calling cipher functions
 * in a generic way.
 *
 * \note        The library does not support custom cipher info structures,
 *              only built-in structures returned by the functions
 *              mbedtls_cipher_info_from_string(),
 *              mbedtls_cipher_info_from_type(),
 *              mbedtls_cipher_info_from_values(),
 *              mbedtls_cipher_info_from_psa().
 *
 * \note        Some fields store a value that has been right-shifted to save
 *              code-size, so should not be used directly. The accessor
 *              functions adjust for this and return the "natural" value.
 */
typedef struct mbedtls_cipher_info_t {
    /** Name of the cipher. */
    const char *MBEDTLS_PRIVATE(name);

    /** The block size, in bytes. */
    unsigned int MBEDTLS_PRIVATE(block_size) : 5;

    /** IV or nonce size, in bytes (right shifted by #MBEDTLS_IV_SIZE_SHIFT).
     * For ciphers that accept variable IV sizes,
     * this is the recommended size.
     */
    unsigned int MBEDTLS_PRIVATE(iv_size) : 3;

    /** The cipher key length, in bits (right shifted by #MBEDTLS_KEY_BITLEN_SHIFT).
     * This is the default length for variable sized ciphers.
     * Includes parity bits for ciphers like DES.
     */
    unsigned int MBEDTLS_PRIVATE(key_bitlen) : 4;

    /** The cipher mode (as per mbedtls_cipher_mode_t).
     * For example, MBEDTLS_MODE_CBC.
     */
    unsigned int MBEDTLS_PRIVATE(mode) : 4;

    /** Full cipher identifier (as per mbedtls_cipher_type_t).
     * For example, MBEDTLS_CIPHER_AES_256_CBC.
     *
     * This could be 7 bits, but 8 bits retains byte alignment for the
     * next field, which reduces code size to access that field.
     */
    unsigned int MBEDTLS_PRIVATE(type) : 8;

    /** Bitflag comprised of MBEDTLS_CIPHER_VARIABLE_IV_LEN and
     *  MBEDTLS_CIPHER_VARIABLE_KEY_LEN indicating whether the
     *  cipher supports variable IV or variable key sizes, respectively.
     */
    unsigned int MBEDTLS_PRIVATE(flags) : 2;

    /** Index to LUT for base cipher information and functions. */
    unsigned int MBEDTLS_PRIVATE(base_idx) : 5;

} mbedtls_cipher_info_t;

/* For internal use only.
 * These are used to more compactly represent the fields above. */
#define MBEDTLS_KEY_BITLEN_SHIFT  6
#define MBEDTLS_IV_SIZE_SHIFT     2
/**
 * Generic cipher context.
 */
typedef struct mbedtls_cipher_context_t {
    /** Information about the associated cipher. */
    const mbedtls_cipher_info_t *MBEDTLS_PRIVATE(cipher_info);

    /** Key length to use. */
    int MBEDTLS_PRIVATE(key_bitlen);

    /** Operation that the key of the context has been
     * initialized for.
     */
    mbedtls_operation_t MBEDTLS_PRIVATE(operation);

#if defined(MBEDTLS_CIPHER_MODE_WITH_PADDING)
    /** Padding functions to use, if relevant for
     * the specific cipher mode.
     */
    void(*MBEDTLS_PRIVATE(add_padding))(unsigned char *output, size_t olen, size_t data_len);
    int(*MBEDTLS_PRIVATE(get_padding))(unsigned char *input, size_t ilen, size_t *data_len);
#endif

    /** Buffer for input that has not been processed yet. */
    unsigned char MBEDTLS_PRIVATE(unprocessed_data)[MBEDTLS_MAX_BLOCK_LENGTH];

    /** Number of Bytes that have not been processed yet. */
    size_t MBEDTLS_PRIVATE(unprocessed_len);

    /** Current IV or NONCE_COUNTER for CTR-mode, data unit (or sector) number
     * for XTS-mode. */
    unsigned char MBEDTLS_PRIVATE(iv)[MBEDTLS_MAX_IV_LENGTH];

    /** IV size in Bytes, for ciphers with variable-length IVs. */
    size_t MBEDTLS_PRIVATE(iv_size);

    /** The cipher-specific context. */
    void *MBEDTLS_PRIVATE(cipher_ctx);

#if defined(MBEDTLS_CMAC_C)
    /** CMAC-specific context. */
    mbedtls_cmac_context_t *MBEDTLS_PRIVATE(cmac_ctx);
#endif

#if defined(MBEDTLS_USE_PSA_CRYPTO) && !defined(MBEDTLS_DEPRECATED_REMOVED)
    /** Indicates whether the cipher operations should be performed
     *  by Mbed TLS' own crypto library or an external implementation
     *  of the PSA Crypto API.
     *  This is unset if the cipher context was established through
     *  mbedtls_cipher_setup(), and set if it was established through
     *  mbedtls_cipher_setup_psa().
     */
    unsigned char MBEDTLS_PRIVATE(psa_enabled);
#endif /* MBEDTLS_USE_PSA_CRYPTO && !MBEDTLS_DEPRECATED_REMOVED */

} mbedtls_cipher_context_t;

/**
 * \brief This function retrieves the list of ciphers supported
 *        by the generic cipher module.
 *
 *        For any cipher identifier in the returned list, you can
 *        obtain the corresponding generic cipher information structure
 *        via mbedtls_cipher_info_from_type(), which can then be used
 *        to prepare a cipher context via mbedtls_cipher_setup().
 *
 *
 * \return      A statically-allocated array of cipher identifiers
 *              of type cipher_type_t. The last entry is zero.
 */
const int *mbedtls_cipher_list(void);

/**
 * \brief               This function retrieves the cipher-information
 *                      structure associated with the given cipher name.
 *
 * \param cipher_name   Name of the cipher to search for. This must not be
 *                      \c NULL.
 *
 * \return              The cipher information structure associated with the
 *                      given \p cipher_name.
 * \return              \c NULL if the associated cipher information is not found.
 */
const mbedtls_cipher_info_t *mbedtls_cipher_info_from_string(const char *cipher_name);

/**
 * \brief               This function retrieves the cipher-information
 *                      structure associated with the given cipher type.
 *
 * \param cipher_type   Type of the cipher to search for.
 *
 * \return              The cipher information structure associated with the
 *                      given \p cipher_type.
 * \return              \c NULL if the associated cipher information is not found.
 */
const mbedtls_cipher_info_t *mbedtls_cipher_info_from_type(const mbedtls_cipher_type_t cipher_type);

/**
 * \brief               This function retrieves the cipher-information
 *                      structure associated with the given cipher ID,
 *                      key size and mode.
 *
 * \param cipher_id     The ID of the cipher to search for. For example,
 *                      #MBEDTLS_CIPHER_ID_AES.
 * \param key_bitlen    The length of the key in bits.
 * \param mode          The cipher mode. For example, #MBEDTLS_MODE_CBC.
 *
 * \return              The cipher information structure associated with the
 *                      given \p cipher_id.
 * \return              \c NULL if the associated cipher information is not found.
 */
const mbedtls_cipher_info_t *mbedtls_cipher_info_from_values(const mbedtls_cipher_id_t cipher_id,
                                                             int key_bitlen,
                                                             const mbedtls_cipher_mode_t mode);

/**
 * \brief               Retrieve the identifier for a cipher info structure.
 *
 * \param[in] info      The cipher info structure to query.
 *                      This may be \c NULL.
 *
 * \return              The full cipher identifier (\c MBEDTLS_CIPHER_xxx).
 * \return              #MBEDTLS_CIPHER_NONE if \p info is \c NULL.
 */
static inline mbedtls_cipher_type_t mbedtls_cipher_info_get_type(
    const mbedtls_cipher_info_t *info)
{
    if (info == NULL) {
        return MBEDTLS_CIPHER_NONE;
    } else {
        return (mbedtls_cipher_type_t) info->MBEDTLS_PRIVATE(type);
    }
}

/**
 * \brief               Retrieve the operation mode for a cipher info structure.
 *
 * \param[in] info      The cipher info structure to query.
 *                      This may be \c NULL.
 *
 * \return              The cipher mode (\c MBEDTLS_MODE_xxx).
 * \return              #MBEDTLS_MODE_NONE if \p info is \c NULL.
 */
static inline mbedtls_cipher_mode_t mbedtls_cipher_info_get_mode(
    const mbedtls_cipher_info_t *info)
{
    if (info == NULL) {
        return MBEDTLS_MODE_NONE;
    } else {
        return (mbedtls_cipher_mode_t) info->MBEDTLS_PRIVATE(mode);
    }
}

/**
 * \brief               Retrieve the key size for a cipher info structure.
 *
 * \param[in] info      The cipher info structure to query.
 *                      This may be \c NULL.
 *
 * \return              The key length in bits.
 *                      For variable-sized ciphers, this is the default length.
 *                      For DES, this includes the parity bits.
 * \return              \c 0 if \p info is \c NULL.
 */
static inline size_t mbedtls_cipher_info_get_key_bitlen(
    const mbedtls_cipher_info_t *info)
{
    if (info == NULL) {
        return 0;
    } else {
        return ((size_t) info->MBEDTLS_PRIVATE(key_bitlen)) << MBEDTLS_KEY_BITLEN_SHIFT;
    }
}

/**
 * \brief               Retrieve the human-readable name for a
 *                      cipher info structure.
 *
 * \param[in] info      The cipher info structure to query.
 *                      This may be \c NULL.
 *
 * \return              The cipher name, which is a human readable string,
 *                      with static storage duration.
 * \return              \c NULL if \p info is \c NULL.
 */
static inline const char *mbedtls_cipher_info_get_name(
    const mbedtls_cipher_info_t *info)
{
    if (info == NULL) {
        return NULL;
    } else {
        return info->MBEDTLS_PRIVATE(name);
    }
}

/**
 * \brief       This function returns the size of the IV or nonce
 *              for the cipher info structure, in bytes.
 *
 * \param info  The cipher info structure. This may be \c NULL.
 *
 * \return      The recommended IV size.
 * \return      \c 0 for ciphers not using an IV or a nonce.
 * \return      \c 0 if \p info is \c NULL.
 */
static inline size_t mbedtls_cipher_info_get_iv_size(
    const mbedtls_cipher_info_t *info)
{
    if (info == NULL) {
        return 0;
    }

    return ((size_t) info->MBEDTLS_PRIVATE(iv_size)) << MBEDTLS_IV_SIZE_SHIFT;
}

/**
 * \brief        This function returns the block size of the given
 *               cipher info structure in bytes.
 *
 * \param info   The cipher info structure. This may be \c NULL.
 *
 * \return       The block size of the cipher.
 * \return       \c 1 if the cipher is a stream cipher.
 * \return       \c 0 if \p info is \c NULL.
 */
static inline size_t mbedtls_cipher_info_get_block_size(
    const mbedtls_cipher_info_t *info)
{
    if (info == NULL) {
        return 0;
    }

    return (size_t) (info->MBEDTLS_PRIVATE(block_size));
}

/**
 * \brief        This function returns a non-zero value if the key length for
 *               the given cipher is variable.
 *
 * \param info   The cipher info structure. This may be \c NULL.
 *
 * \return       Non-zero if the key length is variable, \c 0 otherwise.
 * \return       \c 0 if the given pointer is \c NULL.
 */
static inline int mbedtls_cipher_info_has_variable_key_bitlen(
    const mbedtls_cipher_info_t *info)
{
    if (info == NULL) {
        return 0;
    }

    return info->MBEDTLS_PRIVATE(flags) & MBEDTLS_CIPHER_VARIABLE_KEY_LEN;
}

/**
 * \brief        This function returns a non-zero value if the IV size for
 *               the given cipher is variable.
 *
 * \param info   The cipher info structure. This may be \c NULL.
 *
 * \return       Non-zero if the IV size is variable, \c 0 otherwise.
 * \return       \c 0 if the given pointer is \c NULL.
 */
static inline int mbedtls_cipher_info_has_variable_iv_size(
    const mbedtls_cipher_info_t *info)
{
    if (info == NULL) {
        return 0;
    }

    return info->MBEDTLS_PRIVATE(flags) & MBEDTLS_CIPHER_VARIABLE_IV_LEN;
}

/**
 * \brief               This function initializes a \p ctx as NONE.
 *
 * \param ctx           The context to be initialized. This must not be \c NULL.
 */
void mbedtls_cipher_init(mbedtls_cipher_context_t *ctx);

/**
 * \brief               This function frees and clears the cipher-specific
 *                      context of \p ctx. Freeing \p ctx itself remains the
 *                      responsibility of the caller.
 *
 * \param ctx           The context to be freed. If this is \c NULL, the
 *                      function has no effect, otherwise this must point to an
 *                      initialized context.
 */
void mbedtls_cipher_free(mbedtls_cipher_context_t *ctx);


/**
 * \brief               This function prepares a cipher context for
 *                      use with the given cipher primitive.
 *
 * \note                After calling this function, you should call
 *                      mbedtls_cipher_setkey() and, if the mode uses padding,
 *                      mbedtls_cipher_set_padding_mode(), then for each
 *                      message to encrypt or decrypt with this key, either:
 *                      - mbedtls_cipher_crypt() for one-shot processing with
 *                      non-AEAD modes;
 *                      - mbedtls_cipher_auth_encrypt_ext() or
 *                      mbedtls_cipher_auth_decrypt_ext() for one-shot
 *                      processing with AEAD modes or NIST_KW;
 *                      - for multi-part processing, see the documentation of
 *                      mbedtls_cipher_reset().
 *
 * \param ctx           The context to prepare. This must be initialized by
 *                      a call to mbedtls_cipher_init() first.
 * \param cipher_info   The cipher to use.
 *
 * \return              \c 0 on success.
 * \return              #MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA on
 *                      parameter-verification failure.
 * \return              #MBEDTLS_ERR_CIPHER_ALLOC_FAILED if allocation of the
 *                      cipher-specific context fails.
 */
int mbedtls_cipher_setup(mbedtls_cipher_context_t *ctx,
                         const mbedtls_cipher_info_t *cipher_info);

#if defined(MBEDTLS_USE_PSA_CRYPTO)
#if !defined(MBEDTLS_DEPRECATED_REMOVED)
/**
 * \brief               This function initializes a cipher context for
 *                      PSA-based use with the given cipher primitive.
 *
 * \deprecated          This function is deprecated and will be removed in a
 *                      future version of the library.
 *                      Please use psa_aead_xxx() / psa_cipher_xxx() directly
 *                      instead.
 *
 * \note                See #MBEDTLS_USE_PSA_CRYPTO for information on PSA.
 *
 * \param ctx           The context to initialize. May not be \c NULL.
 * \param cipher_info   The cipher to use.
 * \param taglen        For AEAD ciphers, the length in bytes of the
 *                      authentication tag to use. Subsequent uses of
 *                      mbedtls_cipher_auth_encrypt_ext() or
 *                      mbedtls_cipher_auth_decrypt_ext() must provide
 *                      the same tag length.
 *                      For non-AEAD ciphers, the value must be \c 0.
 *
 * \return              \c 0 on success.
 * \return              #MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA on
 *                      parameter-verification failure.
 * \return              #MBEDTLS_ERR_CIPHER_ALLOC_FAILED if allocation of the
 *                      cipher-specific context fails.
 */
int MBEDTLS_DEPRECATED mbedtls_cipher_setup_psa(mbedtls_cipher_context_t *ctx,
                                                const mbedtls_cipher_info_t *cipher_info,
                                                size_t taglen);
#endif /* MBEDTLS_DEPRECATED_REMOVED */
#endif /* MBEDTLS_USE_PSA_CRYPTO */

/**
 * \brief        This function returns the block size of the given cipher
 *               in bytes.
 *
 * \param ctx    The context of the cipher.
 *
 * \return       The block size of the underlying cipher.
 * \return       \c 1 if the cipher is a stream cipher.
 * \return       \c 0 if \p ctx has not been initialized.
 */
static inline unsigned int mbedtls_cipher_get_block_size(
    const mbedtls_cipher_context_t *ctx)
{
    if (ctx->MBEDTLS_PRIVATE(cipher_info) == NULL) {
        return 0;
    }

    return (unsigned int) ctx->MBEDTLS_PRIVATE(cipher_info)->MBEDTLS_PRIVATE(block_size);
}

/**
 * \brief        This function returns the mode of operation for
 *               the cipher. For example, MBEDTLS_MODE_CBC.
 *
 * \param ctx    The context of the cipher. This must be initialized.
 *
 * \return       The mode of operation.
 * \return       #MBEDTLS_MODE_NONE if \p ctx has not been initialized.
 */
static inline mbedtls_cipher_mode_t mbedtls_cipher_get_cipher_mode(
    const mbedtls_cipher_context_t *ctx)
{
    if (ctx->MBEDTLS_PRIVATE(cipher_info) == NULL) {
        return MBEDTLS_MODE_NONE;
    }

    return (mbedtls_cipher_mode_t) ctx->MBEDTLS_PRIVATE(cipher_info)->MBEDTLS_PRIVATE(mode);
}

/**
 * \brief       This function returns the size of the IV or nonce
 *              of the cipher, in Bytes.
 *
 * \param ctx   The context of the cipher. This must be initialized.
 *
 * \return      The recommended IV size if no IV has been set.
 * \return      \c 0 for ciphers not using an IV or a nonce.
 * \return      The actual size if an IV has been set.
 */
static inline int mbedtls_cipher_get_iv_size(
    const mbedtls_cipher_context_t *ctx)
{
    if (ctx->MBEDTLS_PRIVATE(cipher_info) == NULL) {
        return 0;
    }

    if (ctx->MBEDTLS_PRIVATE(iv_size) != 0) {
        return (int) ctx->MBEDTLS_PRIVATE(iv_size);
    }

    return (int) (((int) ctx->MBEDTLS_PRIVATE(cipher_info)->MBEDTLS_PRIVATE(iv_size)) <<
                  MBEDTLS_IV_SIZE_SHIFT);
}

/**
 * \brief               This function returns the type of the given cipher.
 *
 * \param ctx           The context of the cipher. This must be initialized.
 *
 * \return              The type of the cipher.
 * \return              #MBEDTLS_CIPHER_NONE if \p ctx has not been initialized.
 */
static inline mbedtls_cipher_type_t mbedtls_cipher_get_type(
    const mbedtls_cipher_context_t *ctx)
{
    if (ctx->MBEDTLS_PRIVATE(cipher_info) == NULL) {
        return MBEDTLS_CIPHER_NONE;
    }

    return (mbedtls_cipher_type_t) ctx->MBEDTLS_PRIVATE(cipher_info)->MBEDTLS_PRIVATE(type);
}

/**
 * \brief               This function returns the name of the given cipher
 *                      as a string.
 *
 * \param ctx           The context of the cipher. This must be initialized.
 *
 * \return              The name of the cipher.
 * \return              NULL if \p ctx has not been not initialized.
 */
static inline const char *mbedtls_cipher_get_name(
    const mbedtls_cipher_context_t *ctx)
{
    if (ctx->MBEDTLS_PRIVATE(cipher_info) == NULL) {
        return 0;
    }

    return ctx->MBEDTLS_PRIVATE(cipher_info)->MBEDTLS_PRIVATE(name);
}

/**
 * \brief               This function returns the key length of the cipher.
 *
 * \param ctx           The context of the cipher. This must be initialized.
 *
 * \return              The key length of the cipher in bits.
 * \return              #MBEDTLS_KEY_LENGTH_NONE if \p ctx has not been
 *                      initialized.
 */
static inline int mbedtls_cipher_get_key_bitlen(
    const mbedtls_cipher_context_t *ctx)
{
    if (ctx->MBEDTLS_PRIVATE(cipher_info) == NULL) {
        return MBEDTLS_KEY_LENGTH_NONE;
    }

    return (int) ctx->MBEDTLS_PRIVATE(cipher_info)->MBEDTLS_PRIVATE(key_bitlen) <<
           MBEDTLS_KEY_BITLEN_SHIFT;
}

/**
 * \brief          This function returns the operation of the given cipher.
 *
 * \param ctx      The context of the cipher. This must be initialized.
 *
 * \return         The type of operation: #MBEDTLS_ENCRYPT or #MBEDTLS_DECRYPT.
 * \return         #MBEDTLS_OPERATION_NONE if \p ctx has not been initialized.
 */
static inline mbedtls_operation_t mbedtls_cipher_get_operation(
    const mbedtls_cipher_context_t *ctx)
{
    if (ctx->MBEDTLS_PRIVATE(cipher_info) == NULL) {
        return MBEDTLS_OPERATION_NONE;
    }

    return ctx->MBEDTLS_PRIVATE(operation);
}

/**
 * \brief               This function sets the key to use with the given context.
 *
 * \param ctx           The generic cipher context. This must be initialized and
 *                      bound to a cipher information structure.
 * \param key           The key to use. This must be a readable buffer of at
 *                      least \p key_bitlen Bits.
 * \param key_bitlen    The key length to use, in Bits.
 * \param operation     The operation that the key will be used for:
 *                      #MBEDTLS_ENCRYPT or #MBEDTLS_DECRYPT.
 *
 * \return              \c 0 on success.
 * \return              #MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA on
 *                      parameter-verification failure.
 * \return              A cipher-specific error code on failure.
 */
int mbedtls_cipher_setkey(mbedtls_cipher_context_t *ctx,
                          const unsigned char *key,
                          int key_bitlen,
                          const mbedtls_operation_t operation);

#if defined(MBEDTLS_CIPHER_MODE_WITH_PADDING)
/**
 * \brief               This function sets the padding mode, for cipher modes
 *                      that use padding.
 *
 *
 * \param ctx           The generic cipher context. This must be initialized and
 *                      bound to a cipher information structure.
 * \param mode          The padding mode.
 *
 * \return              \c 0 on success.
 * \return              #MBEDTLS_ERR_CIPHER_FEATURE_UNAVAILABLE
 *                      if the selected padding mode is not supported.
 * \return              #MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA if the cipher mode
 *                      does not support padding.
 */
int mbedtls_cipher_set_padding_mode(mbedtls_cipher_context_t *ctx,
                                    mbedtls_cipher_padding_t mode);
#endif /* MBEDTLS_CIPHER_MODE_WITH_PADDING */

/**
 * \brief           This function sets the initialization vector (IV)
 *                  or nonce.
 *
 * \note            Some ciphers do not use IVs nor nonce. For these
 *                  ciphers, this function has no effect.
 *
 * \note            For #MBEDTLS_CIPHER_CHACHA20, the nonce length must
 *                  be 12, and the initial counter value is 0.
 *
 * \note            For #MBEDTLS_CIPHER_CHACHA20_POLY1305, the nonce length
 *                  must be 12.
 *
 * \param ctx       The generic cipher context. This must be initialized and
 *                  bound to a cipher information structure.
 * \param iv        The IV to use, or NONCE_COUNTER for CTR-mode ciphers. This
 *                  must be a readable buffer of at least \p iv_len Bytes.
 * \param iv_len    The IV length for ciphers with variable-size IV.
 *                  This parameter is discarded by ciphers with fixed-size IV.
 *
 * \return          \c 0 on success.
 * \return          #MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA on
 *                  parameter-verification failure.
 */
int mbedtls_cipher_set_iv(mbedtls_cipher_context_t *ctx,
                          const unsigned char *iv,
                          size_t iv_len);

/**
 * \brief         This function resets the cipher state.
 *
 * \note          With non-AEAD ciphers, the order of calls for each message
 *                is as follows:
 *                1. mbedtls_cipher_set_iv() if the mode uses an IV/nonce.
 *                2. mbedtls_cipher_reset()
 *                3. mbedtls_cipher_update() one or more times
 *                4. mbedtls_cipher_finish()
 *                .
 *                This sequence can be repeated to encrypt or decrypt multiple
 *                messages with the same key.
 *
 * \note          With AEAD ciphers, the order of calls for each message
 *                is as follows:
 *                1. mbedtls_cipher_set_iv() if the mode uses an IV/nonce.
 *                2. mbedtls_cipher_reset()
 *                3. mbedtls_cipher_update_ad()
 *                4. mbedtls_cipher_update() one or more times
 *                5. mbedtls_cipher_finish()
 *                6. mbedtls_cipher_check_tag() (for decryption) or
 *                mbedtls_cipher_write_tag() (for encryption).
 *                .
 *                This sequence can be repeated to encrypt or decrypt multiple
 *                messages with the same key.
 *
 * \param ctx     The generic cipher context. This must be bound to a key.
 *
 * \return        \c 0 on success.
 * \return        #MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA on
 *                parameter-verification failure.
 */
int mbedtls_cipher_reset(mbedtls_cipher_context_t *ctx);

#if defined(MBEDTLS_GCM_C) || defined(MBEDTLS_CHACHAPOLY_C)
/**
 * \brief               This function adds additional data for AEAD ciphers.
 *                      Currently supported with GCM and ChaCha20+Poly1305.
 *
 * \param ctx           The generic cipher context. This must be initialized.
 * \param ad            The additional data to use. This must be a readable
 *                      buffer of at least \p ad_len Bytes.
 * \param ad_len        The length of \p ad in Bytes.
 *
 * \return              \c 0 on success.
 * \return              A specific error code on failure.
 */
int mbedtls_cipher_update_ad(mbedtls_cipher_context_t *ctx,
                             const unsigned char *ad, size_t ad_len);
#endif /* MBEDTLS_GCM_C || MBEDTLS_CHACHAPOLY_C */

/**
 * \brief               The generic cipher update function. It encrypts or
 *                      decrypts using the given cipher context. Writes as
 *                      many block-sized blocks of data as possible to output.
 *                      Any data that cannot be written immediately is either
 *                      added to the next block, or flushed when
 *                      mbedtls_cipher_finish() is called.
 *                      Exception: For MBEDTLS_MODE_ECB, expects a single block
 *                      in size. For example, 16 Bytes for AES.
 *
 * \param ctx           The generic cipher context. This must be initialized and
 *                      bound to a key.
 * \param input         The buffer holding the input data. This must be a
 *                      readable buffer of at least \p ilen Bytes.
 * \param ilen          The length of the input data.
 * \param output        The buffer for the output data. This must be able to
 *                      hold at least `ilen + block_size`. This must not be the
 *                      same buffer as \p input.
 * \param olen          The length of the output data, to be updated with the
 *                      actual number of Bytes written. This must not be
 *                      \c NULL.
 *
 * \return              \c 0 on success.
 * \return              #MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA on
 *                      parameter-verification failure.
 * \return              #MBEDTLS_ERR_CIPHER_FEATURE_UNAVAILABLE on an
 *                      unsupported mode for a cipher.
 * \return              A cipher-specific error code on failure.
 */
int mbedtls_cipher_update(mbedtls_cipher_context_t *ctx,
                          const unsigned char *input,
                          size_t ilen, unsigned char *output,
                          size_t *olen);

/**
 * \brief               The generic cipher finalization function. If data still
 *                      needs to be flushed from an incomplete block, the data
 *                      contained in it is padded to the size of
 *                      the last block, and written to the \p output buffer.
 *
 * \param ctx           The generic cipher context. This must be initialized and
 *                      bound to a key.
 * \param output        The buffer to write data to. This needs to be a writable
 *                      buffer of at least block_size Bytes.
 * \param olen          The length of the data written to the \p output buffer.
 *                      This may not be \c NULL.
 *
 * \return              \c 0 on success.
 * \return              #MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA on
 *                      parameter-verification failure.
 * \return              #MBEDTLS_ERR_CIPHER_FULL_BLOCK_EXPECTED on decryption
 *                      expecting a full block but not receiving one.
 * \return              #MBEDTLS_ERR_CIPHER_INVALID_PADDING on invalid padding
 *                      while decrypting.
 * \return              A cipher-specific error code on failure.
 */
int mbedtls_cipher_finish(mbedtls_cipher_context_t *ctx,
                          unsigned char *output, size_t *olen);

#if defined(MBEDTLS_GCM_C) || defined(MBEDTLS_CHACHAPOLY_C)
/**
 * \brief               This function writes a tag for AEAD ciphers.
 *                      Currently supported with GCM and ChaCha20+Poly1305.
 *                      This must be called after mbedtls_cipher_finish().
 *
 * \param ctx           The generic cipher context. This must be initialized,
 *                      bound to a key, and have just completed a cipher
 *                      operation through mbedtls_cipher_finish() the tag for
 *                      which should be written.
 * \param tag           The buffer to write the tag to. This must be a writable
 *                      buffer of at least \p tag_len Bytes.
 * \param tag_len       The length of the tag to write.
 *
 * \return              \c 0 on success.
 * \return              A specific error code on failure.
 */
int mbedtls_cipher_write_tag(mbedtls_cipher_context_t *ctx,
                             unsigned char *tag, size_t tag_len);

/**
 * \brief               This function checks the tag for AEAD ciphers.
 *                      Currently supported with GCM and ChaCha20+Poly1305.
 *                      This must be called after mbedtls_cipher_finish().
 *
 * \param ctx           The generic cipher context. This must be initialized.
 * \param tag           The buffer holding the tag. This must be a readable
 *                      buffer of at least \p tag_len Bytes.
 * \param tag_len       The length of the tag to check.
 *
 * \return              \c 0 on success.
 * \return              A specific error code on failure.
 */
int mbedtls_cipher_check_tag(mbedtls_cipher_context_t *ctx,
                             const unsigned char *tag, size_t tag_len);
#endif /* MBEDTLS_GCM_C || MBEDTLS_CHACHAPOLY_C */

/**
 * \brief               The generic all-in-one encryption/decryption function,
 *                      for all ciphers except AEAD constructs.
 *
 * \param ctx           The generic cipher context. This must be initialized.
 * \param iv            The IV to use, or NONCE_COUNTER for CTR-mode ciphers.
 *                      This must be a readable buffer of at least \p iv_len
 *                      Bytes.
 * \param iv_len        The IV length for ciphers with variable-size IV.
 *                      This parameter is discarded by ciphers with fixed-size
 *                      IV.
 * \param input         The buffer holding the input data. This must be a
 *                      readable buffer of at least \p ilen Bytes.
 * \param ilen          The length of the input data in Bytes.
 * \param output        The buffer for the output data. This must be able to
 *                      hold at least `ilen + block_size`. This must not be the
 *                      same buffer as \p input.
 * \param olen          The length of the output data, to be updated with the
 *                      actual number of Bytes written. This must not be
 *                      \c NULL.
 *
 * \note                Some ciphers do not use IVs nor nonce. For these
 *                      ciphers, use \p iv = NULL and \p iv_len = 0.
 *
 * \return              \c 0 on success.
 * \return              #MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA on
 *                      parameter-verification failure.
 * \return              #MBEDTLS_ERR_CIPHER_FULL_BLOCK_EXPECTED on decryption
 *                      expecting a full block but not receiving one.
 * \return              #MBEDTLS_ERR_CIPHER_INVALID_PADDING on invalid padding
 *                      while decrypting.
 * \return              A cipher-specific error code on failure.
 */
int mbedtls_cipher_crypt(mbedtls_cipher_context_t *ctx,
                         const unsigned char *iv, size_t iv_len,
                         const unsigned char *input, size_t ilen,
                         unsigned char *output, size_t *olen);

#if defined(MBEDTLS_CIPHER_MODE_AEAD) || defined(MBEDTLS_NIST_KW_C)
/**
 * \brief               The authenticated encryption (AEAD/NIST_KW) function.
 *
 * \note                For AEAD modes, the tag will be appended to the
 *                      ciphertext, as recommended by RFC 5116.
 *                      (NIST_KW doesn't have a separate tag.)
 *
 * \param ctx           The generic cipher context. This must be initialized and
 *                      bound to a key, with an AEAD algorithm or NIST_KW.
 * \param iv            The nonce to use. This must be a readable buffer of
 *                      at least \p iv_len Bytes and may be \c NULL if \p
 *                      iv_len is \c 0.
 * \param iv_len        The length of the nonce. For AEAD ciphers, this must
 *                      satisfy the constraints imposed by the cipher used.
 *                      For NIST_KW, this must be \c 0.
 * \param ad            The additional data to authenticate. This must be a
 *                      readable buffer of at least \p ad_len Bytes, and may
 *                      be \c NULL is \p ad_len is \c 0.
 * \param ad_len        The length of \p ad. For NIST_KW, this must be \c 0.
 * \param input         The buffer holding the input data. This must be a
 *                      readable buffer of at least \p ilen Bytes, and may be
 *                      \c NULL if \p ilen is \c 0.
 * \param ilen          The length of the input data.
 * \param output        The buffer for the output data. This must be a
 *                      writable buffer of at least \p output_len Bytes, and
 *                      must not be \c NULL.
 * \param output_len    The length of the \p output buffer in Bytes. For AEAD
 *                      ciphers, this must be at least \p ilen + \p tag_len.
 *                      For NIST_KW, this must be at least \p ilen + 8
 *                      (rounded up to a multiple of 8 if KWP is used);
 *                      \p ilen + 15 is always a safe value.
 * \param olen          This will be filled with the actual number of Bytes
 *                      written to the \p output buffer. This must point to a
 *                      writable object of type \c size_t.
 * \param tag_len       The desired length of the authentication tag. For AEAD
 *                      ciphers, this must match the constraints imposed by
 *                      the cipher used, and in particular must not be \c 0.
 *                      For NIST_KW, this must be \c 0.
 *
 * \return              \c 0 on success.
 * \return              #MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA on
 *                      parameter-verification failure.
 * \return              A cipher-specific error code on failure.
 */
int mbedtls_cipher_auth_encrypt_ext(mbedtls_cipher_context_t *ctx,
                                    const unsigned char *iv, size_t iv_len,
                                    const unsigned char *ad, size_t ad_len,
                                    const unsigned char *input, size_t ilen,
                                    unsigned char *output, size_t output_len,
                                    size_t *olen, size_t tag_len);

/**
 * \brief               The authenticated encryption (AEAD/NIST_KW) function.
 *
 * \note                If the data is not authentic, then the output buffer
 *                      is zeroed out to prevent the unauthentic plaintext being
 *                      used, making this interface safer.
 *
 * \note                For AEAD modes, the tag must be appended to the
 *                      ciphertext, as recommended by RFC 5116.
 *                      (NIST_KW doesn't have a separate tag.)
 *
 * \param ctx           The generic cipher context. This must be initialized and
 *                      bound to a key, with an AEAD algorithm or NIST_KW.
 * \param iv            The nonce to use. This must be a readable buffer of
 *                      at least \p iv_len Bytes and may be \c NULL if \p
 *                      iv_len is \c 0.
 * \param iv_len        The length of the nonce. For AEAD ciphers, this must
 *                      satisfy the constraints imposed by the cipher used.
 *                      For NIST_KW, this must be \c 0.
 * \param ad            The additional data to authenticate. This must be a
 *                      readable buffer of at least \p ad_len Bytes, and may
 *                      be \c NULL is \p ad_len is \c 0.
 * \param ad_len        The length of \p ad. For NIST_KW, this must be \c 0.
 * \param input         The buffer holding the input data. This must be a
 *                      readable buffer of at least \p ilen Bytes, and may be
 *                      \c NULL if \p ilen is \c 0.
 * \param ilen          The length of the input data. For AEAD ciphers this
 *                      must be at least \p tag_len. For NIST_KW this must be
 *                      at least \c 8.
 * \param output        The buffer for the output data. This must be a
 *                      writable buffer of at least \p output_len Bytes, and
 *                      may be \c NULL if \p output_len is \c 0.
 * \param output_len    The length of the \p output buffer in Bytes. For AEAD
 *                      ciphers, this must be at least \p ilen - \p tag_len.
 *                      For NIST_KW, this must be at least \p ilen - 8.
 * \param olen          This will be filled with the actual number of Bytes
 *                      written to the \p output buffer. This must point to a
 *                      writable object of type \c size_t.
 * \param tag_len       The actual length of the authentication tag. For AEAD
 *                      ciphers, this must match the constraints imposed by
 *                      the cipher used, and in particular must not be \c 0.
 *                      For NIST_KW, this must be \c 0.
 *
 * \return              \c 0 on success.
 * \return              #MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA on
 *                      parameter-verification failure.
 * \return              #MBEDTLS_ERR_CIPHER_AUTH_FAILED if data is not authentic.
 * \return              A cipher-specific error code on failure.
 */
int mbedtls_cipher_auth_decrypt_ext(mbedtls_cipher_context_t *ctx,
                                    const unsigned char *iv, size_t iv_len,
                                    const unsigned char *ad, size_t ad_len,
                                    const unsigned char *input, size_t ilen,
                                    unsigned char *output, size_t output_len,
                                    size_t *olen, size_t tag_len);
#endif /* MBEDTLS_CIPHER_MODE_AEAD || MBEDTLS_NIST_KW_C */
#ifdef __cplusplus
}
#endif

#endif /* MBEDTLS_CIPHER_H */
