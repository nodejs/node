/* crypto/x509/x509.h */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 * 
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 * 
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from 
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 * 
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */
/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 * ECDH support in OpenSSL originally developed by 
 * SUN MICROSYSTEMS, INC., and contributed to the OpenSSL project.
 */

#ifndef HEADER_X509_H
#define HEADER_X509_H

#include <openssl/e_os2.h>
#include <openssl/symhacks.h>
#ifndef OPENSSL_NO_BUFFER
#include <openssl/buffer.h>
#endif
#ifndef OPENSSL_NO_EVP
#include <openssl/evp.h>
#endif
#ifndef OPENSSL_NO_BIO
#include <openssl/bio.h>
#endif
#include <openssl/stack.h>
#include <openssl/asn1.h>
#include <openssl/safestack.h>

#ifndef OPENSSL_NO_EC
#include <openssl/ec.h>
#endif

#ifndef OPENSSL_NO_ECDSA
#include <openssl/ecdsa.h>
#endif

#ifndef OPENSSL_NO_ECDH
#include <openssl/ecdh.h>
#endif

#ifndef OPENSSL_NO_DEPRECATED
#ifndef OPENSSL_NO_RSA
#include <openssl/rsa.h>
#endif
#ifndef OPENSSL_NO_DSA
#include <openssl/dsa.h>
#endif
#ifndef OPENSSL_NO_DH
#include <openssl/dh.h>
#endif
#endif

#ifndef OPENSSL_NO_SHA
#include <openssl/sha.h>
#endif
#include <openssl/ossl_typ.h>

#ifdef  __cplusplus
extern "C" {
#endif

#ifdef OPENSSL_SYS_WIN32
/* Under Win32 these are defined in wincrypt.h */
#undef X509_NAME
#undef X509_CERT_PAIR
#undef X509_EXTENSIONS
#endif

#define X509_FILETYPE_PEM	1
#define X509_FILETYPE_ASN1	2
#define X509_FILETYPE_DEFAULT	3

#define X509v3_KU_DIGITAL_SIGNATURE	0x0080
#define X509v3_KU_NON_REPUDIATION	0x0040
#define X509v3_KU_KEY_ENCIPHERMENT	0x0020
#define X509v3_KU_DATA_ENCIPHERMENT	0x0010
#define X509v3_KU_KEY_AGREEMENT		0x0008
#define X509v3_KU_KEY_CERT_SIGN		0x0004
#define X509v3_KU_CRL_SIGN		0x0002
#define X509v3_KU_ENCIPHER_ONLY		0x0001
#define X509v3_KU_DECIPHER_ONLY		0x8000
#define X509v3_KU_UNDEF			0xffff

typedef struct X509_objects_st
	{
	int nid;
	int (*a2i)(void);
	int (*i2a)(void);
	} X509_OBJECTS;

struct X509_algor_st
	{
	ASN1_OBJECT *algorithm;
	ASN1_TYPE *parameter;
	} /* X509_ALGOR */;

DECLARE_ASN1_SET_OF(X509_ALGOR)

typedef STACK_OF(X509_ALGOR) X509_ALGORS;

typedef struct X509_val_st
	{
	ASN1_TIME *notBefore;
	ASN1_TIME *notAfter;
	} X509_VAL;

typedef struct X509_pubkey_st
	{
	X509_ALGOR *algor;
	ASN1_BIT_STRING *public_key;
	EVP_PKEY *pkey;
	} X509_PUBKEY;

typedef struct X509_sig_st
	{
	X509_ALGOR *algor;
	ASN1_OCTET_STRING *digest;
	} X509_SIG;

typedef struct X509_name_entry_st
	{
	ASN1_OBJECT *object;
	ASN1_STRING *value;
	int set;
	int size; 	/* temp variable */
	} X509_NAME_ENTRY;

DECLARE_STACK_OF(X509_NAME_ENTRY)
DECLARE_ASN1_SET_OF(X509_NAME_ENTRY)

/* we always keep X509_NAMEs in 2 forms. */
struct X509_name_st
	{
	STACK_OF(X509_NAME_ENTRY) *entries;
	int modified;	/* true if 'bytes' needs to be built */
#ifndef OPENSSL_NO_BUFFER
	BUF_MEM *bytes;
#else
	char *bytes;
#endif
	unsigned long hash; /* Keep the hash around for lookups */
	} /* X509_NAME */;

DECLARE_STACK_OF(X509_NAME)

#define X509_EX_V_NETSCAPE_HACK		0x8000
#define X509_EX_V_INIT			0x0001
typedef struct X509_extension_st
	{
	ASN1_OBJECT *object;
	ASN1_BOOLEAN critical;
	ASN1_OCTET_STRING *value;
	} X509_EXTENSION;

typedef STACK_OF(X509_EXTENSION) X509_EXTENSIONS;

DECLARE_STACK_OF(X509_EXTENSION)
DECLARE_ASN1_SET_OF(X509_EXTENSION)

/* a sequence of these are used */
typedef struct x509_attributes_st
	{
	ASN1_OBJECT *object;
	int single; /* 0 for a set, 1 for a single item (which is wrong) */
	union	{
		char		*ptr;
/* 0 */		STACK_OF(ASN1_TYPE) *set;
/* 1 */		ASN1_TYPE	*single;
		} value;
	} X509_ATTRIBUTE;

DECLARE_STACK_OF(X509_ATTRIBUTE)
DECLARE_ASN1_SET_OF(X509_ATTRIBUTE)


typedef struct X509_req_info_st
	{
	ASN1_ENCODING enc;
	ASN1_INTEGER *version;
	X509_NAME *subject;
	X509_PUBKEY *pubkey;
	/*  d=2 hl=2 l=  0 cons: cont: 00 */
	STACK_OF(X509_ATTRIBUTE) *attributes; /* [ 0 ] */
	} X509_REQ_INFO;

typedef struct X509_req_st
	{
	X509_REQ_INFO *req_info;
	X509_ALGOR *sig_alg;
	ASN1_BIT_STRING *signature;
	int references;
	} X509_REQ;

typedef struct x509_cinf_st
	{
	ASN1_INTEGER *version;		/* [ 0 ] default of v1 */
	ASN1_INTEGER *serialNumber;
	X509_ALGOR *signature;
	X509_NAME *issuer;
	X509_VAL *validity;
	X509_NAME *subject;
	X509_PUBKEY *key;
	ASN1_BIT_STRING *issuerUID;		/* [ 1 ] optional in v2 */
	ASN1_BIT_STRING *subjectUID;		/* [ 2 ] optional in v2 */
	STACK_OF(X509_EXTENSION) *extensions;	/* [ 3 ] optional in v3 */
	} X509_CINF;

/* This stuff is certificate "auxiliary info"
 * it contains details which are useful in certificate
 * stores and databases. When used this is tagged onto
 * the end of the certificate itself
 */

typedef struct x509_cert_aux_st
	{
	STACK_OF(ASN1_OBJECT) *trust;		/* trusted uses */
	STACK_OF(ASN1_OBJECT) *reject;		/* rejected uses */
	ASN1_UTF8STRING *alias;			/* "friendly name" */
	ASN1_OCTET_STRING *keyid;		/* key id of private key */
	STACK_OF(X509_ALGOR) *other;		/* other unspecified info */
	} X509_CERT_AUX;

struct x509_st
	{
	X509_CINF *cert_info;
	X509_ALGOR *sig_alg;
	ASN1_BIT_STRING *signature;
	int valid;
	int references;
	char *name;
	CRYPTO_EX_DATA ex_data;
	/* These contain copies of various extension values */
	long ex_pathlen;
	long ex_pcpathlen;
	unsigned long ex_flags;
	unsigned long ex_kusage;
	unsigned long ex_xkusage;
	unsigned long ex_nscert;
	ASN1_OCTET_STRING *skid;
	struct AUTHORITY_KEYID_st *akid;
	X509_POLICY_CACHE *policy_cache;
#ifndef OPENSSL_NO_RFC3779
	STACK_OF(IPAddressFamily) *rfc3779_addr;
	struct ASIdentifiers_st *rfc3779_asid;
#endif
#ifndef OPENSSL_NO_SHA
	unsigned char sha1_hash[SHA_DIGEST_LENGTH];
#endif
	X509_CERT_AUX *aux;
	} /* X509 */;

DECLARE_STACK_OF(X509)
DECLARE_ASN1_SET_OF(X509)

/* This is used for a table of trust checking functions */

typedef struct x509_trust_st {
	int trust;
	int flags;
	int (*check_trust)(struct x509_trust_st *, X509 *, int);
	char *name;
	int arg1;
	void *arg2;
} X509_TRUST;

DECLARE_STACK_OF(X509_TRUST)

typedef struct x509_cert_pair_st {
	X509 *forward;
	X509 *reverse;
} X509_CERT_PAIR;

/* standard trust ids */

#define X509_TRUST_DEFAULT	-1	/* Only valid in purpose settings */

#define X509_TRUST_COMPAT	1
#define X509_TRUST_SSL_CLIENT	2
#define X509_TRUST_SSL_SERVER	3
#define X509_TRUST_EMAIL	4
#define X509_TRUST_OBJECT_SIGN	5
#define X509_TRUST_OCSP_SIGN	6
#define X509_TRUST_OCSP_REQUEST	7

/* Keep these up to date! */
#define X509_TRUST_MIN		1
#define X509_TRUST_MAX		7


/* trust_flags values */
#define	X509_TRUST_DYNAMIC 	1
#define	X509_TRUST_DYNAMIC_NAME	2

/* check_trust return codes */

#define X509_TRUST_TRUSTED	1
#define X509_TRUST_REJECTED	2
#define X509_TRUST_UNTRUSTED	3

/* Flags for X509_print_ex() */

#define	X509_FLAG_COMPAT		0
#define	X509_FLAG_NO_HEADER		1L
#define	X509_FLAG_NO_VERSION		(1L << 1)
#define	X509_FLAG_NO_SERIAL		(1L << 2)
#define	X509_FLAG_NO_SIGNAME		(1L << 3)
#define	X509_FLAG_NO_ISSUER		(1L << 4)
#define	X509_FLAG_NO_VALIDITY		(1L << 5)
#define	X509_FLAG_NO_SUBJECT		(1L << 6)
#define	X509_FLAG_NO_PUBKEY		(1L << 7)
#define	X509_FLAG_NO_EXTENSIONS		(1L << 8)
#define	X509_FLAG_NO_SIGDUMP		(1L << 9)
#define	X509_FLAG_NO_AUX		(1L << 10)
#define	X509_FLAG_NO_ATTRIBUTES		(1L << 11)

/* Flags specific to X509_NAME_print_ex() */	

/* The field separator information */

#define XN_FLAG_SEP_MASK	(0xf << 16)

#define XN_FLAG_COMPAT		0		/* Traditional SSLeay: use old X509_NAME_print */
#define XN_FLAG_SEP_COMMA_PLUS	(1 << 16)	/* RFC2253 ,+ */
#define XN_FLAG_SEP_CPLUS_SPC	(2 << 16)	/* ,+ spaced: more readable */
#define XN_FLAG_SEP_SPLUS_SPC	(3 << 16)	/* ;+ spaced */
#define XN_FLAG_SEP_MULTILINE	(4 << 16)	/* One line per field */

#define XN_FLAG_DN_REV		(1 << 20)	/* Reverse DN order */

/* How the field name is shown */

#define XN_FLAG_FN_MASK		(0x3 << 21)

#define XN_FLAG_FN_SN		0		/* Object short name */
#define XN_FLAG_FN_LN		(1 << 21)	/* Object long name */
#define XN_FLAG_FN_OID		(2 << 21)	/* Always use OIDs */
#define XN_FLAG_FN_NONE		(3 << 21)	/* No field names */

#define XN_FLAG_SPC_EQ		(1 << 23)	/* Put spaces round '=' */

/* This determines if we dump fields we don't recognise:
 * RFC2253 requires this.
 */

#define XN_FLAG_DUMP_UNKNOWN_FIELDS (1 << 24)

#define XN_FLAG_FN_ALIGN	(1 << 25)	/* Align field names to 20 characters */

/* Complete set of RFC2253 flags */

#define XN_FLAG_RFC2253 (ASN1_STRFLGS_RFC2253 | \
			XN_FLAG_SEP_COMMA_PLUS | \
			XN_FLAG_DN_REV | \
			XN_FLAG_FN_SN | \
			XN_FLAG_DUMP_UNKNOWN_FIELDS)

