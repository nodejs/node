/*
 * Copyright 1999-2020 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <stdlib.h>
#include "internal/cryptlib.h"
#include <openssl/x509.h>
#include <openssl/evp.h>

/*
 * Doesn't do anything now: Builtin PBE algorithms in static table.
 */

void PKCS5_PBE_add(void)
{
}

int PKCS5_PBE_keyivgen(EVP_CIPHER_CTX *cctx, const char *pass, int passlen,
                       ASN1_TYPE *param, const EVP_CIPHER *cipher,
                       const EVP_MD *md, int en_de)
{
    EVP_MD_CTX *ctx;
    unsigned char md_tmp[EVP_MAX_MD_SIZE];
    unsigned char key[EVP_MAX_KEY_LENGTH], iv[EVP_MAX_IV_LENGTH];
    int i, ivl, kl;
    PBEPARAM *pbe;
    int saltlen, iter;
    unsigned char *salt;
    int mdsize;
    int rv = 0;

    /* Extract useful info from parameter */
    if (param == NULL || param->type != V_ASN1_SEQUENCE ||
        param->value.sequence == NULL) {
        ERR_raise(ERR_LIB_EVP, EVP_R_DECODE_ERROR);
        return 0;
    }

    pbe = ASN1_TYPE_unpack_sequence(ASN1_ITEM_rptr(PBEPARAM), param);
    if (pbe == NULL) {
        ERR_raise(ERR_LIB_EVP, EVP_R_DECODE_ERROR);
        return 0;
    }

    ivl = EVP_CIPHER_iv_length(cipher);
    if (ivl < 0 || ivl > 16) {
        ERR_raise(ERR_LIB_EVP, EVP_R_INVALID_IV_LENGTH);
        PBEPARAM_free(pbe);
        return 0;
    }
    kl = EVP_CIPHER_key_length(cipher);
    if (kl < 0 || kl > (int)sizeof(md_tmp)) {
        ERR_raise(ERR_LIB_EVP, EVP_R_INVALID_KEY_LENGTH);
        PBEPARAM_free(pbe);
        return 0;
    }

    if (pbe->iter == NULL)
        iter = 1;
    else
        iter = ASN1_INTEGER_get(pbe->iter);
    salt = pbe->salt->data;
    saltlen = pbe->salt->length;

    if (pass == NULL)
        passlen = 0;
    else if (passlen == -1)
        passlen = strlen(pass);

    ctx = EVP_MD_CTX_new();
    if (ctx == NULL) {
        ERR_raise(ERR_LIB_EVP, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    if (!EVP_DigestInit_ex(ctx, md, NULL))
        goto err;
    if (!EVP_DigestUpdate(ctx, pass, passlen))
        goto err;
    if (!EVP_DigestUpdate(ctx, salt, saltlen))
        goto err;
    PBEPARAM_free(pbe);
    pbe = NULL;
    if (!EVP_DigestFinal_ex(ctx, md_tmp, NULL))
        goto err;
    mdsize = EVP_MD_size(md);
    if (mdsize < 0)
        goto err;
    for (i = 1; i < iter; i++) {
        if (!EVP_DigestInit_ex(ctx, md, NULL))
            goto err;
        if (!EVP_DigestUpdate(ctx, md_tmp, mdsize))
            goto err;
        if (!EVP_DigestFinal_ex(ctx, md_tmp, NULL))
            goto err;
    }
    memcpy(key, md_tmp, kl);
    memcpy(iv, md_tmp + (16 - ivl), ivl);
    if (!EVP_CipherInit_ex(cctx, cipher, NULL, key, iv, en_de))
        goto err;
    OPENSSL_cleanse(md_tmp, EVP_MAX_MD_SIZE);
    OPENSSL_cleanse(key, EVP_MAX_KEY_LENGTH);
    OPENSSL_cleanse(iv, EVP_MAX_IV_LENGTH);
    rv = 1;
 err:
    PBEPARAM_free(pbe);
    EVP_MD_CTX_free(ctx);
    return rv;
}
