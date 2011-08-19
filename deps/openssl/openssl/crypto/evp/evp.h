/* crypto/evp/evp.h */
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

#ifndef HEADER_ENVELOPE_H
#define HEADER_ENVELOPE_H

#ifdef OPENSSL_ALGORITHM_DEFINES
# include <openssl/opensslconf.h>
#else
# define OPENSSL_ALGORITHM_DEFINES
# include <openssl/opensslconf.h>
# undef OPENSSL_ALGORITHM_DEFINES
#endif

#include <openssl/ossl_typ.h>

#include <openssl/symhacks.h>

#ifndef OPENSSL_NO_BIO
#include <openssl/bio.h>
#endif

#ifdef OPENSSL_FIPS
#include <openssl/fips.h>
#endif

/*
#define EVP_RC2_KEY_SIZE		16
#define EVP_RC4_KEY_SIZE		16
#define EVP_BLOWFISH_KEY_SIZE		16
#define EVP_CAST5_KEY_SIZE		16
#define EVP_RC5_32_12_16_KEY_SIZE	16
*/
#define EVP_MAX_MD_SIZE			64	/* longest known is SHA512 */
#define EVP_MAX_KEY_LENGTH		32
#define EVP_MAX_IV_LENGTH		16
#define EVP_MAX_BLOCK_LENGTH		32

#define PKCS5_SALT_LEN			8
/* Default PKCS#5 iteration count */
#define PKCS5_DEFAULT_ITER		2048

#include <openssl/objects.h>

#define EVP_PK_RSA	0x0001
#define EVP_PK_DSA	0x0002
#define EVP_PK_DH	0x0004
#define EVP_PK_EC	0x0008
#define EVP_PKT_SIGN	0x0010
#define EVP_PKT_ENC	0x0020
#define EVP_PKT_EXCH	0x0040
#define EVP_PKS_RSA	0x0100
#define EVP_PKS_DSA	0x0200
#define EVP_PKS_EC	0x0400
#define EVP_PKT_EXP	0x1000 /* <= 512 bit key */

#define EVP_PKEY_NONE	NID_undef
#define EVP_PKEY_RSA	NID_rsaEncryption
#define EVP_PKEY_RSA2	NID_rsa
#define EVP_PKEY_DSA	NID_dsa
#define EVP_PKEY_DSA1	NID_dsa_2
#define EVP_PKEY_DSA2	NID_dsaWithSHA
#define EVP_PKEY_DSA3	NID_dsaWithSHA1
#define EVP_PKEY_DSA4	NID_dsaWithSHA1_2
#define EVP_PKEY_DH	NID_dhKeyAgreement
#define EVP_PKEY_EC	NID_X9_62_id_ecPublicKey

