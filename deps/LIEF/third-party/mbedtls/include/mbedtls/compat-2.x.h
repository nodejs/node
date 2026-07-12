/**
 * \file compat-2.x.h
 *
 * \brief Compatibility definitions
 *
 * \deprecated Use the new names directly instead
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#if defined(MBEDTLS_DEPRECATED_WARNING)
#warning "Including compat-2.x.h is deprecated"
#endif

#ifndef MBEDTLS_COMPAT2X_H
#define MBEDTLS_COMPAT2X_H

/*
 * Macros for renamed functions
 */
#define mbedtls_ctr_drbg_update_ret   mbedtls_ctr_drbg_update
#define mbedtls_hmac_drbg_update_ret  mbedtls_hmac_drbg_update
#define mbedtls_md5_starts_ret        mbedtls_md5_starts
#define mbedtls_md5_update_ret        mbedtls_md5_update
#define mbedtls_md5_finish_ret        mbedtls_md5_finish
#define mbedtls_md5_ret               mbedtls_md5
#define mbedtls_ripemd160_starts_ret  mbedtls_ripemd160_starts
#define mbedtls_ripemd160_update_ret  mbedtls_ripemd160_update
#define mbedtls_ripemd160_finish_ret  mbedtls_ripemd160_finish
#define mbedtls_ripemd160_ret         mbedtls_ripemd160
#define mbedtls_sha1_starts_ret       mbedtls_sha1_starts
#define mbedtls_sha1_update_ret       mbedtls_sha1_update
#define mbedtls_sha1_finish_ret       mbedtls_sha1_finish
#define mbedtls_sha1_ret              mbedtls_sha1
#define mbedtls_sha256_starts_ret     mbedtls_sha256_starts
#define mbedtls_sha256_update_ret     mbedtls_sha256_update
#define mbedtls_sha256_finish_ret     mbedtls_sha256_finish
#define mbedtls_sha256_ret            mbedtls_sha256
#define mbedtls_sha512_starts_ret     mbedtls_sha512_starts
#define mbedtls_sha512_update_ret     mbedtls_sha512_update
#define mbedtls_sha512_finish_ret     mbedtls_sha512_finish
#define mbedtls_sha512_ret            mbedtls_sha512

#endif /* MBEDTLS_COMPAT2X_H */
