/*
  zip_crypto_win.c -- Windows Crypto API wrapper.
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
#include <limits.h>

#include "zipint.h"

#include "zip_crypto.h"

#define WIN32_LEAN_AND_MEAN
#define NOCRYPT

#include <windows.h>

#include <bcrypt.h>

#pragma comment(lib, "bcrypt.lib")

/*

This code is using the Cryptography API: Next Generation (CNG)
https://docs.microsoft.com/en-us/windows/desktop/seccng/cng-portal

This API is supported on
 - Windows Vista or later (client OS)
 - Windows Server 2008 (server OS)
 - Windows Embedded Compact 2013 (don't know about Windows Embedded Compact 7)

The code was developed for Windows Embedded Compact 2013 (WEC2013),
but should be working for all of the above mentioned OSes.

There are 2 restrictions for WEC2013, Windows Vista and Windows Server 2008:

1.) The function "BCryptDeriveKeyPBKDF2" is not available

I found some code which is implementing this function using the deprecated Crypto API here:
https://www.idrix.fr/Root/content/view/37/54/

I took this code and converted it to the newer CNG API. The original code was more
flexible, but this is not needed here so i refactored it a bit and just kept what is needed.

The define "HAS_BCRYPTDERIVEKEYPBKDF2" controls whether "BCryptDeriveKeyPBKDF2"
of the CNG API is used or not. This define must not be set if you are compiling for WEC2013 or Windows Vista.


2.) "BCryptCreateHash" can't manage the memory needed for the hash object internally

On Windows 7 or later it is possible to pass NULL for the hash object buffer.
This is not supported on WEC2013, so we have to handle the memory allocation/deallocation ourselves.
There is no #ifdef to control that, because this is working for all supported OSes.

*/

#if !defined(WINCE) && !defined(__MINGW32__)
#define HAS_BCRYPTDERIVEKEYPBKDF2
#endif

#ifdef HAS_BCRYPTDERIVEKEYPBKDF2

bool
_zip_crypto_pbkdf2(const zip_uint8_t *key, zip_uint64_t key_length, const zip_uint8_t *salt, zip_uint16_t salt_length, zip_uint16_t iterations, zip_uint8_t *output, zip_uint16_t output_length) {
    BCRYPT_ALG_HANDLE hAlgorithm = NULL;
    bool result;

    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlgorithm, BCRYPT_SHA1_ALGORITHM, NULL, BCRYPT_ALG_HANDLE_HMAC_FLAG))) {
        return false;
    }

    result = BCRYPT_SUCCESS(BCryptDeriveKeyPBKDF2(hAlgorithm, (PUCHAR)key, (ULONG)key_length, (PUCHAR)salt, salt_length, iterations, output, output_length, 0));

    BCryptCloseAlgorithmProvider(hAlgorithm, 0);

    return result;
}

#else

#include <math.h>

#define DIGEST_SIZE 20
#define BLOCK_SIZE 64

typedef struct {
    BCRYPT_ALG_HANDLE hAlgorithm;
    BCRYPT_HASH_HANDLE hInnerHash;
    BCRYPT_HASH_HANDLE hOuterHash;
    ULONG cbHashObject;
    PUCHAR pbInnerHash;
    PUCHAR pbOuterHash;
} PRF_CTX;

static void
hmacFree(PRF_CTX *pContext) {
    if (pContext->hOuterHash)
        BCryptDestroyHash(pContext->hOuterHash);
    if (pContext->hInnerHash)
        BCryptDestroyHash(pContext->hInnerHash);
    free(pContext->pbOuterHash);
    free(pContext->pbInnerHash);
    if (pContext->hAlgorithm)
        BCryptCloseAlgorithmProvider(pContext->hAlgorithm, 0);
}

