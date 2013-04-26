/* evp_locl.h */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2000.
 */
/* ====================================================================
 * Copyright (c) 1999 The OpenSSL Project.  All rights reserved.
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
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

/* Macros to code block cipher wrappers */

/* Wrapper functions for each cipher mode */

#define BLOCK_CIPHER_ecb_loop() \
	size_t i, bl; \
	bl = ctx->cipher->block_size;\
	if(inl < bl) return 1;\
	inl -= bl; \
	for(i=0; i <= inl; i+=bl) 

#define BLOCK_CIPHER_func_ecb(cname, cprefix, kstruct, ksched) \
static int cname##_ecb_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out, const unsigned char *in, size_t inl) \
{\
	BLOCK_CIPHER_ecb_loop() \
		cprefix##_ecb_encrypt(in + i, out + i, &((kstruct *)ctx->cipher_data)->ksched, ctx->encrypt);\
	return 1;\
}

#define EVP_MAXCHUNK ((size_t)1<<(sizeof(long)*8-2))

#define BLOCK_CIPHER_func_ofb(cname, cprefix, cbits, kstruct, ksched) \
static int cname##_ofb_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out, const unsigned char *in, size_t inl) \
{\
	while(inl>=EVP_MAXCHUNK)\
	    {\
	    cprefix##_ofb##cbits##_encrypt(in, out, (long)EVP_MAXCHUNK, &((kstruct *)ctx->cipher_data)->ksched, ctx->iv, &ctx->num);\
	    inl-=EVP_MAXCHUNK;\
	    in +=EVP_MAXCHUNK;\
	    out+=EVP_MAXCHUNK;\
	    }\
	if (inl)\
	    cprefix##_ofb##cbits##_encrypt(in, out, (long)inl, &((kstruct *)ctx->cipher_data)->ksched, ctx->iv, &ctx->num);\
	return 1;\
}

#define BLOCK_CIPHER_func_cbc(cname, cprefix, kstruct, ksched) \
static int cname##_cbc_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out, const unsigned char *in, size_t inl) \
{\
	while(inl>=EVP_MAXCHUNK) \
	    {\
	    cprefix##_cbc_encrypt(in, out, (long)EVP_MAXCHUNK, &((kstruct *)ctx->cipher_data)->ksched, ctx->iv, ctx->encrypt);\
	    inl-=EVP_MAXCHUNK;\
	    in +=EVP_MAXCHUNK;\
	    out+=EVP_MAXCHUNK;\
	    }\
	if (inl)\
	    cprefix##_cbc_encrypt(in, out, (long)inl, &((kstruct *)ctx->cipher_data)->ksched, ctx->iv, ctx->encrypt);\
	return 1;\
}

#define BLOCK_CIPHER_func_cfb(cname, cprefix, cbits, kstruct, ksched) \
static int cname##_cfb##cbits##_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out, const unsigned char *in, size_t inl) \
{\
	size_t chunk=EVP_MAXCHUNK;\
	if (cbits==1)  chunk>>=3;\
	if (inl<chunk) chunk=inl;\
	while(inl && inl>=chunk)\
	    {\
            cprefix##_cfb##cbits##_encrypt(in, out, (long)((cbits==1) && !(ctx->flags & EVP_CIPH_FLAG_LENGTH_BITS) ?inl*8:inl), &((kstruct *)ctx->cipher_data)->ksched, ctx->iv, &ctx->num, ctx->encrypt);\
	    inl-=chunk;\
	    in +=chunk;\
	    out+=chunk;\
	    if(inl<chunk) chunk=inl;\
	    }\
	return 1;\
}

#define BLOCK_CIPHER_all_funcs(cname, cprefix, cbits, kstruct, ksched) \
	BLOCK_CIPHER_func_cbc(cname, cprefix, kstruct, ksched) \
	BLOCK_CIPHER_func_cfb(cname, cprefix, cbits, kstruct, ksched) \
	BLOCK_CIPHER_func_ecb(cname, cprefix, kstruct, ksched) \
	BLOCK_CIPHER_func_ofb(cname, cprefix, cbits, kstruct, ksched)

#define BLOCK_CIPHER_def1(cname, nmode, mode, MODE, kstruct, nid, block_size, \
			  key_len, iv_len, flags, init_key, cleanup, \
			  set_asn1, get_asn1, ctrl) \
static const EVP_CIPHER cname##_##mode = { \
	nid##_##nmode, block_size, key_len, iv_len, \
	flags | EVP_CIPH_##MODE##_MODE, \
	init_key, \
	cname##_##mode##_cipher, \
	cleanup, \
	sizeof(kstruct), \
	set_asn1, get_asn1,\
	ctrl, \
	NULL \
}; \
const EVP_CIPHER *EVP_##cname##_##mode(void) { return &cname##_##mode; }

#define BLOCK_CIPHER_def_cbc(cname, kstruct, nid, block_size, key_len, \
			     iv_len, flags, init_key, cleanup, set_asn1, \
			     get_asn1, ctrl) \
