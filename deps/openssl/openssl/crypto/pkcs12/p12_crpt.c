/* p12_crpt.c */
/*
 * Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL project
 * 1999.
 */
/* ====================================================================
 * Copyright (c) 1999 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

#include <stdio.h>
#include "cryptlib.h"
#include <openssl/pkcs12.h>

/* PKCS#12 PBE algorithms now in static table */

void PKCS12_PBE_add(void)
{
}

int PKCS12_PBE_keyivgen(EVP_CIPHER_CTX *ctx, const char *pass, int passlen,
                        ASN1_TYPE *param, const EVP_CIPHER *cipher,
                        const EVP_MD *md, int en_de)
{
    PBEPARAM *pbe;
    int saltlen, iter, ret;
    unsigned char *salt;
    const unsigned char *pbuf;
    unsigned char key[EVP_MAX_KEY_LENGTH], iv[EVP_MAX_IV_LENGTH];

    if (cipher == NULL)
        return 0;

    /* Extract useful info from parameter */
    if (param == NULL || param->type != V_ASN1_SEQUENCE ||
        param->value.sequence == NULL) {
        PKCS12err(PKCS12_F_PKCS12_PBE_KEYIVGEN, PKCS12_R_DECODE_ERROR);
        return 0;
    }

    pbuf = param->value.sequence->data;
    if (!(pbe = d2i_PBEPARAM(NULL, &pbuf, param->value.sequence->length))) {
        PKCS12err(PKCS12_F_PKCS12_PBE_KEYIVGEN, PKCS12_R_DECODE_ERROR);
        return 0;
    }

    if (!pbe->iter)
        iter = 1;
    else
        iter = ASN1_INTEGER_get(pbe->iter);
    salt = pbe->salt->data;
    saltlen = pbe->salt->length;
    if (!PKCS12_key_gen(pass, passlen, salt, saltlen, PKCS12_KEY_ID,
                        iter, EVP_CIPHER_key_length(cipher), key, md)) {
        PKCS12err(PKCS12_F_PKCS12_PBE_KEYIVGEN, PKCS12_R_KEY_GEN_ERROR);
        PBEPARAM_free(pbe);
        return 0;
    }
    if (!PKCS12_key_gen(pass, passlen, salt, saltlen, PKCS12_IV_ID,
                        iter, EVP_CIPHER_iv_length(cipher), iv, md)) {
        PKCS12err(PKCS12_F_PKCS12_PBE_KEYIVGEN, PKCS12_R_IV_GEN_ERROR);
        PBEPARAM_free(pbe);
        return 0;
    }
    PBEPARAM_free(pbe);
    ret = EVP_CipherInit_ex(ctx, cipher, NULL, key, iv, en_de);
    OPENSSL_cleanse(key, EVP_MAX_KEY_LENGTH);
    OPENSSL_cleanse(iv, EVP_MAX_IV_LENGTH);
    return ret;
}
