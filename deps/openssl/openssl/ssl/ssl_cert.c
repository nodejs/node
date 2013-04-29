/*! \file ssl/ssl_cert.c */
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
 * Copyright (c) 1998-2007 The OpenSSL Project.  All rights reserved.
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
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
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
/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 * ECC cipher suite support in OpenSSL originally developed by 
 * SUN MICROSYSTEMS, INC., and contributed to the OpenSSL project.
 */

#include <stdio.h>

#include "e_os.h"
#ifndef NO_SYS_TYPES_H
# include <sys/types.h>
#endif

#include "o_dir.h"
#include <openssl/objects.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>
#ifndef OPENSSL_NO_DH
#include <openssl/dh.h>
#endif
#include <openssl/bn.h>
#include "ssl_locl.h"

int SSL_get_ex_data_X509_STORE_CTX_idx(void)
	{
	static volatile int ssl_x509_store_ctx_idx= -1;
	int got_write_lock = 0;

	CRYPTO_r_lock(CRYPTO_LOCK_SSL_CTX);

	if (ssl_x509_store_ctx_idx < 0)
		{
		CRYPTO_r_unlock(CRYPTO_LOCK_SSL_CTX);
		CRYPTO_w_lock(CRYPTO_LOCK_SSL_CTX);
		got_write_lock = 1;
		
		if (ssl_x509_store_ctx_idx < 0)
			{
			ssl_x509_store_ctx_idx=X509_STORE_CTX_get_ex_new_index(
				0,"SSL for verify callback",NULL,NULL,NULL);
			}
		}

	if (got_write_lock)
		CRYPTO_w_unlock(CRYPTO_LOCK_SSL_CTX);
	else
		CRYPTO_r_unlock(CRYPTO_LOCK_SSL_CTX);
	
	return ssl_x509_store_ctx_idx;
	}

static void ssl_cert_set_default_md(CERT *cert)
	{
	/* Set digest values to defaults */
#ifndef OPENSSL_NO_DSA
	cert->pkeys[SSL_PKEY_DSA_SIGN].digest = EVP_sha1();
#endif
#ifndef OPENSSL_NO_RSA
	cert->pkeys[SSL_PKEY_RSA_SIGN].digest = EVP_sha1();
	cert->pkeys[SSL_PKEY_RSA_ENC].digest = EVP_sha1();
#endif
#ifndef OPENSSL_NO_ECDSA
	cert->pkeys[SSL_PKEY_ECC].digest = EVP_sha1();
#endif
	}

CERT *ssl_cert_new(void)
	{
	CERT *ret;

	ret=(CERT *)OPENSSL_malloc(sizeof(CERT));
	if (ret == NULL)
		{
		SSLerr(SSL_F_SSL_CERT_NEW,ERR_R_MALLOC_FAILURE);
		return(NULL);
		}
	memset(ret,0,sizeof(CERT));

	ret->key= &(ret->pkeys[SSL_PKEY_RSA_ENC]);
	ret->references=1;
	ssl_cert_set_default_md(ret);
	return(ret);
	}

