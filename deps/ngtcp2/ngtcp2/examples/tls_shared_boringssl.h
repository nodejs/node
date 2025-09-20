/*
 * ngtcp2
 *
 * Copyright (c) 2024 ngtcp2 contributors
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
#ifndef TLS_SHARED_BORINGSSL_H
#define TLS_SHARED_BORINGSSL_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif // defined(HAVE_CONFIG_H)

#include <openssl/ssl.h>

namespace ngtcp2 {

namespace tls {

constexpr uint16_t CERTIFICATE_COMPRESSION_ALGO_BROTLI = 2;

#ifdef HAVE_LIBBROTLI
int cert_compress(SSL *ssl, CBB *out, const uint8_t *in, size_t in_len);

int cert_decompress(SSL *ssl, CRYPTO_BUFFER **out, size_t uncompressed_len,
                    const uint8_t *in, size_t in_len);
#endif // defined(HAVE_LIBBROTLI)

} // namespace tls

} // namespace ngtcp2

#endif // !defined(TLS_SHARED_BORINGSSL_H)
