/*
  zip_winzip_aes.c -- Winzip AES de/encryption backend routines
  Copyright (C) 2017-2021 Dieter Baron and Thomas Klausner

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

#include "zipint.h"

#include "zip_crypto.h"

#include <stdlib.h>
#include <string.h>


#define MAX_KEY_LENGTH 256
#define PBKDF2_ITERATIONS 1000

struct _zip_winzip_aes {
    _zip_crypto_aes_t *aes;
    _zip_crypto_hmac_t *hmac;
    zip_uint8_t counter[ZIP_CRYPTO_AES_BLOCK_LENGTH];
    zip_uint8_t pad[ZIP_CRYPTO_AES_BLOCK_LENGTH];
    int pad_offset;
};

static bool
aes_crypt(zip_winzip_aes_t *ctx, zip_uint8_t *data, zip_uint64_t length) {
    zip_uint64_t i, j;

    for (i = 0; i < length; i++) {
        if (ctx->pad_offset == AES_BLOCK_SIZE) {
            for (j = 0; j < 8; j++) {
                ctx->counter[j]++;
                if (ctx->counter[j] != 0) {
                    break;
                }
            }
            if (!_zip_crypto_aes_encrypt_block(ctx->aes, ctx->counter, ctx->pad)) {
                return false;
            }
            ctx->pad_offset = 0;
        }
        data[i] ^= ctx->pad[ctx->pad_offset++];
    }

    return true;
}


zip_winzip_aes_t *
_zip_winzip_aes_new(const zip_uint8_t *password, zip_uint64_t password_length, const zip_uint8_t *salt, zip_uint16_t encryption_method, zip_uint8_t *password_verify, zip_error_t *error) {
    zip_winzip_aes_t *ctx;
    zip_uint8_t buffer[2 * (MAX_KEY_LENGTH / 8) + WINZIP_AES_PASSWORD_VERIFY_LENGTH];
    zip_uint16_t key_size = 0; /* in bits */
    zip_uint16_t key_length;   /* in bytes */

    switch (encryption_method) {
    case ZIP_EM_AES_128:
        key_size = 128;
        break;
    case ZIP_EM_AES_192:
        key_size = 192;
        break;
    case ZIP_EM_AES_256:
        key_size = 256;
        break;
    }

    if (key_size == 0 || salt == NULL || password == NULL || password_length == 0) {
        zip_error_set(error, ZIP_ER_INVAL, 0);
        return NULL;
    }

    key_length = key_size / 8;

    if ((ctx = (zip_winzip_aes_t *)malloc(sizeof(*ctx))) == NULL) {
        zip_error_set(error, ZIP_ER_MEMORY, 0);
        return NULL;
    }

    memset(ctx->counter, 0, sizeof(ctx->counter));
    ctx->pad_offset = ZIP_CRYPTO_AES_BLOCK_LENGTH;

    if (!_zip_crypto_pbkdf2(password, password_length, salt, key_length / 2, PBKDF2_ITERATIONS, buffer, 2 * key_length + WINZIP_AES_PASSWORD_VERIFY_LENGTH)) {
        free(ctx);
        return NULL;
    }

    if ((ctx->aes = _zip_crypto_aes_new(buffer, key_size, error)) == NULL) {
        _zip_crypto_clear(ctx, sizeof(*ctx));
        free(ctx);
        return NULL;
    }
    if ((ctx->hmac = _zip_crypto_hmac_new(buffer + key_length, key_length, error)) == NULL) {
        _zip_crypto_aes_free(ctx->aes);
        free(ctx);
        return NULL;
    }

    if (password_verify) {
        (void)memcpy_s(password_verify, WINZIP_AES_PASSWORD_VERIFY_LENGTH, buffer + (2 * key_size / 8), WINZIP_AES_PASSWORD_VERIFY_LENGTH);
    }

    return ctx;
}


bool
_zip_winzip_aes_encrypt(zip_winzip_aes_t *ctx, zip_uint8_t *data, zip_uint64_t length) {
    return aes_crypt(ctx, data, length) && _zip_crypto_hmac(ctx->hmac, data, length);
}


bool
_zip_winzip_aes_decrypt(zip_winzip_aes_t *ctx, zip_uint8_t *data, zip_uint64_t length) {
    return _zip_crypto_hmac(ctx->hmac, data, length) && aes_crypt(ctx, data, length);
}


bool
_zip_winzip_aes_finish(zip_winzip_aes_t *ctx, zip_uint8_t *hmac) {
    return _zip_crypto_hmac_output(ctx->hmac, hmac);
}


void
_zip_winzip_aes_free(zip_winzip_aes_t *ctx) {
    if (ctx == NULL) {
        return;
    }

    _zip_crypto_aes_free(ctx->aes);
    _zip_crypto_hmac_free(ctx->hmac);
    free(ctx);
}