static BOOL
hmacPrecomputeDigest(BCRYPT_HASH_HANDLE hHash, PUCHAR pbPassword, DWORD cbPassword, BYTE mask) {
    BYTE buffer[BLOCK_SIZE];
    DWORD i;

    if (cbPassword > BLOCK_SIZE) {
        return FALSE;
    }

    memset(buffer, mask, sizeof(buffer));

    for (i = 0; i < cbPassword; ++i) {
        buffer[i] = (char)(pbPassword[i] ^ mask);
    }

    return BCRYPT_SUCCESS(BCryptHashData(hHash, buffer, sizeof(buffer), 0));
}

static BOOL
hmacInit(PRF_CTX *pContext, PUCHAR pbPassword, DWORD cbPassword) {
    BOOL bStatus = FALSE;
    ULONG cbResult;
    BYTE key[DIGEST_SIZE];

    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&pContext->hAlgorithm, BCRYPT_SHA1_ALGORITHM, NULL, 0)) || !BCRYPT_SUCCESS(BCryptGetProperty(pContext->hAlgorithm, BCRYPT_OBJECT_LENGTH, (PUCHAR)&pContext->cbHashObject, sizeof(pContext->cbHashObject), &cbResult, 0)) || ((pContext->pbInnerHash = malloc(pContext->cbHashObject)) == NULL) || ((pContext->pbOuterHash = malloc(pContext->cbHashObject)) == NULL) || !BCRYPT_SUCCESS(BCryptCreateHash(pContext->hAlgorithm, &pContext->hInnerHash, pContext->pbInnerHash, pContext->cbHashObject, NULL, 0, 0)) || !BCRYPT_SUCCESS(BCryptCreateHash(pContext->hAlgorithm, &pContext->hOuterHash, pContext->pbOuterHash, pContext->cbHashObject, NULL, 0, 0))) {
        goto hmacInit_end;
    }

    if (cbPassword > BLOCK_SIZE) {
        BCRYPT_HASH_HANDLE hHash = NULL;
        PUCHAR pbHashObject = malloc(pContext->cbHashObject);
        if (pbHashObject == NULL) {
            goto hmacInit_end;
        }

        bStatus = BCRYPT_SUCCESS(BCryptCreateHash(pContext->hAlgorithm, &hHash, pbHashObject, pContext->cbHashObject, NULL, 0, 0)) && BCRYPT_SUCCESS(BCryptHashData(hHash, pbPassword, cbPassword, 0)) && BCRYPT_SUCCESS(BCryptGetProperty(hHash, BCRYPT_HASH_LENGTH, (PUCHAR)&cbPassword, sizeof(cbPassword), &cbResult, 0)) && BCRYPT_SUCCESS(BCryptFinishHash(hHash, key, cbPassword, 0));

        if (hHash)
            BCryptDestroyHash(hHash);
        free(pbHashObject);

        if (!bStatus) {
            goto hmacInit_end;
        }

        pbPassword = key;
    }

    bStatus = hmacPrecomputeDigest(pContext->hInnerHash, pbPassword, cbPassword, 0x36) && hmacPrecomputeDigest(pContext->hOuterHash, pbPassword, cbPassword, 0x5C);

hmacInit_end:

    if (bStatus == FALSE)
        hmacFree(pContext);

    return bStatus;
}

static BOOL
hmacCalculateInternal(BCRYPT_HASH_HANDLE hHashTemplate, PUCHAR pbData, DWORD cbData, PUCHAR pbOutput, DWORD cbOutput, DWORD cbHashObject) {
    BOOL success = FALSE;
    BCRYPT_HASH_HANDLE hHash = NULL;
    PUCHAR pbHashObject = malloc(cbHashObject);

    if (pbHashObject == NULL) {
        return FALSE;
    }

    if (BCRYPT_SUCCESS(BCryptDuplicateHash(hHashTemplate, &hHash, pbHashObject, cbHashObject, 0))) {
        success = BCRYPT_SUCCESS(BCryptHashData(hHash, pbData, cbData, 0)) && BCRYPT_SUCCESS(BCryptFinishHash(hHash, pbOutput, cbOutput, 0));

        BCryptDestroyHash(hHash);
    }

    free(pbHashObject);

    return success;
}

