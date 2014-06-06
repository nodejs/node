/* engines/e_capi.c */
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


#include <stdio.h>
#include <string.h>
#include <openssl/crypto.h>
#include <openssl/buffer.h>
#include <openssl/bn.h>

#ifdef OPENSSL_SYS_WIN32
#ifndef OPENSSL_NO_CAPIENG

#include <openssl/rsa.h>

#include <windows.h>

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0400
#endif

#include <wincrypt.h>

/*
 * This module uses several "new" interfaces, among which is
 * CertGetCertificateContextProperty. CERT_KEY_PROV_INFO_PROP_ID is
 * one of possible values you can pass to function in question. By
 * checking if it's defined we can see if wincrypt.h and accompanying
 * crypt32.lib are in shape. The native MingW32 headers up to and
 * including __W32API_VERSION 3.14 lack of struct DSSPUBKEY and the
 * defines CERT_STORE_PROV_SYSTEM_A and CERT_STORE_READONLY_FLAG,
 * so we check for these too and avoid compiling.
 * Yes, it's rather "weak" test and if compilation fails,
 * then re-configure with -DOPENSSL_NO_CAPIENG.
 */
#if defined(CERT_KEY_PROV_INFO_PROP_ID) && \
    defined(CERT_STORE_PROV_SYSTEM_A) && \
    defined(CERT_STORE_READONLY_FLAG)
# define __COMPILE_CAPIENG
#endif /* CERT_KEY_PROV_INFO_PROP_ID */
#endif /* OPENSSL_NO_CAPIENG */
#endif /* OPENSSL_SYS_WIN32 */

#ifdef __COMPILE_CAPIENG

#undef X509_EXTENSIONS
#undef X509_CERT_PAIR

/* Definitions which may be missing from earlier version of headers */
#ifndef CERT_STORE_OPEN_EXISTING_FLAG
#define CERT_STORE_OPEN_EXISTING_FLAG                   0x00004000
#endif

#ifndef CERT_STORE_CREATE_NEW_FLAG
#define CERT_STORE_CREATE_NEW_FLAG                      0x00002000
#endif

#ifndef CERT_SYSTEM_STORE_CURRENT_USER
#define CERT_SYSTEM_STORE_CURRENT_USER			0x00010000
#endif 

#include <openssl/engine.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>

#include "e_capi_err.h"
#include "e_capi_err.c"


static const char *engine_capi_id = "capi";
static const char *engine_capi_name = "CryptoAPI ENGINE";

typedef struct CAPI_CTX_st CAPI_CTX;
typedef struct CAPI_KEY_st CAPI_KEY;

static void capi_addlasterror(void);
static void capi_adderror(DWORD err);

static void CAPI_trace(CAPI_CTX *ctx, char *format, ...);

static int capi_list_providers(CAPI_CTX *ctx, BIO *out);
static int capi_list_containers(CAPI_CTX *ctx, BIO *out);
int capi_list_certs(CAPI_CTX *ctx, BIO *out, char *storename);
void capi_free_key(CAPI_KEY *key);

static PCCERT_CONTEXT capi_find_cert(CAPI_CTX *ctx, const char *id, HCERTSTORE hstore);

CAPI_KEY *capi_find_key(CAPI_CTX *ctx, const char *id);

static EVP_PKEY *capi_load_privkey(ENGINE *eng, const char *key_id,
	UI_METHOD *ui_method, void *callback_data);
static int capi_rsa_sign(int dtype, const unsigned char *m, unsigned int m_len,
             unsigned char *sigret, unsigned int *siglen, const RSA *rsa);
static int capi_rsa_priv_enc(int flen, const unsigned char *from,
                unsigned char *to, RSA *rsa, int padding);
static int capi_rsa_priv_dec(int flen, const unsigned char *from,
                unsigned char *to, RSA *rsa, int padding);
static int capi_rsa_free(RSA *rsa);

static DSA_SIG *capi_dsa_do_sign(const unsigned char *digest, int dlen,
							DSA *dsa);
static int capi_dsa_free(DSA *dsa);

static int capi_load_ssl_client_cert(ENGINE *e, SSL *ssl,
	STACK_OF(X509_NAME) *ca_dn, X509 **pcert, EVP_PKEY **pkey,
	STACK_OF(X509) **pother, UI_METHOD *ui_method, void *callback_data);

static int cert_select_simple(ENGINE *e, SSL *ssl, STACK_OF(X509) *certs);
#ifdef OPENSSL_CAPIENG_DIALOG
static int cert_select_dialog(ENGINE *e, SSL *ssl, STACK_OF(X509) *certs);
#endif

typedef PCCERT_CONTEXT (WINAPI *CERTDLG)(HCERTSTORE, HWND, LPCWSTR,
						LPCWSTR, DWORD, DWORD,
						void *);
typedef HWND (WINAPI *GETCONSWIN)(void);

/* This structure contains CAPI ENGINE specific data:
 * it contains various global options and affects how
 * other functions behave.
 */

#define CAPI_DBG_TRACE	2
#define CAPI_DBG_ERROR	1

struct CAPI_CTX_st {
	int debug_level;
	char *debug_file;
	/* Parameters to use for container lookup */
	DWORD keytype;
	LPSTR cspname;
	DWORD csptype;
	/* Certificate store name to use */
	LPSTR storename;
	LPSTR ssl_client_store;
	/* System store flags */
	DWORD store_flags;

/* Lookup string meanings in load_private_key */
/* Substring of subject: uses "storename" */
#define CAPI_LU_SUBSTR		1
/* Friendly name: uses storename */
#define CAPI_LU_FNAME		2
/* Container name: uses cspname, keytype */
#define CAPI_LU_CONTNAME	3
	int lookup_method;
/* Info to dump with dumpcerts option */
/* Issuer and serial name strings */
#define CAPI_DMP_SUMMARY	0x1
/* Friendly name */
#define CAPI_DMP_FNAME		0x2
/* Full X509_print dump */
#define CAPI_DMP_FULL		0x4
/* Dump PEM format certificate */
#define CAPI_DMP_PEM		0x8
/* Dump pseudo key (if possible) */
#define CAPI_DMP_PSKEY		0x10
/* Dump key info (if possible) */
#define CAPI_DMP_PKEYINFO	0x20

	DWORD dump_flags;
	int (*client_cert_select)(ENGINE *e, SSL *ssl, STACK_OF(X509) *certs);

	CERTDLG certselectdlg;
	GETCONSWIN getconswindow;
};


static CAPI_CTX *capi_ctx_new();
static void capi_ctx_free(CAPI_CTX *ctx);
static int capi_ctx_set_provname(CAPI_CTX *ctx, LPSTR pname, DWORD type, int check);
static int capi_ctx_set_provname_idx(CAPI_CTX *ctx, int idx);

#define CAPI_CMD_LIST_CERTS		ENGINE_CMD_BASE
#define CAPI_CMD_LOOKUP_CERT		(ENGINE_CMD_BASE + 1)
#define CAPI_CMD_DEBUG_LEVEL		(ENGINE_CMD_BASE + 2)
#define CAPI_CMD_DEBUG_FILE		(ENGINE_CMD_BASE + 3)
#define CAPI_CMD_KEYTYPE		(ENGINE_CMD_BASE + 4)
#define CAPI_CMD_LIST_CSPS		(ENGINE_CMD_BASE + 5)
#define CAPI_CMD_SET_CSP_IDX		(ENGINE_CMD_BASE + 6)
#define CAPI_CMD_SET_CSP_NAME		(ENGINE_CMD_BASE + 7)
#define CAPI_CMD_SET_CSP_TYPE		(ENGINE_CMD_BASE + 8)
#define CAPI_CMD_LIST_CONTAINERS	(ENGINE_CMD_BASE + 9)
#define CAPI_CMD_LIST_OPTIONS		(ENGINE_CMD_BASE + 10)
#define CAPI_CMD_LOOKUP_METHOD		(ENGINE_CMD_BASE + 11)
#define CAPI_CMD_STORE_NAME		(ENGINE_CMD_BASE + 12)
#define CAPI_CMD_STORE_FLAGS		(ENGINE_CMD_BASE + 13)

