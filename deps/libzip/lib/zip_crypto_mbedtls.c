/*
  zip_crypto_mbedtls.c -- mbed TLS wrapper
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

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/pkcs5.h>

#include <limits.h>

_zip_crypto_aes_t *
_zip_crypto_aes_new(const zip_uint8_t *key, zip_uint16_t key_size, zip_error_t *error) {
    _zip_crypto_aes_t *aes;

    if ((aes = (_zip_crypto_aes_t *)malloc(sizeof(*aes))) == NULL) {
        zip_error_set(error, ZIP_ER_MEMORY, 0);
        return NULL;
    }

    mbedtls_aes_init(aes);
    mbedtls_aes_setkey_enc(aes, (const unsigned char *)key, (unsigned int)key_size);

    return aes;
}

void
_zip_crypto_aes_free(_zip_crypto_aes_t *aes) {
    if (aes == NULL) {
        return;
    }

    mbedtls_aes_free(aes);
    free(aes);
}


_zip_crypto_hmac_t *
_zip_crypto_hmac_new(const zip_uint8_t *secret, zip_uint64_t secret_length, zip_error_t *error) {
    _zip_crypto_hmac_t *hmac;

    if (secret_length > INT_MAX) {
        zip_error_set(error, ZIP_ER_INVAL, 0);
        return NULL;
    }

    if ((hmac = (_zip_crypto_hmac_t *)malloc(sizeof(*hmac))) == NULL) {
        zip_error_set(error, ZIP_ER_MEMORY, 0);
        return NULL;
    }

    mbedtls_md_init(hmac);

    if (mbedtls_md_setup(hmac, mbedtls_md_info_from_type(MBEDTLS_MD_SHA1), 1) != 0) {
        zip_error_set(error, ZIP_ER_INTERNAL, 0);
        free(hmac);
        return NULL;
    }

    if (mbedtls_md_hmac_starts(hmac, (const unsigned char *)secret, (size_t)secret_length) != 0) {
        zip_error_set(error, ZIP_ER_INTERNAL, 0);
        free(hmac);
        return NULL;
    }

    return hmac;
}


void
_zip_crypto_hmac_free(_zip_crypto_hmac_t *hmac) {
    if (hmac == NULL) {
        return;
    }

    mbedtls_md_free(hmac);
    free(hmac);
}


bool
_zip_crypto_pbkdf2(const zip_uint8_t *key, zip_uint64_t key_length, const zip_uint8_t *salt, zip_uint16_t salt_length, int iterations, zip_uint8_t *output, zip_uint64_t output_length) {
    mbedtls_md_context_t sha1_ctx;
    bool ok = true;

    mbedtls_md_init(&sha1_ctx);

    if (mbedtls_md_setup(&sha1_ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA1), 1) != 0) {
        ok = false;
    }

    if (ok && mbedtls_pkcs5_pbkdf2_hmac(&sha1_ctx, (const unsigned char *)key, (size_t)key_length, (const unsigned char *)salt, (size_t)salt_length, (unsigned int)iterations, (uint32_t)output_length, (unsigned char *)output) != 0) {
        ok = false;
    }

    mbedtls_md_free(&sha1_ctx);
    return ok;
}


typedef struct {
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
} zip_random_context_t;

ZIP_EXTERN bool
zip_secure_random(zip_uint8_t *buffer, zip_uint16_t length) {
    static zip_random_context_t *ctx = NULL;
    const unsigned char *pers = "zip_crypto_mbedtls";

    if (!ctx) {
        ctx = (zip_random_context_t *)malloc(sizeof(zip_random_context_t));
        if (!ctx) {
            return false;
        }
        mbedtls_entropy_init(&ctx->entropy);
        mbedtls_ctr_drbg_init(&ctx->ctr_drbg);
        if (mbedtls_ctr_drbg_seed(&ctx->ctr_drbg, mbedtls_entropy_func, &ctx->entropy, pers, strlen(pers)) != 0) {
            mbedtls_ctr_drbg_free(&ctx->ctr_drbg);
            mbedtls_entropy_free(&ctx->entropy);
            free(ctx);
            ctx = NULL;
            return false;
        }
    }

    return mbedtls_ctr_drbg_random(&ctx->ctr_drbg, (unsigned char *)buffer, (size_t)length) == 0;
}
