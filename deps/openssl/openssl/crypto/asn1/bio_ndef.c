/*
 * Copyright 2008-2023 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/asn1.h>
#include <openssl/asn1t.h>
#include <openssl/bio.h>
#include <openssl/err.h>

#include <stdio.h>

/* Experimental NDEF ASN1 BIO support routines */

/*
 * The usage is quite simple, initialize an ASN1 structure, get a BIO from it
 * then any data written through the BIO will end up translated to
 * appropriate format on the fly. The data is streamed out and does *not*
 * need to be all held in memory at once. When the BIO is flushed the output
 * is finalized and any signatures etc written out. The BIO is a 'proper'
 * BIO and can handle non blocking I/O correctly. The usage is simple. The
 * implementation is *not*...
 */

/* BIO support data stored in the ASN1 BIO ex_arg */

typedef struct ndef_aux_st {
    /* ASN1 structure this BIO refers to */
    ASN1_VALUE *val;
    const ASN1_ITEM *it;
    /* Top of the BIO chain */
    BIO *ndef_bio;
    /* Output BIO */
    BIO *out;
    /* Boundary where content is inserted */
    unsigned char **boundary;
    /* DER buffer start */
    unsigned char *derbuf;
} NDEF_SUPPORT;

static int ndef_prefix(BIO *b, unsigned char **pbuf, int *plen, void *parg);
static int ndef_prefix_free(BIO *b, unsigned char **pbuf, int *plen,
                            void *parg);
static int ndef_suffix(BIO *b, unsigned char **pbuf, int *plen, void *parg);
static int ndef_suffix_free(BIO *b, unsigned char **pbuf, int *plen,
                            void *parg);

/*
 * On success, the returned BIO owns the input BIO as part of its BIO chain.
 * On failure, NULL is returned and the input BIO is owned by the caller.
 *
 * Unfortunately cannot constify this due to CMS_stream() and PKCS7_stream()
 */
BIO *BIO_new_NDEF(BIO *out, ASN1_VALUE *val, const ASN1_ITEM *it)
{
    NDEF_SUPPORT *ndef_aux = NULL;
    BIO *asn_bio = NULL;
    const ASN1_AUX *aux = it->funcs;
    ASN1_STREAM_ARG sarg;
    BIO *pop_bio = NULL;

    if (!aux || !aux->asn1_cb) {
        ERR_raise(ERR_LIB_ASN1, ASN1_R_STREAMING_NOT_SUPPORTED);
        return NULL;
    }
    ndef_aux = OPENSSL_zalloc(sizeof(*ndef_aux));
    asn_bio = BIO_new(BIO_f_asn1());
    if (ndef_aux == NULL || asn_bio == NULL)
        goto err;

    /* ASN1 bio needs to be next to output BIO */
    out = BIO_push(asn_bio, out);
    if (out == NULL)
        goto err;
    pop_bio = asn_bio;

    if (BIO_asn1_set_prefix(asn_bio, ndef_prefix, ndef_prefix_free) <= 0
            || BIO_asn1_set_suffix(asn_bio, ndef_suffix, ndef_suffix_free) <= 0
            || BIO_ctrl(asn_bio, BIO_C_SET_EX_ARG, 0, ndef_aux) <= 0)
        goto err;

    /*
     * Now let the callback prepend any digest, cipher, etc., that the BIO's
     * ASN1 structure needs.
     */

    sarg.out = out;
    sarg.ndef_bio = NULL;
    sarg.boundary = NULL;

    /*
     * The asn1_cb(), must not have mutated asn_bio on error, leaving it in the
     * middle of some partially built, but not returned BIO chain.
     */
    if (aux->asn1_cb(ASN1_OP_STREAM_PRE, &val, it, &sarg) <= 0) {
        /*
         * ndef_aux is now owned by asn_bio so we must not free it in the err
         * clean up block
         */
        ndef_aux = NULL;
        goto err;
    }

    /*
     * We must not fail now because the callback has prepended additional
     * BIOs to the chain
     */

    ndef_aux->val = val;
    ndef_aux->it = it;
    ndef_aux->ndef_bio = sarg.ndef_bio;
    ndef_aux->boundary = sarg.boundary;
    ndef_aux->out = out;

    return sarg.ndef_bio;

 err:
    /* BIO_pop() is NULL safe */
    (void)BIO_pop(pop_bio);
    BIO_free(asn_bio);
    OPENSSL_free(ndef_aux);
    return NULL;
}