#ifdef	__cplusplus
extern "C" {
#endif

/* Type needs to be a bit field
 * Sub-type needs to be for variations on the method, as in, can it do
 * arbitrary encryption.... */
struct evp_pkey_st
	{
	int type;
	int save_type;
	int references;
	union	{
		char *ptr;
#ifndef OPENSSL_NO_RSA
		struct rsa_st *rsa;	/* RSA */
#endif
#ifndef OPENSSL_NO_DSA
		struct dsa_st *dsa;	/* DSA */
#endif
#ifndef OPENSSL_NO_DH
		struct dh_st *dh;	/* DH */
#endif
#ifndef OPENSSL_NO_EC
		struct ec_key_st *ec;	/* ECC */
#endif
		} pkey;
	int save_parameters;
	STACK_OF(X509_ATTRIBUTE) *attributes; /* [ 0 ] */
	} /* EVP_PKEY */;

#define EVP_PKEY_MO_SIGN	0x0001
#define EVP_PKEY_MO_VERIFY	0x0002
#define EVP_PKEY_MO_ENCRYPT	0x0004
#define EVP_PKEY_MO_DECRYPT	0x0008

#if 0
/* This structure is required to tie the message digest and signing together.
 * The lookup can be done by md/pkey_method, oid, oid/pkey_method, or
 * oid, md and pkey.
 * This is required because for various smart-card perform the digest and
 * signing/verification on-board.  To handle this case, the specific
 * EVP_MD and EVP_PKEY_METHODs need to be closely associated.
 * When a PKEY is created, it will have a EVP_PKEY_METHOD associated with it.
 * This can either be software or a token to provide the required low level
 * routines.
 */
typedef struct evp_pkey_md_st
	{
	int oid;
	EVP_MD *md;
	EVP_PKEY_METHOD *pkey;
	} EVP_PKEY_MD;

#define EVP_rsa_md2() \
		EVP_PKEY_MD_add(NID_md2WithRSAEncryption,\
			EVP_rsa_pkcs1(),EVP_md2())
#define EVP_rsa_md5() \
		EVP_PKEY_MD_add(NID_md5WithRSAEncryption,\
			EVP_rsa_pkcs1(),EVP_md5())
#define EVP_rsa_sha0() \
		EVP_PKEY_MD_add(NID_shaWithRSAEncryption,\
			EVP_rsa_pkcs1(),EVP_sha())
#define EVP_rsa_sha1() \
		EVP_PKEY_MD_add(NID_sha1WithRSAEncryption,\
			EVP_rsa_pkcs1(),EVP_sha1())
#define EVP_rsa_ripemd160() \
		EVP_PKEY_MD_add(NID_ripemd160WithRSA,\
			EVP_rsa_pkcs1(),EVP_ripemd160())
#define EVP_rsa_mdc2() \
		EVP_PKEY_MD_add(NID_mdc2WithRSA,\
			EVP_rsa_octet_string(),EVP_mdc2())
#define EVP_dsa_sha() \
		EVP_PKEY_MD_add(NID_dsaWithSHA,\
			EVP_dsa(),EVP_sha())
#define EVP_dsa_sha1() \
		EVP_PKEY_MD_add(NID_dsaWithSHA1,\
			EVP_dsa(),EVP_sha1())

typedef struct evp_pkey_method_st
	{
	char *name;
	int flags;
	int type;		/* RSA, DSA, an SSLeay specific constant */
	int oid;		/* For the pub-key type */
	int encrypt_oid;	/* pub/priv key encryption */

	int (*sign)();
	int (*verify)();
	struct	{
		int (*set)();	/* get and/or set the underlying type */
		int (*get)();
		int (*encrypt)();
		int (*decrypt)();
		int (*i2d)();
		int (*d2i)();
		int (*dup)();
		} pub,priv;
	int (*set_asn1_parameters)();
	int (*get_asn1_parameters)();
	} EVP_PKEY_METHOD;
#endif

#ifndef EVP_MD
struct env_md_st
	{
	int type;
	int pkey_type;
	int md_size;
	unsigned long flags;
	int (*init)(EVP_MD_CTX *ctx);
	int (*update)(EVP_MD_CTX *ctx,const void *data,size_t count);
	int (*final)(EVP_MD_CTX *ctx,unsigned char *md);
	int (*copy)(EVP_MD_CTX *to,const EVP_MD_CTX *from);
	int (*cleanup)(EVP_MD_CTX *ctx);

	/* FIXME: prototype these some day */
	int (*sign)(int type, const unsigned char *m, unsigned int m_length,
		    unsigned char *sigret, unsigned int *siglen, void *key);
	int (*verify)(int type, const unsigned char *m, unsigned int m_length,
		      const unsigned char *sigbuf, unsigned int siglen,
		      void *key);
	int required_pkey_type[5]; /*EVP_PKEY_xxx */
	int block_size;
	int ctx_size; /* how big does the ctx->md_data need to be */
	} /* EVP_MD */;

typedef int evp_sign_method(int type,const unsigned char *m,
			    unsigned int m_length,unsigned char *sigret,
			    unsigned int *siglen, void *key);
typedef int evp_verify_method(int type,const unsigned char *m,
			    unsigned int m_length,const unsigned char *sigbuf,
			    unsigned int siglen, void *key);

typedef struct
	{
	EVP_MD_CTX *mctx;
	void *key;
	} EVP_MD_SVCTX;

#define EVP_MD_FLAG_ONESHOT	0x0001 /* digest can only handle a single
					* block */

#define EVP_MD_FLAG_FIPS	0x0400 /* Note if suitable for use in FIPS mode */

#define EVP_MD_FLAG_SVCTX	0x0800 /* pass EVP_MD_SVCTX to sign/verify */

#define EVP_PKEY_NULL_method	NULL,NULL,{0,0,0,0}

#ifndef OPENSSL_NO_DSA
#define EVP_PKEY_DSA_method	(evp_sign_method *)DSA_sign, \
				(evp_verify_method *)DSA_verify, \
				{EVP_PKEY_DSA,EVP_PKEY_DSA2,EVP_PKEY_DSA3, \
					EVP_PKEY_DSA4,0}
#else
#define EVP_PKEY_DSA_method	EVP_PKEY_NULL_method
#endif

#ifndef OPENSSL_NO_ECDSA
#define EVP_PKEY_ECDSA_method   (evp_sign_method *)ECDSA_sign, \
				(evp_verify_method *)ECDSA_verify, \
                                 {EVP_PKEY_EC,0,0,0}
#else   
#define EVP_PKEY_ECDSA_method   EVP_PKEY_NULL_method
#endif

#ifndef OPENSSL_NO_RSA
#define EVP_PKEY_RSA_method	(evp_sign_method *)RSA_sign, \
				(evp_verify_method *)RSA_verify, \
				{EVP_PKEY_RSA,EVP_PKEY_RSA2,0,0}
#define EVP_PKEY_RSA_ASN1_OCTET_STRING_method \
				(evp_sign_method *)RSA_sign_ASN1_OCTET_STRING, \
				(evp_verify_method *)RSA_verify_ASN1_OCTET_STRING, \
				{EVP_PKEY_RSA,EVP_PKEY_RSA2,0,0}
#else
#define EVP_PKEY_RSA_method	EVP_PKEY_NULL_method
#define EVP_PKEY_RSA_ASN1_OCTET_STRING_method EVP_PKEY_NULL_method
#endif

#endif /* !EVP_MD */

struct env_md_ctx_st
	{
	const EVP_MD *digest;
	ENGINE *engine; /* functional reference if 'digest' is ENGINE-provided */
	unsigned long flags;
	void *md_data;
	} /* EVP_MD_CTX */;

/* values for EVP_MD_CTX flags */

#define EVP_MD_CTX_FLAG_ONESHOT		0x0001 /* digest update will be called
						* once only */
#define EVP_MD_CTX_FLAG_CLEANED		0x0002 /* context has already been
						* cleaned */
#define EVP_MD_CTX_FLAG_REUSE		0x0004 /* Don't free up ctx->md_data
						* in EVP_MD_CTX_cleanup */
#define EVP_MD_CTX_FLAG_NON_FIPS_ALLOW	0x0008	/* Allow use of non FIPS digest
						 * in FIPS mode */

#define EVP_MD_CTX_FLAG_PAD_MASK	0xF0	/* RSA mode to use */
#define EVP_MD_CTX_FLAG_PAD_PKCS1	0x00	/* PKCS#1 v1.5 mode */
#define EVP_MD_CTX_FLAG_PAD_X931	0x10	/* X9.31 mode */
#define EVP_MD_CTX_FLAG_PAD_PSS		0x20	/* PSS mode */
#define M_EVP_MD_CTX_FLAG_PSS_SALT(ctx) \
		((ctx->flags>>16) &0xFFFF) /* seed length */
#define EVP_MD_CTX_FLAG_PSS_MDLEN	0xFFFF	/* salt len same as digest */
#define EVP_MD_CTX_FLAG_PSS_MREC	0xFFFE	/* salt max or auto recovered */

struct evp_cipher_st
	{
	int nid;
	int block_size;
	int key_len;		/* Default value for variable length ciphers */
	int iv_len;
	unsigned long flags;	/* Various flags */
	int (*init)(EVP_CIPHER_CTX *ctx, const unsigned char *key,
		    const unsigned char *iv, int enc);	/* init key */
	int (*do_cipher)(EVP_CIPHER_CTX *ctx, unsigned char *out,
			 const unsigned char *in, unsigned int inl);/* encrypt/decrypt data */
	int (*cleanup)(EVP_CIPHER_CTX *); /* cleanup ctx */
	int ctx_size;		/* how big ctx->cipher_data needs to be */
	int (*set_asn1_parameters)(EVP_CIPHER_CTX *, ASN1_TYPE *); /* Populate a ASN1_TYPE with parameters */
	int (*get_asn1_parameters)(EVP_CIPHER_CTX *, ASN1_TYPE *); /* Get parameters from a ASN1_TYPE */
	int (*ctrl)(EVP_CIPHER_CTX *, int type, int arg, void *ptr); /* Miscellaneous operations */
	void *app_data;		/* Application data */
	} /* EVP_CIPHER */;

/* Values for cipher flags */

/* Modes for ciphers */

#define		EVP_CIPH_STREAM_CIPHER		0x0
#define		EVP_CIPH_ECB_MODE		0x1
#define		EVP_CIPH_CBC_MODE		0x2
#define		EVP_CIPH_CFB_MODE		0x3
#define		EVP_CIPH_OFB_MODE		0x4
#define 	EVP_CIPH_MODE			0x7
/* Set if variable length cipher */
#define 	EVP_CIPH_VARIABLE_LENGTH	0x8
/* Set if the iv handling should be done by the cipher itself */
#define 	EVP_CIPH_CUSTOM_IV		0x10
/* Set if the cipher's init() function should be called if key is NULL */
#define 	EVP_CIPH_ALWAYS_CALL_INIT	0x20
/* Call ctrl() to init cipher parameters */
#define 	EVP_CIPH_CTRL_INIT		0x40
/* Don't use standard key length function */
#define 	EVP_CIPH_CUSTOM_KEY_LENGTH	0x80
/* Don't use standard block padding */
#define 	EVP_CIPH_NO_PADDING		0x100
/* cipher handles random key generation */
#define 	EVP_CIPH_RAND_KEY		0x200
/* Note if suitable for use in FIPS mode */
#define		EVP_CIPH_FLAG_FIPS		0x400
/* Allow non FIPS cipher in FIPS mode */
#define		EVP_CIPH_FLAG_NON_FIPS_ALLOW	0x800
/* Allow use default ASN1 get/set iv */
#define		EVP_CIPH_FLAG_DEFAULT_ASN1	0x1000
/* Buffer length in bits not bytes: CFB1 mode only */
#define		EVP_CIPH_FLAG_LENGTH_BITS	0x2000

/* ctrl() values */

#define		EVP_CTRL_INIT			0x0
#define 	EVP_CTRL_SET_KEY_LENGTH		0x1
#define 	EVP_CTRL_GET_RC2_KEY_BITS	0x2
#define 	EVP_CTRL_SET_RC2_KEY_BITS	0x3
#define 	EVP_CTRL_GET_RC5_ROUNDS		0x4
#define 	EVP_CTRL_SET_RC5_ROUNDS		0x5
#define 	EVP_CTRL_RAND_KEY		0x6

typedef struct evp_cipher_info_st
	{
	const EVP_CIPHER *cipher;
	unsigned char iv[EVP_MAX_IV_LENGTH];
	} EVP_CIPHER_INFO;

struct evp_cipher_ctx_st
	{
	const EVP_CIPHER *cipher;
	ENGINE *engine;	/* functional reference if 'cipher' is ENGINE-provided */
	int encrypt;		/* encrypt or decrypt */
	int buf_len;		/* number we have left */

	unsigned char  oiv[EVP_MAX_IV_LENGTH];	/* original iv */
	unsigned char  iv[EVP_MAX_IV_LENGTH];	/* working iv */
	unsigned char buf[EVP_MAX_BLOCK_LENGTH];/* saved partial block */
	int num;				/* used by cfb/ofb mode */

	void *app_data;		/* application stuff */
	int key_len;		/* May change for variable length cipher */
	unsigned long flags;	/* Various flags */
	void *cipher_data; /* per EVP data */
	int final_used;
	int block_mask;
	unsigned char final[EVP_MAX_BLOCK_LENGTH];/* possible final block */
	} /* EVP_CIPHER_CTX */;

typedef struct evp_Encode_Ctx_st
	{
	int num;	/* number saved in a partial encode/decode */
	int length;	/* The length is either the output line length
			 * (in input bytes) or the shortest input line
			 * length that is ok.  Once decoding begins,
			 * the length is adjusted up each time a longer
			 * line is decoded */
	unsigned char enc_data[80];	/* data to encode */
	int line_num;	/* number read on current line */
	int expect_nl;
	} EVP_ENCODE_CTX;

/* Password based encryption function */
typedef int (EVP_PBE_KEYGEN)(EVP_CIPHER_CTX *ctx, const char *pass, int passlen,
		ASN1_TYPE *param, const EVP_CIPHER *cipher,
                const EVP_MD *md, int en_de);

#ifndef OPENSSL_NO_RSA
#define EVP_PKEY_assign_RSA(pkey,rsa) EVP_PKEY_assign((pkey),EVP_PKEY_RSA,\
					(char *)(rsa))
