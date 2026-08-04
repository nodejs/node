/*
 *  Version feature information
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#include "common.h"

#if defined(MBEDTLS_VERSION_C)

#include "mbedtls/version.h"

#include <string.h>

static const char * const features[] = {
#if defined(MBEDTLS_VERSION_FEATURES)
    #if defined(MBEDTLS_HAVE_ASM)
    "HAVE_ASM", //no-check-names
#endif /* MBEDTLS_HAVE_ASM */
#if defined(MBEDTLS_NO_UDBL_DIVISION)
    "NO_UDBL_DIVISION", //no-check-names
#endif /* MBEDTLS_NO_UDBL_DIVISION */
#if defined(MBEDTLS_NO_64BIT_MULTIPLICATION)
    "NO_64BIT_MULTIPLICATION", //no-check-names
#endif /* MBEDTLS_NO_64BIT_MULTIPLICATION */
#if defined(MBEDTLS_HAVE_SSE2)
    "HAVE_SSE2", //no-check-names
#endif /* MBEDTLS_HAVE_SSE2 */
#if defined(MBEDTLS_HAVE_TIME)
    "HAVE_TIME", //no-check-names
#endif /* MBEDTLS_HAVE_TIME */
#if defined(MBEDTLS_HAVE_TIME_DATE)
    "HAVE_TIME_DATE", //no-check-names
#endif /* MBEDTLS_HAVE_TIME_DATE */
#if defined(MBEDTLS_PLATFORM_MEMORY)
    "PLATFORM_MEMORY", //no-check-names
#endif /* MBEDTLS_PLATFORM_MEMORY */
#if defined(MBEDTLS_PLATFORM_NO_STD_FUNCTIONS)
    "PLATFORM_NO_STD_FUNCTIONS", //no-check-names
#endif /* MBEDTLS_PLATFORM_NO_STD_FUNCTIONS */
#if defined(MBEDTLS_PLATFORM_SETBUF_ALT)
    "PLATFORM_SETBUF_ALT", //no-check-names
#endif /* MBEDTLS_PLATFORM_SETBUF_ALT */
#if defined(MBEDTLS_PLATFORM_EXIT_ALT)
    "PLATFORM_EXIT_ALT", //no-check-names
#endif /* MBEDTLS_PLATFORM_EXIT_ALT */
#if defined(MBEDTLS_PLATFORM_TIME_ALT)
    "PLATFORM_TIME_ALT", //no-check-names
#endif /* MBEDTLS_PLATFORM_TIME_ALT */
#if defined(MBEDTLS_PLATFORM_FPRINTF_ALT)
    "PLATFORM_FPRINTF_ALT", //no-check-names
#endif /* MBEDTLS_PLATFORM_FPRINTF_ALT */
#if defined(MBEDTLS_PLATFORM_PRINTF_ALT)
    "PLATFORM_PRINTF_ALT", //no-check-names
#endif /* MBEDTLS_PLATFORM_PRINTF_ALT */
#if defined(MBEDTLS_PLATFORM_SNPRINTF_ALT)
    "PLATFORM_SNPRINTF_ALT", //no-check-names
#endif /* MBEDTLS_PLATFORM_SNPRINTF_ALT */
#if defined(MBEDTLS_PLATFORM_VSNPRINTF_ALT)
    "PLATFORM_VSNPRINTF_ALT", //no-check-names
#endif /* MBEDTLS_PLATFORM_VSNPRINTF_ALT */
#if defined(MBEDTLS_PLATFORM_NV_SEED_ALT)
    "PLATFORM_NV_SEED_ALT", //no-check-names
#endif /* MBEDTLS_PLATFORM_NV_SEED_ALT */
#if defined(MBEDTLS_PLATFORM_SETUP_TEARDOWN_ALT)
    "PLATFORM_SETUP_TEARDOWN_ALT", //no-check-names
#endif /* MBEDTLS_PLATFORM_SETUP_TEARDOWN_ALT */
#if defined(MBEDTLS_PLATFORM_MS_TIME_ALT)
    "PLATFORM_MS_TIME_ALT", //no-check-names
#endif /* MBEDTLS_PLATFORM_MS_TIME_ALT */
#if defined(MBEDTLS_PLATFORM_GMTIME_R_ALT)
    "PLATFORM_GMTIME_R_ALT", //no-check-names
#endif /* MBEDTLS_PLATFORM_GMTIME_R_ALT */
#if defined(MBEDTLS_PLATFORM_ZEROIZE_ALT)
    "PLATFORM_ZEROIZE_ALT", //no-check-names
#endif /* MBEDTLS_PLATFORM_ZEROIZE_ALT */
#if defined(MBEDTLS_DEPRECATED_WARNING)
    "DEPRECATED_WARNING", //no-check-names
#endif /* MBEDTLS_DEPRECATED_WARNING */
#if defined(MBEDTLS_DEPRECATED_REMOVED)
    "DEPRECATED_REMOVED", //no-check-names
#endif /* MBEDTLS_DEPRECATED_REMOVED */
#if defined(MBEDTLS_TIMING_ALT)
    "TIMING_ALT", //no-check-names
#endif /* MBEDTLS_TIMING_ALT */
#if defined(MBEDTLS_AES_ALT)
    "AES_ALT", //no-check-names
#endif /* MBEDTLS_AES_ALT */
#if defined(MBEDTLS_ARIA_ALT)
    "ARIA_ALT", //no-check-names
#endif /* MBEDTLS_ARIA_ALT */
#if defined(MBEDTLS_CAMELLIA_ALT)
    "CAMELLIA_ALT", //no-check-names
#endif /* MBEDTLS_CAMELLIA_ALT */
#if defined(MBEDTLS_CCM_ALT)
    "CCM_ALT", //no-check-names
#endif /* MBEDTLS_CCM_ALT */
#if defined(MBEDTLS_CHACHA20_ALT)
    "CHACHA20_ALT", //no-check-names
#endif /* MBEDTLS_CHACHA20_ALT */
#if defined(MBEDTLS_CHACHAPOLY_ALT)
    "CHACHAPOLY_ALT", //no-check-names
#endif /* MBEDTLS_CHACHAPOLY_ALT */
#if defined(MBEDTLS_CMAC_ALT)
    "CMAC_ALT", //no-check-names
#endif /* MBEDTLS_CMAC_ALT */
#if defined(MBEDTLS_DES_ALT)
    "DES_ALT", //no-check-names
#endif /* MBEDTLS_DES_ALT */
#if defined(MBEDTLS_DHM_ALT)
    "DHM_ALT", //no-check-names
#endif /* MBEDTLS_DHM_ALT */
#if defined(MBEDTLS_ECJPAKE_ALT)
    "ECJPAKE_ALT", //no-check-names
