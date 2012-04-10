/**********************************************************************
 *                          gost2001.c                                *
 *             Copyright (c) 2005-2006 Cryptocom LTD                  *
 *         This file is distributed under the same license as OpenSSL *
 *                                                                    *
 *          Implementation of GOST R 34.10-2001      				  *
 *          Requires OpenSSL 0.9.9 for compilation                    *
 **********************************************************************/
#include "gost_lcl.h"
#include "gost_params.h"
#include <string.h>
#include <openssl/rand.h>
#include <openssl/ecdsa.h>
#include <openssl/err.h>
#include "e_gost_err.h"
#ifdef DEBUG_SIGN
extern 
void dump_signature(const char *message,const unsigned char *buffer,size_t len);
void dump_dsa_sig(const char *message, DSA_SIG *sig);
#else

#define dump_signature(a,b,c)
#define dump_dsa_sig(a,b)
#endif

/*
 * Fills EC_KEY structure hidden in the app_data field of DSA structure
 * with parameter information, extracted from parameter array in
 * params.c file.
 *
 * Also fils DSA->q field with copy of EC_GROUP order field to make
 * DSA_size function work
 */ 
int fill_GOST2001_params(EC_KEY *eckey, int nid)
	{
	R3410_2001_params *params = R3410_2001_paramset;
	EC_GROUP *grp=NULL;
	BIGNUM *p=NULL,*q=NULL,*a=NULL,*b=NULL,*x=NULL,*y=NULL;
	EC_POINT *P=NULL;
	BN_CTX *ctx=BN_CTX_new();
	int ok=0;
	
	BN_CTX_start(ctx);
	p=BN_CTX_get(ctx);
	a=BN_CTX_get(ctx);
	b=BN_CTX_get(ctx);
	x=BN_CTX_get(ctx);
	y=BN_CTX_get(ctx);
	q=BN_CTX_get(ctx);
	while (params->nid!=NID_undef && params->nid != nid) params++;
	if (params->nid == NID_undef)
		{
		GOSTerr(GOST_F_FILL_GOST2001_PARAMS,GOST_R_UNSUPPORTED_PARAMETER_SET);
		goto err;
		}	
	BN_hex2bn(&p,params->p);
	BN_hex2bn(&a,params->a);
	BN_hex2bn(&b,params->b);
	
	grp = EC_GROUP_new_curve_GFp(p,a,b,ctx);

	P = EC_POINT_new(grp);

	BN_hex2bn(&x,params->x);
	BN_hex2bn(&y,params->y);
	EC_POINT_set_affine_coordinates_GFp(grp,P,x,y,ctx);
	BN_hex2bn(&q,params->q);
#ifdef DEBUG_KEYS
	fprintf(stderr,"Set params index %d oid %s\nq=",
		(params-R3410_2001_paramset),OBJ_nid2sn(params->nid));
	BN_print_fp(stderr,q);
	fprintf(stderr,"\n");
#endif	

	EC_GROUP_set_generator(grp,P,q,NULL);
	EC_GROUP_set_curve_name(grp,params->nid);

	EC_KEY_set_group(eckey,grp);
	ok=1;
	err:
	EC_POINT_free(P);
	EC_GROUP_free(grp);
	BN_CTX_end(ctx);
	BN_CTX_free(ctx);
	return ok;
	}	


/*
 * Computes gost2001 signature as DSA_SIG structure 
 *
 *
 */ 