BLOCK_CIPHER_def1(cname, cbc, cbc, CBC, kstruct, nid, block_size, key_len, \
		  iv_len, flags, init_key, cleanup, set_asn1, get_asn1, ctrl)

#define BLOCK_CIPHER_def_cfb(cname, kstruct, nid, key_len, \
			     iv_len, cbits, flags, init_key, cleanup, \
			     set_asn1, get_asn1, ctrl) \
BLOCK_CIPHER_def1(cname, cfb##cbits, cfb##cbits, CFB, kstruct, nid, 1, \
		  key_len, iv_len, flags, init_key, cleanup, set_asn1, \
		  get_asn1, ctrl)

#define BLOCK_CIPHER_def_ofb(cname, kstruct, nid, key_len, \
			     iv_len, cbits, flags, init_key, cleanup, \
			     set_asn1, get_asn1, ctrl) \
BLOCK_CIPHER_def1(cname, ofb##cbits, ofb, OFB, kstruct, nid, 1, \
		  key_len, iv_len, flags, init_key, cleanup, set_asn1, \
		  get_asn1, ctrl)

#define BLOCK_CIPHER_def_ecb(cname, kstruct, nid, block_size, key_len, \
			     flags, init_key, cleanup, set_asn1, \
			     get_asn1, ctrl) \
BLOCK_CIPHER_def1(cname, ecb, ecb, ECB, kstruct, nid, block_size, key_len, \
		  0, flags, init_key, cleanup, set_asn1, get_asn1, ctrl)

#define BLOCK_CIPHER_defs(cname, kstruct, \
			  nid, block_size, key_len, iv_len, cbits, flags, \
			  init_key, cleanup, set_asn1, get_asn1, ctrl) \
BLOCK_CIPHER_def_cbc(cname, kstruct, nid, block_size, key_len, iv_len, flags, \
		     init_key, cleanup, set_asn1, get_asn1, ctrl) \
BLOCK_CIPHER_def_cfb(cname, kstruct, nid, key_len, iv_len, cbits, \
		     flags, init_key, cleanup, set_asn1, get_asn1, ctrl) \
BLOCK_CIPHER_def_ofb(cname, kstruct, nid, key_len, iv_len, cbits, \
		     flags, init_key, cleanup, set_asn1, get_asn1, ctrl) \
BLOCK_CIPHER_def_ecb(cname, kstruct, nid, block_size, key_len, flags, \
		     init_key, cleanup, set_asn1, get_asn1, ctrl)


/*
#define BLOCK_CIPHER_defs(cname, kstruct, \
				nid, block_size, key_len, iv_len, flags,\
				 init_key, cleanup, set_asn1, get_asn1, ctrl)\
static const EVP_CIPHER cname##_cbc = {\
	nid##_cbc, block_size, key_len, iv_len, \
	flags | EVP_CIPH_CBC_MODE,\
	init_key,\
	cname##_cbc_cipher,\
	cleanup,\
	sizeof(EVP_CIPHER_CTX)-sizeof((((EVP_CIPHER_CTX *)NULL)->c))+\
		sizeof((((EVP_CIPHER_CTX *)NULL)->c.kstruct)),\
	set_asn1, get_asn1,\
	ctrl, \
	NULL \
};\
const EVP_CIPHER *EVP_##cname##_cbc(void) { return &cname##_cbc; }\
static const EVP_CIPHER cname##_cfb = {\
	nid##_cfb64, 1, key_len, iv_len, \
	flags | EVP_CIPH_CFB_MODE,\
	init_key,\
	cname##_cfb_cipher,\
	cleanup,\
	sizeof(EVP_CIPHER_CTX)-sizeof((((EVP_CIPHER_CTX *)NULL)->c))+\
		sizeof((((EVP_CIPHER_CTX *)NULL)->c.kstruct)),\
	set_asn1, get_asn1,\
	ctrl,\
	NULL \
};\
const EVP_CIPHER *EVP_##cname##_cfb(void) { return &cname##_cfb; }\
static const EVP_CIPHER cname##_ofb = {\
	nid##_ofb64, 1, key_len, iv_len, \
	flags | EVP_CIPH_OFB_MODE,\
	init_key,\
	cname##_ofb_cipher,\
	cleanup,\
	sizeof(EVP_CIPHER_CTX)-sizeof((((EVP_CIPHER_CTX *)NULL)->c))+\
		sizeof((((EVP_CIPHER_CTX *)NULL)->c.kstruct)),\
	set_asn1, get_asn1,\
	ctrl,\
	NULL \
};\
const EVP_CIPHER *EVP_##cname##_ofb(void) { return &cname##_ofb; }\
static const EVP_CIPHER cname##_ecb = {\
	nid##_ecb, block_size, key_len, iv_len, \
	flags | EVP_CIPH_ECB_MODE,\
	init_key,\
	cname##_ecb_cipher,\
	cleanup,\
	sizeof(EVP_CIPHER_CTX)-sizeof((((EVP_CIPHER_CTX *)NULL)->c))+\
		sizeof((((EVP_CIPHER_CTX *)NULL)->c.kstruct)),\
	set_asn1, get_asn1,\
	ctrl,\
	NULL \
};\
const EVP_CIPHER *EVP_##cname##_ecb(void) { return &cname##_ecb; }
*/