#endif

#ifndef OPENSSL_NO_DSA
#define EVP_PKEY_assign_DSA(pkey,dsa) EVP_PKEY_assign((pkey),EVP_PKEY_DSA,\
					(char *)(dsa))
#endif

#ifndef OPENSSL_NO_DH
#define EVP_PKEY_assign_DH(pkey,dh) EVP_PKEY_assign((pkey),EVP_PKEY_DH,\
					(char *)(dh))
#endif

#ifndef OPENSSL_NO_EC
#define EVP_PKEY_assign_EC_KEY(pkey,eckey) EVP_PKEY_assign((pkey),EVP_PKEY_EC,\
                                        (char *)(eckey))
#endif

/* Add some extra combinations */
#define EVP_get_digestbynid(a) EVP_get_digestbyname(OBJ_nid2sn(a))
#define EVP_get_digestbyobj(a) EVP_get_digestbynid(OBJ_obj2nid(a))
#define EVP_get_cipherbynid(a) EVP_get_cipherbyname(OBJ_nid2sn(a))
#define EVP_get_cipherbyobj(a) EVP_get_cipherbynid(OBJ_obj2nid(a))

/* Macros to reduce FIPS dependencies: do NOT use in applications */
#define M_EVP_MD_size(e)		((e)->md_size)
#define M_EVP_MD_block_size(e)		((e)->block_size)
#define M_EVP_MD_CTX_set_flags(ctx,flgs) ((ctx)->flags|=(flgs))
#define M_EVP_MD_CTX_clear_flags(ctx,flgs) ((ctx)->flags&=~(flgs))
#define M_EVP_MD_CTX_test_flags(ctx,flgs) ((ctx)->flags&(flgs))
#define M_EVP_MD_type(e)			((e)->type)
#define M_EVP_MD_CTX_type(e)		M_EVP_MD_type(M_EVP_MD_CTX_md(e))
#define M_EVP_MD_CTX_md(e)			((e)->digest)

#define M_EVP_CIPHER_CTX_set_flags(ctx,flgs) ((ctx)->flags|=(flgs))

int EVP_MD_type(const EVP_MD *md);
#define EVP_MD_nid(e)			EVP_MD_type(e)
#define EVP_MD_name(e)			OBJ_nid2sn(EVP_MD_nid(e))
int EVP_MD_pkey_type(const EVP_MD *md);	
int EVP_MD_size(const EVP_MD *md);
int EVP_MD_block_size(const EVP_MD *md);

const EVP_MD * EVP_MD_CTX_md(const EVP_MD_CTX *ctx);
#define EVP_MD_CTX_size(e)		EVP_MD_size(EVP_MD_CTX_md(e))
#define EVP_MD_CTX_block_size(e)	EVP_MD_block_size(EVP_MD_CTX_md(e))
#define EVP_MD_CTX_type(e)		EVP_MD_type(EVP_MD_CTX_md(e))

int EVP_CIPHER_nid(const EVP_CIPHER *cipher);
#define EVP_CIPHER_name(e)		OBJ_nid2sn(EVP_CIPHER_nid(e))
int EVP_CIPHER_block_size(const EVP_CIPHER *cipher);
int EVP_CIPHER_key_length(const EVP_CIPHER *cipher);
int EVP_CIPHER_iv_length(const EVP_CIPHER *cipher);
unsigned long EVP_CIPHER_flags(const EVP_CIPHER *cipher);
#define EVP_CIPHER_mode(e)		(EVP_CIPHER_flags(e) & EVP_CIPH_MODE)