/* readable oneline form */

#define XN_FLAG_ONELINE (ASN1_STRFLGS_RFC2253 | \
			ASN1_STRFLGS_ESC_QUOTE | \
			XN_FLAG_SEP_CPLUS_SPC | \
			XN_FLAG_SPC_EQ | \
			XN_FLAG_FN_SN)

/* readable multiline form */

#define XN_FLAG_MULTILINE (ASN1_STRFLGS_ESC_CTRL | \
			ASN1_STRFLGS_ESC_MSB | \
			XN_FLAG_SEP_MULTILINE | \
			XN_FLAG_SPC_EQ | \
			XN_FLAG_FN_LN | \
			XN_FLAG_FN_ALIGN)

typedef struct X509_revoked_st
	{
	ASN1_INTEGER *serialNumber;
	ASN1_TIME *revocationDate;
	STACK_OF(X509_EXTENSION) /* optional */ *extensions;
	int sequence; /* load sequence */
	} X509_REVOKED;

DECLARE_STACK_OF(X509_REVOKED)
DECLARE_ASN1_SET_OF(X509_REVOKED)

typedef struct X509_crl_info_st
	{
	ASN1_INTEGER *version;
	X509_ALGOR *sig_alg;
	X509_NAME *issuer;
	ASN1_TIME *lastUpdate;
	ASN1_TIME *nextUpdate;
	STACK_OF(X509_REVOKED) *revoked;
	STACK_OF(X509_EXTENSION) /* [0] */ *extensions;
	ASN1_ENCODING enc;
	} X509_CRL_INFO;

struct X509_crl_st
	{
	/* actual signature */
	X509_CRL_INFO *crl;
	X509_ALGOR *sig_alg;
	ASN1_BIT_STRING *signature;
	int references;
	} /* X509_CRL */;

DECLARE_STACK_OF(X509_CRL)
DECLARE_ASN1_SET_OF(X509_CRL)

typedef struct private_key_st
	{
	int version;
	/* The PKCS#8 data types */
	X509_ALGOR *enc_algor;
	ASN1_OCTET_STRING *enc_pkey;	/* encrypted pub key */

	/* When decrypted, the following will not be NULL */
	EVP_PKEY *dec_pkey;

	/* used to encrypt and decrypt */
	int key_length;
	char *key_data;
	int key_free;	/* true if we should auto free key_data */

	/* expanded version of 'enc_algor' */
	EVP_CIPHER_INFO cipher;

	int references;
	} X509_PKEY;

#ifndef OPENSSL_NO_EVP
typedef struct X509_info_st
	{
	X509 *x509;
	X509_CRL *crl;
	X509_PKEY *x_pkey;

	EVP_CIPHER_INFO enc_cipher;
	int enc_len;
	char *enc_data;

	int references;
	} X509_INFO;

DECLARE_STACK_OF(X509_INFO)
#endif

/* The next 2 structures and their 8 routines were sent to me by
 * Pat Richard <patr@x509.com> and are used to manipulate
 * Netscapes spki structures - useful if you are writing a CA web page
 */
typedef struct Netscape_spkac_st
	{
	X509_PUBKEY *pubkey;
	ASN1_IA5STRING *challenge;	/* challenge sent in atlas >= PR2 */
	} NETSCAPE_SPKAC;

typedef struct Netscape_spki_st
	{
	NETSCAPE_SPKAC *spkac;	/* signed public key and challenge */
	X509_ALGOR *sig_algor;
	ASN1_BIT_STRING *signature;
	} NETSCAPE_SPKI;

/* Netscape certificate sequence structure */
typedef struct Netscape_certificate_sequence
	{
	ASN1_OBJECT *type;
	STACK_OF(X509) *certs;
	} NETSCAPE_CERT_SEQUENCE;

/* Unused (and iv length is wrong)
typedef struct CBCParameter_st
	{
	unsigned char iv[8];
	} CBC_PARAM;
*/

/* Password based encryption structure */

typedef struct PBEPARAM_st {
ASN1_OCTET_STRING *salt;
ASN1_INTEGER *iter;
} PBEPARAM;

/* Password based encryption V2 structures */

typedef struct PBE2PARAM_st {
X509_ALGOR *keyfunc;
X509_ALGOR *encryption;
} PBE2PARAM;

typedef struct PBKDF2PARAM_st {
ASN1_TYPE *salt;	/* Usually OCTET STRING but could be anything */
ASN1_INTEGER *iter;
ASN1_INTEGER *keylength;
X509_ALGOR *prf;
} PBKDF2PARAM;


/* PKCS#8 private key info structure */

typedef struct pkcs8_priv_key_info_st
        {
        int broken;     /* Flag for various broken formats */
#define PKCS8_OK		0
#define PKCS8_NO_OCTET		1
#define PKCS8_EMBEDDED_PARAM	2
#define PKCS8_NS_DB		3
        ASN1_INTEGER *version;
        X509_ALGOR *pkeyalg;
        ASN1_TYPE *pkey; /* Should be OCTET STRING but some are broken */
        STACK_OF(X509_ATTRIBUTE) *attributes;
        } PKCS8_PRIV_KEY_INFO;

