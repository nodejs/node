/* crypto/engine/hw_cswift.c */
/* Written by Geoff Thorpe (geoff@geoffthorpe.net) for the OpenSSL
 * project 2000.
 */
/* ====================================================================
 * Copyright (c) 1999-2001 The OpenSSL Project.  All rights reserved.
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

#include <stdio.h>
#include <string.h>
#include <openssl/crypto.h>
#include <openssl/buffer.h>
#include <openssl/dso.h>
#include <openssl/engine.h>
#ifndef OPENSSL_NO_RSA
#include <openssl/rsa.h>
#endif
#ifndef OPENSSL_NO_DSA
#include <openssl/dsa.h>
#endif
#ifndef OPENSSL_NO_DH
#include <openssl/dh.h>
#endif
#include <openssl/rand.h>
#include <openssl/bn.h>

#ifndef OPENSSL_NO_HW
#ifndef OPENSSL_NO_HW_CSWIFT

/* Attribution notice: Rainbow have generously allowed me to reproduce
 * the necessary definitions here from their API. This means the support
 * can build independently of whether application builders have the
 * API or hardware. This will allow developers to easily produce software
 * that has latent hardware support for any users that have accelerators
 * installed, without the developers themselves needing anything extra.
 *
 * I have only clipped the parts from the CryptoSwift header files that
 * are (or seem) relevant to the CryptoSwift support code. This is
 * simply to keep the file sizes reasonable.
 * [Geoff]
 */
#ifdef FLAT_INC
#include "cswift.h"
#else
#include "vendor_defns/cswift.h"
#endif

#define CSWIFT_LIB_NAME "cswift engine"
#include "e_cswift_err.c"

#define DECIMAL_SIZE(type)	((sizeof(type)*8+2)/3+1)

static int cswift_destroy(ENGINE *e);
static int cswift_init(ENGINE *e);
static int cswift_finish(ENGINE *e);
static int cswift_ctrl(ENGINE *e, int cmd, long i, void *p, void (*f)(void));
#ifndef OPENSSL_NO_RSA
static int cswift_bn_32copy(SW_LARGENUMBER * out, const BIGNUM * in);
#endif

/* BIGNUM stuff */
static int cswift_mod_exp(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
		const BIGNUM *m, BN_CTX *ctx);
#ifndef OPENSSL_NO_RSA
static int cswift_mod_exp_crt(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
		const BIGNUM *q, const BIGNUM *dmp1, const BIGNUM *dmq1,
		const BIGNUM *iqmp, BN_CTX *ctx);
#endif

#ifndef OPENSSL_NO_RSA
/* RSA stuff */
static int cswift_rsa_mod_exp(BIGNUM *r0, const BIGNUM *I, RSA *rsa, BN_CTX *ctx);
/* This function is aliased to mod_exp (with the mont stuff dropped). */
static int cswift_mod_exp_mont(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
		const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *m_ctx);
#endif

#ifndef OPENSSL_NO_DSA
/* DSA stuff */
static DSA_SIG *cswift_dsa_sign(const unsigned char *dgst, int dlen, DSA *dsa);
static int cswift_dsa_verify(const unsigned char *dgst, int dgst_len,
				DSA_SIG *sig, DSA *dsa);
#endif

#ifndef OPENSSL_NO_DH
/* DH stuff */
/* This function is alised to mod_exp (with the DH and mont dropped). */
static int cswift_mod_exp_dh(const DH *dh, BIGNUM *r,
		const BIGNUM *a, const BIGNUM *p,
		const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *m_ctx);
#endif

/* RAND stuff */
static int cswift_rand_bytes(unsigned char *buf, int num);
static int cswift_rand_status(void);

/* The definitions for control commands specific to this engine */
#define CSWIFT_CMD_SO_PATH		ENGINE_CMD_BASE
static const ENGINE_CMD_DEFN cswift_cmd_defns[] = {
	{CSWIFT_CMD_SO_PATH,
		"SO_PATH",
		"Specifies the path to the 'cswift' shared library",
		ENGINE_CMD_FLAG_STRING},
	{0, NULL, NULL, 0}
	};

#ifndef OPENSSL_NO_RSA
/* Our internal RSA_METHOD that we provide pointers to */
static RSA_METHOD cswift_rsa =
	{
	"CryptoSwift RSA method",
	NULL,
	NULL,
	NULL,
	NULL,
	cswift_rsa_mod_exp,
	cswift_mod_exp_mont,
	NULL,
	NULL,
	0,
	NULL,
	NULL,
	NULL,
	NULL
	};
#endif

#ifndef OPENSSL_NO_DSA
/* Our internal DSA_METHOD that we provide pointers to */
static DSA_METHOD cswift_dsa =
	{
	"CryptoSwift DSA method",
	cswift_dsa_sign,
	NULL, /* dsa_sign_setup */
	cswift_dsa_verify,
	NULL, /* dsa_mod_exp */
	NULL, /* bn_mod_exp */
	NULL, /* init */
	NULL, /* finish */
	0, /* flags */
	NULL, /* app_data */
	NULL, /* dsa_paramgen */
	NULL /* dsa_keygen */
	};
#endif

#ifndef OPENSSL_NO_DH
/* Our internal DH_METHOD that we provide pointers to */
static DH_METHOD cswift_dh =
	{
	"CryptoSwift DH method",
	NULL,
	NULL,
	cswift_mod_exp_dh,
	NULL,
	NULL,
	0,
	NULL,
	NULL
	};