const EVP_CIPHER * EVP_CIPHER_CTX_cipher(const EVP_CIPHER_CTX *ctx);
int EVP_CIPHER_CTX_nid(const EVP_CIPHER_CTX *ctx);
int EVP_CIPHER_CTX_block_size(const EVP_CIPHER_CTX *ctx);
int EVP_CIPHER_CTX_key_length(const EVP_CIPHER_CTX *ctx);
int EVP_CIPHER_CTX_iv_length(const EVP_CIPHER_CTX *ctx);
void * EVP_CIPHER_CTX_get_app_data(const EVP_CIPHER_CTX *ctx);
void EVP_CIPHER_CTX_set_app_data(EVP_CIPHER_CTX *ctx, void *data);
#define EVP_CIPHER_CTX_type(c)         EVP_CIPHER_type(EVP_CIPHER_CTX_cipher(c))
unsigned long EVP_CIPHER_CTX_flags(const EVP_CIPHER_CTX *ctx);
#define EVP_CIPHER_CTX_mode(e)		(EVP_CIPHER_CTX_flags(e) & EVP_CIPH_MODE)

#define EVP_ENCODE_LENGTH(l)	(((l+2)/3*4)+(l/48+1)*2+80)
#define EVP_DECODE_LENGTH(l)	((l+3)/4*3+80)

#define EVP_SignInit_ex(a,b,c)		EVP_DigestInit_ex(a,b,c)
#define EVP_SignInit(a,b)		EVP_DigestInit(a,b)
#define EVP_SignUpdate(a,b,c)		EVP_DigestUpdate(a,b,c)
#define	EVP_VerifyInit_ex(a,b,c)	EVP_DigestInit_ex(a,b,c)
#define	EVP_VerifyInit(a,b)		EVP_DigestInit(a,b)
#define	EVP_VerifyUpdate(a,b,c)		EVP_DigestUpdate(a,b,c)
#define EVP_OpenUpdate(a,b,c,d,e)	EVP_DecryptUpdate(a,b,c,d,e)
#define EVP_SealUpdate(a,b,c,d,e)	EVP_EncryptUpdate(a,b,c,d,e)	

#ifdef CONST_STRICT
void BIO_set_md(BIO *,const EVP_MD *md);
#else
# define BIO_set_md(b,md)		BIO_ctrl(b,BIO_C_SET_MD,0,(char *)md)
#endif
#define BIO_get_md(b,mdp)		BIO_ctrl(b,BIO_C_GET_MD,0,(char *)mdp)
#define BIO_get_md_ctx(b,mdcp)     BIO_ctrl(b,BIO_C_GET_MD_CTX,0,(char *)mdcp)
#define BIO_set_md_ctx(b,mdcp)     BIO_ctrl(b,BIO_C_SET_MD_CTX,0,(char *)mdcp)
#define BIO_get_cipher_status(b)	BIO_ctrl(b,BIO_C_GET_CIPHER_STATUS,0,NULL)
#define BIO_get_cipher_ctx(b,c_pp)	BIO_ctrl(b,BIO_C_GET_CIPHER_CTX,0,(char *)c_pp)

int EVP_Cipher(EVP_CIPHER_CTX *c,
		unsigned char *out,
		const unsigned char *in,
		unsigned int inl);

#define EVP_add_cipher_alias(n,alias) \
	OBJ_NAME_add((alias),OBJ_NAME_TYPE_CIPHER_METH|OBJ_NAME_ALIAS,(n))
#define EVP_add_digest_alias(n,alias) \
	OBJ_NAME_add((alias),OBJ_NAME_TYPE_MD_METH|OBJ_NAME_ALIAS,(n))
#define EVP_delete_cipher_alias(alias) \
	OBJ_NAME_remove(alias,OBJ_NAME_TYPE_CIPHER_METH|OBJ_NAME_ALIAS);
#define EVP_delete_digest_alias(alias) \
	OBJ_NAME_remove(alias,OBJ_NAME_TYPE_MD_METH|OBJ_NAME_ALIAS);

void	EVP_MD_CTX_init(EVP_MD_CTX *ctx);
int	EVP_MD_CTX_cleanup(EVP_MD_CTX *ctx);
EVP_MD_CTX *EVP_MD_CTX_create(void);
void	EVP_MD_CTX_destroy(EVP_MD_CTX *ctx);
int     EVP_MD_CTX_copy_ex(EVP_MD_CTX *out,const EVP_MD_CTX *in);  
void	EVP_MD_CTX_set_flags(EVP_MD_CTX *ctx, int flags);
void	EVP_MD_CTX_clear_flags(EVP_MD_CTX *ctx, int flags);
int 	EVP_MD_CTX_test_flags(const EVP_MD_CTX *ctx,int flags);
int	EVP_DigestInit_ex(EVP_MD_CTX *ctx, const EVP_MD *type, ENGINE *impl);
int	EVP_DigestUpdate(EVP_MD_CTX *ctx,const void *d,
			 size_t cnt);
int	EVP_DigestFinal_ex(EVP_MD_CTX *ctx,unsigned char *md,unsigned int *s);
int	EVP_Digest(const void *data, size_t count,
		unsigned char *md, unsigned int *size, const EVP_MD *type, ENGINE *impl);

int     EVP_MD_CTX_copy(EVP_MD_CTX *out,const EVP_MD_CTX *in);  
int	EVP_DigestInit(EVP_MD_CTX *ctx, const EVP_MD *type);
int	EVP_DigestFinal(EVP_MD_CTX *ctx,unsigned char *md,unsigned int *s);

int	EVP_read_pw_string(char *buf,int length,const char *prompt,int verify);
void	EVP_set_pw_prompt(const char *prompt);
char *	EVP_get_pw_prompt(void);

int	EVP_BytesToKey(const EVP_CIPHER *type,const EVP_MD *md,
		const unsigned char *salt, const unsigned char *data,
		int datal, int count, unsigned char *key,unsigned char *iv);

void	EVP_CIPHER_CTX_set_flags(EVP_CIPHER_CTX *ctx, int flags);
void	EVP_CIPHER_CTX_clear_flags(EVP_CIPHER_CTX *ctx, int flags);
int 	EVP_CIPHER_CTX_test_flags(const EVP_CIPHER_CTX *ctx,int flags);

int	EVP_EncryptInit(EVP_CIPHER_CTX *ctx,const EVP_CIPHER *cipher,
		const unsigned char *key, const unsigned char *iv);
int	EVP_EncryptInit_ex(EVP_CIPHER_CTX *ctx,const EVP_CIPHER *cipher, ENGINE *impl,
		const unsigned char *key, const unsigned char *iv);
int	EVP_EncryptUpdate(EVP_CIPHER_CTX *ctx, unsigned char *out,
		int *outl, const unsigned char *in, int inl);