static const ENGINE_CMD_DEFN capi_cmd_defns[] = {
	{CAPI_CMD_LIST_CERTS,
		"list_certs",
		"List all certificates in store",
		ENGINE_CMD_FLAG_NO_INPUT},
	{CAPI_CMD_LOOKUP_CERT,
		"lookup_cert",
		"Lookup and output certificates",
		ENGINE_CMD_FLAG_STRING},
	{CAPI_CMD_DEBUG_LEVEL,
		"debug_level",
		"debug level (1=errors, 2=trace)",
		ENGINE_CMD_FLAG_NUMERIC},
	{CAPI_CMD_DEBUG_FILE,
		"debug_file",
		"debugging filename)",
		ENGINE_CMD_FLAG_STRING},
	{CAPI_CMD_KEYTYPE,
		"key_type",
		"Key type: 1=AT_KEYEXCHANGE (default), 2=AT_SIGNATURE",
		ENGINE_CMD_FLAG_NUMERIC},
	{CAPI_CMD_LIST_CSPS,
		"list_csps",
		"List all CSPs",
		ENGINE_CMD_FLAG_NO_INPUT},
	{CAPI_CMD_SET_CSP_IDX,
		"csp_idx",
		"Set CSP by index",
		ENGINE_CMD_FLAG_NUMERIC},
	{CAPI_CMD_SET_CSP_NAME,
		"csp_name",
		"Set CSP name, (default CSP used if not specified)",
		ENGINE_CMD_FLAG_STRING},
	{CAPI_CMD_SET_CSP_TYPE,
		"csp_type",
		"Set CSP type, (default RSA_PROV_FULL)",
		ENGINE_CMD_FLAG_NUMERIC},
	{CAPI_CMD_LIST_CONTAINERS,
		"list_containers",
		"list container names",
		ENGINE_CMD_FLAG_NO_INPUT},
	{CAPI_CMD_LIST_OPTIONS,
		"list_options",
		"Set list options (1=summary,2=friendly name, 4=full printout, 8=PEM output, 16=XXX, "
		"32=private key info)",
		ENGINE_CMD_FLAG_NUMERIC},
	{CAPI_CMD_LOOKUP_METHOD,
		"lookup_method",
		"Set key lookup method (1=substring, 2=friendlyname, 3=container name)",
		ENGINE_CMD_FLAG_NUMERIC},
	{CAPI_CMD_STORE_NAME,
		"store_name",
		"certificate store name, default \"MY\"",
		ENGINE_CMD_FLAG_STRING},
	{CAPI_CMD_STORE_FLAGS,
		"store_flags",
		"Certificate store flags: 1 = system store",
		ENGINE_CMD_FLAG_NUMERIC},

	{0, NULL, NULL, 0}
	};

static int capi_idx = -1;
static int rsa_capi_idx = -1;
static int dsa_capi_idx = -1;
static int cert_capi_idx = -1;

static int capi_ctrl(ENGINE *e, int cmd, long i, void *p, void (*f)(void))
	{
	int ret = 1;
	CAPI_CTX *ctx;
	BIO *out;
	if (capi_idx == -1)
		{
		CAPIerr(CAPI_F_CAPI_CTRL, CAPI_R_ENGINE_NOT_INITIALIZED);
		return 0;
		}
	ctx = ENGINE_get_ex_data(e, capi_idx);
	out = BIO_new_fp(stdout, BIO_NOCLOSE);
	switch (cmd)
		{
		case CAPI_CMD_LIST_CSPS:
		ret = capi_list_providers(ctx, out);
		break;

		case CAPI_CMD_LIST_CERTS:
		ret = capi_list_certs(ctx, out, NULL);
		break;

		case CAPI_CMD_LOOKUP_CERT:
		ret = capi_list_certs(ctx, out, p);
		break;

		case CAPI_CMD_LIST_CONTAINERS:
		ret = capi_list_containers(ctx, out);
		break;

		case CAPI_CMD_STORE_NAME:
		if (ctx->storename)
			OPENSSL_free(ctx->storename);
		ctx->storename = BUF_strdup(p);
		CAPI_trace(ctx, "Setting store name to %s\n", p);
		break;

		case CAPI_CMD_STORE_FLAGS:
		if (i & 1)
			{
			ctx->store_flags |= CERT_SYSTEM_STORE_LOCAL_MACHINE;
			ctx->store_flags &= ~CERT_SYSTEM_STORE_CURRENT_USER;
			}
		else
			{
			ctx->store_flags |= CERT_SYSTEM_STORE_CURRENT_USER;
			ctx->store_flags &= ~CERT_SYSTEM_STORE_LOCAL_MACHINE;
			}
		CAPI_trace(ctx, "Setting flags to %d\n", i);
		break;

		case CAPI_CMD_DEBUG_LEVEL:
		ctx->debug_level = (int)i;
		CAPI_trace(ctx, "Setting debug level to %d\n", ctx->debug_level);
		break;

		case CAPI_CMD_DEBUG_FILE:
		ctx->debug_file = BUF_strdup(p);
		CAPI_trace(ctx, "Setting debug file to %s\n", ctx->debug_file);
		break;

		case CAPI_CMD_KEYTYPE:
		ctx->keytype = i;
		CAPI_trace(ctx, "Setting key type to %d\n", ctx->keytype);
		break;

		case CAPI_CMD_SET_CSP_IDX:
		ret = capi_ctx_set_provname_idx(ctx, i);
		break;

		case CAPI_CMD_LIST_OPTIONS:
		ctx->dump_flags = i;
		break;

		case CAPI_CMD_LOOKUP_METHOD:
		if (i < 1 || i > 3)
			{
			CAPIerr(CAPI_F_CAPI_CTRL, CAPI_R_INVALID_LOOKUP_METHOD);
			return 0;
			}
		ctx->lookup_method = i;
		break;

		case CAPI_CMD_SET_CSP_NAME:
		ret = capi_ctx_set_provname(ctx, p, ctx->csptype, 1);
		break;

		case CAPI_CMD_SET_CSP_TYPE:
		ctx->csptype = i;
		break;

		default:
		CAPIerr(CAPI_F_CAPI_CTRL, CAPI_R_UNKNOWN_COMMAND);
		ret = 0;
	}

	BIO_free(out);
	return ret;

	}

static RSA_METHOD capi_rsa_method =
	{
	"CryptoAPI RSA method",
	0,				/* pub_enc */
	0,				/* pub_dec */
	capi_rsa_priv_enc,		/* priv_enc */
	capi_rsa_priv_dec,		/* priv_dec */
	0,				/* rsa_mod_exp */
	0,				/* bn_mod_exp */
	0,				/* init	*/
	capi_rsa_free,			/* finish */
	RSA_FLAG_SIGN_VER, 		/* flags */
	NULL,				/* app_data */
	capi_rsa_sign,			/* rsa_sign */
	0				/* rsa_verify */
	};

static DSA_METHOD capi_dsa_method =
	{
	"CryptoAPI DSA method",
	capi_dsa_do_sign,		/* dsa_do_sign */
	0,				/* dsa_sign_setup */
	0,				/* dsa_do_verify */
	0,				/* dsa_mod_exp */
	0,				/* bn_mod_exp */
	0,				/* init	*/
	capi_dsa_free,			/* finish */
	0, 				/* flags */
	NULL,				/* app_data */
	0,				/* dsa_paramgen */
	0				/* dsa_keygen */
	};

