/* crypto/pem/pem.h */
/* Copyright (C) 1995-1997 Eric Young (eay@cryptsoft.com)
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

#ifndef HEADER_PEM_H
#define HEADER_PEM_H

#include <openssl/e_os2.h>
#ifndef OPENSSL_NO_BIO
#include <openssl/bio.h>
#endif
#ifndef OPENSSL_NO_STACK
#include <openssl/stack.h>
#endif
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/pem2.h>

#ifdef  __cplusplus
extern "C" {
#endif

#define PEM_BUFSIZE		1024

#define PEM_OBJ_UNDEF		0
#define PEM_OBJ_X509		1
#define PEM_OBJ_X509_REQ	2
#define PEM_OBJ_CRL		3
#define PEM_OBJ_SSL_SESSION	4
#define PEM_OBJ_PRIV_KEY	10
#define PEM_OBJ_PRIV_RSA	11
#define PEM_OBJ_PRIV_DSA	12
#define PEM_OBJ_PRIV_DH		13
#define PEM_OBJ_PUB_RSA		14
#define PEM_OBJ_PUB_DSA		15
#define PEM_OBJ_PUB_DH		16
#define PEM_OBJ_DHPARAMS	17
#define PEM_OBJ_DSAPARAMS	18
#define PEM_OBJ_PRIV_RSA_PUBLIC	19
#define PEM_OBJ_PRIV_ECDSA	20
#define PEM_OBJ_PUB_ECDSA	21
#define PEM_OBJ_ECPARAMETERS	22

#define PEM_ERROR		30
#define PEM_DEK_DES_CBC         40
#define PEM_DEK_IDEA_CBC        45
#define PEM_DEK_DES_EDE         50
#define PEM_DEK_DES_ECB         60
#define PEM_DEK_RSA             70
#define PEM_DEK_RSA_MD2         80
#define PEM_DEK_RSA_MD5         90

#define PEM_MD_MD2		NID_md2
#define PEM_MD_MD5		NID_md5
#define PEM_MD_SHA		NID_sha
#define PEM_MD_MD2_RSA		NID_md2WithRSAEncryption
#define PEM_MD_MD5_RSA		NID_md5WithRSAEncryption
#define PEM_MD_SHA_RSA		NID_sha1WithRSAEncryption

#define PEM_STRING_X509_OLD	"X509 CERTIFICATE"
#define PEM_STRING_X509		"CERTIFICATE"
#define PEM_STRING_X509_PAIR	"CERTIFICATE PAIR"
#define PEM_STRING_X509_TRUSTED	"TRUSTED CERTIFICATE"
#define PEM_STRING_X509_REQ_OLD	"NEW CERTIFICATE REQUEST"
#define PEM_STRING_X509_REQ	"CERTIFICATE REQUEST"
#define PEM_STRING_X509_CRL	"X509 CRL"
#define PEM_STRING_EVP_PKEY	"ANY PRIVATE KEY"
#define PEM_STRING_PUBLIC	"PUBLIC KEY"
#define PEM_STRING_RSA		"RSA PRIVATE KEY"
#define PEM_STRING_RSA_PUBLIC	"RSA PUBLIC KEY"
#define PEM_STRING_DSA		"DSA PRIVATE KEY"
#define PEM_STRING_DSA_PUBLIC	"DSA PUBLIC KEY"
#define PEM_STRING_PKCS7	"PKCS7"
#define PEM_STRING_PKCS7_SIGNED	"PKCS #7 SIGNED DATA"
#define PEM_STRING_PKCS8	"ENCRYPTED PRIVATE KEY"
#define PEM_STRING_PKCS8INF	"PRIVATE KEY"
#define PEM_STRING_DHPARAMS	"DH PARAMETERS"
#define PEM_STRING_SSL_SESSION	"SSL SESSION PARAMETERS"
#define PEM_STRING_DSAPARAMS	"DSA PARAMETERS"
#define PEM_STRING_ECDSA_PUBLIC "ECDSA PUBLIC KEY"
#define PEM_STRING_ECPARAMETERS "EC PARAMETERS"
#define PEM_STRING_ECPRIVATEKEY	"EC PRIVATE KEY"
#define PEM_STRING_CMS		"CMS"

  /* Note that this structure is initialised by PEM_SealInit and cleaned up
     by PEM_SealFinal (at least for now) */
typedef struct PEM_Encode_Seal_st
	{
	EVP_ENCODE_CTX encode;
	EVP_MD_CTX md;
	EVP_CIPHER_CTX cipher;
	} PEM_ENCODE_SEAL_CTX;

/* enc_type is one off */
#define PEM_TYPE_ENCRYPTED      10
#define PEM_TYPE_MIC_ONLY       20
#define PEM_TYPE_MIC_CLEAR      30
#define PEM_TYPE_CLEAR		40

typedef struct pem_recip_st
	{
	char *name;
	X509_NAME *dn;

	int cipher;
	int key_enc;
	/*	char iv[8]; unused and wrong size */
	} PEM_USER;