static BOOL
hmacCalculate(PRF_CTX *pContext, PUCHAR pbData, DWORD cbData, PUCHAR pbDigest) {
    DWORD cbResult;
    DWORD cbHashObject;

    return BCRYPT_SUCCESS(BCryptGetProperty(pContext->hAlgorithm, BCRYPT_OBJECT_LENGTH, (PUCHAR)&cbHashObject, sizeof(cbHashObject), &cbResult, 0)) && hmacCalculateInternal(pContext->hInnerHash, pbData, cbData, pbDigest, DIGEST_SIZE, cbHashObject) && hmacCalculateInternal(pContext->hOuterHash, pbDigest, DIGEST_SIZE, pbDigest, DIGEST_SIZE, cbHashObject);
}

static void
myxor(LPBYTE ptr1, LPBYTE ptr2, DWORD dwLen) {
    while (dwLen--)
        *ptr1++ ^= *ptr2++;
}

BOOL
pbkdf2(PUCHAR pbPassword, ULONG cbPassword, PUCHAR pbSalt, ULONG cbSalt, DWORD cIterations, PUCHAR pbDerivedKey, ULONG cbDerivedKey) {
    BOOL bStatus = FALSE;
    DWORD l, r, dwULen, i, j;
    BYTE Ti[DIGEST_SIZE];
    BYTE V[DIGEST_SIZE];
    LPBYTE U = malloc(max((cbSalt + 4), DIGEST_SIZE));
    PRF_CTX prfCtx = {0};

    if (U == NULL) {
        return FALSE;
    }

    if (pbPassword == NULL || cbPassword == 0 || pbSalt == NULL || cbSalt == 0 || cIterations == 0 || pbDerivedKey == NULL || cbDerivedKey == 0) {
        free(U);
        return FALSE;
    }

    if (!hmacInit(&prfCtx, pbPassword, cbPassword)) {
        goto PBKDF2_end;
    }

    l = (DWORD)ceil((double)cbDerivedKey / (double)DIGEST_SIZE);
    r = cbDerivedKey - (l - 1) * DIGEST_SIZE;

    for (i = 1; i <= l; i++) {
        ZeroMemory(Ti, DIGEST_SIZE);
        for (j = 0; j < cIterations; j++) {
            if (j == 0) {
                /* construct first input for PRF */
                (void)memcpy_s(U, cbSalt, pbSalt, cbSalt);
                U[cbSalt] = (BYTE)((i & 0xFF000000) >> 24);
                U[cbSalt + 1] = (BYTE)((i & 0x00FF0000) >> 16);
                U[cbSalt + 2] = (BYTE)((i & 0x0000FF00) >> 8);
                U[cbSalt + 3] = (BYTE)((i & 0x000000FF));
                dwULen = cbSalt + 4;
            }
            else {
                (void)memcpy_s(U, DIGEST_SIZE, V, DIGEST_SIZE);
                dwULen = DIGEST_SIZE;
            }

            if (!hmacCalculate(&prfCtx, U, dwULen, V)) {
                goto PBKDF2_end;
            }

            myxor(Ti, V, DIGEST_SIZE);
        }

        if (i != l) {
            (void)memcpy_s(&pbDerivedKey[(i - 1) * DIGEST_SIZE], cbDerivedKey - (i - 1) * DIGEST_SIZE, Ti, DIGEST_SIZE);
        }
        else {
            /* Take only the first r bytes */
            (void)memcpy_s(&pbDerivedKey[(i - 1) * DIGEST_SIZE], cbDerivedKey - (i - 1) * DIGEST_SIZE, Ti, r);
        }
    }

    bStatus = TRUE;

PBKDF2_end:

    hmacFree(&prfCtx);
    free(U);
    return bStatus;
}