static int capi_init(ENGINE *e)
	{
	CAPI_CTX *ctx;
	const RSA_METHOD *ossl_rsa_meth;
	const DSA_METHOD *ossl_dsa_meth;

	if (capi_idx < 0)
		{
		capi_idx = ENGINE_get_ex_new_index(0, NULL, NULL, NULL, 0);
		if (capi_idx < 0)
			goto memerr;

		cert_capi_idx = X509_get_ex_new_index(0, NULL, NULL, NULL, 0);

		/* Setup RSA_METHOD */
		rsa_capi_idx = RSA_get_ex_new_index(0, NULL, NULL, NULL, 0);
		ossl_rsa_meth = RSA_PKCS1_SSLeay();
		capi_rsa_method.rsa_pub_enc = ossl_rsa_meth->rsa_pub_enc;
		capi_rsa_method.rsa_pub_dec = ossl_rsa_meth->rsa_pub_dec;
		capi_rsa_method.rsa_mod_exp = ossl_rsa_meth->rsa_mod_exp;
		capi_rsa_method.bn_mod_exp = ossl_rsa_meth->bn_mod_exp;

		/* Setup DSA Method */
		dsa_capi_idx = DSA_get_ex_new_index(0, NULL, NULL, NULL, 0);
		ossl_dsa_meth = DSA_OpenSSL();
		capi_dsa_method.dsa_do_verify = ossl_dsa_meth->dsa_do_verify;
		capi_dsa_method.dsa_mod_exp = ossl_dsa_meth->dsa_mod_exp;
		capi_dsa_method.bn_mod_exp = ossl_dsa_meth->bn_mod_exp;
		}

	ctx = capi_ctx_new();
	if (!ctx)
		goto memerr;

	ENGINE_set_ex_data(e, capi_idx, ctx);

#ifdef OPENSSL_CAPIENG_DIALOG
	{
	HMODULE cryptui = LoadLibrary(TEXT("CRYPTUI.DLL"));
	HMODULE kernel = GetModuleHandle(TEXT("KERNEL32.DLL"));
	if (cryptui)
		ctx->certselectdlg = (CERTDLG)GetProcAddress(cryptui, "CryptUIDlgSelectCertificateFromStore");
	if (kernel)
		ctx->getconswindow = (GETCONSWIN)GetProcAddress(kernel, "GetConsoleWindow");
	if (cryptui && !OPENSSL_isservice())
		ctx->client_cert_select = cert_select_dialog;
	}
#endif
		

	return 1;

	memerr:
	CAPIerr(CAPI_F_CAPI_INIT, ERR_R_MALLOC_FAILURE);
	return 0;

	return 1;
	}

static int capi_destroy(ENGINE *e)
	{
	ERR_unload_CAPI_strings();
	return 1;
	}

static int capi_finish(ENGINE *e)
	{
	CAPI_CTX *ctx;
	ctx = ENGINE_get_ex_data(e, capi_idx);
	capi_ctx_free(ctx);
	ENGINE_set_ex_data(e, capi_idx, NULL);
	return 1;
	}


/* CryptoAPI key application data. This contains
 * a handle to the private key container (for sign operations)
 * and a handle to the key (for decrypt operations).
 */

struct CAPI_KEY_st
	{
	/* Associated certificate context (if any) */
	PCCERT_CONTEXT pcert;
	HCRYPTPROV hprov;
	HCRYPTKEY key;
	DWORD keyspec;
	};

static int bind_capi(ENGINE *e)
	{
	if (!ENGINE_set_id(e, engine_capi_id)
		|| !ENGINE_set_name(e, engine_capi_name)
		|| !ENGINE_set_init_function(e, capi_init)
		|| !ENGINE_set_finish_function(e, capi_finish)
		|| !ENGINE_set_destroy_function(e, capi_destroy)
		|| !ENGINE_set_RSA(e, &capi_rsa_method)
		|| !ENGINE_set_DSA(e, &capi_dsa_method)
		|| !ENGINE_set_load_privkey_function(e, capi_load_privkey)
		|| !ENGINE_set_load_ssl_client_cert_function(e,
						capi_load_ssl_client_cert)
		|| !ENGINE_set_cmd_defns(e, capi_cmd_defns)
		|| !ENGINE_set_ctrl_function(e, capi_ctrl))
			return 0;
	ERR_load_CAPI_strings();

	return 1;

	}

#ifndef OPENSSL_NO_DYNAMIC_ENGINE
static int bind_helper(ENGINE *e, const char *id)
	{
	if(id && (strcmp(id, engine_capi_id) != 0))
		return 0;
	if(!bind_capi(e))
		return 0;
	return 1;
	}       
IMPLEMENT_DYNAMIC_CHECK_FN()
IMPLEMENT_DYNAMIC_BIND_FN(bind_helper)
#else
static ENGINE *engine_capi(void)
	{
	ENGINE *ret = ENGINE_new();
	if(!ret)
		return NULL;
	if(!bind_capi(ret))
		{
		ENGINE_free(ret);
		return NULL;
		}
	return ret;
	}

void ENGINE_load_capi(void)
	{
	/* Copied from eng_[openssl|dyn].c */
	ENGINE *toadd = engine_capi();
	if(!toadd) return;
	ENGINE_add(toadd);
	ENGINE_free(toadd);
	ERR_clear_error();
	}
#endif


static int lend_tobn(BIGNUM *bn, unsigned char *bin, int binlen)
	{
	int i;
	/* Reverse buffer in place: since this is a keyblob structure
	 * that will be freed up after conversion anyway it doesn't 
	 * matter if we change it.
	 */
	for(i = 0; i < binlen / 2; i++)
		{
		unsigned char c;
		c = bin[i];
		bin[i] = bin[binlen - i - 1];
		bin[binlen - i - 1] = c;
		}

	if (!BN_bin2bn(bin, binlen, bn))
		return 0;
	return 1;
	}

/* Given a CAPI_KEY get an EVP_PKEY structure */

