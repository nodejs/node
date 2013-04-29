/* crypto/cms/cms_lcl.h */
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

#ifndef HEADER_CMS_LCL_H
#define HEADER_CMS_LCL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <openssl/x509.h>

/* Cryptographic message syntax (CMS) structures: taken
 * from RFC3852
 */

/* Forward references */

typedef struct CMS_IssuerAndSerialNumber_st CMS_IssuerAndSerialNumber;
typedef struct CMS_EncapsulatedContentInfo_st CMS_EncapsulatedContentInfo;
typedef struct CMS_SignerIdentifier_st CMS_SignerIdentifier;
typedef struct CMS_SignedData_st CMS_SignedData;
typedef struct CMS_OtherRevocationInfoFormat_st CMS_OtherRevocationInfoFormat;
typedef struct CMS_OriginatorInfo_st CMS_OriginatorInfo;
typedef struct CMS_EncryptedContentInfo_st CMS_EncryptedContentInfo;
typedef struct CMS_EnvelopedData_st CMS_EnvelopedData;
typedef struct CMS_DigestedData_st CMS_DigestedData;
typedef struct CMS_EncryptedData_st CMS_EncryptedData;
typedef struct CMS_AuthenticatedData_st CMS_AuthenticatedData;
typedef struct CMS_CompressedData_st CMS_CompressedData;
typedef struct CMS_OtherCertificateFormat_st CMS_OtherCertificateFormat;
typedef struct CMS_KeyTransRecipientInfo_st CMS_KeyTransRecipientInfo;
typedef struct CMS_OriginatorPublicKey_st CMS_OriginatorPublicKey;
typedef struct CMS_OriginatorIdentifierOrKey_st CMS_OriginatorIdentifierOrKey;
typedef struct CMS_KeyAgreeRecipientInfo_st CMS_KeyAgreeRecipientInfo;
typedef struct CMS_OtherKeyAttribute_st CMS_OtherKeyAttribute;
typedef struct CMS_RecipientKeyIdentifier_st CMS_RecipientKeyIdentifier;
typedef struct CMS_KeyAgreeRecipientIdentifier_st CMS_KeyAgreeRecipientIdentifier;
typedef struct CMS_RecipientEncryptedKey_st CMS_RecipientEncryptedKey;
typedef struct CMS_KEKIdentifier_st CMS_KEKIdentifier;
typedef struct CMS_KEKRecipientInfo_st CMS_KEKRecipientInfo;
typedef struct CMS_PasswordRecipientInfo_st CMS_PasswordRecipientInfo;
typedef struct CMS_OtherRecipientInfo_st CMS_OtherRecipientInfo;
typedef struct CMS_ReceiptsFrom_st CMS_ReceiptsFrom;

struct CMS_ContentInfo_st
	{
	ASN1_OBJECT *contentType;
	union	{
		ASN1_OCTET_STRING *data;
		CMS_SignedData *signedData;
		CMS_EnvelopedData *envelopedData;
		CMS_DigestedData *digestedData;
		CMS_EncryptedData *encryptedData;
		CMS_AuthenticatedData *authenticatedData;
		CMS_CompressedData *compressedData;
		ASN1_TYPE *other;
		/* Other types ... */
		void *otherData;
		} d;
	};

struct CMS_SignedData_st
	{
	long version;
	STACK_OF(X509_ALGOR) *digestAlgorithms;
	CMS_EncapsulatedContentInfo *encapContentInfo;
	STACK_OF(CMS_CertificateChoices) *certificates;
	STACK_OF(CMS_RevocationInfoChoice) *crls;
	STACK_OF(CMS_SignerInfo) *signerInfos;
	};
 
struct CMS_EncapsulatedContentInfo_st
	{
	ASN1_OBJECT *eContentType;
	ASN1_OCTET_STRING *eContent;
	/* Set to 1 if incomplete structure only part set up */
	int partial;
	};

