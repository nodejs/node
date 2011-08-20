/* crypto/x509/x509_vfy.c */
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
#include <time.h>
#include <errno.h>

#include "cryptlib.h"
#include <openssl/crypto.h>
#include <openssl/lhash.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/asn1.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/objects.h>

static int null_callback(int ok,X509_STORE_CTX *e);
static int check_issued(X509_STORE_CTX *ctx, X509 *x, X509 *issuer);
static X509 *find_issuer(X509_STORE_CTX *ctx, STACK_OF(X509) *sk, X509 *x);
static int check_chain_extensions(X509_STORE_CTX *ctx);
static int check_trust(X509_STORE_CTX *ctx);
static int check_revocation(X509_STORE_CTX *ctx);
static int check_cert(X509_STORE_CTX *ctx);
static int check_policy(X509_STORE_CTX *ctx);
static int internal_verify(X509_STORE_CTX *ctx);
const char X509_version[]="X.509" OPENSSL_VERSION_PTEXT;


static int null_callback(int ok, X509_STORE_CTX *e)
	{
	return ok;
	}

#if 0
static int x509_subject_cmp(X509 **a, X509 **b)
	{
	return X509_subject_name_cmp(*a,*b);
	}
#endif

int X509_verify_cert(X509_STORE_CTX *ctx)
	{
	X509 *x,*xtmp,*chain_ss=NULL;
	int bad_chain = 0;
	X509_VERIFY_PARAM *param = ctx->param;
	int depth,i,ok=0;
	int num;
	int (*cb)(int xok,X509_STORE_CTX *xctx);
	STACK_OF(X509) *sktmp=NULL;
	if (ctx->cert == NULL)
		{
		X509err(X509_F_X509_VERIFY_CERT,X509_R_NO_CERT_SET_FOR_US_TO_VERIFY);
		return -1;
		}

	cb=ctx->verify_cb;

	/* first we make sure the chain we are going to build is
	 * present and that the first entry is in place */
	if (ctx->chain == NULL)
		{
		if (	((ctx->chain=sk_X509_new_null()) == NULL) ||
			(!sk_X509_push(ctx->chain,ctx->cert)))
			{
			X509err(X509_F_X509_VERIFY_CERT,ERR_R_MALLOC_FAILURE);
			goto end;
			}
		CRYPTO_add(&ctx->cert->references,1,CRYPTO_LOCK_X509);
		ctx->last_untrusted=1;
		}

	/* We use a temporary STACK so we can chop and hack at it */
	if (ctx->untrusted != NULL
	    && (sktmp=sk_X509_dup(ctx->untrusted)) == NULL)
		{
		X509err(X509_F_X509_VERIFY_CERT,ERR_R_MALLOC_FAILURE);
		goto end;
		}

	num=sk_X509_num(ctx->chain);
	x=sk_X509_value(ctx->chain,num-1);
	depth=param->depth;


	for (;;)
		{
		/* If we have enough, we break */
		if (depth < num) break; /* FIXME: If this happens, we should take
		                         * note of it and, if appropriate, use the
		                         * X509_V_ERR_CERT_CHAIN_TOO_LONG error
		                         * code later.
		                         */

		/* If we are self signed, we break */
		if (ctx->check_issued(ctx, x,x)) break;

		/* If we were passed a cert chain, use it first */
		if (ctx->untrusted != NULL)
			{
			xtmp=find_issuer(ctx, sktmp,x);
			if (xtmp != NULL)
				{
				if (!sk_X509_push(ctx->chain,xtmp))
					{
					X509err(X509_F_X509_VERIFY_CERT,ERR_R_MALLOC_FAILURE);
					goto end;
					}
				CRYPTO_add(&xtmp->references,1,CRYPTO_LOCK_X509);
				(void)sk_X509_delete_ptr(sktmp,xtmp);
				ctx->last_untrusted++;
				x=xtmp;
				num++;
				/* reparse the full chain for
				 * the next one */
				continue;
				}
			}
		break;
		}

	/* at this point, chain should contain a list of untrusted
	 * certificates.  We now need to add at least one trusted one,
	 * if possible, otherwise we complain. */

	/* Examine last certificate in chain and see if it
 	 * is self signed.
 	 */

	i=sk_X509_num(ctx->chain);
	x=sk_X509_value(ctx->chain,i-1);
	if (ctx->check_issued(ctx, x, x))
		{
		/* we have a self signed certificate */
		if (sk_X509_num(ctx->chain) == 1)
			{
			/* We have a single self signed certificate: see if
			 * we can find it in the store. We must have an exact
			 * match to avoid possible impersonation.
			 */
			ok = ctx->get_issuer(&xtmp, ctx, x);
			if ((ok <= 0) || X509_cmp(x, xtmp)) 
				{
				ctx->error=X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT;
				ctx->current_cert=x;
				ctx->error_depth=i-1;
				if (ok == 1) X509_free(xtmp);
				bad_chain = 1;
				ok=cb(0,ctx);
				if (!ok) goto end;
				}
			else 
				{
				/* We have a match: replace certificate with store version
				 * so we get any trust settings.
				 */
				X509_free(x);
				x = xtmp;
				(void)sk_X509_set(ctx->chain, i - 1, x);
				ctx->last_untrusted=0;
				}
			}
		else
			{
			/* extract and save self signed certificate for later use */
			chain_ss=sk_X509_pop(ctx->chain);
			ctx->last_untrusted--;
			num--;
			x=sk_X509_value(ctx->chain,num-1);
			}
		}

	/* We now lookup certs from the certificate store */
	for (;;)
		{
		/* If we have enough, we break */
		if (depth < num) break;

		/* If we are self signed, we break */
		if (ctx->check_issued(ctx,x,x)) break;

		ok = ctx->get_issuer(&xtmp, ctx, x);

		if (ok < 0) return ok;
		if (ok == 0) break;

		x = xtmp;
		if (!sk_X509_push(ctx->chain,x))
			{
			X509_free(xtmp);
			X509err(X509_F_X509_VERIFY_CERT,ERR_R_MALLOC_FAILURE);
			return 0;
			}
		num++;
		}

	/* we now have our chain, lets check it... */

	/* Is last certificate looked up self signed? */
	if (!ctx->check_issued(ctx,x,x))
		{
		if ((chain_ss == NULL) || !ctx->check_issued(ctx, x, chain_ss))
			{
			if (ctx->last_untrusted >= num)
				ctx->error=X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY;
			else
				ctx->error=X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT;
			ctx->current_cert=x;
			}
		else
			{

			sk_X509_push(ctx->chain,chain_ss);
			num++;
			ctx->last_untrusted=num;
			ctx->current_cert=chain_ss;
			ctx->error=X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN;
			chain_ss=NULL;
			}

		ctx->error_depth=num-1;
		bad_chain = 1;
		ok=cb(0,ctx);
		if (!ok) goto end;
		}

	/* We have the chain complete: now we need to check its purpose */
	ok = check_chain_extensions(ctx);

	if (!ok) goto end;

	/* The chain extensions are OK: check trust */

	if (param->trust > 0) ok = check_trust(ctx);

	if (!ok) goto end;

	/* We may as well copy down any DSA parameters that are required */
	X509_get_pubkey_parameters(NULL,ctx->chain);

	/* Check revocation status: we do this after copying parameters
	 * because they may be needed for CRL signature verification.
	 */

	ok = ctx->check_revocation(ctx);
	if(!ok) goto end;

	/* At this point, we have a chain and need to verify it */
	if (ctx->verify != NULL)
		ok=ctx->verify(ctx);
	else
		ok=internal_verify(ctx);
	if(!ok) goto end;

#ifndef OPENSSL_NO_RFC3779
	/* RFC 3779 path validation, now that CRL check has been done */
	ok = v3_asid_validate_path(ctx);
	if (!ok) goto end;
	ok = v3_addr_validate_path(ctx);
	if (!ok) goto end;
#endif

	/* If we get this far evaluate policies */
	if (!bad_chain && (ctx->param->flags & X509_V_FLAG_POLICY_CHECK))
		ok = ctx->check_policy(ctx);
	if(!ok) goto end;
	if (0)
		{
end:
		X509_get_pubkey_parameters(NULL,ctx->chain);
		}
	if (sktmp != NULL) sk_X509_free(sktmp);
	if (chain_ss != NULL) X509_free(chain_ss);
	return ok;
	}


