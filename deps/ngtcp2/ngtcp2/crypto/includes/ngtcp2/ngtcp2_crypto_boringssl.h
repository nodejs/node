/*
 * ngtcp2
 *
 * Copyright (c) 2020 ngtcp2 contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef NGTCP2_CRYPTO_BORINGSSL_H
#define NGTCP2_CRYPTO_BORINGSSL_H

#include <ngtcp2/ngtcp2.h>

#include <openssl/ssl.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @function
 *
 * `ngtcp2_crypto_boringssl_from_ssl_encryption_level` translates
 * |ssl_level| to :type:`ngtcp2_crypto_level`.  This function is only
 * available for BoringSSL backend.
 */
NGTCP2_EXTERN ngtcp2_crypto_level
ngtcp2_crypto_boringssl_from_ssl_encryption_level(
    enum ssl_encryption_level_t ssl_level);

/**
 * @function
 *
 * `ngtcp2_crypto_boringssl_from_ngtcp2_crypto_level` translates
 * |crypto_level| to ssl_encryption_level_t.  This function is only
 * available for BoringSSL backend.
 */
NGTCP2_EXTERN enum ssl_encryption_level_t
ngtcp2_crypto_boringssl_from_ngtcp2_crypto_level(
    ngtcp2_crypto_level crypto_level);

#ifdef __cplusplus
}
#endif

#endif /* NGTCP2_CRYPTO_BORINGSSL_H */
