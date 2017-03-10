/*
 * Copyright 2006-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include "internal/cryptlib.h"
#include <openssl/x509.h>
#include <openssl/ec.h>
#include <openssl/rand.h>
#include "internal/asn1_int.h"
#include "internal/evp_int.h"
#include "ec_lcl.h"

#define X25519_KEYLEN        32
#define X25519_BITS          253
#define X25519_SECURITY_BITS 128

typedef struct {
    unsigned char pubkey[X25519_KEYLEN];
    unsigned char *privkey;
} X25519_KEY;

typedef enum {
    X25519_PUBLIC,
    X25519_PRIVATE,
    X25519_KEYGEN
} ecx_key_op_t;

/* Setup EVP_PKEY using public, private or generation */
static int ecx_key_op(EVP_PKEY *pkey, const X509_ALGOR *palg,
                      const unsigned char *p, int plen, ecx_key_op_t op)
{
    X25519_KEY *xkey;

    if (op != X25519_KEYGEN) {
        if (palg != NULL) {
            int ptype;

            /* Algorithm parameters must be absent */
            X509_ALGOR_get0(NULL, &ptype, NULL, palg);
            if (ptype != V_ASN1_UNDEF) {
                ECerr(EC_F_ECX_KEY_OP, EC_R_INVALID_ENCODING);
                return 0;
            }
        }

        if (p == NULL || plen != X25519_KEYLEN) {
            ECerr(EC_F_ECX_KEY_OP, EC_R_INVALID_ENCODING);
            return 0;
        }
    }

    xkey = OPENSSL_zalloc(sizeof(*xkey));
    if (xkey == NULL) {
        ECerr(EC_F_ECX_KEY_OP, ERR_R_MALLOC_FAILURE);
        return 0;
    }

    if (op == X25519_PUBLIC) {
        memcpy(xkey->pubkey, p, plen);
    } else {
        xkey->privkey = OPENSSL_secure_malloc(X25519_KEYLEN);
        if (xkey->privkey == NULL) {
            ECerr(EC_F_ECX_KEY_OP, ERR_R_MALLOC_FAILURE);
            OPENSSL_free(xkey);
            return 0;
        }
        if (op == X25519_KEYGEN) {
            if (RAND_bytes(xkey->privkey, X25519_KEYLEN) <= 0) {
                OPENSSL_secure_free(xkey->privkey);
                OPENSSL_free(xkey);
                return 0;
            }
            xkey->privkey[0] &= 248;
            xkey->privkey[31] &= 127;
            xkey->privkey[31] |= 64;
        } else {
            memcpy(xkey->privkey, p, X25519_KEYLEN);
        }
        X25519_public_from_private(xkey->pubkey, xkey->privkey);
    }

    EVP_PKEY_assign(pkey, NID_X25519, xkey);
    return 1;
}

static int ecx_pub_encode(X509_PUBKEY *pk, const EVP_PKEY *pkey)
{
    const X25519_KEY *xkey = pkey->pkey.ptr;
    unsigned char *penc;

    if (xkey == NULL) {
        ECerr(EC_F_ECX_PUB_ENCODE, EC_R_INVALID_KEY);
        return 0;
    }

    penc = OPENSSL_memdup(xkey->pubkey, X25519_KEYLEN);
    if (penc == NULL) {
        ECerr(EC_F_ECX_PUB_ENCODE, ERR_R_MALLOC_FAILURE);
        return 0;
    }

    if (!X509_PUBKEY_set0_param(pk, OBJ_nid2obj(NID_X25519), V_ASN1_UNDEF,
                                NULL, penc, X25519_KEYLEN)) {
        OPENSSL_free(penc);
        ECerr(EC_F_ECX_PUB_ENCODE, ERR_R_MALLOC_FAILURE);
        return 0;
    }
    return 1;
}

static int ecx_pub_decode(EVP_PKEY *pkey, X509_PUBKEY *pubkey)
{
    const unsigned char *p;
    int pklen;
    X509_ALGOR *palg;

    if (!X509_PUBKEY_get0_param(NULL, &p, &pklen, &palg, pubkey))
        return 0;
    return ecx_key_op(pkey, palg, p, pklen, X25519_PUBLIC);
}

