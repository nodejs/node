/* crypto/engine/hw_ubsec.c */
/* Written by Geoff Thorpe (geoff@geoffthorpe.net) for the OpenSSL
 * project 2000.
 *
 * Cloned shamelessly by Joe Tardo. 
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
#include <openssl/bn.h>

#ifndef OPENSSL_NO_HW
#ifndef OPENSSL_NO_HW_UBSEC

#ifdef FLAT_INC
#include "hw_ubsec.h"
#else
#include "vendor_defns/hw_ubsec.h"
#endif

#define UBSEC_LIB_NAME "ubsec engine"
#include "e_ubsec_err.c"

#define FAIL_TO_SOFTWARE -15

static int ubsec_destroy(ENGINE *e);
static int ubsec_init(ENGINE *e);
static int ubsec_finish(ENGINE *e);
static int ubsec_ctrl(ENGINE *e, int cmd, long i, void *p, void (*f)(void));
static int ubsec_mod_exp(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
		const BIGNUM *m, BN_CTX *ctx);
#ifndef OPENSSL_NO_RSA
static int ubsec_mod_exp_crt(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
			const BIGNUM *q, const BIGNUM *dp,
			const BIGNUM *dq, const BIGNUM *qinv, BN_CTX *ctx);
static int ubsec_rsa_mod_exp(BIGNUM *r0, const BIGNUM *I, RSA *rsa, BN_CTX *ctx);
static int ubsec_mod_exp_mont(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
		const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *m_ctx);
#endif
#ifndef OPENSSL_NO_DSA
#ifdef NOT_USED
static int ubsec_dsa_mod_exp(DSA *dsa, BIGNUM *rr, BIGNUM *a1,
		BIGNUM *p1, BIGNUM *a2, BIGNUM *p2, BIGNUM *m,
		BN_CTX *ctx, BN_MONT_CTX *in_mont);
static int ubsec_mod_exp_dsa(DSA *dsa, BIGNUM *r, BIGNUM *a,
		const BIGNUM *p, const BIGNUM *m, BN_CTX *ctx,
		BN_MONT_CTX *m_ctx);
#endif
static DSA_SIG *ubsec_dsa_do_sign(const unsigned char *dgst, int dlen, DSA *dsa);
static int ubsec_dsa_verify(const unsigned char *dgst, int dgst_len,
                                DSA_SIG *sig, DSA *dsa);
#endif
#ifndef OPENSSL_NO_DH
static int ubsec_mod_exp_dh(const DH *dh, BIGNUM *r, const BIGNUM *a,
		const BIGNUM *p, const BIGNUM *m, BN_CTX *ctx,
		BN_MONT_CTX *m_ctx);
static int ubsec_dh_compute_key(unsigned char *key,const BIGNUM *pub_key,DH *dh);
static int ubsec_dh_generate_key(DH *dh);
#endif

#ifdef NOT_USED
static int ubsec_rand_bytes(unsigned char *buf, int num);
static int ubsec_rand_status(void);
#endif

#define UBSEC_CMD_SO_PATH		ENGINE_CMD_BASE
static const ENGINE_CMD_DEFN ubsec_cmd_defns[] = {
	{UBSEC_CMD_SO_PATH,
		"SO_PATH",
		"Specifies the path to the 'ubsec' shared library",
		ENGINE_CMD_FLAG_STRING},
	{0, NULL, NULL, 0}
	};

#ifndef OPENSSL_NO_RSA
/* Our internal RSA_METHOD that we provide pointers to */
static RSA_METHOD ubsec_rsa =
	{
	"UBSEC RSA method",
	NULL,
	NULL,
	NULL,
	NULL,
	ubsec_rsa_mod_exp,
	ubsec_mod_exp_mont,
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
static DSA_METHOD ubsec_dsa =
	{
	"UBSEC DSA method",
	ubsec_dsa_do_sign, /* dsa_do_sign */
	NULL, /* dsa_sign_setup */
	ubsec_dsa_verify, /* dsa_do_verify */
	NULL, /* ubsec_dsa_mod_exp */ /* dsa_mod_exp */
	NULL, /* ubsec_mod_exp_dsa */ /* bn_mod_exp */
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
static DH_METHOD ubsec_dh =
	{
	"UBSEC DH method",
	ubsec_dh_generate_key,
	ubsec_dh_compute_key,
	ubsec_mod_exp_dh,
	NULL,
	NULL,
	0,
	NULL,
	NULL
	};
#endif

/* Constants used when creating the ENGINE */
static const char *engine_ubsec_id = "ubsec";
static const char *engine_ubsec_name = "UBSEC hardware engine support";

/* This internal function is used by ENGINE_ubsec() and possibly by the
 * "dynamic" ENGINE support too */
static int bind_helper(ENGINE *e)
	{
#ifndef OPENSSL_NO_RSA
	const RSA_METHOD *meth1;
#endif
#ifndef OPENSSL_NO_DH
#ifndef HAVE_UBSEC_DH
	const DH_METHOD *meth3;
#endif /* HAVE_UBSEC_DH */
#endif
	if(!ENGINE_set_id(e, engine_ubsec_id) ||
			!ENGINE_set_name(e, engine_ubsec_name) ||
#ifndef OPENSSL_NO_RSA
			!ENGINE_set_RSA(e, &ubsec_rsa) ||
#endif
#ifndef OPENSSL_NO_DSA
			!ENGINE_set_DSA(e, &ubsec_dsa) ||
#endif
#ifndef OPENSSL_NO_DH
			!ENGINE_set_DH(e, &ubsec_dh) ||
#endif
			!ENGINE_set_destroy_function(e, ubsec_destroy) ||
			!ENGINE_set_init_function(e, ubsec_init) ||
			!ENGINE_set_finish_function(e, ubsec_finish) ||
			!ENGINE_set_ctrl_function(e, ubsec_ctrl) ||
			!ENGINE_set_cmd_defns(e, ubsec_cmd_defns))
		return 0;

#ifndef OPENSSL_NO_RSA
	/* We know that the "PKCS1_SSLeay()" functions hook properly
	 * to the Broadcom-specific mod_exp and mod_exp_crt so we use
	 * those functions. NB: We don't use ENGINE_openssl() or
	 * anything "more generic" because something like the RSAref
	 * code may not hook properly, and if you own one of these
	 * cards then you have the right to do RSA operations on it
	 * anyway! */ 
	meth1 = RSA_PKCS1_SSLeay();
	ubsec_rsa.rsa_pub_enc = meth1->rsa_pub_enc;
	ubsec_rsa.rsa_pub_dec = meth1->rsa_pub_dec;
	ubsec_rsa.rsa_priv_enc = meth1->rsa_priv_enc;
	ubsec_rsa.rsa_priv_dec = meth1->rsa_priv_dec;
#endif

#ifndef OPENSSL_NO_DH
#ifndef HAVE_UBSEC_DH
	/* Much the same for Diffie-Hellman */
	meth3 = DH_OpenSSL();
	ubsec_dh.generate_key = meth3->generate_key;
	ubsec_dh.compute_key = meth3->compute_key;
#endif /* HAVE_UBSEC_DH */
#endif

	/* Ensure the ubsec error handling is set up */
	ERR_load_UBSEC_strings();
	return 1;
	}

#ifdef OPENSSL_NO_DYNAMIC_ENGINE
static ENGINE *engine_ubsec(void)
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

void ENGINE_load_ubsec(void)
	{
	/* Copied from eng_[openssl|dyn].c */
	ENGINE *toadd = engine_ubsec();
	if(!toadd) return;
	ENGINE_add(toadd);
	ENGINE_free(toadd);
	ERR_clear_error();
	}
#endif

/* This is a process-global DSO handle used for loading and unloading
 * the UBSEC library. NB: This is only set (or unset) during an
 * init() or finish() call (reference counts permitting) and they're
 * operating with global locks, so this should be thread-safe
 * implicitly. */

static DSO *ubsec_dso = NULL;

/* These are the function pointers that are (un)set when the library has
 * successfully (un)loaded. */

static t_UBSEC_ubsec_bytes_to_bits *p_UBSEC_ubsec_bytes_to_bits = NULL;
static t_UBSEC_ubsec_bits_to_bytes *p_UBSEC_ubsec_bits_to_bytes = NULL;
static t_UBSEC_ubsec_open *p_UBSEC_ubsec_open = NULL;
static t_UBSEC_ubsec_close *p_UBSEC_ubsec_close = NULL;
#ifndef OPENSSL_NO_DH
static t_UBSEC_diffie_hellman_generate_ioctl 
	*p_UBSEC_diffie_hellman_generate_ioctl = NULL;
static t_UBSEC_diffie_hellman_agree_ioctl *p_UBSEC_diffie_hellman_agree_ioctl = NULL;
#endif
#ifndef OPENSSL_NO_RSA
static t_UBSEC_rsa_mod_exp_ioctl *p_UBSEC_rsa_mod_exp_ioctl = NULL;
static t_UBSEC_rsa_mod_exp_crt_ioctl *p_UBSEC_rsa_mod_exp_crt_ioctl = NULL;
#endif
#ifndef OPENSSL_NO_DSA
static t_UBSEC_dsa_sign_ioctl *p_UBSEC_dsa_sign_ioctl = NULL;
static t_UBSEC_dsa_verify_ioctl *p_UBSEC_dsa_verify_ioctl = NULL;
#endif
static t_UBSEC_math_accelerate_ioctl *p_UBSEC_math_accelerate_ioctl = NULL;
static t_UBSEC_rng_ioctl *p_UBSEC_rng_ioctl = NULL;
static t_UBSEC_max_key_len_ioctl *p_UBSEC_max_key_len_ioctl = NULL;

static int max_key_len = 1024;  /* ??? */

/* 
 * These are the static string constants for the DSO file name and the function
 * symbol names to bind to. 
 */

static const char *UBSEC_LIBNAME = NULL;
static const char *get_UBSEC_LIBNAME(void)
	{
	if(UBSEC_LIBNAME)
		return UBSEC_LIBNAME;
	return "ubsec";
	}
static void free_UBSEC_LIBNAME(void)
	{
	if(UBSEC_LIBNAME)
		OPENSSL_free((void*)UBSEC_LIBNAME);
	UBSEC_LIBNAME = NULL;
	}
static long set_UBSEC_LIBNAME(const char *name)
	{
	free_UBSEC_LIBNAME();
	return (((UBSEC_LIBNAME = BUF_strdup(name)) != NULL) ? 1 : 0);
	}
static const char *UBSEC_F1 = "ubsec_bytes_to_bits";
static const char *UBSEC_F2 = "ubsec_bits_to_bytes";
static const char *UBSEC_F3 = "ubsec_open";
static const char *UBSEC_F4 = "ubsec_close";
#ifndef OPENSSL_NO_DH
static const char *UBSEC_F5 = "diffie_hellman_generate_ioctl";
static const char *UBSEC_F6 = "diffie_hellman_agree_ioctl";
#endif
/* #ifndef OPENSSL_NO_RSA */
static const char *UBSEC_F7 = "rsa_mod_exp_ioctl";
static const char *UBSEC_F8 = "rsa_mod_exp_crt_ioctl";
/* #endif */
#ifndef OPENSSL_NO_DSA
static const char *UBSEC_F9 = "dsa_sign_ioctl";
static const char *UBSEC_F10 = "dsa_verify_ioctl";
#endif
static const char *UBSEC_F11 = "math_accelerate_ioctl";
static const char *UBSEC_F12 = "rng_ioctl";
static const char *UBSEC_F13 = "ubsec_max_key_len_ioctl";

/* Destructor (complements the "ENGINE_ubsec()" constructor) */
static int ubsec_destroy(ENGINE *e)
	{
	free_UBSEC_LIBNAME();
	ERR_unload_UBSEC_strings();
	return 1;
	}

/* (de)initialisation functions. */
static int ubsec_init(ENGINE *e)
	{
	t_UBSEC_ubsec_bytes_to_bits *p1;
	t_UBSEC_ubsec_bits_to_bytes *p2;
	t_UBSEC_ubsec_open *p3;
	t_UBSEC_ubsec_close *p4;
#ifndef OPENSSL_NO_DH
	t_UBSEC_diffie_hellman_generate_ioctl *p5;
	t_UBSEC_diffie_hellman_agree_ioctl *p6;
#endif
/* #ifndef OPENSSL_NO_RSA */
	t_UBSEC_rsa_mod_exp_ioctl *p7;
	t_UBSEC_rsa_mod_exp_crt_ioctl *p8;
/* #endif */
#ifndef OPENSSL_NO_DSA
	t_UBSEC_dsa_sign_ioctl *p9;
	t_UBSEC_dsa_verify_ioctl *p10;
#endif
	t_UBSEC_math_accelerate_ioctl *p11;
	t_UBSEC_rng_ioctl *p12;
        t_UBSEC_max_key_len_ioctl *p13;
	int fd = 0;

	if(ubsec_dso != NULL)
		{
		UBSECerr(UBSEC_F_UBSEC_INIT, UBSEC_R_ALREADY_LOADED);
		goto err;
		}
	/* 
	 * Attempt to load libubsec.so/ubsec.dll/whatever. 
	 */
	ubsec_dso = DSO_load(NULL, get_UBSEC_LIBNAME(), NULL, 0);
	if(ubsec_dso == NULL)
		{
		UBSECerr(UBSEC_F_UBSEC_INIT, UBSEC_R_DSO_FAILURE);
		goto err;
		}

	if (
	!(p1 = (t_UBSEC_ubsec_bytes_to_bits *) DSO_bind_func(ubsec_dso, UBSEC_F1)) ||
	!(p2 = (t_UBSEC_ubsec_bits_to_bytes *) DSO_bind_func(ubsec_dso, UBSEC_F2)) ||
	!(p3 = (t_UBSEC_ubsec_open *) DSO_bind_func(ubsec_dso, UBSEC_F3)) ||
	!(p4 = (t_UBSEC_ubsec_close *) DSO_bind_func(ubsec_dso, UBSEC_F4)) ||
#ifndef OPENSSL_NO_DH
	!(p5 = (t_UBSEC_diffie_hellman_generate_ioctl *) 
				DSO_bind_func(ubsec_dso, UBSEC_F5)) ||
	!(p6 = (t_UBSEC_diffie_hellman_agree_ioctl *) 
				DSO_bind_func(ubsec_dso, UBSEC_F6)) ||
#endif
/* #ifndef OPENSSL_NO_RSA */
	!(p7 = (t_UBSEC_rsa_mod_exp_ioctl *) DSO_bind_func(ubsec_dso, UBSEC_F7)) ||
	!(p8 = (t_UBSEC_rsa_mod_exp_crt_ioctl *) DSO_bind_func(ubsec_dso, UBSEC_F8)) ||
/* #endif */
#ifndef OPENSSL_NO_DSA
	!(p9 = (t_UBSEC_dsa_sign_ioctl *) DSO_bind_func(ubsec_dso, UBSEC_F9)) ||
	!(p10 = (t_UBSEC_dsa_verify_ioctl *) DSO_bind_func(ubsec_dso, UBSEC_F10)) ||
#endif
	!(p11 = (t_UBSEC_math_accelerate_ioctl *) 
				DSO_bind_func(ubsec_dso, UBSEC_F11)) ||
	!(p12 = (t_UBSEC_rng_ioctl *) DSO_bind_func(ubsec_dso, UBSEC_F12)) ||
        !(p13 = (t_UBSEC_max_key_len_ioctl *) DSO_bind_func(ubsec_dso, UBSEC_F13)))
		{
		UBSECerr(UBSEC_F_UBSEC_INIT, UBSEC_R_DSO_FAILURE);
		goto err;
		}

	/* Copy the pointers */
	p_UBSEC_ubsec_bytes_to_bits = p1;
	p_UBSEC_ubsec_bits_to_bytes = p2;
	p_UBSEC_ubsec_open = p3;
	p_UBSEC_ubsec_close = p4;
#ifndef OPENSSL_NO_DH
	p_UBSEC_diffie_hellman_generate_ioctl = p5;
	p_UBSEC_diffie_hellman_agree_ioctl = p6;
#endif
#ifndef OPENSSL_NO_RSA
	p_UBSEC_rsa_mod_exp_ioctl = p7;
	p_UBSEC_rsa_mod_exp_crt_ioctl = p8;
#endif
#ifndef OPENSSL_NO_DSA
	p_UBSEC_dsa_sign_ioctl = p9;
	p_UBSEC_dsa_verify_ioctl = p10;
#endif
	p_UBSEC_math_accelerate_ioctl = p11;
	p_UBSEC_rng_ioctl = p12;
        p_UBSEC_max_key_len_ioctl = p13;

	/* Perform an open to see if there's actually any unit running. */
	if (((fd = p_UBSEC_ubsec_open(UBSEC_KEY_DEVICE_NAME)) > 0) && (p_UBSEC_max_key_len_ioctl(fd, &max_key_len) == 0))
	{
	   p_UBSEC_ubsec_close(fd);
	   return 1;
	}
	else
	{
	  UBSECerr(UBSEC_F_UBSEC_INIT, UBSEC_R_UNIT_FAILURE);
	}

err:
	if(ubsec_dso)
		DSO_free(ubsec_dso);
	ubsec_dso = NULL;
	p_UBSEC_ubsec_bytes_to_bits = NULL;
	p_UBSEC_ubsec_bits_to_bytes = NULL;
	p_UBSEC_ubsec_open = NULL;
	p_UBSEC_ubsec_close = NULL;
#ifndef OPENSSL_NO_DH
	p_UBSEC_diffie_hellman_generate_ioctl = NULL;
	p_UBSEC_diffie_hellman_agree_ioctl = NULL;
#endif
#ifndef OPENSSL_NO_RSA
	p_UBSEC_rsa_mod_exp_ioctl = NULL;
	p_UBSEC_rsa_mod_exp_crt_ioctl = NULL;
#endif
#ifndef OPENSSL_NO_DSA
	p_UBSEC_dsa_sign_ioctl = NULL;
	p_UBSEC_dsa_verify_ioctl = NULL;
#endif
	p_UBSEC_math_accelerate_ioctl = NULL;
	p_UBSEC_rng_ioctl = NULL;
        p_UBSEC_max_key_len_ioctl = NULL;

	return 0;
	}

static int ubsec_finish(ENGINE *e)
	{
	free_UBSEC_LIBNAME();
	if(ubsec_dso == NULL)
		{
		UBSECerr(UBSEC_F_UBSEC_FINISH, UBSEC_R_NOT_LOADED);
		return 0;
		}
	if(!DSO_free(ubsec_dso))
		{
		UBSECerr(UBSEC_F_UBSEC_FINISH, UBSEC_R_DSO_FAILURE);
		return 0;
		}
	ubsec_dso = NULL;
	p_UBSEC_ubsec_bytes_to_bits = NULL;
	p_UBSEC_ubsec_bits_to_bytes = NULL;
	p_UBSEC_ubsec_open = NULL;
	p_UBSEC_ubsec_close = NULL;
#ifndef OPENSSL_NO_DH
	p_UBSEC_diffie_hellman_generate_ioctl = NULL;
	p_UBSEC_diffie_hellman_agree_ioctl = NULL;
#endif
#ifndef OPENSSL_NO_RSA
	p_UBSEC_rsa_mod_exp_ioctl = NULL;
	p_UBSEC_rsa_mod_exp_crt_ioctl = NULL;
#endif
#ifndef OPENSSL_NO_DSA
	p_UBSEC_dsa_sign_ioctl = NULL;
	p_UBSEC_dsa_verify_ioctl = NULL;
#endif
	p_UBSEC_math_accelerate_ioctl = NULL;
	p_UBSEC_rng_ioctl = NULL;
        p_UBSEC_max_key_len_ioctl = NULL;
	return 1;
	}

static int ubsec_ctrl(ENGINE *e, int cmd, long i, void *p, void (*f)(void))
	{
	int initialised = ((ubsec_dso == NULL) ? 0 : 1);
	switch(cmd)
		{
	case UBSEC_CMD_SO_PATH:
		if(p == NULL)
			{
			UBSECerr(UBSEC_F_UBSEC_CTRL,ERR_R_PASSED_NULL_PARAMETER);
			return 0;
			}
		if(initialised)
			{
			UBSECerr(UBSEC_F_UBSEC_CTRL,UBSEC_R_ALREADY_LOADED);
			return 0;
			}
		return set_UBSEC_LIBNAME((const char *)p);
	default:
		break;
		}
	UBSECerr(UBSEC_F_UBSEC_CTRL,UBSEC_R_CTRL_COMMAND_NOT_IMPLEMENTED);
	return 0;
	}

static int ubsec_mod_exp(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
		const BIGNUM *m, BN_CTX *ctx)
	{
	int 	y_len = 0;
	int 	fd;

	if(ubsec_dso == NULL)
	{
		UBSECerr(UBSEC_F_UBSEC_MOD_EXP, UBSEC_R_NOT_LOADED);
		return 0;
	}

	/* Check if hardware can't handle this argument. */
	y_len = BN_num_bits(m);
	if (y_len > max_key_len) {
		UBSECerr(UBSEC_F_UBSEC_MOD_EXP, UBSEC_R_SIZE_TOO_LARGE_OR_TOO_SMALL);
                return BN_mod_exp(r, a, p, m, ctx);
	} 

	if(!bn_wexpand(r, m->top))
	{
		UBSECerr(UBSEC_F_UBSEC_MOD_EXP, UBSEC_R_BN_EXPAND_FAIL);
		return 0;
	}

	if ((fd = p_UBSEC_ubsec_open(UBSEC_KEY_DEVICE_NAME)) <= 0) {
		fd = 0;
		UBSECerr(UBSEC_F_UBSEC_MOD_EXP, UBSEC_R_UNIT_FAILURE);
                return BN_mod_exp(r, a, p, m, ctx);
	}

	if (p_UBSEC_rsa_mod_exp_ioctl(fd, (unsigned char *)a->d, BN_num_bits(a),
		(unsigned char *)m->d, BN_num_bits(m), (unsigned char *)p->d, 
		BN_num_bits(p), (unsigned char *)r->d, &y_len) != 0)
	{
		UBSECerr(UBSEC_F_UBSEC_MOD_EXP, UBSEC_R_REQUEST_FAILED);
                p_UBSEC_ubsec_close(fd);

                return BN_mod_exp(r, a, p, m, ctx);
	}

	p_UBSEC_ubsec_close(fd);

	r->top = (BN_num_bits(m)+BN_BITS2-1)/BN_BITS2;
	return 1;
	}

#ifndef OPENSSL_NO_RSA
static int ubsec_rsa_mod_exp(BIGNUM *r0, const BIGNUM *I, RSA *rsa, BN_CTX *ctx)
	{
	int to_return = 0;

	if(!rsa->p || !rsa->q || !rsa->dmp1 || !rsa->dmq1 || !rsa->iqmp)
		{
		UBSECerr(UBSEC_F_UBSEC_RSA_MOD_EXP, UBSEC_R_MISSING_KEY_COMPONENTS);
		goto err;
		}

	to_return = ubsec_mod_exp_crt(r0, I, rsa->p, rsa->q, rsa->dmp1,
		    rsa->dmq1, rsa->iqmp, ctx);
	if (to_return == FAIL_TO_SOFTWARE)
	{
	  /*
	   * Do in software as hardware failed.
	   */
	   const RSA_METHOD *meth = RSA_PKCS1_SSLeay();
	   to_return = (*meth->rsa_mod_exp)(r0, I, rsa, ctx);
	}
err:
	return to_return;
	}

static int ubsec_mod_exp_crt(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
			const BIGNUM *q, const BIGNUM *dp,
			const BIGNUM *dq, const BIGNUM *qinv, BN_CTX *ctx)
	{
	int	y_len,
		fd;

	y_len = BN_num_bits(p) + BN_num_bits(q);

	/* Check if hardware can't handle this argument. */
	if (y_len > max_key_len) {
		UBSECerr(UBSEC_F_UBSEC_MOD_EXP_CRT, UBSEC_R_SIZE_TOO_LARGE_OR_TOO_SMALL);
		return FAIL_TO_SOFTWARE;
	} 

	if (!bn_wexpand(r, p->top + q->top + 1)) {
		UBSECerr(UBSEC_F_UBSEC_MOD_EXP_CRT, UBSEC_R_BN_EXPAND_FAIL);
		return 0;
	}

	if ((fd = p_UBSEC_ubsec_open(UBSEC_KEY_DEVICE_NAME)) <= 0) {
		fd = 0;
		UBSECerr(UBSEC_F_UBSEC_MOD_EXP_CRT, UBSEC_R_UNIT_FAILURE);
		return FAIL_TO_SOFTWARE;
	}

	if (p_UBSEC_rsa_mod_exp_crt_ioctl(fd,
		(unsigned char *)a->d, BN_num_bits(a), 
		(unsigned char *)qinv->d, BN_num_bits(qinv),
		(unsigned char *)dp->d, BN_num_bits(dp),
		(unsigned char *)p->d, BN_num_bits(p),
		(unsigned char *)dq->d, BN_num_bits(dq),
		(unsigned char *)q->d, BN_num_bits(q),
		(unsigned char *)r->d,  &y_len) != 0) {
		UBSECerr(UBSEC_F_UBSEC_MOD_EXP_CRT, UBSEC_R_REQUEST_FAILED);
                p_UBSEC_ubsec_close(fd);
		return FAIL_TO_SOFTWARE;
	}

	p_UBSEC_ubsec_close(fd);

	r->top = (BN_num_bits(p) + BN_num_bits(q) + BN_BITS2 - 1)/BN_BITS2;
	return 1;
}
#endif

#ifndef OPENSSL_NO_DSA
#ifdef NOT_USED
static int ubsec_dsa_mod_exp(DSA *dsa, BIGNUM *rr, BIGNUM *a1,
		BIGNUM *p1, BIGNUM *a2, BIGNUM *p2, BIGNUM *m,
		BN_CTX *ctx, BN_MONT_CTX *in_mont)
	{
	BIGNUM t;
	int to_return = 0;
 
	BN_init(&t);
	/* let rr = a1 ^ p1 mod m */
	if (!ubsec_mod_exp(rr,a1,p1,m,ctx)) goto end;
	/* let t = a2 ^ p2 mod m */
	if (!ubsec_mod_exp(&t,a2,p2,m,ctx)) goto end;
	/* let rr = rr * t mod m */
	if (!BN_mod_mul(rr,rr,&t,m,ctx)) goto end;
	to_return = 1;
end:
	BN_free(&t);
	return to_return;
	}

static int ubsec_mod_exp_dsa(DSA *dsa, BIGNUM *r, BIGNUM *a,
		const BIGNUM *p, const BIGNUM *m, BN_CTX *ctx,
		BN_MONT_CTX *m_ctx)
	{
	return ubsec_mod_exp(r, a, p, m, ctx);
	}
#endif
#endif

#ifndef OPENSSL_NO_RSA

/*
 * This function is aliased to mod_exp (with the mont stuff dropped).
 */
static int ubsec_mod_exp_mont(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
		const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *m_ctx)
        {
	int ret = 0;

 	/* Do in software if the key is too large for the hardware. */
	if (BN_num_bits(m) > max_key_len)
                {
		const RSA_METHOD *meth = RSA_PKCS1_SSLeay();
		ret = (*meth->bn_mod_exp)(r, a, p, m, ctx, m_ctx);
                }
        else
                {
		ret = ubsec_mod_exp(r, a, p, m, ctx);
                }
	
	return ret;
        }
#endif

#ifndef OPENSSL_NO_DH
/* This function is aliased to mod_exp (with the dh and mont dropped). */
static int ubsec_mod_exp_dh(const DH *dh, BIGNUM *r, const BIGNUM *a,
		const BIGNUM *p, const BIGNUM *m, BN_CTX *ctx,
		BN_MONT_CTX *m_ctx)
	{
	return ubsec_mod_exp(r, a, p, m, ctx);
	}
#endif

#ifndef OPENSSL_NO_DSA
static DSA_SIG *ubsec_dsa_do_sign(const unsigned char *dgst, int dlen, DSA *dsa)
	{
	DSA_SIG *to_return = NULL;
	int s_len = 160, r_len = 160, d_len, fd;
	BIGNUM m, *r=NULL, *s=NULL;

	BN_init(&m);

	s = BN_new();
	r = BN_new();
	if ((s == NULL) || (r==NULL))
		goto err;

	d_len = p_UBSEC_ubsec_bytes_to_bits((unsigned char *)dgst, dlen);

        if(!bn_wexpand(r, (160+BN_BITS2-1)/BN_BITS2) ||
       	   (!bn_wexpand(s, (160+BN_BITS2-1)/BN_BITS2))) {
		UBSECerr(UBSEC_F_UBSEC_DSA_DO_SIGN, UBSEC_R_BN_EXPAND_FAIL);
		goto err;
	}

	if (BN_bin2bn(dgst,dlen,&m) == NULL) {
		UBSECerr(UBSEC_F_UBSEC_DSA_DO_SIGN, UBSEC_R_BN_EXPAND_FAIL);
		goto err;
	} 

	if ((fd = p_UBSEC_ubsec_open(UBSEC_KEY_DEVICE_NAME)) <= 0) {
                const DSA_METHOD *meth;
		fd = 0;
		UBSECerr(UBSEC_F_UBSEC_DSA_DO_SIGN, UBSEC_R_UNIT_FAILURE);
                meth = DSA_OpenSSL();
                to_return =  meth->dsa_do_sign(dgst, dlen, dsa);
		goto err;
	}

	if (p_UBSEC_dsa_sign_ioctl(fd, 0, /* compute hash before signing */
		(unsigned char *)dgst, d_len,
		NULL, 0,  /* compute random value */
		(unsigned char *)dsa->p->d, BN_num_bits(dsa->p), 
		(unsigned char *)dsa->q->d, BN_num_bits(dsa->q),
		(unsigned char *)dsa->g->d, BN_num_bits(dsa->g),
		(unsigned char *)dsa->priv_key->d, BN_num_bits(dsa->priv_key),
		(unsigned char *)r->d, &r_len,
		(unsigned char *)s->d, &s_len ) != 0) {
                const DSA_METHOD *meth;

		UBSECerr(UBSEC_F_UBSEC_DSA_DO_SIGN, UBSEC_R_REQUEST_FAILED);
                p_UBSEC_ubsec_close(fd);
                meth = DSA_OpenSSL();
                to_return = meth->dsa_do_sign(dgst, dlen, dsa);

		goto err;
	}

	p_UBSEC_ubsec_close(fd);

	r->top = (160+BN_BITS2-1)/BN_BITS2;
	s->top = (160+BN_BITS2-1)/BN_BITS2;

	to_return = DSA_SIG_new();
	if(to_return == NULL) {
		UBSECerr(UBSEC_F_UBSEC_DSA_DO_SIGN, UBSEC_R_BN_EXPAND_FAIL);
		goto err;
	}

	to_return->r = r;
	to_return->s = s;

err:
	if (!to_return) {
		if (r) BN_free(r);
		if (s) BN_free(s);
	}                                 
	BN_clear_free(&m);
	return to_return;
}

static int ubsec_dsa_verify(const unsigned char *dgst, int dgst_len,
                                DSA_SIG *sig, DSA *dsa)
	{
	int v_len, d_len;
	int to_return = 0;
	int fd;
	BIGNUM v, *pv = &v;

	BN_init(&v);

	if(!bn_wexpand(pv, dsa->p->top)) {
		UBSECerr(UBSEC_F_UBSEC_DSA_VERIFY, UBSEC_R_BN_EXPAND_FAIL);
		goto err;
	}

	v_len = BN_num_bits(dsa->p);

	d_len = p_UBSEC_ubsec_bytes_to_bits((unsigned char *)dgst, dgst_len);

	if ((fd = p_UBSEC_ubsec_open(UBSEC_KEY_DEVICE_NAME)) <= 0) {
                const DSA_METHOD *meth;
		fd = 0;
		UBSECerr(UBSEC_F_UBSEC_DSA_VERIFY, UBSEC_R_UNIT_FAILURE);
                meth = DSA_OpenSSL();
                to_return = meth->dsa_do_verify(dgst, dgst_len, sig, dsa);
		goto err;
	}

	if (p_UBSEC_dsa_verify_ioctl(fd, 0, /* compute hash before signing */
		(unsigned char *)dgst, d_len,
		(unsigned char *)dsa->p->d, BN_num_bits(dsa->p), 
		(unsigned char *)dsa->q->d, BN_num_bits(dsa->q),
		(unsigned char *)dsa->g->d, BN_num_bits(dsa->g),
		(unsigned char *)dsa->pub_key->d, BN_num_bits(dsa->pub_key),
		(unsigned char *)sig->r->d, BN_num_bits(sig->r),
		(unsigned char *)sig->s->d, BN_num_bits(sig->s),
		(unsigned char *)v.d, &v_len) != 0) {
                const DSA_METHOD *meth;
		UBSECerr(UBSEC_F_UBSEC_DSA_VERIFY, UBSEC_R_REQUEST_FAILED);
                p_UBSEC_ubsec_close(fd);

                meth = DSA_OpenSSL();
                to_return = meth->dsa_do_verify(dgst, dgst_len, sig, dsa);

		goto err;
	}

	p_UBSEC_ubsec_close(fd);

	to_return = 1;
err:
	BN_clear_free(&v);
	return to_return;
	}
#endif

#ifndef OPENSSL_NO_DH
static int ubsec_dh_compute_key(unsigned char *key,const BIGNUM *pub_key,DH *dh)
        {
        int      ret      = -1,
                 k_len,
                 fd;

        k_len = BN_num_bits(dh->p);

        if ((fd = p_UBSEC_ubsec_open(UBSEC_KEY_DEVICE_NAME)) <= 0)
                {
                const DH_METHOD *meth;
                UBSECerr(UBSEC_F_UBSEC_DH_COMPUTE_KEY, UBSEC_R_UNIT_FAILURE);
                meth = DH_OpenSSL();
                ret = meth->compute_key(key, pub_key, dh);
                goto err;
                }

        if (p_UBSEC_diffie_hellman_agree_ioctl(fd,
                                               (unsigned char *)dh->priv_key->d, BN_num_bits(dh->priv_key),
                                               (unsigned char *)pub_key->d, BN_num_bits(pub_key),
                                               (unsigned char *)dh->p->d, BN_num_bits(dh->p),
                                               key, &k_len) != 0)
                {
                /* Hardware's a no go, failover to software */
                const DH_METHOD *meth;
                UBSECerr(UBSEC_F_UBSEC_DH_COMPUTE_KEY, UBSEC_R_REQUEST_FAILED);
                p_UBSEC_ubsec_close(fd);

                meth = DH_OpenSSL();
                ret = meth->compute_key(key, pub_key, dh);

                goto err;
                }

        p_UBSEC_ubsec_close(fd);

        ret = p_UBSEC_ubsec_bits_to_bytes(k_len);
err:
        return ret;
        }

static int ubsec_dh_generate_key(DH *dh)
        {
        int      ret               = 0,
                 random_bits       = 0,
                 pub_key_len       = 0,
                 priv_key_len      = 0,
                 fd;
        BIGNUM   *pub_key          = NULL;
        BIGNUM   *priv_key         = NULL;

        /* 
         *  How many bits should Random x be? dh_key.c
         *  sets the range from 0 to num_bits(modulus) ???
         */

        if (dh->priv_key == NULL)
                {
                priv_key = BN_new();
                if (priv_key == NULL) goto err;
                priv_key_len = BN_num_bits(dh->p);
                if(bn_wexpand(priv_key, dh->p->top) == NULL) goto err;
                do
                        if (!BN_rand_range(priv_key, dh->p)) goto err;
                while (BN_is_zero(priv_key));
                random_bits = BN_num_bits(priv_key);
                }
        else
                {
                priv_key = dh->priv_key;
                }

        if (dh->pub_key == NULL)
                {
                pub_key = BN_new();
                pub_key_len = BN_num_bits(dh->p);
                if(bn_wexpand(pub_key, dh->p->top) == NULL) goto err;
                if(pub_key == NULL) goto err;
                }
        else
                {
                pub_key = dh->pub_key;
                }

        if ((fd = p_UBSEC_ubsec_open(UBSEC_KEY_DEVICE_NAME)) <= 0)
                {
                const DH_METHOD *meth;
                UBSECerr(UBSEC_F_UBSEC_DH_GENERATE_KEY, UBSEC_R_UNIT_FAILURE);
                meth = DH_OpenSSL();
                ret = meth->generate_key(dh);
                goto err;
                }

        if (p_UBSEC_diffie_hellman_generate_ioctl(fd,
                                                  (unsigned char *)priv_key->d, &priv_key_len,
                                                  (unsigned char *)pub_key->d,  &pub_key_len,
                                                  (unsigned char *)dh->g->d, BN_num_bits(dh->g),
                                                  (unsigned char *)dh->p->d, BN_num_bits(dh->p),
                                                  0, 0, random_bits) != 0)
                {
                /* Hardware's a no go, failover to software */
                const DH_METHOD *meth;

                UBSECerr(UBSEC_F_UBSEC_DH_GENERATE_KEY, UBSEC_R_REQUEST_FAILED);
                p_UBSEC_ubsec_close(fd);

                meth = DH_OpenSSL();
                ret = meth->generate_key(dh);

                goto err;
                }

        p_UBSEC_ubsec_close(fd);

        dh->pub_key = pub_key;
        dh->pub_key->top = (pub_key_len + BN_BITS2-1) / BN_BITS2;
        dh->priv_key = priv_key;
        dh->priv_key->top = (priv_key_len + BN_BITS2-1) / BN_BITS2;

        ret = 1;
err:
        return ret;
        }
#endif

#ifdef NOT_USED
static int ubsec_rand_bytes(unsigned char * buf,
                            int num)
        {
        int      ret      = 0,
                 fd;

        if ((fd = p_UBSEC_ubsec_open(UBSEC_KEY_DEVICE_NAME)) <= 0)
                {
                const RAND_METHOD *meth;
                UBSECerr(UBSEC_F_UBSEC_RAND_BYTES, UBSEC_R_UNIT_FAILURE);
                num = p_UBSEC_ubsec_bits_to_bytes(num);
                meth = RAND_SSLeay();
                meth->seed(buf, num);
                ret = meth->bytes(buf, num);
                goto err;
                }

        num *= 8; /* bytes to bits */

        if (p_UBSEC_rng_ioctl(fd,
                              UBSEC_RNG_DIRECT,
                              buf,
                              &num) != 0)
                {
                /* Hardware's a no go, failover to software */
                const RAND_METHOD *meth;

                UBSECerr(UBSEC_F_UBSEC_RAND_BYTES, UBSEC_R_REQUEST_FAILED);
                p_UBSEC_ubsec_close(fd);

                num = p_UBSEC_ubsec_bits_to_bytes(num);
                meth = RAND_SSLeay();
                meth->seed(buf, num);
                ret = meth->bytes(buf, num);

                goto err;
                }

        p_UBSEC_ubsec_close(fd);

        ret = 1;
err:
        return(ret);
        }


static int ubsec_rand_status(void)
	{
	return 0;
	}
#endif

/* This stuff is needed if this ENGINE is being compiled into a self-contained
 * shared-library. */
#ifndef OPENSSL_NO_DYNAMIC_ENGINE
static int bind_fn(ENGINE *e, const char *id)
	{
	if(id && (strcmp(id, engine_ubsec_id) != 0))
		return 0;
	if(!bind_helper(e))
		return 0;
	return 1;
	}
IMPLEMENT_DYNAMIC_CHECK_FN()
IMPLEMENT_DYNAMIC_BIND_FN(bind_fn)
#endif /* OPENSSL_NO_DYNAMIC_ENGINE */

#endif /* !OPENSSL_NO_HW_UBSEC */
#endif /* !OPENSSL_NO_HW */