/* Given a STACK_OF(X509) find the issuer of cert (if any)
 */

static X509 *find_issuer(X509_STORE_CTX *ctx, STACK_OF(X509) *sk, X509 *x)
{
	int i;
	X509 *issuer;
	for (i = 0; i < sk_X509_num(sk); i++)
		{
		issuer = sk_X509_value(sk, i);
		if (ctx->check_issued(ctx, x, issuer))
			return issuer;
		}
	return NULL;
}

/* Given a possible certificate and issuer check them */

static int check_issued(X509_STORE_CTX *ctx, X509 *x, X509 *issuer)
{
	int ret;
	ret = X509_check_issued(issuer, x);
	if (ret == X509_V_OK)
		return 1;
	/* If we haven't asked for issuer errors don't set ctx */
	if (!(ctx->param->flags & X509_V_FLAG_CB_ISSUER_CHECK))
		return 0;

	ctx->error = ret;
	ctx->current_cert = x;
	ctx->current_issuer = issuer;
	return ctx->verify_cb(0, ctx);
	return 0;
}

/* Alternative lookup method: look from a STACK stored in other_ctx */

static int get_issuer_sk(X509 **issuer, X509_STORE_CTX *ctx, X509 *x)
{
	*issuer = find_issuer(ctx, ctx->other_ctx, x);
	if (*issuer)
		{
		CRYPTO_add(&(*issuer)->references,1,CRYPTO_LOCK_X509);
		return 1;
		}
	else
		return 0;
}
	

/* Check a certificate chains extensions for consistency
 * with the supplied purpose
 */

