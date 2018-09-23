/* crypto/cms/cms_io.c */
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
 */

#include <openssl/asn1t.h>
#include <openssl/x509.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include "cms.h"
#include "cms_lcl.h"

int CMS_stream(unsigned char ***boundary, CMS_ContentInfo *cms)
{
    ASN1_OCTET_STRING **pos;
    pos = CMS_get0_content(cms);
    if (!pos)
        return 0;
    if (!*pos)
        *pos = ASN1_OCTET_STRING_new();
    if (*pos) {
        (*pos)->flags |= ASN1_STRING_FLAG_NDEF;
        (*pos)->flags &= ~ASN1_STRING_FLAG_CONT;
        *boundary = &(*pos)->data;
        return 1;
    }
    CMSerr(CMS_F_CMS_STREAM, ERR_R_MALLOC_FAILURE);
    return 0;
}

CMS_ContentInfo *d2i_CMS_bio(BIO *bp, CMS_ContentInfo **cms)
{
    return ASN1_item_d2i_bio(ASN1_ITEM_rptr(CMS_ContentInfo), bp, cms);
}

int i2d_CMS_bio(BIO *bp, CMS_ContentInfo *cms)
{
    return ASN1_item_i2d_bio(ASN1_ITEM_rptr(CMS_ContentInfo), bp, cms);
}

IMPLEMENT_PEM_rw_const(CMS, CMS_ContentInfo, PEM_STRING_CMS, CMS_ContentInfo)

BIO *BIO_new_CMS(BIO *out, CMS_ContentInfo *cms)
{
    return BIO_new_NDEF(out, (ASN1_VALUE *)cms,
                        ASN1_ITEM_rptr(CMS_ContentInfo));
}

/* CMS wrappers round generalised stream and MIME routines */

int i2d_CMS_bio_stream(BIO *out, CMS_ContentInfo *cms, BIO *in, int flags)
{
    return i2d_ASN1_bio_stream(out, (ASN1_VALUE *)cms, in, flags,
                               ASN1_ITEM_rptr(CMS_ContentInfo));
}

int PEM_write_bio_CMS_stream(BIO *out, CMS_ContentInfo *cms, BIO *in,
                             int flags)
{
    return PEM_write_bio_ASN1_stream(out, (ASN1_VALUE *)cms, in, flags,
                                     "CMS", ASN1_ITEM_rptr(CMS_ContentInfo));
}

int SMIME_write_CMS(BIO *bio, CMS_ContentInfo *cms, BIO *data, int flags)
{
    STACK_OF(X509_ALGOR) *mdalgs;
    int ctype_nid = OBJ_obj2nid(cms->contentType);
    int econt_nid = OBJ_obj2nid(CMS_get0_eContentType(cms));
    if (ctype_nid == NID_pkcs7_signed)
        mdalgs = cms->d.signedData->digestAlgorithms;
    else
        mdalgs = NULL;

    return SMIME_write_ASN1(bio, (ASN1_VALUE *)cms, data, flags,
                            ctype_nid, econt_nid, mdalgs,
                            ASN1_ITEM_rptr(CMS_ContentInfo));
}

CMS_ContentInfo *SMIME_read_CMS(BIO *bio, BIO **bcont)
{
    return (CMS_ContentInfo *)SMIME_read_ASN1(bio, bcont,
                                              ASN1_ITEM_rptr
                                              (CMS_ContentInfo));
}