static int ecx_pub_cmp(const EVP_PKEY *a, const EVP_PKEY *b)
{
    const X25519_KEY *akey = a->pkey.ptr;
    const X25519_KEY *bkey = b->pkey.ptr;

    if (akey == NULL || bkey == NULL)
        return -2;
    return !CRYPTO_memcmp(akey->pubkey, bkey->pubkey, X25519_KEYLEN);
}

static int ecx_priv_decode(EVP_PKEY *pkey, const PKCS8_PRIV_KEY_INFO *p8)
{
    const unsigned char *p;
    int plen;
    ASN1_OCTET_STRING *oct = NULL;
    const X509_ALGOR *palg;
    int rv;

    if (!PKCS8_pkey_get0(NULL, &p, &plen, &palg, p8))
        return 0;

    oct = d2i_ASN1_OCTET_STRING(NULL, &p, plen);
    if (oct == NULL) {
        p = NULL;
        plen = 0;
    } else {
        p = ASN1_STRING_get0_data(oct);
        plen = ASN1_STRING_length(oct);
    }

    rv = ecx_key_op(pkey, palg, p, plen, X25519_PRIVATE);
    ASN1_OCTET_STRING_free(oct);
    return rv;
}

static int ecx_priv_encode(PKCS8_PRIV_KEY_INFO *p8, const EVP_PKEY *pkey)
{
    const X25519_KEY *xkey = pkey->pkey.ptr;
    ASN1_OCTET_STRING oct;
    unsigned char *penc = NULL;
    int penclen;

    if (xkey == NULL || xkey->privkey == NULL) {
        ECerr(EC_F_ECX_PRIV_ENCODE, EC_R_INVALID_PRIVATE_KEY);
        return 0;
    }

    oct.data = xkey->privkey;
    oct.length = X25519_KEYLEN;
    oct.flags = 0;

    penclen = i2d_ASN1_OCTET_STRING(&oct, &penc);
    if (penclen < 0) {
        ECerr(EC_F_ECX_PRIV_ENCODE, ERR_R_MALLOC_FAILURE);
        return 0;
    }

    if (!PKCS8_pkey_set0(p8, OBJ_nid2obj(NID_X25519), 0,
                         V_ASN1_UNDEF, NULL, penc, penclen)) {
        OPENSSL_clear_free(penc, penclen);
        ECerr(EC_F_ECX_PRIV_ENCODE, ERR_R_MALLOC_FAILURE);
        return 0;
    }

    return 1;
}

static int ecx_size(const EVP_PKEY *pkey)
{
    return X25519_KEYLEN;
}

static int ecx_bits(const EVP_PKEY *pkey)
{
    return X25519_BITS;
}

static int ecx_security_bits(const EVP_PKEY *pkey)
{
    return X25519_SECURITY_BITS;
}

static void ecx_free(EVP_PKEY *pkey)
{
    X25519_KEY *xkey = pkey->pkey.ptr;

    if (xkey)
        OPENSSL_secure_free(xkey->privkey);
    OPENSSL_free(xkey);
}

/* "parameters" are always equal */
static int ecx_cmp_parameters(const EVP_PKEY *a, const EVP_PKEY *b)
{
    return 1;
}

static int ecx_key_print(BIO *bp, const EVP_PKEY *pkey, int indent,
                         ASN1_PCTX *ctx, ecx_key_op_t op)
{
    const X25519_KEY *xkey = pkey->pkey.ptr;

    if (op == X25519_PRIVATE) {
        if (xkey == NULL || xkey->privkey == NULL) {
            if (BIO_printf(bp, "%*s<INVALID PRIVATE KEY>\n", indent, "") <= 0)
                return 0;
            return 1;
        }
        if (BIO_printf(bp, "%*sX25519 Private-Key:\n", indent, "") <= 0)
            return 0;
        if (BIO_printf(bp, "%*spriv:\n", indent, "") <= 0)
            return 0;
        if (ASN1_buf_print(bp, xkey->privkey, X25519_KEYLEN, indent + 4) == 0)
            return 0;
    } else {
        if (xkey == NULL) {
            if (BIO_printf(bp, "%*s<INVALID PUBLIC KEY>\n", indent, "") <= 0)
                return 0;
            return 1;
        }
        if (BIO_printf(bp, "%*sX25519 Public-Key:\n", indent, "") <= 0)
            return 0;
    }
    if (BIO_printf(bp, "%*spub:\n", indent, "") <= 0)
        return 0;
    if (ASN1_buf_print(bp, xkey->pubkey, X25519_KEYLEN, indent + 4) == 0)
        return 0;
    return 1;
}