int	EVP_EncryptFinal_ex(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl);
int	EVP_EncryptFinal(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl);

int	EVP_DecryptInit(EVP_CIPHER_CTX *ctx,const EVP_CIPHER *cipher,
		const unsigned char *key, const unsigned char *iv);
int	EVP_DecryptInit_ex(EVP_CIPHER_CTX *ctx,const EVP_CIPHER *cipher, ENGINE *impl,
		const unsigned char *key, const unsigned char *iv);
int	EVP_DecryptUpdate(EVP_CIPHER_CTX *ctx, unsigned char *out,
		int *outl, const unsigned char *in, int inl);
int	EVP_DecryptFinal(EVP_CIPHER_CTX *ctx, unsigned char *outm, int *outl);
int	EVP_DecryptFinal_ex(EVP_CIPHER_CTX *ctx, unsigned char *outm, int *outl);

int	EVP_CipherInit(EVP_CIPHER_CTX *ctx,const EVP_CIPHER *cipher,
		       const unsigned char *key,const unsigned char *iv,
		       int enc);
int	EVP_CipherInit_ex(EVP_CIPHER_CTX *ctx,const EVP_CIPHER *cipher, ENGINE *impl,
		       const unsigned char *key,const unsigned char *iv,
		       int enc);
int	EVP_CipherUpdate(EVP_CIPHER_CTX *ctx, unsigned char *out,
		int *outl, const unsigned char *in, int inl);
int	EVP_CipherFinal(EVP_CIPHER_CTX *ctx, unsigned char *outm, int *outl);
int	EVP_CipherFinal_ex(EVP_CIPHER_CTX *ctx, unsigned char *outm, int *outl);

int	EVP_SignFinal(EVP_MD_CTX *ctx,unsigned char *md,unsigned int *s,
		EVP_PKEY *pkey);

int	EVP_VerifyFinal(EVP_MD_CTX *ctx,const unsigned char *sigbuf,
		unsigned int siglen,EVP_PKEY *pkey);

int	EVP_OpenInit(EVP_CIPHER_CTX *ctx,const EVP_CIPHER *type,
		const unsigned char *ek, int ekl, const unsigned char *iv,
		EVP_PKEY *priv);
int	EVP_OpenFinal(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl);

int	EVP_SealInit(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *type,
		 unsigned char **ek, int *ekl, unsigned char *iv,
		EVP_PKEY **pubk, int npubk);
int	EVP_SealFinal(EVP_CIPHER_CTX *ctx,unsigned char *out,int *outl);

void	EVP_EncodeInit(EVP_ENCODE_CTX *ctx);
void	EVP_EncodeUpdate(EVP_ENCODE_CTX *ctx,unsigned char *out,int *outl,
		const unsigned char *in,int inl);
void	EVP_EncodeFinal(EVP_ENCODE_CTX *ctx,unsigned char *out,int *outl);
int	EVP_EncodeBlock(unsigned char *t, const unsigned char *f, int n);

void	EVP_DecodeInit(EVP_ENCODE_CTX *ctx);
int	EVP_DecodeUpdate(EVP_ENCODE_CTX *ctx,unsigned char *out,int *outl,
		const unsigned char *in, int inl);
int	EVP_DecodeFinal(EVP_ENCODE_CTX *ctx, unsigned
		char *out, int *outl);
int	EVP_DecodeBlock(unsigned char *t, const unsigned char *f, int n);

void EVP_CIPHER_CTX_init(EVP_CIPHER_CTX *a);
int EVP_CIPHER_CTX_cleanup(EVP_CIPHER_CTX *a);
EVP_CIPHER_CTX *EVP_CIPHER_CTX_new(void);
void EVP_CIPHER_CTX_free(EVP_CIPHER_CTX *a);
int EVP_CIPHER_CTX_set_key_length(EVP_CIPHER_CTX *x, int keylen);
int EVP_CIPHER_CTX_set_padding(EVP_CIPHER_CTX *c, int pad);
int EVP_CIPHER_CTX_ctrl(EVP_CIPHER_CTX *ctx, int type, int arg, void *ptr);
int EVP_CIPHER_CTX_rand_key(EVP_CIPHER_CTX *ctx, unsigned char *key);

#ifndef OPENSSL_NO_BIO
BIO_METHOD *BIO_f_md(void);
BIO_METHOD *BIO_f_base64(void);
BIO_METHOD *BIO_f_cipher(void);
BIO_METHOD *BIO_f_reliable(void);
void BIO_set_cipher(BIO *b,const EVP_CIPHER *c,const unsigned char *k,
		const unsigned char *i, int enc);
#endif

const EVP_MD *EVP_md_null(void);
#ifndef OPENSSL_NO_MD2
const EVP_MD *EVP_md2(void);
#endif
#ifndef OPENSSL_NO_MD4
const EVP_MD *EVP_md4(void);
#endif
#ifndef OPENSSL_NO_MD5
const EVP_MD *EVP_md5(void);
#endif
#ifndef OPENSSL_NO_SHA
const EVP_MD *EVP_sha(void);
const EVP_MD *EVP_sha1(void);
const EVP_MD *EVP_dss(void);
const EVP_MD *EVP_dss1(void);
const EVP_MD *EVP_ecdsa(void);
#endif
#ifndef OPENSSL_NO_SHA256
const EVP_MD *EVP_sha224(void);
const EVP_MD *EVP_sha256(void);
#endif
#ifndef OPENSSL_NO_SHA512
const EVP_MD *EVP_sha384(void);
const EVP_MD *EVP_sha512(void);
#endif
#ifndef OPENSSL_NO_MDC2
const EVP_MD *EVP_mdc2(void);
#endif
#ifndef OPENSSL_NO_RIPEMD
const EVP_MD *EVP_ripemd160(void);
#endif
const EVP_CIPHER *EVP_enc_null(void);		/* does nothing :-) */
#ifndef OPENSSL_NO_DES
const EVP_CIPHER *EVP_des_ecb(void);
const EVP_CIPHER *EVP_des_ede(void);
const EVP_CIPHER *EVP_des_ede3(void);
const EVP_CIPHER *EVP_des_ede_ecb(void);
const EVP_CIPHER *EVP_des_ede3_ecb(void);
const EVP_CIPHER *EVP_des_cfb64(void);
# define EVP_des_cfb EVP_des_cfb64
const EVP_CIPHER *EVP_des_cfb1(void);
const EVP_CIPHER *EVP_des_cfb8(void);
const EVP_CIPHER *EVP_des_ede_cfb64(void);
# define EVP_des_ede_cfb EVP_des_ede_cfb64
#if 0
const EVP_CIPHER *EVP_des_ede_cfb1(void);
const EVP_CIPHER *EVP_des_ede_cfb8(void);
#endif
const EVP_CIPHER *EVP_des_ede3_cfb64(void);
# define EVP_des_ede3_cfb EVP_des_ede3_cfb64
const EVP_CIPHER *EVP_des_ede3_cfb1(void);
const EVP_CIPHER *EVP_des_ede3_cfb8(void);
const EVP_CIPHER *EVP_des_ofb(void);
const EVP_CIPHER *EVP_des_ede_ofb(void);
const EVP_CIPHER *EVP_des_ede3_ofb(void);
const EVP_CIPHER *EVP_des_cbc(void);
const EVP_CIPHER *EVP_des_ede_cbc(void);
const EVP_CIPHER *EVP_des_ede3_cbc(void);
const EVP_CIPHER *EVP_desx_cbc(void);
/* This should now be supported through the dev_crypto ENGINE. But also, why are
 * rc4 and md5 declarations made here inside a "NO_DES" precompiler branch? */
