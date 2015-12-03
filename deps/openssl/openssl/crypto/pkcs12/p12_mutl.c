/* p12_mutl.c */
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

#ifndef OPENSSL_NO_HMAC
# include <stdio.h>
# include "cryptlib.h"
# include <openssl/crypto.h>
# include <openssl/hmac.h>
# include <openssl/rand.h>
# include <openssl/pkcs12.h>

/* Generate a MAC */
int PKCS12_gen_mac(PKCS12 *p12, const char *pass, int passlen,
                   unsigned char *mac, unsigned int *maclen)
{
    const EVP_MD *md_type;
    HMAC_CTX hmac;
    unsigned char key[EVP_MAX_MD_SIZE], *salt;
    int saltlen, iter;
    int md_size;

    if (!PKCS7_type_is_data(p12->authsafes)) {
        PKCS12err(PKCS12_F_PKCS12_GEN_MAC, PKCS12_R_CONTENT_TYPE_NOT_DATA);
        return 0;
    }

    salt = p12->mac->salt->data;
    saltlen = p12->mac->salt->length;
    if (!p12->mac->iter)
        iter = 1;
    else
        iter = ASN1_INTEGER_get(p12->mac->iter);
    if (!(md_type = EVP_get_digestbyobj(p12->mac->dinfo->algor->algorithm))) {
        PKCS12err(PKCS12_F_PKCS12_GEN_MAC, PKCS12_R_UNKNOWN_DIGEST_ALGORITHM);
        return 0;
    }
    md_size = EVP_MD_size(md_type);
    if (md_size < 0)
        return 0;
    if (!PKCS12_key_gen(pass, passlen, salt, saltlen, PKCS12_MAC_ID, iter,
                        md_size, key, md_type)) {
        PKCS12err(PKCS12_F_PKCS12_GEN_MAC, PKCS12_R_KEY_GEN_ERROR);
        return 0;
    }
    HMAC_CTX_init(&hmac);
    if (!HMAC_Init_ex(&hmac, key, md_size, md_type, NULL)
        || !HMAC_Update(&hmac, p12->authsafes->d.data->data,
                        p12->authsafes->d.data->length)
        || !HMAC_Final(&hmac, mac, maclen)) {
        HMAC_CTX_cleanup(&hmac);
        return 0;
    }
    HMAC_CTX_cleanup(&hmac);
    return 1;
}

/* Verify the mac */
int PKCS12_verify_mac(PKCS12 *p12, const char *pass, int passlen)
{
    unsigned char mac[EVP_MAX_MD_SIZE];
    unsigned int maclen;
    if (p12->mac == NULL) {
        PKCS12err(PKCS12_F_PKCS12_VERIFY_MAC, PKCS12_R_MAC_ABSENT);
        return 0;
    }
    if (!PKCS12_gen_mac(p12, pass, passlen, mac, &maclen)) {
        PKCS12err(PKCS12_F_PKCS12_VERIFY_MAC, PKCS12_R_MAC_GENERATION_ERROR);
        return 0;
    }
    if ((maclen != (unsigned int)p12->mac->dinfo->digest->length)
        || CRYPTO_memcmp(mac, p12->mac->dinfo->digest->data, maclen))
        return 0;
    return 1;
}

/* Set a mac */

int PKCS12_set_mac(PKCS12 *p12, const char *pass, int passlen,
                   unsigned char *salt, int saltlen, int iter,
                   const EVP_MD *md_type)
{
    unsigned char mac[EVP_MAX_MD_SIZE];
    unsigned int maclen;

    if (!md_type)
        md_type = EVP_sha1();
    if (PKCS12_setup_mac(p12, iter, salt, saltlen, md_type) == PKCS12_ERROR) {
        PKCS12err(PKCS12_F_PKCS12_SET_MAC, PKCS12_R_MAC_SETUP_ERROR);
        return 0;
    }
    if (!PKCS12_gen_mac(p12, pass, passlen, mac, &maclen)) {
        PKCS12err(PKCS12_F_PKCS12_SET_MAC, PKCS12_R_MAC_GENERATION_ERROR);
        return 0;
    }
    if (!(M_ASN1_OCTET_STRING_set(p12->mac->dinfo->digest, mac, maclen))) {
        PKCS12err(PKCS12_F_PKCS12_SET_MAC, PKCS12_R_MAC_STRING_SET_ERROR);
        return 0;
    }
    return 1;
}

/* Set up a mac structure */
int PKCS12_setup_mac(PKCS12 *p12, int iter, unsigned char *salt, int saltlen,
                     const EVP_MD *md_type)
{
    if (!(p12->mac = PKCS12_MAC_DATA_new()))
        return PKCS12_ERROR;
    if (iter > 1) {
        if (!(p12->mac->iter = M_ASN1_INTEGER_new())) {
            PKCS12err(PKCS12_F_PKCS12_SETUP_MAC, ERR_R_MALLOC_FAILURE);
            return 0;
        }
        if (!ASN1_INTEGER_set(p12->mac->iter, iter)) {
            PKCS12err(PKCS12_F_PKCS12_SETUP_MAC, ERR_R_MALLOC_FAILURE);
            return 0;
        }
    }
    if (!saltlen)
        saltlen = PKCS12_SALT_LEN;
    if ((p12->mac->salt->data = OPENSSL_malloc(saltlen)) == NULL) {
        PKCS12err(PKCS12_F_PKCS12_SETUP_MAC, ERR_R_MALLOC_FAILURE);
        return 0;
    }
    p12->mac->salt->length = saltlen;
    if (!salt) {
        if (RAND_pseudo_bytes(p12->mac->salt->data, saltlen) < 0)
            return 0;
    } else
        memcpy(p12->mac->salt->data, salt, saltlen);
    p12->mac->dinfo->algor->algorithm = OBJ_nid2obj(EVP_MD_type(md_type));
    if (!(p12->mac->dinfo->algor->parameter = ASN1_TYPE_new())) {
        PKCS12err(PKCS12_F_PKCS12_SETUP_MAC, ERR_R_MALLOC_FAILURE);
        return 0;
    }
    p12->mac->dinfo->algor->parameter->type = V_ASN1_NULL;

    return 1;
}
#endif
