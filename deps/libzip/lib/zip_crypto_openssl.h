/*
  zip_crypto_openssl.h -- definitions for OpenSSL wrapper.
  Copyright (C) 2018-2021 Dieter Baron and Thomas Klausner

  This file is part of libzip, a library to manipulate ZIP archives.
  The authors can be contacted at <info@libzip.org>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
  1. Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in
  the documentation and/or other materials provided with the
  distribution.
  3. The names of the authors may not be used to endorse or promote
  products derived from this software without specific prior
  written permission.

  THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS
  OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
  IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
  IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef HAD_ZIP_CRYPTO_OPENSSL_H
#define HAD_ZIP_CRYPTO_OPENSSL_H

#define HAVE_SECURE_RANDOM

#include <openssl/evp.h>
#include <openssl/hmac.h>

#define _zip_crypto_aes_t EVP_CIPHER_CTX
#define _zip_crypto_hmac_t HMAC_CTX

void _zip_crypto_aes_free(_zip_crypto_aes_t *aes);
bool _zip_crypto_aes_encrypt_block(_zip_crypto_aes_t *aes, const zip_uint8_t *in, zip_uint8_t *out);
_zip_crypto_aes_t *_zip_crypto_aes_new(const zip_uint8_t *key, zip_uint16_t key_size, zip_error_t *error);

#define _zip_crypto_hmac(hmac, data, length) (HMAC_Update((hmac), (data), (length)) == 1)
void _zip_crypto_hmac_free(_zip_crypto_hmac_t *hmac);
_zip_crypto_hmac_t *_zip_crypto_hmac_new(const zip_uint8_t *secret, zip_uint64_t secret_length, zip_error_t *error);
bool _zip_crypto_hmac_output(_zip_crypto_hmac_t *hmac, zip_uint8_t *data);

#define _zip_crypto_pbkdf2(key, key_length, salt, salt_length, iterations, output, output_length) (PKCS5_PBKDF2_HMAC_SHA1((const char *)(key), (key_length), (salt), (salt_length), (iterations), (output_length), (output)))

#endif /*  HAD_ZIP_CRYPTO_OPENSSL_H */