typedef struct pem_ctx_st
	{
	int type;		/* what type of object */

	struct	{
		int version;	
		int mode;		
		} proc_type;

	char *domain;

	struct	{
		int cipher;
	/* unused, and wrong size
	   unsigned char iv[8]; */
		} DEK_info;
		
	PEM_USER *originator;

	int num_recipient;
	PEM_USER **recipient;

#ifndef OPENSSL_NO_STACK
	STACK *x509_chain;	/* certificate chain */
#else
	char *x509_chain;	/* certificate chain */
#endif
	EVP_MD *md;		/* signature type */

	int md_enc;		/* is the md encrypted or not? */
	int md_len;		/* length of md_data */
	char *md_data;		/* message digest, could be pkey encrypted */

	EVP_CIPHER *dec;	/* date encryption cipher */
	int key_len;		/* key length */
	unsigned char *key;	/* key */
	/* unused, and wrong size
	   unsigned char iv[8]; */

	
	int  data_enc;		/* is the data encrypted */
	int data_len;
	unsigned char *data;
	} PEM_CTX;

/* These macros make the PEM_read/PEM_write functions easier to maintain and
 * write. Now they are all implemented with either:
 * IMPLEMENT_PEM_rw(...) or IMPLEMENT_PEM_rw_cb(...)
 */

#ifdef OPENSSL_NO_FP_API

#define IMPLEMENT_PEM_read_fp(name, type, str, asn1) /**/
#define IMPLEMENT_PEM_write_fp(name, type, str, asn1) /**/
#define IMPLEMENT_PEM_write_fp_const(name, type, str, asn1) /**/
#define IMPLEMENT_PEM_write_cb_fp(name, type, str, asn1) /**/
#define IMPLEMENT_PEM_write_cb_fp_const(name, type, str, asn1) /**/

#else