DSA_SIG *gost2001_do_sign(const unsigned char *dgst,int dlen, EC_KEY *eckey)
	{
	DSA_SIG *newsig = NULL;
	BIGNUM *md = hashsum2bn(dgst);
	BIGNUM *order = NULL;
	const EC_GROUP *group;
	const BIGNUM *priv_key;
	BIGNUM *r=NULL,*s=NULL,*X=NULL,*tmp=NULL,*tmp2=NULL, *k=NULL,*e=NULL;
	EC_POINT *C=NULL;
	BN_CTX *ctx = BN_CTX_new();	
	BN_CTX_start(ctx);
	OPENSSL_assert(dlen==32);
	newsig=DSA_SIG_new();
	if (!newsig) 
		{
		GOSTerr(GOST_F_GOST2001_DO_SIGN,GOST_R_NO_MEMORY);
		goto err;
		}	
	group = EC_KEY_get0_group(eckey);
	order=BN_CTX_get(ctx);
	EC_GROUP_get_order(group,order,ctx);
	priv_key = EC_KEY_get0_private_key(eckey);
	e = BN_CTX_get(ctx);
	BN_mod(e,md,order,ctx);
#ifdef DEBUG_SIGN
	fprintf(stderr,"digest as bignum=");
	BN_print_fp(stderr,md);
	fprintf(stderr,"\ndigest mod q=");
	BN_print_fp(stderr,e);
	fprintf(stderr,"\n");
#endif		
	if (BN_is_zero(e))
		{
		BN_one(e);
		}   
	k =BN_CTX_get(ctx);
	C=EC_POINT_new(group);
	do 
		{
		do 
			{
			if (!BN_rand_range(k,order)) 
				{
				GOSTerr(GOST_F_GOST2001_DO_SIGN,GOST_R_RANDOM_NUMBER_GENERATOR_FAILED);
				DSA_SIG_free(newsig);
				newsig = NULL;
				goto err;
				}	
			if (!EC_POINT_mul(group,C,k,NULL,NULL,ctx))
				{
				GOSTerr(GOST_F_GOST2001_DO_SIGN,ERR_R_EC_LIB);
				DSA_SIG_free(newsig);
				newsig = NULL;
				goto err;
				}	
			if (!X) X=BN_CTX_get(ctx);
			if (!EC_POINT_get_affine_coordinates_GFp(group,C,X,NULL,ctx))
				{
				GOSTerr(GOST_F_GOST2001_DO_SIGN,ERR_R_EC_LIB);
				DSA_SIG_free(newsig);
				newsig = NULL;
				goto err;
				}	
			if (!r) r=BN_CTX_get(ctx);
			BN_nnmod(r,X,order,ctx);
			}
		while (BN_is_zero(r));
		/* s =  (r*priv_key+k*e) mod order */
		if (!tmp) tmp = BN_CTX_get(ctx);
		BN_mod_mul(tmp,priv_key,r,order,ctx);
		if (!tmp2) tmp2 = BN_CTX_get(ctx);
		BN_mod_mul(tmp2,k,e,order,ctx);
		if (!s) s=BN_CTX_get(ctx);
		BN_mod_add(s,tmp,tmp2,order,ctx);
		}
	while (BN_is_zero(s));	

	newsig->s=BN_dup(s);
	newsig->r=BN_dup(r);
	err:			
	BN_CTX_end(ctx);
	BN_CTX_free(ctx);
	EC_POINT_free(C);
	BN_free(md);
	return newsig;
	}
/*
 * Verifies gost 2001 signature
 *
 */ 