#endif

static RAND_METHOD cswift_random =
    {
    /* "CryptoSwift RAND method", */
    NULL,
    cswift_rand_bytes,
    NULL,
    NULL,
    cswift_rand_bytes,
    cswift_rand_status,
    };


/* Constants used when creating the ENGINE */
static const char *engine_cswift_id = "cswift";
static const char *engine_cswift_name = "CryptoSwift hardware engine support";

/* This internal function is used by ENGINE_cswift() and possibly by the
 * "dynamic" ENGINE support too */
static int bind_helper(ENGINE *e)
	{
#ifndef OPENSSL_NO_RSA
	const RSA_METHOD *meth1;
#endif
#ifndef OPENSSL_NO_DH
	const DH_METHOD *meth2;
#endif
	if(!ENGINE_set_id(e, engine_cswift_id) ||
			!ENGINE_set_name(e, engine_cswift_name) ||
#ifndef OPENSSL_NO_RSA
			!ENGINE_set_RSA(e, &cswift_rsa) ||
#endif
#ifndef OPENSSL_NO_DSA
			!ENGINE_set_DSA(e, &cswift_dsa) ||
#endif
#ifndef OPENSSL_NO_DH
			!ENGINE_set_DH(e, &cswift_dh) ||
#endif
			!ENGINE_set_RAND(e, &cswift_random) ||
			!ENGINE_set_destroy_function(e, cswift_destroy) ||
			!ENGINE_set_init_function(e, cswift_init) ||
			!ENGINE_set_finish_function(e, cswift_finish) ||
			!ENGINE_set_ctrl_function(e, cswift_ctrl) ||
			!ENGINE_set_cmd_defns(e, cswift_cmd_defns))
		return 0;

#ifndef OPENSSL_NO_RSA
	/* We know that the "PKCS1_SSLeay()" functions hook properly
	 * to the cswift-specific mod_exp and mod_exp_crt so we use
	 * those functions. NB: We don't use ENGINE_openssl() or
	 * anything "more generic" because something like the RSAref
	 * code may not hook properly, and if you own one of these
	 * cards then you have the right to do RSA operations on it
	 * anyway! */ 
	meth1 = RSA_PKCS1_SSLeay();
	cswift_rsa.rsa_pub_enc = meth1->rsa_pub_enc;
	cswift_rsa.rsa_pub_dec = meth1->rsa_pub_dec;
	cswift_rsa.rsa_priv_enc = meth1->rsa_priv_enc;
	cswift_rsa.rsa_priv_dec = meth1->rsa_priv_dec;
#endif

#ifndef OPENSSL_NO_DH
	/* Much the same for Diffie-Hellman */
	meth2 = DH_OpenSSL();
	cswift_dh.generate_key = meth2->generate_key;
	cswift_dh.compute_key = meth2->compute_key;
#endif

	/* Ensure the cswift error handling is set up */
	ERR_load_CSWIFT_strings();
	return 1;
	}

#ifdef OPENSSL_NO_DYNAMIC_ENGINE
static ENGINE *engine_cswift(void)
	{
	ENGINE *ret = ENGINE_new();
	if(!ret)
		return NULL;
	if(!bind_helper(ret))
		{
		ENGINE_free(ret);
		return NULL;
		}
	return ret;
	}

void ENGINE_load_cswift(void)
	{
	/* Copied from eng_[openssl|dyn].c */
	ENGINE *toadd = engine_cswift();
	if(!toadd) return;
	ENGINE_add(toadd);
	ENGINE_free(toadd);
	ERR_clear_error();
	}
#endif

/* This is a process-global DSO handle used for loading and unloading
 * the CryptoSwift library. NB: This is only set (or unset) during an
 * init() or finish() call (reference counts permitting) and they're
 * operating with global locks, so this should be thread-safe
 * implicitly. */
static DSO *cswift_dso = NULL;

/* These are the function pointers that are (un)set when the library has
 * successfully (un)loaded. */
t_swAcquireAccContext *p_CSwift_AcquireAccContext = NULL;
t_swAttachKeyParam *p_CSwift_AttachKeyParam = NULL;
t_swSimpleRequest *p_CSwift_SimpleRequest = NULL;
t_swReleaseAccContext *p_CSwift_ReleaseAccContext = NULL;

/* Used in the DSO operations. */
static const char *CSWIFT_LIBNAME = NULL;
static const char *get_CSWIFT_LIBNAME(void)
	{
	if(CSWIFT_LIBNAME)
		return CSWIFT_LIBNAME;
	return "swift";
	}
static void free_CSWIFT_LIBNAME(void)
	{
	if(CSWIFT_LIBNAME)
		OPENSSL_free((void*)CSWIFT_LIBNAME);
	CSWIFT_LIBNAME = NULL;
	}
static long set_CSWIFT_LIBNAME(const char *name)
	{
	free_CSWIFT_LIBNAME();
	return (((CSWIFT_LIBNAME = BUF_strdup(name)) != NULL) ? 1 : 0);
	}
static const char *CSWIFT_F1 = "swAcquireAccContext";
static const char *CSWIFT_F2 = "swAttachKeyParam";
static const char *CSWIFT_F3 = "swSimpleRequest";
static const char *CSWIFT_F4 = "swReleaseAccContext";