#define IMPLEMENT_PEM_read_fp(name, type, str, asn1) \
type *PEM_read_##name(FILE *fp, type **x, pem_password_cb *cb, void *u)\
{ \
    return (type*)PEM_ASN1_read(CHECKED_D2I_OF(type, d2i_##asn1), \
				str, fp, \
				CHECKED_PPTR_OF(type, x), \
				cb, u); \
} 

#define IMPLEMENT_PEM_write_fp(name, type, str, asn1) \
int PEM_write_##name(FILE *fp, type *x) \
{ \
    return PEM_ASN1_write(CHECKED_I2D_OF(type, i2d_##asn1), \
			  str, fp, \
			  CHECKED_PTR_OF(type, x), \
			  NULL, NULL, 0, NULL, NULL); \
}

#define IMPLEMENT_PEM_write_fp_const(name, type, str, asn1) \
int PEM_write_##name(FILE *fp, const type *x) \
{ \
    return PEM_ASN1_write(CHECKED_I2D_OF(const type, i2d_##asn1), \
			  str, fp, \
			  CHECKED_PTR_OF(const type, x), \
			  NULL, NULL, 0, NULL, NULL); \
}

#define IMPLEMENT_PEM_write_cb_fp(name, type, str, asn1) \
int PEM_write_##name(FILE *fp, type *x, const EVP_CIPHER *enc, \
	     unsigned char *kstr, int klen, pem_password_cb *cb, \
		  void *u) \
	{ \
	    return PEM_ASN1_write(CHECKED_I2D_OF(type, i2d_##asn1), \
				  str, fp, \
				  CHECKED_PTR_OF(type, x), \
				  enc, kstr, klen, cb, u); \
	}

#define IMPLEMENT_PEM_write_cb_fp_const(name, type, str, asn1) \
int PEM_write_##name(FILE *fp, type *x, const EVP_CIPHER *enc, \
	     unsigned char *kstr, int klen, pem_password_cb *cb, \
		  void *u) \
	{ \
	    return PEM_ASN1_write(CHECKED_I2D_OF(const type, i2d_##asn1), \
				  str, fp, \
				  CHECKED_PTR_OF(const type, x), \
				  enc, kstr, klen, cb, u); \
	}

#endif

#define IMPLEMENT_PEM_read_bio(name, type, str, asn1) \
type *PEM_read_bio_##name(BIO *bp, type **x, pem_password_cb *cb, void *u)\
{ \
    return (type*)PEM_ASN1_read_bio(CHECKED_D2I_OF(type, d2i_##asn1), \
				    str, bp, \
				    CHECKED_PPTR_OF(type, x), \
				    cb, u); \
}

#define IMPLEMENT_PEM_write_bio(name, type, str, asn1) \
int PEM_write_bio_##name(BIO *bp, type *x) \
{ \
    return PEM_ASN1_write_bio(CHECKED_I2D_OF(type, i2d_##asn1), \
			      str, bp, \
			      CHECKED_PTR_OF(type, x), \
			      NULL, NULL, 0, NULL, NULL); \
}

#define IMPLEMENT_PEM_write_bio_const(name, type, str, asn1) \
int PEM_write_bio_##name(BIO *bp, const type *x) \
{ \
    return PEM_ASN1_write_bio(CHECKED_I2D_OF(const type, i2d_##asn1), \
			      str, bp, \
			      CHECKED_PTR_OF(const type, x), \
			      NULL, NULL, 0, NULL, NULL); \
}

#define IMPLEMENT_PEM_write_cb_bio(name, type, str, asn1) \
int PEM_write_bio_##name(BIO *bp, type *x, const EVP_CIPHER *enc, \
	     unsigned char *kstr, int klen, pem_password_cb *cb, void *u) \
	{ \
	    return PEM_ASN1_write_bio(CHECKED_I2D_OF(type, i2d_##asn1), \
				      str, bp, \
				      CHECKED_PTR_OF(type, x), \
				      enc, kstr, klen, cb, u); \
	}

#define IMPLEMENT_PEM_write_cb_bio_const(name, type, str, asn1) \
int PEM_write_bio_##name(BIO *bp, type *x, const EVP_CIPHER *enc, \
	     unsigned char *kstr, int klen, pem_password_cb *cb, void *u) \
	{ \
	    return PEM_ASN1_write_bio(CHECKED_I2D_OF(const type, i2d_##asn1), \
				      str, bp, \
				      CHECKED_PTR_OF(const type, x), \
				      enc, kstr, klen, cb, u); \
	}

#define IMPLEMENT_PEM_write(name, type, str, asn1) \
	IMPLEMENT_PEM_write_bio(name, type, str, asn1) \
	IMPLEMENT_PEM_write_fp(name, type, str, asn1) 

#define IMPLEMENT_PEM_write_const(name, type, str, asn1) \
	IMPLEMENT_PEM_write_bio_const(name, type, str, asn1) \
	IMPLEMENT_PEM_write_fp_const(name, type, str, asn1) 

#define IMPLEMENT_PEM_write_cb(name, type, str, asn1) \
	IMPLEMENT_PEM_write_cb_bio(name, type, str, asn1) \
	IMPLEMENT_PEM_write_cb_fp(name, type, str, asn1) 

#define IMPLEMENT_PEM_write_cb_const(name, type, str, asn1) \
	IMPLEMENT_PEM_write_cb_bio_const(name, type, str, asn1) \
	IMPLEMENT_PEM_write_cb_fp_const(name, type, str, asn1) 

#define IMPLEMENT_PEM_read(name, type, str, asn1) \
	IMPLEMENT_PEM_read_bio(name, type, str, asn1) \
	IMPLEMENT_PEM_read_fp(name, type, str, asn1) 

#define IMPLEMENT_PEM_rw(name, type, str, asn1) \
	IMPLEMENT_PEM_read(name, type, str, asn1) \
	IMPLEMENT_PEM_write(name, type, str, asn1)

#define IMPLEMENT_PEM_rw_const(name, type, str, asn1) \
	IMPLEMENT_PEM_read(name, type, str, asn1) \
	IMPLEMENT_PEM_write_const(name, type, str, asn1)

#define IMPLEMENT_PEM_rw_cb(name, type, str, asn1) \
	IMPLEMENT_PEM_read(name, type, str, asn1) \
	IMPLEMENT_PEM_write_cb(name, type, str, asn1)

/* These are the same except they are for the declarations */

#if defined(OPENSSL_SYS_WIN16) || defined(OPENSSL_NO_FP_API)

#define DECLARE_PEM_read_fp(name, type) /**/
#define DECLARE_PEM_write_fp(name, type) /**/
#define DECLARE_PEM_write_fp_const(name, type) /**/
#define DECLARE_PEM_write_cb_fp(name, type) /**/

#else

#define DECLARE_PEM_read_fp(name, type) \
	type *PEM_read_##name(FILE *fp, type **x, pem_password_cb *cb, void *u);

#define DECLARE_PEM_write_fp(name, type) \
	int PEM_write_##name(FILE *fp, type *x);

#define DECLARE_PEM_write_fp_const(name, type) \
	int PEM_write_##name(FILE *fp, const type *x);

#define DECLARE_PEM_write_cb_fp(name, type) \
	int PEM_write_##name(FILE *fp, type *x, const EVP_CIPHER *enc, \
	     unsigned char *kstr, int klen, pem_password_cb *cb, void *u);

#endif

#ifndef OPENSSL_NO_BIO
#define DECLARE_PEM_read_bio(name, type) \
	type *PEM_read_bio_##name(BIO *bp, type **x, pem_password_cb *cb, void *u);

#define DECLARE_PEM_write_bio(name, type) \
	int PEM_write_bio_##name(BIO *bp, type *x);

#define DECLARE_PEM_write_bio_const(name, type) \
	int PEM_write_bio_##name(BIO *bp, const type *x);

#define DECLARE_PEM_write_cb_bio(name, type) \
	int PEM_write_bio_##name(BIO *bp, type *x, const EVP_CIPHER *enc, \
	     unsigned char *kstr, int klen, pem_password_cb *cb, void *u);

#else

#define DECLARE_PEM_read_bio(name, type) /**/
#define DECLARE_PEM_write_bio(name, type) /**/
#define DECLARE_PEM_write_bio_const(name, type) /**/
#define DECLARE_PEM_write_cb_bio(name, type) /**/

#endif

#define DECLARE_PEM_write(name, type) \
	DECLARE_PEM_write_bio(name, type) \
	DECLARE_PEM_write_fp(name, type) 

#define DECLARE_PEM_write_const(name, type) \
	DECLARE_PEM_write_bio_const(name, type) \
	DECLARE_PEM_write_fp_const(name, type)

#define DECLARE_PEM_write_cb(name, type) \
	DECLARE_PEM_write_cb_bio(name, type) \
	DECLARE_PEM_write_cb_fp(name, type) 

#define DECLARE_PEM_read(name, type) \
	DECLARE_PEM_read_bio(name, type) \
	DECLARE_PEM_read_fp(name, type)

#define DECLARE_PEM_rw(name, type) \
	DECLARE_PEM_read(name, type) \
	DECLARE_PEM_write(name, type)

#define DECLARE_PEM_rw_const(name, type) \
	DECLARE_PEM_read(name, type) \
	DECLARE_PEM_write_const(name, type)

#define DECLARE_PEM_rw_cb(name, type) \
	DECLARE_PEM_read(name, type) \
	DECLARE_PEM_write_cb(name, type)

#ifdef SSLEAY_MACROS

#define PEM_write_SSL_SESSION(fp,x) \
		PEM_ASN1_write((int (*)())i2d_SSL_SESSION, \
			PEM_STRING_SSL_SESSION,fp, (char *)x, NULL,NULL,0,NULL,NULL)
#define PEM_write_X509(fp,x) \
		PEM_ASN1_write((int (*)())i2d_X509,PEM_STRING_X509,fp, \
			(char *)x, NULL,NULL,0,NULL,NULL)
#define PEM_write_X509_REQ(fp,x) PEM_ASN1_write( \
		(int (*)())i2d_X509_REQ,PEM_STRING_X509_REQ,fp,(char *)x, \
			NULL,NULL,0,NULL,NULL)
#define PEM_write_X509_CRL(fp,x) \
		PEM_ASN1_write((int (*)())i2d_X509_CRL,PEM_STRING_X509_CRL, \
			fp,(char *)x, NULL,NULL,0,NULL,NULL)
#define	PEM_write_RSAPrivateKey(fp,x,enc,kstr,klen,cb,u) \
		PEM_ASN1_write((int (*)())i2d_RSAPrivateKey,PEM_STRING_RSA,fp,\
			(char *)x,enc,kstr,klen,cb,u)
#define	PEM_write_RSAPublicKey(fp,x) \
		PEM_ASN1_write((int (*)())i2d_RSAPublicKey,\
			PEM_STRING_RSA_PUBLIC,fp,(char *)x,NULL,NULL,0,NULL,NULL)
#define	PEM_write_DSAPrivateKey(fp,x,enc,kstr,klen,cb,u) \
		PEM_ASN1_write((int (*)())i2d_DSAPrivateKey,PEM_STRING_DSA,fp,\
			(char *)x,enc,kstr,klen,cb,u)
#define	PEM_write_PrivateKey(bp,x,enc,kstr,klen,cb,u) \
		PEM_ASN1_write((int (*)())i2d_PrivateKey,\
		(((x)->type == EVP_PKEY_DSA)?PEM_STRING_DSA:PEM_STRING_RSA),\
			bp,(char *)x,enc,kstr,klen,cb,u)
#define PEM_write_PKCS7(fp,x) \
		PEM_ASN1_write((int (*)())i2d_PKCS7,PEM_STRING_PKCS7,fp, \
			(char *)x, NULL,NULL,0,NULL,NULL)
#define PEM_write_DHparams(fp,x) \
		PEM_ASN1_write((int (*)())i2d_DHparams,PEM_STRING_DHPARAMS,fp,\
			(char *)x,NULL,NULL,0,NULL,NULL)

#define PEM_write_NETSCAPE_CERT_SEQUENCE(fp,x) \
                PEM_ASN1_write((int (*)())i2d_NETSCAPE_CERT_SEQUENCE, \
			PEM_STRING_X509,fp, \
                        (char *)x, NULL,NULL,0,NULL,NULL)

#define	PEM_read_SSL_SESSION(fp,x,cb,u) (SSL_SESSION *)PEM_ASN1_read( \
	(char *(*)())d2i_SSL_SESSION,PEM_STRING_SSL_SESSION,fp,(char **)x,cb,u)
#define	PEM_read_X509(fp,x,cb,u) (X509 *)PEM_ASN1_read( \
	(char *(*)())d2i_X509,PEM_STRING_X509,fp,(char **)x,cb,u)
#define	PEM_read_X509_REQ(fp,x,cb,u) (X509_REQ *)PEM_ASN1_read( \
	(char *(*)())d2i_X509_REQ,PEM_STRING_X509_REQ,fp,(char **)x,cb,u)