static int check_chain_extensions(X509_STORE_CTX *ctx)
{
#ifdef OPENSSL_NO_CHAIN_VERIFY
	return 1;
#else
	int i, ok=0, must_be_ca, plen = 0;
	X509 *x;
	int (*cb)(int xok,X509_STORE_CTX *xctx);
	int proxy_path_length = 0;
	int allow_proxy_certs =
		!!(ctx->param->flags & X509_V_FLAG_ALLOW_PROXY_CERTS);
	cb=ctx->verify_cb;

	/* must_be_ca can have 1 of 3 values:
	   -1: we accept both CA and non-CA certificates, to allow direct
	       use of self-signed certificates (which are marked as CA).
	   0:  we only accept non-CA certificates.  This is currently not
	       used, but the possibility is present for future extensions.
	   1:  we only accept CA certificates.  This is currently used for
	       all certificates in the chain except the leaf certificate.
	*/
	must_be_ca = -1;

	/* A hack to keep people who don't want to modify their software
	   happy */
	if (getenv("OPENSSL_ALLOW_PROXY_CERTS"))
		allow_proxy_certs = 1;

	/* Check all untrusted certificates */
	for (i = 0; i < ctx->last_untrusted; i++)
		{
		int ret;
		x = sk_X509_value(ctx->chain, i);
		if (!(ctx->param->flags & X509_V_FLAG_IGNORE_CRITICAL)
			&& (x->ex_flags & EXFLAG_CRITICAL))
			{
			ctx->error = X509_V_ERR_UNHANDLED_CRITICAL_EXTENSION;
			ctx->error_depth = i;
			ctx->current_cert = x;
			ok=cb(0,ctx);
			if (!ok) goto end;
			}
		if (!allow_proxy_certs && (x->ex_flags & EXFLAG_PROXY))
			{
			ctx->error = X509_V_ERR_PROXY_CERTIFICATES_NOT_ALLOWED;
			ctx->error_depth = i;
			ctx->current_cert = x;
			ok=cb(0,ctx);
			if (!ok) goto end;
			}
		ret = X509_check_ca(x);
		switch(must_be_ca)
			{
		case -1:
			if ((ctx->param->flags & X509_V_FLAG_X509_STRICT)
				&& (ret != 1) && (ret != 0))
				{
				ret = 0;
				ctx->error = X509_V_ERR_INVALID_CA;
				}
			else
				ret = 1;
			break;
		case 0:
			if (ret != 0)
				{
				ret = 0;
				ctx->error = X509_V_ERR_INVALID_NON_CA;
				}
			else
				ret = 1;
			break;
		default:
			if ((ret == 0)
				|| ((ctx->param->flags & X509_V_FLAG_X509_STRICT)
					&& (ret != 1)))
				{
				ret = 0;
				ctx->error = X509_V_ERR_INVALID_CA;
				}
			else
				ret = 1;
			break;
			}
		if (ret == 0)
			{
			ctx->error_depth = i;
			ctx->current_cert = x;
			ok=cb(0,ctx);
			if (!ok) goto end;
			}
		if (ctx->param->purpose > 0)
			{
			ret = X509_check_purpose(x, ctx->param->purpose,
				must_be_ca > 0);
			if ((ret == 0)
				|| ((ctx->param->flags & X509_V_FLAG_X509_STRICT)
					&& (ret != 1)))
				{
				ctx->error = X509_V_ERR_INVALID_PURPOSE;
				ctx->error_depth = i;
				ctx->current_cert = x;
				ok=cb(0,ctx);
				if (!ok) goto end;
				}
			}
		/* Check pathlen if not self issued */
		if ((i > 1) && !(x->ex_flags & EXFLAG_SI)
			   && (x->ex_pathlen != -1)
			   && (plen > (x->ex_pathlen + proxy_path_length + 1)))
			{
			ctx->error = X509_V_ERR_PATH_LENGTH_EXCEEDED;
			ctx->error_depth = i;
			ctx->current_cert = x;
			ok=cb(0,ctx);
			if (!ok) goto end;
			}
		/* Increment path length if not self issued */
		if (!(x->ex_flags & EXFLAG_SI))
			plen++;
		/* If this certificate is a proxy certificate, the next
		   certificate must be another proxy certificate or a EE
		   certificate.  If not, the next certificate must be a
		   CA certificate.  */
		if (x->ex_flags & EXFLAG_PROXY)
			{
			if (x->ex_pcpathlen != -1 && i > x->ex_pcpathlen)
				{
				ctx->error =
					X509_V_ERR_PROXY_PATH_LENGTH_EXCEEDED;
				ctx->error_depth = i;
				ctx->current_cert = x;
				ok=cb(0,ctx);
				if (!ok) goto end;
				}
			proxy_path_length++;
			must_be_ca = 0;
			}
		else
			must_be_ca = 1;
		}
	ok = 1;
 end:
	return ok;
#endif
}

static int check_trust(X509_STORE_CTX *ctx)
{
#ifdef OPENSSL_NO_CHAIN_VERIFY
	return 1;
#else
	int i, ok;
	X509 *x;
	int (*cb)(int xok,X509_STORE_CTX *xctx);
	cb=ctx->verify_cb;
/* For now just check the last certificate in the chain */
	i = sk_X509_num(ctx->chain) - 1;
	x = sk_X509_value(ctx->chain, i);
	ok = X509_check_trust(x, ctx->param->trust, 0);
	if (ok == X509_TRUST_TRUSTED)
		return 1;
	ctx->error_depth = i;
	ctx->current_cert = x;
	if (ok == X509_TRUST_REJECTED)
		ctx->error = X509_V_ERR_CERT_REJECTED;
	else
		ctx->error = X509_V_ERR_CERT_UNTRUSTED;
	ok = cb(0, ctx);
	return ok;
#endif
}

static int check_revocation(X509_STORE_CTX *ctx)
	{
	int i, last, ok;
	if (!(ctx->param->flags & X509_V_FLAG_CRL_CHECK))
		return 1;
	if (ctx->param->flags & X509_V_FLAG_CRL_CHECK_ALL)
		last = sk_X509_num(ctx->chain) - 1;
	else
		last = 0;
	for(i = 0; i <= last; i++)
		{
		ctx->error_depth = i;
		ok = check_cert(ctx);
		if (!ok) return ok;
		}
	return 1;
	}

static int check_cert(X509_STORE_CTX *ctx)
	{
	X509_CRL *crl = NULL;
	X509 *x;
	int ok, cnum;
	cnum = ctx->error_depth;
	x = sk_X509_value(ctx->chain, cnum);
	ctx->current_cert = x;
	/* Try to retrieve relevant CRL */
	ok = ctx->get_crl(ctx, &crl, x);
	/* If error looking up CRL, nothing we can do except
	 * notify callback
	 */
	if(!ok)
		{
		ctx->error = X509_V_ERR_UNABLE_TO_GET_CRL;
		ok = ctx->verify_cb(0, ctx);
		goto err;
		}
	ctx->current_crl = crl;
	ok = ctx->check_crl(ctx, crl);
	if (!ok) goto err;
	ok = ctx->cert_crl(ctx, crl, x);
	err:
	ctx->current_crl = NULL;
	X509_CRL_free(crl);
	return ok;

	}

/* Check CRL times against values in X509_STORE_CTX */