bool
_zip_crypto_pbkdf2(const zip_uint8_t *key, zip_uint64_t key_length, const zip_uint8_t *salt, zip_uint16_t salt_length, zip_uint16_t iterations, zip_uint8_t *output, zip_uint16_t output_length) {
    return (key_length <= ZIP_UINT32_MAX) && pbkdf2((PUCHAR)key, (ULONG)key_length, (PUCHAR)salt, salt_length, iterations, output, output_length);
}

#endif


struct _zip_crypto_aes_s {
    BCRYPT_ALG_HANDLE hAlgorithm;
    BCRYPT_KEY_HANDLE hKey;
    ULONG cbKeyObject;
    PUCHAR pbKeyObject;
};

_zip_crypto_aes_t *
_zip_crypto_aes_new(const zip_uint8_t *key, zip_uint16_t key_size, zip_error_t *error) {
    _zip_crypto_aes_t *aes = (_zip_crypto_aes_t *)calloc(1, sizeof(*aes));

    ULONG cbResult;
    ULONG key_length = key_size / 8;

    if (aes == NULL) {
        zip_error_set(error, ZIP_ER_MEMORY, 0);
        return NULL;
    }

    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&aes->hAlgorithm, BCRYPT_AES_ALGORITHM, NULL, 0))) {
        _zip_crypto_aes_free(aes);
        return NULL;
    }

    if (!BCRYPT_SUCCESS(BCryptSetProperty(aes->hAlgorithm, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_ECB, sizeof(BCRYPT_CHAIN_MODE_ECB), 0))) {
        _zip_crypto_aes_free(aes);
        return NULL;
    }

    if (!BCRYPT_SUCCESS(BCryptGetProperty(aes->hAlgorithm, BCRYPT_OBJECT_LENGTH, (PUCHAR)&aes->cbKeyObject, sizeof(aes->cbKeyObject), &cbResult, 0))) {
        _zip_crypto_aes_free(aes);
        return NULL;
    }

    aes->pbKeyObject = malloc(aes->cbKeyObject);
    if (aes->pbKeyObject == NULL) {
        _zip_crypto_aes_free(aes);
        zip_error_set(error, ZIP_ER_MEMORY, 0);
        return NULL;
    }

    if (!BCRYPT_SUCCESS(BCryptGenerateSymmetricKey(aes->hAlgorithm, &aes->hKey, aes->pbKeyObject, aes->cbKeyObject, (PUCHAR)key, key_length, 0))) {
        _zip_crypto_aes_free(aes);
        return NULL;
    }

    return aes;
}

void
_zip_crypto_aes_free(_zip_crypto_aes_t *aes) {
    if (aes == NULL) {
        return;
    }

    if (aes->hKey != NULL) {
        BCryptDestroyKey(aes->hKey);
    }

    if (aes->pbKeyObject != NULL) {
        free(aes->pbKeyObject);
    }

    if (aes->hAlgorithm != NULL) {
        BCryptCloseAlgorithmProvider(aes->hAlgorithm, 0);
    }

    free(aes);
}

bool
_zip_crypto_aes_encrypt_block(_zip_crypto_aes_t *aes, const zip_uint8_t *in, zip_uint8_t *out) {
    ULONG cbResult;
    NTSTATUS status = BCryptEncrypt(aes->hKey, (PUCHAR)in, ZIP_CRYPTO_AES_BLOCK_LENGTH, NULL, NULL, 0, (PUCHAR)out, ZIP_CRYPTO_AES_BLOCK_LENGTH, &cbResult, 0);
    return BCRYPT_SUCCESS(status);
}

struct _zip_crypto_hmac_s {
    BCRYPT_ALG_HANDLE hAlgorithm;
    BCRYPT_HASH_HANDLE hHash;
    DWORD cbHashObject;
    PUCHAR pbHashObject;
    DWORD cbHash;
    PUCHAR pbHash;
};

/* https://code.msdn.microsoft.com/windowsdesktop/Hmac-Computation-Sample-11fe8ec1/sourcecode?fileId=42820&pathId=283874677 */