static EVP_PKEY *capi_get_pkey(ENGINE *eng, CAPI_KEY *key)
	{
	unsigned char *pubkey = NULL;
	DWORD len;
	BLOBHEADER *bh;
	RSA *rkey = NULL;
	DSA *dkey = NULL;
	EVP_PKEY *ret = NULL;
	if (!CryptExportKey(key->key, 0, PUBLICKEYBLOB, 0, NULL, &len))
		{
		CAPIerr(CAPI_F_CAPI_GET_PKEY, CAPI_R_PUBKEY_EXPORT_LENGTH_ERROR);
		capi_addlasterror();
		return NULL;
		}

	pubkey = OPENSSL_malloc(len);

	if (!pubkey)
		goto memerr;

	if (!CryptExportKey(key->key, 0, PUBLICKEYBLOB, 0, pubkey, &len))
		{
		CAPIerr(CAPI_F_CAPI_GET_PKEY, CAPI_R_PUBKEY_EXPORT_ERROR);
		capi_addlasterror();
		goto err;
		}

	bh = (BLOBHEADER *)pubkey;
	if (bh->bType != PUBLICKEYBLOB)
		{
		CAPIerr(CAPI_F_CAPI_GET_PKEY, CAPI_R_INVALID_PUBLIC_KEY_BLOB);
		goto err;
		}
	if (bh->aiKeyAlg == CALG_RSA_SIGN || bh->aiKeyAlg == CALG_RSA_KEYX)
		{
		RSAPUBKEY *rp;
		DWORD rsa_modlen;
		unsigned char *rsa_modulus;
		rp = (RSAPUBKEY *)(bh + 1);
		if (rp->magic != 0x31415352)
			{
			char magstr[10];
			BIO_snprintf(magstr, 10, "%lx", rp->magic);
			CAPIerr(CAPI_F_CAPI_GET_PKEY, CAPI_R_INVALID_RSA_PUBLIC_KEY_BLOB_MAGIC_NUMBER);
			ERR_add_error_data(2, "magic=0x", magstr);
			goto err;
			}
		rsa_modulus = (unsigned char *)(rp + 1);
		rkey = RSA_new_method(eng);
		if (!rkey)
			goto memerr;

		rkey->e = BN_new();
		rkey->n = BN_new();

		if (!rkey->e || !rkey->n)
			goto memerr;

		if (!BN_set_word(rkey->e, rp->pubexp))
			goto memerr;

		rsa_modlen = rp->bitlen / 8;
		if (!lend_tobn(rkey->n, rsa_modulus, rsa_modlen))
			goto memerr;

		RSA_set_ex_data(rkey, rsa_capi_idx, key);

		if (!(ret = EVP_PKEY_new()))
			goto memerr;

		EVP_PKEY_assign_RSA(ret, rkey);
		rkey = NULL;

		}
	else if (bh->aiKeyAlg == CALG_DSS_SIGN)
		{
		DSSPUBKEY *dp;
		DWORD dsa_plen;
		unsigned char *btmp;
		dp = (DSSPUBKEY *)(bh + 1);
		if (dp->magic != 0x31535344)
			{
			char magstr[10];
			BIO_snprintf(magstr, 10, "%lx", dp->magic);
			CAPIerr(CAPI_F_CAPI_GET_PKEY, CAPI_R_INVALID_DSA_PUBLIC_KEY_BLOB_MAGIC_NUMBER);
			ERR_add_error_data(2, "magic=0x", magstr);
			goto err;
			}
		dsa_plen = dp->bitlen / 8;
		btmp = (unsigned char *)(dp + 1);
		dkey = DSA_new_method(eng);
		if (!dkey)
			goto memerr;
		dkey->p = BN_new();
		dkey->q = BN_new();
		dkey->g = BN_new();
		dkey->pub_key = BN_new();
		if (!dkey->p || !dkey->q || !dkey->g || !dkey->pub_key)
			goto memerr;
		if (!lend_tobn(dkey->p, btmp, dsa_plen))
			goto memerr;
		btmp += dsa_plen;
		if (!lend_tobn(dkey->q, btmp, 20))
			goto memerr;
		btmp += 20;
		if (!lend_tobn(dkey->g, btmp, dsa_plen))
			goto memerr;
		btmp += dsa_plen;
		if (!lend_tobn(dkey->pub_key, btmp, dsa_plen))
			goto memerr;
		btmp += dsa_plen;

		DSA_set_ex_data(dkey, dsa_capi_idx, key);

		if (!(ret = EVP_PKEY_new()))
			goto memerr;

		EVP_PKEY_assign_DSA(ret, dkey);
		dkey = NULL;
		}
	else
		{
		char algstr[10];
		BIO_snprintf(algstr, 10, "%lx", bh->aiKeyAlg);
		CAPIerr(CAPI_F_CAPI_GET_PKEY, CAPI_R_UNSUPPORTED_PUBLIC_KEY_ALGORITHM);
		ERR_add_error_data(2, "aiKeyAlg=0x", algstr);
		goto err;
		}


	err:
	if (pubkey)
		OPENSSL_free(pubkey);
	if (!ret)
		{
		if (rkey)
			RSA_free(rkey);
		if (dkey)
			DSA_free(dkey);
		}

	return ret;

memerr:
	CAPIerr(CAPI_F_CAPI_GET_PKEY, ERR_R_MALLOC_FAILURE);
	goto err;

	}

static EVP_PKEY *capi_load_privkey(ENGINE *eng, const char *key_id,
	UI_METHOD *ui_method, void *callback_data)
	{
	CAPI_CTX *ctx;
	CAPI_KEY *key;
	EVP_PKEY *ret;
	ctx = ENGINE_get_ex_data(eng, capi_idx);

	if (!ctx)
		{
		CAPIerr(CAPI_F_CAPI_LOAD_PRIVKEY, CAPI_R_CANT_FIND_CAPI_CONTEXT);
		return NULL;
		}

	key = capi_find_key(ctx, key_id);

	if (!key)
		return NULL;

	ret = capi_get_pkey(eng, key);

	if (!ret)
		capi_free_key(key);
	return ret;

	}

/* CryptoAPI RSA operations */

int capi_rsa_priv_enc(int flen, const unsigned char *from,
                unsigned char *to, RSA *rsa, int padding)
	{
	CAPIerr(CAPI_F_CAPI_RSA_PRIV_ENC, CAPI_R_FUNCTION_NOT_SUPPORTED);
	return -1;
	}

int capi_rsa_sign(int dtype, const unsigned char *m, unsigned int m_len,
             unsigned char *sigret, unsigned int *siglen, const RSA *rsa)
	{
	ALG_ID alg;
	HCRYPTHASH hash;
	DWORD slen;
	unsigned int i;
	int ret = -1;
	CAPI_KEY *capi_key;
	CAPI_CTX *ctx;

	ctx = ENGINE_get_ex_data(rsa->engine, capi_idx);

	CAPI_trace(ctx, "Called CAPI_rsa_sign()\n");

	capi_key = RSA_get_ex_data(rsa, rsa_capi_idx);
	if (!capi_key)
		{
		CAPIerr(CAPI_F_CAPI_RSA_SIGN, CAPI_R_CANT_GET_KEY);
		return -1;
		}
/* Convert the signature type to a CryptoAPI algorithm ID */
	switch(dtype)
		{
	case NID_sha1:
		alg = CALG_SHA1;
		break;

	case NID_md5:
		alg = CALG_MD5;
		break;

	case NID_md5_sha1:
		alg = CALG_SSL3_SHAMD5;
		break;
	default:
		{
		char algstr[10];
		BIO_snprintf(algstr, 10, "%lx", dtype);
		CAPIerr(CAPI_F_CAPI_RSA_SIGN, CAPI_R_UNSUPPORTED_ALGORITHM_NID);
		ERR_add_error_data(2, "NID=0x", algstr);
		return -1;
		}
	}



/* Create the hash object */
	if(!CryptCreateHash(capi_key->hprov, alg, 0, 0, &hash))
		{
		CAPIerr(CAPI_F_CAPI_RSA_SIGN, CAPI_R_CANT_CREATE_HASH_OBJECT);
		capi_addlasterror();
		return -1;
		}
/* Set the hash value to the value passed */

	if(!CryptSetHashParam(hash, HP_HASHVAL, (unsigned char *)m, 0))
		{
		CAPIerr(CAPI_F_CAPI_RSA_SIGN, CAPI_R_CANT_SET_HASH_VALUE);
		capi_addlasterror();
		goto err;
		}


/* Finally sign it */
	slen = RSA_size(rsa);
	if(!CryptSignHashA(hash, capi_key->keyspec, NULL, 0, sigret, &slen))
		{
		CAPIerr(CAPI_F_CAPI_RSA_SIGN, CAPI_R_ERROR_SIGNING_HASH);
		capi_addlasterror();
		goto err;
		}
	else
		{
		ret = 1;
		/* Inplace byte reversal of signature */
		for(i = 0; i < slen / 2; i++)
			{
			unsigned char c;
			c = sigret[i];
			sigret[i] = sigret[slen - i - 1];
			sigret[slen - i - 1] = c;
			}
		*siglen = slen;
		}

	/* Now cleanup */

err:
	CryptDestroyHash(hash);

	return ret;
	}