struct CMS_SignerInfo_st
	{
	long version;
	CMS_SignerIdentifier *sid;
	X509_ALGOR *digestAlgorithm;
	STACK_OF(X509_ATTRIBUTE) *signedAttrs;
	X509_ALGOR *signatureAlgorithm;
	ASN1_OCTET_STRING *signature;
	STACK_OF(X509_ATTRIBUTE) *unsignedAttrs;
	/* Signing certificate and key */
	X509 *signer;
	EVP_PKEY *pkey;
	};

struct CMS_SignerIdentifier_st
	{
	int type;
	union	{
		CMS_IssuerAndSerialNumber *issuerAndSerialNumber;
		ASN1_OCTET_STRING *subjectKeyIdentifier;
		} d;
	};

struct CMS_EnvelopedData_st
	{
	long version;
	CMS_OriginatorInfo *originatorInfo;
	STACK_OF(CMS_RecipientInfo) *recipientInfos;
	CMS_EncryptedContentInfo *encryptedContentInfo;
	STACK_OF(X509_ATTRIBUTE) *unprotectedAttrs;
	};

struct CMS_OriginatorInfo_st
	{
	STACK_OF(CMS_CertificateChoices) *certificates;
	STACK_OF(CMS_RevocationInfoChoice) *crls;
	};

struct CMS_EncryptedContentInfo_st
	{
	ASN1_OBJECT *contentType;
	X509_ALGOR *contentEncryptionAlgorithm;
	ASN1_OCTET_STRING *encryptedContent;
	/* Content encryption algorithm and key */
	const EVP_CIPHER *cipher;
	unsigned char *key;
	size_t keylen;
	};

struct CMS_RecipientInfo_st
	{
 	int type;
 	union	{
  	 	CMS_KeyTransRecipientInfo *ktri;
   		CMS_KeyAgreeRecipientInfo *kari;
   		CMS_KEKRecipientInfo *kekri;
		CMS_PasswordRecipientInfo *pwri;
		CMS_OtherRecipientInfo *ori;
		} d;
	};

typedef CMS_SignerIdentifier CMS_RecipientIdentifier;

struct CMS_KeyTransRecipientInfo_st
	{
	long version;
	CMS_RecipientIdentifier *rid;
	X509_ALGOR *keyEncryptionAlgorithm;
	ASN1_OCTET_STRING *encryptedKey;
	/* Recipient Key and cert */
	X509 *recip;
	EVP_PKEY *pkey;
	};

struct CMS_KeyAgreeRecipientInfo_st
	{
	long version;
	CMS_OriginatorIdentifierOrKey *originator;
	ASN1_OCTET_STRING *ukm;
 	X509_ALGOR *keyEncryptionAlgorithm;
	STACK_OF(CMS_RecipientEncryptedKey) *recipientEncryptedKeys;
	};

struct CMS_OriginatorIdentifierOrKey_st
	{
	int type;
	union	{
		CMS_IssuerAndSerialNumber *issuerAndSerialNumber;
		ASN1_OCTET_STRING *subjectKeyIdentifier;
		CMS_OriginatorPublicKey *originatorKey;
		} d;
	};

struct CMS_OriginatorPublicKey_st
	{
	X509_ALGOR *algorithm;
	ASN1_BIT_STRING *publicKey;
	};

struct CMS_RecipientEncryptedKey_st
	{
 	CMS_KeyAgreeRecipientIdentifier *rid;
 	ASN1_OCTET_STRING *encryptedKey;
	};

struct CMS_KeyAgreeRecipientIdentifier_st
	{
	int type;
	union	{
		CMS_IssuerAndSerialNumber *issuerAndSerialNumber;
		CMS_RecipientKeyIdentifier *rKeyId;
		} d;
	};

struct CMS_RecipientKeyIdentifier_st
	{
 	ASN1_OCTET_STRING *subjectKeyIdentifier;
 	ASN1_GENERALIZEDTIME *date;
 	CMS_OtherKeyAttribute *other;
	};