CERT *ssl_cert_dup(CERT *cert)
	{
	CERT *ret;
	int i;

	ret = (CERT *)OPENSSL_malloc(sizeof(CERT));
	if (ret == NULL)
		{
		SSLerr(SSL_F_SSL_CERT_DUP, ERR_R_MALLOC_FAILURE);
		return(NULL);
		}

	memset(ret, 0, sizeof(CERT));

	ret->key = &ret->pkeys[cert->key - &cert->pkeys[0]];
	/* or ret->key = ret->pkeys + (cert->key - cert->pkeys),
	 * if you find that more readable */

	ret->valid = cert->valid;
	ret->mask_k = cert->mask_k;
	ret->mask_a = cert->mask_a;
	ret->export_mask_k = cert->export_mask_k;
	ret->export_mask_a = cert->export_mask_a;

#ifndef OPENSSL_NO_RSA
	if (cert->rsa_tmp != NULL)
		{
		RSA_up_ref(cert->rsa_tmp);
		ret->rsa_tmp = cert->rsa_tmp;
		}
	ret->rsa_tmp_cb = cert->rsa_tmp_cb;
#endif

#ifndef OPENSSL_NO_DH
	if (cert->dh_tmp != NULL)
		{
		ret->dh_tmp = DHparams_dup(cert->dh_tmp);
		if (ret->dh_tmp == NULL)
			{
			SSLerr(SSL_F_SSL_CERT_DUP, ERR_R_DH_LIB);
			goto err;
			}
		if (cert->dh_tmp->priv_key)
			{
			BIGNUM *b = BN_dup(cert->dh_tmp->priv_key);
			if (!b)
				{
				SSLerr(SSL_F_SSL_CERT_DUP, ERR_R_BN_LIB);
				goto err;
				}
			ret->dh_tmp->priv_key = b;
			}
		if (cert->dh_tmp->pub_key)
			{
			BIGNUM *b = BN_dup(cert->dh_tmp->pub_key);
			if (!b)
				{
				SSLerr(SSL_F_SSL_CERT_DUP, ERR_R_BN_LIB);
				goto err;
				}
			ret->dh_tmp->pub_key = b;
			}
		}
	ret->dh_tmp_cb = cert->dh_tmp_cb;
#endif

#ifndef OPENSSL_NO_ECDH
	if (cert->ecdh_tmp)
		{
		ret->ecdh_tmp = EC_KEY_dup(cert->ecdh_tmp);
		if (ret->ecdh_tmp == NULL)
			{
			SSLerr(SSL_F_SSL_CERT_DUP, ERR_R_EC_LIB);
			goto err;
			}
		}
	ret->ecdh_tmp_cb = cert->ecdh_tmp_cb;
#endif

	for (i = 0; i < SSL_PKEY_NUM; i++)
		{
		if (cert->pkeys[i].x509 != NULL)
			{
			ret->pkeys[i].x509 = cert->pkeys[i].x509;
			CRYPTO_add(&ret->pkeys[i].x509->references, 1,
				CRYPTO_LOCK_X509);
			}
		
		if (cert->pkeys[i].privatekey != NULL)
			{
			ret->pkeys[i].privatekey = cert->pkeys[i].privatekey;
			CRYPTO_add(&ret->pkeys[i].privatekey->references, 1,
				CRYPTO_LOCK_EVP_PKEY);

			switch(i) 
				{
				/* If there was anything special to do for
				 * certain types of keys, we'd do it here.
				 * (Nothing at the moment, I think.) */

			case SSL_PKEY_RSA_ENC:
			case SSL_PKEY_RSA_SIGN:
				/* We have an RSA key. */
				break;
				
			case SSL_PKEY_DSA_SIGN:
				/* We have a DSA key. */
				break;
				
			case SSL_PKEY_DH_RSA:
			case SSL_PKEY_DH_DSA:
				/* We have a DH key. */
				break;

			case SSL_PKEY_ECC:
				/* We have an ECC key */
				break;

			default:
				/* Can't happen. */
				SSLerr(SSL_F_SSL_CERT_DUP, SSL_R_LIBRARY_BUG);
				}
			}
		}
	
	/* ret->extra_certs *should* exist, but currently the own certificate
	 * chain is held inside SSL_CTX */

	ret->references=1;
	/* Set digests to defaults. NB: we don't copy existing values as they
	 * will be set during handshake.
	 */
	ssl_cert_set_default_md(ret);

	return(ret);
	
#if !defined(OPENSSL_NO_DH) || !defined(OPENSSL_NO_ECDH)
err:
#endif
#ifndef OPENSSL_NO_RSA
	if (ret->rsa_tmp != NULL)
		RSA_free(ret->rsa_tmp);
#endif
#ifndef OPENSSL_NO_DH
	if (ret->dh_tmp != NULL)
		DH_free(ret->dh_tmp);
#endif
#ifndef OPENSSL_NO_ECDH
	if (ret->ecdh_tmp != NULL)
		EC_KEY_free(ret->ecdh_tmp);
#endif

	for (i = 0; i < SSL_PKEY_NUM; i++)
		{
		if (ret->pkeys[i].x509 != NULL)
			X509_free(ret->pkeys[i].x509);
		if (ret->pkeys[i].privatekey != NULL)
			EVP_PKEY_free(ret->pkeys[i].privatekey);
		}

	return NULL;
	}