/* CryptoSwift library functions and mechanics - these are used by the
 * higher-level functions further down. NB: As and where there's no
 * error checking, take a look lower down where these functions are
 * called, the checking and error handling is probably down there. */

/* utility function to obtain a context */
static int get_context(SW_CONTEXT_HANDLE *hac)
	{
        SW_STATUS status;
 
        status = p_CSwift_AcquireAccContext(hac);
        if(status != SW_OK)
                return 0;
        return 1;
	}
 
/* similarly to release one. */
static void release_context(SW_CONTEXT_HANDLE hac)
	{
        p_CSwift_ReleaseAccContext(hac);
	}

/* Destructor (complements the "ENGINE_cswift()" constructor) */
static int cswift_destroy(ENGINE *e)
	{
	free_CSWIFT_LIBNAME();
	ERR_unload_CSWIFT_strings();
	return 1;
	}

/* (de)initialisation functions. */
static int cswift_init(ENGINE *e)
	{
        SW_CONTEXT_HANDLE hac;
        t_swAcquireAccContext *p1;
        t_swAttachKeyParam *p2;
        t_swSimpleRequest *p3;
        t_swReleaseAccContext *p4;

	if(cswift_dso != NULL)
		{
		CSWIFTerr(CSWIFT_F_CSWIFT_INIT,CSWIFT_R_ALREADY_LOADED);
		goto err;
		}
	/* Attempt to load libswift.so/swift.dll/whatever. */
	cswift_dso = DSO_load(NULL, get_CSWIFT_LIBNAME(), NULL, 0);
	if(cswift_dso == NULL)
		{
		CSWIFTerr(CSWIFT_F_CSWIFT_INIT,CSWIFT_R_NOT_LOADED);
		goto err;
		}
	if(!(p1 = (t_swAcquireAccContext *)
				DSO_bind_func(cswift_dso, CSWIFT_F1)) ||
			!(p2 = (t_swAttachKeyParam *)
				DSO_bind_func(cswift_dso, CSWIFT_F2)) ||
			!(p3 = (t_swSimpleRequest *)
				DSO_bind_func(cswift_dso, CSWIFT_F3)) ||
			!(p4 = (t_swReleaseAccContext *)
				DSO_bind_func(cswift_dso, CSWIFT_F4)))
		{
		CSWIFTerr(CSWIFT_F_CSWIFT_INIT,CSWIFT_R_NOT_LOADED);
		goto err;
		}
	/* Copy the pointers */
	p_CSwift_AcquireAccContext = p1;
	p_CSwift_AttachKeyParam = p2;
	p_CSwift_SimpleRequest = p3;
	p_CSwift_ReleaseAccContext = p4;
	/* Try and get a context - if not, we may have a DSO but no
	 * accelerator! */
	if(!get_context(&hac))
		{
		CSWIFTerr(CSWIFT_F_CSWIFT_INIT,CSWIFT_R_UNIT_FAILURE);
		goto err;
		}
	release_context(hac);
	/* Everything's fine. */
	return 1;
err:
	if(cswift_dso)
	{
		DSO_free(cswift_dso);
		cswift_dso = NULL;
	}
	p_CSwift_AcquireAccContext = NULL;
	p_CSwift_AttachKeyParam = NULL;
	p_CSwift_SimpleRequest = NULL;
	p_CSwift_ReleaseAccContext = NULL;
	return 0;
	}

static int cswift_finish(ENGINE *e)
	{
	free_CSWIFT_LIBNAME();
	if(cswift_dso == NULL)
		{
		CSWIFTerr(CSWIFT_F_CSWIFT_FINISH,CSWIFT_R_NOT_LOADED);
		return 0;
		}
	if(!DSO_free(cswift_dso))
		{
		CSWIFTerr(CSWIFT_F_CSWIFT_FINISH,CSWIFT_R_UNIT_FAILURE);
		return 0;
		}
	cswift_dso = NULL;
	p_CSwift_AcquireAccContext = NULL;
	p_CSwift_AttachKeyParam = NULL;
	p_CSwift_SimpleRequest = NULL;
	p_CSwift_ReleaseAccContext = NULL;
	return 1;
	}

static int cswift_ctrl(ENGINE *e, int cmd, long i, void *p, void (*f)(void))
	{
	int initialised = ((cswift_dso == NULL) ? 0 : 1);
	switch(cmd)
		{
	case CSWIFT_CMD_SO_PATH:
		if(p == NULL)
			{
			CSWIFTerr(CSWIFT_F_CSWIFT_CTRL,ERR_R_PASSED_NULL_PARAMETER);
			return 0;
			}
		if(initialised)
			{
			CSWIFTerr(CSWIFT_F_CSWIFT_CTRL,CSWIFT_R_ALREADY_LOADED);
			return 0;
			}
		return set_CSWIFT_LIBNAME((const char *)p);
	default:
		break;
		}
	CSWIFTerr(CSWIFT_F_CSWIFT_CTRL,CSWIFT_R_CTRL_COMMAND_NOT_IMPLEMENTED);
	return 0;
	}