int capi_rsa_priv_dec(int flen, const unsigned char *from,
                unsigned char *to, RSA *rsa, int padding)
	{
	int i;
	unsigned char *tmpbuf;
	CAPI_KEY *capi_key;
	CAPI_CTX *ctx;
	ctx = ENGINE_get_ex_data(rsa->engine, capi_idx);

	CAPI_trace(ctx, "Called capi_rsa_priv_dec()\n");


	capi_key = RSA_get_ex_data(rsa, rsa_capi_idx);
	if (!capi_key)
		{
		CAPIerr(CAPI_F_CAPI_RSA_PRIV_DEC, CAPI_R_CANT_GET_KEY);
		return -1;
		}

	if(padding != RSA_PKCS1_PADDING)
		{
		char errstr[10];
		BIO_snprintf(errstr, 10, "%d", padding);
		CAPIerr(CAPI_F_CAPI_RSA_PRIV_DEC, CAPI_R_UNSUPPORTED_PADDING);
		ERR_add_error_data(2, "padding=", errstr);
		return -1;
		}

	/* Create temp reverse order version of input */
	if(!(tmpbuf = OPENSSL_malloc(flen)) ) 
		{
		CAPIerr(CAPI_F_CAPI_RSA_PRIV_DEC, ERR_R_MALLOC_FAILURE);
		return -1;
		}
	for(i = 0; i < flen; i++)
		tmpbuf[flen - i - 1] = from[i];
	
	/* Finally decrypt it */
	if(!CryptDecrypt(capi_key->key, 0, TRUE, 0, tmpbuf, &flen))
		{
		CAPIerr(CAPI_F_CAPI_RSA_PRIV_DEC, CAPI_R_DECRYPT_ERROR);
		capi_addlasterror();
		OPENSSL_free(tmpbuf);
		return -1;
		} 
	else memcpy(to, tmpbuf, flen);

	OPENSSL_free(tmpbuf);

	return flen;
	}

static int capi_rsa_free(RSA *rsa)
	{
	CAPI_KEY *capi_key;
	capi_key = RSA_get_ex_data(rsa, rsa_capi_idx);
	capi_free_key(capi_key);
	RSA_set_ex_data(rsa, rsa_capi_idx, 0);
	return 1;
	}

/* CryptoAPI DSA operations */

static DSA_SIG *capi_dsa_do_sign(const unsigned char *digest, int dlen,
								DSA *dsa)
	{
	HCRYPTHASH hash;
	DWORD slen;
	DSA_SIG *ret = NULL;
	CAPI_KEY *capi_key;
	CAPI_CTX *ctx;
	unsigned char csigbuf[40];

	ctx = ENGINE_get_ex_data(dsa->engine, capi_idx);

	CAPI_trace(ctx, "Called CAPI_dsa_do_sign()\n");

	capi_key = DSA_get_ex_data(dsa, dsa_capi_idx);

	if (!capi_key)
		{
		CAPIerr(CAPI_F_CAPI_DSA_DO_SIGN, CAPI_R_CANT_GET_KEY);
		return NULL;
		}

	if (dlen != 20)
		{
		CAPIerr(CAPI_F_CAPI_DSA_DO_SIGN, CAPI_R_INVALID_DIGEST_LENGTH);
		return NULL;
		}

	/* Create the hash object */
	if(!CryptCreateHash(capi_key->hprov, CALG_SHA1, 0, 0, &hash))
		{
		CAPIerr(CAPI_F_CAPI_DSA_DO_SIGN, CAPI_R_CANT_CREATE_HASH_OBJECT);
		capi_addlasterror();
		return NULL;
		}

	/* Set the hash value to the value passed */
	if(!CryptSetHashParam(hash, HP_HASHVAL, (unsigned char *)digest, 0))
		{
		CAPIerr(CAPI_F_CAPI_DSA_DO_SIGN, CAPI_R_CANT_SET_HASH_VALUE);
		capi_addlasterror();
		goto err;
		}


	/* Finally sign it */
	slen = sizeof(csigbuf);
	if(!CryptSignHashA(hash, capi_key->keyspec, NULL, 0, csigbuf, &slen))
		{
		CAPIerr(CAPI_F_CAPI_DSA_DO_SIGN, CAPI_R_ERROR_SIGNING_HASH);
		capi_addlasterror();
		goto err;
		}
	else
		{
		ret = DSA_SIG_new();
		if (!ret)
			goto err;
		ret->r = BN_new();
		ret->s = BN_new();
		if (!ret->r || !ret->s)
			goto err;
		if (!lend_tobn(ret->r, csigbuf, 20)
			|| !lend_tobn(ret->s, csigbuf + 20, 20))
			{
			DSA_SIG_free(ret);
			ret = NULL;
			goto err;
			}
		}

	/* Now cleanup */

err:
	OPENSSL_cleanse(csigbuf, 40);
	CryptDestroyHash(hash);
	return ret;
	}

static int capi_dsa_free(DSA *dsa)
	{
	CAPI_KEY *capi_key;
	capi_key = DSA_get_ex_data(dsa, dsa_capi_idx);
	capi_free_key(capi_key);
	DSA_set_ex_data(dsa, dsa_capi_idx, 0);
	return 1;
	}

static void capi_vtrace(CAPI_CTX *ctx, int level, char *format, va_list argptr)
	{
	BIO *out;

	if (!ctx || (ctx->debug_level < level) || (!ctx->debug_file))
		return;
	out = BIO_new_file(ctx->debug_file, "a+");
	BIO_vprintf(out, format, argptr);
	BIO_free(out);
	}

static void CAPI_trace(CAPI_CTX *ctx, char *format, ...)
	{
	va_list args;
	va_start(args, format);
	capi_vtrace(ctx, CAPI_DBG_TRACE, format, args);
	va_end(args);
	}

static void capi_addlasterror(void)
	{
	capi_adderror(GetLastError());
	}

static void capi_adderror(DWORD err)
	{
	char errstr[10];
	BIO_snprintf(errstr, 10, "%lX", err);
	ERR_add_error_data(2, "Error code= 0x", errstr);
	}

static char *wide_to_asc(LPWSTR wstr)
	{
	char *str;
	int len_0,sz;

	if (!wstr)
		return NULL;
	len_0 = (int)wcslen(wstr)+1;	/* WideCharToMultiByte expects int */
        sz = WideCharToMultiByte(CP_ACP,0,wstr,len_0,NULL,0,NULL,NULL);
	if (!sz)
		{
		CAPIerr(CAPI_F_WIDE_TO_ASC, CAPI_R_WIN32_ERROR);
		return NULL;
		}
	str = OPENSSL_malloc(sz);
	if (!str)
		{
		CAPIerr(CAPI_F_WIDE_TO_ASC, ERR_R_MALLOC_FAILURE);
		return NULL;
		}
	if (!WideCharToMultiByte(CP_ACP,0,wstr,len_0,str,sz,NULL,NULL))
		{
		OPENSSL_free(str);
		CAPIerr(CAPI_F_WIDE_TO_ASC, CAPI_R_WIN32_ERROR);
		return NULL;
		}
	return str;
	}

static int capi_get_provname(CAPI_CTX *ctx, LPSTR *pname, DWORD *ptype, DWORD idx)
	{
	LPSTR name;
	DWORD len, err;
	CAPI_trace(ctx, "capi_get_provname, index=%d\n", idx);
	if (!CryptEnumProvidersA(idx, NULL, 0, ptype, NULL, &len))
		{
		err = GetLastError();
		if (err == ERROR_NO_MORE_ITEMS)
			return 2;
		CAPIerr(CAPI_F_CAPI_GET_PROVNAME, CAPI_R_CRYPTENUMPROVIDERS_ERROR);
		capi_adderror(err);
		return 0;
		}
	name = OPENSSL_malloc(len);
	if (!CryptEnumProvidersA(idx, NULL, 0, ptype, name, &len))
		{
		err = GetLastError();
		if (err == ERROR_NO_MORE_ITEMS)
			return 2;
		CAPIerr(CAPI_F_CAPI_GET_PROVNAME, CAPI_R_CRYPTENUMPROVIDERS_ERROR);
		capi_adderror(err);
		return 0;
		}
	*pname = name;
	CAPI_trace(ctx, "capi_get_provname, returned name=%s, type=%d\n", name, *ptype);

	return 1;
	}

static int capi_list_providers(CAPI_CTX *ctx, BIO *out)
	{
	DWORD idx, ptype;
	int ret;
	LPSTR provname = NULL;
	CAPI_trace(ctx, "capi_list_providers\n");
	BIO_printf(out, "Available CSPs:\n");
	for(idx = 0; ; idx++)
		{
		ret = capi_get_provname(ctx, &provname, &ptype, idx);
		if (ret == 2)
			break;
		if (ret == 0)
			break;
		BIO_printf(out, "%d. %s, type %d\n", idx, provname, ptype);
		OPENSSL_free(provname);
		}
	return 1;
	}