#ifdef  __cplusplus
}
#endif

#include <openssl/x509_vfy.h>
#include <openssl/pkcs7.h>

#ifdef  __cplusplus
extern "C" {
#endif

#ifdef SSLEAY_MACROS
#define X509_verify(a,r) ASN1_verify((int (*)())i2d_X509_CINF,a->sig_alg,\
	a->signature,(char *)a->cert_info,r)
#define X509_REQ_verify(a,r) ASN1_verify((int (*)())i2d_X509_REQ_INFO, \
	a->sig_alg,a->signature,(char *)a->req_info,r)
#define X509_CRL_verify(a,r) ASN1_verify((int (*)())i2d_X509_CRL_INFO, \
	a->sig_alg, a->signature,(char *)a->crl,r)

#define X509_sign(x,pkey,md) \
	ASN1_sign((int (*)())i2d_X509_CINF, x->cert_info->signature, \
		x->sig_alg, x->signature, (char *)x->cert_info,pkey,md)
#define X509_REQ_sign(x,pkey,md) \
	ASN1_sign((int (*)())i2d_X509_REQ_INFO,x->sig_alg, NULL, \
		x->signature, (char *)x->req_info,pkey,md)
#define X509_CRL_sign(x,pkey,md) \
	ASN1_sign((int (*)())i2d_X509_CRL_INFO,x->crl->sig_alg,x->sig_alg, \
		x->signature, (char *)x->crl,pkey,md)
#define NETSCAPE_SPKI_sign(x,pkey,md) \
	ASN1_sign((int (*)())i2d_NETSCAPE_SPKAC, x->sig_algor,NULL, \
		x->signature, (char *)x->spkac,pkey,md)

#define X509_dup(x509) (X509 *)ASN1_dup((int (*)())i2d_X509, \
		(char *(*)())d2i_X509,(char *)x509)
#define X509_ATTRIBUTE_dup(xa) (X509_ATTRIBUTE *)ASN1_dup(\
		(int (*)())i2d_X509_ATTRIBUTE, \
		(char *(*)())d2i_X509_ATTRIBUTE,(char *)xa)
#define X509_EXTENSION_dup(ex) (X509_EXTENSION *)ASN1_dup( \
		(int (*)())i2d_X509_EXTENSION, \
		(char *(*)())d2i_X509_EXTENSION,(char *)ex)
#define d2i_X509_fp(fp,x509) (X509 *)ASN1_d2i_fp((char *(*)())X509_new, \
		(char *(*)())d2i_X509, (fp),(unsigned char **)(x509))
#define i2d_X509_fp(fp,x509) ASN1_i2d_fp(i2d_X509,fp,(unsigned char *)x509)
#define d2i_X509_bio(bp,x509) (X509 *)ASN1_d2i_bio((char *(*)())X509_new, \
		(char *(*)())d2i_X509, (bp),(unsigned char **)(x509))
#define i2d_X509_bio(bp,x509) ASN1_i2d_bio(i2d_X509,bp,(unsigned char *)x509)

#define X509_CRL_dup(crl) (X509_CRL *)ASN1_dup((int (*)())i2d_X509_CRL, \
		(char *(*)())d2i_X509_CRL,(char *)crl)
#define d2i_X509_CRL_fp(fp,crl) (X509_CRL *)ASN1_d2i_fp((char *(*)()) \
		X509_CRL_new,(char *(*)())d2i_X509_CRL, (fp),\
		(unsigned char **)(crl))
#define i2d_X509_CRL_fp(fp,crl) ASN1_i2d_fp(i2d_X509_CRL,fp,\
		(unsigned char *)crl)
#define d2i_X509_CRL_bio(bp,crl) (X509_CRL *)ASN1_d2i_bio((char *(*)()) \
		X509_CRL_new,(char *(*)())d2i_X509_CRL, (bp),\
		(unsigned char **)(crl))
#define i2d_X509_CRL_bio(bp,crl) ASN1_i2d_bio(i2d_X509_CRL,bp,\
		(unsigned char *)crl)

#define PKCS7_dup(p7) (PKCS7 *)ASN1_dup((int (*)())i2d_PKCS7, \
		(char *(*)())d2i_PKCS7,(char *)p7)
#define d2i_PKCS7_fp(fp,p7) (PKCS7 *)ASN1_d2i_fp((char *(*)()) \
		PKCS7_new,(char *(*)())d2i_PKCS7, (fp),\
		(unsigned char **)(p7))
#define i2d_PKCS7_fp(fp,p7) ASN1_i2d_fp(i2d_PKCS7,fp,\
		(unsigned char *)p7)
#define d2i_PKCS7_bio(bp,p7) (PKCS7 *)ASN1_d2i_bio((char *(*)()) \
		PKCS7_new,(char *(*)())d2i_PKCS7, (bp),\
		(unsigned char **)(p7))
#define i2d_PKCS7_bio(bp,p7) ASN1_i2d_bio(i2d_PKCS7,bp,\
		(unsigned char *)p7)

#define X509_REQ_dup(req) (X509_REQ *)ASN1_dup((int (*)())i2d_X509_REQ, \
		(char *(*)())d2i_X509_REQ,(char *)req)
#define d2i_X509_REQ_fp(fp,req) (X509_REQ *)ASN1_d2i_fp((char *(*)())\
		X509_REQ_new, (char *(*)())d2i_X509_REQ, (fp),\
		(unsigned char **)(req))
#define i2d_X509_REQ_fp(fp,req) ASN1_i2d_fp(i2d_X509_REQ,fp,\
		(unsigned char *)req)
#define d2i_X509_REQ_bio(bp,req) (X509_REQ *)ASN1_d2i_bio((char *(*)())\
		X509_REQ_new, (char *(*)())d2i_X509_REQ, (bp),\
		(unsigned char **)(req))
#define i2d_X509_REQ_bio(bp,req) ASN1_i2d_bio(i2d_X509_REQ,bp,\
		(unsigned char *)req)

#define RSAPublicKey_dup(rsa) (RSA *)ASN1_dup((int (*)())i2d_RSAPublicKey, \
		(char *(*)())d2i_RSAPublicKey,(char *)rsa)
#define RSAPrivateKey_dup(rsa) (RSA *)ASN1_dup((int (*)())i2d_RSAPrivateKey, \
		(char *(*)())d2i_RSAPrivateKey,(char *)rsa)

#define d2i_RSAPrivateKey_fp(fp,rsa) (RSA *)ASN1_d2i_fp((char *(*)())\
		RSA_new,(char *(*)())d2i_RSAPrivateKey, (fp), \
		(unsigned char **)(rsa))
#define i2d_RSAPrivateKey_fp(fp,rsa) ASN1_i2d_fp(i2d_RSAPrivateKey,fp, \
		(unsigned char *)rsa)
#define d2i_RSAPrivateKey_bio(bp,rsa) (RSA *)ASN1_d2i_bio((char *(*)())\
		RSA_new,(char *(*)())d2i_RSAPrivateKey, (bp), \
		(unsigned char **)(rsa))
#define i2d_RSAPrivateKey_bio(bp,rsa) ASN1_i2d_bio(i2d_RSAPrivateKey,bp, \
		(unsigned char *)rsa)

#define d2i_RSAPublicKey_fp(fp,rsa) (RSA *)ASN1_d2i_fp((char *(*)())\
		RSA_new,(char *(*)())d2i_RSAPublicKey, (fp), \
		(unsigned char **)(rsa))
#define i2d_RSAPublicKey_fp(fp,rsa) ASN1_i2d_fp(i2d_RSAPublicKey,fp, \
		(unsigned char *)rsa)
#define d2i_RSAPublicKey_bio(bp,rsa) (RSA *)ASN1_d2i_bio((char *(*)())\
		RSA_new,(char *(*)())d2i_RSAPublicKey, (bp), \
		(unsigned char **)(rsa))
#define i2d_RSAPublicKey_bio(bp,rsa) ASN1_i2d_bio(i2d_RSAPublicKey,bp, \
		(unsigned char *)rsa)

#define d2i_DSAPrivateKey_fp(fp,dsa) (DSA *)ASN1_d2i_fp((char *(*)())\
		DSA_new,(char *(*)())d2i_DSAPrivateKey, (fp), \
		(unsigned char **)(dsa))
#define i2d_DSAPrivateKey_fp(fp,dsa) ASN1_i2d_fp(i2d_DSAPrivateKey,fp, \
		(unsigned char *)dsa)
#define d2i_DSAPrivateKey_bio(bp,dsa) (DSA *)ASN1_d2i_bio((char *(*)())\
		DSA_new,(char *(*)())d2i_DSAPrivateKey, (bp), \
		(unsigned char **)(dsa))
