/*
 * Copyright 1999-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include "internal/cryptlib.h"
#include <openssl/pkcs12.h>
#include <openssl/bn.h>

/* Uncomment out this line to get debugging info about key generation */
/*
 * #define OPENSSL_DEBUG_KEYGEN
 */
#ifdef OPENSSL_DEBUG_KEYGEN
# include <openssl/bio.h>
extern BIO *bio_err;
void h__dump(unsigned char *p, int len);
#endif

/* PKCS12 compatible key/IV generation */
#ifndef min
# define min(a,b) ((a) < (b) ? (a) : (b))
#endif

int PKCS12_key_gen_asc(const char *pass, int passlen, unsigned char *salt,
                       int saltlen, int id, int iter, int n,
                       unsigned char *out, const EVP_MD *md_type)
{
    int ret;
    unsigned char *unipass;
    int uniplen;

    if (!pass) {
        unipass = NULL;
        uniplen = 0;
    } else if (!OPENSSL_asc2uni(pass, passlen, &unipass, &uniplen)) {
        PKCS12err(PKCS12_F_PKCS12_KEY_GEN_ASC, ERR_R_MALLOC_FAILURE);
        return 0;
    }
    ret = PKCS12_key_gen_uni(unipass, uniplen, salt, saltlen,
                             id, iter, n, out, md_type);
    if (ret <= 0)
        return 0;
    OPENSSL_clear_free(unipass, uniplen);
    return ret;
}

int PKCS12_key_gen_utf8(const char *pass, int passlen, unsigned char *salt,
                        int saltlen, int id, int iter, int n,
                        unsigned char *out, const EVP_MD *md_type)
{
    int ret;
    unsigned char *unipass;
    int uniplen;

    if (!pass) {
        unipass = NULL;
        uniplen = 0;
    } else if (!OPENSSL_utf82uni(pass, passlen, &unipass, &uniplen)) {
        PKCS12err(PKCS12_F_PKCS12_KEY_GEN_UTF8, ERR_R_MALLOC_FAILURE);
        return 0;
    }
    ret = PKCS12_key_gen_uni(unipass, uniplen, salt, saltlen,
                             id, iter, n, out, md_type);
    if (ret <= 0)
        return 0;
    OPENSSL_clear_free(unipass, uniplen);
    return ret;
}

int PKCS12_key_gen_uni(unsigned char *pass, int passlen, unsigned char *salt,
                       int saltlen, int id, int iter, int n,
                       unsigned char *out, const EVP_MD *md_type)
{
    unsigned char *B = NULL, *D = NULL, *I = NULL, *p = NULL, *Ai = NULL;
    int Slen, Plen, Ilen, Ijlen;
    int i, j, u, v;
    int ret = 0;
    BIGNUM *Ij = NULL, *Bpl1 = NULL; /* These hold Ij and B + 1 */
    EVP_MD_CTX *ctx = NULL;
#ifdef  OPENSSL_DEBUG_KEYGEN
    unsigned char *tmpout = out;
    int tmpn = n;
#endif

    ctx = EVP_MD_CTX_new();
    if (ctx == NULL)
        goto err;

#ifdef  OPENSSL_DEBUG_KEYGEN
    fprintf(stderr, "KEYGEN DEBUG\n");
    fprintf(stderr, "ID %d, ITER %d\n", id, iter);
    fprintf(stderr, "Password (length %d):\n", passlen);
    h__dump(pass, passlen);
    fprintf(stderr, "Salt (length %d):\n", saltlen);
    h__dump(salt, saltlen);
#endif
    v = EVP_MD_block_size(md_type);
    u = EVP_MD_size(md_type);
    if (u < 0 || v <= 0)
        goto err;
    D = OPENSSL_malloc(v);
    Ai = OPENSSL_malloc(u);
    B = OPENSSL_malloc(v + 1);
    Slen = v * ((saltlen + v - 1) / v);
    if (passlen)
        Plen = v * ((passlen + v - 1) / v);
    else
        Plen = 0;
    Ilen = Slen + Plen;
    I = OPENSSL_malloc(Ilen);
    Ij = BN_new();
    Bpl1 = BN_new();
    if (D == NULL || Ai == NULL || B == NULL || I == NULL || Ij == NULL
            || Bpl1 == NULL)
        goto err;
    for (i = 0; i < v; i++)
        D[i] = id;
    p = I;
    for (i = 0; i < Slen; i++)
        *p++ = salt[i % saltlen];
    for (i = 0; i < Plen; i++)
        *p++ = pass[i % passlen];
    for (;;) {
        if (!EVP_DigestInit_ex(ctx, md_type, NULL)
            || !EVP_DigestUpdate(ctx, D, v)
            || !EVP_DigestUpdate(ctx, I, Ilen)
            || !EVP_DigestFinal_ex(ctx, Ai, NULL))
            goto err;
        for (j = 1; j < iter; j++) {
            if (!EVP_DigestInit_ex(ctx, md_type, NULL)
                || !EVP_DigestUpdate(ctx, Ai, u)
                || !EVP_DigestFinal_ex(ctx, Ai, NULL))
                goto err;
        }
        memcpy(out, Ai, min(n, u));
        if (u >= n) {
#ifdef OPENSSL_DEBUG_KEYGEN
            fprintf(stderr, "Output KEY (length %d)\n", tmpn);
            h__dump(tmpout, tmpn);
#endif
            ret = 1;
            goto end;
        }
        n -= u;
        out += u;
        for (j = 0; j < v; j++)
            B[j] = Ai[j % u];
        /* Work out B + 1 first then can use B as tmp space */
        if (!BN_bin2bn(B, v, Bpl1))
            goto err;
        if (!BN_add_word(Bpl1, 1))
            goto err;
        for (j = 0; j < Ilen; j += v) {
            if (!BN_bin2bn(I + j, v, Ij))
                goto err;
            if (!BN_add(Ij, Ij, Bpl1))
                goto err;
            if (!BN_bn2bin(Ij, B))
                goto err;
            Ijlen = BN_num_bytes(Ij);
            /* If more than 2^(v*8) - 1 cut off MSB */
            if (Ijlen > v) {
                if (!BN_bn2bin(Ij, B))
                    goto err;
                memcpy(I + j, B + 1, v);
#ifndef PKCS12_BROKEN_KEYGEN
                /* If less than v bytes pad with zeroes */
            } else if (Ijlen < v) {
                memset(I + j, 0, v - Ijlen);
                if (!BN_bn2bin(Ij, I + j + v - Ijlen))
                    goto err;
#endif
            } else if (!BN_bn2bin(Ij, I + j))
                goto err;
        }
    }

 err:
    PKCS12err(PKCS12_F_PKCS12_KEY_GEN_UNI, ERR_R_MALLOC_FAILURE);

 end:
    OPENSSL_free(Ai);
    OPENSSL_free(B);
    OPENSSL_free(D);
    OPENSSL_free(I);
    BN_free(Ij);
    BN_free(Bpl1);
    EVP_MD_CTX_free(ctx);
    return ret;
}

#ifdef OPENSSL_DEBUG_KEYGEN
void h__dump(unsigned char *p, int len)
{
    for (; len--; p++)
        fprintf(stderr, "%02X", *p);
    fprintf(stderr, "\n");
}
#endif
