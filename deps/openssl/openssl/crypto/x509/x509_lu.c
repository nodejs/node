/* crypto/x509/x509_lu.c */
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

#include <stdio.h>
#include "cryptlib.h"
#include <openssl/lhash.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

X509_LOOKUP *X509_LOOKUP_new(X509_LOOKUP_METHOD *method)
	{
	X509_LOOKUP *ret;

	ret=(X509_LOOKUP *)OPENSSL_malloc(sizeof(X509_LOOKUP));
	if (ret == NULL) return NULL;

	ret->init=0;
	ret->skip=0;
	ret->method=method;
	ret->method_data=NULL;
	ret->store_ctx=NULL;
	if ((method->new_item != NULL) && !method->new_item(ret))
		{
		OPENSSL_free(ret);
		return NULL;
		}
	return ret;
	}

void X509_LOOKUP_free(X509_LOOKUP *ctx)
	{
	if (ctx == NULL) return;
	if (	(ctx->method != NULL) &&
		(ctx->method->free != NULL))
		ctx->method->free(ctx);
	OPENSSL_free(ctx);
	}

int X509_LOOKUP_init(X509_LOOKUP *ctx)
	{
	if (ctx->method == NULL) return 0;
	if (ctx->method->init != NULL)
		return ctx->method->init(ctx);
	else
		return 1;
	}

int X509_LOOKUP_shutdown(X509_LOOKUP *ctx)
	{
	if (ctx->method == NULL) return 0;
	if (ctx->method->shutdown != NULL)
		return ctx->method->shutdown(ctx);
	else
		return 1;
	}

int X509_LOOKUP_ctrl(X509_LOOKUP *ctx, int cmd, const char *argc, long argl,
	     char **ret)
	{
	if (ctx->method == NULL) return -1;
	if (ctx->method->ctrl != NULL)
		return ctx->method->ctrl(ctx,cmd,argc,argl,ret);
	else
		return 1;
	}

int X509_LOOKUP_by_subject(X509_LOOKUP *ctx, int type, X509_NAME *name,
	     X509_OBJECT *ret)
	{
	if ((ctx->method == NULL) || (ctx->method->get_by_subject == NULL))
		return X509_LU_FAIL;
	if (ctx->skip) return 0;
	return ctx->method->get_by_subject(ctx,type,name,ret);
	}

int X509_LOOKUP_by_issuer_serial(X509_LOOKUP *ctx, int type, X509_NAME *name,
	     ASN1_INTEGER *serial, X509_OBJECT *ret)
	{
	if ((ctx->method == NULL) ||
		(ctx->method->get_by_issuer_serial == NULL))
		return X509_LU_FAIL;
	return ctx->method->get_by_issuer_serial(ctx,type,name,serial,ret);
	}

int X509_LOOKUP_by_fingerprint(X509_LOOKUP *ctx, int type,
	     unsigned char *bytes, int len, X509_OBJECT *ret)
	{
	if ((ctx->method == NULL) || (ctx->method->get_by_fingerprint == NULL))
		return X509_LU_FAIL;
	return ctx->method->get_by_fingerprint(ctx,type,bytes,len,ret);
	}

int X509_LOOKUP_by_alias(X509_LOOKUP *ctx, int type, char *str, int len,
	     X509_OBJECT *ret)
	{
	if ((ctx->method == NULL) || (ctx->method->get_by_alias == NULL))
		return X509_LU_FAIL;
	return ctx->method->get_by_alias(ctx,type,str,len,ret);
	}

  
static int x509_object_cmp(const X509_OBJECT * const *a, const X509_OBJECT * const *b)
  	{
 	int ret;

 	ret=((*a)->type - (*b)->type);
 	if (ret) return ret;
 	switch ((*a)->type)
 		{
 	case X509_LU_X509:
 		ret=X509_subject_name_cmp((*a)->data.x509,(*b)->data.x509);
 		break;
 	case X509_LU_CRL:
 		ret=X509_CRL_cmp((*a)->data.crl,(*b)->data.crl);
 		break;
	default:
		/* abort(); */
		return 0;
		}
	return ret;
	}