#if 0
# ifdef OPENSSL_OPENBSD_DEV_CRYPTO
const EVP_CIPHER *EVP_dev_crypto_des_ede3_cbc(void);
const EVP_CIPHER *EVP_dev_crypto_rc4(void);
const EVP_MD *EVP_dev_crypto_md5(void);
# endif
#endif
#endif
#ifndef OPENSSL_NO_RC4
const EVP_CIPHER *EVP_rc4(void);
const EVP_CIPHER *EVP_rc4_40(void);
#endif
#ifndef OPENSSL_NO_IDEA
const EVP_CIPHER *EVP_idea_ecb(void);
const EVP_CIPHER *EVP_idea_cfb64(void);
# define EVP_idea_cfb EVP_idea_cfb64
const EVP_CIPHER *EVP_idea_ofb(void);
const EVP_CIPHER *EVP_idea_cbc(void);
#endif
#ifndef OPENSSL_NO_RC2
const EVP_CIPHER *EVP_rc2_ecb(void);
const EVP_CIPHER *EVP_rc2_cbc(void);
const EVP_CIPHER *EVP_rc2_40_cbc(void);
const EVP_CIPHER *EVP_rc2_64_cbc(void);
const EVP_CIPHER *EVP_rc2_cfb64(void);
# define EVP_rc2_cfb EVP_rc2_cfb64
const EVP_CIPHER *EVP_rc2_ofb(void);
#endif
#ifndef OPENSSL_NO_BF
const EVP_CIPHER *EVP_bf_ecb(void);
const EVP_CIPHER *EVP_bf_cbc(void);
const EVP_CIPHER *EVP_bf_cfb64(void);
# define EVP_bf_cfb EVP_bf_cfb64
const EVP_CIPHER *EVP_bf_ofb(void);
#endif
#ifndef OPENSSL_NO_CAST
const EVP_CIPHER *EVP_cast5_ecb(void);
const EVP_CIPHER *EVP_cast5_cbc(void);
const EVP_CIPHER *EVP_cast5_cfb64(void);
# define EVP_cast5_cfb EVP_cast5_cfb64
const EVP_CIPHER *EVP_cast5_ofb(void);
#endif
#ifndef OPENSSL_NO_RC5
const EVP_CIPHER *EVP_rc5_32_12_16_cbc(void);
const EVP_CIPHER *EVP_rc5_32_12_16_ecb(void);
const EVP_CIPHER *EVP_rc5_32_12_16_cfb64(void);
# define EVP_rc5_32_12_16_cfb EVP_rc5_32_12_16_cfb64
const EVP_CIPHER *EVP_rc5_32_12_16_ofb(void);
#endif
#ifndef OPENSSL_NO_AES
const EVP_CIPHER *EVP_aes_128_ecb(void);
const EVP_CIPHER *EVP_aes_128_cbc(void);
const EVP_CIPHER *EVP_aes_128_cfb1(void);
const EVP_CIPHER *EVP_aes_128_cfb8(void);
const EVP_CIPHER *EVP_aes_128_cfb128(void);
# define EVP_aes_128_cfb EVP_aes_128_cfb128
const EVP_CIPHER *EVP_aes_128_ofb(void);
#if 0
const EVP_CIPHER *EVP_aes_128_ctr(void);
#endif
const EVP_CIPHER *EVP_aes_192_ecb(void);
const EVP_CIPHER *EVP_aes_192_cbc(void);
const EVP_CIPHER *EVP_aes_192_cfb1(void);
const EVP_CIPHER *EVP_aes_192_cfb8(void);
const EVP_CIPHER *EVP_aes_192_cfb128(void);
# define EVP_aes_192_cfb EVP_aes_192_cfb128
const EVP_CIPHER *EVP_aes_192_ofb(void);
#if 0
const EVP_CIPHER *EVP_aes_192_ctr(void);
#endif
const EVP_CIPHER *EVP_aes_256_ecb(void);
const EVP_CIPHER *EVP_aes_256_cbc(void);
const EVP_CIPHER *EVP_aes_256_cfb1(void);
const EVP_CIPHER *EVP_aes_256_cfb8(void);
const EVP_CIPHER *EVP_aes_256_cfb128(void);
# define EVP_aes_256_cfb EVP_aes_256_cfb128
const EVP_CIPHER *EVP_aes_256_ofb(void);
#if 0
const EVP_CIPHER *EVP_aes_256_ctr(void);
#endif
#endif
#ifndef OPENSSL_NO_CAMELLIA
const EVP_CIPHER *EVP_camellia_128_ecb(void);
const EVP_CIPHER *EVP_camellia_128_cbc(void);
const EVP_CIPHER *EVP_camellia_128_cfb1(void);
const EVP_CIPHER *EVP_camellia_128_cfb8(void);
const EVP_CIPHER *EVP_camellia_128_cfb128(void);
# define EVP_camellia_128_cfb EVP_camellia_128_cfb128
const EVP_CIPHER *EVP_camellia_128_ofb(void);
const EVP_CIPHER *EVP_camellia_192_ecb(void);
const EVP_CIPHER *EVP_camellia_192_cbc(void);
const EVP_CIPHER *EVP_camellia_192_cfb1(void);
const EVP_CIPHER *EVP_camellia_192_cfb8(void);
const EVP_CIPHER *EVP_camellia_192_cfb128(void);
# define EVP_camellia_192_cfb EVP_camellia_192_cfb128
const EVP_CIPHER *EVP_camellia_192_ofb(void);
const EVP_CIPHER *EVP_camellia_256_ecb(void);
const EVP_CIPHER *EVP_camellia_256_cbc(void);
const EVP_CIPHER *EVP_camellia_256_cfb1(void);
const EVP_CIPHER *EVP_camellia_256_cfb8(void);
const EVP_CIPHER *EVP_camellia_256_cfb128(void);
# define EVP_camellia_256_cfb EVP_camellia_256_cfb128
const EVP_CIPHER *EVP_camellia_256_ofb(void);
#endif

