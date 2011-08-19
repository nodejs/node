/* crypto/engine/hw_zencod.c */
 /* Written by Fred Donnat (frederic.donnat@zencod.com) for "zencod"
 * engine integration in order to redirect crypto computing on a crypto
 * hardware accelerator zenssl32  ;-)
 *
 * Date : 25 jun 2002
 * Revision : 17 Ju7 2002
 * Version : zencod_engine-0.9.7
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


/* ENGINE general include */
#include <stdio.h>
#include <openssl/crypto.h>
#include <openssl/dso.h>
#include <openssl/engine.h>

#ifndef OPENSSL_NO_HW
#ifndef OPENSSL_NO_HW_ZENCOD

#ifdef FLAT_INC
#  include "hw_zencod.h"
#else
#  include "vendor_defns/hw_zencod.h"
#endif

#define ZENCOD_LIB_NAME "zencod engine"
#include "hw_zencod_err.c"

#define FAIL_TO_SOFTWARE		-15

#define	ZEN_LIBRARY	"zenbridge"

#if 0
#  define PERROR(s)	perror(s)
#  define CHEESE()	fputs("## [ZenEngine] ## " __FUNCTION__ "\n", stderr)
#else
#  define PERROR(s)
#  define CHEESE()
#endif


/* Sorry ;) */
#ifndef WIN32
static inline void esrever ( unsigned char *d, int l )
{
	for(;--l>0;--l,d++){*d^=*(d+l);*(d+l)^=*d;*d^=*(d+l);}
}

static inline void ypcmem ( unsigned char *d, const unsigned char *s, int l )
{
	for(d+=l;l--;)*--d=*s++;
}
#else
static __inline void esrever ( unsigned char *d, int l )
{
	for(;--l>0;--l,d++){*d^=*(d+l);*(d+l)^=*d;*d^=*(d+l);}
}

static __inline void ypcmem ( unsigned char *d, const unsigned char *s, int l )
{
	for(d+=l;l--;)*--d=*s++;
}
#endif


#define BIGNUM2ZEN(n, bn)	(ptr_zencod_init_number((n), \
					(unsigned long) ((bn)->top * BN_BITS2), \
					(unsigned char *) ((bn)->d)))

#define ZEN_BITS(n, bytes)	(ptr_zencod_bytes2bits((unsigned char *) (n), (unsigned long) (bytes)))
#define ZEN_BYTES(bits)	(ptr_zencod_bits2bytes((unsigned long) (bits)))


/* Function for ENGINE detection and control */
static int zencod_destroy ( ENGINE *e ) ;
static int zencod_init ( ENGINE *e ) ;
static int zencod_finish ( ENGINE *e ) ;
static int zencod_ctrl ( ENGINE *e, int cmd, long i, void *p, void (*f) () ) ;

/* BIGNUM stuff */
static int zencod_bn_mod_exp ( BIGNUM *r, const BIGNUM *a, const BIGNUM *p, const BIGNUM *m, BN_CTX *ctx ) ;

/* RSA stuff */
#ifndef OPENSSL_NO_RSA
static int RSA_zencod_rsa_mod_exp ( BIGNUM *r0, const BIGNUM *I, RSA *rsa ) ;
static int RSA_zencod_bn_mod_exp ( BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
		const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *m_ctx ) ;
#endif

/* DSA stuff */
#ifndef OPENSSL_NO_DSA
static int DSA_zencod_bn_mod_exp ( DSA *dsa, BIGNUM *r, BIGNUM *a, const BIGNUM *p, const BIGNUM *m, BN_CTX *ctx,
		BN_MONT_CTX *m_ctx ) ;

static DSA_SIG *DSA_zencod_do_sign ( const unsigned char *dgst, int dlen, DSA *dsa ) ;
static int DSA_zencod_do_verify ( const unsigned char *dgst, int dgst_len, DSA_SIG *sig,
		DSA *dsa ) ;
#endif

/* DH stuff */
#ifndef OPENSSL_NO_DH
static int DH_zencod_bn_mod_exp ( const DH *dh, BIGNUM *r, const BIGNUM *a, const BIGNUM *p, const BIGNUM *m, BN_CTX *ctx,
		BN_MONT_CTX *m_ctx ) ;
static int DH_zencod_generate_key ( DH *dh ) ;
static int DH_zencod_compute_key ( unsigned char *key, const BIGNUM *pub_key, DH *dh ) ;
#endif

/* Rand stuff */
static void RAND_zencod_seed ( const void *buf, int num ) ;
static int RAND_zencod_rand_bytes ( unsigned char *buf, int num ) ;
static int RAND_zencod_rand_status ( void ) ;

/* Digest Stuff */
static int engine_digests ( ENGINE *e, const EVP_MD **digest, const int **nids, int nid ) ;

/* Cipher Stuff */
static int engine_ciphers ( ENGINE *e, const EVP_CIPHER **cipher, const int **nids, int nid ) ;


#define ZENCOD_CMD_SO_PATH			ENGINE_CMD_BASE
static const ENGINE_CMD_DEFN zencod_cmd_defns [ ] =
{
	{ ZENCOD_CMD_SO_PATH,
	  "SO_PATH",
	  "Specifies the path to the 'zenbridge' shared library",
	  ENGINE_CMD_FLAG_STRING},
	{ 0, NULL, NULL, 0 }
} ;


#ifndef OPENSSL_NO_RSA
/* Our internal RSA_METHOD specific to zencod ENGINE providing pointers to our function */
static RSA_METHOD zencod_rsa =
{
	"ZENCOD RSA method",
	NULL,
	NULL,
	NULL,
	NULL,
	RSA_zencod_rsa_mod_exp,
	RSA_zencod_bn_mod_exp,
	NULL,
	NULL,
	0,
	NULL,
	NULL,
	NULL
} ;
#endif

#ifndef OPENSSL_NO_DSA
/* Our internal DSA_METHOD specific to zencod ENGINE providing pointers to our function */
static DSA_METHOD zencod_dsa =
{
	"ZENCOD DSA method",
	DSA_zencod_do_sign,
	NULL,
	DSA_zencod_do_verify,
	NULL,
	DSA_zencod_bn_mod_exp,
	NULL,
	NULL,
	0,
	NULL
} ;
#endif

#ifndef OPENSSL_NO_DH
/* Our internal DH_METHOD specific to zencod ENGINE providing pointers to our function */
static DH_METHOD zencod_dh =
{
	"ZENCOD DH method",
	DH_zencod_generate_key,
	DH_zencod_compute_key,
	DH_zencod_bn_mod_exp,
	NULL,
	NULL,
	0,
	NULL
} ;
#endif

/* Our internal RAND_meth specific to zencod ZNGINE providing pointers to  our function */
static RAND_METHOD zencod_rand =
{
	RAND_zencod_seed,
	RAND_zencod_rand_bytes,
	NULL,
	NULL,
	RAND_zencod_rand_bytes,
	RAND_zencod_rand_status
} ;


/* Constants used when creating the ENGINE */
static const char *engine_zencod_id = "zencod";
static const char *engine_zencod_name = "ZENCOD hardware engine support";


/* This internal function is used by ENGINE_zencod () and possibly by the
 * "dynamic" ENGINE support too   ;-)
 */
