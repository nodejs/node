/*
 * Copyright 2020-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <string.h>
#include <openssl/err.h>
#include "crypto/ecx.h"

ECX_KEY *ossl_ecx_key_new(OSSL_LIB_CTX *libctx, ECX_KEY_TYPE type, int haspubkey,
                          const char *propq)
{
    ECX_KEY *ret = OPENSSL_zalloc(sizeof(*ret));

    if (ret == NULL)
        return NULL;

    ret->libctx = libctx;
    ret->haspubkey = haspubkey;
    switch (type) {
    case ECX_KEY_TYPE_X25519:
        ret->keylen = X25519_KEYLEN;
        break;
    case ECX_KEY_TYPE_X448:
        ret->keylen = X448_KEYLEN;
        break;
    case ECX_KEY_TYPE_ED25519:
        ret->keylen = ED25519_KEYLEN;
        break;
    case ECX_KEY_TYPE_ED448:
        ret->keylen = ED448_KEYLEN;
        break;
    }
    ret->type = type;
    ret->references = 1;

    if (propq != NULL) {
        ret->propq = OPENSSL_strdup(propq);
        if (ret->propq == NULL)
            goto err;
    }

    ret->lock = CRYPTO_THREAD_lock_new();
    if (ret->lock == NULL)
        goto err;
    return ret;
err:
    ERR_raise(ERR_LIB_EC, ERR_R_MALLOC_FAILURE);
    OPENSSL_free(ret);
    return NULL;
}

void ossl_ecx_key_free(ECX_KEY *key)
{
    int i;

    if (key == NULL)
        return;

    CRYPTO_DOWN_REF(&key->references, &i, key->lock);
    REF_PRINT_COUNT("ECX_KEY", key);
    if (i > 0)
        return;
    REF_ASSERT_ISNT(i < 0);

    OPENSSL_free(key->propq);
    OPENSSL_secure_clear_free(key->privkey, key->keylen);
    CRYPTO_THREAD_lock_free(key->lock);
    OPENSSL_free(key);
}

void ossl_ecx_key_set0_libctx(ECX_KEY *key, OSSL_LIB_CTX *libctx)
{
    key->libctx = libctx;
}

int ossl_ecx_key_up_ref(ECX_KEY *key)
{
    int i;

    if (CRYPTO_UP_REF(&key->references, &i, key->lock) <= 0)
        return 0;

    REF_PRINT_COUNT("ECX_KEY", key);
    REF_ASSERT_ISNT(i < 2);
    return ((i > 1) ? 1 : 0);
}

unsigned char *ossl_ecx_key_allocate_privkey(ECX_KEY *key)
{
    key->privkey = OPENSSL_secure_zalloc(key->keylen);

    return key->privkey;
}