#ifndef OPENSSL_NO_SEED
const EVP_CIPHER *EVP_seed_ecb(void);
const EVP_CIPHER *EVP_seed_cbc(void);
const EVP_CIPHER *EVP_seed_cfb128(void);
# define EVP_seed_cfb EVP_seed_cfb128
const EVP_CIPHER *EVP_seed_ofb(void);
#endif

void OPENSSL_add_all_algorithms_noconf(void);
void OPENSSL_add_all_algorithms_conf(void);

#ifdef OPENSSL_LOAD_CONF
#define OpenSSL_add_all_algorithms() \
		OPENSSL_add_all_algorithms_conf()
#else
#define OpenSSL_add_all_algorithms() \
		OPENSSL_add_all_algorithms_noconf()
#endif

void OpenSSL_add_all_ciphers(void);
void OpenSSL_add_all_digests(void);
#define SSLeay_add_all_algorithms() OpenSSL_add_all_algorithms()
#define SSLeay_add_all_ciphers() OpenSSL_add_all_ciphers()
#define SSLeay_add_all_digests() OpenSSL_add_all_digests()

int EVP_add_cipher(const EVP_CIPHER *cipher);
int EVP_add_digest(const EVP_MD *digest);

const EVP_CIPHER *EVP_get_cipherbyname(const char *name);
const EVP_MD *EVP_get_digestbyname(const char *name);
void EVP_cleanup(void);

int		EVP_PKEY_decrypt(unsigned char *dec_key,
			const unsigned char *enc_key,int enc_key_len,
			EVP_PKEY *private_key);
int		EVP_PKEY_encrypt(unsigned char *enc_key,
			const unsigned char *key,int key_len,
			EVP_PKEY *pub_key);
int		EVP_PKEY_type(int type);
int		EVP_PKEY_bits(EVP_PKEY *pkey);
int		EVP_PKEY_size(EVP_PKEY *pkey);
int 		EVP_PKEY_assign(EVP_PKEY *pkey,int type,char *key);

#ifndef OPENSSL_NO_RSA
struct rsa_st;
int EVP_PKEY_set1_RSA(EVP_PKEY *pkey,struct rsa_st *key);
struct rsa_st *EVP_PKEY_get1_RSA(EVP_PKEY *pkey);
#endif
#ifndef OPENSSL_NO_DSA
struct dsa_st;
int EVP_PKEY_set1_DSA(EVP_PKEY *pkey,struct dsa_st *key);
struct dsa_st *EVP_PKEY_get1_DSA(EVP_PKEY *pkey);
#endif
#ifndef OPENSSL_NO_DH
struct dh_st;
int EVP_PKEY_set1_DH(EVP_PKEY *pkey,struct dh_st *key);
struct dh_st *EVP_PKEY_get1_DH(EVP_PKEY *pkey);
#endif
#ifndef OPENSSL_NO_EC
struct ec_key_st;
int EVP_PKEY_set1_EC_KEY(EVP_PKEY *pkey,struct ec_key_st *key);
struct ec_key_st *EVP_PKEY_get1_EC_KEY(EVP_PKEY *pkey);
#endif

EVP_PKEY *	EVP_PKEY_new(void);
void		EVP_PKEY_free(EVP_PKEY *pkey);

EVP_PKEY *	d2i_PublicKey(int type,EVP_PKEY **a, const unsigned char **pp,
			long length);
int		i2d_PublicKey(EVP_PKEY *a, unsigned char **pp);

EVP_PKEY *	d2i_PrivateKey(int type,EVP_PKEY **a, const unsigned char **pp,
			long length);
EVP_PKEY *	d2i_AutoPrivateKey(EVP_PKEY **a, const unsigned char **pp,
			long length);
int		i2d_PrivateKey(EVP_PKEY *a, unsigned char **pp);

int EVP_PKEY_copy_parameters(EVP_PKEY *to, const EVP_PKEY *from);
int EVP_PKEY_missing_parameters(const EVP_PKEY *pkey);
int EVP_PKEY_save_parameters(EVP_PKEY *pkey,int mode);
int EVP_PKEY_cmp_parameters(const EVP_PKEY *a, const EVP_PKEY *b);

int EVP_PKEY_cmp(const EVP_PKEY *a, const EVP_PKEY *b);

int EVP_CIPHER_type(const EVP_CIPHER *ctx);

/* calls methods */
int EVP_CIPHER_param_to_asn1(EVP_CIPHER_CTX *c, ASN1_TYPE *type);
int EVP_CIPHER_asn1_to_param(EVP_CIPHER_CTX *c, ASN1_TYPE *type);

/* These are used by EVP_CIPHER methods */
int EVP_CIPHER_set_asn1_iv(EVP_CIPHER_CTX *c,ASN1_TYPE *type);
int EVP_CIPHER_get_asn1_iv(EVP_CIPHER_CTX *c,ASN1_TYPE *type);

/* PKCS5 password based encryption */
int PKCS5_PBE_keyivgen(EVP_CIPHER_CTX *ctx, const char *pass, int passlen,
			 ASN1_TYPE *param, const EVP_CIPHER *cipher, const EVP_MD *md,
			 int en_de);
int PKCS5_PBKDF2_HMAC_SHA1(const char *pass, int passlen,
			   const unsigned char *salt, int saltlen, int iter,
			   int keylen, unsigned char *out);
int PKCS5_v2_PBE_keyivgen(EVP_CIPHER_CTX *ctx, const char *pass, int passlen,
			 ASN1_TYPE *param, const EVP_CIPHER *cipher, const EVP_MD *md,
			 int en_de);

void PKCS5_PBE_add(void);

int EVP_PBE_CipherInit (ASN1_OBJECT *pbe_obj, const char *pass, int passlen,
	     ASN1_TYPE *param, EVP_CIPHER_CTX *ctx, int en_de);
int EVP_PBE_alg_add(int nid, const EVP_CIPHER *cipher, const EVP_MD *md,
		    EVP_PBE_KEYGEN *keygen);
void EVP_PBE_cleanup(void);

#ifdef OPENSSL_FIPS
#ifndef OPENSSL_NO_ENGINE
void int_EVP_MD_set_engine_callbacks(
	int (*eng_md_init)(ENGINE *impl),
	int (*eng_md_fin)(ENGINE *impl),
	int (*eng_md_evp)
		(EVP_MD_CTX *ctx, const EVP_MD **ptype, ENGINE *impl));
void int_EVP_MD_init_engine_callbacks(void);
void int_EVP_CIPHER_set_engine_callbacks(
	int (*eng_ciph_fin)(ENGINE *impl),
	int (*eng_ciph_evp)
		(EVP_CIPHER_CTX *ctx, const EVP_CIPHER **pciph, ENGINE *impl));
void int_EVP_CIPHER_init_engine_callbacks(void);
#endif
#endif

void EVP_add_alg_module(void);

/* BEGIN ERROR CODES */
/* The following lines are auto generated by the script mkerr.pl. Any changes
 * made after this point may be overwritten when the script is next run.
 */