static int bind_helper ( ENGINE *e )
{

#ifndef OPENSSL_NO_RSA
	const RSA_METHOD *meth_rsa ;
#endif
#ifndef OPENSSL_NO_DSA
	const DSA_METHOD *meth_dsa ;
#endif
#ifndef OPENSSL_NO_DH
	const DH_METHOD *meth_dh ;
#endif

	const RAND_METHOD *meth_rand ;


	if ( !ENGINE_set_id ( e, engine_zencod_id ) ||
			!ENGINE_set_name ( e, engine_zencod_name ) ||
#ifndef OPENSSL_NO_RSA
			!ENGINE_set_RSA ( e, &zencod_rsa ) ||
#endif
#ifndef OPENSSL_NO_DSA
			!ENGINE_set_DSA ( e, &zencod_dsa ) ||
#endif
#ifndef OPENSSL_NO_DH
			!ENGINE_set_DH ( e, &zencod_dh ) ||
#endif
			!ENGINE_set_RAND ( e, &zencod_rand ) ||

			!ENGINE_set_destroy_function ( e, zencod_destroy ) ||
			!ENGINE_set_init_function ( e, zencod_init ) ||
			!ENGINE_set_finish_function ( e, zencod_finish ) ||
			!ENGINE_set_ctrl_function ( e, zencod_ctrl ) ||
			!ENGINE_set_cmd_defns ( e, zencod_cmd_defns ) ||
			!ENGINE_set_digests ( e, engine_digests ) ||
			!ENGINE_set_ciphers ( e, engine_ciphers ) ) {
		return 0 ;
	}

#ifndef OPENSSL_NO_RSA
	/* We know that the "PKCS1_SSLeay()" functions hook properly
	 * to the Zencod-specific mod_exp and mod_exp_crt so we use
	 * those functions. NB: We don't use ENGINE_openssl() or
	 * anything "more generic" because something like the RSAref
	 * code may not hook properly, and if you own one of these
	 * cards then you have the right to do RSA operations on it
	 * anyway!
	 */
	meth_rsa = RSA_PKCS1_SSLeay () ;

	zencod_rsa.rsa_pub_enc = meth_rsa->rsa_pub_enc ;
	zencod_rsa.rsa_pub_dec = meth_rsa->rsa_pub_dec ;
	zencod_rsa.rsa_priv_enc = meth_rsa->rsa_priv_enc ;
	zencod_rsa.rsa_priv_dec = meth_rsa->rsa_priv_dec ;
	/* meth_rsa->rsa_mod_exp */
	/* meth_rsa->bn_mod_exp */
	zencod_rsa.init = meth_rsa->init ;
	zencod_rsa.finish = meth_rsa->finish ;
#endif

#ifndef OPENSSL_NO_DSA
	/* We use OpenSSL meth to supply what we don't provide ;-*)
	 */
	meth_dsa = DSA_OpenSSL () ;

	/* meth_dsa->dsa_do_sign */
	zencod_dsa.dsa_sign_setup = meth_dsa->dsa_sign_setup ;
	/* meth_dsa->dsa_do_verify */
	zencod_dsa.dsa_mod_exp = meth_dsa->dsa_mod_exp ;
	/* zencod_dsa.bn_mod_exp = meth_dsa->bn_mod_exp ; */
	zencod_dsa.init = meth_dsa->init ;
	zencod_dsa.finish = meth_dsa->finish ;
#endif

#ifndef OPENSSL_NO_DH
	/* We use OpenSSL meth to supply what we don't provide ;-*)
	 */
	meth_dh = DH_OpenSSL () ;

	/* zencod_dh.generate_key = meth_dh->generate_key ; */
	/* zencod_dh.compute_key = meth_dh->compute_key ; */
	/* zencod_dh.bn_mod_exp = meth_dh->bn_mod_exp ; */
	zencod_dh.init = meth_dh->init ;
	zencod_dh.finish = meth_dh->finish ;

#endif

	/* We use OpenSSL (SSLeay) meth to supply what we don't provide ;-*)
	 */
	meth_rand = RAND_SSLeay () ;

	/* meth_rand->seed ; */
	/* zencod_rand.seed = meth_rand->seed ; */
	/* meth_rand->bytes ; */
	/* zencod_rand.bytes = meth_rand->bytes ; */
	zencod_rand.cleanup = meth_rand->cleanup ;
	zencod_rand.add = meth_rand->add ;
	/* meth_rand->pseudorand ; */
	/* zencod_rand.pseudorand = meth_rand->pseudorand ; */
	/* zencod_rand.status = meth_rand->status ; */
	/* meth_rand->status ; */

	/* Ensure the zencod error handling is set up */
	ERR_load_ZENCOD_strings () ;
	return 1 ;
}


/* As this is only ever called once, there's no need for locking
 * (indeed - the lock will already be held by our caller!!!)
 */
static ENGINE *ENGINE_zencod ( void )
{

	ENGINE *eng = ENGINE_new () ;

	if ( !eng ) {
		return NULL ;
	}
	if ( !bind_helper ( eng ) ) {
		ENGINE_free ( eng ) ;
		return NULL ;
	}

	return eng ;
}


#ifdef ENGINE_DYNAMIC_SUPPORT
static
#endif
void ENGINE_load_zencod ( void )
{
	/* Copied from eng_[openssl|dyn].c */
	ENGINE *toadd = ENGINE_zencod ( ) ;
	if ( !toadd ) return ;
	ENGINE_add ( toadd ) ;
	ENGINE_free ( toadd ) ;
	ERR_clear_error ( ) ;
}


/* This is a process-global DSO handle used for loading and unloading
 * the ZENBRIDGE library.
 * NB: This is only set (or unset) during an * init () or finish () call
 * (reference counts permitting) and they're  * operating with global locks,
 * so this should be thread-safe * implicitly.
 */
static DSO *zencod_dso = NULL ;

static t_zencod_test *ptr_zencod_test = NULL ;
static t_zencod_bytes2bits *ptr_zencod_bytes2bits = NULL ;
static t_zencod_bits2bytes *ptr_zencod_bits2bytes = NULL ;
static t_zencod_new_number *ptr_zencod_new_number = NULL ;
static t_zencod_init_number *ptr_zencod_init_number = NULL ;

static t_zencod_rsa_mod_exp *ptr_zencod_rsa_mod_exp = NULL ;
static t_zencod_rsa_mod_exp_crt *ptr_zencod_rsa_mod_exp_crt = NULL ;
static t_zencod_dsa_do_sign *ptr_zencod_dsa_do_sign = NULL ;
static t_zencod_dsa_do_verify *ptr_zencod_dsa_do_verify = NULL ;
static t_zencod_dh_generate_key *ptr_zencod_dh_generate_key = NULL ;
static t_zencod_dh_compute_key *ptr_zencod_dh_compute_key = NULL ;
static t_zencod_rand_bytes *ptr_zencod_rand_bytes = NULL ;
static t_zencod_math_mod_exp *ptr_zencod_math_mod_exp = NULL ;

static t_zencod_md5_init *ptr_zencod_md5_init = NULL ;
static t_zencod_md5_update *ptr_zencod_md5_update = NULL ;
static t_zencod_md5_do_final *ptr_zencod_md5_do_final = NULL ;
static t_zencod_sha1_init *ptr_zencod_sha1_init = NULL ;
static t_zencod_sha1_update *ptr_zencod_sha1_update = NULL ;
static t_zencod_sha1_do_final *ptr_zencod_sha1_do_final = NULL ;

static t_zencod_xdes_cipher *ptr_zencod_xdes_cipher = NULL ;
static t_zencod_rc4_cipher *ptr_zencod_rc4_cipher = NULL ;

/* These are the static string constants for the DSO file name and the function
 * symbol names to bind to.
 */
static const char *ZENCOD_LIBNAME = ZEN_LIBRARY ;

static const char *ZENCOD_Fct_0 = "test_device" ;
static const char *ZENCOD_Fct_1 = "zenbridge_bytes2bits" ;
static const char *ZENCOD_Fct_2 = "zenbridge_bits2bytes" ;
static const char *ZENCOD_Fct_3 = "zenbridge_new_number" ;
static const char *ZENCOD_Fct_4 = "zenbridge_init_number" ;

static const char *ZENCOD_Fct_exp_1 = "zenbridge_rsa_mod_exp" ;
static const char *ZENCOD_Fct_exp_2 = "zenbridge_rsa_mod_exp_crt" ;
static const char *ZENCOD_Fct_dsa_1 = "zenbridge_dsa_do_sign" ;
static const char *ZENCOD_Fct_dsa_2 = "zenbridge_dsa_do_verify" ;
static const char *ZENCOD_Fct_dh_1 = "zenbridge_dh_generate_key" ;
static const char *ZENCOD_Fct_dh_2 = "zenbridge_dh_compute_key" ;
static const char *ZENCOD_Fct_rand_1 = "zenbridge_rand_bytes" ;
static const char *ZENCOD_Fct_math_1 = "zenbridge_math_mod_exp" ;

static const char *ZENCOD_Fct_md5_1 = "zenbridge_md5_init" ;
static const char *ZENCOD_Fct_md5_2 = "zenbridge_md5_update" ;
static const char *ZENCOD_Fct_md5_3 = "zenbridge_md5_do_final" ;
static const char *ZENCOD_Fct_sha1_1 = "zenbridge_sha1_init" ;
static const char *ZENCOD_Fct_sha1_2 = "zenbridge_sha1_update" ;
static const char *ZENCOD_Fct_sha1_3 = "zenbridge_sha1_do_final" ;

static const char *ZENCOD_Fct_xdes_1 = "zenbridge_xdes_cipher" ;
static const char *ZENCOD_Fct_rc4_1 = "zenbridge_rc4_cipher" ;

/* Destructor (complements the "ENGINE_zencod ()" constructor)
 */
static int zencod_destroy (ENGINE *e )
{

	ERR_unload_ZENCOD_strings () ;

	return 1 ;
}


/* (de)initialisation functions. Control Function
 */
