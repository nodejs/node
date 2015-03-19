/* bio_ndef.c */
/*
 * Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project.
 */
/* ====================================================================
 * Copyright (c) 2008 The OpenSSL Project.  All rights reserved.
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
 * approptiate format on the fly. The data is streamed out and does *not*
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

BIO *BIO_new_NDEF(BIO *out, ASN1_VALUE *val, const ASN1_ITEM *it)
{
    NDEF_SUPPORT *ndef_aux = NULL;
    BIO *asn_bio = NULL;
    const ASN1_AUX *aux = it->funcs;
    ASN1_STREAM_ARG sarg;

    if (!aux || !aux->asn1_cb) {
        ASN1err(ASN1_F_BIO_NEW_NDEF, ASN1_R_STREAMING_NOT_SUPPORTED);
        return NULL;
    }
    ndef_aux = OPENSSL_malloc(sizeof(NDEF_SUPPORT));
    asn_bio = BIO_new(BIO_f_asn1());

    /* ASN1 bio needs to be next to output BIO */

    out = BIO_push(asn_bio, out);

    if (!ndef_aux || !asn_bio || !out)
        goto err;

    BIO_asn1_set_prefix(asn_bio, ndef_prefix, ndef_prefix_free);
    BIO_asn1_set_suffix(asn_bio, ndef_suffix, ndef_suffix_free);

    /*
     * Now let callback prepend any digest, cipher etc BIOs ASN1 structure
     * needs.
     */

    sarg.out = out;
    sarg.ndef_bio = NULL;
    sarg.boundary = NULL;

    if (aux->asn1_cb(ASN1_OP_STREAM_PRE, &val, it, &sarg) <= 0)
        goto err;

    ndef_aux->val = val;
    ndef_aux->it = it;
    ndef_aux->ndef_bio = sarg.ndef_bio;
    ndef_aux->boundary = sarg.boundary;
    ndef_aux->out = out;

    BIO_ctrl(asn_bio, BIO_C_SET_EX_ARG, 0, ndef_aux);

    return sarg.ndef_bio;

 err:
    if (asn_bio)
        BIO_free(asn_bio);
    if (ndef_aux)
        OPENSSL_free(ndef_aux);
    return NULL;
}

static int ndef_prefix(BIO *b, unsigned char **pbuf, int *plen, void *parg)
{
    NDEF_SUPPORT *ndef_aux;
    unsigned char *p;
    int derlen;

    if (!parg)
        return 0;

    ndef_aux = *(NDEF_SUPPORT **)parg;

    derlen = ASN1_item_ndef_i2d(ndef_aux->val, NULL, ndef_aux->it);
    p = OPENSSL_malloc(derlen);
    if(!p)
        return 0;

    ndef_aux->derbuf = p;
    *pbuf = p;
    derlen = ASN1_item_ndef_i2d(ndef_aux->val, &p, ndef_aux->it);

    if (!*ndef_aux->boundary)
        return 0;

    *plen = *ndef_aux->boundary - *pbuf;

    return 1;
}

static int ndef_prefix_free(BIO *b, unsigned char **pbuf, int *plen,
                            void *parg)
{
    NDEF_SUPPORT *ndef_aux;

    if (!parg)
        return 0;

    ndef_aux = *(NDEF_SUPPORT **)parg;

    if (ndef_aux->derbuf)
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

    if (!parg)
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
    p = OPENSSL_malloc(derlen);
    if(!p)
        return 0;

    ndef_aux->derbuf = p;
    *pbuf = p;
    derlen = ASN1_item_ndef_i2d(ndef_aux->val, &p, ndef_aux->it);

    if (!*ndef_aux->boundary)
        return 0;
    *pbuf = *ndef_aux->boundary;
    *plen = derlen - (*ndef_aux->boundary - ndef_aux->derbuf);

    return 1;
}
