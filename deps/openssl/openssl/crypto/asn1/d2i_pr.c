/*
 * Copyright 1995-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include "internal/cryptlib.h"
#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/engine.h>
#include <openssl/x509.h>
#include <openssl/asn1.h>
#include "crypto/asn1.h"
#include "crypto/evp.h"

EVP_PKEY *d2i_PrivateKey(int type, EVP_PKEY **a, const unsigned char **pp,
                         long length)
{
    EVP_PKEY *ret;
    const unsigned char *p = *pp;

    if ((a == NULL) || (*a == NULL)) {
        if ((ret = EVP_PKEY_new()) == NULL) {
            ASN1err(ASN1_F_D2I_PRIVATEKEY, ERR_R_EVP_LIB);
            return NULL;
        }
    } else {
        ret = *a;
#ifndef OPENSSL_NO_ENGINE
        ENGINE_finish(ret->engine);
        ret->engine = NULL;
#endif
    }

    if (!EVP_PKEY_set_type(ret, type)) {
        ASN1err(ASN1_F_D2I_PRIVATEKEY, ASN1_R_UNKNOWN_PUBLIC_KEY_TYPE);
        goto err;
    }

    if (!ret->ameth->old_priv_decode ||
        !ret->ameth->old_priv_decode(ret, &p, length)) {
        if (ret->ameth->priv_decode) {
            EVP_PKEY *tmp;
            PKCS8_PRIV_KEY_INFO *p8 = NULL;
            p8 = d2i_PKCS8_PRIV_KEY_INFO(NULL, &p, length);
            if (!p8)
                goto err;
            tmp = EVP_PKCS82PKEY(p8);
            PKCS8_PRIV_KEY_INFO_free(p8);
            if (tmp == NULL)
                goto err;
            EVP_PKEY_free(ret);
            ret = tmp;
            if (EVP_PKEY_type(type) != EVP_PKEY_base_id(ret))
                goto err;
        } else {
            ASN1err(ASN1_F_D2I_PRIVATEKEY, ERR_R_ASN1_LIB);
            goto err;
        }
    }
    *pp = p;
    if (a != NULL)
        (*a) = ret;
    return ret;
 err:
    if (a == NULL || *a != ret)
        EVP_PKEY_free(ret);
    return NULL;
}

/*
 * This works like d2i_PrivateKey() except it automatically works out the
 * type
 */

static EVP_PKEY *key_as_pkcs8(const unsigned char **pp, long length, int *carry_on)
{
    const unsigned char *p = *pp;
    PKCS8_PRIV_KEY_INFO *p8 = d2i_PKCS8_PRIV_KEY_INFO(NULL, &p, length);
    EVP_PKEY *ret;

    if (p8 == NULL)
        return NULL;

    ret = EVP_PKCS82PKEY(p8);
    if (ret == NULL)
        *carry_on = 0;

    PKCS8_PRIV_KEY_INFO_free(p8);

    if (ret != NULL)
        *pp = p;

    return ret;
}

EVP_PKEY *d2i_AutoPrivateKey(EVP_PKEY **a, const unsigned char **pp,
                             long length)
{
    STACK_OF(ASN1_TYPE) *inkey;
    const unsigned char *p;
    int keytype;
    EVP_PKEY *ret = NULL;
    int carry_on = 1;

    ERR_set_mark();
    ret = key_as_pkcs8(pp, length, &carry_on);
    if (ret != NULL) {
        ERR_clear_last_mark();
        if (a != NULL)
            *a = ret;
        return ret;
    }

    if (carry_on == 0) {
        ERR_clear_last_mark();
        ASN1err(ASN1_F_D2I_AUTOPRIVATEKEY,
                ASN1_R_UNSUPPORTED_PUBLIC_KEY_TYPE);
        return NULL;
    }
    p = *pp;

    /*
     * Dirty trick: read in the ASN1 data into a STACK_OF(ASN1_TYPE): by
     * analyzing it we can determine the passed structure: this assumes the
     * input is surrounded by an ASN1 SEQUENCE.
     */
    inkey = d2i_ASN1_SEQUENCE_ANY(NULL, &p, length);
    p = *pp;
    /*
     * Since we only need to discern "traditional format" RSA and DSA keys we
     * can just count the elements.
     */
    if (sk_ASN1_TYPE_num(inkey) == 6)
        keytype = EVP_PKEY_DSA;
    else if (sk_ASN1_TYPE_num(inkey) == 4)
        keytype = EVP_PKEY_EC;
    else
        keytype = EVP_PKEY_RSA;
    sk_ASN1_TYPE_pop_free(inkey, ASN1_TYPE_free);

    ret = d2i_PrivateKey(keytype, a, pp, length);
    if (ret != NULL)
        ERR_pop_to_mark();
    else
        ERR_clear_last_mark();

    return ret;
}