X509_STORE *X509_STORE_new(void)
	{
	X509_STORE *ret;

	if ((ret=(X509_STORE *)OPENSSL_malloc(sizeof(X509_STORE))) == NULL)
		return NULL;
	ret->objs = sk_X509_OBJECT_new(x509_object_cmp);
	ret->cache=1;
	ret->get_cert_methods=sk_X509_LOOKUP_new_null();
	ret->verify=0;
	ret->verify_cb=0;

	if ((ret->param = X509_VERIFY_PARAM_new()) == NULL)
		return NULL;

	ret->get_issuer = 0;
	ret->check_issued = 0;
	ret->check_revocation = 0;
	ret->get_crl = 0;
	ret->check_crl = 0;
	ret->cert_crl = 0;
	ret->lookup_certs = 0;
	ret->lookup_crls = 0;
	ret->cleanup = 0;

	if (!CRYPTO_new_ex_data(CRYPTO_EX_INDEX_X509_STORE, ret, &ret->ex_data))
		{
		sk_X509_OBJECT_free(ret->objs);
		OPENSSL_free(ret);
		return NULL;
		}

	ret->references=1;
	return ret;
	}

static void cleanup(X509_OBJECT *a)
	{
	if (a->type == X509_LU_X509)
		{
		X509_free(a->data.x509);
		}
	else if (a->type == X509_LU_CRL)
		{
		X509_CRL_free(a->data.crl);
		}
	else
		{
		/* abort(); */
		}

	OPENSSL_free(a);
	}

void X509_STORE_free(X509_STORE *vfy)
	{
	int i;
	STACK_OF(X509_LOOKUP) *sk;
	X509_LOOKUP *lu;

	if (vfy == NULL)
	    return;

	sk=vfy->get_cert_methods;
	for (i=0; i<sk_X509_LOOKUP_num(sk); i++)
		{
		lu=sk_X509_LOOKUP_value(sk,i);
		X509_LOOKUP_shutdown(lu);
		X509_LOOKUP_free(lu);
		}
	sk_X509_LOOKUP_free(sk);
	sk_X509_OBJECT_pop_free(vfy->objs, cleanup);

	CRYPTO_free_ex_data(CRYPTO_EX_INDEX_X509_STORE, vfy, &vfy->ex_data);
	if (vfy->param)
		X509_VERIFY_PARAM_free(vfy->param);
	OPENSSL_free(vfy);
	}

X509_LOOKUP *X509_STORE_add_lookup(X509_STORE *v, X509_LOOKUP_METHOD *m)
	{
	int i;
	STACK_OF(X509_LOOKUP) *sk;
	X509_LOOKUP *lu;

	sk=v->get_cert_methods;
	for (i=0; i<sk_X509_LOOKUP_num(sk); i++)
		{
		lu=sk_X509_LOOKUP_value(sk,i);
		if (m == lu->method)
			{
			return lu;
			}
		}
	/* a new one */
	lu=X509_LOOKUP_new(m);
	if (lu == NULL)
		return NULL;
	else
		{
		lu->store_ctx=v;
		if (sk_X509_LOOKUP_push(v->get_cert_methods,lu))
			return lu;
		else
			{
			X509_LOOKUP_free(lu);
			return NULL;
			}
		}
	}

int X509_STORE_get_by_subject(X509_STORE_CTX *vs, int type, X509_NAME *name,
	     X509_OBJECT *ret)
	{
	X509_STORE *ctx=vs->ctx;
	X509_LOOKUP *lu;
	X509_OBJECT stmp,*tmp;
	int i,j;

	CRYPTO_w_lock(CRYPTO_LOCK_X509_STORE);
	tmp=X509_OBJECT_retrieve_by_subject(ctx->objs,type,name);
	CRYPTO_w_unlock(CRYPTO_LOCK_X509_STORE);

	if (tmp == NULL || type == X509_LU_CRL)
		{
		for (i=vs->current_method; i<sk_X509_LOOKUP_num(ctx->get_cert_methods); i++)
			{
			lu=sk_X509_LOOKUP_value(ctx->get_cert_methods,i);
			j=X509_LOOKUP_by_subject(lu,type,name,&stmp);
			if (j < 0)
				{
				vs->current_method=j;
				return j;
				}
			else if (j)
				{
				tmp= &stmp;
				break;
				}
			}
		vs->current_method=0;
		if (tmp == NULL)
			return 0;
		}

/*	if (ret->data.ptr != NULL)
		X509_OBJECT_free_contents(ret); */

	ret->type=tmp->type;
	ret->data.ptr=tmp->data.ptr;

	X509_OBJECT_up_ref_count(ret);

	return 1;
	}