struct CMS_KEKRecipientInfo_st
	{
 	long version;
 	CMS_KEKIdentifier *kekid;
 	X509_ALGOR *keyEncryptionAlgorithm;
 	ASN1_OCTET_STRING *encryptedKey;
	/* Extra info: symmetric key to use */
	unsigned char *key;
	size_t keylen;
	};

struct CMS_KEKIdentifier_st
	{
 	ASN1_OCTET_STRING *keyIdentifier;
 	ASN1_GENERALIZEDTIME *date;
 	CMS_OtherKeyAttribute *other;
	};

struct CMS_PasswordRecipientInfo_st
	{
 	long version;
 	X509_ALGOR *keyDerivationAlgorithm;
 	X509_ALGOR *keyEncryptionAlgorithm;
 	ASN1_OCTET_STRING *encryptedKey;
	};

struct CMS_OtherRecipientInfo_st
	{
 	ASN1_OBJECT *oriType;
 	ASN1_TYPE *oriValue;
	};

struct CMS_DigestedData_st
	{
	long version;
	X509_ALGOR *digestAlgorithm;
	CMS_EncapsulatedContentInfo *encapContentInfo;
	ASN1_OCTET_STRING *digest;
	};

struct CMS_EncryptedData_st
	{
	long version;
	CMS_EncryptedContentInfo *encryptedContentInfo;
	STACK_OF(X509_ATTRIBUTE) *unprotectedAttrs;
	};

struct CMS_AuthenticatedData_st
	{
	long version;
	CMS_OriginatorInfo *originatorInfo;
	STACK_OF(CMS_RecipientInfo) *recipientInfos;
	X509_ALGOR *macAlgorithm;
	X509_ALGOR *digestAlgorithm;
	CMS_EncapsulatedContentInfo *encapContentInfo;
	STACK_OF(X509_ATTRIBUTE) *authAttrs;
	ASN1_OCTET_STRING *mac;
	STACK_OF(X509_ATTRIBUTE) *unauthAttrs;
	};

struct CMS_CompressedData_st
	{
	long version;
	X509_ALGOR *compressionAlgorithm;
	STACK_OF(CMS_RecipientInfo) *recipientInfos;
	CMS_EncapsulatedContentInfo *encapContentInfo;
	};

struct CMS_RevocationInfoChoice_st
	{
	int type;
	union	{
		X509_CRL *crl;
		CMS_OtherRevocationInfoFormat *other;
		} d;
	};

#define CMS_REVCHOICE_CRL		0
#define CMS_REVCHOICE_OTHER		1

struct CMS_OtherRevocationInfoFormat_st
	{
	ASN1_OBJECT *otherRevInfoFormat;
 	ASN1_TYPE *otherRevInfo;
	};

struct CMS_CertificateChoices
	{
	int type;
		union {
		X509 *certificate;
		ASN1_STRING *extendedCertificate;	/* Obsolete */
		ASN1_STRING *v1AttrCert;	/* Left encoded for now */
		ASN1_STRING *v2AttrCert;	/* Left encoded for now */
		CMS_OtherCertificateFormat *other;
		} d;
	};

#define CMS_CERTCHOICE_CERT		0
#define CMS_CERTCHOICE_EXCERT		1
#define CMS_CERTCHOICE_V1ACERT		2
#define CMS_CERTCHOICE_V2ACERT		3
#define CMS_CERTCHOICE_OTHER		4

struct CMS_OtherCertificateFormat_st
	{
	ASN1_OBJECT *otherCertFormat;
 	ASN1_TYPE *otherCert;
	};

/* This is also defined in pkcs7.h but we duplicate it
 * to allow the CMS code to be independent of PKCS#7
 */

struct CMS_IssuerAndSerialNumber_st
	{
	X509_NAME *issuer;
	ASN1_INTEGER *serialNumber;
	};

struct CMS_OtherKeyAttribute_st
	{
	ASN1_OBJECT *keyAttrId;
 	ASN1_TYPE *keyAttr;
	};