static int check_crl_time(X509_STORE_CTX *ctx, X509_CRL *crl, int notify)
	{
	time_t *ptime;
	int i;
	ctx->current_crl = crl;
	if (ctx->param->flags & X509_V_FLAG_USE_CHECK_TIME)
		ptime = &ctx->param->check_time;
	else
		ptime = NULL;

	i=X509_cmp_time(X509_CRL_get_lastUpdate(crl), ptime);
	if (i == 0)
		{
		ctx->error=X509_V_ERR_ERROR_IN_CRL_LAST_UPDATE_FIELD;
		if (!notify || !ctx->verify_cb(0, ctx))
			return 0;
		}

	if (i > 0)
		{
		ctx->error=X509_V_ERR_CRL_NOT_YET_VALID;
		if (!notify || !ctx->verify_cb(0, ctx))
			return 0;
		}

	if(X509_CRL_get_nextUpdate(crl))
		{
		i=X509_cmp_time(X509_CRL_get_nextUpdate(crl), ptime);

		if (i == 0)
			{
			ctx->error=X509_V_ERR_ERROR_IN_CRL_NEXT_UPDATE_FIELD;
			if (!notify || !ctx->verify_cb(0, ctx))
				return 0;
			}

		if (i < 0)
			{
			ctx->error=X509_V_ERR_CRL_HAS_EXPIRED;
			if (!notify || !ctx->verify_cb(0, ctx))
				return 0;
			}
		}

	ctx->current_crl = NULL;

	return 1;
	}

/* Lookup CRLs from the supplied list. Look for matching isser name
 * and validity. If we can't find a valid CRL return the last one
 * with matching name. This gives more meaningful error codes. Otherwise
 * we'd get a CRL not found error if a CRL existed with matching name but
 * was invalid.
 */

static int get_crl_sk(X509_STORE_CTX *ctx, X509_CRL **pcrl,
			X509_NAME *nm, STACK_OF(X509_CRL) *crls)
	{
	int i;
	X509_CRL *crl, *best_crl = NULL;
	for (i = 0; i < sk_X509_CRL_num(crls); i++)
		{
		crl = sk_X509_CRL_value(crls, i);
		if (X509_NAME_cmp(nm, X509_CRL_get_issuer(crl)))
			continue;
		if (check_crl_time(ctx, crl, 0))
			{
			*pcrl = crl;
			CRYPTO_add(&crl->references, 1, CRYPTO_LOCK_X509);
			return 1;
			}
		best_crl = crl;
		}
	if (best_crl)
		{
		*pcrl = best_crl;
		CRYPTO_add(&best_crl->references, 1, CRYPTO_LOCK_X509);
		}
		
	return 0;
	}

/* Retrieve CRL corresponding to certificate: currently just a
 * subject lookup: maybe use AKID later...
 */
static int get_crl(X509_STORE_CTX *ctx, X509_CRL **pcrl, X509 *x)
	{
	int ok;
	X509_CRL *crl = NULL;
	X509_OBJECT xobj;
	X509_NAME *nm;
	nm = X509_get_issuer_name(x);
	ok = get_crl_sk(ctx, &crl, nm, ctx->crls);
	if (ok)
		{
		*pcrl = crl;
		return 1;
		}

	ok = X509_STORE_get_by_subject(ctx, X509_LU_CRL, nm, &xobj);

	if (!ok)
		{
		/* If we got a near match from get_crl_sk use that */
		if (crl)
			{
			*pcrl = crl;
			return 1;
			}
		return 0;
		}

	*pcrl = xobj.data.crl;
	if (crl)
		X509_CRL_free(crl);
	return 1;
	}

/* Check CRL validity */
static int check_crl(X509_STORE_CTX *ctx, X509_CRL *crl)
	{
	X509 *issuer = NULL;
	EVP_PKEY *ikey = NULL;
	int ok = 0, chnum, cnum;
	cnum = ctx->error_depth;
	chnum = sk_X509_num(ctx->chain) - 1;
	/* Find CRL issuer: if not last certificate then issuer
	 * is next certificate in chain.
	 */
	if(cnum < chnum)
		issuer = sk_X509_value(ctx->chain, cnum + 1);
	else
		{
		issuer = sk_X509_value(ctx->chain, chnum);
		/* If not self signed, can't check signature */
		if(!ctx->check_issued(ctx, issuer, issuer))
			{
			ctx->error = X509_V_ERR_UNABLE_TO_GET_CRL_ISSUER;
			ok = ctx->verify_cb(0, ctx);
			if(!ok) goto err;
			}
		}

	if(issuer)
		{
		/* Check for cRLSign bit if keyUsage present */
		if ((issuer->ex_flags & EXFLAG_KUSAGE) &&
			!(issuer->ex_kusage & KU_CRL_SIGN))
			{
			ctx->error = X509_V_ERR_KEYUSAGE_NO_CRL_SIGN;
			ok = ctx->verify_cb(0, ctx);
			if(!ok) goto err;
			}

		/* Attempt to get issuer certificate public key */
		ikey = X509_get_pubkey(issuer);

		if(!ikey)
			{
			ctx->error=X509_V_ERR_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY;
			ok = ctx->verify_cb(0, ctx);
			if (!ok) goto err;
			}
		else
			{
			/* Verify CRL signature */
			if(X509_CRL_verify(crl, ikey) <= 0)
				{
				ctx->error=X509_V_ERR_CRL_SIGNATURE_FAILURE;
				ok = ctx->verify_cb(0, ctx);
				if (!ok) goto err;
				}
			}
		}

	ok = check_crl_time(ctx, crl, 1);
	if (!ok)
		goto err;

	ok = 1;

	err:
	EVP_PKEY_free(ikey);
	return ok;
	}

