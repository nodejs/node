/*
  zip_crypto_gnutls.c -- GnuTLS wrapper.
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

#include <stdlib.h>

#include "zipint.h"

#include "zip_crypto.h"

_zip_crypto_aes_t *
_zip_crypto_aes_new(const zip_uint8_t *key, zip_uint16_t key_size, zip_error_t *error) {
    _zip_crypto_aes_t *aes;

    if ((aes = (_zip_crypto_aes_t *)malloc(sizeof(*aes))) == NULL) {
        zip_error_set(error, ZIP_ER_MEMORY, 0);
        return NULL;
    }

    aes->key_size = key_size;

    switch (aes->key_size) {
    case 128:
        nettle_aes128_set_encrypt_key(&aes->ctx.ctx_128, key);
        break;
    case 192:
        nettle_aes192_set_encrypt_key(&aes->ctx.ctx_192, key);
        break;
    case 256:
        nettle_aes256_set_encrypt_key(&aes->ctx.ctx_256, key);
        break;
    default:
        zip_error_set(error, ZIP_ER_INVAL, 0);
        free(aes);
        return NULL;
    }

    return aes;
}

bool
_zip_crypto_aes_encrypt_block(_zip_crypto_aes_t *aes, const zip_uint8_t *in, zip_uint8_t *out) {
    switch (aes->key_size) {
    case 128:
        nettle_aes128_encrypt(&aes->ctx.ctx_128, ZIP_CRYPTO_AES_BLOCK_LENGTH, out, in);
        break;
    case 192:
        nettle_aes192_encrypt(&aes->ctx.ctx_192, ZIP_CRYPTO_AES_BLOCK_LENGTH, out, in);
        break;
    case 256:
        nettle_aes256_encrypt(&aes->ctx.ctx_256, ZIP_CRYPTO_AES_BLOCK_LENGTH, out, in);
        break;
    }

    return true;
}

void
_zip_crypto_aes_free(_zip_crypto_aes_t *aes) {
    if (aes == NULL) {
        return;
    }

    _zip_crypto_clear(aes, sizeof(*aes));
    free(aes);
}


_zip_crypto_hmac_t *
_zip_crypto_hmac_new(const zip_uint8_t *secret, zip_uint64_t secret_length, zip_error_t *error) {
    _zip_crypto_hmac_t *hmac;

    if ((hmac = (_zip_crypto_hmac_t *)malloc(sizeof(*hmac))) == NULL) {
        zip_error_set(error, ZIP_ER_MEMORY, 0);
        return NULL;
    }

    if (gnutls_hmac_init(hmac, GNUTLS_MAC_SHA1, secret, secret_length) < 0) {
        zip_error_set(error, ZIP_ER_INTERNAL, 0);
        free(hmac);
        return NULL;
    }

    return hmac;
}


void
_zip_crypto_hmac_free(_zip_crypto_hmac_t *hmac) {
    zip_uint8_t buf[ZIP_CRYPTO_SHA1_LENGTH];

    if (hmac == NULL) {
        return;
    }

    gnutls_hmac_deinit(*hmac, buf);
    _zip_crypto_clear(hmac, sizeof(*hmac));
    free(hmac);
}


ZIP_EXTERN bool
zip_secure_random(zip_uint8_t *buffer, zip_uint16_t length) {
    return gnutls_rnd(GNUTLS_RND_KEY, buffer, length) == 0;
}