#define i2d_DSAPrivateKey_bio(bp,dsa) ASN1_i2d_bio(i2d_DSAPrivateKey,bp, \
		(unsigned char *)dsa)

#define d2i_ECPrivateKey_fp(fp,ecdsa) (EC_KEY *)ASN1_d2i_fp((char *(*)())\
		EC_KEY_new,(char *(*)())d2i_ECPrivateKey, (fp), \
		(unsigned char **)(ecdsa))
#define i2d_ECPrivateKey_fp(fp,ecdsa) ASN1_i2d_fp(i2d_ECPrivateKey,fp, \
		(unsigned char *)ecdsa)
#define d2i_ECPrivateKey_bio(bp,ecdsa) (EC_KEY *)ASN1_d2i_bio((char *(*)())\
		EC_KEY_new,(char *(*)())d2i_ECPrivateKey, (bp), \
		(unsigned char **)(ecdsa))
#define i2d_ECPrivateKey_bio(bp,ecdsa) ASN1_i2d_bio(i2d_ECPrivateKey,bp, \
		(unsigned char *)ecdsa)

#define X509_ALGOR_dup(xn) (X509_ALGOR *)ASN1_dup((int (*)())i2d_X509_ALGOR,\
		(char *(*)())d2i_X509_ALGOR,(char *)xn)

#define X509_NAME_dup(xn) (X509_NAME *)ASN1_dup((int (*)())i2d_X509_NAME, \
		(char *(*)())d2i_X509_NAME,(char *)xn)
#define X509_NAME_ENTRY_dup(ne) (X509_NAME_ENTRY *)ASN1_dup( \
		(int (*)())i2d_X509_NAME_ENTRY, \
		(char *(*)())d2i_X509_NAME_ENTRY,\
		(char *)ne)

#define X509_digest(data,type,md,len) \
	ASN1_digest((int (*)())i2d_X509,type,(char *)data,md,len)
#define X509_NAME_digest(data,type,md,len) \
	ASN1_digest((int (*)())i2d_X509_NAME,type,(char *)data,md,len)
#ifndef PKCS7_ISSUER_AND_SERIAL_digest
#define PKCS7_ISSUER_AND_SERIAL_digest(data,type,md,len) \
	ASN1_digest((int (*)())i2d_PKCS7_ISSUER_AND_SERIAL,type,\
		(char *)data,md,len)
#endif
#endif

#define X509_EXT_PACK_UNKNOWN	1
#define X509_EXT_PACK_STRING	2

#define		X509_get_version(x) ASN1_INTEGER_get((x)->cert_info->version)
/* #define	X509_get_serialNumber(x) ((x)->cert_info->serialNumber) */
#define		X509_get_notBefore(x) ((x)->cert_info->validity->notBefore)
#define		X509_get_notAfter(x) ((x)->cert_info->validity->notAfter)
#define		X509_extract_key(x)	X509_get_pubkey(x) /*****/
#define		X509_REQ_get_version(x) ASN1_INTEGER_get((x)->req_info->version)
#define		X509_REQ_get_subject_name(x) ((x)->req_info->subject)
#define		X509_REQ_extract_key(a)	X509_REQ_get_pubkey(a)
#define		X509_name_cmp(a,b)	X509_NAME_cmp((a),(b))
#define		X509_get_signature_type(x) EVP_PKEY_type(OBJ_obj2nid((x)->sig_alg->algorithm))

#define		X509_CRL_get_version(x) ASN1_INTEGER_get((x)->crl->version)
#define 	X509_CRL_get_lastUpdate(x) ((x)->crl->lastUpdate)
#define 	X509_CRL_get_nextUpdate(x) ((x)->crl->nextUpdate)
#define		X509_CRL_get_issuer(x) ((x)->crl->issuer)
#define		X509_CRL_get_REVOKED(x) ((x)->crl->revoked)

/* This one is only used so that a binary form can output, as in
 * i2d_X509_NAME(X509_get_X509_PUBKEY(x),&buf) */
#define 	X509_get_X509_PUBKEY(x) ((x)->cert_info->key)


const char *X509_verify_cert_error_string(long n);

#ifndef SSLEAY_MACROS
#ifndef OPENSSL_NO_EVP
int X509_verify(X509 *a, EVP_PKEY *r);

int X509_REQ_verify(X509_REQ *a, EVP_PKEY *r);
int X509_CRL_verify(X509_CRL *a, EVP_PKEY *r);
int NETSCAPE_SPKI_verify(NETSCAPE_SPKI *a, EVP_PKEY *r);

NETSCAPE_SPKI * NETSCAPE_SPKI_b64_decode(const char *str, int len);
char * NETSCAPE_SPKI_b64_encode(NETSCAPE_SPKI *x);
EVP_PKEY *NETSCAPE_SPKI_get_pubkey(NETSCAPE_SPKI *x);
int NETSCAPE_SPKI_set_pubkey(NETSCAPE_SPKI *x, EVP_PKEY *pkey);

int NETSCAPE_SPKI_print(BIO *out, NETSCAPE_SPKI *spki);

int X509_signature_print(BIO *bp,X509_ALGOR *alg, ASN1_STRING *sig);

int X509_sign(X509 *x, EVP_PKEY *pkey, const EVP_MD *md);
int X509_REQ_sign(X509_REQ *x, EVP_PKEY *pkey, const EVP_MD *md);
int X509_CRL_sign(X509_CRL *x, EVP_PKEY *pkey, const EVP_MD *md);
int NETSCAPE_SPKI_sign(NETSCAPE_SPKI *x, EVP_PKEY *pkey, const EVP_MD *md);

int X509_pubkey_digest(const X509 *data,const EVP_MD *type,
		unsigned char *md, unsigned int *len);
int X509_digest(const X509 *data,const EVP_MD *type,
		unsigned char *md, unsigned int *len);
int X509_CRL_digest(const X509_CRL *data,const EVP_MD *type,
		unsigned char *md, unsigned int *len);
int X509_REQ_digest(const X509_REQ *data,const EVP_MD *type,
		unsigned char *md, unsigned int *len);
int X509_NAME_digest(const X509_NAME *data,const EVP_MD *type,
		unsigned char *md, unsigned int *len);
#endif

#ifndef OPENSSL_NO_FP_API
X509 *d2i_X509_fp(FILE *fp, X509 **x509);
int i2d_X509_fp(FILE *fp,X509 *x509);
X509_CRL *d2i_X509_CRL_fp(FILE *fp,X509_CRL **crl);
int i2d_X509_CRL_fp(FILE *fp,X509_CRL *crl);
X509_REQ *d2i_X509_REQ_fp(FILE *fp,X509_REQ **req);
int i2d_X509_REQ_fp(FILE *fp,X509_REQ *req);
#ifndef OPENSSL_NO_RSA
RSA *d2i_RSAPrivateKey_fp(FILE *fp,RSA **rsa);
int i2d_RSAPrivateKey_fp(FILE *fp,RSA *rsa);
RSA *d2i_RSAPublicKey_fp(FILE *fp,RSA **rsa);
int i2d_RSAPublicKey_fp(FILE *fp,RSA *rsa);
RSA *d2i_RSA_PUBKEY_fp(FILE *fp,RSA **rsa);
int i2d_RSA_PUBKEY_fp(FILE *fp,RSA *rsa);
#endif
#ifndef OPENSSL_NO_DSA
DSA *d2i_DSA_PUBKEY_fp(FILE *fp, DSA **dsa);
int i2d_DSA_PUBKEY_fp(FILE *fp, DSA *dsa);
DSA *d2i_DSAPrivateKey_fp(FILE *fp, DSA **dsa);
int i2d_DSAPrivateKey_fp(FILE *fp, DSA *dsa);
#endif
#ifndef OPENSSL_NO_EC
EC_KEY *d2i_EC_PUBKEY_fp(FILE *fp, EC_KEY **eckey);
int   i2d_EC_PUBKEY_fp(FILE *fp, EC_KEY *eckey);
EC_KEY *d2i_ECPrivateKey_fp(FILE *fp, EC_KEY **eckey);
int   i2d_ECPrivateKey_fp(FILE *fp, EC_KEY *eckey);
#endif
X509_SIG *d2i_PKCS8_fp(FILE *fp,X509_SIG **p8);
int i2d_PKCS8_fp(FILE *fp,X509_SIG *p8);
PKCS8_PRIV_KEY_INFO *d2i_PKCS8_PRIV_KEY_INFO_fp(FILE *fp,
						PKCS8_PRIV_KEY_INFO **p8inf);