static int zencod_init ( ENGINE *e )
{

	t_zencod_test *ptr_0 ;
	t_zencod_bytes2bits *ptr_1 ;
	t_zencod_bits2bytes *ptr_2 ;
	t_zencod_new_number *ptr_3 ;
	t_zencod_init_number *ptr_4 ;
	t_zencod_rsa_mod_exp *ptr_exp_1 ;
	t_zencod_rsa_mod_exp_crt *ptr_exp_2 ;
	t_zencod_dsa_do_sign *ptr_dsa_1 ;
	t_zencod_dsa_do_verify *ptr_dsa_2 ;
	t_zencod_dh_generate_key *ptr_dh_1 ;
	t_zencod_dh_compute_key *ptr_dh_2 ;
	t_zencod_rand_bytes *ptr_rand_1 ;
	t_zencod_math_mod_exp *ptr_math_1 ;
	t_zencod_md5_init *ptr_md5_1 ;
	t_zencod_md5_update *ptr_md5_2 ;
	t_zencod_md5_do_final *ptr_md5_3 ;
	t_zencod_sha1_init *ptr_sha1_1 ;
	t_zencod_sha1_update *ptr_sha1_2 ;
	t_zencod_sha1_do_final *ptr_sha1_3 ;
	t_zencod_xdes_cipher *ptr_xdes_1 ;
	t_zencod_rc4_cipher *ptr_rc4_1 ;

	CHEESE () ;

	/*
	 * We Should add some tests for non NULL parameters or bad value !!
	 * Stuff to be done ...
	 */

	if ( zencod_dso != NULL ) {
		ZENCODerr ( ZENCOD_F_ZENCOD_INIT, ZENCOD_R_ALREADY_LOADED ) ;
		goto err ;
	}
	/* Trying to load the Library "cryptozen"
	 */
	zencod_dso = DSO_load ( NULL, ZENCOD_LIBNAME, NULL, 0 ) ;
	if ( zencod_dso == NULL ) {
		ZENCODerr ( ZENCOD_F_ZENCOD_INIT, ZENCOD_R_DSO_FAILURE ) ;
		goto err ;
	}

	/* Trying to load Function from the Library
	 */
	if ( ! ( ptr_1 = (t_zencod_bytes2bits*) DSO_bind_func ( zencod_dso, ZENCOD_Fct_1 ) ) ||
			! ( ptr_2 = (t_zencod_bits2bytes*) DSO_bind_func ( zencod_dso, ZENCOD_Fct_2 ) ) ||
			! ( ptr_3 = (t_zencod_new_number*) DSO_bind_func ( zencod_dso, ZENCOD_Fct_3 ) ) ||
			! ( ptr_4 = (t_zencod_init_number*) DSO_bind_func ( zencod_dso, ZENCOD_Fct_4 ) ) ||
			! ( ptr_exp_1 = (t_zencod_rsa_mod_exp*) DSO_bind_func ( zencod_dso, ZENCOD_Fct_exp_1 ) ) ||
			! ( ptr_exp_2 = (t_zencod_rsa_mod_exp_crt*) DSO_bind_func ( zencod_dso, ZENCOD_Fct_exp_2 ) ) ||
			! ( ptr_dsa_1 = (t_zencod_dsa_do_sign*) DSO_bind_func ( zencod_dso, ZENCOD_Fct_dsa_1 ) ) ||
			! ( ptr_dsa_2 = (t_zencod_dsa_do_verify*) DSO_bind_func ( zencod_dso, ZENCOD_Fct_dsa_2 ) ) ||
			! ( ptr_dh_1 = (t_zencod_dh_generate_key*) DSO_bind_func ( zencod_dso, ZENCOD_Fct_dh_1 ) ) ||
			! ( ptr_dh_2 = (t_zencod_dh_compute_key*) DSO_bind_func ( zencod_dso, ZENCOD_Fct_dh_2 ) ) ||
			! ( ptr_rand_1 = (t_zencod_rand_bytes*) DSO_bind_func ( zencod_dso, ZENCOD_Fct_rand_1 ) ) ||
			! ( ptr_math_1 = (t_zencod_math_mod_exp*) DSO_bind_func ( zencod_dso, ZENCOD_Fct_math_1 ) ) ||
			! ( ptr_0 = (t_zencod_test *) DSO_bind_func ( zencod_dso, ZENCOD_Fct_0 ) ) ||
			! ( ptr_md5_1 = (t_zencod_md5_init *) DSO_bind_func ( zencod_dso, ZENCOD_Fct_md5_1 ) ) ||
			! ( ptr_md5_2 = (t_zencod_md5_update *) DSO_bind_func ( zencod_dso, ZENCOD_Fct_md5_2 ) ) ||
			! ( ptr_md5_3 = (t_zencod_md5_do_final *) DSO_bind_func ( zencod_dso, ZENCOD_Fct_md5_3 ) ) ||
			! ( ptr_sha1_1 = (t_zencod_sha1_init *) DSO_bind_func ( zencod_dso, ZENCOD_Fct_sha1_1 ) ) ||
			! ( ptr_sha1_2 = (t_zencod_sha1_update *) DSO_bind_func ( zencod_dso, ZENCOD_Fct_sha1_2 ) ) ||
			! ( ptr_sha1_3 = (t_zencod_sha1_do_final *) DSO_bind_func ( zencod_dso, ZENCOD_Fct_sha1_3 ) ) ||
			! ( ptr_xdes_1 = (t_zencod_xdes_cipher *) DSO_bind_func ( zencod_dso, ZENCOD_Fct_xdes_1 ) ) ||
			! ( ptr_rc4_1 = (t_zencod_rc4_cipher *) DSO_bind_func ( zencod_dso, ZENCOD_Fct_rc4_1 ) ) ) {

		ZENCODerr ( ZENCOD_F_ZENCOD_INIT, ZENCOD_R_DSO_FAILURE ) ;
		goto err ;
	}

	/* The function from "cryptozen" Library have been correctly loaded so copy them
	 */
	ptr_zencod_test = ptr_0 ;
	ptr_zencod_bytes2bits = ptr_1 ;
	ptr_zencod_bits2bytes = ptr_2 ;
	ptr_zencod_new_number = ptr_3 ;
	ptr_zencod_init_number = ptr_4 ;
	ptr_zencod_rsa_mod_exp = ptr_exp_1 ;
	ptr_zencod_rsa_mod_exp_crt = ptr_exp_2 ;
	ptr_zencod_dsa_do_sign = ptr_dsa_1 ;
	ptr_zencod_dsa_do_verify = ptr_dsa_2 ;
	ptr_zencod_dh_generate_key = ptr_dh_1 ;
	ptr_zencod_dh_compute_key = ptr_dh_2 ;
	ptr_zencod_rand_bytes = ptr_rand_1 ;
	ptr_zencod_math_mod_exp = ptr_math_1 ;
	ptr_zencod_test = ptr_0 ;
	ptr_zencod_md5_init = ptr_md5_1 ;
	ptr_zencod_md5_update = ptr_md5_2 ;
	ptr_zencod_md5_do_final = ptr_md5_3 ;
	ptr_zencod_sha1_init = ptr_sha1_1 ;
	ptr_zencod_sha1_update = ptr_sha1_2 ;
	ptr_zencod_sha1_do_final = ptr_sha1_3 ;
	ptr_zencod_xdes_cipher = ptr_xdes_1 ;
	ptr_zencod_rc4_cipher = ptr_rc4_1 ;

	/* We should peform a test to see if there is actually any unit runnig on the system ...
	 * Even if the cryptozen library is loaded the module coul not be loaded on the system ...
	 * For now we may just open and close the device !!
	 */

	if ( ptr_zencod_test () != 0 ) {
		ZENCODerr ( ZENCOD_F_ZENCOD_INIT, ZENCOD_R_UNIT_FAILURE ) ;
		goto err ;
	}

	return 1 ;
err :
	if ( zencod_dso ) {
		DSO_free ( zencod_dso ) ;
	}
	zencod_dso = NULL ;
	ptr_zencod_bytes2bits = NULL ;
	ptr_zencod_bits2bytes = NULL ;
	ptr_zencod_new_number = NULL ;
	ptr_zencod_init_number = NULL ;
	ptr_zencod_rsa_mod_exp = NULL ;
	ptr_zencod_rsa_mod_exp_crt = NULL ;
	ptr_zencod_dsa_do_sign = NULL ;
	ptr_zencod_dsa_do_verify = NULL ;
	ptr_zencod_dh_generate_key = NULL ;
	ptr_zencod_dh_compute_key = NULL ;
	ptr_zencod_rand_bytes = NULL ;
	ptr_zencod_math_mod_exp = NULL ;
	ptr_zencod_test = NULL ;
	ptr_zencod_md5_init = NULL ;
	ptr_zencod_md5_update = NULL ;
	ptr_zencod_md5_do_final = NULL ;
	ptr_zencod_sha1_init = NULL ;
	ptr_zencod_sha1_update = NULL ;
	ptr_zencod_sha1_do_final = NULL ;
	ptr_zencod_xdes_cipher = NULL ;
	ptr_zencod_rc4_cipher = NULL ;

	return 0 ;
}


