/*
 * Copyright 2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/core_dispatch.h>
#include "crypto/lms.h"
#include <string.h>

/**
 * @brief Create a new LMS_KEY object
 *
 * @param libctx A OSSL_LIB_CTX object used for fetching algorithms.
 * @returns The new LMS_KEY object on success, or NULL on malloc failure
 */
LMS_KEY *ossl_lms_key_new(OSSL_LIB_CTX *libctx)
{
    LMS_KEY *ret = OPENSSL_zalloc(sizeof(LMS_KEY));

    if (ret != NULL)
        ret->libctx = libctx;
    return ret;
}

/**
 * @brief Destroy a LMS_KEY object
 */
void ossl_lms_key_free(LMS_KEY *lmskey)
{
    LMS_PUB_KEY *pub;

    if (lmskey == NULL)
        return;

    pub = &lmskey->pub;
    OPENSSL_free(pub->encoded);
    OPENSSL_free(lmskey);
}

/**
 * @brief Are 2 LMS public keys equal?
 *
 * To be equal the keys must have the same LMS_PARAMS, LM_OTS_PARAMS and
 * encoded public keys.
 *
 * @param key1 A LMS_KEY object
 * @param key2 A LMS_KEY object
 * @param selection Only OSSL_KEYMGMT_SELECT_PUBLIC_KEY is supported
 * @returns 1 if the keys are equal otherwise it returns 0.
 */
int ossl_lms_key_equal(const LMS_KEY *key1, const LMS_KEY *key2, int selection)
{
    int ok = 1;

    if (key1->lms_params != key2->lms_params
            || key1->ots_params != key2->ots_params)
        return 0;

    if ((selection & OSSL_KEYMGMT_SELECT_PUBLIC_KEY) != 0) {
        if (key1->pub.encodedlen != key2->pub.encodedlen)
            return 0;
        ok = (key1->pub.encodedlen == 0)
            || (memcmp(key1->pub.encoded, key2->pub.encoded,
                       key1->pub.encodedlen) == 0);
    }
    return ok;
}

/**
 * @brief Is a LMS_KEY valid.
 *
 * @param key A LMS_KEY object
 * @param selection Currently only supports |OSSL_KEYMGMT_SELECT_PUBLIC_KEY|
 * @returns 1 if a LMS_KEY contains valid key data.
 */
int ossl_lms_key_valid(const LMS_KEY *key, int selection)
{
    if (key == NULL)
        return 0;

    if ((selection & OSSL_KEYMGMT_SELECT_PUBLIC_KEY) != 0)
        return key->pub.encoded != NULL && key->pub.encodedlen != 0;
    if ((selection & OSSL_KEYMGMT_SELECT_PRIVATE_KEY) != 0)
        return 0; /* a private key that doesn't exist can't be valid */
    return 1;
}

/**
 * @brief Does a LMS_KEY object contain a public key.
 *
 * @param key A LMS_KEY object
 * @param selection Currently only supports |OSSL_KEYMGMT_SELECT_PUBLIC_KEY|
 * @returns 1 if a LMS_KEY contains public key data, or 0 otherwise.
 */
int ossl_lms_key_has(const LMS_KEY *key, int selection)
{
    if ((selection & OSSL_KEYMGMT_SELECT_PUBLIC_KEY) != 0)
        return (key != NULL) && (key->pub.K != NULL);
    /* There is no private key currently */
    if ((selection & OSSL_KEYMGMT_SELECT_PRIVATE_KEY) != 0)
        return 0;
    return 1;
}