int i2d_PKCS8_PRIV_KEY_INFO_fp(FILE *fp,PKCS8_PRIV_KEY_INFO *p8inf);
int i2d_PKCS8PrivateKeyInfo_fp(FILE *fp, EVP_PKEY *key);
int i2d_PrivateKey_fp(FILE *fp, EVP_PKEY *pkey);
EVP_PKEY *d2i_PrivateKey_fp(FILE *fp, EVP_PKEY **a);
int i2d_PUBKEY_fp(FILE *fp, EVP_PKEY *pkey);
EVP_PKEY *d2i_PUBKEY_fp(FILE *fp, EVP_PKEY **a);
#endif

#ifndef OPENSSL_NO_BIO
X509 *d2i_X509_bio(BIO *bp,X509 **x509);
int i2d_X509_bio(BIO *bp,X509 *x509);
X509_CRL *d2i_X509_CRL_bio(BIO *bp,X509_CRL **crl);
int i2d_X509_CRL_bio(BIO *bp,X509_CRL *crl);
X509_REQ *d2i_X509_REQ_bio(BIO *bp,X509_REQ **req);
int i2d_X509_REQ_bio(BIO *bp,X509_REQ *req);
#ifndef OPENSSL_NO_RSA
RSA *d2i_RSAPrivateKey_bio(BIO *bp,RSA **rsa);
int i2d_RSAPrivateKey_bio(BIO *bp,RSA *rsa);
RSA *d2i_RSAPublicKey_bio(BIO *bp,RSA **rsa);
int i2d_RSAPublicKey_bio(BIO *bp,RSA *rsa);
RSA *d2i_RSA_PUBKEY_bio(BIO *bp,RSA **rsa);
int i2d_RSA_PUBKEY_bio(BIO *bp,RSA *rsa);
#endif
#ifndef OPENSSL_NO_DSA
DSA *d2i_DSA_PUBKEY_bio(BIO *bp, DSA **dsa);
int i2d_DSA_PUBKEY_bio(BIO *bp, DSA *dsa);
DSA *d2i_DSAPrivateKey_bio(BIO *bp, DSA **dsa);
int i2d_DSAPrivateKey_bio(BIO *bp, DSA *dsa);
#endif
#ifndef OPENSSL_NO_EC
EC_KEY *d2i_EC_PUBKEY_bio(BIO *bp, EC_KEY **eckey);
int   i2d_EC_PUBKEY_bio(BIO *bp, EC_KEY *eckey);
EC_KEY *d2i_ECPrivateKey_bio(BIO *bp, EC_KEY **eckey);
int   i2d_ECPrivateKey_bio(BIO *bp, EC_KEY *eckey);
#endif
X509_SIG *d2i_PKCS8_bio(BIO *bp,X509_SIG **p8);
int i2d_PKCS8_bio(BIO *bp,X509_SIG *p8);
PKCS8_PRIV_KEY_INFO *d2i_PKCS8_PRIV_KEY_INFO_bio(BIO *bp,
						PKCS8_PRIV_KEY_INFO **p8inf);
int i2d_PKCS8_PRIV_KEY_INFO_bio(BIO *bp,PKCS8_PRIV_KEY_INFO *p8inf);
int i2d_PKCS8PrivateKeyInfo_bio(BIO *bp, EVP_PKEY *key);
int i2d_PrivateKey_bio(BIO *bp, EVP_PKEY *pkey);
EVP_PKEY *d2i_PrivateKey_bio(BIO *bp, EVP_PKEY **a);
int i2d_PUBKEY_bio(BIO *bp, EVP_PKEY *pkey);
EVP_PKEY *d2i_PUBKEY_bio(BIO *bp, EVP_PKEY **a);
#endif

X509 *X509_dup(X509 *x509);
X509_ATTRIBUTE *X509_ATTRIBUTE_dup(X509_ATTRIBUTE *xa);
X509_EXTENSION *X509_EXTENSION_dup(X509_EXTENSION *ex);
X509_CRL *X509_CRL_dup(X509_CRL *crl);
X509_REQ *X509_REQ_dup(X509_REQ *req);
X509_ALGOR *X509_ALGOR_dup(X509_ALGOR *xn);
int X509_ALGOR_set0(X509_ALGOR *alg, ASN1_OBJECT *aobj, int ptype, void *pval);
void X509_ALGOR_get0(ASN1_OBJECT **paobj, int *pptype, void **ppval,
						X509_ALGOR *algor);

X509_NAME *X509_NAME_dup(X509_NAME *xn);
X509_NAME_ENTRY *X509_NAME_ENTRY_dup(X509_NAME_ENTRY *ne);

#endif /* !SSLEAY_MACROS */

int		X509_cmp_time(ASN1_TIME *s, time_t *t);
int		X509_cmp_current_time(ASN1_TIME *s);
ASN1_TIME *	X509_time_adj(ASN1_TIME *s, long adj, time_t *t);
ASN1_TIME *	X509_gmtime_adj(ASN1_TIME *s, long adj);

const char *	X509_get_default_cert_area(void );
const char *	X509_get_default_cert_dir(void );
const char *	X509_get_default_cert_file(void );
const char *	X509_get_default_cert_dir_env(void );
const char *	X509_get_default_cert_file_env(void );
const char *	X509_get_default_private_dir(void );

X509_REQ *	X509_to_X509_REQ(X509 *x, EVP_PKEY *pkey, const EVP_MD *md);
X509 *		X509_REQ_to_X509(X509_REQ *r, int days,EVP_PKEY *pkey);

DECLARE_ASN1_FUNCTIONS(X509_ALGOR)
DECLARE_ASN1_ENCODE_FUNCTIONS(X509_ALGORS, X509_ALGORS, X509_ALGORS)
DECLARE_ASN1_FUNCTIONS(X509_VAL)

DECLARE_ASN1_FUNCTIONS(X509_PUBKEY)

int		X509_PUBKEY_set(X509_PUBKEY **x, EVP_PKEY *pkey);
EVP_PKEY *	X509_PUBKEY_get(X509_PUBKEY *key);
int		X509_get_pubkey_parameters(EVP_PKEY *pkey,
					   STACK_OF(X509) *chain);
int		i2d_PUBKEY(EVP_PKEY *a,unsigned char **pp);
EVP_PKEY *	d2i_PUBKEY(EVP_PKEY **a,const unsigned char **pp,
			long length);
#ifndef OPENSSL_NO_RSA
int		i2d_RSA_PUBKEY(RSA *a,unsigned char **pp);
RSA *		d2i_RSA_PUBKEY(RSA **a,const unsigned char **pp,
			long length);
#endif
#ifndef OPENSSL_NO_DSA
int		i2d_DSA_PUBKEY(DSA *a,unsigned char **pp);
DSA *		d2i_DSA_PUBKEY(DSA **a,const unsigned char **pp,
			long length);
#endif
#ifndef OPENSSL_NO_EC
int		i2d_EC_PUBKEY(EC_KEY *a, unsigned char **pp);
EC_KEY 		*d2i_EC_PUBKEY(EC_KEY **a, const unsigned char **pp,
			long length);
#endif

DECLARE_ASN1_FUNCTIONS(X509_SIG)
DECLARE_ASN1_FUNCTIONS(X509_REQ_INFO)
DECLARE_ASN1_FUNCTIONS(X509_REQ)

DECLARE_ASN1_FUNCTIONS(X509_ATTRIBUTE)
X509_ATTRIBUTE *X509_ATTRIBUTE_create(int nid, int atrtype, void *value);

DECLARE_ASN1_FUNCTIONS(X509_EXTENSION)
DECLARE_ASN1_ENCODE_FUNCTIONS(X509_EXTENSIONS, X509_EXTENSIONS, X509_EXTENSIONS)

DECLARE_ASN1_FUNCTIONS(X509_NAME_ENTRY)

DECLARE_ASN1_FUNCTIONS(X509_NAME)

int		X509_NAME_set(X509_NAME **xn, X509_NAME *name);

DECLARE_ASN1_FUNCTIONS(X509_CINF)

DECLARE_ASN1_FUNCTIONS(X509)
DECLARE_ASN1_FUNCTIONS(X509_CERT_AUX)

DECLARE_ASN1_FUNCTIONS(X509_CERT_PAIR)

int X509_get_ex_new_index(long argl, void *argp, CRYPTO_EX_new *new_func,
	     CRYPTO_EX_dup *dup_func, CRYPTO_EX_free *free_func);
int X509_set_ex_data(X509 *r, int idx, void *arg);
void *X509_get_ex_data(X509 *r, int idx);
int		i2d_X509_AUX(X509 *a,unsigned char **pp);
X509 *		d2i_X509_AUX(X509 **a,const unsigned char **pp,long length);