/* Check certificate against CRL */
static int cert_crl(X509_STORE_CTX *ctx, X509_CRL *crl, X509 *x)
	{
	int idx, ok;
	X509_REVOKED rtmp;
	STACK_OF(X509_EXTENSION) *exts;
	X509_EXTENSION *ext;
	/* Look for serial number of certificate in CRL */
	rtmp.serialNumber = X509_get_serialNumber(x);
	/* Sort revoked into serial number order if not already sorted.
	 * Do this under a lock to avoid race condition.
 	 */
	if (!sk_X509_REVOKED_is_sorted(crl->crl->revoked))
		{
		CRYPTO_w_lock(CRYPTO_LOCK_X509_CRL);
		sk_X509_REVOKED_sort(crl->crl->revoked);
		CRYPTO_w_unlock(CRYPTO_LOCK_X509_CRL);
		}
	idx = sk_X509_REVOKED_find(crl->crl->revoked, &rtmp);
	/* If found assume revoked: want something cleverer than
	 * this to handle entry extensions in V2 CRLs.
	 */
	if(idx >= 0)
		{
		ctx->error = X509_V_ERR_CERT_REVOKED;
		ok = ctx->verify_cb(0, ctx);
		if (!ok) return 0;
		}

	if (ctx->param->flags & X509_V_FLAG_IGNORE_CRITICAL)
		return 1;

	/* See if we have any critical CRL extensions: since we
	 * currently don't handle any CRL extensions the CRL must be
	 * rejected. 
	 * This code accesses the X509_CRL structure directly: applications
	 * shouldn't do this.
	 */

	exts = crl->crl->extensions;

	for (idx = 0; idx < sk_X509_EXTENSION_num(exts); idx++)
		{
		ext = sk_X509_EXTENSION_value(exts, idx);
		if (ext->critical > 0)
			{
			ctx->error =
				X509_V_ERR_UNHANDLED_CRITICAL_CRL_EXTENSION;
			ok = ctx->verify_cb(0, ctx);
			if(!ok) return 0;
			break;
			}
		}
	return 1;
	}

static int check_policy(X509_STORE_CTX *ctx)
	{
	int ret;
	ret = X509_policy_check(&ctx->tree, &ctx->explicit_policy, ctx->chain,
				ctx->param->policies, ctx->param->flags);
	if (ret == 0)
		{
		X509err(X509_F_CHECK_POLICY,ERR_R_MALLOC_FAILURE);
		return 0;
		}
	/* Invalid or inconsistent extensions */
	if (ret == -1)
		{
		/* Locate certificates with bad extensions and notify
		 * callback.
		 */
		X509 *x;
		int i;
		for (i = 1; i < sk_X509_num(ctx->chain); i++)
			{
			x = sk_X509_value(ctx->chain, i);
			if (!(x->ex_flags & EXFLAG_INVALID_POLICY))
				continue;
			ctx->current_cert = x;
			ctx->error = X509_V_ERR_INVALID_POLICY_EXTENSION;
			ret = ctx->verify_cb(0, ctx);
			}
		return 1;
		}
	if (ret == -2)
		{
		ctx->current_cert = NULL;
		ctx->error = X509_V_ERR_NO_EXPLICIT_POLICY;
		return ctx->verify_cb(0, ctx);
		}

	if (ctx->param->flags & X509_V_FLAG_NOTIFY_POLICY)
		{
		ctx->current_cert = NULL;
		ctx->error = X509_V_OK;
		if (!ctx->verify_cb(2, ctx))
			return 0;
		}

	return 1;
	}

static int check_cert_time(X509_STORE_CTX *ctx, X509 *x)
	{
	time_t *ptime;
	int i;

	if (ctx->param->flags & X509_V_FLAG_USE_CHECK_TIME)
		ptime = &ctx->param->check_time;
	else
		ptime = NULL;

	i=X509_cmp_time(X509_get_notBefore(x), ptime);
	if (i == 0)
		{
		ctx->error=X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD;
		ctx->current_cert=x;
		if (!ctx->verify_cb(0, ctx))
			return 0;
		}

	if (i > 0)
		{
		ctx->error=X509_V_ERR_CERT_NOT_YET_VALID;
		ctx->current_cert=x;
		if (!ctx->verify_cb(0, ctx))
			return 0;
		}

	i=X509_cmp_time(X509_get_notAfter(x), ptime);
	if (i == 0)
		{
		ctx->error=X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD;
		ctx->current_cert=x;
		if (!ctx->verify_cb(0, ctx))
			return 0;
		}

	if (i < 0)
		{
		ctx->error=X509_V_ERR_CERT_HAS_EXPIRED;
		ctx->current_cert=x;
		if (!ctx->verify_cb(0, ctx))
			return 0;
		}

	return 1;
	}

static int internal_verify(X509_STORE_CTX *ctx)
	{
	int ok=0,n;
	X509 *xs,*xi;
	EVP_PKEY *pkey=NULL;
	int (*cb)(int xok,X509_STORE_CTX *xctx);

	cb=ctx->verify_cb;

	n=sk_X509_num(ctx->chain);
	ctx->error_depth=n-1;
	n--;
	xi=sk_X509_value(ctx->chain,n);

	if (ctx->check_issued(ctx, xi, xi))
		xs=xi;
	else
		{
		if (n <= 0)
			{
			ctx->error=X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE;
			ctx->current_cert=xi;
			ok=cb(0,ctx);
			goto end;
			}
		else
			{
			n--;
			ctx->error_depth=n;
			xs=sk_X509_value(ctx->chain,n);
			}
		}

/*	ctx->error=0;  not needed */
	while (n >= 0)
		{
		ctx->error_depth=n;

		/* Skip signature check for self signed certificates unless
		 * explicitly asked for. It doesn't add any security and
		 * just wastes time.
		 */
		if (!xs->valid && (xs != xi || (ctx->param->flags & X509_V_FLAG_CHECK_SS_SIGNATURE)))
			{
			if ((pkey=X509_get_pubkey(xi)) == NULL)
				{
				ctx->error=X509_V_ERR_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY;
				ctx->current_cert=xi;
				ok=(*cb)(0,ctx);
				if (!ok) goto end;
				}
			else if (X509_verify(xs,pkey) <= 0)
				{
				ctx->error=X509_V_ERR_CERT_SIGNATURE_FAILURE;
				ctx->current_cert=xs;
				ok=(*cb)(0,ctx);
				if (!ok)
					{
					EVP_PKEY_free(pkey);
					goto end;
					}
				}
			EVP_PKEY_free(pkey);
			pkey=NULL;
			}

		xs->valid = 1;

		ok = check_cert_time(ctx, xs);
		if (!ok)
			goto end;

		/* The last error (if any) is still in the error value */
		ctx->current_issuer=xi;
		ctx->current_cert=xs;
		ok=(*cb)(1,ctx);
		if (!ok) goto end;

		n--;
		if (n >= 0)
			{
			xi=xs;
			xs=sk_X509_value(ctx->chain,n);
			}
		}
	ok=1;
end:
	return ok;
	}