static int zencod_finish ( ENGINE *e )
{

	CHEESE () ;

	/*
	 * We Should add some tests for non NULL parameters or bad value !!
	 * Stuff to be done ...
	 */
	if ( zencod_dso == NULL ) {
		ZENCODerr ( ZENCOD_F_ZENCOD_FINISH, ZENCOD_R_NOT_LOADED ) ;
		return 0 ;
	}
	if ( !DSO_free ( zencod_dso ) ) {
		ZENCODerr ( ZENCOD_F_ZENCOD_FINISH, ZENCOD_R_DSO_FAILURE ) ;
		return 0 ;
	}

	zencod_dso = NULL ;

	ptr_zencod_bytes2bits = NULL ;
	ptr_zencod_bits2bytes = NULL ;
	ptr_zencod_new_number = NULL ;
	ptr_zencod_init_number = NULL ;
	ptr_zencod_rsa_mod_exp = NULL ;
	ptr_zencod_rsa_mod_exp_crt = NULL ;
	ptr_zencod_dsa_do_sign = NULL ;
	ptr_zencod_dsa_do_verify = NULL ;
	ptr_zencod_dh_generate_key = NULL ;
	ptr_zencod_dh_compute_key = NULL ;
	ptr_zencod_rand_bytes = NULL ;
	ptr_zencod_math_mod_exp = NULL ;
	ptr_zencod_test = NULL ;
	ptr_zencod_md5_init = NULL ;
	ptr_zencod_md5_update = NULL ;
	ptr_zencod_md5_do_final = NULL ;
	ptr_zencod_sha1_init = NULL ;
	ptr_zencod_sha1_update = NULL ;
	ptr_zencod_sha1_do_final = NULL ;
	ptr_zencod_xdes_cipher = NULL ;
	ptr_zencod_rc4_cipher = NULL ;

	return 1 ;
}


static int zencod_ctrl ( ENGINE *e, int cmd, long i, void *p, void (*f) () )
{

	int initialised = ( ( zencod_dso == NULL ) ? 0 : 1 ) ;

	CHEESE () ;

	/*
	 * We Should add some tests for non NULL parameters or bad value !!
	 * Stuff to be done ...
	 */
	switch ( cmd ) {
	case ZENCOD_CMD_SO_PATH :
		if ( p == NULL ) {
			ZENCODerr ( ZENCOD_F_ZENCOD_CTRL, ERR_R_PASSED_NULL_PARAMETER ) ;
			return 0 ;
		}
		if ( initialised ) {
			ZENCODerr ( ZENCOD_F_ZENCOD_CTRL, ZENCOD_R_ALREADY_LOADED ) ;
			return 0 ;
		}
		ZENCOD_LIBNAME = (const char *) p ;
		return 1 ;
	default :
		break ;
	}

	ZENCODerr ( ZENCOD_F_ZENCOD_CTRL, ZENCOD_R_CTRL_COMMAND_NOT_IMPLEMENTED ) ;

	return 0 ;
}


/* BIGNUM stuff Functions
 */
static int zencod_bn_mod_exp ( BIGNUM *r, const BIGNUM *a, const BIGNUM *p, const BIGNUM *m, BN_CTX *ctx )
{
	zen_nb_t y, x, e, n;
	int ret;

	CHEESE () ;

	if ( !zencod_dso ) {
		ENGINEerr(ZENCOD_F_ZENCOD_BN_MOD_EXP, ZENCOD_R_NOT_LOADED);
		return 0;
	}

	if ( !bn_wexpand(r, m->top + 1) ) {
		ENGINEerr(ZENCOD_F_ZENCOD_BN_MOD_EXP, ZENCOD_R_BN_EXPAND_FAIL);
		return 0;
	}

	memset(r->d, 0, BN_num_bytes(m));

	ptr_zencod_init_number ( &y, (r->dmax - 1) * sizeof (BN_ULONG) * 8, (unsigned char *) r->d ) ;
	BIGNUM2ZEN ( &x, a ) ;
	BIGNUM2ZEN ( &e, p ) ;
	BIGNUM2ZEN ( &n, m ) ;

	/* Must invert x and e parameter due to BN mod exp prototype ... */
	ret = ptr_zencod_math_mod_exp ( &y, &e, &x, &n ) ;

	if ( ret )  {
		PERROR("zenbridge_math_mod_exp");
		ENGINEerr(ZENCOD_F_ZENCOD_BN_MOD_EXP, ZENCOD_R_REQUEST_FAILED);
		return 0;
	}

	r->top = (BN_num_bits(m) + BN_BITS2 - 1) / BN_BITS2;

	return 1;
}


/* RSA stuff Functions
 */
#ifndef OPENSSL_NO_RSA
static int RSA_zencod_rsa_mod_exp ( BIGNUM *r0, const BIGNUM *i, RSA *rsa )
{

	CHEESE () ;

	if ( !zencod_dso ) {
		ENGINEerr(ZENCOD_F_ZENCOD_RSA_MOD_EXP_CRT, ZENCOD_R_NOT_LOADED);
		return 0;
	}

	if ( !rsa->p || !rsa->q || !rsa->dmp1 || !rsa->dmq1 || !rsa->iqmp ) {
		ENGINEerr(ZENCOD_F_ZENCOD_RSA_MOD_EXP_CRT, ZENCOD_R_BAD_KEY_COMPONENTS);
		return 0;
	}

	/* Do in software if argument is too large for hardware */
	if ( RSA_size(rsa) * 8 > ZENBRIDGE_MAX_KEYSIZE_RSA_CRT ) {
		const RSA_METHOD *meth;

		meth = RSA_PKCS1_SSLeay();
		return meth->rsa_mod_exp(r0, i, rsa);
	} else {
		zen_nb_t y, x, p, q, dmp1, dmq1, iqmp;

		if ( !bn_expand(r0, RSA_size(rsa) * 8) ) {
			ENGINEerr(ZENCOD_F_ZENCOD_RSA_MOD_EXP_CRT, ZENCOD_R_BN_EXPAND_FAIL);
			return 0;
		}
		r0->top = (RSA_size(rsa) * 8 + BN_BITS2 - 1) / BN_BITS2;

		BIGNUM2ZEN ( &x, i ) ;
		BIGNUM2ZEN ( &y, r0 ) ;
		BIGNUM2ZEN ( &p, rsa->p ) ;
		BIGNUM2ZEN ( &q, rsa->q ) ;
		BIGNUM2ZEN ( &dmp1, rsa->dmp1 ) ;
		BIGNUM2ZEN ( &dmq1, rsa->dmq1 ) ;
		BIGNUM2ZEN ( &iqmp, rsa->iqmp ) ;

		if ( ptr_zencod_rsa_mod_exp_crt ( &y, &x, &p, &q, &dmp1, &dmq1, &iqmp ) < 0 ) {
			PERROR("zenbridge_rsa_mod_exp_crt");
			ENGINEerr(ZENCOD_F_ZENCOD_RSA_MOD_EXP_CRT, ZENCOD_R_REQUEST_FAILED);
			return 0;
		}

		return 1;
	}
}


/* This function is aliased to RSA_mod_exp (with the mont stuff dropped).
 */
static int RSA_zencod_bn_mod_exp ( BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
		const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *m_ctx )
{

	CHEESE () ;

	if ( !zencod_dso ) {
		ENGINEerr(ZENCOD_F_ZENCOD_RSA_MOD_EXP, ZENCOD_R_NOT_LOADED);
		return 0;
	}

	/* Do in software if argument is too large for hardware */
	if ( BN_num_bits(m) > ZENBRIDGE_MAX_KEYSIZE_RSA ) {
		const RSA_METHOD *meth;

		meth = RSA_PKCS1_SSLeay();
		return meth->bn_mod_exp(r, a, p, m, ctx, m_ctx);
	} else {
		zen_nb_t y, x, e, n;

		if ( !bn_expand(r, BN_num_bits(m)) ) {
			ENGINEerr(ZENCOD_F_ZENCOD_RSA_MOD_EXP, ZENCOD_R_BN_EXPAND_FAIL);
			return 0;
		}
		r->top = (BN_num_bits(m) + BN_BITS2 - 1) / BN_BITS2;

		BIGNUM2ZEN ( &x, a ) ;
		BIGNUM2ZEN ( &y, r ) ;
		BIGNUM2ZEN ( &e, p ) ;
		BIGNUM2ZEN ( &n, m ) ;

		if ( ptr_zencod_rsa_mod_exp ( &y, &x, &n, &e ) < 0 ) {
			PERROR("zenbridge_rsa_mod_exp");
			ENGINEerr(ZENCOD_F_ZENCOD_RSA_MOD_EXP, ZENCOD_R_REQUEST_FAILED);
			return 0;
		}

		return 1;
	}
}
#endif /* !OPENSSL_NO_RSA */