int X509_STORE_add_cert(X509_STORE *ctx, X509 *x)
	{
	X509_OBJECT *obj;
	int ret=1;

	if (x == NULL) return 0;
	obj=(X509_OBJECT *)OPENSSL_malloc(sizeof(X509_OBJECT));
	if (obj == NULL)
		{
		X509err(X509_F_X509_STORE_ADD_CERT,ERR_R_MALLOC_FAILURE);
		return 0;
		}
	obj->type=X509_LU_X509;
	obj->data.x509=x;

	CRYPTO_w_lock(CRYPTO_LOCK_X509_STORE);

	X509_OBJECT_up_ref_count(obj);

	if (X509_OBJECT_retrieve_match(ctx->objs, obj))
		{
		X509_OBJECT_free_contents(obj);
		OPENSSL_free(obj);
		X509err(X509_F_X509_STORE_ADD_CERT,X509_R_CERT_ALREADY_IN_HASH_TABLE);
		ret=0;
		} 
	else sk_X509_OBJECT_push(ctx->objs, obj);

	CRYPTO_w_unlock(CRYPTO_LOCK_X509_STORE);

	return ret;
	}

int X509_STORE_add_crl(X509_STORE *ctx, X509_CRL *x)
	{
	X509_OBJECT *obj;
	int ret=1;

	if (x == NULL) return 0;
	obj=(X509_OBJECT *)OPENSSL_malloc(sizeof(X509_OBJECT));
	if (obj == NULL)
		{
		X509err(X509_F_X509_STORE_ADD_CRL,ERR_R_MALLOC_FAILURE);
		return 0;
		}
	obj->type=X509_LU_CRL;
	obj->data.crl=x;

	CRYPTO_w_lock(CRYPTO_LOCK_X509_STORE);

	X509_OBJECT_up_ref_count(obj);

	if (X509_OBJECT_retrieve_match(ctx->objs, obj))
		{
		X509_OBJECT_free_contents(obj);
		OPENSSL_free(obj);
		X509err(X509_F_X509_STORE_ADD_CRL,X509_R_CERT_ALREADY_IN_HASH_TABLE);
		ret=0;
		}
	else sk_X509_OBJECT_push(ctx->objs, obj);

	CRYPTO_w_unlock(CRYPTO_LOCK_X509_STORE);

	return ret;
	}

void X509_OBJECT_up_ref_count(X509_OBJECT *a)
	{
	switch (a->type)
		{
	case X509_LU_X509:
		CRYPTO_add(&a->data.x509->references,1,CRYPTO_LOCK_X509);
		break;
	case X509_LU_CRL:
		CRYPTO_add(&a->data.crl->references,1,CRYPTO_LOCK_X509_CRL);
		break;
		}
	}

void X509_OBJECT_free_contents(X509_OBJECT *a)
	{
	switch (a->type)
		{
	case X509_LU_X509:
		X509_free(a->data.x509);
		break;
	case X509_LU_CRL:
		X509_CRL_free(a->data.crl);
		break;
		}
	}

static int x509_object_idx_cnt(STACK_OF(X509_OBJECT) *h, int type,
	     X509_NAME *name, int *pnmatch)
	{
	X509_OBJECT stmp;
	X509 x509_s;
	X509_CINF cinf_s;
	X509_CRL crl_s;
	X509_CRL_INFO crl_info_s;
	int idx;

	stmp.type=type;
	switch (type)
		{
	case X509_LU_X509:
		stmp.data.x509= &x509_s;
		x509_s.cert_info= &cinf_s;
		cinf_s.subject=name;
		break;
	case X509_LU_CRL:
		stmp.data.crl= &crl_s;
		crl_s.crl= &crl_info_s;
		crl_info_s.issuer=name;
		break;
	default:
		/* abort(); */
		return -1;
		}

	idx = sk_X509_OBJECT_find(h,&stmp);
	if (idx >= 0 && pnmatch)
		{
		int tidx;
		const X509_OBJECT *tobj, *pstmp;
		*pnmatch = 1;
		pstmp = &stmp;
		for (tidx = idx + 1; tidx < sk_X509_OBJECT_num(h); tidx++)
			{
			tobj = sk_X509_OBJECT_value(h, tidx);
			if (x509_object_cmp(&tobj, &pstmp))
				break;
			(*pnmatch)++;
			}
		}
	return idx;
	}


int X509_OBJECT_idx_by_subject(STACK_OF(X509_OBJECT) *h, int type,
	     X509_NAME *name)
	{
	return x509_object_idx_cnt(h, type, name, NULL);
	}