int gost2001_do_verify(const unsigned char *dgst,int dgst_len,
	DSA_SIG *sig, EC_KEY *ec)
	{
	BN_CTX *ctx=BN_CTX_new();
	const EC_GROUP *group = EC_KEY_get0_group(ec);
	BIGNUM *order;
	BIGNUM *md = NULL,*e=NULL,*R=NULL,*v=NULL,*z1=NULL,*z2=NULL;
	BIGNUM *X=NULL,*tmp=NULL;
	EC_POINT *C = NULL;
	const EC_POINT *pub_key=NULL;
	int ok=0;

	BN_CTX_start(ctx);
	order = BN_CTX_get(ctx);
	e = BN_CTX_get(ctx);
	z1 = BN_CTX_get(ctx);
	z2 = BN_CTX_get(ctx);
	tmp = BN_CTX_get(ctx);
	X= BN_CTX_get(ctx);	
	R=BN_CTX_get(ctx);
	v=BN_CTX_get(ctx);
	
	EC_GROUP_get_order(group,order,ctx);
	pub_key = EC_KEY_get0_public_key(ec);
	if (BN_is_zero(sig->s) || BN_is_zero(sig->r) ||
		(BN_cmp(sig->s,order)>=1) || (BN_cmp(sig->r,order)>=1)) 
		{
		GOSTerr(GOST_F_GOST2001_DO_VERIFY,GOST_R_SIGNATURE_PARTS_GREATER_THAN_Q);
		goto err;

		}
	md = hashsum2bn(dgst);

	BN_mod(e,md,order,ctx);
#ifdef DEBUG_SIGN
	fprintf(stderr,"digest as bignum: ");
	BN_print_fp(stderr,md);
	fprintf(stderr,"\ndigest mod q: ");
	BN_print_fp(stderr,e);
#endif	
	if (BN_is_zero(e)) BN_one(e);
	v=BN_mod_inverse(v,e,order,ctx);
	BN_mod_mul(z1,sig->s,v,order,ctx);
	BN_sub(tmp,order,sig->r);
	BN_mod_mul(z2,tmp,v,order,ctx);
#ifdef DEBUG_SIGN
	fprintf(stderr,"\nInverted digest value: ");
	BN_print_fp(stderr,v);
	fprintf(stderr,"\nz1: ");
	BN_print_fp(stderr,z1);
	fprintf(stderr,"\nz2: ");
	BN_print_fp(stderr,z2);
#endif	
	C = EC_POINT_new(group);
	if (!EC_POINT_mul(group,C,z1,pub_key,z2,ctx)) 
		{	
		GOSTerr(GOST_F_GOST2001_DO_VERIFY,ERR_R_EC_LIB);
		goto err;
		}	
	if (!EC_POINT_get_affine_coordinates_GFp(group,C,X,NULL,ctx)) 
		{
		GOSTerr(GOST_F_GOST2001_DO_VERIFY,ERR_R_EC_LIB);
		goto err;
		}
	BN_mod(R,X,order,ctx);
#ifdef DEBUG_SIGN
	fprintf(stderr,"\nX=");
	BN_print_fp(stderr,X);
	fprintf(stderr,"\nX mod q=");
	BN_print_fp(stderr,R);
	fprintf(stderr,"\n");
#endif	
	if (BN_cmp(R,sig->r)!=0)
		{
		GOSTerr(GOST_F_GOST2001_DO_VERIFY,GOST_R_SIGNATURE_MISMATCH);
		}
	else
		{
		ok = 1;
		}
	err:
	EC_POINT_free(C);
	BN_CTX_end(ctx);
	BN_CTX_free(ctx);
	BN_free(md);
	return ok;
	}
/*
 * Computes GOST R 34.10-2001 public key
 *
 *
 */ 
int gost2001_compute_public(EC_KEY *ec) 
	{
	const EC_GROUP *group = EC_KEY_get0_group(ec);
	EC_POINT *pub_key=NULL;
	const BIGNUM *priv_key=NULL;
	BN_CTX *ctx=NULL;
	int ok=0;

	if (!group)
		{
		GOSTerr(GOST_F_GOST2001_COMPUTE_PUBLIC,GOST_R_KEY_IS_NOT_INITIALIZED);
		return 0;
		}	
	ctx=BN_CTX_new();
	BN_CTX_start(ctx);
	if (!(priv_key=EC_KEY_get0_private_key(ec))) 
		{
		GOSTerr(GOST_F_GOST2001_COMPUTE_PUBLIC,ERR_R_EC_LIB);
		goto err;
		}	

	pub_key = EC_POINT_new(group);
	if (!EC_POINT_mul(group,pub_key,priv_key,NULL,NULL,ctx)) 
		{
		GOSTerr(GOST_F_GOST2001_COMPUTE_PUBLIC,ERR_R_EC_LIB);
		goto err;
		}	
	if (!EC_KEY_set_public_key(ec,pub_key))
		{
		GOSTerr(GOST_F_GOST2001_COMPUTE_PUBLIC,ERR_R_EC_LIB);
		goto err;
		}	
	ok = 256;
	err:
	BN_CTX_end(ctx);
	EC_POINT_free(pub_key);
	BN_CTX_free(ctx);
	return ok;
	}
/*
 * 
 * Generates GOST R 34.10-2001 keypair
 *
 *
 */ 
int gost2001_keygen(EC_KEY *ec)
	{
	BIGNUM *order = BN_new(),*d=BN_new();
	const EC_GROUP *group = EC_KEY_get0_group(ec);
	EC_GROUP_get_order(group,order,NULL);
	
	do 
		{
		if (!BN_rand_range(d,order)) 
			{
			GOSTerr(GOST_F_GOST2001_KEYGEN,GOST_R_RANDOM_NUMBER_GENERATOR_FAILED);
			BN_free(d);
			BN_free(order);
			return 0;
			}	
		}
	while (BN_is_zero(d));
	EC_KEY_set_private_key(ec,d);
	BN_free(d);
	BN_free(order);
	return gost2001_compute_public(ec);
	}