void ssl_cert_free(CERT *c)
	{
	int i;

	if(c == NULL)
	    return;

	i=CRYPTO_add(&c->references,-1,CRYPTO_LOCK_SSL_CERT);
#ifdef REF_PRINT
	REF_PRINT("CERT",c);
#endif
	if (i > 0) return;
#ifdef REF_CHECK
	if (i < 0)
		{
		fprintf(stderr,"ssl_cert_free, bad reference count\n");
		abort(); /* ok */
		}
#endif

#ifndef OPENSSL_NO_RSA
	if (c->rsa_tmp) RSA_free(c->rsa_tmp);
#endif
#ifndef OPENSSL_NO_DH
	if (c->dh_tmp) DH_free(c->dh_tmp);
#endif
#ifndef OPENSSL_NO_ECDH
	if (c->ecdh_tmp) EC_KEY_free(c->ecdh_tmp);
#endif

	for (i=0; i<SSL_PKEY_NUM; i++)
		{
		if (c->pkeys[i].x509 != NULL)
			X509_free(c->pkeys[i].x509);
		if (c->pkeys[i].privatekey != NULL)
			EVP_PKEY_free(c->pkeys[i].privatekey);
#if 0
		if (c->pkeys[i].publickey != NULL)
			EVP_PKEY_free(c->pkeys[i].publickey);
#endif
		}
	OPENSSL_free(c);
	}

int ssl_cert_inst(CERT **o)
	{
	/* Create a CERT if there isn't already one
	 * (which cannot really happen, as it is initially created in
	 * SSL_CTX_new; but the earlier code usually allows for that one
	 * being non-existant, so we follow that behaviour, as it might
	 * turn out that there actually is a reason for it -- but I'm
	 * not sure that *all* of the existing code could cope with
	 * s->cert being NULL, otherwise we could do without the
	 * initialization in SSL_CTX_new).
	 */
	
	if (o == NULL) 
		{
		SSLerr(SSL_F_SSL_CERT_INST, ERR_R_PASSED_NULL_PARAMETER);
		return(0);
		}
	if (*o == NULL)
		{
		if ((*o = ssl_cert_new()) == NULL)
			{
			SSLerr(SSL_F_SSL_CERT_INST, ERR_R_MALLOC_FAILURE);
			return(0);
			}
		}
	return(1);
	}


SESS_CERT *ssl_sess_cert_new(void)
	{
	SESS_CERT *ret;

	ret = OPENSSL_malloc(sizeof *ret);
	if (ret == NULL)
		{
		SSLerr(SSL_F_SSL_SESS_CERT_NEW, ERR_R_MALLOC_FAILURE);
		return NULL;
		}

	memset(ret, 0 ,sizeof *ret);
	ret->peer_key = &(ret->peer_pkeys[SSL_PKEY_RSA_ENC]);
	ret->references = 1;

	return ret;
	}

void ssl_sess_cert_free(SESS_CERT *sc)
	{
	int i;

	if (sc == NULL)
		return;

	i = CRYPTO_add(&sc->references, -1, CRYPTO_LOCK_SSL_SESS_CERT);
#ifdef REF_PRINT
	REF_PRINT("SESS_CERT", sc);
#endif
	if (i > 0)
		return;
#ifdef REF_CHECK
	if (i < 0)
		{
		fprintf(stderr,"ssl_sess_cert_free, bad reference count\n");
		abort(); /* ok */
		}
#endif

	/* i == 0 */
	if (sc->cert_chain != NULL)
		sk_X509_pop_free(sc->cert_chain, X509_free);
	for (i = 0; i < SSL_PKEY_NUM; i++)
		{
		if (sc->peer_pkeys[i].x509 != NULL)
			X509_free(sc->peer_pkeys[i].x509);
#if 0 /* We don't have the peer's private key.  These lines are just
	   * here as a reminder that we're still using a not-quite-appropriate
	   * data structure. */
		if (sc->peer_pkeys[i].privatekey != NULL)
			EVP_PKEY_free(sc->peer_pkeys[i].privatekey);
#endif
		}

#ifndef OPENSSL_NO_RSA
	if (sc->peer_rsa_tmp != NULL)
		RSA_free(sc->peer_rsa_tmp);
#endif
#ifndef OPENSSL_NO_DH
	if (sc->peer_dh_tmp != NULL)
		DH_free(sc->peer_dh_tmp);
#endif
#ifndef OPENSSL_NO_ECDH
	if (sc->peer_ecdh_tmp != NULL)
		EC_KEY_free(sc->peer_ecdh_tmp);
#endif

	OPENSSL_free(sc);
	}