/* Un petit mod_exp */
static int cswift_mod_exp(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
			const BIGNUM *m, BN_CTX *ctx)
	{
	/* I need somewhere to store temporary serialised values for
	 * use with the CryptoSwift API calls. A neat cheat - I'll use
	 * BIGNUMs from the BN_CTX but access their arrays directly as
	 * byte arrays <grin>. This way I don't have to clean anything
	 * up. */
	BIGNUM *modulus;
	BIGNUM *exponent;
	BIGNUM *argument;
	BIGNUM *result;
	SW_STATUS sw_status;
	SW_LARGENUMBER arg, res;
	SW_PARAM sw_param;
	SW_CONTEXT_HANDLE hac;
	int to_return, acquired;
 
	modulus = exponent = argument = result = NULL;
	to_return = 0; /* expect failure */
	acquired = 0;
 
	if(!get_context(&hac))
		{
		CSWIFTerr(CSWIFT_F_CSWIFT_MOD_EXP,CSWIFT_R_UNIT_FAILURE);
		goto err;
		}
	acquired = 1;
	/* Prepare the params */
	BN_CTX_start(ctx);
	modulus = BN_CTX_get(ctx);
	exponent = BN_CTX_get(ctx);
	argument = BN_CTX_get(ctx);
	result = BN_CTX_get(ctx);
	if(!result)
		{
		CSWIFTerr(CSWIFT_F_CSWIFT_MOD_EXP,CSWIFT_R_BN_CTX_FULL);
		goto err;
		}
	if(!bn_wexpand(modulus, m->top) || !bn_wexpand(exponent, p->top) ||
		!bn_wexpand(argument, a->top) || !bn_wexpand(result, m->top))
		{
		CSWIFTerr(CSWIFT_F_CSWIFT_MOD_EXP,CSWIFT_R_BN_EXPAND_FAIL);
		goto err;
		}
	sw_param.type = SW_ALG_EXP;
	sw_param.up.exp.modulus.nbytes = BN_bn2bin(m,
		(unsigned char *)modulus->d);
	sw_param.up.exp.modulus.value = (unsigned char *)modulus->d;
	sw_param.up.exp.exponent.nbytes = BN_bn2bin(p,
		(unsigned char *)exponent->d);
	sw_param.up.exp.exponent.value = (unsigned char *)exponent->d;
	/* Attach the key params */
	sw_status = p_CSwift_AttachKeyParam(hac, &sw_param);
	switch(sw_status)
		{
	case SW_OK:
		break;
	case SW_ERR_INPUT_SIZE:
		CSWIFTerr(CSWIFT_F_CSWIFT_MOD_EXP,CSWIFT_R_BAD_KEY_SIZE);
		goto err;
	default:
		{
		char tmpbuf[DECIMAL_SIZE(sw_status)+1];
		CSWIFTerr(CSWIFT_F_CSWIFT_MOD_EXP,CSWIFT_R_REQUEST_FAILED);
		sprintf(tmpbuf, "%ld", sw_status);
		ERR_add_error_data(2, "CryptoSwift error number is ",tmpbuf);
		}
		goto err;
		}
	/* Prepare the argument and response */
	arg.nbytes = BN_bn2bin(a, (unsigned char *)argument->d);
	arg.value = (unsigned char *)argument->d;
	res.nbytes = BN_num_bytes(m);
	memset(result->d, 0, res.nbytes);
	res.value = (unsigned char *)result->d;
	/* Perform the operation */
	if((sw_status = p_CSwift_SimpleRequest(hac, SW_CMD_MODEXP, &arg, 1,
		&res, 1)) != SW_OK)
		{
		char tmpbuf[DECIMAL_SIZE(sw_status)+1];
		CSWIFTerr(CSWIFT_F_CSWIFT_MOD_EXP,CSWIFT_R_REQUEST_FAILED);
		sprintf(tmpbuf, "%ld", sw_status);
		ERR_add_error_data(2, "CryptoSwift error number is ",tmpbuf);
		goto err;
		}
	/* Convert the response */
	BN_bin2bn((unsigned char *)result->d, res.nbytes, r);
	to_return = 1;
err:
	if(acquired)
		release_context(hac);
	BN_CTX_end(ctx);
	return to_return;
	}


#ifndef OPENSSL_NO_RSA
int cswift_bn_32copy(SW_LARGENUMBER * out, const BIGNUM * in)
{
	int mod;
	int numbytes = BN_num_bytes(in);

	mod = 0;
	while( ((out->nbytes = (numbytes+mod)) % 32) )
	{
		mod++;
	}
	out->value = (unsigned char*)OPENSSL_malloc(out->nbytes);
	if(!out->value)
	{
		return 0;
	}
	BN_bn2bin(in, &out->value[mod]);
	if(mod)
		memset(out->value, 0, mod);

	return 1;
}
#endif