#define	PEM_read_X509_CRL(fp,x,cb,u) (X509_CRL *)PEM_ASN1_read( \
	(char *(*)())d2i_X509_CRL,PEM_STRING_X509_CRL,fp,(char **)x,cb,u)
#define	PEM_read_RSAPrivateKey(fp,x,cb,u) (RSA *)PEM_ASN1_read( \
	(char *(*)())d2i_RSAPrivateKey,PEM_STRING_RSA,fp,(char **)x,cb,u)
#define	PEM_read_RSAPublicKey(fp,x,cb,u) (RSA *)PEM_ASN1_read( \
	(char *(*)())d2i_RSAPublicKey,PEM_STRING_RSA_PUBLIC,fp,(char **)x,cb,u)
#define	PEM_read_DSAPrivateKey(fp,x,cb,u) (DSA *)PEM_ASN1_read( \
	(char *(*)())d2i_DSAPrivateKey,PEM_STRING_DSA,fp,(char **)x,cb,u)
#define	PEM_read_PrivateKey(fp,x,cb,u) (EVP_PKEY *)PEM_ASN1_read( \
	(char *(*)())d2i_PrivateKey,PEM_STRING_EVP_PKEY,fp,(char **)x,cb,u)
#define	PEM_read_PKCS7(fp,x,cb,u) (PKCS7 *)PEM_ASN1_read( \
	(char *(*)())d2i_PKCS7,PEM_STRING_PKCS7,fp,(char **)x,cb,u)
