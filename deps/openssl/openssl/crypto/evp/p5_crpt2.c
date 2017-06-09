/* p5_crpt2.c */
/*
 * Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL project
 * 1999.
 */
/* ====================================================================
 * Copyright (c) 1999-2006 The OpenSSL Project.  All rights reserved.
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
#include <stdlib.h>
#include "cryptlib.h"
#if !defined(OPENSSL_NO_HMAC) && !defined(OPENSSL_NO_SHA)
# include <openssl/x509.h>
# include <openssl/evp.h>
# include <openssl/hmac.h>
# include "evp_locl.h"

/* set this to print out info about the keygen algorithm */
/* #define DEBUG_PKCS5V2 */

# ifdef DEBUG_PKCS5V2
static void h__dump(const unsigned char *p, int len);
# endif

/*
 * This is an implementation of PKCS#5 v2.0 password based encryption key
 * derivation function PBKDF2. SHA1 version verified against test vectors
 * posted by Peter Gutmann <pgut001@cs.auckland.ac.nz> to the PKCS-TNG
 * <pkcs-tng@rsa.com> mailing list.
 */

int PKCS5_PBKDF2_HMAC(const char *pass, int passlen,
                      const unsigned char *salt, int saltlen, int iter,
                      const EVP_MD *digest, int keylen, unsigned char *out)
{
    unsigned char digtmp[EVP_MAX_MD_SIZE], *p, itmp[4];
    int cplen, j, k, tkeylen, mdlen;
    unsigned long i = 1;
    HMAC_CTX hctx_tpl, hctx;

    mdlen = EVP_MD_size(digest);
    if (mdlen < 0)
        return 0;

    HMAC_CTX_init(&hctx_tpl);
    p = out;
    tkeylen = keylen;
    if (!pass)
        passlen = 0;
    else if (passlen == -1)
        passlen = strlen(pass);
    if (!HMAC_Init_ex(&hctx_tpl, pass, passlen, digest, NULL)) {
        HMAC_CTX_cleanup(&hctx_tpl);
        return 0;
    }
    while (tkeylen) {
        if (tkeylen > mdlen)
            cplen = mdlen;
        else
            cplen = tkeylen;
        /*
         * We are unlikely to ever use more than 256 blocks (5120 bits!) but
         * just in case...
         */
        itmp[0] = (unsigned char)((i >> 24) & 0xff);
        itmp[1] = (unsigned char)((i >> 16) & 0xff);
        itmp[2] = (unsigned char)((i >> 8) & 0xff);
        itmp[3] = (unsigned char)(i & 0xff);
        if (!HMAC_CTX_copy(&hctx, &hctx_tpl)) {
            HMAC_CTX_cleanup(&hctx_tpl);
            return 0;
        }
        if (!HMAC_Update(&hctx, salt, saltlen)
            || !HMAC_Update(&hctx, itmp, 4)
            || !HMAC_Final(&hctx, digtmp, NULL)) {
            HMAC_CTX_cleanup(&hctx_tpl);
            HMAC_CTX_cleanup(&hctx);
            return 0;
        }
        HMAC_CTX_cleanup(&hctx);
        memcpy(p, digtmp, cplen);
        for (j = 1; j < iter; j++) {
            if (!HMAC_CTX_copy(&hctx, &hctx_tpl)) {
                HMAC_CTX_cleanup(&hctx_tpl);
                return 0;
            }
            if (!HMAC_Update(&hctx, digtmp, mdlen)
                || !HMAC_Final(&hctx, digtmp, NULL)) {
                HMAC_CTX_cleanup(&hctx_tpl);
                HMAC_CTX_cleanup(&hctx);
                return 0;
            }
            HMAC_CTX_cleanup(&hctx);
            for (k = 0; k < cplen; k++)
                p[k] ^= digtmp[k];
        }
        tkeylen -= cplen;
        i++;
        p += cplen;
    }
    HMAC_CTX_cleanup(&hctx_tpl);
# ifdef DEBUG_PKCS5V2
    fprintf(stderr, "Password:\n");
    h__dump(pass, passlen);
    fprintf(stderr, "Salt:\n");
    h__dump(salt, saltlen);
    fprintf(stderr, "Iteration count %d\n", iter);
    fprintf(stderr, "Key:\n");
    h__dump(out, keylen);
# endif
    return 1;
}

int PKCS5_PBKDF2_HMAC_SHA1(const char *pass, int passlen,
                           const unsigned char *salt, int saltlen, int iter,
                           int keylen, unsigned char *out)
{
    return PKCS5_PBKDF2_HMAC(pass, passlen, salt, saltlen, iter, EVP_sha1(),
                             keylen, out);
}

# ifdef DO_TEST
main()
{
    unsigned char out[4];
    unsigned char salt[] = { 0x12, 0x34, 0x56, 0x78 };
    PKCS5_PBKDF2_HMAC_SHA1("password", -1, salt, 4, 5, 4, out);
    fprintf(stderr, "Out %02X %02X %02X %02X\n",
            out[0], out[1], out[2], out[3]);
}

# endif

/*
 * Now the key derivation function itself. This is a bit evil because it has
 * to check the ASN1 parameters are valid: and there are quite a few of
 * them...
 */

