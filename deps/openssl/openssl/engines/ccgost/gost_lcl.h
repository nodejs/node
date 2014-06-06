#ifndef GOST_TOOLS_H
#define GOST_TOOLS_H
/**********************************************************************
 *                        gost_lcl.h                                  *
 *             Copyright (c) 2006 Cryptocom LTD                       *
 *       This file is distributed under the same license as OpenSSL   *
 *                                                                    *
 *         Internal declarations  used in GOST engine                *
 *         OpenSSL 0.9.9 libraries required to compile and use        *
 *                              this code                             *
 **********************************************************************/ 
#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/dsa.h>
#include <openssl/asn1t.h>
#include <openssl/x509.h>
#include <openssl/engine.h>
#include <openssl/ec.h>
#include "gost89.h"
#include "gosthash.h"
/* Control commands */
#define GOST_PARAM_CRYPT_PARAMS 0
#define GOST_PARAM_MAX 0
#define GOST_CTRL_CRYPT_PARAMS (ENGINE_CMD_BASE+GOST_PARAM_CRYPT_PARAMS)

	extern const ENGINE_CMD_DEFN gost_cmds[];
	int gost_control_func(ENGINE *e,int cmd, long i, void *p, void (*f)(void));
	const char *get_gost_engine_param(int param);	
	int gost_set_default_param(int param, const char *value); 
	void gost_param_free(void);

/* method registration */

	int register_ameth_gost (int nid, EVP_PKEY_ASN1_METHOD **ameth, const char* pemstr, const char* info);
	int register_pmeth_gost (int id, EVP_PKEY_METHOD **pmeth, int flags);

/* Gost-specific pmeth control-function parameters */
/* For GOST R34.10 parameters */
#define param_ctrl_string "paramset"
#define EVP_PKEY_CTRL_GOST_PARAMSET (EVP_PKEY_ALG_CTRL+1)
/* For GOST 28147 MAC */
#define key_ctrl_string "key"
#define hexkey_ctrl_string "hexkey"
#define EVP_PKEY_CTRL_GOST_MAC_HEXKEY (EVP_PKEY_ALG_CTRL+3)
/* Pmeth internal representation */
	struct gost_pmeth_data {
   	    int sign_param_nid; /* Should be set whenever parameters are filled */
		EVP_MD *md;
		unsigned char *shared_ukm;
		int peer_key_used;
	};

	struct gost_mac_pmeth_data {
		int key_set;
		EVP_MD *md;
		unsigned char key[32];
	}	;
/* GOST-specific ASN1 structures */


typedef struct {
	ASN1_OCTET_STRING *encrypted_key;
	ASN1_OCTET_STRING *imit;
} GOST_KEY_INFO;

DECLARE_ASN1_FUNCTIONS(GOST_KEY_INFO)

typedef struct {
	ASN1_OBJECT *cipher;
	X509_PUBKEY *ephem_key;
	ASN1_OCTET_STRING *eph_iv;
} GOST_KEY_AGREEMENT_INFO;

DECLARE_ASN1_FUNCTIONS(GOST_KEY_AGREEMENT_INFO)
	
typedef struct {
	GOST_KEY_INFO *key_info;
	GOST_KEY_AGREEMENT_INFO *key_agreement_info;
} GOST_KEY_TRANSPORT;

DECLARE_ASN1_FUNCTIONS(GOST_KEY_TRANSPORT)

typedef struct { /* FIXME incomplete */
	GOST_KEY_TRANSPORT *gkt;
} GOST_CLIENT_KEY_EXCHANGE_PARAMS;

/* Hacks to shorten symbols to 31 characters or less, or OpenVMS.
   This mimics what's done in symhacks.h, but since this is a very
   local header file, I prefered to put this hack directly here.
   -- Richard Levitte */
#ifdef OPENSSL_SYS_VMS
#undef GOST_CLIENT_KEY_EXCHANGE_PARAMS_it
#define GOST_CLIENT_KEY_EXCHANGE_PARAMS_it	GOST_CLIENT_KEY_EXC_PARAMS_it
#undef GOST_CLIENT_KEY_EXCHANGE_PARAMS_new
#define GOST_CLIENT_KEY_EXCHANGE_PARAMS_new	GOST_CLIENT_KEY_EXC_PARAMS_new
#undef GOST_CLIENT_KEY_EXCHANGE_PARAMS_free
#define GOST_CLIENT_KEY_EXCHANGE_PARAMS_free	GOST_CLIENT_KEY_EXC_PARAMS_free
#undef d2i_GOST_CLIENT_KEY_EXCHANGE_PARAMS
#define d2i_GOST_CLIENT_KEY_EXCHANGE_PARAMS	d2i_GOST_CLIENT_KEY_EXC_PARAMS
#undef i2d_GOST_CLIENT_KEY_EXCHANGE_PARAMS
#define i2d_GOST_CLIENT_KEY_EXCHANGE_PARAMS	i2d_GOST_CLIENT_KEY_EXC_PARAMS
#endif /* End of hack */
DECLARE_ASN1_FUNCTIONS(GOST_CLIENT_KEY_EXCHANGE_PARAMS)
typedef struct {
	ASN1_OBJECT *key_params;
	ASN1_OBJECT *hash_params;
	ASN1_OBJECT *cipher_params;
} GOST_KEY_PARAMS;

DECLARE_ASN1_FUNCTIONS(GOST_KEY_PARAMS)