#endif /* MBEDTLS_ECJPAKE_ALT */
#if defined(MBEDTLS_GCM_ALT)
    "GCM_ALT", //no-check-names
#endif /* MBEDTLS_GCM_ALT */
#if defined(MBEDTLS_NIST_KW_ALT)
    "NIST_KW_ALT", //no-check-names
#endif /* MBEDTLS_NIST_KW_ALT */
#if defined(MBEDTLS_MD5_ALT)
    "MD5_ALT", //no-check-names
#endif /* MBEDTLS_MD5_ALT */
#if defined(MBEDTLS_POLY1305_ALT)
    "POLY1305_ALT", //no-check-names
#endif /* MBEDTLS_POLY1305_ALT */
#if defined(MBEDTLS_RIPEMD160_ALT)
    "RIPEMD160_ALT", //no-check-names
#endif /* MBEDTLS_RIPEMD160_ALT */
#if defined(MBEDTLS_RSA_ALT)
    "RSA_ALT", //no-check-names
#endif /* MBEDTLS_RSA_ALT */
#if defined(MBEDTLS_SHA1_ALT)
    "SHA1_ALT", //no-check-names
#endif /* MBEDTLS_SHA1_ALT */
#if defined(MBEDTLS_SHA256_ALT)
    "SHA256_ALT", //no-check-names
#endif /* MBEDTLS_SHA256_ALT */
#if defined(MBEDTLS_SHA512_ALT)
    "SHA512_ALT", //no-check-names
#endif /* MBEDTLS_SHA512_ALT */
#if defined(MBEDTLS_ECP_ALT)
    "ECP_ALT", //no-check-names
#endif /* MBEDTLS_ECP_ALT */
#if defined(MBEDTLS_MD5_PROCESS_ALT)
    "MD5_PROCESS_ALT", //no-check-names
#endif /* MBEDTLS_MD5_PROCESS_ALT */
#if defined(MBEDTLS_RIPEMD160_PROCESS_ALT)
    "RIPEMD160_PROCESS_ALT", //no-check-names
#endif /* MBEDTLS_RIPEMD160_PROCESS_ALT */
#if defined(MBEDTLS_SHA1_PROCESS_ALT)
    "SHA1_PROCESS_ALT", //no-check-names
#endif /* MBEDTLS_SHA1_PROCESS_ALT */
#if defined(MBEDTLS_SHA256_PROCESS_ALT)
    "SHA256_PROCESS_ALT", //no-check-names
#endif /* MBEDTLS_SHA256_PROCESS_ALT */
#if defined(MBEDTLS_SHA512_PROCESS_ALT)
    "SHA512_PROCESS_ALT", //no-check-names
#endif /* MBEDTLS_SHA512_PROCESS_ALT */
#if defined(MBEDTLS_DES_SETKEY_ALT)
    "DES_SETKEY_ALT", //no-check-names
#endif /* MBEDTLS_DES_SETKEY_ALT */
#if defined(MBEDTLS_DES_CRYPT_ECB_ALT)
    "DES_CRYPT_ECB_ALT", //no-check-names
#endif /* MBEDTLS_DES_CRYPT_ECB_ALT */
#if defined(MBEDTLS_DES3_CRYPT_ECB_ALT)
    "DES3_CRYPT_ECB_ALT", //no-check-names
#endif /* MBEDTLS_DES3_CRYPT_ECB_ALT */
#if defined(MBEDTLS_AES_SETKEY_ENC_ALT)
    "AES_SETKEY_ENC_ALT", //no-check-names
#endif /* MBEDTLS_AES_SETKEY_ENC_ALT */
#if defined(MBEDTLS_AES_SETKEY_DEC_ALT)
    "AES_SETKEY_DEC_ALT", //no-check-names
#endif /* MBEDTLS_AES_SETKEY_DEC_ALT */
#if defined(MBEDTLS_AES_ENCRYPT_ALT)
    "AES_ENCRYPT_ALT", //no-check-names
#endif /* MBEDTLS_AES_ENCRYPT_ALT */
#if defined(MBEDTLS_AES_DECRYPT_ALT)
    "AES_DECRYPT_ALT", //no-check-names
#endif /* MBEDTLS_AES_DECRYPT_ALT */
#if defined(MBEDTLS_ECDH_GEN_PUBLIC_ALT)
    "ECDH_GEN_PUBLIC_ALT", //no-check-names
#endif /* MBEDTLS_ECDH_GEN_PUBLIC_ALT */
#if defined(MBEDTLS_ECDH_COMPUTE_SHARED_ALT)
    "ECDH_COMPUTE_SHARED_ALT", //no-check-names
#endif /* MBEDTLS_ECDH_COMPUTE_SHARED_ALT */
#if defined(MBEDTLS_ECDSA_VERIFY_ALT)
    "ECDSA_VERIFY_ALT", //no-check-names
#endif /* MBEDTLS_ECDSA_VERIFY_ALT */
#if defined(MBEDTLS_ECDSA_SIGN_ALT)
    "ECDSA_SIGN_ALT", //no-check-names
#endif /* MBEDTLS_ECDSA_SIGN_ALT */
#if defined(MBEDTLS_ECDSA_GENKEY_ALT)
    "ECDSA_GENKEY_ALT", //no-check-names
#endif /* MBEDTLS_ECDSA_GENKEY_ALT */
#if defined(MBEDTLS_ECP_INTERNAL_ALT)
    "ECP_INTERNAL_ALT", //no-check-names
#endif /* MBEDTLS_ECP_INTERNAL_ALT */
#if defined(MBEDTLS_ECP_NO_FALLBACK)
    "ECP_NO_FALLBACK", //no-check-names
#endif /* MBEDTLS_ECP_NO_FALLBACK */
#if defined(MBEDTLS_ECP_RANDOMIZE_JAC_ALT)
    "ECP_RANDOMIZE_JAC_ALT", //no-check-names
#endif /* MBEDTLS_ECP_RANDOMIZE_JAC_ALT */
#if defined(MBEDTLS_ECP_ADD_MIXED_ALT)
    "ECP_ADD_MIXED_ALT", //no-check-names
#endif /* MBEDTLS_ECP_ADD_MIXED_ALT */
#if defined(MBEDTLS_ECP_DOUBLE_JAC_ALT)
    "ECP_DOUBLE_JAC_ALT", //no-check-names
#endif /* MBEDTLS_ECP_DOUBLE_JAC_ALT */
#if defined(MBEDTLS_ECP_NORMALIZE_JAC_MANY_ALT)
    "ECP_NORMALIZE_JAC_MANY_ALT", //no-check-names
#endif /* MBEDTLS_ECP_NORMALIZE_JAC_MANY_ALT */
#if defined(MBEDTLS_ECP_NORMALIZE_JAC_ALT)
    "ECP_NORMALIZE_JAC_ALT", //no-check-names