static int capi_list_containers(CAPI_CTX *ctx, BIO *out)
	{
	int ret = 1;
	HCRYPTPROV hprov;
	DWORD err, idx, flags, buflen = 0, clen;
	LPSTR cname;
	CAPI_trace(ctx, "Listing containers CSP=%s, type = %d\n", ctx->cspname, ctx->csptype);
	if (!CryptAcquireContextA(&hprov, NULL, ctx->cspname, ctx->csptype, CRYPT_VERIFYCONTEXT))
		{
		CAPIerr(CAPI_F_CAPI_LIST_CONTAINERS, CAPI_R_CRYPTACQUIRECONTEXT_ERROR);
		capi_addlasterror();
		return 0;
		}
	if (!CryptGetProvParam(hprov, PP_ENUMCONTAINERS, NULL, &buflen, CRYPT_FIRST))
		{
		CAPIerr(CAPI_F_CAPI_LIST_CONTAINERS, CAPI_R_ENUMCONTAINERS_ERROR);
		capi_addlasterror();
		CryptReleaseContext(hprov, 0);
		return 0;
		}
	CAPI_trace(ctx, "Got max container len %d\n", buflen);
	if (buflen == 0)
		buflen = 1024;
	cname = OPENSSL_malloc(buflen);
	if (!cname)
		{
		CAPIerr(CAPI_F_CAPI_LIST_CONTAINERS, ERR_R_MALLOC_FAILURE);
		goto err;
		}

	for (idx = 0;;idx++)
		{
		clen = buflen;
		cname[0] = 0;

		if (idx == 0)
			flags = CRYPT_FIRST;
		else
			flags = 0;
		if(!CryptGetProvParam(hprov, PP_ENUMCONTAINERS, cname, &clen, flags))
			{
			err = GetLastError();
			if (err == ERROR_NO_MORE_ITEMS)
				goto done;
			CAPIerr(CAPI_F_CAPI_LIST_CONTAINERS, CAPI_R_ENUMCONTAINERS_ERROR);
			capi_adderror(err);
			goto err;
			}
		CAPI_trace(ctx, "Container name %s, len=%d, index=%d, flags=%d\n", cname, clen, idx, flags);
		if (!cname[0] && (clen == buflen))
			{
			CAPI_trace(ctx, "Enumerate bug: using workaround\n");
			goto done;
			}
		BIO_printf(out, "%d. %s\n", idx, cname);
		}
	err:

	ret = 0;

	done:
	if (cname)
		OPENSSL_free(cname);
	CryptReleaseContext(hprov, 0);

	return ret;
	}

CRYPT_KEY_PROV_INFO *capi_get_prov_info(CAPI_CTX *ctx, PCCERT_CONTEXT cert)
	{
	DWORD len;
	CRYPT_KEY_PROV_INFO *pinfo;
	
	if(!CertGetCertificateContextProperty(cert, CERT_KEY_PROV_INFO_PROP_ID, NULL, &len))
		return NULL;
	pinfo = OPENSSL_malloc(len);
	if (!pinfo)
		{
		CAPIerr(CAPI_F_CAPI_GET_PROV_INFO, ERR_R_MALLOC_FAILURE);
		return NULL;
		}
	if(!CertGetCertificateContextProperty(cert, CERT_KEY_PROV_INFO_PROP_ID, pinfo, &len))
		{
		CAPIerr(CAPI_F_CAPI_GET_PROV_INFO, CAPI_R_ERROR_GETTING_KEY_PROVIDER_INFO);
		capi_addlasterror();
		OPENSSL_free(pinfo);
		return NULL;
		}
	return pinfo;
	}

static void capi_dump_prov_info(CAPI_CTX *ctx, BIO *out, CRYPT_KEY_PROV_INFO *pinfo)
	{
	char *provname = NULL, *contname = NULL;
	if (!pinfo)
		{
		BIO_printf(out, "  No Private Key\n");
		return;
		}
	provname = wide_to_asc(pinfo->pwszProvName);
	contname = wide_to_asc(pinfo->pwszContainerName);
	if (!provname || !contname)
		goto err;

	BIO_printf(out, "  Private Key Info:\n");
	BIO_printf(out, "    Provider Name:  %s, Provider Type %d\n", provname, pinfo->dwProvType);
	BIO_printf(out, "    Container Name: %s, Key Type %d\n", contname, pinfo->dwKeySpec);
	err:
	if (provname)
		OPENSSL_free(provname);
	if (contname)
		OPENSSL_free(contname);
	}

char * capi_cert_get_fname(CAPI_CTX *ctx, PCCERT_CONTEXT cert)
	{
	LPWSTR wfname;
	DWORD dlen;

	CAPI_trace(ctx, "capi_cert_get_fname\n");
	if (!CertGetCertificateContextProperty(cert, CERT_FRIENDLY_NAME_PROP_ID, NULL, &dlen))
		return NULL;
	wfname = OPENSSL_malloc(dlen);
	if (CertGetCertificateContextProperty(cert, CERT_FRIENDLY_NAME_PROP_ID, wfname, &dlen))
		{
		char *fname = wide_to_asc(wfname);
		OPENSSL_free(wfname);
		return fname;
		}
	CAPIerr(CAPI_F_CAPI_CERT_GET_FNAME, CAPI_R_ERROR_GETTING_FRIENDLY_NAME);
	capi_addlasterror();

	OPENSSL_free(wfname);
	return NULL;
	}


void capi_dump_cert(CAPI_CTX *ctx, BIO *out, PCCERT_CONTEXT cert)
	{
	X509 *x;
	unsigned char *p;
	unsigned long flags = ctx->dump_flags;
	if (flags & CAPI_DMP_FNAME)
		{
		char *fname;
		fname = capi_cert_get_fname(ctx, cert);
		if (fname)
			{
			BIO_printf(out, "  Friendly Name \"%s\"\n", fname);
			OPENSSL_free(fname);
			}
		else
			BIO_printf(out, "  <No Friendly Name>\n");
		}

	p = cert->pbCertEncoded;
	x = d2i_X509(NULL, &p, cert->cbCertEncoded);
	if (!x)
		BIO_printf(out, "  <Can't parse certificate>\n");
	if (flags & CAPI_DMP_SUMMARY)
		{
		BIO_printf(out, "  Subject: ");
		X509_NAME_print_ex(out, X509_get_subject_name(x), 0, XN_FLAG_ONELINE);
		BIO_printf(out, "\n  Issuer: ");
		X509_NAME_print_ex(out, X509_get_issuer_name(x), 0, XN_FLAG_ONELINE);
		BIO_printf(out, "\n");
		}
	if (flags & CAPI_DMP_FULL)
		X509_print_ex(out, x, XN_FLAG_ONELINE,0);

	if (flags & CAPI_DMP_PKEYINFO)
		{
		CRYPT_KEY_PROV_INFO *pinfo;
		pinfo = capi_get_prov_info(ctx, cert);
		capi_dump_prov_info(ctx, out, pinfo);
		if (pinfo)
			OPENSSL_free(pinfo);
		}

	if (flags & CAPI_DMP_PEM)
		PEM_write_bio_X509(out, x);
	X509_free(x);
	}

HCERTSTORE capi_open_store(CAPI_CTX *ctx, char *storename)
	{
	HCERTSTORE hstore;

	if (!storename)
		storename = ctx->storename;
	if (!storename)
		storename = "MY";
	CAPI_trace(ctx, "Opening certificate store %s\n", storename);

	hstore = CertOpenStore(CERT_STORE_PROV_SYSTEM_A, 0, 0, 
				ctx->store_flags, storename);
	if (!hstore)
		{
		CAPIerr(CAPI_F_CAPI_OPEN_STORE, CAPI_R_ERROR_OPENING_STORE);
		capi_addlasterror();
		}
	return hstore;
	}

