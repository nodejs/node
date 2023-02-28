/*
  zip_crypto_commoncrypto.c -- CommonCrypto wrapper.
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

#include <fcntl.h>
#include <unistd.h>

void
_zip_crypto_aes_free(_zip_crypto_aes_t *aes) {
    if (aes == NULL) {
        return;
    }

    CCCryptorRelease(aes);
}


bool
_zip_crypto_aes_encrypt_block(_zip_crypto_aes_t *aes, const zip_uint8_t *in, zip_uint8_t *out) {
    size_t len;
    CCCryptorUpdate(aes, in, ZIP_CRYPTO_AES_BLOCK_LENGTH, out, ZIP_CRYPTO_AES_BLOCK_LENGTH, &len);
    return true;
}


_zip_crypto_aes_t *
_zip_crypto_aes_new(const zip_uint8_t *key, zip_uint16_t key_size, zip_error_t *error) {
    _zip_crypto_aes_t *aes;
    CCCryptorStatus ret;

    ret = CCCryptorCreate(kCCEncrypt, kCCAlgorithmAES, kCCOptionECBMode, key, key_size / 8, NULL, &aes);

    switch (ret) {
    case kCCSuccess:
        return aes;

    case kCCMemoryFailure:
        zip_error_set(error, ZIP_ER_MEMORY, 0);
        return NULL;

    case kCCParamError:
        zip_error_set(error, ZIP_ER_INVAL, 0);
        return NULL;

    default:
        zip_error_set(error, ZIP_ER_INTERNAL, 0);
        return NULL;
    }
}


void
_zip_crypto_hmac_free(_zip_crypto_hmac_t *hmac) {
    if (hmac == NULL) {
        return;
    }

    _zip_crypto_clear(hmac, sizeof(*hmac));
    free(hmac);
}


_zip_crypto_hmac_t *
_zip_crypto_hmac_new(const zip_uint8_t *secret, zip_uint64_t secret_length, zip_error_t *error) {
    _zip_crypto_hmac_t *hmac;

    if ((hmac = (_zip_crypto_hmac_t *)malloc(sizeof(*hmac))) == NULL) {
        zip_error_set(error, ZIP_ER_MEMORY, 0);
        return NULL;
    }

    CCHmacInit(hmac, kCCHmacAlgSHA1, secret, secret_length);

    return hmac;
}