#ifndef OPENSSL_NO_RSA
/* Un petit mod_exp chinois */
static int cswift_mod_exp_crt(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
			const BIGNUM *q, const BIGNUM *dmp1,
			const BIGNUM *dmq1, const BIGNUM *iqmp, BN_CTX *ctx)
	{
	SW_STATUS sw_status;
	SW_LARGENUMBER arg, res;
	SW_PARAM sw_param;
	SW_CONTEXT_HANDLE hac;
	BIGNUM *result = NULL;
	BIGNUM *argument = NULL;
	int to_return = 0; /* expect failure */
	int acquired = 0;

	sw_param.up.crt.p.value = NULL;
	sw_param.up.crt.q.value = NULL;
	sw_param.up.crt.dmp1.value = NULL;
	sw_param.up.crt.dmq1.value = NULL;
	sw_param.up.crt.iqmp.value = NULL;
 
	if(!get_context(&hac))
		{
		CSWIFTerr(CSWIFT_F_CSWIFT_MOD_EXP_CRT,CSWIFT_R_UNIT_FAILURE);
		goto err;
		}
	acquired = 1;

	/* Prepare the params */
	argument = BN_new();
	result = BN_new();
	if(!result || !argument)
		{
		CSWIFTerr(CSWIFT_F_CSWIFT_MOD_EXP_CRT,CSWIFT_R_BN_CTX_FULL);
		goto err;
		}


	sw_param.type = SW_ALG_CRT;
	/************************************************************************/
	/* 04/02/2003                                                           */
	/* Modified by Frederic Giudicelli (deny-all.com) to overcome the       */
	/* limitation of cswift with values not a multiple of 32                */
	/************************************************************************/
	if(!cswift_bn_32copy(&sw_param.up.crt.p, p))
	{
		CSWIFTerr(CSWIFT_F_CSWIFT_MOD_EXP_CRT,CSWIFT_R_BN_EXPAND_FAIL);
		goto err;
	}
	if(!cswift_bn_32copy(&sw_param.up.crt.q, q))
	{
		CSWIFTerr(CSWIFT_F_CSWIFT_MOD_EXP_CRT,CSWIFT_R_BN_EXPAND_FAIL);
		goto err;
	}
	if(!cswift_bn_32copy(&sw_param.up.crt.dmp1, dmp1))
	{
		CSWIFTerr(CSWIFT_F_CSWIFT_MOD_EXP_CRT,CSWIFT_R_BN_EXPAND_FAIL);
		goto err;
	}
	if(!cswift_bn_32copy(&sw_param.up.crt.dmq1, dmq1))
	{
		CSWIFTerr(CSWIFT_F_CSWIFT_MOD_EXP_CRT,CSWIFT_R_BN_EXPAND_FAIL);
		goto err;
	}
	if(!cswift_bn_32copy(&sw_param.up.crt.iqmp, iqmp))
	{
		CSWIFTerr(CSWIFT_F_CSWIFT_MOD_EXP_CRT,CSWIFT_R_BN_EXPAND_FAIL);
		goto err;
	}
	if(	!bn_wexpand(argument, a->top) ||
			!bn_wexpand(result, p->top + q->top))
		{
		CSWIFTerr(CSWIFT_F_CSWIFT_MOD_EXP_CRT,CSWIFT_R_BN_EXPAND_FAIL);
		goto err;
		}

	/* Attach the key params */
	sw_status = p_CSwift_AttachKeyParam(hac, &sw_param);
	switch(sw_status)
		{
	case SW_OK:
		break;
	case SW_ERR_INPUT_SIZE:
		CSWIFTerr(CSWIFT_F_CSWIFT_MOD_EXP_CRT,CSWIFT_R_BAD_KEY_SIZE);
		goto err;
	default:
		{
		char tmpbuf[DECIMAL_SIZE(sw_status)+1];
		CSWIFTerr(CSWIFT_F_CSWIFT_MOD_EXP_CRT,CSWIFT_R_REQUEST_FAILED);
		sprintf(tmpbuf, "%ld", sw_status);
		ERR_add_error_data(2, "CryptoSwift error number is ",tmpbuf);
		}
		goto err;
		}
	/* Prepare the argument and response */
	arg.nbytes = BN_bn2bin(a, (unsigned char *)argument->d);
	arg.value = (unsigned char *)argument->d;
	res.nbytes = 2 * BN_num_bytes(p);
	memset(result->d, 0, res.nbytes);
	res.value = (unsigned char *)result->d;
	/* Perform the operation */
	if((sw_status = p_CSwift_SimpleRequest(hac, SW_CMD_MODEXP_CRT, &arg, 1,
		&res, 1)) != SW_OK)
		{
		char tmpbuf[DECIMAL_SIZE(sw_status)+1];
		CSWIFTerr(CSWIFT_F_CSWIFT_MOD_EXP_CRT,CSWIFT_R_REQUEST_FAILED);
		sprintf(tmpbuf, "%ld", sw_status);
		ERR_add_error_data(2, "CryptoSwift error number is ",tmpbuf);
		goto err;
		}
	/* Convert the response */
	BN_bin2bn((unsigned char *)result->d, res.nbytes, r);
	to_return = 1;
err:
	if(sw_param.up.crt.p.value)
		OPENSSL_free(sw_param.up.crt.p.value);
	if(sw_param.up.crt.q.value)
		OPENSSL_free(sw_param.up.crt.q.value);
	if(sw_param.up.crt.dmp1.value)
		OPENSSL_free(sw_param.up.crt.dmp1.value);
	if(sw_param.up.crt.dmq1.value)
		OPENSSL_free(sw_param.up.crt.dmq1.value);
	if(sw_param.up.crt.iqmp.value)
		OPENSSL_free(sw_param.up.crt.iqmp.value);
	if(result)
		BN_free(result);
	if(argument)
		BN_free(argument);
	if(acquired)
		release_context(hac);
	return to_return;
	}
#endif
 