int capi_list_certs(CAPI_CTX *ctx, BIO *out, char *id)
	{
	char *storename;
	int idx;
	int ret = 1;
	HCERTSTORE hstore;
	PCCERT_CONTEXT cert = NULL;

	storename = ctx->storename;
	if (!storename)
		storename = "MY";
	CAPI_trace(ctx, "Listing certs for store %s\n", storename);

	hstore = capi_open_store(ctx, storename);
	if (!hstore)
		return 0;
	if (id)
		{
		cert = capi_find_cert(ctx, id, hstore);
		if (!cert)
			{
			ret = 0;
			goto err;
			}
		capi_dump_cert(ctx, out, cert);
		CertFreeCertificateContext(cert);
		}
	else
		{
		for(idx = 0;;idx++)
			{
			LPWSTR fname = NULL;
			cert = CertEnumCertificatesInStore(hstore, cert);
			if (!cert)
				break;
			BIO_printf(out, "Certificate %d\n", idx);
			capi_dump_cert(ctx, out, cert);
			}
		}
	err:
	CertCloseStore(hstore, 0);
	return ret;
	}

static PCCERT_CONTEXT capi_find_cert(CAPI_CTX *ctx, const char *id, HCERTSTORE hstore)
	{
	PCCERT_CONTEXT cert = NULL;
	char *fname = NULL;
	int match;
	switch(ctx->lookup_method)
		{
		case CAPI_LU_SUBSTR:
			return CertFindCertificateInStore(hstore,
					X509_ASN_ENCODING, 0,
					CERT_FIND_SUBJECT_STR_A, id, NULL);
		case CAPI_LU_FNAME:
			for(;;)
				{
				cert = CertEnumCertificatesInStore(hstore, cert);
				if (!cert)
					return NULL;
				fname = capi_cert_get_fname(ctx, cert);
				if (fname)
					{
					if (strcmp(fname, id))
						match = 0;
					else
						match = 1;
					OPENSSL_free(fname);
					if (match)
						return cert;
					}
				}
		default:
			return NULL;
		}
	}

static CAPI_KEY *capi_get_key(CAPI_CTX *ctx, const char *contname, char *provname, DWORD ptype, DWORD keyspec)
	{
	CAPI_KEY *key;
    DWORD dwFlags = 0; 
	key = OPENSSL_malloc(sizeof(CAPI_KEY));
	CAPI_trace(ctx, "capi_get_key, contname=%s, provname=%s, type=%d\n", 
						contname, provname, ptype);
    if(ctx->store_flags & CERT_SYSTEM_STORE_LOCAL_MACHINE)
        dwFlags = CRYPT_MACHINE_KEYSET;
    if (!CryptAcquireContextA(&key->hprov, contname, provname, ptype, dwFlags)) 
		{
		CAPIerr(CAPI_F_CAPI_GET_KEY, CAPI_R_CRYPTACQUIRECONTEXT_ERROR);
		capi_addlasterror();
		goto err;
		}
	if (!CryptGetUserKey(key->hprov, keyspec, &key->key))
		{
		CAPIerr(CAPI_F_CAPI_GET_KEY, CAPI_R_GETUSERKEY_ERROR);
		capi_addlasterror();
		CryptReleaseContext(key->hprov, 0);
		goto err;
		}
	key->keyspec = keyspec;
	key->pcert = NULL;
	return key;

	err:
	OPENSSL_free(key);
	return NULL;
	}

static CAPI_KEY *capi_get_cert_key(CAPI_CTX *ctx, PCCERT_CONTEXT cert)
	{
	CAPI_KEY *key = NULL;
	CRYPT_KEY_PROV_INFO *pinfo = NULL;
	char *provname = NULL, *contname = NULL;
	pinfo = capi_get_prov_info(ctx, cert);
	if (!pinfo)
		goto err;
	provname = wide_to_asc(pinfo->pwszProvName);
	contname = wide_to_asc(pinfo->pwszContainerName);
	if (!provname || !contname)
		goto err;
	key = capi_get_key(ctx, contname, provname,
				pinfo->dwProvType, pinfo->dwKeySpec);

	err:
	if (pinfo)
		OPENSSL_free(pinfo);
	if (provname)
		OPENSSL_free(provname);
	if (contname)
		OPENSSL_free(contname);
	return key;
	}

CAPI_KEY *capi_find_key(CAPI_CTX *ctx, const char *id)
	{
	PCCERT_CONTEXT cert;
	HCERTSTORE hstore;
	CAPI_KEY *key = NULL;
	switch (ctx->lookup_method)
		{
		case CAPI_LU_SUBSTR:
		case CAPI_LU_FNAME:
		hstore = capi_open_store(ctx, NULL);
		if (!hstore)
			return NULL;
		cert = capi_find_cert(ctx, id, hstore);
		if (cert)
			{
			key = capi_get_cert_key(ctx, cert);
			CertFreeCertificateContext(cert);
			}
		CertCloseStore(hstore, 0);
		break;

		case CAPI_LU_CONTNAME:
		key = capi_get_key(ctx, id, ctx->cspname, ctx->csptype,
							ctx->keytype);
		break;
		}

	return key;
	}

void capi_free_key(CAPI_KEY *key)
	{
	if (!key)
		return;
	CryptDestroyKey(key->key);
	CryptReleaseContext(key->hprov, 0);
	if (key->pcert)
		CertFreeCertificateContext(key->pcert);
	OPENSSL_free(key);
	}


/* Initialize a CAPI_CTX structure */

static CAPI_CTX *capi_ctx_new()
	{
	CAPI_CTX *ctx;
	ctx = OPENSSL_malloc(sizeof(CAPI_CTX));
	if (!ctx)
		{
		CAPIerr(CAPI_F_CAPI_CTX_NEW, ERR_R_MALLOC_FAILURE);
		return NULL;
		}
	ctx->cspname = NULL;
	ctx->csptype = PROV_RSA_FULL;
	ctx->dump_flags = CAPI_DMP_SUMMARY|CAPI_DMP_FNAME;
	ctx->keytype = AT_KEYEXCHANGE;
	ctx->storename = NULL;
	ctx->ssl_client_store = NULL;
	ctx->store_flags = CERT_STORE_OPEN_EXISTING_FLAG |
				CERT_STORE_READONLY_FLAG |
				CERT_SYSTEM_STORE_CURRENT_USER;
	ctx->lookup_method = CAPI_LU_SUBSTR;
	ctx->debug_level = 0;
	ctx->debug_file = NULL;
	ctx->client_cert_select = cert_select_simple;
	return ctx;
	}

static void capi_ctx_free(CAPI_CTX *ctx)
	{
	CAPI_trace(ctx, "Calling capi_ctx_free with %lx\n", ctx);
	if (!ctx)
		return;
	if (ctx->cspname)
		OPENSSL_free(ctx->cspname);
	if (ctx->debug_file)
		OPENSSL_free(ctx->debug_file);
	if (ctx->storename)
		OPENSSL_free(ctx->storename);
	if (ctx->ssl_client_store)
		OPENSSL_free(ctx->ssl_client_store);
	OPENSSL_free(ctx);
	}

static int capi_ctx_set_provname(CAPI_CTX *ctx, LPSTR pname, DWORD type, int check)
	{
	CAPI_trace(ctx, "capi_ctx_set_provname, name=%s, type=%d\n", pname, type);
	if (check)
		{
		HCRYPTPROV hprov;
		if (!CryptAcquireContextA(&hprov, NULL, pname, type,
						CRYPT_VERIFYCONTEXT))
			{
			CAPIerr(CAPI_F_CAPI_CTX_SET_PROVNAME, CAPI_R_CRYPTACQUIRECONTEXT_ERROR);
			capi_addlasterror();
			return 0;
			}
		CryptReleaseContext(hprov, 0);
		}
	if (ctx->cspname)
		OPENSSL_free(ctx->cspname);
	ctx->cspname = BUF_strdup(pname);
	ctx->csptype = type;
	return 1;
	}

