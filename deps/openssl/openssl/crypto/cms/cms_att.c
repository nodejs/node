/* crypto/cms/cms_att.c */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
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
#include <openssl/pem.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>
#include "cms.h"
#include "cms_lcl.h"

/* CMS SignedData Attribute utilities */

int CMS_signed_get_attr_count(const CMS_SignerInfo *si)
{
	return X509at_get_attr_count(si->signedAttrs);
}

int CMS_signed_get_attr_by_NID(const CMS_SignerInfo *si, int nid,
			  int lastpos)
{
	return X509at_get_attr_by_NID(si->signedAttrs, nid, lastpos);
}

int CMS_signed_get_attr_by_OBJ(const CMS_SignerInfo *si, ASN1_OBJECT *obj,
			  int lastpos)
{
	return X509at_get_attr_by_OBJ(si->signedAttrs, obj, lastpos);
}

X509_ATTRIBUTE *CMS_signed_get_attr(const CMS_SignerInfo *si, int loc)
{
	return X509at_get_attr(si->signedAttrs, loc);
}

X509_ATTRIBUTE *CMS_signed_delete_attr(CMS_SignerInfo *si, int loc)
{
	return X509at_delete_attr(si->signedAttrs, loc);
}

int CMS_signed_add1_attr(CMS_SignerInfo *si, X509_ATTRIBUTE *attr)
{
	if(X509at_add1_attr(&si->signedAttrs, attr)) return 1;
	return 0;
}

int CMS_signed_add1_attr_by_OBJ(CMS_SignerInfo *si,
			const ASN1_OBJECT *obj, int type,
			const void *bytes, int len)
{
	if(X509at_add1_attr_by_OBJ(&si->signedAttrs, obj,
				type, bytes, len)) return 1;
	return 0;
}

int CMS_signed_add1_attr_by_NID(CMS_SignerInfo *si,
			int nid, int type,
			const void *bytes, int len)
{
	if(X509at_add1_attr_by_NID(&si->signedAttrs, nid,
				type, bytes, len)) return 1;
	return 0;
}

int CMS_signed_add1_attr_by_txt(CMS_SignerInfo *si,
			const char *attrname, int type,
			const void *bytes, int len)
{
	if(X509at_add1_attr_by_txt(&si->signedAttrs, attrname,
				type, bytes, len)) return 1;
	return 0;
}

void *CMS_signed_get0_data_by_OBJ(CMS_SignerInfo *si, ASN1_OBJECT *oid,
					int lastpos, int type)
{
	return X509at_get0_data_by_OBJ(si->signedAttrs, oid, lastpos, type);
}

int CMS_unsigned_get_attr_count(const CMS_SignerInfo *si)
{
	return X509at_get_attr_count(si->unsignedAttrs);
}

int CMS_unsigned_get_attr_by_NID(const CMS_SignerInfo *si, int nid,
			  int lastpos)
{
	return X509at_get_attr_by_NID(si->unsignedAttrs, nid, lastpos);
}

int CMS_unsigned_get_attr_by_OBJ(const CMS_SignerInfo *si, ASN1_OBJECT *obj,
			  int lastpos)
{
	return X509at_get_attr_by_OBJ(si->unsignedAttrs, obj, lastpos);
}

X509_ATTRIBUTE *CMS_unsigned_get_attr(const CMS_SignerInfo *si, int loc)
{
	return X509at_get_attr(si->unsignedAttrs, loc);
}

X509_ATTRIBUTE *CMS_unsigned_delete_attr(CMS_SignerInfo *si, int loc)
{
	return X509at_delete_attr(si->unsignedAttrs, loc);
}

int CMS_unsigned_add1_attr(CMS_SignerInfo *si, X509_ATTRIBUTE *attr)
{
	if(X509at_add1_attr(&si->unsignedAttrs, attr)) return 1;
	return 0;
}

int CMS_unsigned_add1_attr_by_OBJ(CMS_SignerInfo *si,
			const ASN1_OBJECT *obj, int type,
			const void *bytes, int len)
{
	if(X509at_add1_attr_by_OBJ(&si->unsignedAttrs, obj,
				type, bytes, len)) return 1;
	return 0;
}

int CMS_unsigned_add1_attr_by_NID(CMS_SignerInfo *si,
			int nid, int type,
			const void *bytes, int len)
{
	if(X509at_add1_attr_by_NID(&si->unsignedAttrs, nid,
				type, bytes, len)) return 1;
	return 0;
}

int CMS_unsigned_add1_attr_by_txt(CMS_SignerInfo *si,
			const char *attrname, int type,
			const void *bytes, int len)
{
	if(X509at_add1_attr_by_txt(&si->unsignedAttrs, attrname,
				type, bytes, len)) return 1;
	return 0;
}

void *CMS_unsigned_get0_data_by_OBJ(CMS_SignerInfo *si, ASN1_OBJECT *oid,
					int lastpos, int type)
{
	return X509at_get0_data_by_OBJ(si->unsignedAttrs, oid, lastpos, type);
}

/* Specific attribute cases */