#endif /* MBEDTLS_ECP_NORMALIZE_JAC_ALT */
#if defined(MBEDTLS_ECP_DOUBLE_ADD_MXZ_ALT)
    "ECP_DOUBLE_ADD_MXZ_ALT", //no-check-names
#endif /* MBEDTLS_ECP_DOUBLE_ADD_MXZ_ALT */
#if defined(MBEDTLS_ECP_RANDOMIZE_MXZ_ALT)
    "ECP_RANDOMIZE_MXZ_ALT", //no-check-names
#endif /* MBEDTLS_ECP_RANDOMIZE_MXZ_ALT */
#if defined(MBEDTLS_ECP_NORMALIZE_MXZ_ALT)
    "ECP_NORMALIZE_MXZ_ALT", //no-check-names
#endif /* MBEDTLS_ECP_NORMALIZE_MXZ_ALT */
#if defined(MBEDTLS_ENTROPY_HARDWARE_ALT)
    "ENTROPY_HARDWARE_ALT", //no-check-names
#endif /* MBEDTLS_ENTROPY_HARDWARE_ALT */
#if defined(MBEDTLS_AES_ROM_TABLES)
    "AES_ROM_TABLES", //no-check-names
#endif /* MBEDTLS_AES_ROM_TABLES */
#if defined(MBEDTLS_AES_FEWER_TABLES)
    "AES_FEWER_TABLES", //no-check-names
#endif /* MBEDTLS_AES_FEWER_TABLES */
#if defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
    "AES_ONLY_128_BIT_KEY_LENGTH", //no-check-names
#endif /* MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH */
#if defined(MBEDTLS_AES_USE_HARDWARE_ONLY)
    "AES_USE_HARDWARE_ONLY", //no-check-names
#endif /* MBEDTLS_AES_USE_HARDWARE_ONLY */
#if defined(MBEDTLS_CAMELLIA_SMALL_MEMORY)
    "CAMELLIA_SMALL_MEMORY", //no-check-names
#endif /* MBEDTLS_CAMELLIA_SMALL_MEMORY */
#if defined(MBEDTLS_CHECK_RETURN_WARNING)
    "CHECK_RETURN_WARNING", //no-check-names
#endif /* MBEDTLS_CHECK_RETURN_WARNING */
#if defined(MBEDTLS_CIPHER_MODE_CBC)
    "CIPHER_MODE_CBC", //no-check-names
#endif /* MBEDTLS_CIPHER_MODE_CBC */
#if defined(MBEDTLS_CIPHER_MODE_CFB)
    "CIPHER_MODE_CFB", //no-check-names
#endif /* MBEDTLS_CIPHER_MODE_CFB */
#if defined(MBEDTLS_CIPHER_MODE_CTR)
    "CIPHER_MODE_CTR", //no-check-names
#endif /* MBEDTLS_CIPHER_MODE_CTR */
#if defined(MBEDTLS_CIPHER_MODE_OFB)
    "CIPHER_MODE_OFB", //no-check-names
#endif /* MBEDTLS_CIPHER_MODE_OFB */
#if defined(MBEDTLS_CIPHER_MODE_XTS)
    "CIPHER_MODE_XTS", //no-check-names
#endif /* MBEDTLS_CIPHER_MODE_XTS */
#if defined(MBEDTLS_CIPHER_NULL_CIPHER)
    "CIPHER_NULL_CIPHER", //no-check-names
#endif /* MBEDTLS_CIPHER_NULL_CIPHER */
#if defined(MBEDTLS_CIPHER_PADDING_PKCS7)
    "CIPHER_PADDING_PKCS7", //no-check-names
#endif /* MBEDTLS_CIPHER_PADDING_PKCS7 */
#if defined(MBEDTLS_CIPHER_PADDING_ONE_AND_ZEROS)
    "CIPHER_PADDING_ONE_AND_ZEROS", //no-check-names
#endif /* MBEDTLS_CIPHER_PADDING_ONE_AND_ZEROS */
#if defined(MBEDTLS_CIPHER_PADDING_ZEROS_AND_LEN)
    "CIPHER_PADDING_ZEROS_AND_LEN", //no-check-names
#endif /* MBEDTLS_CIPHER_PADDING_ZEROS_AND_LEN */
#if defined(MBEDTLS_CIPHER_PADDING_ZEROS)
    "CIPHER_PADDING_ZEROS", //no-check-names
#endif /* MBEDTLS_CIPHER_PADDING_ZEROS */
#if defined(MBEDTLS_CTR_DRBG_USE_128_BIT_KEY)
    "CTR_DRBG_USE_128_BIT_KEY", //no-check-names
#endif /* MBEDTLS_CTR_DRBG_USE_128_BIT_KEY */
#if defined(MBEDTLS_ECDH_VARIANT_EVEREST_ENABLED)
    "ECDH_VARIANT_EVEREST_ENABLED", //no-check-names
#endif /* MBEDTLS_ECDH_VARIANT_EVEREST_ENABLED */
#if defined(MBEDTLS_ECP_DP_SECP192R1_ENABLED)
    "ECP_DP_SECP192R1_ENABLED", //no-check-names
#endif /* MBEDTLS_ECP_DP_SECP192R1_ENABLED */
#if defined(MBEDTLS_ECP_DP_SECP224R1_ENABLED)
    "ECP_DP_SECP224R1_ENABLED", //no-check-names
#endif /* MBEDTLS_ECP_DP_SECP224R1_ENABLED */
#if defined(MBEDTLS_ECP_DP_SECP256R1_ENABLED)
    "ECP_DP_SECP256R1_ENABLED", //no-check-names
#endif /* MBEDTLS_ECP_DP_SECP256R1_ENABLED */
#if defined(MBEDTLS_ECP_DP_SECP384R1_ENABLED)
    "ECP_DP_SECP384R1_ENABLED", //no-check-names
#endif /* MBEDTLS_ECP_DP_SECP384R1_ENABLED */
#if defined(MBEDTLS_ECP_DP_SECP521R1_ENABLED)
    "ECP_DP_SECP521R1_ENABLED", //no-check-names
#endif /* MBEDTLS_ECP_DP_SECP521R1_ENABLED */
#if defined(MBEDTLS_ECP_DP_SECP192K1_ENABLED)
    "ECP_DP_SECP192K1_ENABLED", //no-check-names
#endif /* MBEDTLS_ECP_DP_SECP192K1_ENABLED */
#if defined(MBEDTLS_ECP_DP_SECP224K1_ENABLED)
    "ECP_DP_SECP224K1_ENABLED", //no-check-names
#endif /* MBEDTLS_ECP_DP_SECP224K1_ENABLED */
#if defined(MBEDTLS_ECP_DP_SECP256K1_ENABLED)
    "ECP_DP_SECP256K1_ENABLED", //no-check-names