#define IMPLEMENT_BLOCK_CIPHER(cname, ksched, cprefix, kstruct, nid, \
			       block_size, key_len, iv_len, cbits, \
			       flags, init_key, \
			       cleanup, set_asn1, get_asn1, ctrl) \
	BLOCK_CIPHER_all_funcs(cname, cprefix, cbits, kstruct, ksched) \
	BLOCK_CIPHER_defs(cname, kstruct, nid, block_size, key_len, iv_len, \
			  cbits, flags, init_key, cleanup, set_asn1, \
			  get_asn1, ctrl)

#define EVP_C_DATA(kstruct, ctx)	((kstruct *)(ctx)->cipher_data)

#define IMPLEMENT_CFBR(cipher,cprefix,kstruct,ksched,keysize,cbits,iv_len) \
	BLOCK_CIPHER_func_cfb(cipher##_##keysize,cprefix,cbits,kstruct,ksched) \
	BLOCK_CIPHER_def_cfb(cipher##_##keysize,kstruct, \
			     NID_##cipher##_##keysize, keysize/8, iv_len, cbits, \
			     0, cipher##_init_key, NULL, \
			     EVP_CIPHER_set_asn1_iv, \
			     EVP_CIPHER_get_asn1_iv, \
			     NULL)

struct evp_pkey_ctx_st
	{
	/* Method associated with this operation */
	const EVP_PKEY_METHOD *pmeth;
	/* Engine that implements this method or NULL if builtin */
	ENGINE *engine;
	/* Key: may be NULL */
	EVP_PKEY *pkey;
	/* Peer key for key agreement, may be NULL */
	EVP_PKEY *peerkey;
	/* Actual operation */
	int operation;
	/* Algorithm specific data */
	void *data;
	/* Application specific data */
	void *app_data;
	/* Keygen callback */
	EVP_PKEY_gen_cb *pkey_gencb;
	/* implementation specific keygen data */
	int *keygen_info;
	int keygen_info_count;
	} /* EVP_PKEY_CTX */;

#define EVP_PKEY_FLAG_DYNAMIC	1

struct evp_pkey_method_st
	{
	int pkey_id;
	int flags;

	int (*init)(EVP_PKEY_CTX *ctx);
	int (*copy)(EVP_PKEY_CTX *dst, EVP_PKEY_CTX *src);
	void (*cleanup)(EVP_PKEY_CTX *ctx);

	int (*paramgen_init)(EVP_PKEY_CTX *ctx);
	int (*paramgen)(EVP_PKEY_CTX *ctx, EVP_PKEY *pkey);

	int (*keygen_init)(EVP_PKEY_CTX *ctx);
	int (*keygen)(EVP_PKEY_CTX *ctx, EVP_PKEY *pkey);

	int (*sign_init)(EVP_PKEY_CTX *ctx);
	int (*sign)(EVP_PKEY_CTX *ctx, unsigned char *sig, size_t *siglen,
				const unsigned char *tbs, size_t tbslen);

	int (*verify_init)(EVP_PKEY_CTX *ctx);
	int (*verify)(EVP_PKEY_CTX *ctx,
				const unsigned char *sig, size_t siglen,
				const unsigned char *tbs, size_t tbslen);

	int (*verify_recover_init)(EVP_PKEY_CTX *ctx);
	int (*verify_recover)(EVP_PKEY_CTX *ctx,
				unsigned char *rout, size_t *routlen,
				const unsigned char *sig, size_t siglen);

	int (*signctx_init)(EVP_PKEY_CTX *ctx, EVP_MD_CTX *mctx);
	int (*signctx)(EVP_PKEY_CTX *ctx, unsigned char *sig, size_t *siglen,
					EVP_MD_CTX *mctx);

	int (*verifyctx_init)(EVP_PKEY_CTX *ctx, EVP_MD_CTX *mctx);
	int (*verifyctx)(EVP_PKEY_CTX *ctx, const unsigned char *sig,int siglen,
					EVP_MD_CTX *mctx);

	int (*encrypt_init)(EVP_PKEY_CTX *ctx);
	int (*encrypt)(EVP_PKEY_CTX *ctx, unsigned char *out, size_t *outlen,
					const unsigned char *in, size_t inlen);

	int (*decrypt_init)(EVP_PKEY_CTX *ctx);
	int (*decrypt)(EVP_PKEY_CTX *ctx, unsigned char *out, size_t *outlen,
					const unsigned char *in, size_t inlen);

	int (*derive_init)(EVP_PKEY_CTX *ctx);
	int (*derive)(EVP_PKEY_CTX *ctx, unsigned char *key, size_t *keylen);

	int (*ctrl)(EVP_PKEY_CTX *ctx, int type, int p1, void *p2);
	int (*ctrl_str)(EVP_PKEY_CTX *ctx, const char *type, const char *value);


	} /* EVP_PKEY_METHOD */;

void evp_pkey_set_cb_translate(BN_GENCB *cb, EVP_PKEY_CTX *ctx);