int X509_cmp_current_time(ASN1_TIME *ctm)
{
	return X509_cmp_time(ctm, NULL);
}

int X509_cmp_time(ASN1_TIME *ctm, time_t *cmp_time)
	{
	char *str;
	ASN1_TIME atm;
	long offset;
	char buff1[24],buff2[24],*p;
	int i,j;

	p=buff1;
	i=ctm->length;
	str=(char *)ctm->data;
	if (ctm->type == V_ASN1_UTCTIME)
		{
		if ((i < 11) || (i > 17)) return 0;
		memcpy(p,str,10);
		p+=10;
		str+=10;
		}
	else
		{
		if (i < 13) return 0;
		memcpy(p,str,12);
		p+=12;
		str+=12;
		}

	if ((*str == 'Z') || (*str == '-') || (*str == '+'))
		{ *(p++)='0'; *(p++)='0'; }
	else
		{ 
		*(p++)= *(str++);
		*(p++)= *(str++);
		/* Skip any fractional seconds... */
		if (*str == '.')
			{
			str++;
			while ((*str >= '0') && (*str <= '9')) str++;
			}
		
		}
	*(p++)='Z';
	*(p++)='\0';

	if (*str == 'Z')
		offset=0;
	else
		{
		if ((*str != '+') && (*str != '-'))
			return 0;
		offset=((str[1]-'0')*10+(str[2]-'0'))*60;
		offset+=(str[3]-'0')*10+(str[4]-'0');
		if (*str == '-')
			offset= -offset;
		}
	atm.type=ctm->type;
	atm.length=sizeof(buff2);
	atm.data=(unsigned char *)buff2;

	if (X509_time_adj(&atm,-offset*60, cmp_time) == NULL)
		return 0;

	if (ctm->type == V_ASN1_UTCTIME)
		{
		i=(buff1[0]-'0')*10+(buff1[1]-'0');
		if (i < 50) i+=100; /* cf. RFC 2459 */
		j=(buff2[0]-'0')*10+(buff2[1]-'0');
		if (j < 50) j+=100;

		if (i < j) return -1;
		if (i > j) return 1;
		}
	i=strcmp(buff1,buff2);
	if (i == 0) /* wait a second then return younger :-) */
		return -1;
	else
		return i;
	}

ASN1_TIME *X509_gmtime_adj(ASN1_TIME *s, long adj)
{
	return X509_time_adj(s, adj, NULL);
}

ASN1_TIME *X509_time_adj(ASN1_TIME *s, long adj, time_t *in_tm)
	{
	time_t t;
	int type = -1;

	if (in_tm) t = *in_tm;
	else time(&t);

	t+=adj;
	if (s) type = s->type;
	if (type == V_ASN1_UTCTIME) return ASN1_UTCTIME_set(s,t);
	if (type == V_ASN1_GENERALIZEDTIME) return ASN1_GENERALIZEDTIME_set(s, t);
	return ASN1_TIME_set(s, t);
	}

int X509_get_pubkey_parameters(EVP_PKEY *pkey, STACK_OF(X509) *chain)
	{
	EVP_PKEY *ktmp=NULL,*ktmp2;
	int i,j;

	if ((pkey != NULL) && !EVP_PKEY_missing_parameters(pkey)) return 1;

	for (i=0; i<sk_X509_num(chain); i++)
		{
		ktmp=X509_get_pubkey(sk_X509_value(chain,i));
		if (ktmp == NULL)
			{
			X509err(X509_F_X509_GET_PUBKEY_PARAMETERS,X509_R_UNABLE_TO_GET_CERTS_PUBLIC_KEY);
			return 0;
			}
		if (!EVP_PKEY_missing_parameters(ktmp))
			break;
		else
			{
			EVP_PKEY_free(ktmp);
			ktmp=NULL;
			}
		}
	if (ktmp == NULL)
		{
		X509err(X509_F_X509_GET_PUBKEY_PARAMETERS,X509_R_UNABLE_TO_FIND_PARAMETERS_IN_CHAIN);
		return 0;
		}

	/* first, populate the other certs */
	for (j=i-1; j >= 0; j--)
		{
		ktmp2=X509_get_pubkey(sk_X509_value(chain,j));
		EVP_PKEY_copy_parameters(ktmp2,ktmp);
		EVP_PKEY_free(ktmp2);
		}
	
	if (pkey != NULL) EVP_PKEY_copy_parameters(pkey,ktmp);
	EVP_PKEY_free(ktmp);
	return 1;
	}

int X509_STORE_CTX_get_ex_new_index(long argl, void *argp, CRYPTO_EX_new *new_func,
	     CRYPTO_EX_dup *dup_func, CRYPTO_EX_free *free_func)
	{
	/* This function is (usually) called only once, by
	 * SSL_get_ex_data_X509_STORE_CTX_idx (ssl/ssl_cert.c). */
	return CRYPTO_get_ex_new_index(CRYPTO_EX_INDEX_X509_STORE_CTX, argl, argp,
			new_func, dup_func, free_func);
	}

int X509_STORE_CTX_set_ex_data(X509_STORE_CTX *ctx, int idx, void *data)
	{
	return CRYPTO_set_ex_data(&ctx->ex_data,idx,data);
	}