#endif /* MBEDTLS_ECP_DP_SECP256K1_ENABLED */
#if defined(MBEDTLS_ECP_DP_BP256R1_ENABLED)
    "ECP_DP_BP256R1_ENABLED", //no-check-names
#endif /* MBEDTLS_ECP_DP_BP256R1_ENABLED */
#if defined(MBEDTLS_ECP_DP_BP384R1_ENABLED)
    "ECP_DP_BP384R1_ENABLED", //no-check-names
#endif /* MBEDTLS_ECP_DP_BP384R1_ENABLED */
#if defined(MBEDTLS_ECP_DP_BP512R1_ENABLED)
    "ECP_DP_BP512R1_ENABLED", //no-check-names
#endif /* MBEDTLS_ECP_DP_BP512R1_ENABLED */
#if defined(MBEDTLS_ECP_DP_CURVE25519_ENABLED)
    "ECP_DP_CURVE25519_ENABLED", //no-check-names
#endif /* MBEDTLS_ECP_DP_CURVE25519_ENABLED */
#if defined(MBEDTLS_ECP_DP_CURVE448_ENABLED)
    "ECP_DP_CURVE448_ENABLED", //no-check-names
#endif /* MBEDTLS_ECP_DP_CURVE448_ENABLED */
#if defined(MBEDTLS_ECP_NIST_OPTIM)
    "ECP_NIST_OPTIM", //no-check-names
#endif /* MBEDTLS_ECP_NIST_OPTIM */
#if defined(MBEDTLS_ECP_RESTARTABLE)
    "ECP_RESTARTABLE", //no-check-names
#endif /* MBEDTLS_ECP_RESTARTABLE */
#if defined(MBEDTLS_ECP_WITH_MPI_UINT)
    "ECP_WITH_MPI_UINT", //no-check-names
#endif /* MBEDTLS_ECP_WITH_MPI_UINT */
#if defined(MBEDTLS_ECDSA_DETERMINISTIC)
    "ECDSA_DETERMINISTIC", //no-check-names
#endif /* MBEDTLS_ECDSA_DETERMINISTIC */
#if defined(MBEDTLS_KEY_EXCHANGE_PSK_ENABLED)
    "KEY_EXCHANGE_PSK_ENABLED", //no-check-names
#endif /* MBEDTLS_KEY_EXCHANGE_PSK_ENABLED */
#if defined(MBEDTLS_KEY_EXCHANGE_DHE_PSK_ENABLED)
    "KEY_EXCHANGE_DHE_PSK_ENABLED", //no-check-names
#endif /* MBEDTLS_KEY_EXCHANGE_DHE_PSK_ENABLED */
#if defined(MBEDTLS_KEY_EXCHANGE_ECDHE_PSK_ENABLED)
    "KEY_EXCHANGE_ECDHE_PSK_ENABLED", //no-check-names
#endif /* MBEDTLS_KEY_EXCHANGE_ECDHE_PSK_ENABLED */
#if defined(MBEDTLS_KEY_EXCHANGE_RSA_PSK_ENABLED)
    "KEY_EXCHANGE_RSA_PSK_ENABLED", //no-check-names
#endif /* MBEDTLS_KEY_EXCHANGE_RSA_PSK_ENABLED */
#if defined(MBEDTLS_KEY_EXCHANGE_RSA_ENABLED)
    "KEY_EXCHANGE_RSA_ENABLED", //no-check-names
#endif /* MBEDTLS_KEY_EXCHANGE_RSA_ENABLED */
#if defined(MBEDTLS_KEY_EXCHANGE_DHE_RSA_ENABLED)
    "KEY_EXCHANGE_DHE_RSA_ENABLED", //no-check-names
#endif /* MBEDTLS_KEY_EXCHANGE_DHE_RSA_ENABLED */
#if defined(MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED)
    "KEY_EXCHANGE_ECDHE_RSA_ENABLED", //no-check-names
#endif /* MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED */
#if defined(MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED)
    "KEY_EXCHANGE_ECDHE_ECDSA_ENABLED", //no-check-names
#endif /* MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED */
#if defined(MBEDTLS_KEY_EXCHANGE_ECDH_ECDSA_ENABLED)
    "KEY_EXCHANGE_ECDH_ECDSA_ENABLED", //no-check-names
#endif /* MBEDTLS_KEY_EXCHANGE_ECDH_ECDSA_ENABLED */
#if defined(MBEDTLS_KEY_EXCHANGE_ECDH_RSA_ENABLED)
    "KEY_EXCHANGE_ECDH_RSA_ENABLED", //no-check-names
#endif /* MBEDTLS_KEY_EXCHANGE_ECDH_RSA_ENABLED */
#if defined(MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED)
    "KEY_EXCHANGE_ECJPAKE_ENABLED", //no-check-names
#endif /* MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED */
#if defined(MBEDTLS_PK_PARSE_EC_EXTENDED)
    "PK_PARSE_EC_EXTENDED", //no-check-names
#endif /* MBEDTLS_PK_PARSE_EC_EXTENDED */
#if defined(MBEDTLS_PK_PARSE_EC_COMPRESSED)
    "PK_PARSE_EC_COMPRESSED", //no-check-names
#endif /* MBEDTLS_PK_PARSE_EC_COMPRESSED */
#if defined(MBEDTLS_ERROR_STRERROR_DUMMY)
    "ERROR_STRERROR_DUMMY", //no-check-names
#endif /* MBEDTLS_ERROR_STRERROR_DUMMY */
#if defined(MBEDTLS_GENPRIME)
    "GENPRIME", //no-check-names
#endif /* MBEDTLS_GENPRIME */
#if defined(MBEDTLS_FS_IO)
    "FS_IO", //no-check-names
#endif /* MBEDTLS_FS_IO */
#if defined(MBEDTLS_NO_DEFAULT_ENTROPY_SOURCES)
    "NO_DEFAULT_ENTROPY_SOURCES", //no-check-names
#endif /* MBEDTLS_NO_DEFAULT_ENTROPY_SOURCES */
#if defined(MBEDTLS_NO_PLATFORM_ENTROPY)
    "NO_PLATFORM_ENTROPY", //no-check-names
#endif /* MBEDTLS_NO_PLATFORM_ENTROPY */
#if defined(MBEDTLS_ENTROPY_FORCE_SHA256)
    "ENTROPY_FORCE_SHA256", //no-check-names
#endif /* MBEDTLS_ENTROPY_FORCE_SHA256 */
#if defined(MBEDTLS_ENTROPY_NV_SEED)
    "ENTROPY_NV_SEED", //no-check-names
#endif /* MBEDTLS_ENTROPY_NV_SEED */
#if defined(MBEDTLS_PSA_CRYPTO_KEY_ID_ENCODES_OWNER)
    "PSA_CRYPTO_KEY_ID_ENCODES_OWNER", //no-check-names