#define	PEM_read_DHparams(fp,x,cb,u) (DH *)PEM_ASN1_read( \
	(char *(*)())d2i_DHparams,PEM_STRING_DHPARAMS,fp,(char **)x,cb,u)

#define PEM_read_NETSCAPE_CERT_SEQUENCE(fp,x,cb,u) \
		(NETSCAPE_CERT_SEQUENCE *)PEM_ASN1_read( \
        (char *(*)())d2i_NETSCAPE_CERT_SEQUENCE,PEM_STRING_X509,fp,\
							(char **)x,cb,u)

#define PEM_write_bio_X509(bp,x) \
		PEM_ASN1_write_bio((int (*)())i2d_X509,PEM_STRING_X509,bp, \
			(char *)x, NULL,NULL,0,NULL,NULL)
#define PEM_write_bio_X509_REQ(bp,x) PEM_ASN1_write_bio( \
		(int (*)())i2d_X509_REQ,PEM_STRING_X509_REQ,bp,(char *)x, \
			NULL,NULL,0,NULL,NULL)
#define PEM_write_bio_X509_CRL(bp,x) \
		PEM_ASN1_write_bio((int (*)())i2d_X509_CRL,PEM_STRING_X509_CRL,\
			bp,(char *)x, NULL,NULL,0,NULL,NULL)
#define	PEM_write_bio_RSAPrivateKey(bp,x,enc,kstr,klen,cb,u) \
		PEM_ASN1_write_bio((int (*)())i2d_RSAPrivateKey,PEM_STRING_RSA,\
			bp,(char *)x,enc,kstr,klen,cb,u)
#define	PEM_write_bio_RSAPublicKey(bp,x) \
		PEM_ASN1_write_bio((int (*)())i2d_RSAPublicKey, \
			PEM_STRING_RSA_PUBLIC,\
			bp,(char *)x,NULL,NULL,0,NULL,NULL)
#define	PEM_write_bio_DSAPrivateKey(bp,x,enc,kstr,klen,cb,u) \
		PEM_ASN1_write_bio((int (*)())i2d_DSAPrivateKey,PEM_STRING_DSA,\
			bp,(char *)x,enc,kstr,klen,cb,u)
#define	PEM_write_bio_PrivateKey(bp,x,enc,kstr,klen,cb,u) \
		PEM_ASN1_write_bio((int (*)())i2d_PrivateKey,\
		(((x)->type == EVP_PKEY_DSA)?PEM_STRING_DSA:PEM_STRING_RSA),\
			bp,(char *)x,enc,kstr,klen,cb,u)
#define PEM_write_bio_PKCS7(bp,x) \
		PEM_ASN1_write_bio((int (*)())i2d_PKCS7,PEM_STRING_PKCS7,bp, \
			(char *)x, NULL,NULL,0,NULL,NULL)
#define PEM_write_bio_DHparams(bp,x) \
		PEM_ASN1_write_bio((int (*)())i2d_DHparams,PEM_STRING_DHPARAMS,\
			bp,(char *)x,NULL,NULL,0,NULL,NULL)