X509_OBJECT *X509_OBJECT_retrieve_by_subject(STACK_OF(X509_OBJECT) *h, int type,
	     X509_NAME *name)
	{
	int idx;
	idx = X509_OBJECT_idx_by_subject(h, type, name);
	if (idx==-1) return NULL;
	return sk_X509_OBJECT_value(h, idx);
	}

STACK_OF(X509)* X509_STORE_get1_certs(X509_STORE_CTX *ctx, X509_NAME *nm)
	{
	int i, idx, cnt;
	STACK_OF(X509) *sk;
	X509 *x;
	X509_OBJECT *obj;
	sk = sk_X509_new_null();
	CRYPTO_w_lock(CRYPTO_LOCK_X509_STORE);
	idx = x509_object_idx_cnt(ctx->ctx->objs, X509_LU_X509, nm, &cnt);
	if (idx < 0)
		{
		/* Nothing found in cache: do lookup to possibly add new
		 * objects to cache
		 */
		X509_OBJECT xobj;
		CRYPTO_w_unlock(CRYPTO_LOCK_X509_STORE);
		if (!X509_STORE_get_by_subject(ctx, X509_LU_X509, nm, &xobj))
			{
			sk_X509_free(sk);
			return NULL;
			}
		X509_OBJECT_free_contents(&xobj);
		CRYPTO_w_lock(CRYPTO_LOCK_X509_STORE);
		idx = x509_object_idx_cnt(ctx->ctx->objs,X509_LU_X509,nm, &cnt);
		if (idx < 0)
			{
			CRYPTO_w_unlock(CRYPTO_LOCK_X509_STORE);
			sk_X509_free(sk);
			return NULL;
			}
		}
	for (i = 0; i < cnt; i++, idx++)
		{
		obj = sk_X509_OBJECT_value(ctx->ctx->objs, idx);
		x = obj->data.x509;
		CRYPTO_add(&x->references, 1, CRYPTO_LOCK_X509);
		if (!sk_X509_push(sk, x))
			{
			CRYPTO_w_unlock(CRYPTO_LOCK_X509_STORE);
			X509_free(x);
			sk_X509_pop_free(sk, X509_free);
			return NULL;
			}
		}
	CRYPTO_w_unlock(CRYPTO_LOCK_X509_STORE);
	return sk;

	}

STACK_OF(X509_CRL)* X509_STORE_get1_crls(X509_STORE_CTX *ctx, X509_NAME *nm)
	{
	int i, idx, cnt;
	STACK_OF(X509_CRL) *sk;
	X509_CRL *x;
	X509_OBJECT *obj, xobj;
	sk = sk_X509_CRL_new_null();
	CRYPTO_w_lock(CRYPTO_LOCK_X509_STORE);
	/* Check cache first */
	idx = x509_object_idx_cnt(ctx->ctx->objs, X509_LU_CRL, nm, &cnt);

	/* Always do lookup to possibly add new CRLs to cache
	 */
	CRYPTO_w_unlock(CRYPTO_LOCK_X509_STORE);
	if (!X509_STORE_get_by_subject(ctx, X509_LU_CRL, nm, &xobj))
		{
		sk_X509_CRL_free(sk);
		return NULL;
		}
	X509_OBJECT_free_contents(&xobj);
	CRYPTO_w_lock(CRYPTO_LOCK_X509_STORE);
	idx = x509_object_idx_cnt(ctx->ctx->objs,X509_LU_CRL, nm, &cnt);
	if (idx < 0)
		{
		CRYPTO_w_unlock(CRYPTO_LOCK_X509_STORE);
		sk_X509_CRL_free(sk);
		return NULL;
		}

	for (i = 0; i < cnt; i++, idx++)
		{
		obj = sk_X509_OBJECT_value(ctx->ctx->objs, idx);
		x = obj->data.crl;
		CRYPTO_add(&x->references, 1, CRYPTO_LOCK_X509_CRL);
		if (!sk_X509_CRL_push(sk, x))
			{
			CRYPTO_w_unlock(CRYPTO_LOCK_X509_STORE);
			X509_CRL_free(x);
			sk_X509_CRL_pop_free(sk, X509_CRL_free);
			return NULL;
			}
		}
	CRYPTO_w_unlock(CRYPTO_LOCK_X509_STORE);
	return sk;
	}