#endif /* MBEDTLS_PSA_CRYPTO_KEY_ID_ENCODES_OWNER */
#if defined(MBEDTLS_MEMORY_DEBUG)
    "MEMORY_DEBUG", //no-check-names
#endif /* MBEDTLS_MEMORY_DEBUG */
#if defined(MBEDTLS_MEMORY_BACKTRACE)
    "MEMORY_BACKTRACE", //no-check-names
#endif /* MBEDTLS_MEMORY_BACKTRACE */
#if defined(MBEDTLS_PK_RSA_ALT_SUPPORT)
    "PK_RSA_ALT_SUPPORT", //no-check-names
#endif /* MBEDTLS_PK_RSA_ALT_SUPPORT */
#if defined(MBEDTLS_PKCS1_V15)
    "PKCS1_V15", //no-check-names
#endif /* MBEDTLS_PKCS1_V15 */
#if defined(MBEDTLS_PKCS1_V21)
    "PKCS1_V21", //no-check-names
#endif /* MBEDTLS_PKCS1_V21 */
#if defined(MBEDTLS_PSA_CRYPTO_BUILTIN_KEYS)
    "PSA_CRYPTO_BUILTIN_KEYS", //no-check-names
#endif /* MBEDTLS_PSA_CRYPTO_BUILTIN_KEYS */
#if defined(MBEDTLS_PSA_CRYPTO_CLIENT)
    "PSA_CRYPTO_CLIENT", //no-check-names
#endif /* MBEDTLS_PSA_CRYPTO_CLIENT */
#if defined(MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG)
    "PSA_CRYPTO_EXTERNAL_RNG", //no-check-names
#endif /* MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG */
#if defined(MBEDTLS_PSA_CRYPTO_SPM)
    "PSA_CRYPTO_SPM", //no-check-names
#endif /* MBEDTLS_PSA_CRYPTO_SPM */
#if defined(MBEDTLS_PSA_KEY_STORE_DYNAMIC)
    "PSA_KEY_STORE_DYNAMIC", //no-check-names
#endif /* MBEDTLS_PSA_KEY_STORE_DYNAMIC */
#if defined(MBEDTLS_PSA_P256M_DRIVER_ENABLED)
    "PSA_P256M_DRIVER_ENABLED", //no-check-names
#endif /* MBEDTLS_PSA_P256M_DRIVER_ENABLED */
#if defined(MBEDTLS_PSA_INJECT_ENTROPY)
    "PSA_INJECT_ENTROPY", //no-check-names
#endif /* MBEDTLS_PSA_INJECT_ENTROPY */
#if defined(MBEDTLS_PSA_ASSUME_EXCLUSIVE_BUFFERS)
    "PSA_ASSUME_EXCLUSIVE_BUFFERS", //no-check-names
#endif /* MBEDTLS_PSA_ASSUME_EXCLUSIVE_BUFFERS */
#if defined(MBEDTLS_RSA_NO_CRT)
    "RSA_NO_CRT", //no-check-names
#endif /* MBEDTLS_RSA_NO_CRT */
#if defined(MBEDTLS_SELF_TEST)
    "SELF_TEST", //no-check-names
#endif /* MBEDTLS_SELF_TEST */
#if defined(MBEDTLS_SHA256_SMALLER)
    "SHA256_SMALLER", //no-check-names
#endif /* MBEDTLS_SHA256_SMALLER */
#if defined(MBEDTLS_SHA512_SMALLER)
    "SHA512_SMALLER", //no-check-names
#endif /* MBEDTLS_SHA512_SMALLER */
#if defined(MBEDTLS_SSL_ALL_ALERT_MESSAGES)
    "SSL_ALL_ALERT_MESSAGES", //no-check-names
#endif /* MBEDTLS_SSL_ALL_ALERT_MESSAGES */
#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
    "SSL_DTLS_CONNECTION_ID", //no-check-names
#endif /* MBEDTLS_SSL_DTLS_CONNECTION_ID */
#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID_COMPAT)
    "SSL_DTLS_CONNECTION_ID_COMPAT", //no-check-names
#endif /* MBEDTLS_SSL_DTLS_CONNECTION_ID_COMPAT */
#if defined(MBEDTLS_SSL_ASYNC_PRIVATE)
    "SSL_ASYNC_PRIVATE", //no-check-names
#endif /* MBEDTLS_SSL_ASYNC_PRIVATE */
#if defined(MBEDTLS_SSL_CLI_ALLOW_WEAK_CERTIFICATE_VERIFICATION_WITHOUT_HOSTNAME)
    "SSL_CLI_ALLOW_WEAK_CERTIFICATE_VERIFICATION_WITHOUT_HOSTNAME", //no-check-names
#endif /* MBEDTLS_SSL_CLI_ALLOW_WEAK_CERTIFICATE_VERIFICATION_WITHOUT_HOSTNAME */
#if defined(MBEDTLS_SSL_CONTEXT_SERIALIZATION)
    "SSL_CONTEXT_SERIALIZATION", //no-check-names
#endif /* MBEDTLS_SSL_CONTEXT_SERIALIZATION */
#if defined(MBEDTLS_SSL_DEBUG_ALL)
    "SSL_DEBUG_ALL", //no-check-names
#endif /* MBEDTLS_SSL_DEBUG_ALL */
#if defined(MBEDTLS_SSL_ENCRYPT_THEN_MAC)
    "SSL_ENCRYPT_THEN_MAC", //no-check-names
#endif /* MBEDTLS_SSL_ENCRYPT_THEN_MAC */
#if defined(MBEDTLS_SSL_EXTENDED_MASTER_SECRET)
    "SSL_EXTENDED_MASTER_SECRET", //no-check-names
#endif /* MBEDTLS_SSL_EXTENDED_MASTER_SECRET */
#if defined(MBEDTLS_SSL_KEEP_PEER_CERTIFICATE)
    "SSL_KEEP_PEER_CERTIFICATE", //no-check-names
#endif /* MBEDTLS_SSL_KEEP_PEER_CERTIFICATE */
#if defined(MBEDTLS_SSL_KEYING_MATERIAL_EXPORT)
    "SSL_KEYING_MATERIAL_EXPORT", //no-check-names
#endif /* MBEDTLS_SSL_KEYING_MATERIAL_EXPORT */
#if defined(MBEDTLS_SSL_RENEGOTIATION)
    "SSL_RENEGOTIATION", //no-check-names
#endif /* MBEDTLS_SSL_RENEGOTIATION */
#if defined(MBEDTLS_SSL_MAX_FRAGMENT_LENGTH)
    "SSL_MAX_FRAGMENT_LENGTH", //no-check-names