#define PEM_write_bio_DSAparams(bp,x) \
		PEM_ASN1_write_bio((int (*)())i2d_DSAparams, \
			PEM_STRING_DSAPARAMS,bp,(char *)x,NULL,NULL,0,NULL,NULL)

#define PEM_write_bio_NETSCAPE_CERT_SEQUENCE(bp,x) \
                PEM_ASN1_write_bio((int (*)())i2d_NETSCAPE_CERT_SEQUENCE, \
			PEM_STRING_X509,bp, \
                        (char *)x, NULL,NULL,0,NULL,NULL)

#define	PEM_read_bio_X509(bp,x,cb,u) (X509 *)PEM_ASN1_read_bio( \
	(char *(*)())d2i_X509,PEM_STRING_X509,bp,(char **)x,cb,u)
#define	PEM_read_bio_X509_REQ(bp,x,cb,u) (X509_REQ *)PEM_ASN1_read_bio( \
	(char *(*)())d2i_X509_REQ,PEM_STRING_X509_REQ,bp,(char **)x,cb,u)
#define	PEM_read_bio_X509_CRL(bp,x,cb,u) (X509_CRL *)PEM_ASN1_read_bio( \
	(char *(*)())d2i_X509_CRL,PEM_STRING_X509_CRL,bp,(char **)x,cb,u)
#define	PEM_read_bio_RSAPrivateKey(bp,x,cb,u) (RSA *)PEM_ASN1_read_bio( \
	(char *(*)())d2i_RSAPrivateKey,PEM_STRING_RSA,bp,(char **)x,cb,u)
#define	PEM_read_bio_RSAPublicKey(bp,x,cb,u) (RSA *)PEM_ASN1_read_bio( \
	(char *(*)())d2i_RSAPublicKey,PEM_STRING_RSA_PUBLIC,bp,(char **)x,cb,u)
#define	PEM_read_bio_DSAPrivateKey(bp,x,cb,u) (DSA *)PEM_ASN1_read_bio( \
	(char *(*)())d2i_DSAPrivateKey,PEM_STRING_DSA,bp,(char **)x,cb,u)
#define	PEM_read_bio_PrivateKey(bp,x,cb,u) (EVP_PKEY *)PEM_ASN1_read_bio( \
	(char *(*)())d2i_PrivateKey,PEM_STRING_EVP_PKEY,bp,(char **)x,cb,u)

#define	PEM_read_bio_PKCS7(bp,x,cb,u) (PKCS7 *)PEM_ASN1_read_bio( \
	(char *(*)())d2i_PKCS7,PEM_STRING_PKCS7,bp,(char **)x,cb,u)
#define	PEM_read_bio_DHparams(bp,x,cb,u) (DH *)PEM_ASN1_read_bio( \
	(char *(*)())d2i_DHparams,PEM_STRING_DHPARAMS,bp,(char **)x,cb,u)
#define	PEM_read_bio_DSAparams(bp,x,cb,u) (DSA *)PEM_ASN1_read_bio( \
	(char *(*)())d2i_DSAparams,PEM_STRING_DSAPARAMS,bp,(char **)x,cb,u)

#define PEM_read_bio_NETSCAPE_CERT_SEQUENCE(bp,x,cb,u) \
		(NETSCAPE_CERT_SEQUENCE *)PEM_ASN1_read_bio( \
        (char *(*)())d2i_NETSCAPE_CERT_SEQUENCE,PEM_STRING_X509,bp,\
							(char **)x,cb,u)

#endif

#if 1
/* "userdata": new with OpenSSL 0.9.4 */
typedef int pem_password_cb(char *buf, int size, int rwflag, void *userdata);
#else
/* OpenSSL 0.9.3, 0.9.3a */
typedef int pem_password_cb(char *buf, int size, int rwflag);
#endif

int	PEM_get_EVP_CIPHER_INFO(char *header, EVP_CIPHER_INFO *cipher);
int	PEM_do_header (EVP_CIPHER_INFO *cipher, unsigned char *data,long *len,
	pem_password_cb *callback,void *u);

#ifndef OPENSSL_NO_BIO
int	PEM_read_bio(BIO *bp, char **name, char **header,
		unsigned char **data,long *len);
int	PEM_write_bio(BIO *bp,const char *name,char *hdr,unsigned char *data,
		long len);
int PEM_bytes_read_bio(unsigned char **pdata, long *plen, char **pnm, const char *name, BIO *bp,
	     pem_password_cb *cb, void *u);
void *	PEM_ASN1_read_bio(d2i_of_void *d2i, const char *name, BIO *bp,
			  void **x, pem_password_cb *cb, void *u);

#define PEM_ASN1_read_bio_of(type,d2i,name,bp,x,cb,u) \
    ((type*)PEM_ASN1_read_bio(CHECKED_D2I_OF(type, d2i), \
			      name, bp,			\
			      CHECKED_PPTR_OF(type, x), \
			      cb, u))