void ERR_load_EVP_strings(void);

/* Error codes for the EVP functions. */

/* Function codes. */
#define EVP_F_AES_INIT_KEY				 133
#define EVP_F_ALG_MODULE_INIT				 138
#define EVP_F_CAMELLIA_INIT_KEY				 159
#define EVP_F_D2I_PKEY					 100
#define EVP_F_DO_EVP_ENC_ENGINE				 140
#define EVP_F_DO_EVP_ENC_ENGINE_FULL			 141
#define EVP_F_DO_EVP_MD_ENGINE				 139
#define EVP_F_DO_EVP_MD_ENGINE_FULL			 142
#define EVP_F_DSAPKEY2PKCS8				 134
#define EVP_F_DSA_PKEY2PKCS8				 135
#define EVP_F_ECDSA_PKEY2PKCS8				 129
#define EVP_F_ECKEY_PKEY2PKCS8				 132
#define EVP_F_EVP_CIPHERINIT				 137
#define EVP_F_EVP_CIPHERINIT_EX				 123
#define EVP_F_EVP_CIPHER_CTX_CTRL			 124
#define EVP_F_EVP_CIPHER_CTX_SET_KEY_LENGTH		 122
#define EVP_F_EVP_DECRYPTFINAL_EX			 101
#define EVP_F_EVP_DIGESTINIT				 136
#define EVP_F_EVP_DIGESTINIT_EX				 128
#define EVP_F_EVP_ENCRYPTFINAL_EX			 127
#define EVP_F_EVP_MD_CTX_COPY_EX			 110
#define EVP_F_EVP_OPENINIT				 102
#define EVP_F_EVP_PBE_ALG_ADD				 115
#define EVP_F_EVP_PBE_CIPHERINIT			 116
#define EVP_F_EVP_PKCS82PKEY				 111
#define EVP_F_EVP_PKEY2PKCS8_BROKEN			 113
#define EVP_F_EVP_PKEY_COPY_PARAMETERS			 103
#define EVP_F_EVP_PKEY_DECRYPT				 104
#define EVP_F_EVP_PKEY_ENCRYPT				 105
#define EVP_F_EVP_PKEY_GET1_DH				 119
#define EVP_F_EVP_PKEY_GET1_DSA				 120
#define EVP_F_EVP_PKEY_GET1_ECDSA			 130
#define EVP_F_EVP_PKEY_GET1_EC_KEY			 131
#define EVP_F_EVP_PKEY_GET1_RSA				 121
#define EVP_F_EVP_PKEY_NEW				 106
#define EVP_F_EVP_RIJNDAEL				 126
#define EVP_F_EVP_SIGNFINAL				 107
#define EVP_F_EVP_VERIFYFINAL				 108
#define EVP_F_PKCS5_PBE_KEYIVGEN			 117
#define EVP_F_PKCS5_V2_PBE_KEYIVGEN			 118
#define EVP_F_PKCS8_SET_BROKEN				 112
#define EVP_F_RC2_MAGIC_TO_METH				 109
#define EVP_F_RC5_CTRL					 125

/* Reason codes. */
#define EVP_R_AES_KEY_SETUP_FAILED			 143
#define EVP_R_ASN1_LIB					 140
#define EVP_R_BAD_BLOCK_LENGTH				 136
#define EVP_R_BAD_DECRYPT				 100
#define EVP_R_BAD_KEY_LENGTH				 137
#define EVP_R_BN_DECODE_ERROR				 112
#define EVP_R_BN_PUBKEY_ERROR				 113
#define EVP_R_CAMELLIA_KEY_SETUP_FAILED			 157
#define EVP_R_CIPHER_PARAMETER_ERROR			 122
#define EVP_R_CTRL_NOT_IMPLEMENTED			 132
#define EVP_R_CTRL_OPERATION_NOT_IMPLEMENTED		 133
#define EVP_R_DATA_NOT_MULTIPLE_OF_BLOCK_LENGTH		 138
#define EVP_R_DECODE_ERROR				 114
#define EVP_R_DIFFERENT_KEY_TYPES			 101
#define EVP_R_DISABLED_FOR_FIPS				 144
#define EVP_R_ENCODE_ERROR				 115
#define EVP_R_ERROR_LOADING_SECTION			 145
#define EVP_R_ERROR_SETTING_FIPS_MODE			 146
#define EVP_R_EVP_PBE_CIPHERINIT_ERROR			 119
#define EVP_R_EXPECTING_AN_RSA_KEY			 127
#define EVP_R_EXPECTING_A_DH_KEY			 128
#define EVP_R_EXPECTING_A_DSA_KEY			 129
#define EVP_R_EXPECTING_A_ECDSA_KEY			 141
#define EVP_R_EXPECTING_A_EC_KEY			 142
#define EVP_R_FIPS_MODE_NOT_SUPPORTED			 147
#define EVP_R_INITIALIZATION_ERROR			 134
#define EVP_R_INPUT_NOT_INITIALIZED			 111
#define EVP_R_INVALID_FIPS_MODE				 148
#define EVP_R_INVALID_KEY_LENGTH			 130
#define EVP_R_IV_TOO_LARGE				 102
#define EVP_R_KEYGEN_FAILURE				 120
#define EVP_R_MISSING_PARAMETERS			 103
#define EVP_R_NO_CIPHER_SET				 131
#define EVP_R_NO_DIGEST_SET				 139
#define EVP_R_NO_DSA_PARAMETERS				 116
#define EVP_R_NO_SIGN_FUNCTION_CONFIGURED		 104
#define EVP_R_NO_VERIFY_FUNCTION_CONFIGURED		 105
#define EVP_R_PKCS8_UNKNOWN_BROKEN_TYPE			 117
#define EVP_R_PUBLIC_KEY_NOT_RSA			 106
#define EVP_R_UNKNOWN_OPTION				 149
#define EVP_R_UNKNOWN_PBE_ALGORITHM			 121
#define EVP_R_UNSUPORTED_NUMBER_OF_ROUNDS		 135
#define EVP_R_UNSUPPORTED_CIPHER			 107
#define EVP_R_UNSUPPORTED_KEYLENGTH			 123
#define EVP_R_UNSUPPORTED_KEY_DERIVATION_FUNCTION	 124
#define EVP_R_UNSUPPORTED_KEY_SIZE			 108
#define EVP_R_UNSUPPORTED_PRF				 125
#define EVP_R_UNSUPPORTED_PRIVATE_KEY_ALGORITHM		 118
#define EVP_R_UNSUPPORTED_SALT_TYPE			 126
#define EVP_R_WRONG_FINAL_BLOCK_LENGTH			 109
#define EVP_R_WRONG_PUBLIC_KEY_TYPE			 110
#define EVP_R_SEED_KEY_SETUP_FAILED			 162

#ifdef  __cplusplus
}
#endif
#endif