int PKCS5_v2_PBE_keyivgen(EVP_CIPHER_CTX *ctx, const char *pass, int passlen,
                          ASN1_TYPE *param, const EVP_CIPHER *c,
                          const EVP_MD *md, int en_de)
{
    const unsigned char *pbuf;
    int plen;
    PBE2PARAM *pbe2 = NULL;
    const EVP_CIPHER *cipher;

    int rv = 0;

    if (param == NULL || param->type != V_ASN1_SEQUENCE ||
        param->value.sequence == NULL) {
        EVPerr(EVP_F_PKCS5_V2_PBE_KEYIVGEN, EVP_R_DECODE_ERROR);
        goto err;
    }

    pbuf = param->value.sequence->data;
    plen = param->value.sequence->length;
    if (!(pbe2 = d2i_PBE2PARAM(NULL, &pbuf, plen))) {
        EVPerr(EVP_F_PKCS5_V2_PBE_KEYIVGEN, EVP_R_DECODE_ERROR);
        goto err;
    }

    /* See if we recognise the key derivation function */

    if (OBJ_obj2nid(pbe2->keyfunc->algorithm) != NID_id_pbkdf2) {
        EVPerr(EVP_F_PKCS5_V2_PBE_KEYIVGEN,
               EVP_R_UNSUPPORTED_KEY_DERIVATION_FUNCTION);
        goto err;
    }

    /*
     * lets see if we recognise the encryption algorithm.
     */

    cipher = EVP_get_cipherbyobj(pbe2->encryption->algorithm);

    if (!cipher) {
        EVPerr(EVP_F_PKCS5_V2_PBE_KEYIVGEN, EVP_R_UNSUPPORTED_CIPHER);
        goto err;
    }

    /* Fixup cipher based on AlgorithmIdentifier */
    if (!EVP_CipherInit_ex(ctx, cipher, NULL, NULL, NULL, en_de))
        goto err;
    if (EVP_CIPHER_asn1_to_param(ctx, pbe2->encryption->parameter) < 0) {
        EVPerr(EVP_F_PKCS5_V2_PBE_KEYIVGEN, EVP_R_CIPHER_PARAMETER_ERROR);
        goto err;
    }
    rv = PKCS5_v2_PBKDF2_keyivgen(ctx, pass, passlen,
                                  pbe2->keyfunc->parameter, c, md, en_de);
 err:
    PBE2PARAM_free(pbe2);
    return rv;
}

int PKCS5_v2_PBKDF2_keyivgen(EVP_CIPHER_CTX *ctx, const char *pass,
                             int passlen, ASN1_TYPE *param,
                             const EVP_CIPHER *c, const EVP_MD *md, int en_de)
{
    unsigned char *salt, key[EVP_MAX_KEY_LENGTH];
    const unsigned char *pbuf;
    int saltlen, iter, plen;
    int rv = 0;
    unsigned int keylen = 0;
    int prf_nid, hmac_md_nid;
    PBKDF2PARAM *kdf = NULL;
    const EVP_MD *prfmd;

    if (EVP_CIPHER_CTX_cipher(ctx) == NULL) {
        EVPerr(EVP_F_PKCS5_V2_PBKDF2_KEYIVGEN, EVP_R_NO_CIPHER_SET);
        goto err;
    }
    keylen = EVP_CIPHER_CTX_key_length(ctx);
    OPENSSL_assert(keylen <= sizeof key);

    /* Decode parameter */

    if (!param || (param->type != V_ASN1_SEQUENCE)) {
        EVPerr(EVP_F_PKCS5_V2_PBKDF2_KEYIVGEN, EVP_R_DECODE_ERROR);
        goto err;
    }

    pbuf = param->value.sequence->data;
    plen = param->value.sequence->length;

    if (!(kdf = d2i_PBKDF2PARAM(NULL, &pbuf, plen))) {
        EVPerr(EVP_F_PKCS5_V2_PBKDF2_KEYIVGEN, EVP_R_DECODE_ERROR);
        goto err;
    }

    keylen = EVP_CIPHER_CTX_key_length(ctx);

    /* Now check the parameters of the kdf */

    if (kdf->keylength && (ASN1_INTEGER_get(kdf->keylength) != (int)keylen)) {
        EVPerr(EVP_F_PKCS5_V2_PBKDF2_KEYIVGEN, EVP_R_UNSUPPORTED_KEYLENGTH);
        goto err;
    }

    if (kdf->prf)
        prf_nid = OBJ_obj2nid(kdf->prf->algorithm);
    else
        prf_nid = NID_hmacWithSHA1;

    if (!EVP_PBE_find(EVP_PBE_TYPE_PRF, prf_nid, NULL, &hmac_md_nid, 0)) {
        EVPerr(EVP_F_PKCS5_V2_PBKDF2_KEYIVGEN, EVP_R_UNSUPPORTED_PRF);
        goto err;
    }

    prfmd = EVP_get_digestbynid(hmac_md_nid);
    if (prfmd == NULL) {
        EVPerr(EVP_F_PKCS5_V2_PBKDF2_KEYIVGEN, EVP_R_UNSUPPORTED_PRF);
        goto err;
    }

    if (kdf->salt->type != V_ASN1_OCTET_STRING) {
        EVPerr(EVP_F_PKCS5_V2_PBKDF2_KEYIVGEN, EVP_R_UNSUPPORTED_SALT_TYPE);
        goto err;
    }

    /* it seems that its all OK */
    salt = kdf->salt->value.octet_string->data;
    saltlen = kdf->salt->value.octet_string->length;
    iter = ASN1_INTEGER_get(kdf->iter);
    if (!PKCS5_PBKDF2_HMAC(pass, passlen, salt, saltlen, iter, prfmd,
                           keylen, key))
        goto err;
    rv = EVP_CipherInit_ex(ctx, NULL, NULL, key, NULL, en_de);
 err:
    OPENSSL_cleanse(key, keylen);
    PBKDF2PARAM_free(kdf);
    return rv;
}

# ifdef DEBUG_PKCS5V2
static void h__dump(const unsigned char *p, int len)
{
    for (; len--; p++)
        fprintf(stderr, "%02X ", *p);
    fprintf(stderr, "\n");
}
# endif
#endif
