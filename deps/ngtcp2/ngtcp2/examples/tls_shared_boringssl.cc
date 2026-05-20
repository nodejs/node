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
#include "tls_shared_boringssl.h"

#ifdef HAVE_LIBBROTLI
#  include <brotli/encode.h>
#  include <brotli/decode.h>
#endif // defined(HAVE_LIBBROTLI)

namespace ngtcp2 {

namespace tls {

#ifdef HAVE_LIBBROTLI
int cert_compress(SSL *ssl, CBB *out, const uint8_t *in, size_t in_len) {
  uint8_t *dest;

  auto compressed_size = BrotliEncoderMaxCompressedSize(in_len);
  if (compressed_size == 0) {
    return 0;
  }

  if (!CBB_reserve(out, &dest, compressed_size)) {
    return 0;
  }

  if (BrotliEncoderCompress(BROTLI_MAX_QUALITY, BROTLI_DEFAULT_WINDOW,
                            BROTLI_MODE_GENERIC, in_len, in, &compressed_size,
                            dest) != BROTLI_TRUE) {
    return 0;
  }

  if (!CBB_did_write(out, compressed_size)) {
    return 0;
  }

  return 1;
}

int cert_decompress(SSL *ssl, CRYPTO_BUFFER **out, size_t uncompressed_len,
                    const uint8_t *in, size_t in_len) {
  uint8_t *dest;
  auto buf = CRYPTO_BUFFER_alloc(&dest, uncompressed_len);
  auto len = uncompressed_len;

  if (BrotliDecoderDecompress(in_len, in, &len, dest) !=
      BROTLI_DECODER_RESULT_SUCCESS) {
    CRYPTO_BUFFER_free(buf);

    return 0;
  }

  if (uncompressed_len != len) {
    CRYPTO_BUFFER_free(buf);

    return 0;
  }

  *out = buf;

  return 1;
}
#endif // defined(HAVE_LIBBROTLI)

} // namespace tls

} // namespace ngtcp2