int X509_alias_set1(X509 *x, unsigned char *name, int len);
int X509_keyid_set1(X509 *x, unsigned char *id, int len);
unsigned char * X509_alias_get0(X509 *x, int *len);
unsigned char * X509_keyid_get0(X509 *x, int *len);
int (*X509_TRUST_set_default(int (*trust)(int , X509 *, int)))(int, X509 *, int);
int X509_TRUST_set(int *t, int trust);
int X509_add1_trust_object(X509 *x, ASN1_OBJECT *obj);
int X509_add1_reject_object(X509 *x, ASN1_OBJECT *obj);
void X509_trust_clear(X509 *x);
void X509_reject_clear(X509 *x);

DECLARE_ASN1_FUNCTIONS(X509_REVOKED)
DECLARE_ASN1_FUNCTIONS(X509_CRL_INFO)
DECLARE_ASN1_FUNCTIONS(X509_CRL)

int X509_CRL_add0_revoked(X509_CRL *crl, X509_REVOKED *rev);

X509_PKEY *	X509_PKEY_new(void );
void		X509_PKEY_free(X509_PKEY *a);
int		i2d_X509_PKEY(X509_PKEY *a,unsigned char **pp);
X509_PKEY *	d2i_X509_PKEY(X509_PKEY **a,const unsigned char **pp,long length);

DECLARE_ASN1_FUNCTIONS(NETSCAPE_SPKI)
DECLARE_ASN1_FUNCTIONS(NETSCAPE_SPKAC)
DECLARE_ASN1_FUNCTIONS(NETSCAPE_CERT_SEQUENCE)

#ifndef OPENSSL_NO_EVP
X509_INFO *	X509_INFO_new(void);
void		X509_INFO_free(X509_INFO *a);
char *		X509_NAME_oneline(X509_NAME *a,char *buf,int size);

int ASN1_verify(i2d_of_void *i2d, X509_ALGOR *algor1,
		ASN1_BIT_STRING *signature,char *data,EVP_PKEY *pkey);

int ASN1_digest(i2d_of_void *i2d,const EVP_MD *type,char *data,
		unsigned char *md,unsigned int *len);

int ASN1_sign(i2d_of_void *i2d, X509_ALGOR *algor1,
	      X509_ALGOR *algor2, ASN1_BIT_STRING *signature,
	      char *data,EVP_PKEY *pkey, const EVP_MD *type);

int ASN1_item_digest(const ASN1_ITEM *it,const EVP_MD *type,void *data,
	unsigned char *md,unsigned int *len);

int ASN1_item_verify(const ASN1_ITEM *it, X509_ALGOR *algor1,
	ASN1_BIT_STRING *signature,void *data,EVP_PKEY *pkey);

int ASN1_item_sign(const ASN1_ITEM *it, X509_ALGOR *algor1, X509_ALGOR *algor2,
	ASN1_BIT_STRING *signature,
	void *data, EVP_PKEY *pkey, const EVP_MD *type);
#endif

int 		X509_set_version(X509 *x,long version);
int 		X509_set_serialNumber(X509 *x, ASN1_INTEGER *serial);
ASN1_INTEGER *	X509_get_serialNumber(X509 *x);
int 		X509_set_issuer_name(X509 *x, X509_NAME *name);
X509_NAME *	X509_get_issuer_name(X509 *a);
int 		X509_set_subject_name(X509 *x, X509_NAME *name);
X509_NAME *	X509_get_subject_name(X509 *a);
int 		X509_set_notBefore(X509 *x, ASN1_TIME *tm);
int 		X509_set_notAfter(X509 *x, ASN1_TIME *tm);
int 		X509_set_pubkey(X509 *x, EVP_PKEY *pkey);
EVP_PKEY *	X509_get_pubkey(X509 *x);
ASN1_BIT_STRING * X509_get0_pubkey_bitstr(const X509 *x);
int		X509_certificate_type(X509 *x,EVP_PKEY *pubkey /* optional */);

int		X509_REQ_set_version(X509_REQ *x,long version);
int		X509_REQ_set_subject_name(X509_REQ *req,X509_NAME *name);
int		X509_REQ_set_pubkey(X509_REQ *x, EVP_PKEY *pkey);
EVP_PKEY *	X509_REQ_get_pubkey(X509_REQ *req);
int		X509_REQ_extension_nid(int nid);
int *		X509_REQ_get_extension_nids(void);
void		X509_REQ_set_extension_nids(int *nids);
STACK_OF(X509_EXTENSION) *X509_REQ_get_extensions(X509_REQ *req);
int X509_REQ_add_extensions_nid(X509_REQ *req, STACK_OF(X509_EXTENSION) *exts,
				int nid);
int X509_REQ_add_extensions(X509_REQ *req, STACK_OF(X509_EXTENSION) *exts);
int X509_REQ_get_attr_count(const X509_REQ *req);
int X509_REQ_get_attr_by_NID(const X509_REQ *req, int nid,
			  int lastpos);
int X509_REQ_get_attr_by_OBJ(const X509_REQ *req, ASN1_OBJECT *obj,
			  int lastpos);
X509_ATTRIBUTE *X509_REQ_get_attr(const X509_REQ *req, int loc);
X509_ATTRIBUTE *X509_REQ_delete_attr(X509_REQ *req, int loc);
int X509_REQ_add1_attr(X509_REQ *req, X509_ATTRIBUTE *attr);
int X509_REQ_add1_attr_by_OBJ(X509_REQ *req,
			const ASN1_OBJECT *obj, int type,
			const unsigned char *bytes, int len);
int X509_REQ_add1_attr_by_NID(X509_REQ *req,
			int nid, int type,
			const unsigned char *bytes, int len);
int X509_REQ_add1_attr_by_txt(X509_REQ *req,
			const char *attrname, int type,
			const unsigned char *bytes, int len);

int X509_CRL_set_version(X509_CRL *x, long version);
int X509_CRL_set_issuer_name(X509_CRL *x, X509_NAME *name);
int X509_CRL_set_lastUpdate(X509_CRL *x, ASN1_TIME *tm);
int X509_CRL_set_nextUpdate(X509_CRL *x, ASN1_TIME *tm);
int X509_CRL_sort(X509_CRL *crl);

int X509_REVOKED_set_serialNumber(X509_REVOKED *x, ASN1_INTEGER *serial);
int X509_REVOKED_set_revocationDate(X509_REVOKED *r, ASN1_TIME *tm);

int		X509_REQ_check_private_key(X509_REQ *x509,EVP_PKEY *pkey);

int		X509_check_private_key(X509 *x509,EVP_PKEY *pkey);

int		X509_issuer_and_serial_cmp(const X509 *a, const X509 *b);
unsigned long	X509_issuer_and_serial_hash(X509 *a);

int		X509_issuer_name_cmp(const X509 *a, const X509 *b);
unsigned long	X509_issuer_name_hash(X509 *a);

int		X509_subject_name_cmp(const X509 *a, const X509 *b);
unsigned long	X509_subject_name_hash(X509 *x);

int		X509_cmp(const X509 *a, const X509 *b);
int		X509_NAME_cmp(const X509_NAME *a, const X509_NAME *b);
unsigned long	X509_NAME_hash(X509_NAME *x);

int		X509_CRL_cmp(const X509_CRL *a, const X509_CRL *b);
#ifndef OPENSSL_NO_FP_API
int		X509_print_ex_fp(FILE *bp,X509 *x, unsigned long nmflag, unsigned long cflag);
int		X509_print_fp(FILE *bp,X509 *x);
int		X509_CRL_print_fp(FILE *bp,X509_CRL *x);
int		X509_REQ_print_fp(FILE *bp,X509_REQ *req);
int X509_NAME_print_ex_fp(FILE *fp, X509_NAME *nm, int indent, unsigned long flags);
#endif

#ifndef OPENSSL_NO_BIO
int		X509_NAME_print(BIO *bp, X509_NAME *name, int obase);
int X509_NAME_print_ex(BIO *out, X509_NAME *nm, int indent, unsigned long flags);
int		X509_print_ex(BIO *bp,X509 *x, unsigned long nmflag, unsigned long cflag);
int		X509_print(BIO *bp,X509 *x);
int		X509_ocspid_print(BIO *bp,X509 *x);
int		X509_CERT_AUX_print(BIO *bp,X509_CERT_AUX *x, int indent);
int		X509_CRL_print(BIO *bp,X509_CRL *x);
int		X509_REQ_print_ex(BIO *bp, X509_REQ *x, unsigned long nmflag, unsigned long cflag);
int		X509_REQ_print(BIO *bp,X509_REQ *req);
#endif

int 		X509_NAME_entry_count(X509_NAME *name);
int 		X509_NAME_get_text_by_NID(X509_NAME *name, int nid,
			char *buf,int len);
int		X509_NAME_get_text_by_OBJ(X509_NAME *name, ASN1_OBJECT *obj,
			char *buf,int len);

/* NOTE: you should be passsing -1, not 0 as lastpos.  The functions that use
 * lastpos, search after that position on. */