int ssl_set_peer_cert_type(SESS_CERT *sc,int type)
	{
	sc->peer_cert_type = type;
	return(1);
	}

int ssl_verify_cert_chain(SSL *s,STACK_OF(X509) *sk)
	{
	X509 *x;
	int i;
	X509_STORE_CTX ctx;

	if ((sk == NULL) || (sk_X509_num(sk) == 0))
		return(0);

	x=sk_X509_value(sk,0);
	if(!X509_STORE_CTX_init(&ctx,s->ctx->cert_store,x,sk))
		{
		SSLerr(SSL_F_SSL_VERIFY_CERT_CHAIN,ERR_R_X509_LIB);
		return(0);
		}
#if 0
	if (SSL_get_verify_depth(s) >= 0)
		X509_STORE_CTX_set_depth(&ctx, SSL_get_verify_depth(s));
#endif
	X509_STORE_CTX_set_ex_data(&ctx,SSL_get_ex_data_X509_STORE_CTX_idx(),s);

	/* We need to inherit the verify parameters. These can be determined by
	 * the context: if its a server it will verify SSL client certificates
	 * or vice versa.
	 */

	X509_STORE_CTX_set_default(&ctx,
				s->server ? "ssl_client" : "ssl_server");
	/* Anything non-default in "param" should overwrite anything in the
	 * ctx.
	 */
	X509_VERIFY_PARAM_set1(X509_STORE_CTX_get0_param(&ctx), s->param);

	if (s->verify_callback)
		X509_STORE_CTX_set_verify_cb(&ctx, s->verify_callback);

	if (s->ctx->app_verify_callback != NULL)
#if 1 /* new with OpenSSL 0.9.7 */
		i=s->ctx->app_verify_callback(&ctx, s->ctx->app_verify_arg); 
#else
		i=s->ctx->app_verify_callback(&ctx); /* should pass app_verify_arg */
#endif
	else
		{
#ifndef OPENSSL_NO_X509_VERIFY
		i=X509_verify_cert(&ctx);
#else
		i=0;
		ctx.error=X509_V_ERR_APPLICATION_VERIFICATION;
		SSLerr(SSL_F_SSL_VERIFY_CERT_CHAIN,SSL_R_NO_VERIFY_CALLBACK);
#endif
		}

	s->verify_result=ctx.error;
	X509_STORE_CTX_cleanup(&ctx);

	return(i);
	}

static void set_client_CA_list(STACK_OF(X509_NAME) **ca_list,STACK_OF(X509_NAME) *name_list)
	{
	if (*ca_list != NULL)
		sk_X509_NAME_pop_free(*ca_list,X509_NAME_free);

	*ca_list=name_list;
	}

STACK_OF(X509_NAME) *SSL_dup_CA_list(STACK_OF(X509_NAME) *sk)
	{
	int i;
	STACK_OF(X509_NAME) *ret;
	X509_NAME *name;

	ret=sk_X509_NAME_new_null();
	for (i=0; i<sk_X509_NAME_num(sk); i++)
		{
		name=X509_NAME_dup(sk_X509_NAME_value(sk,i));
		if ((name == NULL) || !sk_X509_NAME_push(ret,name))
			{
			sk_X509_NAME_pop_free(ret,X509_NAME_free);
			return(NULL);
			}
		}
	return(ret);
	}