#ifndef OPENSSL_NO_DSA
/* DSA stuff Functions
 */
static DSA_SIG *DSA_zencod_do_sign ( const unsigned char *dgst, int dlen, DSA *dsa )
{
	zen_nb_t p, q, g, x, y, r, s, data;
	DSA_SIG *sig;
	BIGNUM *bn_r = NULL;
	BIGNUM *bn_s = NULL;
	char msg[20];

	CHEESE();

	if ( !zencod_dso ) {
		ENGINEerr(ZENCOD_F_ZENCOD_DSA_DO_SIGN, ZENCOD_R_NOT_LOADED);
		goto FAILED;
	}

	if ( dlen > 160 ) {
		ENGINEerr(ZENCOD_F_ZENCOD_DSA_DO_SIGN, ZENCOD_R_REQUEST_FAILED);
		goto FAILED;
	}

	/* Do in software if argument is too large for hardware */
	if ( BN_num_bits(dsa->p) > ZENBRIDGE_MAX_KEYSIZE_DSA_SIGN ||
		BN_num_bits(dsa->g) > ZENBRIDGE_MAX_KEYSIZE_DSA_SIGN ) {
		const DSA_METHOD *meth;
		ENGINEerr(ZENCOD_F_ZENCOD_DSA_DO_SIGN, ZENCOD_R_BAD_KEY_COMPONENTS);
		meth = DSA_OpenSSL();
		return meth->dsa_do_sign(dgst, dlen, dsa);
	}

	if ( !(bn_s = BN_new()) || !(bn_r = BN_new()) ) {
		ENGINEerr(ZENCOD_F_ZENCOD_DSA_DO_SIGN, ZENCOD_R_BAD_KEY_COMPONENTS);
		goto FAILED;
	}

	if ( !bn_expand(bn_r, 160) || !bn_expand(bn_s, 160) ) {
		ENGINEerr(ZENCOD_F_ZENCOD_DSA_DO_SIGN, ZENCOD_R_BN_EXPAND_FAIL);
		goto FAILED;
	}

	bn_r->top = bn_s->top = (160 + BN_BITS2 - 1) / BN_BITS2;
	BIGNUM2ZEN ( &p, dsa->p ) ;
	BIGNUM2ZEN ( &q, dsa->q ) ;
	BIGNUM2ZEN ( &g, dsa->g ) ;
	BIGNUM2ZEN ( &x, dsa->priv_key ) ;
	BIGNUM2ZEN ( &y, dsa->pub_key ) ;
	BIGNUM2ZEN ( &r, bn_r ) ;
	BIGNUM2ZEN ( &s, bn_s ) ;
	q.len = x.len = 160;

	ypcmem(msg, dgst, 20);
	ptr_zencod_init_number ( &data, 160, msg ) ;

	if ( ptr_zencod_dsa_do_sign ( 0, &data, &y, &p, &q, &g, &x, &r, &s ) < 0 ) {
		PERROR("zenbridge_dsa_do_sign");
		ENGINEerr(ZENCOD_F_ZENCOD_DSA_DO_SIGN, ZENCOD_R_REQUEST_FAILED);
		goto FAILED;
	}

	if ( !( sig = DSA_SIG_new () ) ) {
		ENGINEerr(ZENCOD_F_ZENCOD_DSA_DO_SIGN, ZENCOD_R_REQUEST_FAILED);
		goto FAILED;
	}
	sig->r = bn_r;
	sig->s = bn_s;
	return sig;

 FAILED:
	if (bn_r)
		BN_free(bn_r);
	if (bn_s)
		BN_free(bn_s);
	return NULL;
}


static int DSA_zencod_do_verify ( const unsigned char *dgst, int dlen, DSA_SIG *sig, DSA *dsa )
{
	zen_nb_t data, p, q, g, y, r, s, v;
	char msg[20];
	char v_data[20];
	int ret;

	CHEESE();

	if ( !zencod_dso ) {
		ENGINEerr(ZENCOD_F_ZENCOD_DSA_DO_VERIFY, ZENCOD_R_NOT_LOADED);
		return 0;
	}

	if ( dlen > 160 ) {
		ENGINEerr(ZENCOD_F_ZENCOD_DSA_DO_SIGN, ZENCOD_R_REQUEST_FAILED);
		return 0;
	}

	/* Do in software if argument is too large for hardware */
	if ( BN_num_bits(dsa->p) > ZENBRIDGE_MAX_KEYSIZE_DSA_SIGN ||
		BN_num_bits(dsa->g) > ZENBRIDGE_MAX_KEYSIZE_DSA_SIGN ) {
		const DSA_METHOD *meth;
		ENGINEerr(ZENCOD_F_ZENCOD_DSA_DO_SIGN, ZENCOD_R_BAD_KEY_COMPONENTS);
		meth = DSA_OpenSSL();
		return meth->dsa_do_verify(dgst, dlen, sig, dsa);
	}

	BIGNUM2ZEN ( &p, dsa->p ) ;
	BIGNUM2ZEN ( &q, dsa->q ) ;
	BIGNUM2ZEN ( &g, dsa->g ) ;
	BIGNUM2ZEN ( &y, dsa->pub_key ) ;
	BIGNUM2ZEN ( &r, sig->r ) ;
	BIGNUM2ZEN ( &s, sig->s ) ;
	ptr_zencod_init_number ( &v, 160, v_data ) ;
	ypcmem(msg, dgst, 20);
	ptr_zencod_init_number ( &data, 160, msg ) ;

	if ( ( ret = ptr_zencod_dsa_do_verify ( 0, &data, &p, &q, &g, &y, &r, &s, &v ) ) < 0 ) {
		PERROR("zenbridge_dsa_do_verify");
		ENGINEerr(ZENCOD_F_ZENCOD_DSA_DO_VERIFY, ZENCOD_R_REQUEST_FAILED);
		return 0;
	}

	return ( ( ret == 0 ) ? 1 : ret ) ;
}


static int DSA_zencod_bn_mod_exp ( DSA *dsa, BIGNUM *r, BIGNUM *a, const BIGNUM *p, const BIGNUM *m,
			     BN_CTX *ctx, BN_MONT_CTX *m_ctx )
{
	CHEESE () ;

	return zencod_bn_mod_exp ( r, a, p, m, ctx ) ;
}
#endif /* !OPENSSL_NO_DSA */


#ifndef OPENSSl_NO_DH
/* DH stuff Functions
 */
static int DH_zencod_generate_key ( DH *dh )
{
	BIGNUM *bn_prv = NULL;
	BIGNUM *bn_pub = NULL;
	zen_nb_t y, x, g, p;
	int generate_x;

	CHEESE();

	if ( !zencod_dso ) {
		ENGINEerr(ZENCOD_F_ZENCOD_DH_GENERATE, ZENCOD_R_NOT_LOADED);
		return 0;
	}

	/* Private key */
	if ( dh->priv_key ) {
		bn_prv = dh->priv_key;
		generate_x = 0;
	} else {
		if (!(bn_prv = BN_new())) {
			ENGINEerr(ZENCOD_F_ZENCOD_DH_GENERATE, ZENCOD_R_BN_EXPAND_FAIL);
			goto FAILED;
		}
		generate_x = 1;
	}

	/* Public key */
	if ( dh->pub_key )
		bn_pub = dh->pub_key;
	else
		if ( !( bn_pub = BN_new () ) ) {
			ENGINEerr(ZENCOD_F_ZENCOD_DH_GENERATE, ZENCOD_R_BN_EXPAND_FAIL);
			goto FAILED;
		}

	/* Expand */
	if ( !bn_wexpand ( bn_prv, dh->p->dmax ) ||
	    !bn_wexpand ( bn_pub, dh->p->dmax ) ) {
		ENGINEerr(ZENCOD_F_ZENCOD_DH_GENERATE, ZENCOD_R_BN_EXPAND_FAIL);
		goto FAILED;
	}
	bn_prv->top = dh->p->top;
	bn_pub->top = dh->p->top;

	/* Convert all keys */
	BIGNUM2ZEN ( &p, dh->p ) ;
	BIGNUM2ZEN ( &g, dh->g ) ;
	BIGNUM2ZEN ( &y, bn_pub ) ;
	BIGNUM2ZEN ( &x, bn_prv ) ;
	x.len = DH_size(dh) * 8;

	/* Adjust the lengths of P and G */
	p.len = ptr_zencod_bytes2bits ( p.data, ZEN_BYTES ( p.len ) ) ;
	g.len = ptr_zencod_bytes2bits ( g.data, ZEN_BYTES ( g.len ) ) ;

	/* Send the request to the driver */
	if ( ptr_zencod_dh_generate_key ( &y, &x, &g, &p, generate_x ) < 0 ) {
		perror("zenbridge_dh_generate_key");
		ENGINEerr(ZENCOD_F_ZENCOD_DH_GENERATE, ZENCOD_R_REQUEST_FAILED);
		goto FAILED;
	}

	dh->priv_key = bn_prv;
	dh->pub_key  = bn_pub;

	return 1;

 FAILED:
	if (!dh->priv_key && bn_prv)
		BN_free(bn_prv);
	if (!dh->pub_key && bn_pub)
		BN_free(bn_pub);

	return 0;
}