int 		X509_NAME_get_index_by_NID(X509_NAME *name,int nid,int lastpos);
int 		X509_NAME_get_index_by_OBJ(X509_NAME *name,ASN1_OBJECT *obj,
			int lastpos);
X509_NAME_ENTRY *X509_NAME_get_entry(X509_NAME *name, int loc);
X509_NAME_ENTRY *X509_NAME_delete_entry(X509_NAME *name, int loc);
int 		X509_NAME_add_entry(X509_NAME *name,X509_NAME_ENTRY *ne,
			int loc, int set);
int X509_NAME_add_entry_by_OBJ(X509_NAME *name, ASN1_OBJECT *obj, int type,
			unsigned char *bytes, int len, int loc, int set);
int X509_NAME_add_entry_by_NID(X509_NAME *name, int nid, int type,
			unsigned char *bytes, int len, int loc, int set);
X509_NAME_ENTRY *X509_NAME_ENTRY_create_by_txt(X509_NAME_ENTRY **ne,
		const char *field, int type, const unsigned char *bytes, int len);
X509_NAME_ENTRY *X509_NAME_ENTRY_create_by_NID(X509_NAME_ENTRY **ne, int nid,
			int type,unsigned char *bytes, int len);
int X509_NAME_add_entry_by_txt(X509_NAME *name, const char *field, int type,
			const unsigned char *bytes, int len, int loc, int set);
X509_NAME_ENTRY *X509_NAME_ENTRY_create_by_OBJ(X509_NAME_ENTRY **ne,
			ASN1_OBJECT *obj, int type,const unsigned char *bytes,
			int len);
int 		X509_NAME_ENTRY_set_object(X509_NAME_ENTRY *ne,
			ASN1_OBJECT *obj);
int 		X509_NAME_ENTRY_set_data(X509_NAME_ENTRY *ne, int type,
			const unsigned char *bytes, int len);
ASN1_OBJECT *	X509_NAME_ENTRY_get_object(X509_NAME_ENTRY *ne);
ASN1_STRING *	X509_NAME_ENTRY_get_data(X509_NAME_ENTRY *ne);

int		X509v3_get_ext_count(const STACK_OF(X509_EXTENSION) *x);
int		X509v3_get_ext_by_NID(const STACK_OF(X509_EXTENSION) *x,
				      int nid, int lastpos);
int		X509v3_get_ext_by_OBJ(const STACK_OF(X509_EXTENSION) *x,
				      ASN1_OBJECT *obj,int lastpos);
int		X509v3_get_ext_by_critical(const STACK_OF(X509_EXTENSION) *x,
					   int crit, int lastpos);
X509_EXTENSION *X509v3_get_ext(const STACK_OF(X509_EXTENSION) *x, int loc);
X509_EXTENSION *X509v3_delete_ext(STACK_OF(X509_EXTENSION) *x, int loc);
STACK_OF(X509_EXTENSION) *X509v3_add_ext(STACK_OF(X509_EXTENSION) **x,
					 X509_EXTENSION *ex, int loc);

int		X509_get_ext_count(X509 *x);
int		X509_get_ext_by_NID(X509 *x, int nid, int lastpos);
int		X509_get_ext_by_OBJ(X509 *x,ASN1_OBJECT *obj,int lastpos);
int		X509_get_ext_by_critical(X509 *x, int crit, int lastpos);
X509_EXTENSION *X509_get_ext(X509 *x, int loc);
X509_EXTENSION *X509_delete_ext(X509 *x, int loc);
int		X509_add_ext(X509 *x, X509_EXTENSION *ex, int loc);
void	*	X509_get_ext_d2i(X509 *x, int nid, int *crit, int *idx);
int		X509_add1_ext_i2d(X509 *x, int nid, void *value, int crit,
							unsigned long flags);

int		X509_CRL_get_ext_count(X509_CRL *x);
int		X509_CRL_get_ext_by_NID(X509_CRL *x, int nid, int lastpos);
int		X509_CRL_get_ext_by_OBJ(X509_CRL *x,ASN1_OBJECT *obj,int lastpos);
int		X509_CRL_get_ext_by_critical(X509_CRL *x, int crit, int lastpos);
X509_EXTENSION *X509_CRL_get_ext(X509_CRL *x, int loc);
X509_EXTENSION *X509_CRL_delete_ext(X509_CRL *x, int loc);
int		X509_CRL_add_ext(X509_CRL *x, X509_EXTENSION *ex, int loc);
void	*	X509_CRL_get_ext_d2i(X509_CRL *x, int nid, int *crit, int *idx);
int		X509_CRL_add1_ext_i2d(X509_CRL *x, int nid, void *value, int crit,
							unsigned long flags);

int		X509_REVOKED_get_ext_count(X509_REVOKED *x);
int		X509_REVOKED_get_ext_by_NID(X509_REVOKED *x, int nid, int lastpos);
int		X509_REVOKED_get_ext_by_OBJ(X509_REVOKED *x,ASN1_OBJECT *obj,int lastpos);
int		X509_REVOKED_get_ext_by_critical(X509_REVOKED *x, int crit, int lastpos);
X509_EXTENSION *X509_REVOKED_get_ext(X509_REVOKED *x, int loc);
X509_EXTENSION *X509_REVOKED_delete_ext(X509_REVOKED *x, int loc);
int		X509_REVOKED_add_ext(X509_REVOKED *x, X509_EXTENSION *ex, int loc);
void	*	X509_REVOKED_get_ext_d2i(X509_REVOKED *x, int nid, int *crit, int *idx);
int		X509_REVOKED_add1_ext_i2d(X509_REVOKED *x, int nid, void *value, int crit,
							unsigned long flags);

X509_EXTENSION *X509_EXTENSION_create_by_NID(X509_EXTENSION **ex,
			int nid, int crit, ASN1_OCTET_STRING *data);
X509_EXTENSION *X509_EXTENSION_create_by_OBJ(X509_EXTENSION **ex,
			ASN1_OBJECT *obj,int crit,ASN1_OCTET_STRING *data);
int		X509_EXTENSION_set_object(X509_EXTENSION *ex,ASN1_OBJECT *obj);
int		X509_EXTENSION_set_critical(X509_EXTENSION *ex, int crit);
int		X509_EXTENSION_set_data(X509_EXTENSION *ex,
			ASN1_OCTET_STRING *data);
ASN1_OBJECT *	X509_EXTENSION_get_object(X509_EXTENSION *ex);
ASN1_OCTET_STRING *X509_EXTENSION_get_data(X509_EXTENSION *ne);
int		X509_EXTENSION_get_critical(X509_EXTENSION *ex);

int X509at_get_attr_count(const STACK_OF(X509_ATTRIBUTE) *x);
int X509at_get_attr_by_NID(const STACK_OF(X509_ATTRIBUTE) *x, int nid,
			  int lastpos);
int X509at_get_attr_by_OBJ(const STACK_OF(X509_ATTRIBUTE) *sk, ASN1_OBJECT *obj,
			  int lastpos);
X509_ATTRIBUTE *X509at_get_attr(const STACK_OF(X509_ATTRIBUTE) *x, int loc);
X509_ATTRIBUTE *X509at_delete_attr(STACK_OF(X509_ATTRIBUTE) *x, int loc);
STACK_OF(X509_ATTRIBUTE) *X509at_add1_attr(STACK_OF(X509_ATTRIBUTE) **x,
					 X509_ATTRIBUTE *attr);
STACK_OF(X509_ATTRIBUTE) *X509at_add1_attr_by_OBJ(STACK_OF(X509_ATTRIBUTE) **x,
			const ASN1_OBJECT *obj, int type,
			const unsigned char *bytes, int len);
STACK_OF(X509_ATTRIBUTE) *X509at_add1_attr_by_NID(STACK_OF(X509_ATTRIBUTE) **x,
			int nid, int type,
			const unsigned char *bytes, int len);
STACK_OF(X509_ATTRIBUTE) *X509at_add1_attr_by_txt(STACK_OF(X509_ATTRIBUTE) **x,
			const char *attrname, int type,
			const unsigned char *bytes, int len);
void *X509at_get0_data_by_OBJ(STACK_OF(X509_ATTRIBUTE) *x,
				ASN1_OBJECT *obj, int lastpos, int type);
X509_ATTRIBUTE *X509_ATTRIBUTE_create_by_NID(X509_ATTRIBUTE **attr, int nid,
	     int atrtype, const void *data, int len);
X509_ATTRIBUTE *X509_ATTRIBUTE_create_by_OBJ(X509_ATTRIBUTE **attr,
	     const ASN1_OBJECT *obj, int atrtype, const void *data, int len);
X509_ATTRIBUTE *X509_ATTRIBUTE_create_by_txt(X509_ATTRIBUTE **attr,
		const char *atrname, int type, const unsigned char *bytes, int len);