#ifndef OPENSSL_NO_RSA
static int cswift_rsa_mod_exp(BIGNUM *r0, const BIGNUM *I, RSA *rsa, BN_CTX *ctx)
	{
	int to_return = 0;
	const RSA_METHOD * def_rsa_method;

	if(!rsa->p || !rsa->q || !rsa->dmp1 || !rsa->dmq1 || !rsa->iqmp)
		{
		CSWIFTerr(CSWIFT_F_CSWIFT_RSA_MOD_EXP,CSWIFT_R_MISSING_KEY_COMPONENTS);
		goto err;
		}

	/* Try the limits of RSA (2048 bits) */
	if(BN_num_bytes(rsa->p) > 128 ||
		BN_num_bytes(rsa->q) > 128 ||
		BN_num_bytes(rsa->dmp1) > 128 ||
		BN_num_bytes(rsa->dmq1) > 128 ||
		BN_num_bytes(rsa->iqmp) > 128)
	{
#ifdef RSA_NULL
		def_rsa_method=RSA_null_method();
#else
#if 0
		def_rsa_method=RSA_PKCS1_RSAref();
#else
		def_rsa_method=RSA_PKCS1_SSLeay();
#endif
#endif
		if(def_rsa_method)
			return def_rsa_method->rsa_mod_exp(r0, I, rsa, ctx);
	}

	to_return = cswift_mod_exp_crt(r0, I, rsa->p, rsa->q, rsa->dmp1,
		rsa->dmq1, rsa->iqmp, ctx);
err:
	return to_return;
	}

/* This function is aliased to mod_exp (with the mont stuff dropped). */
static int cswift_mod_exp_mont(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
		const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *m_ctx)
	{
	const RSA_METHOD * def_rsa_method;

	/* Try the limits of RSA (2048 bits) */
	if(BN_num_bytes(r) > 256 ||
		BN_num_bytes(a) > 256 ||
		BN_num_bytes(m) > 256)
	{
#ifdef RSA_NULL
		def_rsa_method=RSA_null_method();
#else
#if 0
		def_rsa_method=RSA_PKCS1_RSAref();
#else
		def_rsa_method=RSA_PKCS1_SSLeay();
#endif
#endif
		if(def_rsa_method)
			return def_rsa_method->bn_mod_exp(r, a, p, m, ctx, m_ctx);
	}

	return cswift_mod_exp(r, a, p, m, ctx);
	}
#endif	/* OPENSSL_NO_RSA */

#ifndef OPENSSL_NO_DSA
static DSA_SIG *cswift_dsa_sign(const unsigned char *dgst, int dlen, DSA *dsa)
	{
	SW_CONTEXT_HANDLE hac;
	SW_PARAM sw_param;
	SW_STATUS sw_status;
	SW_LARGENUMBER arg, res;
	BN_CTX *ctx;
	BIGNUM *dsa_p = NULL;
	BIGNUM *dsa_q = NULL;
	BIGNUM *dsa_g = NULL;
	BIGNUM *dsa_key = NULL;
	BIGNUM *result = NULL;
	DSA_SIG *to_return = NULL;
	int acquired = 0;

	if((ctx = BN_CTX_new()) == NULL)
		goto err;
	if(!get_context(&hac))
		{
		CSWIFTerr(CSWIFT_F_CSWIFT_DSA_SIGN,CSWIFT_R_UNIT_FAILURE);
		goto err;
		}
	acquired = 1;
	/* Prepare the params */
	BN_CTX_start(ctx);
	dsa_p = BN_CTX_get(ctx);
	dsa_q = BN_CTX_get(ctx);
	dsa_g = BN_CTX_get(ctx);
	dsa_key = BN_CTX_get(ctx);
	result = BN_CTX_get(ctx);
	if(!result)
		{
		CSWIFTerr(CSWIFT_F_CSWIFT_DSA_SIGN,CSWIFT_R_BN_CTX_FULL);
		goto err;
		}
	if(!bn_wexpand(dsa_p, dsa->p->top) ||
			!bn_wexpand(dsa_q, dsa->q->top) ||
			!bn_wexpand(dsa_g, dsa->g->top) ||
			!bn_wexpand(dsa_key, dsa->priv_key->top) ||
			!bn_wexpand(result, dsa->p->top))
		{
		CSWIFTerr(CSWIFT_F_CSWIFT_DSA_SIGN,CSWIFT_R_BN_EXPAND_FAIL);
		goto err;
		}
	sw_param.type = SW_ALG_DSA;
	sw_param.up.dsa.p.nbytes = BN_bn2bin(dsa->p,
				(unsigned char *)dsa_p->d);
	sw_param.up.dsa.p.value = (unsigned char *)dsa_p->d;
	sw_param.up.dsa.q.nbytes = BN_bn2bin(dsa->q,
				(unsigned char *)dsa_q->d);
	sw_param.up.dsa.q.value = (unsigned char *)dsa_q->d;
	sw_param.up.dsa.g.nbytes = BN_bn2bin(dsa->g,
				(unsigned char *)dsa_g->d);
	sw_param.up.dsa.g.value = (unsigned char *)dsa_g->d;
	sw_param.up.dsa.key.nbytes = BN_bn2bin(dsa->priv_key,
				(unsigned char *)dsa_key->d);
	sw_param.up.dsa.key.value = (unsigned char *)dsa_key->d;
	/* Attach the key params */
	sw_status = p_CSwift_AttachKeyParam(hac, &sw_param);
	switch(sw_status)
		{
	case SW_OK:
		break;
	case SW_ERR_INPUT_SIZE:
		CSWIFTerr(CSWIFT_F_CSWIFT_DSA_SIGN,CSWIFT_R_BAD_KEY_SIZE);
		goto err;
	default:
		{
		char tmpbuf[DECIMAL_SIZE(sw_status)+1];
		CSWIFTerr(CSWIFT_F_CSWIFT_DSA_SIGN,CSWIFT_R_REQUEST_FAILED);
		sprintf(tmpbuf, "%ld", sw_status);
		ERR_add_error_data(2, "CryptoSwift error number is ",tmpbuf);
		}
		goto err;
		}
	/* Prepare the argument and response */
	arg.nbytes = dlen;
	arg.value = (unsigned char *)dgst;
	res.nbytes = BN_num_bytes(dsa->p);
	memset(result->d, 0, res.nbytes);
	res.value = (unsigned char *)result->d;
	/* Perform the operation */
	sw_status = p_CSwift_SimpleRequest(hac, SW_CMD_DSS_SIGN, &arg, 1,
		&res, 1);
	if(sw_status != SW_OK)
		{
		char tmpbuf[DECIMAL_SIZE(sw_status)+1];
		CSWIFTerr(CSWIFT_F_CSWIFT_DSA_SIGN,CSWIFT_R_REQUEST_FAILED);
		sprintf(tmpbuf, "%ld", sw_status);
		ERR_add_error_data(2, "CryptoSwift error number is ",tmpbuf);
		goto err;
		}
	/* Convert the response */
	if((to_return = DSA_SIG_new()) == NULL)
		goto err;
	to_return->r = BN_bin2bn((unsigned char *)result->d, 20, NULL);
	to_return->s = BN_bin2bn((unsigned char *)result->d + 20, 20, NULL);

err:
	if(acquired)
		release_context(hac);
	if(ctx)
		{
		BN_CTX_end(ctx);
		BN_CTX_free(ctx);
		}
	return to_return;
	}