void SSL_set_client_CA_list(SSL *s,STACK_OF(X509_NAME) *name_list)
	{
	set_client_CA_list(&(s->client_CA),name_list);
	}

void SSL_CTX_set_client_CA_list(SSL_CTX *ctx,STACK_OF(X509_NAME) *name_list)
	{
	set_client_CA_list(&(ctx->client_CA),name_list);
	}

STACK_OF(X509_NAME) *SSL_CTX_get_client_CA_list(const SSL_CTX *ctx)
	{
	return(ctx->client_CA);
	}

STACK_OF(X509_NAME) *SSL_get_client_CA_list(const SSL *s)
	{
	if (s->type == SSL_ST_CONNECT)
		{ /* we are in the client */
		if (((s->version>>8) == SSL3_VERSION_MAJOR) &&
			(s->s3 != NULL))
			return(s->s3->tmp.ca_names);
		else
			return(NULL);
		}
	else
		{
		if (s->client_CA != NULL)
			return(s->client_CA);
		else
			return(s->ctx->client_CA);
		}
	}

static int add_client_CA(STACK_OF(X509_NAME) **sk,X509 *x)
	{
	X509_NAME *name;

	if (x == NULL) return(0);
	if ((*sk == NULL) && ((*sk=sk_X509_NAME_new_null()) == NULL))
		return(0);
		
	if ((name=X509_NAME_dup(X509_get_subject_name(x))) == NULL)
		return(0);

	if (!sk_X509_NAME_push(*sk,name))
		{
		X509_NAME_free(name);
		return(0);
		}
	return(1);
	}

int SSL_add_client_CA(SSL *ssl,X509 *x)
	{
	return(add_client_CA(&(ssl->client_CA),x));
	}

int SSL_CTX_add_client_CA(SSL_CTX *ctx,X509 *x)
	{
	return(add_client_CA(&(ctx->client_CA),x));
	}

static int xname_cmp(const X509_NAME * const *a, const X509_NAME * const *b)
	{
	return(X509_NAME_cmp(*a,*b));
	}

#ifndef OPENSSL_NO_STDIO
/*!
 * Load CA certs from a file into a ::STACK. Note that it is somewhat misnamed;
 * it doesn't really have anything to do with clients (except that a common use
 * for a stack of CAs is to send it to the client). Actually, it doesn't have
 * much to do with CAs, either, since it will load any old cert.
 * \param file the file containing one or more certs.
 * \return a ::STACK containing the certs.
 */
STACK_OF(X509_NAME) *SSL_load_client_CA_file(const char *file)
	{
	BIO *in;
	X509 *x=NULL;
	X509_NAME *xn=NULL;
	STACK_OF(X509_NAME) *ret = NULL,*sk;

	sk=sk_X509_NAME_new(xname_cmp);

	in=BIO_new(BIO_s_file_internal());

	if ((sk == NULL) || (in == NULL))
		{
		SSLerr(SSL_F_SSL_LOAD_CLIENT_CA_FILE,ERR_R_MALLOC_FAILURE);
		goto err;
		}
	
	if (!BIO_read_filename(in,file))
		goto err;

	for (;;)
		{
		if (PEM_read_bio_X509(in,&x,NULL,NULL) == NULL)
			break;
		if (ret == NULL)
			{
			ret = sk_X509_NAME_new_null();
			if (ret == NULL)
				{
				SSLerr(SSL_F_SSL_LOAD_CLIENT_CA_FILE,ERR_R_MALLOC_FAILURE);
				goto err;
				}
			}
		if ((xn=X509_get_subject_name(x)) == NULL) goto err;
		/* check for duplicates */
		xn=X509_NAME_dup(xn);
		if (xn == NULL) goto err;
		if (sk_X509_NAME_find(sk,xn) >= 0)
			X509_NAME_free(xn);
		else
			{
			sk_X509_NAME_push(sk,xn);
			sk_X509_NAME_push(ret,xn);
			}
		}

	if (0)
		{
err:
		if (ret != NULL) sk_X509_NAME_pop_free(ret,X509_NAME_free);
		ret=NULL;
		}
	if (sk != NULL) sk_X509_NAME_free(sk);
	if (in != NULL) BIO_free(in);
	if (x != NULL) X509_free(x);
	if (ret != NULL)
		ERR_clear_error();
	return(ret);
	}