#endif /* MBEDTLS_SSL_MAX_FRAGMENT_LENGTH */
#if defined(MBEDTLS_SSL_RECORD_SIZE_LIMIT)
    "SSL_RECORD_SIZE_LIMIT", //no-check-names
#endif /* MBEDTLS_SSL_RECORD_SIZE_LIMIT */
#if defined(MBEDTLS_SSL_PROTO_TLS1_2)
    "SSL_PROTO_TLS1_2", //no-check-names
#endif /* MBEDTLS_SSL_PROTO_TLS1_2 */
#if defined(MBEDTLS_SSL_PROTO_TLS1_3)
    "SSL_PROTO_TLS1_3", //no-check-names
#endif /* MBEDTLS_SSL_PROTO_TLS1_3 */
#if defined(MBEDTLS_SSL_TLS1_3_COMPATIBILITY_MODE)
    "SSL_TLS1_3_COMPATIBILITY_MODE", //no-check-names
#endif /* MBEDTLS_SSL_TLS1_3_COMPATIBILITY_MODE */
#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_ENABLED)
    "SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_ENABLED", //no-check-names
#endif /* MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_ENABLED */
#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED)
    "SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED", //no-check-names
#endif /* MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED */
#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_EPHEMERAL_ENABLED)
    "SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_EPHEMERAL_ENABLED", //no-check-names
#endif /* MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_EPHEMERAL_ENABLED */
#if defined(MBEDTLS_SSL_EARLY_DATA)
    "SSL_EARLY_DATA", //no-check-names
#endif /* MBEDTLS_SSL_EARLY_DATA */
#if defined(MBEDTLS_SSL_PROTO_DTLS)
    "SSL_PROTO_DTLS", //no-check-names
#endif /* MBEDTLS_SSL_PROTO_DTLS */
#if defined(MBEDTLS_SSL_ALPN)
    "SSL_ALPN", //no-check-names
#endif /* MBEDTLS_SSL_ALPN */
#if defined(MBEDTLS_SSL_DTLS_ANTI_REPLAY)
    "SSL_DTLS_ANTI_REPLAY", //no-check-names
#endif /* MBEDTLS_SSL_DTLS_ANTI_REPLAY */
#if defined(MBEDTLS_SSL_DTLS_HELLO_VERIFY)
    "SSL_DTLS_HELLO_VERIFY", //no-check-names
#endif /* MBEDTLS_SSL_DTLS_HELLO_VERIFY */
#if defined(MBEDTLS_SSL_DTLS_SRTP)
    "SSL_DTLS_SRTP", //no-check-names
#endif /* MBEDTLS_SSL_DTLS_SRTP */
#if defined(MBEDTLS_SSL_DTLS_CLIENT_PORT_REUSE)
    "SSL_DTLS_CLIENT_PORT_REUSE", //no-check-names
#endif /* MBEDTLS_SSL_DTLS_CLIENT_PORT_REUSE */
#if defined(MBEDTLS_SSL_SESSION_TICKETS)
    "SSL_SESSION_TICKETS", //no-check-names
#endif /* MBEDTLS_SSL_SESSION_TICKETS */
#if defined(MBEDTLS_SSL_SERVER_NAME_INDICATION)
    "SSL_SERVER_NAME_INDICATION", //no-check-names
#endif /* MBEDTLS_SSL_SERVER_NAME_INDICATION */
#if defined(MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH)
    "SSL_VARIABLE_BUFFER_LENGTH", //no-check-names
#endif /* MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH */
#if defined(MBEDTLS_TEST_CONSTANT_FLOW_MEMSAN)
    "TEST_CONSTANT_FLOW_MEMSAN", //no-check-names
#endif /* MBEDTLS_TEST_CONSTANT_FLOW_MEMSAN */
#if defined(MBEDTLS_TEST_CONSTANT_FLOW_VALGRIND)
    "TEST_CONSTANT_FLOW_VALGRIND", //no-check-names
#endif /* MBEDTLS_TEST_CONSTANT_FLOW_VALGRIND */
#if defined(MBEDTLS_TEST_HOOKS)
    "TEST_HOOKS", //no-check-names
#endif /* MBEDTLS_TEST_HOOKS */
#if defined(MBEDTLS_THREADING_ALT)
    "THREADING_ALT", //no-check-names
#endif /* MBEDTLS_THREADING_ALT */
#if defined(MBEDTLS_THREADING_PTHREAD)
    "THREADING_PTHREAD", //no-check-names
#endif /* MBEDTLS_THREADING_PTHREAD */
#if defined(MBEDTLS_USE_PSA_CRYPTO)
    "USE_PSA_CRYPTO", //no-check-names
#endif /* MBEDTLS_USE_PSA_CRYPTO */
#if defined(MBEDTLS_PSA_CRYPTO_CONFIG)
    "PSA_CRYPTO_CONFIG", //no-check-names
#endif /* MBEDTLS_PSA_CRYPTO_CONFIG */
#if defined(MBEDTLS_VERSION_FEATURES)
    "VERSION_FEATURES", //no-check-names
#endif /* MBEDTLS_VERSION_FEATURES */
#if defined(MBEDTLS_X509_TRUSTED_CERTIFICATE_CALLBACK)
    "X509_TRUSTED_CERTIFICATE_CALLBACK", //no-check-names
#endif /* MBEDTLS_X509_TRUSTED_CERTIFICATE_CALLBACK */
#if defined(MBEDTLS_X509_REMOVE_INFO)
    "X509_REMOVE_INFO", //no-check-names
#endif /* MBEDTLS_X509_REMOVE_INFO */
#if defined(MBEDTLS_X509_RSASSA_PSS_SUPPORT)
    "X509_RSASSA_PSS_SUPPORT", //no-check-names
#endif /* MBEDTLS_X509_RSASSA_PSS_SUPPORT */
#if defined(MBEDTLS_AESNI_C)
    "AESNI_C", //no-check-names
#endif /* MBEDTLS_AESNI_C */
#if defined(MBEDTLS_AESCE_C)
    "AESCE_C", //no-check-names
#endif /* MBEDTLS_AESCE_C */
#if defined(MBEDTLS_AES_C)
    "AES_C", //no-check-names
#endif /* MBEDTLS_AES_C */
#if defined(MBEDTLS_ASN1_PARSE_C)
    "ASN1_PARSE_C", //no-check-names
#endif /* MBEDTLS_ASN1_PARSE_C */
#if defined(MBEDTLS_ASN1_WRITE_C)
    "ASN1_WRITE_C", //no-check-names
#endif /* MBEDTLS_ASN1_WRITE_C */
#if defined(MBEDTLS_BASE64_C)
    "BASE64_C", //no-check-names
#endif /* MBEDTLS_BASE64_C */
#if defined(MBEDTLS_BLOCK_CIPHER_NO_DECRYPT)
    "BLOCK_CIPHER_NO_DECRYPT", //no-check-names