static int cswift_dsa_verify(const unsigned char *dgst, int dgst_len,
				DSA_SIG *sig, DSA *dsa)
	{
	SW_CONTEXT_HANDLE hac;
	SW_PARAM sw_param;
	SW_STATUS sw_status;
	SW_LARGENUMBER arg[2], res;
	unsigned long sig_result;
	BN_CTX *ctx;
	BIGNUM *dsa_p = NULL;
	BIGNUM *dsa_q = NULL;
	BIGNUM *dsa_g = NULL;
	BIGNUM *dsa_key = NULL;
	BIGNUM *argument = NULL;
	int to_return = -1;
	int acquired = 0;

	if((ctx = BN_CTX_new()) == NULL)
		goto err;
	if(!get_context(&hac))
		{
		CSWIFTerr(CSWIFT_F_CSWIFT_DSA_VERIFY,CSWIFT_R_UNIT_FAILURE);
		goto err;
		}
	acquired = 1;
	/* Prepare the params */
	BN_CTX_start(ctx);
	dsa_p = BN_CTX_get(ctx);
	dsa_q = BN_CTX_get(ctx);
	dsa_g = BN_CTX_get(ctx);
	dsa_key = BN_CTX_get(ctx);
	argument = BN_CTX_get(ctx);
	if(!argument)
		{
		CSWIFTerr(CSWIFT_F_CSWIFT_DSA_VERIFY,CSWIFT_R_BN_CTX_FULL);
		goto err;
		}
	if(!bn_wexpand(dsa_p, dsa->p->top) ||
			!bn_wexpand(dsa_q, dsa->q->top) ||
			!bn_wexpand(dsa_g, dsa->g->top) ||
			!bn_wexpand(dsa_key, dsa->pub_key->top) ||
			!bn_wexpand(argument, 40))
		{
		CSWIFTerr(CSWIFT_F_CSWIFT_DSA_VERIFY,CSWIFT_R_BN_EXPAND_FAIL);
		goto err;
		}
	sw_param.type = SW_ALG_DSA;
	sw_param.up.dsa.p.nbytes = BN_bn2bin(dsa->p,
				(unsigned char *)dsa_p->d);
	sw_param.up.dsa.p.value = (unsigned char *)dsa_p->d;
	sw_param.up.dsa.q.nbytes = BN_bn2bin(dsa->q,
				(unsigned char *)dsa_q->d);
	sw_param.up.dsa.q.value = (unsigned char *)dsa_q->d;
	sw_param.up.dsa.g.nbytes = BN_bn2bin(dsa->g,
				(unsigned char *)dsa_g->d);
	sw_param.up.dsa.g.value = (unsigned char *)dsa_g->d;
	sw_param.up.dsa.key.nbytes = BN_bn2bin(dsa->pub_key,
				(unsigned char *)dsa_key->d);
	sw_param.up.dsa.key.value = (unsigned char *)dsa_key->d;
	/* Attach the key params */
	sw_status = p_CSwift_AttachKeyParam(hac, &sw_param);
	switch(sw_status)
		{
	case SW_OK:
		break;
	case SW_ERR_INPUT_SIZE:
		CSWIFTerr(CSWIFT_F_CSWIFT_DSA_VERIFY,CSWIFT_R_BAD_KEY_SIZE);
		goto err;
	default:
		{
		char tmpbuf[DECIMAL_SIZE(sw_status)+1];
		CSWIFTerr(CSWIFT_F_CSWIFT_DSA_VERIFY,CSWIFT_R_REQUEST_FAILED);
		sprintf(tmpbuf, "%ld", sw_status);
		ERR_add_error_data(2, "CryptoSwift error number is ",tmpbuf);
		}
		goto err;
		}
	/* Prepare the argument and response */
	arg[0].nbytes = dgst_len;
	arg[0].value = (unsigned char *)dgst;
	arg[1].nbytes = 40;
	arg[1].value = (unsigned char *)argument->d;
	memset(arg[1].value, 0, 40);
	BN_bn2bin(sig->r, arg[1].value + 20 - BN_num_bytes(sig->r));
	BN_bn2bin(sig->s, arg[1].value + 40 - BN_num_bytes(sig->s));
	res.nbytes = 4; /* unsigned long */
	res.value = (unsigned char *)(&sig_result);
	/* Perform the operation */
	sw_status = p_CSwift_SimpleRequest(hac, SW_CMD_DSS_VERIFY, arg, 2,
		&res, 1);
	if(sw_status != SW_OK)
		{
		char tmpbuf[DECIMAL_SIZE(sw_status)+1];
		CSWIFTerr(CSWIFT_F_CSWIFT_DSA_VERIFY,CSWIFT_R_REQUEST_FAILED);
		sprintf(tmpbuf, "%ld", sw_status);
		ERR_add_error_data(2, "CryptoSwift error number is ",tmpbuf);
		goto err;
		}
	/* Convert the response */
	to_return = ((sig_result == 0) ? 0 : 1);

err:
	if(acquired)
		release_context(hac);
	if(ctx)
		{
		BN_CTX_end(ctx);
		BN_CTX_free(ctx);
		}
	return to_return;
	}