static int DH_zencod_compute_key ( unsigned char *key, const BIGNUM *pub_key, DH *dh )
{
	zen_nb_t y, x, p, k;

	CHEESE();

	if ( !zencod_dso ) {
		ENGINEerr(ZENCOD_F_ZENCOD_DH_COMPUTE, ZENCOD_R_NOT_LOADED);
		return 0;
	}

	if ( !dh->priv_key ) {
		ENGINEerr(ZENCOD_F_ZENCOD_DH_COMPUTE, ZENCOD_R_BAD_KEY_COMPONENTS);
		return 0;
	}

	/* Convert all keys */
	BIGNUM2ZEN ( &y, pub_key ) ;
	BIGNUM2ZEN ( &x, dh->priv_key ) ;
	BIGNUM2ZEN ( &p, dh->p ) ;
	ptr_zencod_init_number ( &k, p.len, key ) ;

	/* Adjust the lengths */
	p.len = ptr_zencod_bytes2bits ( p.data, ZEN_BYTES ( p.len ) ) ;
	y.len = ptr_zencod_bytes2bits ( y.data, ZEN_BYTES ( y.len ) ) ;
	x.len = ptr_zencod_bytes2bits ( x.data, ZEN_BYTES ( x.len ) ) ;

	/* Call the hardware */
	if ( ptr_zencod_dh_compute_key ( &k, &y, &x, &p ) < 0 ) {
		ENGINEerr(ZENCOD_F_ZENCOD_DH_COMPUTE, ZENCOD_R_REQUEST_FAILED);
		return 0;
	}

	/* The key must be written MSB -> LSB */
	k.len = ptr_zencod_bytes2bits ( k.data, ZEN_BYTES ( k.len ) ) ;
	esrever ( key, ZEN_BYTES ( k.len ) ) ;

	return ZEN_BYTES ( k.len ) ;
}


static int DH_zencod_bn_mod_exp ( const DH *dh, BIGNUM *r, const BIGNUM *a, const BIGNUM *p, const BIGNUM *m, BN_CTX *ctx,
		BN_MONT_CTX *m_ctx )
{
	CHEESE () ;

	return zencod_bn_mod_exp ( r, a, p, m, ctx ) ;
}
#endif	/* !OPENSSL_NO_DH */


/* RAND stuff Functions
 */
static void RAND_zencod_seed ( const void *buf, int num )
{
	/* Nothing to do cause our crypto accelerator provide a true random generator */
}


static int RAND_zencod_rand_bytes ( unsigned char *buf, int num )
{
	zen_nb_t r;

	CHEESE();

	if ( !zencod_dso ) {
		ENGINEerr(ZENCOD_F_ZENCOD_RAND, ZENCOD_R_NOT_LOADED);
		return 0;
	}

	ptr_zencod_init_number ( &r, num * 8, buf ) ;

	if ( ptr_zencod_rand_bytes ( &r, ZENBRIDGE_RNG_DIRECT ) < 0 ) {
		PERROR("zenbridge_rand_bytes");
		ENGINEerr(ZENCOD_F_ZENCOD_RAND, ZENCOD_R_REQUEST_FAILED);
		return 0;
	}

	return 1;
}


static int RAND_zencod_rand_status ( void )
{
	CHEESE () ;

	return 1;
}


/* This stuff is needed if this ENGINE is being compiled into a self-contained
 * shared-library.
 */
#ifdef ENGINE_DYNAMIC_SUPPORT
static int bind_fn ( ENGINE *e, const char *id )
{

	if ( id && ( strcmp ( id, engine_zencod_id ) != 0 ) ) {
		return 0 ;
	}
	if ( !bind_helper ( e ) )  {
		return 0 ;
	}

	return 1 ;
}

IMPLEMENT_DYNAMIC_CHECK_FN ()
IMPLEMENT_DYNAMIC_BIND_FN ( bind_fn )
#endif /* ENGINE_DYNAMIC_SUPPORT */




/*
 * Adding "Digest" and "Cipher" tools ...
 * This is in development ... ;-)
 * In orfer to code this, i refer to hw_openbsd_dev_crypto and openssl engine made by Geoff Thorpe (if i'm rigth),
 * and evp, sha md5 definitions etc ...
 */
/* First add some include ... */
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/md5.h>
#include <openssl/rc4.h>
#include <openssl/des.h>


/* Some variables declaration ... */
/* DONS:
 * Disable symetric computation except DES and 3DES, but let part of the code
 */
/* static int engine_digest_nids [ ] = { NID_sha1, NID_md5 } ; */
static int engine_digest_nids [ ] = {  } ;
static int engine_digest_nids_num = 0 ;
/* static int engine_cipher_nids [ ] = { NID_rc4, NID_rc4_40, NID_des_cbc, NID_des_ede3_cbc } ; */
static int engine_cipher_nids [ ] = { NID_des_cbc, NID_des_ede3_cbc } ;
static int engine_cipher_nids_num = 2 ;


/* Function prototype ... */
/*  SHA stuff */
static int engine_sha1_init ( EVP_MD_CTX *ctx ) ;
static int engine_sha1_update ( EVP_MD_CTX *ctx, const void *data, unsigned long count ) ;
static int engine_sha1_final ( EVP_MD_CTX *ctx, unsigned char *md ) ;

/*  MD5 stuff */
static int engine_md5_init ( EVP_MD_CTX *ctx ) ;
static int engine_md5_update ( EVP_MD_CTX *ctx, const void *data, unsigned long count ) ;
static int engine_md5_final ( EVP_MD_CTX *ctx, unsigned char *md ) ;

static int engine_md_cleanup ( EVP_MD_CTX *ctx ) ;
static int engine_md_copy ( EVP_MD_CTX *to, const EVP_MD_CTX *from ) ;


/* RC4 Stuff */
static int engine_rc4_init_key ( EVP_CIPHER_CTX *ctx, const unsigned char *key, const unsigned char *iv, int enc ) ;
static int engine_rc4_cipher ( EVP_CIPHER_CTX *ctx, unsigned char *out, const unsigned char *in, unsigned int inl ) ;

/* DES Stuff */
static int engine_des_init_key ( EVP_CIPHER_CTX *ctx, const unsigned char *key, const unsigned char *iv, int enc ) ;
static int engine_des_cbc_cipher ( EVP_CIPHER_CTX *ctx, unsigned char *out, const unsigned char *in, unsigned int inl ) ;

/*  3DES Stuff */
static int engine_des_ede3_init_key ( EVP_CIPHER_CTX *ctx, const unsigned char *key, const unsigned char *iv, int enc ) ;
static int engine_des_ede3_cbc_cipher ( EVP_CIPHER_CTX *ctx, unsigned char *out,const unsigned char *in, unsigned int inl ) ;

static int engine_cipher_cleanup ( EVP_CIPHER_CTX *ctx ) ;	/* cleanup ctx */


/* The one for SHA ... */
static const EVP_MD engine_sha1_md =
{
	NID_sha1,
	NID_sha1WithRSAEncryption,
	SHA_DIGEST_LENGTH,
	EVP_MD_FLAG_ONESHOT,
	/* 0, */			/* EVP_MD_FLAG_ONESHOT = x0001 digest can only handle a single block
				* XXX: set according to device info ... */
	engine_sha1_init,
	engine_sha1_update,
	engine_sha1_final,
	engine_md_copy,		/* dev_crypto_sha_copy */
	engine_md_cleanup,		/* dev_crypto_sha_cleanup */
	EVP_PKEY_RSA_method,
	SHA_CBLOCK,
	/* sizeof ( EVP_MD * ) + sizeof ( SHA_CTX ) */
	sizeof ( ZEN_MD_DATA )
	/* sizeof ( MD_CTX_DATA )	The message digest data structure ... */
} ;