#endif /* MBEDTLS_BLOCK_CIPHER_NO_DECRYPT */
#if defined(MBEDTLS_BIGNUM_C)
    "BIGNUM_C", //no-check-names
#endif /* MBEDTLS_BIGNUM_C */
#if defined(MBEDTLS_CAMELLIA_C)
    "CAMELLIA_C", //no-check-names
#endif /* MBEDTLS_CAMELLIA_C */
#if defined(MBEDTLS_ARIA_C)
    "ARIA_C", //no-check-names
#endif /* MBEDTLS_ARIA_C */
#if defined(MBEDTLS_CCM_C)
    "CCM_C", //no-check-names
#endif /* MBEDTLS_CCM_C */
#if defined(MBEDTLS_CHACHA20_C)
    "CHACHA20_C", //no-check-names
#endif /* MBEDTLS_CHACHA20_C */
#if defined(MBEDTLS_CHACHAPOLY_C)
    "CHACHAPOLY_C", //no-check-names
#endif /* MBEDTLS_CHACHAPOLY_C */
#if defined(MBEDTLS_CIPHER_C)
    "CIPHER_C", //no-check-names
#endif /* MBEDTLS_CIPHER_C */
#if defined(MBEDTLS_CMAC_C)
    "CMAC_C", //no-check-names
#endif /* MBEDTLS_CMAC_C */
#if defined(MBEDTLS_CTR_DRBG_C)
    "CTR_DRBG_C", //no-check-names
#endif /* MBEDTLS_CTR_DRBG_C */
#if defined(MBEDTLS_DEBUG_C)
    "DEBUG_C", //no-check-names
#endif /* MBEDTLS_DEBUG_C */
#if defined(MBEDTLS_DES_C)
    "DES_C", //no-check-names
#endif /* MBEDTLS_DES_C */
#if defined(MBEDTLS_DHM_C)
    "DHM_C", //no-check-names
#endif /* MBEDTLS_DHM_C */
#if defined(MBEDTLS_ECDH_C)
    "ECDH_C", //no-check-names
#endif /* MBEDTLS_ECDH_C */
#if defined(MBEDTLS_ECDSA_C)
    "ECDSA_C", //no-check-names
#endif /* MBEDTLS_ECDSA_C */
#if defined(MBEDTLS_ECJPAKE_C)
    "ECJPAKE_C", //no-check-names
#endif /* MBEDTLS_ECJPAKE_C */
#if defined(MBEDTLS_ECP_C)
    "ECP_C", //no-check-names
#endif /* MBEDTLS_ECP_C */
#if defined(MBEDTLS_ENTROPY_C)
    "ENTROPY_C", //no-check-names
#endif /* MBEDTLS_ENTROPY_C */
#if defined(MBEDTLS_ERROR_C)
    "ERROR_C", //no-check-names
#endif /* MBEDTLS_ERROR_C */
#if defined(MBEDTLS_GCM_C)
    "GCM_C", //no-check-names
#endif /* MBEDTLS_GCM_C */
#if defined(MBEDTLS_GCM_LARGE_TABLE)
    "GCM_LARGE_TABLE", //no-check-names
#endif /* MBEDTLS_GCM_LARGE_TABLE */
#if defined(MBEDTLS_HKDF_C)
    "HKDF_C", //no-check-names
#endif /* MBEDTLS_HKDF_C */
#if defined(MBEDTLS_HMAC_DRBG_C)
    "HMAC_DRBG_C", //no-check-names
#endif /* MBEDTLS_HMAC_DRBG_C */
#if defined(MBEDTLS_LMS_C)
    "LMS_C", //no-check-names
#endif /* MBEDTLS_LMS_C */
#if defined(MBEDTLS_LMS_PRIVATE)
    "LMS_PRIVATE", //no-check-names
#endif /* MBEDTLS_LMS_PRIVATE */
#if defined(MBEDTLS_NIST_KW_C)
    "NIST_KW_C", //no-check-names
#endif /* MBEDTLS_NIST_KW_C */
#if defined(MBEDTLS_MD_C)
    "MD_C", //no-check-names
#endif /* MBEDTLS_MD_C */
#if defined(MBEDTLS_MD5_C)
    "MD5_C", //no-check-names
#endif /* MBEDTLS_MD5_C */
#if defined(MBEDTLS_MEMORY_BUFFER_ALLOC_C)
    "MEMORY_BUFFER_ALLOC_C", //no-check-names
#endif /* MBEDTLS_MEMORY_BUFFER_ALLOC_C */
#if defined(MBEDTLS_NET_C)
    "NET_C", //no-check-names
#endif /* MBEDTLS_NET_C */
#if defined(MBEDTLS_OID_C)
    "OID_C", //no-check-names
#endif /* MBEDTLS_OID_C */
#if defined(MBEDTLS_PADLOCK_C)
    "PADLOCK_C", //no-check-names
#endif /* MBEDTLS_PADLOCK_C */
#if defined(MBEDTLS_PEM_PARSE_C)
    "PEM_PARSE_C", //no-check-names
#endif /* MBEDTLS_PEM_PARSE_C */
#if defined(MBEDTLS_PEM_WRITE_C)
    "PEM_WRITE_C", //no-check-names
#endif /* MBEDTLS_PEM_WRITE_C */
#if defined(MBEDTLS_PK_C)
    "PK_C", //no-check-names
#endif /* MBEDTLS_PK_C */
#if defined(MBEDTLS_PK_PARSE_C)
    "PK_PARSE_C", //no-check-names
#endif /* MBEDTLS_PK_PARSE_C */
#if defined(MBEDTLS_PK_WRITE_C)
    "PK_WRITE_C", //no-check-names
#endif /* MBEDTLS_PK_WRITE_C */
#if defined(MBEDTLS_PKCS5_C)
    "PKCS5_C", //no-check-names
#endif /* MBEDTLS_PKCS5_C */
#if defined(MBEDTLS_PKCS7_C)
    "PKCS7_C", //no-check-names
#endif /* MBEDTLS_PKCS7_C */
#if defined(MBEDTLS_PKCS12_C)
    "PKCS12_C", //no-check-names
#endif /* MBEDTLS_PKCS12_C */
#if defined(MBEDTLS_PLATFORM_C)
    "PLATFORM_C", //no-check-names
#endif /* MBEDTLS_PLATFORM_C */
#if defined(MBEDTLS_POLY1305_C)
    "POLY1305_C", //no-check-names
#endif /* MBEDTLS_POLY1305_C */
#if defined(MBEDTLS_PSA_CRYPTO_C)
    "PSA_CRYPTO_C", //no-check-names
#endif /* MBEDTLS_PSA_CRYPTO_C */
#if defined(MBEDTLS_PSA_CRYPTO_SE_C)
    "PSA_CRYPTO_SE_C", //no-check-names