int	PEM_ASN1_write_bio(i2d_of_void *i2d,const char *name,BIO *bp,char *x,
			   const EVP_CIPHER *enc,unsigned char *kstr,int klen,
			   pem_password_cb *cb, void *u);

#define PEM_ASN1_write_bio_of(type,i2d,name,bp,x,enc,kstr,klen,cb,u) \
    (PEM_ASN1_write_bio(CHECKED_I2D_OF(type, i2d), \
			name, bp,		   \
			CHECKED_PTR_OF(type, x), \
			enc, kstr, klen, cb, u))

STACK_OF(X509_INFO) *	PEM_X509_INFO_read_bio(BIO *bp, STACK_OF(X509_INFO) *sk, pem_password_cb *cb, void *u);
int	PEM_X509_INFO_write_bio(BIO *bp,X509_INFO *xi, EVP_CIPHER *enc,
		unsigned char *kstr, int klen, pem_password_cb *cd, void *u);
#endif

#ifndef OPENSSL_SYS_WIN16
int	PEM_read(FILE *fp, char **name, char **header,
		unsigned char **data,long *len);
int	PEM_write(FILE *fp,char *name,char *hdr,unsigned char *data,long len);
void *  PEM_ASN1_read(d2i_of_void *d2i, const char *name, FILE *fp, void **x,
		      pem_password_cb *cb, void *u);
int	PEM_ASN1_write(i2d_of_void *i2d,const char *name,FILE *fp,
		       char *x,const EVP_CIPHER *enc,unsigned char *kstr,
		       int klen,pem_password_cb *callback, void *u);
STACK_OF(X509_INFO) *	PEM_X509_INFO_read(FILE *fp, STACK_OF(X509_INFO) *sk,
	pem_password_cb *cb, void *u);
#endif

int	PEM_SealInit(PEM_ENCODE_SEAL_CTX *ctx, EVP_CIPHER *type,
		EVP_MD *md_type, unsigned char **ek, int *ekl,
		unsigned char *iv, EVP_PKEY **pubk, int npubk);
void	PEM_SealUpdate(PEM_ENCODE_SEAL_CTX *ctx, unsigned char *out, int *outl,
		unsigned char *in, int inl);
int	PEM_SealFinal(PEM_ENCODE_SEAL_CTX *ctx, unsigned char *sig,int *sigl,
		unsigned char *out, int *outl, EVP_PKEY *priv);

void    PEM_SignInit(EVP_MD_CTX *ctx, EVP_MD *type);
void    PEM_SignUpdate(EVP_MD_CTX *ctx,unsigned char *d,unsigned int cnt);
int	PEM_SignFinal(EVP_MD_CTX *ctx, unsigned char *sigret,
		unsigned int *siglen, EVP_PKEY *pkey);

int	PEM_def_callback(char *buf, int num, int w, void *key);
void	PEM_proc_type(char *buf, int type);
void	PEM_dek_info(char *buf, const char *type, int len, char *str);

#ifndef SSLEAY_MACROS

#include <openssl/symhacks.h>

DECLARE_PEM_rw(X509, X509)

DECLARE_PEM_rw(X509_AUX, X509)

DECLARE_PEM_rw(X509_CERT_PAIR, X509_CERT_PAIR)

DECLARE_PEM_rw(X509_REQ, X509_REQ)
DECLARE_PEM_write(X509_REQ_NEW, X509_REQ)

DECLARE_PEM_rw(X509_CRL, X509_CRL)

DECLARE_PEM_rw(PKCS7, PKCS7)

DECLARE_PEM_rw(NETSCAPE_CERT_SEQUENCE, NETSCAPE_CERT_SEQUENCE)

DECLARE_PEM_rw(PKCS8, X509_SIG)

DECLARE_PEM_rw(PKCS8_PRIV_KEY_INFO, PKCS8_PRIV_KEY_INFO)

#ifndef OPENSSL_NO_RSA

DECLARE_PEM_rw_cb(RSAPrivateKey, RSA)

DECLARE_PEM_rw_const(RSAPublicKey, RSA)
DECLARE_PEM_rw(RSA_PUBKEY, RSA)

#endif

#ifndef OPENSSL_NO_DSA

DECLARE_PEM_rw_cb(DSAPrivateKey, DSA)

DECLARE_PEM_rw(DSA_PUBKEY, DSA)

DECLARE_PEM_rw_const(DSAparams, DSA)

#endif

#ifndef OPENSSL_NO_EC
DECLARE_PEM_rw_const(ECPKParameters, EC_GROUP)
DECLARE_PEM_rw_cb(ECPrivateKey, EC_KEY)
DECLARE_PEM_rw(EC_PUBKEY, EC_KEY)
#endif

#ifndef OPENSSL_NO_DH

DECLARE_PEM_rw_const(DHparams, DH)

#endif

DECLARE_PEM_rw_cb(PrivateKey, EVP_PKEY)

DECLARE_PEM_rw(PUBKEY, EVP_PKEY)