#endif

#ifndef OPENSSL_NO_DH
/* This function is aliased to mod_exp (with the dh and mont dropped). */
static int cswift_mod_exp_dh(const DH *dh, BIGNUM *r,
		const BIGNUM *a, const BIGNUM *p,
		const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *m_ctx)
	{
	return cswift_mod_exp(r, a, p, m, ctx);
	}
#endif

/* Random bytes are good */
static int cswift_rand_bytes(unsigned char *buf, int num)
{
	SW_CONTEXT_HANDLE hac;
	SW_STATUS swrc;
	SW_LARGENUMBER largenum;
	int acquired = 0;
	int to_return = 0; /* assume failure */
	unsigned char buf32[1024];


	if (!get_context(&hac))
	{
		CSWIFTerr(CSWIFT_F_CSWIFT_RAND_BYTES, CSWIFT_R_UNIT_FAILURE);
		goto err;
	}
	acquired = 1;

	/************************************************************************/
	/* 04/02/2003                                                           */
	/* Modified by Frederic Giudicelli (deny-all.com) to overcome the       */
	/* limitation of cswift with values not a multiple of 32                */
	/************************************************************************/

	while(num >= (int)sizeof(buf32))
	{
		largenum.value = buf;
		largenum.nbytes = sizeof(buf32);
		/* tell CryptoSwift how many bytes we want and where we want it.
		 * Note: - CryptoSwift cannot do more than 4096 bytes at a time.
		 *       - CryptoSwift can only do multiple of 32-bits. */
		swrc = p_CSwift_SimpleRequest(hac, SW_CMD_RAND, NULL, 0, &largenum, 1);
		if (swrc != SW_OK)
		{
			char tmpbuf[20];
			CSWIFTerr(CSWIFT_F_CSWIFT_RAND_BYTES, CSWIFT_R_REQUEST_FAILED);
			sprintf(tmpbuf, "%ld", swrc);
			ERR_add_error_data(2, "CryptoSwift error number is ", tmpbuf);
			goto err;
		}
		buf += sizeof(buf32);
		num -= sizeof(buf32);
	}
	if(num)
	{
		largenum.nbytes = sizeof(buf32);
		largenum.value = buf32;
		swrc = p_CSwift_SimpleRequest(hac, SW_CMD_RAND, NULL, 0, &largenum, 1);
		if (swrc != SW_OK)
		{
			char tmpbuf[20];
			CSWIFTerr(CSWIFT_F_CSWIFT_RAND_BYTES, CSWIFT_R_REQUEST_FAILED);
			sprintf(tmpbuf, "%ld", swrc);
			ERR_add_error_data(2, "CryptoSwift error number is ", tmpbuf);
			goto err;
		}
		memcpy(buf, largenum.value, num);
	}

	to_return = 1;  /* success */
err:
	if (acquired)
		release_context(hac);

	return to_return;
}

static int cswift_rand_status(void)
{
	return 1;
}


/* This stuff is needed if this ENGINE is being compiled into a self-contained
 * shared-library. */
#ifndef OPENSSL_NO_DYNAMIC_ENGINE
static int bind_fn(ENGINE *e, const char *id)
	{
	if(id && (strcmp(id, engine_cswift_id) != 0))
		return 0;
	if(!bind_helper(e))
		return 0;
	return 1;
	}       
IMPLEMENT_DYNAMIC_CHECK_FN()
IMPLEMENT_DYNAMIC_BIND_FN(bind_fn)
#endif /* OPENSSL_NO_DYNAMIC_ENGINE */

#endif /* !OPENSSL_NO_HW_CSWIFT */
#endif /* !OPENSSL_NO_HW */
