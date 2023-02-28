/*
  zip_crypto_openssl.c -- OpenSSL wrapper.
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

#include <limits.h>
#include <openssl/rand.h>

#if OPENSSL_VERSION_NUMBER < 0x1010000fL || (defined(LIBRESSL_VERSION_NUMBER) && LIBRESSL_VERSION_NUMBER < 0x02070000fL)
#define USE_OPENSSL_1_0_API
#endif


_zip_crypto_aes_t *
_zip_crypto_aes_new(const zip_uint8_t *key, zip_uint16_t key_size, zip_error_t *error) {
    _zip_crypto_aes_t *aes;
    const EVP_CIPHER* cipher_type;

    switch (key_size) {
    case 128:
        cipher_type = EVP_aes_128_ecb();
        break;
    case 192:
        cipher_type = EVP_aes_192_ecb();
        break;
    case 256:
        cipher_type = EVP_aes_256_ecb();
        break;
    default:
        zip_error_set(error, ZIP_ER_INTERNAL, 0);
        return NULL;
    }

#ifdef USE_OPENSSL_1_0_API
    if ((aes = (_zip_crypto_aes_t *)malloc(sizeof(*aes))) == NULL) {
        zip_error_set(error, ZIP_ER_MEMORY, 0);
        return NULL;
    }
    memset(aes, 0, sizeof(*aes));
#else
    if ((aes = EVP_CIPHER_CTX_new()) == NULL) {
        zip_error_set(error, ZIP_ER_MEMORY, 0);
        return NULL;
    }
#endif

    if (EVP_EncryptInit_ex(aes, cipher_type, NULL, key, NULL) != 1) {
#ifdef USE_OPENSSL_1_0_API
        free(aes);
#else
        EVP_CIPHER_CTX_free(aes);
#endif
        zip_error_set(error, ZIP_ER_INTERNAL, 0);
        return NULL;
    }

    return aes;
}

void
_zip_crypto_aes_free(_zip_crypto_aes_t *aes) {
    if (aes == NULL) {
        return;
    }

#ifdef USE_OPENSSL_1_0_API
    EVP_CIPHER_CTX_cleanup(aes);
    _zip_crypto_clear(aes, sizeof(*aes));
    free(aes);
#else
    EVP_CIPHER_CTX_free(aes);
#endif
}


bool
_zip_crypto_aes_encrypt_block(_zip_crypto_aes_t *aes, const zip_uint8_t *in, zip_uint8_t *out) {
    int len;
    if (EVP_EncryptUpdate(aes, out, &len, in, ZIP_CRYPTO_AES_BLOCK_LENGTH) != 1) {
        return false;
    }
    return true;
}


_zip_crypto_hmac_t *
_zip_crypto_hmac_new(const zip_uint8_t *secret, zip_uint64_t secret_length, zip_error_t *error) {
    _zip_crypto_hmac_t *hmac;

    if (secret_length > INT_MAX) {
        zip_error_set(error, ZIP_ER_INVAL, 0);
        return NULL;
    }

#ifdef USE_OPENSSL_1_0_API
    if ((hmac = (_zip_crypto_hmac_t *)malloc(sizeof(*hmac))) == NULL) {
        zip_error_set(error, ZIP_ER_MEMORY, 0);
        return NULL;
    }

    HMAC_CTX_init(hmac);
#else
    if ((hmac = HMAC_CTX_new()) == NULL) {
        zip_error_set(error, ZIP_ER_MEMORY, 0);
        return NULL;
    }
#endif

    if (HMAC_Init_ex(hmac, secret, (int)secret_length, EVP_sha1(), NULL) != 1) {
        zip_error_set(error, ZIP_ER_INTERNAL, 0);
#ifdef USE_OPENSSL_1_0_API
        free(hmac);
#else
        HMAC_CTX_free(hmac);
#endif
        return NULL;
    }

    return hmac;
}


void
_zip_crypto_hmac_free(_zip_crypto_hmac_t *hmac) {
    if (hmac == NULL) {
        return;
    }

#ifdef USE_OPENSSL_1_0_API
    HMAC_CTX_cleanup(hmac);
    _zip_crypto_clear(hmac, sizeof(*hmac));
    free(hmac);
#else
    HMAC_CTX_free(hmac);
#endif
}


bool
_zip_crypto_hmac_output(_zip_crypto_hmac_t *hmac, zip_uint8_t *data) {
    unsigned int length;

    return HMAC_Final(hmac, data, &length) == 1;
}


ZIP_EXTERN bool
zip_secure_random(zip_uint8_t *buffer, zip_uint16_t length) {
    return RAND_bytes(buffer, length) == 1;
}