typedef struct {
	ASN1_OCTET_STRING *iv;
	ASN1_OBJECT *enc_param_set;
} GOST_CIPHER_PARAMS; 

DECLARE_ASN1_FUNCTIONS(GOST_CIPHER_PARAMS)
/*============== Message digest  and cipher related structures  ==========*/
	 /* Structure used as EVP_MD_CTX-md_data. 
	  * It allows to avoid storing in the md-data pointers to
	  * dynamically allocated memory.
	  *
	  * I cannot invent better way to avoid memory leaks, because
	  * openssl insist on invoking Init on Final-ed digests, and there
	  * is no reliable way to find out whether pointer in the passed
	  * md_data is valid or not.
	  * */
struct ossl_gost_digest_ctx {
	gost_hash_ctx dctx;
	gost_ctx cctx;
};	
/* EVP_MD structure for GOST R 34.11 */
extern EVP_MD digest_gost;
/* EVP_MD structure for GOST 28147 in MAC mode */
extern EVP_MD imit_gost_cpa;
/* Cipher context used for EVP_CIPHER operation */
struct ossl_gost_cipher_ctx {
	int paramNID;
	unsigned int count;
	int key_meshing;
	gost_ctx cctx;
};	
/* Structure to map parameter NID to S-block */
struct gost_cipher_info {
	int nid;
	gost_subst_block *sblock;
	int key_meshing;
};
/* Context for MAC */
struct ossl_gost_imit_ctx {
	gost_ctx cctx;
	unsigned char buffer[8];
	unsigned char partial_block[8];
	unsigned int count;
	int key_meshing;
	int bytes_left;
	int key_set;
};	
/* Table which maps parameter NID to S-blocks */
extern struct gost_cipher_info gost_cipher_list[];
/* Find encryption params from ASN1_OBJECT */
const struct gost_cipher_info *get_encryption_params(ASN1_OBJECT *obj);
/* Implementation of GOST 28147-89 cipher in CFB and CNT modes */
extern EVP_CIPHER cipher_gost;
extern EVP_CIPHER cipher_gost_cpacnt;
#define EVP_MD_CTRL_KEY_LEN (EVP_MD_CTRL_ALG_CTRL+3)
#define EVP_MD_CTRL_SET_KEY (EVP_MD_CTRL_ALG_CTRL+4)
/* EVP_PKEY_METHOD key encryption callbacks */
/* From gost94_keyx.c */
int pkey_GOST94cp_encrypt(EVP_PKEY_CTX *ctx, unsigned char *out, size_t *outlen, const unsigned char* key, size_t key_len );

int pkey_GOST94cp_decrypt(EVP_PKEY_CTX *ctx, unsigned char *out, size_t *outlen, const unsigned char* in, size_t in_len );
/* From gost2001_keyx.c */
int pkey_GOST01cp_encrypt(EVP_PKEY_CTX *ctx, unsigned char *out, size_t *outlen, const unsigned char* key, size_t key_len );

int pkey_GOST01cp_decrypt(EVP_PKEY_CTX *ctx, unsigned char *out, size_t *outlen, const unsigned char* in, size_t in_len );
/* derive functions */
/* From gost2001_keyx.c */
int pkey_gost2001_derive(EVP_PKEY_CTX *ctx, unsigned char *key, size_t *keylen);
/* From gost94_keyx.c */
int pkey_gost94_derive(EVP_PKEY_CTX *ctx, unsigned char *key, size_t *keylen);
/* Internal functions for signature algorithms */
int fill_GOST94_params(DSA *dsa,int nid);
int fill_GOST2001_params(EC_KEY *eckey, int nid);
int gost_sign_keygen(DSA *dsa) ;
int gost2001_keygen(EC_KEY *ec) ;

DSA_SIG *gost_do_sign(const unsigned char *dgst,int dlen, DSA *dsa) ;
DSA_SIG *gost2001_do_sign(const unsigned char *dgst,int dlen, EC_KEY *eckey);

int gost_do_verify(const unsigned char *dgst, int dgst_len,
		DSA_SIG *sig, DSA *dsa) ;
int gost2001_do_verify(const unsigned char *dgst,int dgst_len,
			DSA_SIG *sig, EC_KEY *ec);
int gost2001_compute_public(EC_KEY *ec) ;
int gost94_compute_public(DSA *dsa) ;
/*============== miscellaneous functions============================= */
/* from gost_sign.c */
/* Convert GOST R 34.11 hash sum to bignum according to standard */
BIGNUM *hashsum2bn(const unsigned char *dgst) ;
/* Store bignum in byte array of given length, prepending by zeros
 * if nesseccary */
int store_bignum(BIGNUM *bn, unsigned char *buf,int len);
/* Read bignum, which can have few MSB all-zeros    from buffer*/ 
BIGNUM *getbnfrombuf(const unsigned char *buf,size_t len);
/* Pack GOST R 34.10 signature according to CryptoPro rules */
int pack_sign_cp(DSA_SIG *s,int order,unsigned char *sig, size_t *siglen); 
/* Unpack GOST R 34.10 signature according to CryptoPro rules */
DSA_SIG *unpack_cp_signature(const unsigned char *sig,size_t siglen) ;
/* from ameth.c */
/* Get private key as BIGNUM from both R 34.10-94 and R 34.10-2001  keys*/
/* Returns pointer into EVP_PKEY structure */
BIGNUM* gost_get0_priv_key(const EVP_PKEY *pkey) ;
/* Find NID by GOST 94 parameters */
int gost94_nid_by_params(DSA *p) ;


#endif
