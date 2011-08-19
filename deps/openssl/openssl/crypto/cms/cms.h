/* crypto/cms/cms.h */
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


#ifndef HEADER_CMS_H
#define HEADER_CMS_H

#include <openssl/x509.h>

#ifdef OPENSSL_NO_CMS
#error CMS is disabled.
#endif

#ifdef __cplusplus
extern "C" {
#endif


typedef struct CMS_ContentInfo_st CMS_ContentInfo;
typedef struct CMS_SignerInfo_st CMS_SignerInfo;
typedef struct CMS_CertificateChoices CMS_CertificateChoices;
typedef struct CMS_RevocationInfoChoice_st CMS_RevocationInfoChoice;
typedef struct CMS_RecipientInfo_st CMS_RecipientInfo;
typedef struct CMS_ReceiptRequest_st CMS_ReceiptRequest;
typedef struct CMS_Receipt_st CMS_Receipt;

DECLARE_STACK_OF(CMS_SignerInfo)
DECLARE_STACK_OF(GENERAL_NAMES)
DECLARE_ASN1_FUNCTIONS_const(CMS_ContentInfo)
DECLARE_ASN1_FUNCTIONS_const(CMS_ReceiptRequest)

#define CMS_SIGNERINFO_ISSUER_SERIAL	0
#define CMS_SIGNERINFO_KEYIDENTIFIER	1

#define CMS_RECIPINFO_TRANS		0
#define CMS_RECIPINFO_AGREE		1
#define CMS_RECIPINFO_KEK		2
#define CMS_RECIPINFO_PASS		3
#define CMS_RECIPINFO_OTHER		4

/* S/MIME related flags */

#define CMS_TEXT			0x1
#define CMS_NOCERTS			0x2
#define CMS_NO_CONTENT_VERIFY		0x4
#define CMS_NO_ATTR_VERIFY		0x8
#define CMS_NOSIGS			\
			(CMS_NO_CONTENT_VERIFY|CMS_NO_ATTR_VERIFY)
#define CMS_NOINTERN			0x10
#define CMS_NO_SIGNER_CERT_VERIFY	0x20
#define CMS_NOVERIFY			0x20
#define CMS_DETACHED			0x40
#define CMS_BINARY			0x80
#define CMS_NOATTR			0x100
#define	CMS_NOSMIMECAP			0x200
#define CMS_NOOLDMIMETYPE		0x400
#define CMS_CRLFEOL			0x800
#define CMS_STREAM			0x1000
#define CMS_NOCRL			0x2000
#define CMS_PARTIAL			0x4000
#define CMS_REUSE_DIGEST		0x8000
#define CMS_USE_KEYID			0x10000

const ASN1_OBJECT *CMS_get0_type(CMS_ContentInfo *cms);

BIO *CMS_dataInit(CMS_ContentInfo *cms, BIO *icont);
int CMS_dataFinal(CMS_ContentInfo *cms, BIO *bio);

ASN1_OCTET_STRING **CMS_get0_content(CMS_ContentInfo *cms);
int CMS_is_detached(CMS_ContentInfo *cms);
int CMS_set_detached(CMS_ContentInfo *cms, int detached);

#ifdef HEADER_PEM_H
DECLARE_PEM_rw_const(CMS, CMS_ContentInfo)
#endif

CMS_ContentInfo *d2i_CMS_bio(BIO *bp, CMS_ContentInfo **cms);
int i2d_CMS_bio(BIO *bp, CMS_ContentInfo *cms);

CMS_ContentInfo *SMIME_read_CMS(BIO *bio, BIO **bcont);
int SMIME_write_CMS(BIO *bio, CMS_ContentInfo *cms, BIO *data, int flags);

int CMS_final(CMS_ContentInfo *cms, BIO *data, BIO *dcont, unsigned int flags);

CMS_ContentInfo *CMS_sign(X509 *signcert, EVP_PKEY *pkey, STACK_OF(X509) *certs,
						BIO *data, unsigned int flags);

CMS_ContentInfo *CMS_sign_receipt(CMS_SignerInfo *si,
					X509 *signcert, EVP_PKEY *pkey,
					STACK_OF(X509) *certs,
					unsigned int flags);

int CMS_data(CMS_ContentInfo *cms, BIO *out, unsigned int flags);
CMS_ContentInfo *CMS_data_create(BIO *in, unsigned int flags);

int CMS_digest_verify(CMS_ContentInfo *cms, BIO *dcont, BIO *out,
							unsigned int flags);
CMS_ContentInfo *CMS_digest_create(BIO *in, const EVP_MD *md,
							unsigned int flags);

int CMS_EncryptedData_decrypt(CMS_ContentInfo *cms,
				const unsigned char *key, size_t keylen,
				BIO *dcont, BIO *out, unsigned int flags);

CMS_ContentInfo *CMS_EncryptedData_encrypt(BIO *in, const EVP_CIPHER *cipher,
					const unsigned char *key, size_t keylen,
					unsigned int flags);

int CMS_EncryptedData_set1_key(CMS_ContentInfo *cms, const EVP_CIPHER *ciph,
				const unsigned char *key, size_t keylen);

int CMS_verify(CMS_ContentInfo *cms, STACK_OF(X509) *certs,
		 X509_STORE *store, BIO *dcont, BIO *out, unsigned int flags);

int CMS_verify_receipt(CMS_ContentInfo *rcms, CMS_ContentInfo *ocms,
			STACK_OF(X509) *certs,
			X509_STORE *store, unsigned int flags);

STACK_OF(X509) *CMS_get0_signers(CMS_ContentInfo *cms);

CMS_ContentInfo *CMS_encrypt(STACK_OF(X509) *certs, BIO *in,
				const EVP_CIPHER *cipher, unsigned int flags);

int CMS_decrypt(CMS_ContentInfo *cms, EVP_PKEY *pkey, X509 *cert,
				BIO *dcont, BIO *out,
				unsigned int flags);
	
int CMS_decrypt_set1_pkey(CMS_ContentInfo *cms, EVP_PKEY *pk, X509 *cert);
int CMS_decrypt_set1_key(CMS_ContentInfo *cms, 
				unsigned char *key, size_t keylen,
				unsigned char *id, size_t idlen);

STACK_OF(CMS_RecipientInfo) *CMS_get0_RecipientInfos(CMS_ContentInfo *cms);
int CMS_RecipientInfo_type(CMS_RecipientInfo *ri);
CMS_ContentInfo *CMS_EnvelopedData_create(const EVP_CIPHER *cipher);
CMS_RecipientInfo *CMS_add1_recipient_cert(CMS_ContentInfo *cms,
					X509 *recip, unsigned int flags);
int CMS_RecipientInfo_set0_pkey(CMS_RecipientInfo *ri, EVP_PKEY *pkey);
int CMS_RecipientInfo_ktri_cert_cmp(CMS_RecipientInfo *ri, X509 *cert);
int CMS_RecipientInfo_ktri_get0_algs(CMS_RecipientInfo *ri,
					EVP_PKEY **pk, X509 **recip,
					X509_ALGOR **palg);
int CMS_RecipientInfo_ktri_get0_signer_id(CMS_RecipientInfo *ri,
					ASN1_OCTET_STRING **keyid,
					X509_NAME **issuer, ASN1_INTEGER **sno);

CMS_RecipientInfo *CMS_add0_recipient_key(CMS_ContentInfo *cms, int nid,
					unsigned char *key, size_t keylen,
					unsigned char *id, size_t idlen,
					ASN1_GENERALIZEDTIME *date,
					ASN1_OBJECT *otherTypeId,
					ASN1_TYPE *otherType);

int CMS_RecipientInfo_kekri_get0_id(CMS_RecipientInfo *ri,
					X509_ALGOR **palg,
					ASN1_OCTET_STRING **pid,
					ASN1_GENERALIZEDTIME **pdate,
					ASN1_OBJECT **potherid,
					ASN1_TYPE **pothertype);

int CMS_RecipientInfo_set0_key(CMS_RecipientInfo *ri, 
				unsigned char *key, size_t keylen);

int CMS_RecipientInfo_kekri_id_cmp(CMS_RecipientInfo *ri, 
					const unsigned char *id, size_t idlen);

int CMS_RecipientInfo_decrypt(CMS_ContentInfo *cms, CMS_RecipientInfo *ri);
	
int CMS_uncompress(CMS_ContentInfo *cms, BIO *dcont, BIO *out,
							unsigned int flags);
CMS_ContentInfo *CMS_compress(BIO *in, int comp_nid, unsigned int flags);

int CMS_set1_eContentType(CMS_ContentInfo *cms, const ASN1_OBJECT *oid);
const ASN1_OBJECT *CMS_get0_eContentType(CMS_ContentInfo *cms);

CMS_CertificateChoices *CMS_add0_CertificateChoices(CMS_ContentInfo *cms);
int CMS_add0_cert(CMS_ContentInfo *cms, X509 *cert);
int CMS_add1_cert(CMS_ContentInfo *cms, X509 *cert);
STACK_OF(X509) *CMS_get1_certs(CMS_ContentInfo *cms);

CMS_RevocationInfoChoice *CMS_add0_RevocationInfoChoice(CMS_ContentInfo *cms);
int CMS_add0_crl(CMS_ContentInfo *cms, X509_CRL *crl);
STACK_OF(X509_CRL) *CMS_get1_crls(CMS_ContentInfo *cms);

int CMS_SignedData_init(CMS_ContentInfo *cms);
CMS_SignerInfo *CMS_add1_signer(CMS_ContentInfo *cms,
			X509 *signer, EVP_PKEY *pk, const EVP_MD *md,
			unsigned int flags);
STACK_OF(CMS_SignerInfo) *CMS_get0_SignerInfos(CMS_ContentInfo *cms);

void CMS_SignerInfo_set1_signer_cert(CMS_SignerInfo *si, X509 *signer);
int CMS_SignerInfo_get0_signer_id(CMS_SignerInfo *si,
					ASN1_OCTET_STRING **keyid,
					X509_NAME **issuer, ASN1_INTEGER **sno);
int CMS_SignerInfo_cert_cmp(CMS_SignerInfo *si, X509 *cert);
int CMS_set1_signers_certs(CMS_ContentInfo *cms, STACK_OF(X509) *certs,
					unsigned int flags);
void CMS_SignerInfo_get0_algs(CMS_SignerInfo *si, EVP_PKEY **pk, X509 **signer,
					X509_ALGOR **pdig, X509_ALGOR **psig);
int CMS_SignerInfo_sign(CMS_SignerInfo *si);
int CMS_SignerInfo_verify(CMS_SignerInfo *si);
int CMS_SignerInfo_verify_content(CMS_SignerInfo *si, BIO *chain);

int CMS_add_smimecap(CMS_SignerInfo *si, STACK_OF(X509_ALGOR) *algs);
int CMS_add_simple_smimecap(STACK_OF(X509_ALGOR) **algs,
				int algnid, int keysize);
int CMS_add_standard_smimecap(STACK_OF(X509_ALGOR) **smcap);

int CMS_signed_get_attr_count(const CMS_SignerInfo *si);
int CMS_signed_get_attr_by_NID(const CMS_SignerInfo *si, int nid,
			  int lastpos);
int CMS_signed_get_attr_by_OBJ(const CMS_SignerInfo *si, ASN1_OBJECT *obj,
			  int lastpos);
X509_ATTRIBUTE *CMS_signed_get_attr(const CMS_SignerInfo *si, int loc);
X509_ATTRIBUTE *CMS_signed_delete_attr(CMS_SignerInfo *si, int loc);
int CMS_signed_add1_attr(CMS_SignerInfo *si, X509_ATTRIBUTE *attr);
int CMS_signed_add1_attr_by_OBJ(CMS_SignerInfo *si,
			const ASN1_OBJECT *obj, int type,
			const void *bytes, int len);
int CMS_signed_add1_attr_by_NID(CMS_SignerInfo *si,
			int nid, int type,
			const void *bytes, int len);
int CMS_signed_add1_attr_by_txt(CMS_SignerInfo *si,
			const char *attrname, int type,
			const void *bytes, int len);
void *CMS_signed_get0_data_by_OBJ(CMS_SignerInfo *si, ASN1_OBJECT *oid,
					int lastpos, int type);

int CMS_unsigned_get_attr_count(const CMS_SignerInfo *si);
int CMS_unsigned_get_attr_by_NID(const CMS_SignerInfo *si, int nid,
			  int lastpos);
int CMS_unsigned_get_attr_by_OBJ(const CMS_SignerInfo *si, ASN1_OBJECT *obj,
			  int lastpos);
X509_ATTRIBUTE *CMS_unsigned_get_attr(const CMS_SignerInfo *si, int loc);
X509_ATTRIBUTE *CMS_unsigned_delete_attr(CMS_SignerInfo *si, int loc);
int CMS_unsigned_add1_attr(CMS_SignerInfo *si, X509_ATTRIBUTE *attr);
int CMS_unsigned_add1_attr_by_OBJ(CMS_SignerInfo *si,
			const ASN1_OBJECT *obj, int type,
			const void *bytes, int len);
int CMS_unsigned_add1_attr_by_NID(CMS_SignerInfo *si,
			int nid, int type,
			const void *bytes, int len);
int CMS_unsigned_add1_attr_by_txt(CMS_SignerInfo *si,
			const char *attrname, int type,
			const void *bytes, int len);
void *CMS_unsigned_get0_data_by_OBJ(CMS_SignerInfo *si, ASN1_OBJECT *oid,
					int lastpos, int type);

#ifdef HEADER_X509V3_H

int CMS_get1_ReceiptRequest(CMS_SignerInfo *si, CMS_ReceiptRequest **prr);
CMS_ReceiptRequest *CMS_ReceiptRequest_create0(unsigned char *id, int idlen,
				int allorfirst,
				STACK_OF(GENERAL_NAMES) *receiptList,
				STACK_OF(GENERAL_NAMES) *receiptsTo);
int CMS_add1_ReceiptRequest(CMS_SignerInfo *si, CMS_ReceiptRequest *rr);
void CMS_ReceiptRequest_get0_values(CMS_ReceiptRequest *rr,
					ASN1_STRING **pcid,
					int *pallorfirst,
					STACK_OF(GENERAL_NAMES) **plist,
					STACK_OF(GENERAL_NAMES) **prto);

#endif

/* BEGIN ERROR CODES */
/* The following lines are auto generated by the script mkerr.pl. Any changes
 * made after this point may be overwritten when the script is next run.
 */
void ERR_load_CMS_strings(void);

/* Error codes for the CMS functions. */

/* Function codes. */
#define CMS_F_CHECK_CONTENT				 99
#define CMS_F_CMS_ADD0_CERT				 164
#define CMS_F_CMS_ADD0_RECIPIENT_KEY			 100
#define CMS_F_CMS_ADD1_RECEIPTREQUEST			 158
#define CMS_F_CMS_ADD1_RECIPIENT_CERT			 101
#define CMS_F_CMS_ADD1_SIGNER				 102
#define CMS_F_CMS_ADD1_SIGNINGTIME			 103
#define CMS_F_CMS_COMPRESS				 104
#define CMS_F_CMS_COMPRESSEDDATA_CREATE			 105
#define CMS_F_CMS_COMPRESSEDDATA_INIT_BIO		 106
#define CMS_F_CMS_COPY_CONTENT				 107
#define CMS_F_CMS_COPY_MESSAGEDIGEST			 108
#define CMS_F_CMS_DATA					 109
#define CMS_F_CMS_DATAFINAL				 110
#define CMS_F_CMS_DATAINIT				 111
#define CMS_F_CMS_DECRYPT				 112
#define CMS_F_CMS_DECRYPT_SET1_KEY			 113
#define CMS_F_CMS_DECRYPT_SET1_PKEY			 114
#define CMS_F_CMS_DIGESTALGORITHM_FIND_CTX		 115
#define CMS_F_CMS_DIGESTALGORITHM_INIT_BIO		 116
#define CMS_F_CMS_DIGESTEDDATA_DO_FINAL			 117
#define CMS_F_CMS_DIGEST_VERIFY				 118
#define CMS_F_CMS_ENCODE_RECEIPT			 161
#define CMS_F_CMS_ENCRYPT				 119
#define CMS_F_CMS_ENCRYPTEDCONTENT_INIT_BIO		 120
#define CMS_F_CMS_ENCRYPTEDDATA_DECRYPT			 121
#define CMS_F_CMS_ENCRYPTEDDATA_ENCRYPT			 122
#define CMS_F_CMS_ENCRYPTEDDATA_SET1_KEY		 123
#define CMS_F_CMS_ENVELOPEDDATA_CREATE			 124
#define CMS_F_CMS_ENVELOPEDDATA_INIT_BIO		 125
#define CMS_F_CMS_ENVELOPED_DATA_INIT			 126
#define CMS_F_CMS_FINAL					 127
#define CMS_F_CMS_GET0_CERTIFICATE_CHOICES		 128
#define CMS_F_CMS_GET0_CONTENT				 129
#define CMS_F_CMS_GET0_ECONTENT_TYPE			 130
#define CMS_F_CMS_GET0_ENVELOPED			 131
#define CMS_F_CMS_GET0_REVOCATION_CHOICES		 132
#define CMS_F_CMS_GET0_SIGNED				 133
#define CMS_F_CMS_MSGSIGDIGEST_ADD1			 162
#define CMS_F_CMS_RECEIPTREQUEST_CREATE0		 159
#define CMS_F_CMS_RECEIPT_VERIFY			 160
#define CMS_F_CMS_RECIPIENTINFO_DECRYPT			 134
#define CMS_F_CMS_RECIPIENTINFO_KEKRI_DECRYPT		 135
#define CMS_F_CMS_RECIPIENTINFO_KEKRI_ENCRYPT		 136
#define CMS_F_CMS_RECIPIENTINFO_KEKRI_GET0_ID		 137
#define CMS_F_CMS_RECIPIENTINFO_KEKRI_ID_CMP		 138
#define CMS_F_CMS_RECIPIENTINFO_KTRI_CERT_CMP		 139
#define CMS_F_CMS_RECIPIENTINFO_KTRI_DECRYPT		 140
#define CMS_F_CMS_RECIPIENTINFO_KTRI_ENCRYPT		 141
#define CMS_F_CMS_RECIPIENTINFO_KTRI_GET0_ALGS		 142
#define CMS_F_CMS_RECIPIENTINFO_KTRI_GET0_SIGNER_ID	 143
#define CMS_F_CMS_RECIPIENTINFO_SET0_KEY		 144
#define CMS_F_CMS_RECIPIENTINFO_SET0_PKEY		 145
#define CMS_F_CMS_SET1_SIGNERIDENTIFIER			 146
#define CMS_F_CMS_SET_DETACHED				 147
#define CMS_F_CMS_SIGN					 148
#define CMS_F_CMS_SIGNED_DATA_INIT			 149
#define CMS_F_CMS_SIGNERINFO_CONTENT_SIGN		 150
#define CMS_F_CMS_SIGNERINFO_SIGN			 151
#define CMS_F_CMS_SIGNERINFO_VERIFY			 152
#define CMS_F_CMS_SIGNERINFO_VERIFY_CERT		 153
#define CMS_F_CMS_SIGNERINFO_VERIFY_CONTENT		 154
#define CMS_F_CMS_SIGN_RECEIPT				 163
#define CMS_F_CMS_STREAM				 155
#define CMS_F_CMS_UNCOMPRESS				 156
#define CMS_F_CMS_VERIFY				 157

/* Reason codes. */
#define CMS_R_ADD_SIGNER_ERROR				 99
#define CMS_R_CERTIFICATE_ALREADY_PRESENT		 175
#define CMS_R_CERTIFICATE_HAS_NO_KEYID			 160
#define CMS_R_CERTIFICATE_VERIFY_ERROR			 100
#define CMS_R_CIPHER_INITIALISATION_ERROR		 101
#define CMS_R_CIPHER_PARAMETER_INITIALISATION_ERROR	 102
#define CMS_R_CMS_DATAFINAL_ERROR			 103
#define CMS_R_CMS_LIB					 104
#define CMS_R_CONTENTIDENTIFIER_MISMATCH		 170
#define CMS_R_CONTENT_NOT_FOUND				 105
#define CMS_R_CONTENT_TYPE_MISMATCH			 171
#define CMS_R_CONTENT_TYPE_NOT_COMPRESSED_DATA		 106
#define CMS_R_CONTENT_TYPE_NOT_ENVELOPED_DATA		 107
#define CMS_R_CONTENT_TYPE_NOT_SIGNED_DATA		 108
#define CMS_R_CONTENT_VERIFY_ERROR			 109
#define CMS_R_CTRL_ERROR				 110
#define CMS_R_CTRL_FAILURE				 111
#define CMS_R_DECRYPT_ERROR				 112
#define CMS_R_DIGEST_ERROR				 161
#define CMS_R_ERROR_GETTING_PUBLIC_KEY			 113
#define CMS_R_ERROR_READING_MESSAGEDIGEST_ATTRIBUTE	 114
#define CMS_R_ERROR_SETTING_KEY				 115
#define CMS_R_ERROR_SETTING_RECIPIENTINFO		 116
#define CMS_R_INVALID_ENCRYPTED_KEY_LENGTH		 117
#define CMS_R_INVALID_KEY_LENGTH			 118
#define CMS_R_MD_BIO_INIT_ERROR				 119
#define CMS_R_MESSAGEDIGEST_ATTRIBUTE_WRONG_LENGTH	 120
#define CMS_R_MESSAGEDIGEST_WRONG_LENGTH		 121
#define CMS_R_MSGSIGDIGEST_ERROR			 172
#define CMS_R_MSGSIGDIGEST_VERIFICATION_FAILURE		 162
#define CMS_R_MSGSIGDIGEST_WRONG_LENGTH			 163
#define CMS_R_NEED_ONE_SIGNER				 164
#define CMS_R_NOT_A_SIGNED_RECEIPT			 165
#define CMS_R_NOT_ENCRYPTED_DATA			 122
#define CMS_R_NOT_KEK					 123
#define CMS_R_NOT_KEY_TRANSPORT				 124
#define CMS_R_NOT_SUPPORTED_FOR_THIS_KEY_TYPE		 125
#define CMS_R_NO_CIPHER					 126
#define CMS_R_NO_CONTENT				 127
#define CMS_R_NO_CONTENT_TYPE				 173
#define CMS_R_NO_DEFAULT_DIGEST				 128
#define CMS_R_NO_DIGEST_SET				 129
#define CMS_R_NO_KEY					 130
#define CMS_R_NO_KEY_OR_CERT				 174
#define CMS_R_NO_MATCHING_DIGEST			 131
#define CMS_R_NO_MATCHING_RECIPIENT			 132
#define CMS_R_NO_MATCHING_SIGNATURE			 166
#define CMS_R_NO_MSGSIGDIGEST				 167
#define CMS_R_NO_PRIVATE_KEY				 133
#define CMS_R_NO_PUBLIC_KEY				 134
#define CMS_R_NO_RECEIPT_REQUEST			 168
#define CMS_R_NO_SIGNERS				 135
#define CMS_R_PRIVATE_KEY_DOES_NOT_MATCH_CERTIFICATE	 136
#define CMS_R_RECEIPT_DECODE_ERROR			 169
#define CMS_R_RECIPIENT_ERROR				 137
#define CMS_R_SIGNER_CERTIFICATE_NOT_FOUND		 138
#define CMS_R_SIGNFINAL_ERROR				 139
#define CMS_R_SMIME_TEXT_ERROR				 140
#define CMS_R_STORE_INIT_ERROR				 141
#define CMS_R_TYPE_NOT_COMPRESSED_DATA			 142
#define CMS_R_TYPE_NOT_DATA				 143
#define CMS_R_TYPE_NOT_DIGESTED_DATA			 144
#define CMS_R_TYPE_NOT_ENCRYPTED_DATA			 145
#define CMS_R_TYPE_NOT_ENVELOPED_DATA			 146
#define CMS_R_UNABLE_TO_FINALIZE_CONTEXT		 147
#define CMS_R_UNKNOWN_CIPHER				 148
#define CMS_R_UNKNOWN_DIGEST_ALGORIHM			 149
#define CMS_R_UNKNOWN_ID				 150
#define CMS_R_UNSUPPORTED_COMPRESSION_ALGORITHM		 151
#define CMS_R_UNSUPPORTED_CONTENT_TYPE			 152
#define CMS_R_UNSUPPORTED_KEK_ALGORITHM			 153
#define CMS_R_UNSUPPORTED_RECIPIENT_TYPE		 154
#define CMS_R_UNSUPPORTED_RECPIENTINFO_TYPE		 155
#define CMS_R_UNSUPPORTED_TYPE				 156
#define CMS_R_UNWRAP_ERROR				 157
#define CMS_R_VERIFICATION_FAILURE			 158
#define CMS_R_WRAP_ERROR				 159

#ifdef  __cplusplus
}
#endif
#endif