#endif

/*!
 * Add a file of certs to a stack.
 * \param stack the stack to add to.
 * \param file the file to add from. All certs in this file that are not
 * already in the stack will be added.
 * \return 1 for success, 0 for failure. Note that in the case of failure some
 * certs may have been added to \c stack.
 */

int SSL_add_file_cert_subjects_to_stack(STACK_OF(X509_NAME) *stack,
					const char *file)
	{
	BIO *in;
	X509 *x=NULL;
	X509_NAME *xn=NULL;
	int ret=1;
	int (*oldcmp)(const X509_NAME * const *a, const X509_NAME * const *b);
	
	oldcmp=sk_X509_NAME_set_cmp_func(stack,xname_cmp);
	
	in=BIO_new(BIO_s_file_internal());
	
	if (in == NULL)
		{
		SSLerr(SSL_F_SSL_ADD_FILE_CERT_SUBJECTS_TO_STACK,ERR_R_MALLOC_FAILURE);
		goto err;
		}
	
	if (!BIO_read_filename(in,file))
		goto err;
	
	for (;;)
		{
		if (PEM_read_bio_X509(in,&x,NULL,NULL) == NULL)
			break;
		if ((xn=X509_get_subject_name(x)) == NULL) goto err;
		xn=X509_NAME_dup(xn);
		if (xn == NULL) goto err;
		if (sk_X509_NAME_find(stack,xn) >= 0)
			X509_NAME_free(xn);
		else
			sk_X509_NAME_push(stack,xn);
		}

	ERR_clear_error();

	if (0)
		{
err:
		ret=0;
		}
	if(in != NULL)
		BIO_free(in);
	if(x != NULL)
		X509_free(x);
	
	(void)sk_X509_NAME_set_cmp_func(stack,oldcmp);

	return ret;
	}

/*!
 * Add a directory of certs to a stack.
 * \param stack the stack to append to.
 * \param dir the directory to append from. All files in this directory will be
 * examined as potential certs. Any that are acceptable to
 * SSL_add_dir_cert_subjects_to_stack() that are not already in the stack will be
 * included.
 * \return 1 for success, 0 for failure. Note that in the case of failure some
 * certs may have been added to \c stack.
 */

int SSL_add_dir_cert_subjects_to_stack(STACK_OF(X509_NAME) *stack,
				       const char *dir)
	{
	OPENSSL_DIR_CTX *d = NULL;
	const char *filename;
	int ret = 0;

	CRYPTO_w_lock(CRYPTO_LOCK_READDIR);

	/* Note that a side effect is that the CAs will be sorted by name */

	while((filename = OPENSSL_DIR_read(&d, dir)))
		{
		char buf[1024];
		int r;

		if(strlen(dir)+strlen(filename)+2 > sizeof buf)
			{
			SSLerr(SSL_F_SSL_ADD_DIR_CERT_SUBJECTS_TO_STACK,SSL_R_PATH_TOO_LONG);
			goto err;
			}

#ifdef OPENSSL_SYS_VMS
		r = BIO_snprintf(buf,sizeof buf,"%s%s",dir,filename);
#else
		r = BIO_snprintf(buf,sizeof buf,"%s/%s",dir,filename);
#endif
		if (r <= 0 || r >= (int)sizeof(buf))
			goto err;
		if(!SSL_add_file_cert_subjects_to_stack(stack,buf))
			goto err;
		}

	if (errno)
		{
		SYSerr(SYS_F_OPENDIR, get_last_sys_error());
		ERR_add_error_data(3, "OPENSSL_DIR_read(&ctx, '", dir, "')");
		SSLerr(SSL_F_SSL_ADD_DIR_CERT_SUBJECTS_TO_STACK, ERR_R_SYS_LIB);
		goto err;
		}

	ret = 1;

err:
	if (d) OPENSSL_DIR_end(&d);
	CRYPTO_w_unlock(CRYPTO_LOCK_READDIR);
	return ret;
	}

