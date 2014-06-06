/* crypto/cms/cms_cd.c */
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

#include "cryptlib.h"
#include <openssl/asn1t.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>
#include <openssl/cms.h>
#include <openssl/bio.h>
#ifndef OPENSSL_NO_COMP
#include <openssl/comp.h>
#endif
#include "cms_lcl.h"

DECLARE_ASN1_ITEM(CMS_CompressedData)

#ifdef ZLIB

/* CMS CompressedData Utilities */

CMS_ContentInfo *cms_CompressedData_create(int comp_nid)
	{
	CMS_ContentInfo *cms;
	CMS_CompressedData *cd;
	/* Will need something cleverer if there is ever more than one
	 * compression algorithm or parameters have some meaning...
	 */
	if (comp_nid != NID_zlib_compression)
		{
		CMSerr(CMS_F_CMS_COMPRESSEDDATA_CREATE,
				CMS_R_UNSUPPORTED_COMPRESSION_ALGORITHM);
		return NULL;
		}
	cms = CMS_ContentInfo_new();
	if (!cms)
		return NULL;

	cd = M_ASN1_new_of(CMS_CompressedData);

	if (!cd)
		goto err;

	cms->contentType = OBJ_nid2obj(NID_id_smime_ct_compressedData);
	cms->d.compressedData = cd;

	cd->version = 0;

	X509_ALGOR_set0(cd->compressionAlgorithm,
			OBJ_nid2obj(NID_zlib_compression),
			V_ASN1_UNDEF, NULL);

	cd->encapContentInfo->eContentType = OBJ_nid2obj(NID_pkcs7_data);

	return cms;

	err:

	if (cms)
		CMS_ContentInfo_free(cms);

	return NULL;
	}

BIO *cms_CompressedData_init_bio(CMS_ContentInfo *cms)
	{
	CMS_CompressedData *cd;
	ASN1_OBJECT *compoid;
	if (OBJ_obj2nid(cms->contentType) != NID_id_smime_ct_compressedData)
		{
		CMSerr(CMS_F_CMS_COMPRESSEDDATA_INIT_BIO,
				CMS_R_CONTENT_TYPE_NOT_COMPRESSED_DATA);
		return NULL;
		}
	cd = cms->d.compressedData;
	X509_ALGOR_get0(&compoid, NULL, NULL, cd->compressionAlgorithm);
	if (OBJ_obj2nid(compoid) != NID_zlib_compression)
		{
		CMSerr(CMS_F_CMS_COMPRESSEDDATA_INIT_BIO,
				CMS_R_UNSUPPORTED_COMPRESSION_ALGORITHM);
		return NULL;
		}
	return BIO_new(BIO_f_zlib());
	}

#endif