static int ecx_priv_print(BIO *bp, const EVP_PKEY *pkey, int indent,
                          ASN1_PCTX *ctx)
{
    return ecx_key_print(bp, pkey, indent, ctx, X25519_PRIVATE);
}

static int ecx_pub_print(BIO *bp, const EVP_PKEY *pkey, int indent,
                         ASN1_PCTX *ctx)
{
    return ecx_key_print(bp, pkey, indent, ctx, X25519_PUBLIC);
}

static int ecx_ctrl(EVP_PKEY *pkey, int op, long arg1, void *arg2)
{
    switch (op) {

    case ASN1_PKEY_CTRL_SET1_TLS_ENCPT:
        return ecx_key_op(pkey, NULL, arg2, arg1, X25519_PUBLIC);

    case ASN1_PKEY_CTRL_GET1_TLS_ENCPT:
        if (pkey->pkey.ptr != NULL) {
            const X25519_KEY *xkey = pkey->pkey.ptr;
            unsigned char **ppt = arg2;
            *ppt = OPENSSL_memdup(xkey->pubkey, X25519_KEYLEN);
            if (*ppt != NULL)
                return X25519_KEYLEN;
        }
        return 0;

    case ASN1_PKEY_CTRL_DEFAULT_MD_NID:
        *(int *)arg2 = NID_sha256;
        return 2;

    default:
        return -2;

    }
}

const EVP_PKEY_ASN1_METHOD ecx25519_asn1_meth = {
    NID_X25519,
    NID_X25519,
    0,
    "X25519",
    "OpenSSL X25519 algorithm",

    ecx_pub_decode,
    ecx_pub_encode,
    ecx_pub_cmp,
    ecx_pub_print,

    ecx_priv_decode,
    ecx_priv_encode,
    ecx_priv_print,

    ecx_size,
    ecx_bits,
    ecx_security_bits,

    0, 0, 0, 0,
    ecx_cmp_parameters,
    0, 0,

    ecx_free,
    ecx_ctrl,
    NULL,
    NULL
};

static int pkey_ecx_keygen(EVP_PKEY_CTX *ctx, EVP_PKEY *pkey)
{
    return ecx_key_op(pkey, NULL, NULL, 0, X25519_KEYGEN);
}

static int pkey_ecx_derive(EVP_PKEY_CTX *ctx, unsigned char *key,
                           size_t *keylen)
{
    const X25519_KEY *pkey, *peerkey;

    if (ctx->pkey == NULL || ctx->peerkey == NULL) {
        ECerr(EC_F_PKEY_ECX_DERIVE, EC_R_KEYS_NOT_SET);
        return 0;
    }
    pkey = ctx->pkey->pkey.ptr;
    peerkey = ctx->peerkey->pkey.ptr;
    if (pkey == NULL || pkey->privkey == NULL) {
        ECerr(EC_F_PKEY_ECX_DERIVE, EC_R_INVALID_PRIVATE_KEY);
        return 0;
    }
    if (peerkey == NULL) {
        ECerr(EC_F_PKEY_ECX_DERIVE, EC_R_INVALID_PEER_KEY);
        return 0;
    }
    *keylen = X25519_KEYLEN;
    if (key != NULL && X25519(key, pkey->privkey, peerkey->pubkey) == 0)
        return 0;
    return 1;
}

static int pkey_ecx_ctrl(EVP_PKEY_CTX *ctx, int type, int p1, void *p2)
{
    /* Only need to handle peer key for derivation */
    if (type == EVP_PKEY_CTRL_PEER_KEY)
        return 1;
    return -2;
}

const EVP_PKEY_METHOD ecx25519_pkey_meth = {
    NID_X25519,
    0, 0, 0, 0, 0, 0, 0,
    pkey_ecx_keygen,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    pkey_ecx_derive,
    pkey_ecx_ctrl,
    0
};
