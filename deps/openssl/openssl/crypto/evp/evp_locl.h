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
	unsigned int i, bl; \
	bl = ctx->cipher->block_size;\
	if(inl < bl) return 1;\
	inl -= bl; \
	for(i=0; i <= inl; i+=bl) 

#define BLOCK_CIPHER_func_ecb(cname, cprefix, kstruct, ksched) \
static int cname##_ecb_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out, const unsigned char *in, unsigned int inl) \
{\
	BLOCK_CIPHER_ecb_loop() \
		cprefix##_ecb_encrypt(in + i, out + i, &((kstruct *)ctx->cipher_data)->ksched, ctx->encrypt);\
	return 1;\
}

#define BLOCK_CIPHER_func_ofb(cname, cprefix, cbits, kstruct, ksched) \
static int cname##_ofb_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out, const unsigned char *in, unsigned int inl) \
{\
	cprefix##_ofb##cbits##_encrypt(in, out, (long)inl, &((kstruct *)ctx->cipher_data)->ksched, ctx->iv, &ctx->num);\
	return 1;\
}

#define BLOCK_CIPHER_func_cbc(cname, cprefix, kstruct, ksched) \
static int cname##_cbc_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out, const unsigned char *in, unsigned int inl) \
{\
	cprefix##_cbc_encrypt(in, out, (long)inl, &((kstruct *)ctx->cipher_data)->ksched, ctx->iv, ctx->encrypt);\
	return 1;\
}

#define BLOCK_CIPHER_func_cfb(cname, cprefix, cbits, kstruct, ksched) \
static int cname##_cfb##cbits##_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out, const unsigned char *in, unsigned int inl) \
{\
	cprefix##_cfb##cbits##_encrypt(in, out, (long)((cbits==1) && !(ctx->flags & EVP_CIPH_FLAG_LENGTH_BITS) ?inl*8:inl), &((kstruct *)ctx->cipher_data)->ksched, ctx->iv, &ctx->num, ctx->encrypt);\
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

#define IMPLEMENT_CFBR(cipher,cprefix,kstruct,ksched,keysize,cbits,iv_len,fl) \
	BLOCK_CIPHER_func_cfb(cipher##_##keysize,cprefix,cbits,kstruct,ksched) \
	BLOCK_CIPHER_def_cfb(cipher##_##keysize,kstruct, \
			     NID_##cipher##_##keysize, keysize/8, iv_len, cbits, \
			     (fl)|EVP_CIPH_FLAG_DEFAULT_ASN1, \
			     cipher##_init_key, NULL, NULL, NULL, NULL)

#ifdef OPENSSL_FIPS
#define RC2_set_key	private_RC2_set_key
#define RC4_set_key	private_RC4_set_key
#define CAST_set_key	private_CAST_set_key
#define RC5_32_set_key	private_RC5_32_set_key
#define BF_set_key	private_BF_set_key
#define Camellia_set_key private_Camellia_set_key
#define idea_set_encrypt_key private_idea_set_encrypt_key

#define MD5_Init	private_MD5_Init
#define MD4_Init	private_MD4_Init
#define MD2_Init	private_MD2_Init
#define MDC2_Init	private_MDC2_Init
#define SHA_Init	private_SHA_Init

#endif