void *X509_STORE_CTX_get_ex_data(X509_STORE_CTX *ctx, int idx)
	{
	return CRYPTO_get_ex_data(&ctx->ex_data,idx);
	}

int X509_STORE_CTX_get_error(X509_STORE_CTX *ctx)
	{
	return ctx->error;
	}

void X509_STORE_CTX_set_error(X509_STORE_CTX *ctx, int err)
	{
	ctx->error=err;
	}

int X509_STORE_CTX_get_error_depth(X509_STORE_CTX *ctx)
	{
	return ctx->error_depth;
	}

X509 *X509_STORE_CTX_get_current_cert(X509_STORE_CTX *ctx)
	{
	return ctx->current_cert;
	}

STACK_OF(X509) *X509_STORE_CTX_get_chain(X509_STORE_CTX *ctx)
	{
	return ctx->chain;
	}

STACK_OF(X509) *X509_STORE_CTX_get1_chain(X509_STORE_CTX *ctx)
	{
	int i;
	X509 *x;
	STACK_OF(X509) *chain;
	if (!ctx->chain || !(chain = sk_X509_dup(ctx->chain))) return NULL;
	for (i = 0; i < sk_X509_num(chain); i++)
		{
		x = sk_X509_value(chain, i);
		CRYPTO_add(&x->references, 1, CRYPTO_LOCK_X509);
		}
	return chain;
	}

void X509_STORE_CTX_set_cert(X509_STORE_CTX *ctx, X509 *x)
	{
	ctx->cert=x;
	}

void X509_STORE_CTX_set_chain(X509_STORE_CTX *ctx, STACK_OF(X509) *sk)
	{
	ctx->untrusted=sk;
	}

void X509_STORE_CTX_set0_crls(X509_STORE_CTX *ctx, STACK_OF(X509_CRL) *sk)
	{
	ctx->crls=sk;
	}

int X509_STORE_CTX_set_purpose(X509_STORE_CTX *ctx, int purpose)
	{
	return X509_STORE_CTX_purpose_inherit(ctx, 0, purpose, 0);
	}

int X509_STORE_CTX_set_trust(X509_STORE_CTX *ctx, int trust)
	{
	return X509_STORE_CTX_purpose_inherit(ctx, 0, 0, trust);
	}

/* This function is used to set the X509_STORE_CTX purpose and trust
 * values. This is intended to be used when another structure has its
 * own trust and purpose values which (if set) will be inherited by
 * the ctx. If they aren't set then we will usually have a default
 * purpose in mind which should then be used to set the trust value.
 * An example of this is SSL use: an SSL structure will have its own
 * purpose and trust settings which the application can set: if they
 * aren't set then we use the default of SSL client/server.
 */

int X509_STORE_CTX_purpose_inherit(X509_STORE_CTX *ctx, int def_purpose,
				int purpose, int trust)
{
	int idx;
	/* If purpose not set use default */
	if (!purpose) purpose = def_purpose;
	/* If we have a purpose then check it is valid */
	if (purpose)
		{
		X509_PURPOSE *ptmp;
		idx = X509_PURPOSE_get_by_id(purpose);
		if (idx == -1)
			{
			X509err(X509_F_X509_STORE_CTX_PURPOSE_INHERIT,
						X509_R_UNKNOWN_PURPOSE_ID);
			return 0;
			}
		ptmp = X509_PURPOSE_get0(idx);
		if (ptmp->trust == X509_TRUST_DEFAULT)
			{
			idx = X509_PURPOSE_get_by_id(def_purpose);
			if (idx == -1)
				{
				X509err(X509_F_X509_STORE_CTX_PURPOSE_INHERIT,
						X509_R_UNKNOWN_PURPOSE_ID);
				return 0;
				}
			ptmp = X509_PURPOSE_get0(idx);
			}
		/* If trust not set then get from purpose default */
		if (!trust) trust = ptmp->trust;
		}
	if (trust)
		{
		idx = X509_TRUST_get_by_id(trust);
		if (idx == -1)
			{
			X509err(X509_F_X509_STORE_CTX_PURPOSE_INHERIT,
						X509_R_UNKNOWN_TRUST_ID);
			return 0;
			}
		}

	if (purpose && !ctx->param->purpose) ctx->param->purpose = purpose;
	if (trust && !ctx->param->trust) ctx->param->trust = trust;
	return 1;
}

X509_STORE_CTX *X509_STORE_CTX_new(void)
{
	X509_STORE_CTX *ctx;
	ctx = (X509_STORE_CTX *)OPENSSL_malloc(sizeof(X509_STORE_CTX));
	if (!ctx)
		{
		X509err(X509_F_X509_STORE_CTX_NEW,ERR_R_MALLOC_FAILURE);
		return NULL;
		}
	memset(ctx, 0, sizeof(X509_STORE_CTX));
	return ctx;
}

void X509_STORE_CTX_free(X509_STORE_CTX *ctx)
{
	X509_STORE_CTX_cleanup(ctx);
	OPENSSL_free(ctx);
}