int X509_ATTRIBUTE_set1_object(X509_ATTRIBUTE *attr, const ASN1_OBJECT *obj);
int X509_ATTRIBUTE_set1_data(X509_ATTRIBUTE *attr, int attrtype, const void *data, int len);
void *X509_ATTRIBUTE_get0_data(X509_ATTRIBUTE *attr, int idx,
					int atrtype, void *data);
int X509_ATTRIBUTE_count(X509_ATTRIBUTE *attr);
ASN1_OBJECT *X509_ATTRIBUTE_get0_object(X509_ATTRIBUTE *attr);
ASN1_TYPE *X509_ATTRIBUTE_get0_type(X509_ATTRIBUTE *attr, int idx);

int EVP_PKEY_get_attr_count(const EVP_PKEY *key);
int EVP_PKEY_get_attr_by_NID(const EVP_PKEY *key, int nid,
			  int lastpos);
int EVP_PKEY_get_attr_by_OBJ(const EVP_PKEY *key, ASN1_OBJECT *obj,
			  int lastpos);
X509_ATTRIBUTE *EVP_PKEY_get_attr(const EVP_PKEY *key, int loc);
X509_ATTRIBUTE *EVP_PKEY_delete_attr(EVP_PKEY *key, int loc);
int EVP_PKEY_add1_attr(EVP_PKEY *key, X509_ATTRIBUTE *attr);
int EVP_PKEY_add1_attr_by_OBJ(EVP_PKEY *key,
			const ASN1_OBJECT *obj, int type,
			const unsigned char *bytes, int len);
int EVP_PKEY_add1_attr_by_NID(EVP_PKEY *key,
			int nid, int type,
			const unsigned char *bytes, int len);
int EVP_PKEY_add1_attr_by_txt(EVP_PKEY *key,
			const char *attrname, int type,
			const unsigned char *bytes, int len);

int		X509_verify_cert(X509_STORE_CTX *ctx);

/* lookup a cert from a X509 STACK */
X509 *X509_find_by_issuer_and_serial(STACK_OF(X509) *sk,X509_NAME *name,
				     ASN1_INTEGER *serial);
X509 *X509_find_by_subject(STACK_OF(X509) *sk,X509_NAME *name);

DECLARE_ASN1_FUNCTIONS(PBEPARAM)
DECLARE_ASN1_FUNCTIONS(PBE2PARAM)
DECLARE_ASN1_FUNCTIONS(PBKDF2PARAM)

X509_ALGOR *PKCS5_pbe_set(int alg, int iter, unsigned char *salt, int saltlen);
X509_ALGOR *PKCS5_pbe2_set(const EVP_CIPHER *cipher, int iter,
					 unsigned char *salt, int saltlen);

/* PKCS#8 utilities */

DECLARE_ASN1_FUNCTIONS(PKCS8_PRIV_KEY_INFO)

EVP_PKEY *EVP_PKCS82PKEY(PKCS8_PRIV_KEY_INFO *p8);
PKCS8_PRIV_KEY_INFO *EVP_PKEY2PKCS8(EVP_PKEY *pkey);
PKCS8_PRIV_KEY_INFO *EVP_PKEY2PKCS8_broken(EVP_PKEY *pkey, int broken);
PKCS8_PRIV_KEY_INFO *PKCS8_set_broken(PKCS8_PRIV_KEY_INFO *p8, int broken);

int X509_check_trust(X509 *x, int id, int flags);
int X509_TRUST_get_count(void);
X509_TRUST * X509_TRUST_get0(int idx);
int X509_TRUST_get_by_id(int id);
int X509_TRUST_add(int id, int flags, int (*ck)(X509_TRUST *, X509 *, int),
					char *name, int arg1, void *arg2);
void X509_TRUST_cleanup(void);
int X509_TRUST_get_flags(X509_TRUST *xp);
char *X509_TRUST_get0_name(X509_TRUST *xp);
int X509_TRUST_get_trust(X509_TRUST *xp);

/* BEGIN ERROR CODES */
/* The following lines are auto generated by the script mkerr.pl. Any changes
 * made after this point may be overwritten when the script is next run.
 */
void ERR_load_X509_strings(void);

/* Error codes for the X509 functions. */

/* Function codes. */
#define X509_F_ADD_CERT_DIR				 100
#define X509_F_BY_FILE_CTRL				 101
#define X509_F_CHECK_POLICY				 145
#define X509_F_DIR_CTRL					 102
#define X509_F_GET_CERT_BY_SUBJECT			 103
#define X509_F_NETSCAPE_SPKI_B64_DECODE			 129
#define X509_F_NETSCAPE_SPKI_B64_ENCODE			 130
#define X509_F_X509AT_ADD1_ATTR				 135
#define X509_F_X509V3_ADD_EXT				 104
#define X509_F_X509_ATTRIBUTE_CREATE_BY_NID		 136
#define X509_F_X509_ATTRIBUTE_CREATE_BY_OBJ		 137
#define X509_F_X509_ATTRIBUTE_CREATE_BY_TXT		 140
#define X509_F_X509_ATTRIBUTE_GET0_DATA			 139
#define X509_F_X509_ATTRIBUTE_SET1_DATA			 138
#define X509_F_X509_CHECK_PRIVATE_KEY			 128
#define X509_F_X509_CRL_PRINT_FP			 147
#define X509_F_X509_EXTENSION_CREATE_BY_NID		 108
#define X509_F_X509_EXTENSION_CREATE_BY_OBJ		 109
#define X509_F_X509_GET_PUBKEY_PARAMETERS		 110
#define X509_F_X509_LOAD_CERT_CRL_FILE			 132
#define X509_F_X509_LOAD_CERT_FILE			 111
#define X509_F_X509_LOAD_CRL_FILE			 112
#define X509_F_X509_NAME_ADD_ENTRY			 113
#define X509_F_X509_NAME_ENTRY_CREATE_BY_NID		 114
#define X509_F_X509_NAME_ENTRY_CREATE_BY_TXT		 131
#define X509_F_X509_NAME_ENTRY_SET_OBJECT		 115
#define X509_F_X509_NAME_ONELINE			 116
#define X509_F_X509_NAME_PRINT				 117
#define X509_F_X509_PRINT_EX_FP				 118
#define X509_F_X509_PUBKEY_GET				 119
#define X509_F_X509_PUBKEY_SET				 120
#define X509_F_X509_REQ_CHECK_PRIVATE_KEY		 144
#define X509_F_X509_REQ_PRINT_EX			 121
#define X509_F_X509_REQ_PRINT_FP			 122
#define X509_F_X509_REQ_TO_X509				 123
#define X509_F_X509_STORE_ADD_CERT			 124
#define X509_F_X509_STORE_ADD_CRL			 125
#define X509_F_X509_STORE_CTX_GET1_ISSUER		 146
#define X509_F_X509_STORE_CTX_INIT			 143
#define X509_F_X509_STORE_CTX_NEW			 142
#define X509_F_X509_STORE_CTX_PURPOSE_INHERIT		 134
#define X509_F_X509_TO_X509_REQ				 126
#define X509_F_X509_TRUST_ADD				 133
#define X509_F_X509_TRUST_SET				 141
#define X509_F_X509_VERIFY_CERT				 127

/* Reason codes. */
#define X509_R_BAD_X509_FILETYPE			 100
#define X509_R_BASE64_DECODE_ERROR			 118
#define X509_R_CANT_CHECK_DH_KEY			 114
#define X509_R_CERT_ALREADY_IN_HASH_TABLE		 101
#define X509_R_ERR_ASN1_LIB				 102
#define X509_R_INVALID_DIRECTORY			 113
#define X509_R_INVALID_FIELD_NAME			 119
#define X509_R_INVALID_TRUST				 123
#define X509_R_KEY_TYPE_MISMATCH			 115
#define X509_R_KEY_VALUES_MISMATCH			 116
#define X509_R_LOADING_CERT_DIR				 103
#define X509_R_LOADING_DEFAULTS				 104
#define X509_R_NO_CERT_SET_FOR_US_TO_VERIFY		 105
#define X509_R_SHOULD_RETRY				 106
#define X509_R_UNABLE_TO_FIND_PARAMETERS_IN_CHAIN	 107
#define X509_R_UNABLE_TO_GET_CERTS_PUBLIC_KEY		 108
#define X509_R_UNKNOWN_KEY_TYPE				 117
#define X509_R_UNKNOWN_NID				 109
#define X509_R_UNKNOWN_PURPOSE_ID			 121
#define X509_R_UNKNOWN_TRUST_ID				 120
#define X509_R_UNSUPPORTED_ALGORITHM			 111
#define X509_R_WRONG_LOOKUP_TYPE			 112
#define X509_R_WRONG_TYPE				 122

#ifdef  __cplusplus
}
#endif
#endif