_zip_crypto_hmac_t *
_zip_crypto_hmac_new(const zip_uint8_t *secret, zip_uint64_t secret_length, zip_error_t *error) {
    NTSTATUS status;
    ULONG cbResult;
    _zip_crypto_hmac_t *hmac;

    if (secret_length > INT_MAX) {
        zip_error_set(error, ZIP_ER_INVAL, 0);
        return NULL;
    }

    hmac = (_zip_crypto_hmac_t *)calloc(1, sizeof(*hmac));

    if (hmac == NULL) {
        zip_error_set(error, ZIP_ER_MEMORY, 0);
        return NULL;
    }

    status = BCryptOpenAlgorithmProvider(&hmac->hAlgorithm, BCRYPT_SHA1_ALGORITHM, NULL, BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (!BCRYPT_SUCCESS(status)) {
        _zip_crypto_hmac_free(hmac);
        return NULL;
    }

    status = BCryptGetProperty(hmac->hAlgorithm, BCRYPT_OBJECT_LENGTH, (PUCHAR)&hmac->cbHashObject, sizeof(hmac->cbHashObject), &cbResult, 0);
    if (!BCRYPT_SUCCESS(status)) {
        _zip_crypto_hmac_free(hmac);
        return NULL;
    }

    hmac->pbHashObject = malloc(hmac->cbHashObject);
    if (hmac->pbHashObject == NULL) {
        _zip_crypto_hmac_free(hmac);
        zip_error_set(error, ZIP_ER_MEMORY, 0);
        return NULL;
    }

    status = BCryptGetProperty(hmac->hAlgorithm, BCRYPT_HASH_LENGTH, (PUCHAR)&hmac->cbHash, sizeof(hmac->cbHash), &cbResult, 0);
    if (!BCRYPT_SUCCESS(status)) {
        _zip_crypto_hmac_free(hmac);
        return NULL;
    }

    hmac->pbHash = malloc(hmac->cbHash);
    if (hmac->pbHash == NULL) {
        _zip_crypto_hmac_free(hmac);
        zip_error_set(error, ZIP_ER_MEMORY, 0);
        return NULL;
    }

    status = BCryptCreateHash(hmac->hAlgorithm, &hmac->hHash, hmac->pbHashObject, hmac->cbHashObject, (PUCHAR)secret, (ULONG)secret_length, 0);
    if (!BCRYPT_SUCCESS(status)) {
        _zip_crypto_hmac_free(hmac);
        return NULL;
    }

    return hmac;
}

void
_zip_crypto_hmac_free(_zip_crypto_hmac_t *hmac) {
    if (hmac == NULL) {
        return;
    }

    if (hmac->hHash != NULL) {
        BCryptDestroyHash(hmac->hHash);
    }

    if (hmac->pbHash != NULL) {
        free(hmac->pbHash);
    }

    if (hmac->pbHashObject != NULL) {
        free(hmac->pbHashObject);
    }

    if (hmac->hAlgorithm) {
        BCryptCloseAlgorithmProvider(hmac->hAlgorithm, 0);
    }

    free(hmac);
}

bool
_zip_crypto_hmac(_zip_crypto_hmac_t *hmac, zip_uint8_t *data, zip_uint64_t length) {
    if (hmac == NULL || length > ULONG_MAX) {
        return false;
    }

    return BCRYPT_SUCCESS(BCryptHashData(hmac->hHash, data, (ULONG)length, 0));
}

bool
_zip_crypto_hmac_output(_zip_crypto_hmac_t *hmac, zip_uint8_t *data) {
    if (hmac == NULL) {
        return false;
    }

    return BCRYPT_SUCCESS(BCryptFinishHash(hmac->hHash, data, hmac->cbHash, 0));
}

ZIP_EXTERN bool
zip_secure_random(zip_uint8_t *buffer, zip_uint16_t length) {
    return BCRYPT_SUCCESS(BCryptGenRandom(NULL, buffer, length, BCRYPT_USE_SYSTEM_PREFERRED_RNG));
}