#endif /* MBEDTLS_PSA_CRYPTO_SE_C */
#if defined(MBEDTLS_PSA_CRYPTO_STORAGE_C)
    "PSA_CRYPTO_STORAGE_C", //no-check-names
#endif /* MBEDTLS_PSA_CRYPTO_STORAGE_C */
#if defined(MBEDTLS_PSA_ITS_FILE_C)
    "PSA_ITS_FILE_C", //no-check-names
#endif /* MBEDTLS_PSA_ITS_FILE_C */
#if defined(MBEDTLS_PSA_STATIC_KEY_SLOTS)
    "PSA_STATIC_KEY_SLOTS", //no-check-names
#endif /* MBEDTLS_PSA_STATIC_KEY_SLOTS */
#if defined(MBEDTLS_RIPEMD160_C)
    "RIPEMD160_C", //no-check-names
#endif /* MBEDTLS_RIPEMD160_C */
#if defined(MBEDTLS_RSA_C)
    "RSA_C", //no-check-names
#endif /* MBEDTLS_RSA_C */
#if defined(MBEDTLS_SHA1_C)
    "SHA1_C", //no-check-names
#endif /* MBEDTLS_SHA1_C */
#if defined(MBEDTLS_SHA224_C)
    "SHA224_C", //no-check-names
#endif /* MBEDTLS_SHA224_C */
#if defined(MBEDTLS_SHA256_C)
    "SHA256_C", //no-check-names
#endif /* MBEDTLS_SHA256_C */
#if defined(MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_IF_PRESENT)
    "SHA256_USE_ARMV8_A_CRYPTO_IF_PRESENT", //no-check-names
#endif /* MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_IF_PRESENT */
#if defined(MBEDTLS_SHA256_USE_A64_CRYPTO_IF_PRESENT)
    "SHA256_USE_A64_CRYPTO_IF_PRESENT", //no-check-names
#endif /* MBEDTLS_SHA256_USE_A64_CRYPTO_IF_PRESENT */
#if defined(MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_ONLY)
    "SHA256_USE_ARMV8_A_CRYPTO_ONLY", //no-check-names
#endif /* MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_ONLY */
#if defined(MBEDTLS_SHA256_USE_A64_CRYPTO_ONLY)
    "SHA256_USE_A64_CRYPTO_ONLY", //no-check-names
#endif /* MBEDTLS_SHA256_USE_A64_CRYPTO_ONLY */
#if defined(MBEDTLS_SHA384_C)
    "SHA384_C", //no-check-names
#endif /* MBEDTLS_SHA384_C */
#if defined(MBEDTLS_SHA512_C)
    "SHA512_C", //no-check-names
#endif /* MBEDTLS_SHA512_C */
#if defined(MBEDTLS_SHA3_C)
    "SHA3_C", //no-check-names
#endif /* MBEDTLS_SHA3_C */
#if defined(MBEDTLS_SHA512_USE_A64_CRYPTO_IF_PRESENT)
    "SHA512_USE_A64_CRYPTO_IF_PRESENT", //no-check-names
#endif /* MBEDTLS_SHA512_USE_A64_CRYPTO_IF_PRESENT */
#if defined(MBEDTLS_SHA512_USE_A64_CRYPTO_ONLY)
    "SHA512_USE_A64_CRYPTO_ONLY", //no-check-names
#endif /* MBEDTLS_SHA512_USE_A64_CRYPTO_ONLY */
#if defined(MBEDTLS_SSL_CACHE_C)
    "SSL_CACHE_C", //no-check-names
#endif /* MBEDTLS_SSL_CACHE_C */
#if defined(MBEDTLS_SSL_COOKIE_C)
    "SSL_COOKIE_C", //no-check-names
#endif /* MBEDTLS_SSL_COOKIE_C */
#if defined(MBEDTLS_SSL_TICKET_C)
    "SSL_TICKET_C", //no-check-names
#endif /* MBEDTLS_SSL_TICKET_C */
#if defined(MBEDTLS_SSL_CLI_C)
    "SSL_CLI_C", //no-check-names
#endif /* MBEDTLS_SSL_CLI_C */
#if defined(MBEDTLS_SSL_SRV_C)
    "SSL_SRV_C", //no-check-names
#endif /* MBEDTLS_SSL_SRV_C */
#if defined(MBEDTLS_SSL_TLS_C)
    "SSL_TLS_C", //no-check-names
#endif /* MBEDTLS_SSL_TLS_C */
#if defined(MBEDTLS_THREADING_C)
    "THREADING_C", //no-check-names
#endif /* MBEDTLS_THREADING_C */
#if defined(MBEDTLS_TIMING_C)
    "TIMING_C", //no-check-names
#endif /* MBEDTLS_TIMING_C */
#if defined(MBEDTLS_VERSION_C)
    "VERSION_C", //no-check-names
#endif /* MBEDTLS_VERSION_C */
#if defined(MBEDTLS_X509_USE_C)
    "X509_USE_C", //no-check-names
#endif /* MBEDTLS_X509_USE_C */
#if defined(MBEDTLS_X509_CRT_PARSE_C)
    "X509_CRT_PARSE_C", //no-check-names
#endif /* MBEDTLS_X509_CRT_PARSE_C */
#if defined(MBEDTLS_X509_CRL_PARSE_C)
    "X509_CRL_PARSE_C", //no-check-names
#endif /* MBEDTLS_X509_CRL_PARSE_C */
#if defined(MBEDTLS_X509_CSR_PARSE_C)
    "X509_CSR_PARSE_C", //no-check-names
#endif /* MBEDTLS_X509_CSR_PARSE_C */
#if defined(MBEDTLS_X509_CREATE_C)
    "X509_CREATE_C", //no-check-names
#endif /* MBEDTLS_X509_CREATE_C */
#if defined(MBEDTLS_X509_CRT_WRITE_C)
    "X509_CRT_WRITE_C", //no-check-names
#endif /* MBEDTLS_X509_CRT_WRITE_C */
#if defined(MBEDTLS_X509_CSR_WRITE_C)
    "X509_CSR_WRITE_C", //no-check-names
#endif /* MBEDTLS_X509_CSR_WRITE_C */
#endif /* MBEDTLS_VERSION_FEATURES */
    NULL
};

int mbedtls_version_check_feature(const char *feature)
{
    const char * const *idx = features;

    if (*idx == NULL) {
        return -2;
    }

    if (feature == NULL) {
        return -1;
    }

    if (strncmp(feature, "MBEDTLS_", 8)) {
        return -1;
    }

    feature += 8;

    while (*idx != NULL) {
        if (!strcmp(*idx, feature)) {
            return 0;
        }
        idx++;
    }
    return -1;
}

#endif /* MBEDTLS_VERSION_C */