/* ESS structures */

#ifdef HEADER_X509V3_H

struct CMS_ReceiptRequest_st
	{
	ASN1_OCTET_STRING *signedContentIdentifier;
	CMS_ReceiptsFrom *receiptsFrom;
	STACK_OF(GENERAL_NAMES) *receiptsTo;
	};


struct CMS_ReceiptsFrom_st
	{
	int type;
	union
		{
		long allOrFirstTier;
		STACK_OF(GENERAL_NAMES) *receiptList;
		} d;
	};
#endif

struct CMS_Receipt_st
	{
	long version;
	ASN1_OBJECT *contentType;
	ASN1_OCTET_STRING *signedContentIdentifier;
	ASN1_OCTET_STRING *originatorSignatureValue;
	};

DECLARE_ASN1_FUNCTIONS(CMS_ContentInfo)
DECLARE_ASN1_ITEM(CMS_SignerInfo)
DECLARE_ASN1_ITEM(CMS_IssuerAndSerialNumber)
DECLARE_ASN1_ITEM(CMS_Attributes_Sign)
DECLARE_ASN1_ITEM(CMS_Attributes_Verify)
DECLARE_ASN1_ALLOC_FUNCTIONS(CMS_IssuerAndSerialNumber)

#define CMS_SIGNERINFO_ISSUER_SERIAL	0
#define CMS_SIGNERINFO_KEYIDENTIFIER	1

#define CMS_RECIPINFO_ISSUER_SERIAL	0
#define CMS_RECIPINFO_KEYIDENTIFIER	1

BIO *cms_content_bio(CMS_ContentInfo *cms);

CMS_ContentInfo *cms_Data_create(void);

CMS_ContentInfo *cms_DigestedData_create(const EVP_MD *md);
BIO *cms_DigestedData_init_bio(CMS_ContentInfo *cms);
int cms_DigestedData_do_final(CMS_ContentInfo *cms, BIO *chain, int verify);

BIO *cms_SignedData_init_bio(CMS_ContentInfo *cms);
int cms_SignedData_final(CMS_ContentInfo *cms, BIO *chain);
int cms_set1_SignerIdentifier(CMS_SignerIdentifier *sid, X509 *cert, int type);
int cms_SignerIdentifier_get0_signer_id(CMS_SignerIdentifier *sid,
					ASN1_OCTET_STRING **keyid,
					X509_NAME **issuer, ASN1_INTEGER **sno);
int cms_SignerIdentifier_cert_cmp(CMS_SignerIdentifier *sid, X509 *cert);

CMS_ContentInfo *cms_CompressedData_create(int comp_nid);
BIO *cms_CompressedData_init_bio(CMS_ContentInfo *cms);

void cms_DigestAlgorithm_set(X509_ALGOR *alg, const EVP_MD *md);
BIO *cms_DigestAlgorithm_init_bio(X509_ALGOR *digestAlgorithm);
int cms_DigestAlgorithm_find_ctx(EVP_MD_CTX *mctx, BIO *chain,
					X509_ALGOR *mdalg);

BIO *cms_EncryptedContent_init_bio(CMS_EncryptedContentInfo *ec);
BIO *cms_EncryptedData_init_bio(CMS_ContentInfo *cms);
int cms_EncryptedContent_init(CMS_EncryptedContentInfo *ec, 
				const EVP_CIPHER *cipher,
				const unsigned char *key, size_t keylen);

int cms_Receipt_verify(CMS_ContentInfo *cms, CMS_ContentInfo *req_cms);
int cms_msgSigDigest_add1(CMS_SignerInfo *dest, CMS_SignerInfo *src);
ASN1_OCTET_STRING *cms_encode_Receipt(CMS_SignerInfo *si);

BIO *cms_EnvelopedData_init_bio(CMS_ContentInfo *cms);
	
#ifdef  __cplusplus
}
#endif
#endif