static int ndef_prefix(BIO *b, unsigned char **pbuf, int *plen, void *parg)
{
    NDEF_SUPPORT *ndef_aux;
    unsigned char *p;
    int derlen;

    if (parg == NULL)
        return 0;

    ndef_aux = *(NDEF_SUPPORT **)parg;

    derlen = ASN1_item_ndef_i2d(ndef_aux->val, NULL, ndef_aux->it);
    if (derlen < 0)
        return 0;
    if ((p = OPENSSL_malloc(derlen)) == NULL) {
        ERR_raise(ERR_LIB_ASN1, ERR_R_MALLOC_FAILURE);
        return 0;
    }

    ndef_aux->derbuf = p;
    *pbuf = p;
    ASN1_item_ndef_i2d(ndef_aux->val, &p, ndef_aux->it);

    if (*ndef_aux->boundary == NULL)
        return 0;

    *plen = *ndef_aux->boundary - *pbuf;

    return 1;
}

static int ndef_prefix_free(BIO *b, unsigned char **pbuf, int *plen,
                            void *parg)
{
    NDEF_SUPPORT *ndef_aux;

    if (parg == NULL)
        return 0;

    ndef_aux = *(NDEF_SUPPORT **)parg;

    if (ndef_aux == NULL)
        return 0;

    OPENSSL_free(ndef_aux->derbuf);

    ndef_aux->derbuf = NULL;
    *pbuf = NULL;
    *plen = 0;
    return 1;
}

static int ndef_suffix_free(BIO *b, unsigned char **pbuf, int *plen,
                            void *parg)
{
    NDEF_SUPPORT **pndef_aux = (NDEF_SUPPORT **)parg;
    if (!ndef_prefix_free(b, pbuf, plen, parg))
        return 0;
    OPENSSL_free(*pndef_aux);
    *pndef_aux = NULL;
    return 1;
}

static int ndef_suffix(BIO *b, unsigned char **pbuf, int *plen, void *parg)
{
    NDEF_SUPPORT *ndef_aux;
    unsigned char *p;
    int derlen;
    const ASN1_AUX *aux;
    ASN1_STREAM_ARG sarg;

    if (parg == NULL)
        return 0;

    ndef_aux = *(NDEF_SUPPORT **)parg;

    aux = ndef_aux->it->funcs;

    /* Finalize structures */
    sarg.ndef_bio = ndef_aux->ndef_bio;
    sarg.out = ndef_aux->out;
    sarg.boundary = ndef_aux->boundary;
    if (aux->asn1_cb(ASN1_OP_STREAM_POST,
                     &ndef_aux->val, ndef_aux->it, &sarg) <= 0)
        return 0;

    derlen = ASN1_item_ndef_i2d(ndef_aux->val, NULL, ndef_aux->it);
    if (derlen < 0)
        return 0;
    if ((p = OPENSSL_malloc(derlen)) == NULL) {
        ERR_raise(ERR_LIB_ASN1, ERR_R_MALLOC_FAILURE);
        return 0;
    }

    ndef_aux->derbuf = p;
    *pbuf = p;
    derlen = ASN1_item_ndef_i2d(ndef_aux->val, &p, ndef_aux->it);

    if (*ndef_aux->boundary == NULL)
        return 0;
    *pbuf = *ndef_aux->boundary;
    *plen = derlen - (*ndef_aux->boundary - ndef_aux->derbuf);

    return 1;
}