static int capi_ctx_set_provname_idx(CAPI_CTX *ctx, int idx)
	{
	LPSTR pname;
	DWORD type;
	int res;
	if (capi_get_provname(ctx, &pname, &type, idx) != 1)
		return 0;
	res = capi_ctx_set_provname(ctx, pname, type, 0);
	OPENSSL_free(pname);
	return res;
	}

static int cert_issuer_match(STACK_OF(X509_NAME) *ca_dn, X509 *x)
	{
	int i;
	X509_NAME *nm;
	/* Special case: empty list: match anything */
	if (sk_X509_NAME_num(ca_dn) <= 0)
		return 1;
	for (i = 0; i < sk_X509_NAME_num(ca_dn); i++)
		{
		nm = sk_X509_NAME_value(ca_dn, i);
		if (!X509_NAME_cmp(nm, X509_get_issuer_name(x)))
				return 1;
		}
	return 0;
	}



static int capi_load_ssl_client_cert(ENGINE *e, SSL *ssl,
	STACK_OF(X509_NAME) *ca_dn, X509 **pcert, EVP_PKEY **pkey,
	STACK_OF(X509) **pother, UI_METHOD *ui_method, void *callback_data)
	{
	STACK_OF(X509) *certs = NULL;
	X509 *x;
	char *storename;
	const char *p;
	int i, client_cert_idx;
	HCERTSTORE hstore;
	PCCERT_CONTEXT cert = NULL, excert = NULL;
	CAPI_CTX *ctx;
	CAPI_KEY *key;
	ctx = ENGINE_get_ex_data(e, capi_idx);

	*pcert = NULL;
	*pkey = NULL;

	storename = ctx->ssl_client_store;
	if (!storename)
		storename = "MY";

	hstore = capi_open_store(ctx, storename);
	if (!hstore)
		return 0;
	/* Enumerate all certificates collect any matches */
	for(i = 0;;i++)
		{
		cert = CertEnumCertificatesInStore(hstore, cert);
		if (!cert)
			break;
		p = cert->pbCertEncoded;
		x = d2i_X509(NULL, &p, cert->cbCertEncoded);
		if (!x)
			{
			CAPI_trace(ctx, "Can't Parse Certificate %d\n", i);
			continue;
			}
		if (cert_issuer_match(ca_dn, x)
			&& X509_check_purpose(x, X509_PURPOSE_SSL_CLIENT, 0))
			{
			key = capi_get_cert_key(ctx, cert);
			if (!key)
				{
				X509_free(x);
				continue;
				}
			/* Match found: attach extra data to it so
			 * we can retrieve the key later.
			 */
			excert = CertDuplicateCertificateContext(cert);
			key->pcert = excert;
			X509_set_ex_data(x, cert_capi_idx, key);

			if (!certs)
				certs = sk_X509_new_null();

			sk_X509_push(certs, x);
			}
		else
			X509_free(x);

		}

	if (cert)
		CertFreeCertificateContext(cert);
	if (hstore)
		CertCloseStore(hstore, 0);

	if (!certs)
		return 0;


	/* Select the appropriate certificate */

	client_cert_idx = ctx->client_cert_select(e, ssl, certs);

	/* Set the selected certificate and free the rest */

	for(i = 0; i < sk_X509_num(certs); i++)
		{
		x = sk_X509_value(certs, i);
		if (i == client_cert_idx)
			*pcert = x;
		else
			{
			key = X509_get_ex_data(x, cert_capi_idx);
			capi_free_key(key);
			X509_free(x);
			}
		}

	sk_X509_free(certs);

	if (!*pcert)
		return 0;

	/* Setup key for selected certificate */

	key = X509_get_ex_data(*pcert, cert_capi_idx);
	*pkey = capi_get_pkey(e, key);
	X509_set_ex_data(*pcert, cert_capi_idx, NULL);

	return 1;

	}


/* Simple client cert selection function: always select first */

static int cert_select_simple(ENGINE *e, SSL *ssl, STACK_OF(X509) *certs)
	{
	return 0;
	}

#ifdef OPENSSL_CAPIENG_DIALOG

/* More complex cert selection function, using standard function
 * CryptUIDlgSelectCertificateFromStore() to produce a dialog box.
 */

/* Definitions which are in cryptuiapi.h but this is not present in older
 * versions of headers.
 */

#ifndef CRYPTUI_SELECT_LOCATION_COLUMN
#define CRYPTUI_SELECT_LOCATION_COLUMN                   0x000000010
#define CRYPTUI_SELECT_INTENDEDUSE_COLUMN                0x000000004
#endif

#define dlg_title L"OpenSSL Application SSL Client Certificate Selection"
#define dlg_prompt L"Select a certificate to use for authentication"
#define dlg_columns	 CRYPTUI_SELECT_LOCATION_COLUMN \
			|CRYPTUI_SELECT_INTENDEDUSE_COLUMN

static int cert_select_dialog(ENGINE *e, SSL *ssl, STACK_OF(X509) *certs)
	{
	X509 *x;
	HCERTSTORE dstore;
	PCCERT_CONTEXT cert;
	CAPI_CTX *ctx;
	CAPI_KEY *key;
	HWND hwnd;
	int i, idx = -1;
	if (sk_X509_num(certs) == 1)
		return 0;
	ctx = ENGINE_get_ex_data(e, capi_idx);
	/* Create an in memory store of certificates */
	dstore = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0,
					CERT_STORE_CREATE_NEW_FLAG, NULL);
	if (!dstore)
		{
		CAPIerr(CAPI_F_CERT_SELECT_DIALOG, CAPI_R_ERROR_CREATING_STORE);
		capi_addlasterror();
		goto err;
		}
	/* Add all certificates to store */
	for(i = 0; i < sk_X509_num(certs); i++)
		{
		x = sk_X509_value(certs, i);
		key = X509_get_ex_data(x, cert_capi_idx);

		if (!CertAddCertificateContextToStore(dstore, key->pcert,
						CERT_STORE_ADD_NEW, NULL))
			{
			CAPIerr(CAPI_F_CERT_SELECT_DIALOG, CAPI_R_ERROR_ADDING_CERT);
			capi_addlasterror();
			goto err;
			}

		}
	hwnd = GetForegroundWindow();
	if (!hwnd)
		hwnd = GetActiveWindow();
	if (!hwnd && ctx->getconswindow)
		hwnd = ctx->getconswindow();
	/* Call dialog to select one */
	cert = ctx->certselectdlg(dstore, hwnd, dlg_title, dlg_prompt,
						dlg_columns, 0, NULL);

	/* Find matching cert from list */
	if (cert)
		{
		for(i = 0; i < sk_X509_num(certs); i++)
			{
			x = sk_X509_value(certs, i);
			key = X509_get_ex_data(x, cert_capi_idx);
			if (CertCompareCertificate(
				X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
					cert->pCertInfo,
					key->pcert->pCertInfo))
				{
				idx = i;
				break;
				}
			}
		}

	err:
	if (dstore)
		CertCloseStore(dstore, 0);
	return idx;

	}
#endif

#else /* !__COMPILE_CAPIENG */
#include <openssl/engine.h>
#ifndef OPENSSL_NO_DYNAMIC_ENGINE
OPENSSL_EXPORT
int bind_engine(ENGINE *e, const char *id, const dynamic_fns *fns);
OPENSSL_EXPORT
int bind_engine(ENGINE *e, const char *id, const dynamic_fns *fns) { return 0; }
IMPLEMENT_DYNAMIC_CHECK_FN()
#else
void ENGINE_load_capi(void){}
#endif
#endif