/* The one for MD5 ... */
static const EVP_MD engine_md5_md =
{
	NID_md5,
	NID_md5WithRSAEncryption,
	MD5_DIGEST_LENGTH,
	EVP_MD_FLAG_ONESHOT,
	/* 0, */			/* EVP_MD_FLAG_ONESHOT = x0001 digest can only handle a single block
				* XXX: set according to device info ... */
	engine_md5_init,
	engine_md5_update,
	engine_md5_final,
	engine_md_copy,		/* dev_crypto_md5_copy */
	engine_md_cleanup,		/* dev_crypto_md5_cleanup */
	EVP_PKEY_RSA_method,
	MD5_CBLOCK,
	/* sizeof ( EVP_MD * ) + sizeof ( MD5_CTX ) */
	sizeof ( ZEN_MD_DATA )
	/* sizeof ( MD_CTX_DATA )	The message digest data structure ... */
} ;


/* The one for RC4 ... */
#define EVP_RC4_KEY_SIZE			16

/* Try something static ... */
typedef struct
{
	unsigned int len ;
	unsigned int first ;
	unsigned char rc4_state [ 260 ] ;
} NEW_ZEN_RC4_KEY ;

#define rc4_data(ctx)				( (EVP_RC4_KEY *) ( ctx )->cipher_data )

static const EVP_CIPHER engine_rc4 =
{
	NID_rc4,
	1,
	16,				/* EVP_RC4_KEY_SIZE should be 128 bits */
	0,				/* FIXME: key should be up to 256 bytes */
	EVP_CIPH_VARIABLE_LENGTH,
	engine_rc4_init_key,
	engine_rc4_cipher,
	engine_cipher_cleanup,
	sizeof ( NEW_ZEN_RC4_KEY ),
	NULL,
	NULL,
	NULL
} ;

/* The one for RC4_40 ... */
static const EVP_CIPHER engine_rc4_40 =
{
	NID_rc4_40,
	1,
	5,				/* 40 bits */
	0,
	EVP_CIPH_VARIABLE_LENGTH,
	engine_rc4_init_key,
	engine_rc4_cipher,
	engine_cipher_cleanup,
	sizeof ( NEW_ZEN_RC4_KEY ),
	NULL,
	NULL,
	NULL
} ;

/* The one for DES ... */

/* Try something static ... */
typedef struct
{
	unsigned char des_key [ 24 ] ;
	unsigned char des_iv [ 8 ] ;
} ZEN_DES_KEY ;

static const EVP_CIPHER engine_des_cbc =
	{
	NID_des_cbc,
	8, 8, 8,
	0 | EVP_CIPH_CBC_MODE,
	engine_des_init_key,
	engine_des_cbc_cipher,
	engine_cipher_cleanup,
	sizeof(ZEN_DES_KEY),
	EVP_CIPHER_set_asn1_iv,
	EVP_CIPHER_get_asn1_iv,
	NULL,
	NULL
	};

/* The one for 3DES ... */

/* Try something static ... */
typedef struct
{
	unsigned char des3_key [ 24 ] ;
	unsigned char des3_iv [ 8 ] ;
} ZEN_3DES_KEY ;

#define des_data(ctx)				 ( (DES_EDE_KEY *) ( ctx )->cipher_data )

static const EVP_CIPHER engine_des_ede3_cbc =
	{
	NID_des_ede3_cbc,
	8, 8, 8,
	0 | EVP_CIPH_CBC_MODE,
	engine_des_ede3_init_key,
	engine_des_ede3_cbc_cipher,
	engine_cipher_cleanup,
	sizeof(ZEN_3DES_KEY),
	EVP_CIPHER_set_asn1_iv,
	EVP_CIPHER_get_asn1_iv,
	NULL,
	NULL
	};


/* General function cloned on hw_openbsd_dev_crypto one ... */
static int engine_digests ( ENGINE *e, const EVP_MD **digest, const int **nids, int nid )
{

#ifdef DEBUG_ZENCOD_MD
	fprintf ( stderr, "\t=>Function : static int engine_digests () called !\n" ) ;
#endif

	if ( !digest ) {
		/* We are returning a list of supported nids */
		*nids = engine_digest_nids ;
		return engine_digest_nids_num ;
	}
	/* We are being asked for a specific digest */
	if ( nid == NID_md5 ) {
		*digest = &engine_md5_md ;
	}
	else if ( nid == NID_sha1 ) {
		*digest = &engine_sha1_md ;
	}
	else {
		*digest = NULL ;
		return 0 ;
	}
	return 1 ;
}


/* SHA stuff Functions
 */
static int engine_sha1_init ( EVP_MD_CTX *ctx )
{

	int to_return = 0 ;

	/* Test with zenbridge library ... */
	to_return = ptr_zencod_sha1_init ( (ZEN_MD_DATA *) ctx->md_data ) ;
	to_return = !to_return ;

	return to_return ;
}


static int engine_sha1_update ( EVP_MD_CTX *ctx, const void *data, unsigned long count )
{

	zen_nb_t input ;
	int to_return = 0 ;

	/* Convert parameters ... */
	input.len = count ;
	input.data = (unsigned char *) data ;

	/* Test with zenbridge library ... */
	to_return = ptr_zencod_sha1_update ( (ZEN_MD_DATA *) ctx->md_data, (const zen_nb_t *) &input ) ;
	to_return = !to_return ;

	return to_return ;
}


static int engine_sha1_final ( EVP_MD_CTX *ctx, unsigned char *md )
{

	zen_nb_t output ;
	int to_return = 0 ;

	/* Convert parameters ... */
	output.len = SHA_DIGEST_LENGTH ;
	output.data = md ;

	/* Test with zenbridge library ... */
	to_return = ptr_zencod_sha1_do_final ( (ZEN_MD_DATA *) ctx->md_data, (zen_nb_t *) &output ) ;
	to_return = !to_return ;

	return to_return ;
}



/* MD5 stuff Functions
 */
static int engine_md5_init ( EVP_MD_CTX *ctx )
{

	int to_return = 0 ;

	/* Test with zenbridge library ... */
	to_return = ptr_zencod_md5_init ( (ZEN_MD_DATA *) ctx->md_data ) ;
	to_return = !to_return ;

	return to_return ;
}


static int engine_md5_update ( EVP_MD_CTX *ctx, const void *data, unsigned long count )
{

	zen_nb_t input ;
	int to_return = 0 ;

	/* Convert parameters ... */
	input.len = count ;
	input.data = (unsigned char *) data ;

	/* Test with zenbridge library ... */
	to_return = ptr_zencod_md5_update ( (ZEN_MD_DATA *) ctx->md_data, (const zen_nb_t *) &input ) ;
	to_return = !to_return ;

	return to_return ;
}


static int engine_md5_final ( EVP_MD_CTX *ctx, unsigned char *md )
{

	zen_nb_t output ;
	int to_return = 0 ;

	/* Convert parameters ... */
	output.len = MD5_DIGEST_LENGTH ;
	output.data = md ;

	/* Test with zenbridge library ... */
	to_return = ptr_zencod_md5_do_final ( (ZEN_MD_DATA *) ctx->md_data, (zen_nb_t *) &output ) ;
	to_return = !to_return ;

	return to_return ;
}


static int engine_md_cleanup ( EVP_MD_CTX *ctx )
{

	ZEN_MD_DATA *zen_md_data = (ZEN_MD_DATA *) ctx->md_data ;

	if ( zen_md_data->HashBuffer != NULL ) {
		OPENSSL_free ( zen_md_data->HashBuffer ) ;
		zen_md_data->HashBufferSize = 0 ;
		ctx->md_data = NULL ;
	}

	return 1 ;
}


static int engine_md_copy ( EVP_MD_CTX *to, const EVP_MD_CTX *from )
{
	const ZEN_MD_DATA *from_md = (ZEN_MD_DATA *) from->md_data ;
	ZEN_MD_DATA *to_md = (ZEN_MD_DATA *) to->md_data ;

	to_md->HashBuffer = OPENSSL_malloc ( from_md->HashBufferSize ) ;
	memcpy ( to_md->HashBuffer, from_md->HashBuffer, from_md->HashBufferSize ) ;

	return 1;
}


/* General function cloned on hw_openbsd_dev_crypto one ... */
static int engine_ciphers ( ENGINE *e, const EVP_CIPHER **cipher, const int **nids, int nid )
{

	if ( !cipher ) {
		/* We are returning a list of supported nids */
		*nids = engine_cipher_nids ;
		return engine_cipher_nids_num ;
	}
	/* We are being asked for a specific cipher */
	if ( nid == NID_rc4 ) {
		*cipher = &engine_rc4 ;
	}
	else if ( nid == NID_rc4_40 ) {
		*cipher = &engine_rc4_40 ;
	}
	else if ( nid == NID_des_cbc ) {
		*cipher = &engine_des_cbc ;
	}
	else if ( nid == NID_des_ede3_cbc ) {
		*cipher = &engine_des_ede3_cbc ;
	}
	else {
		*cipher = NULL ;
		return 0 ;
	}

	return 1 ;
}