X509_OBJECT *X509_OBJECT_retrieve_match(STACK_OF(X509_OBJECT) *h, X509_OBJECT *x)
	{
	int idx, i;
	X509_OBJECT *obj;
	idx = sk_X509_OBJECT_find(h, x);
	if (idx == -1) return NULL;
	if ((x->type != X509_LU_X509) && (x->type != X509_LU_CRL))
		return sk_X509_OBJECT_value(h, idx);
	for (i = idx; i < sk_X509_OBJECT_num(h); i++)
		{
		obj = sk_X509_OBJECT_value(h, i);
		if (x509_object_cmp((const X509_OBJECT **)&obj, (const X509_OBJECT **)&x))
			return NULL;
		if (x->type == X509_LU_X509)
			{
			if (!X509_cmp(obj->data.x509, x->data.x509))
				return obj;
			}
		else if (x->type == X509_LU_CRL)
			{
			if (!X509_CRL_match(obj->data.crl, x->data.crl))
				return obj;
			}
		else
			return obj;
		}
	return NULL;
	}


/* Try to get issuer certificate from store. Due to limitations
 * of the API this can only retrieve a single certificate matching
 * a given subject name. However it will fill the cache with all
 * matching certificates, so we can examine the cache for all
 * matches.
 *
 * Return values are:
 *  1 lookup successful.
 *  0 certificate not found.
 * -1 some other error.
 */
int X509_STORE_CTX_get1_issuer(X509 **issuer, X509_STORE_CTX *ctx, X509 *x)
	{
	X509_NAME *xn;
	X509_OBJECT obj, *pobj;
	int i, ok, idx, ret;
	xn=X509_get_issuer_name(x);
	ok=X509_STORE_get_by_subject(ctx,X509_LU_X509,xn,&obj);
	if (ok != X509_LU_X509)
		{
		if (ok == X509_LU_RETRY)
			{
			X509_OBJECT_free_contents(&obj);
			X509err(X509_F_X509_STORE_CTX_GET1_ISSUER,X509_R_SHOULD_RETRY);
			return -1;
			}
		else if (ok != X509_LU_FAIL)
			{
			X509_OBJECT_free_contents(&obj);
			/* not good :-(, break anyway */
			return -1;
			}
		return 0;
		}
	/* If certificate matches all OK */
	if (ctx->check_issued(ctx, x, obj.data.x509))
		{
		*issuer = obj.data.x509;
		return 1;
		}
	X509_OBJECT_free_contents(&obj);

	/* Else find index of first cert accepted by 'check_issued' */
	ret = 0;
	CRYPTO_w_lock(CRYPTO_LOCK_X509_STORE);
	idx = X509_OBJECT_idx_by_subject(ctx->ctx->objs, X509_LU_X509, xn);
	if (idx != -1) /* should be true as we've had at least one match */
		{
		/* Look through all matching certs for suitable issuer */
		for (i = idx; i < sk_X509_OBJECT_num(ctx->ctx->objs); i++)
			{
			pobj = sk_X509_OBJECT_value(ctx->ctx->objs, i);
			/* See if we've run past the matches */
			if (pobj->type != X509_LU_X509)
				break;
			if (X509_NAME_cmp(xn, X509_get_subject_name(pobj->data.x509)))
				break;
			if (ctx->check_issued(ctx, x, pobj->data.x509))
				{
				*issuer = pobj->data.x509;
				X509_OBJECT_up_ref_count(pobj);
				ret = 1;
				break;
				}
			}
		}
	CRYPTO_w_unlock(CRYPTO_LOCK_X509_STORE);
	return ret;
	}

int X509_STORE_set_flags(X509_STORE *ctx, unsigned long flags)
	{
	return X509_VERIFY_PARAM_set_flags(ctx->param, flags);
	}

int X509_STORE_set_depth(X509_STORE *ctx, int depth)
	{
	X509_VERIFY_PARAM_set_depth(ctx->param, depth);
	return 1;
	}

int X509_STORE_set_purpose(X509_STORE *ctx, int purpose)
	{
	return X509_VERIFY_PARAM_set_purpose(ctx->param, purpose);
	}

int X509_STORE_set_trust(X509_STORE *ctx, int trust)
	{
	return X509_VERIFY_PARAM_set_trust(ctx->param, trust);
	}

int X509_STORE_set1_param(X509_STORE *ctx, X509_VERIFY_PARAM *param)
	{
	return X509_VERIFY_PARAM_set1(ctx->param, param);
	}

void X509_STORE_set_verify_cb(X509_STORE *ctx,
				  int (*verify_cb)(int, X509_STORE_CTX *))
	{
	ctx->verify_cb = verify_cb;
	}

IMPLEMENT_STACK_OF(X509_LOOKUP)
IMPLEMENT_STACK_OF(X509_OBJECT)