int X509_STORE_CTX_init(X509_STORE_CTX *ctx, X509_STORE *store, X509 *x509,
	     STACK_OF(X509) *chain)
	{
	int ret = 1;
	ctx->ctx=store;
	ctx->current_method=0;
	ctx->cert=x509;
	ctx->untrusted=chain;
	ctx->crls = NULL;
	ctx->last_untrusted=0;
	ctx->other_ctx=NULL;
	ctx->valid=0;
	ctx->chain=NULL;
	ctx->error=0;
	ctx->explicit_policy=0;
	ctx->error_depth=0;
	ctx->current_cert=NULL;
	ctx->current_issuer=NULL;
	ctx->tree = NULL;

	ctx->param = X509_VERIFY_PARAM_new();

	if (!ctx->param)
		{
		X509err(X509_F_X509_STORE_CTX_INIT,ERR_R_MALLOC_FAILURE);
		return 0;
		}

	/* Inherit callbacks and flags from X509_STORE if not set
	 * use defaults.
	 */


	if (store)
		ret = X509_VERIFY_PARAM_inherit(ctx->param, store->param);
	else
		ctx->param->inh_flags |= X509_VP_FLAG_DEFAULT|X509_VP_FLAG_ONCE;

	if (store)
		{
		ctx->verify_cb = store->verify_cb;
		ctx->cleanup = store->cleanup;
		}
	else
		ctx->cleanup = 0;

	if (ret)
		ret = X509_VERIFY_PARAM_inherit(ctx->param,
					X509_VERIFY_PARAM_lookup("default"));

	if (ret == 0)
		{
		X509err(X509_F_X509_STORE_CTX_INIT,ERR_R_MALLOC_FAILURE);
		return 0;
		}

	if (store && store->check_issued)
		ctx->check_issued = store->check_issued;
	else
		ctx->check_issued = check_issued;

	if (store && store->get_issuer)
		ctx->get_issuer = store->get_issuer;
	else
		ctx->get_issuer = X509_STORE_CTX_get1_issuer;

	if (store && store->verify_cb)
		ctx->verify_cb = store->verify_cb;
	else
		ctx->verify_cb = null_callback;

	if (store && store->verify)
		ctx->verify = store->verify;
	else
		ctx->verify = internal_verify;

	if (store && store->check_revocation)
		ctx->check_revocation = store->check_revocation;
	else
		ctx->check_revocation = check_revocation;

	if (store && store->get_crl)
		ctx->get_crl = store->get_crl;
	else
		ctx->get_crl = get_crl;

	if (store && store->check_crl)
		ctx->check_crl = store->check_crl;
	else
		ctx->check_crl = check_crl;

	if (store && store->cert_crl)
		ctx->cert_crl = store->cert_crl;
	else
		ctx->cert_crl = cert_crl;

	ctx->check_policy = check_policy;


	/* This memset() can't make any sense anyway, so it's removed. As
	 * X509_STORE_CTX_cleanup does a proper "free" on the ex_data, we put a
	 * corresponding "new" here and remove this bogus initialisation. */
	/* memset(&(ctx->ex_data),0,sizeof(CRYPTO_EX_DATA)); */
	if(!CRYPTO_new_ex_data(CRYPTO_EX_INDEX_X509_STORE_CTX, ctx,
				&(ctx->ex_data)))
		{
		OPENSSL_free(ctx);
		X509err(X509_F_X509_STORE_CTX_INIT,ERR_R_MALLOC_FAILURE);
		return 0;
		}
	return 1;
	}

/* Set alternative lookup method: just a STACK of trusted certificates.
 * This avoids X509_STORE nastiness where it isn't needed.
 */

void X509_STORE_CTX_trusted_stack(X509_STORE_CTX *ctx, STACK_OF(X509) *sk)
{
	ctx->other_ctx = sk;
	ctx->get_issuer = get_issuer_sk;
}

void X509_STORE_CTX_cleanup(X509_STORE_CTX *ctx)
	{
	if (ctx->cleanup) ctx->cleanup(ctx);
	if (ctx->param != NULL)
		{
		X509_VERIFY_PARAM_free(ctx->param);
		ctx->param=NULL;
		}
	if (ctx->tree != NULL)
		{
		X509_policy_tree_free(ctx->tree);
		ctx->tree=NULL;
		}
	if (ctx->chain != NULL)
		{
		sk_X509_pop_free(ctx->chain,X509_free);
		ctx->chain=NULL;
		}
	CRYPTO_free_ex_data(CRYPTO_EX_INDEX_X509_STORE_CTX, ctx, &(ctx->ex_data));
	memset(&ctx->ex_data,0,sizeof(CRYPTO_EX_DATA));
	}

void X509_STORE_CTX_set_depth(X509_STORE_CTX *ctx, int depth)
	{
	X509_VERIFY_PARAM_set_depth(ctx->param, depth);
	}

void X509_STORE_CTX_set_flags(X509_STORE_CTX *ctx, unsigned long flags)
	{
	X509_VERIFY_PARAM_set_flags(ctx->param, flags);
	}

void X509_STORE_CTX_set_time(X509_STORE_CTX *ctx, unsigned long flags, time_t t)
	{
	X509_VERIFY_PARAM_set_time(ctx->param, t);
	}

void X509_STORE_CTX_set_verify_cb(X509_STORE_CTX *ctx,
				  int (*verify_cb)(int, X509_STORE_CTX *))
	{
	ctx->verify_cb=verify_cb;
	}

X509_POLICY_TREE *X509_STORE_CTX_get0_policy_tree(X509_STORE_CTX *ctx)
	{
	return ctx->tree;
	}

int X509_STORE_CTX_get_explicit_policy(X509_STORE_CTX *ctx)
	{
	return ctx->explicit_policy;
	}

int X509_STORE_CTX_set_default(X509_STORE_CTX *ctx, const char *name)
	{
	const X509_VERIFY_PARAM *param;
	param = X509_VERIFY_PARAM_lookup(name);
	if (!param)
		return 0;
	return X509_VERIFY_PARAM_inherit(ctx->param, param);
	}

X509_VERIFY_PARAM *X509_STORE_CTX_get0_param(X509_STORE_CTX *ctx)
	{
	return ctx->param;
	}

void X509_STORE_CTX_set0_param(X509_STORE_CTX *ctx, X509_VERIFY_PARAM *param)
	{
	if (ctx->param)
		X509_VERIFY_PARAM_free(ctx->param);
	ctx->param = param;
	}

IMPLEMENT_STACK_OF(X509)
IMPLEMENT_ASN1_SET_OF(X509)

IMPLEMENT_STACK_OF(X509_NAME)

IMPLEMENT_STACK_OF(X509_ATTRIBUTE)
IMPLEMENT_ASN1_SET_OF(X509_ATTRIBUTE)