int PEM_write_bio_PKCS8PrivateKey_nid(BIO *bp, EVP_PKEY *x, int nid,
				  char *kstr, int klen,
				  pem_password_cb *cb, void *u);
int PEM_write_bio_PKCS8PrivateKey(BIO *, EVP_PKEY *, const EVP_CIPHER *,
                                  char *, int, pem_password_cb *, void *);
int i2d_PKCS8PrivateKey_bio(BIO *bp, EVP_PKEY *x, const EVP_CIPHER *enc,
				  char *kstr, int klen,
				  pem_password_cb *cb, void *u);
int i2d_PKCS8PrivateKey_nid_bio(BIO *bp, EVP_PKEY *x, int nid,
				  char *kstr, int klen,
				  pem_password_cb *cb, void *u);
EVP_PKEY *d2i_PKCS8PrivateKey_bio(BIO *bp, EVP_PKEY **x, pem_password_cb *cb, void *u);

int i2d_PKCS8PrivateKey_fp(FILE *fp, EVP_PKEY *x, const EVP_CIPHER *enc,
				  char *kstr, int klen,
				  pem_password_cb *cb, void *u);
int i2d_PKCS8PrivateKey_nid_fp(FILE *fp, EVP_PKEY *x, int nid,
				  char *kstr, int klen,
				  pem_password_cb *cb, void *u);
int PEM_write_PKCS8PrivateKey_nid(FILE *fp, EVP_PKEY *x, int nid,
				  char *kstr, int klen,
				  pem_password_cb *cb, void *u);

EVP_PKEY *d2i_PKCS8PrivateKey_fp(FILE *fp, EVP_PKEY **x, pem_password_cb *cb, void *u);

int PEM_write_PKCS8PrivateKey(FILE *fp,EVP_PKEY *x,const EVP_CIPHER *enc,
			      char *kstr,int klen, pem_password_cb *cd, void *u);

#endif /* SSLEAY_MACROS */


/* BEGIN ERROR CODES */
/* The following lines are auto generated by the script mkerr.pl. Any changes
 * made after this point may be overwritten when the script is next run.
 */
void ERR_load_PEM_strings(void);

/* Error codes for the PEM functions. */

/* Function codes. */
#define PEM_F_D2I_PKCS8PRIVATEKEY_BIO			 120
#define PEM_F_D2I_PKCS8PRIVATEKEY_FP			 121
#define PEM_F_DO_PK8PKEY				 126
#define PEM_F_DO_PK8PKEY_FP				 125
#define PEM_F_LOAD_IV					 101
#define PEM_F_PEM_ASN1_READ				 102
#define PEM_F_PEM_ASN1_READ_BIO				 103
#define PEM_F_PEM_ASN1_WRITE				 104
#define PEM_F_PEM_ASN1_WRITE_BIO			 105
#define PEM_F_PEM_DEF_CALLBACK				 100
#define PEM_F_PEM_DO_HEADER				 106
#define PEM_F_PEM_F_PEM_WRITE_PKCS8PRIVATEKEY		 118
#define PEM_F_PEM_GET_EVP_CIPHER_INFO			 107
#define PEM_F_PEM_PK8PKEY				 119
#define PEM_F_PEM_READ					 108
#define PEM_F_PEM_READ_BIO				 109
#define PEM_F_PEM_READ_BIO_PRIVATEKEY			 123
#define PEM_F_PEM_READ_PRIVATEKEY			 124
#define PEM_F_PEM_SEALFINAL				 110
#define PEM_F_PEM_SEALINIT				 111
#define PEM_F_PEM_SIGNFINAL				 112
#define PEM_F_PEM_WRITE					 113
#define PEM_F_PEM_WRITE_BIO				 114
#define PEM_F_PEM_X509_INFO_READ			 115
#define PEM_F_PEM_X509_INFO_READ_BIO			 116
#define PEM_F_PEM_X509_INFO_WRITE_BIO			 117

/* Reason codes. */
#define PEM_R_BAD_BASE64_DECODE				 100
#define PEM_R_BAD_DECRYPT				 101
#define PEM_R_BAD_END_LINE				 102
#define PEM_R_BAD_IV_CHARS				 103
#define PEM_R_BAD_PASSWORD_READ				 104
#define PEM_R_ERROR_CONVERTING_PRIVATE_KEY		 115
#define PEM_R_NOT_DEK_INFO				 105
#define PEM_R_NOT_ENCRYPTED				 106
#define PEM_R_NOT_PROC_TYPE				 107
#define PEM_R_NO_START_LINE				 108
#define PEM_R_PROBLEMS_GETTING_PASSWORD			 109
#define PEM_R_PUBLIC_KEY_NO_RSA				 110
#define PEM_R_READ_KEY					 111
#define PEM_R_SHORT_HEADER				 112
#define PEM_R_UNSUPPORTED_CIPHER			 113
#define PEM_R_UNSUPPORTED_ENCRYPTION			 114

#ifdef  __cplusplus
}
#endif
#endif