static int engine_rc4_init_key ( EVP_CIPHER_CTX *ctx, const unsigned char *key, const unsigned char *iv, int enc )
{
	int to_return = 0 ;
	int i = 0 ;
	int nb = 0 ;
	NEW_ZEN_RC4_KEY *tmp_rc4_key = NULL ;

	tmp_rc4_key = (NEW_ZEN_RC4_KEY *) ( ctx->cipher_data ) ;
	tmp_rc4_key->first = 0 ;
	tmp_rc4_key->len = ctx->key_len ;
	tmp_rc4_key->rc4_state [ 0 ] = 0x00 ;
	tmp_rc4_key->rc4_state [ 2 ] = 0x00 ;
	nb = 256 / ctx->key_len ;
	for ( i = 0; i < nb ; i++ ) {
		memcpy ( &( tmp_rc4_key->rc4_state [ 4 + i*ctx->key_len ] ), key, ctx->key_len ) ;
	}

	to_return = 1 ;

	return to_return ;
}


static int engine_rc4_cipher ( EVP_CIPHER_CTX *ctx, unsigned char *out, const unsigned char *in, unsigned int in_len )
{

	zen_nb_t output, input ;
	zen_nb_t rc4key ;
	int to_return = 0 ;
	NEW_ZEN_RC4_KEY *tmp_rc4_key = NULL ;

	/* Convert parameters ... */
	input.len = in_len ;
	input.data = (unsigned char *) in ;
	output.len = in_len ;
	output.data = (unsigned char *) out ;

	tmp_rc4_key = ( (NEW_ZEN_RC4_KEY *) ( ctx->cipher_data ) ) ;
	rc4key.len = 260 ;
	rc4key.data = &( tmp_rc4_key->rc4_state [ 0 ] ) ;

	/* Test with zenbridge library ... */
	to_return = ptr_zencod_rc4_cipher ( &output, &input, (const zen_nb_t *) &rc4key, &( tmp_rc4_key->rc4_state [0] ), &( tmp_rc4_key->rc4_state [3] ), !tmp_rc4_key->first ) ;
	to_return = !to_return ;

	/* Update encryption state ... */
	tmp_rc4_key->first = 1 ;
	tmp_rc4_key = NULL ;

	return to_return ;
}


static int engine_des_init_key ( EVP_CIPHER_CTX *ctx, const unsigned char *key, const unsigned char *iv, int enc )
{

	ZEN_DES_KEY *tmp_des_key = NULL ;
	int to_return = 0 ;

	tmp_des_key = (ZEN_DES_KEY *) ( ctx->cipher_data ) ;
	memcpy ( &( tmp_des_key->des_key [ 0 ] ), key, 8 ) ;
	memcpy ( &( tmp_des_key->des_key [ 8 ] ), key, 8 ) ;
	memcpy ( &( tmp_des_key->des_key [ 16 ] ), key, 8 ) ;
	memcpy ( &( tmp_des_key->des_iv [ 0 ] ), iv, 8 ) ;

	to_return = 1 ;

	return to_return ;
}


static int engine_des_cbc_cipher ( EVP_CIPHER_CTX *ctx, unsigned char *out, const unsigned char *in, unsigned int inl )
{

	zen_nb_t output, input ;
	zen_nb_t deskey_1, deskey_2, deskey_3, iv ;
	int to_return = 0 ;

	/* Convert parameters ... */
	input.len = inl ;
	input.data = (unsigned char *) in ;
	output.len = inl ;
	output.data = out ;

	/* Set key parameters ... */
	deskey_1.len = 8 ;
	deskey_2.len = 8 ;
	deskey_3.len = 8 ;
	deskey_1.data = (unsigned char *) ( (ZEN_DES_KEY *) ( ctx->cipher_data ) )->des_key ;
	deskey_2.data =  (unsigned char *) &( (ZEN_DES_KEY *) ( ctx->cipher_data ) )->des_key [ 8 ] ;
	deskey_3.data =  (unsigned char *) &( (ZEN_DES_KEY *) ( ctx->cipher_data ) )->des_key [ 16 ] ;

	/* Key correct iv ... */
	memcpy ( ( (ZEN_DES_KEY *) ( ctx->cipher_data ) )->des_iv, ctx->iv, 8 ) ;
	iv.len = 8 ;
	iv.data = (unsigned char *) ( (ZEN_DES_KEY *) ( ctx->cipher_data ) )->des_iv ;

	if ( ctx->encrypt == 0 ) {
		memcpy ( ctx->iv, &( input.data [ input.len - 8 ] ), 8 ) ;
	}

	/* Test with zenbridge library ... */
	to_return = ptr_zencod_xdes_cipher ( &output, &input,
			(zen_nb_t *) &deskey_1, (zen_nb_t *) &deskey_2, (zen_nb_t *) &deskey_3, &iv, ctx->encrypt ) ;
	to_return = !to_return ;

	/* But we need to set up the rigth iv ...
	 * Test ENCRYPT or DECRYPT mode to set iv ... */
	if ( ctx->encrypt == 1 ) {
		memcpy ( ctx->iv, &( output.data [ output.len - 8 ] ), 8 ) ;
	}

	return to_return ;
}


static int engine_des_ede3_init_key ( EVP_CIPHER_CTX *ctx, const unsigned char *key, const unsigned char *iv, int enc )
{

	ZEN_3DES_KEY *tmp_3des_key = NULL ;
	int to_return = 0 ;

	tmp_3des_key = (ZEN_3DES_KEY *) ( ctx->cipher_data ) ;
	memcpy ( &( tmp_3des_key->des3_key [ 0 ] ), key, 24 ) ;
	memcpy ( &( tmp_3des_key->des3_iv [ 0 ] ), iv, 8 ) ;

	to_return = 1;

	return to_return ;
}


static int engine_des_ede3_cbc_cipher ( EVP_CIPHER_CTX *ctx, unsigned char *out, const unsigned char *in,
	unsigned int in_len )
{

	zen_nb_t output, input ;
	zen_nb_t deskey_1, deskey_2, deskey_3, iv ;
	int to_return = 0 ;

	/* Convert parameters ... */
	input.len = in_len ;
	input.data = (unsigned char *) in ;
	output.len = in_len ;
	output.data = out ;

	/* Set key ... */
	deskey_1.len = 8 ;
	deskey_2.len = 8 ;
	deskey_3.len = 8 ;
	deskey_1.data =  (unsigned char *) ( (ZEN_3DES_KEY *) ( ctx->cipher_data ) )->des3_key ;
	deskey_2.data =  (unsigned char *) &( (ZEN_3DES_KEY *) ( ctx->cipher_data ) )->des3_key [ 8 ] ;
	deskey_3.data =  (unsigned char *) &( (ZEN_3DES_KEY *) ( ctx->cipher_data ) )->des3_key [ 16 ] ;

	/* Key correct iv ... */
	memcpy ( ( (ZEN_3DES_KEY *) ( ctx->cipher_data ) )->des3_iv, ctx->iv, 8 ) ;
	iv.len = 8 ;
	iv.data = (unsigned char *) ( (ZEN_3DES_KEY *) ( ctx->cipher_data ) )->des3_iv ;

	if ( ctx->encrypt == 0 ) {
		memcpy ( ctx->iv, &( input.data [ input.len - 8 ] ), 8 ) ;
	}

	/* Test with zenbridge library ... */
	to_return = ptr_zencod_xdes_cipher ( &output, &input,
			(zen_nb_t *) &deskey_1, (zen_nb_t *) &deskey_2, (zen_nb_t *) &deskey_3, &iv, ctx->encrypt ) ;
	to_return = !to_return ;

	if ( ctx->encrypt == 1 ) {
		memcpy ( ctx->iv, &( output.data [ output.len - 8 ] ), 8 ) ;
	}

	return to_return ;
}


static int engine_cipher_cleanup ( EVP_CIPHER_CTX *ctx )
{

	/* Set the key pointer ... */
	if ( ctx->cipher->nid == NID_rc4 || ctx->cipher->nid == NID_rc4_40 ) {
	}
	else if ( ctx->cipher->nid == NID_des_cbc ) {
	}
	else if ( ctx->cipher->nid == NID_des_ede3_cbc ) {
	}

	return 1 ;
}


#endif /* !OPENSSL_NO_HW_ZENCOD */
#endif /* !OPENSSL_NO_HW */
