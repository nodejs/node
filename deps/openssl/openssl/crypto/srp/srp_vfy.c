/* crypto/srp/srp_vfy.c */
/* Written by Christophe Renou (christophe.renou@edelweb.fr) with 
 * the precious help of Peter Sylvester (peter.sylvester@edelweb.fr) 
 * for the EdelKey project and contributed to the OpenSSL project 2004.
 */
/* ====================================================================
 * Copyright (c) 2004 The OpenSSL Project.  All rights reserved.
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
#ifndef OPENSSL_NO_SRP
#include "cryptlib.h"
#include "srp_lcl.h"
#include <openssl/srp.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <openssl/rand.h>
#include <openssl/txt_db.h>

#define SRP_RANDOM_SALT_LEN 20
#define MAX_LEN 2500

static char b64table[] =
  "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz./";

/* the following two conversion routines have been inspired by code from Stanford */ 

/*
 * Convert a base64 string into raw byte array representation.
 */
static int t_fromb64(unsigned char *a, const char *src)
	{
	char *loc;
	int i, j;
	int size;

	while(*src && (*src == ' ' || *src == '\t' || *src == '\n'))
		++src;
	size = strlen(src);
	i = 0;
	while(i < size)
		{
		loc = strchr(b64table, src[i]);
		if(loc == (char *) 0) break;
		else a[i] = loc - b64table;
		++i;
		}
	size = i;
	i = size - 1;
	j = size;
	while(1)
		{
		a[j] = a[i];
		if(--i < 0) break;
		a[j] |= (a[i] & 3) << 6;
		--j;
		a[j] = (unsigned char) ((a[i] & 0x3c) >> 2);
		if(--i < 0) break;
		a[j] |= (a[i] & 0xf) << 4;
		--j;
		a[j] = (unsigned char) ((a[i] & 0x30) >> 4);
		if(--i < 0) break;
		a[j] |= (a[i] << 2);

		a[--j] = 0;
		if(--i < 0) break;
		}
	while(a[j] == 0 && j <= size) ++j;
	i = 0;
	while (j <= size) a[i++] = a[j++];
	return i;
	}


/*
 * Convert a raw byte string into a null-terminated base64 ASCII string.
 */
static char *t_tob64(char *dst, const unsigned char *src, int size)
	{
	int c, pos = size % 3;
	unsigned char b0 = 0, b1 = 0, b2 = 0, notleading = 0;
	char *olddst = dst;

	switch(pos)
		{
	case 1:
		b2 = src[0];
		break;
	case 2:
		b1 = src[0];
		b2 = src[1];
		break;
		}

	while(1)
		{
		c = (b0 & 0xfc) >> 2;
		if(notleading || c != 0)
			{
			*dst++ = b64table[c];
			notleading = 1;
			}
		c = ((b0 & 3) << 4) | ((b1 & 0xf0) >> 4);
		if(notleading || c != 0)
			{
			*dst++ = b64table[c];
			notleading = 1;
			}
		c = ((b1 & 0xf) << 2) | ((b2 & 0xc0) >> 6);
		if(notleading || c != 0)
			{
			*dst++ = b64table[c];
			notleading = 1;
			}
		c = b2 & 0x3f;
		if(notleading || c != 0)
			{
			*dst++ = b64table[c];
			notleading = 1;
			}
		if(pos >= size) break;
		else
			{
			b0 = src[pos++];
			b1 = src[pos++];
			b2 = src[pos++];
			}
		}

	*dst++ = '\0';
	return olddst;
	}

static void SRP_user_pwd_free(SRP_user_pwd *user_pwd)
	{
	if (user_pwd == NULL) 
		return;
	BN_free(user_pwd->s);
	BN_clear_free(user_pwd->v);
	OPENSSL_free(user_pwd->id);
	OPENSSL_free(user_pwd->info);
	OPENSSL_free(user_pwd);
	}

static SRP_user_pwd *SRP_user_pwd_new()
	{
	SRP_user_pwd *ret = OPENSSL_malloc(sizeof(SRP_user_pwd));
	if (ret == NULL)
		return NULL;								
	ret->N = NULL;
	ret->g = NULL;	
	ret->s = NULL;
	ret->v = NULL;
	ret->id = NULL ;
	ret->info = NULL;
	return ret;
	}

static void SRP_user_pwd_set_gN(SRP_user_pwd *vinfo, const BIGNUM *g,
				const BIGNUM *N)
	{
	vinfo->N = N;
	vinfo->g = g;	
	}

static int SRP_user_pwd_set_ids(SRP_user_pwd *vinfo, const char *id,
				const char *info)
	{
	if (id != NULL && NULL == (vinfo->id = BUF_strdup(id)))
		return 0;
	return (info == NULL || NULL != (vinfo->info = BUF_strdup(info))) ;
	}

static int SRP_user_pwd_set_sv(SRP_user_pwd *vinfo, const char *s,
			       const char *v)
	{
	unsigned char tmp[MAX_LEN];
	int len;

	if (strlen(s) > MAX_LEN || strlen(v) > MAX_LEN) 
		return 0; 
	len = t_fromb64(tmp, v);
	if (NULL == (vinfo->v = BN_bin2bn(tmp, len, NULL)) )
		return 0;
	len = t_fromb64(tmp, s);
	return ((vinfo->s = BN_bin2bn(tmp, len, NULL)) != NULL) ;
	}

static int SRP_user_pwd_set_sv_BN(SRP_user_pwd *vinfo, BIGNUM *s, BIGNUM *v)
	{
	vinfo->v = v;
	vinfo->s = s;
	return (vinfo->s != NULL && vinfo->v != NULL) ;
	}

SRP_VBASE *SRP_VBASE_new(char *seed_key)
	{
	SRP_VBASE *vb = (SRP_VBASE *) OPENSSL_malloc(sizeof(SRP_VBASE));

	if (vb == NULL)
		return NULL;
	if (!(vb->users_pwd = sk_SRP_user_pwd_new_null()) ||
		!(vb->gN_cache = sk_SRP_gN_cache_new_null()))
		{
		OPENSSL_free(vb);
		return NULL;
		}
	vb->default_g = NULL;
	vb->default_N = NULL;
	vb->seed_key = NULL;
	if ((seed_key != NULL) && 
		(vb->seed_key = BUF_strdup(seed_key)) == NULL)
		{
		sk_SRP_user_pwd_free(vb->users_pwd);
		sk_SRP_gN_cache_free(vb->gN_cache);
		OPENSSL_free(vb);
		return NULL;
		}
	return vb;
	}


int SRP_VBASE_free(SRP_VBASE *vb)
	{
	sk_SRP_user_pwd_pop_free(vb->users_pwd,SRP_user_pwd_free);
	sk_SRP_gN_cache_free(vb->gN_cache);
	OPENSSL_free(vb->seed_key);
	OPENSSL_free(vb);
	return 0;
	}


static SRP_gN_cache *SRP_gN_new_init(const char *ch)
	{
	unsigned char tmp[MAX_LEN];
	int len;

	SRP_gN_cache *newgN = (SRP_gN_cache *)OPENSSL_malloc(sizeof(SRP_gN_cache));
	if (newgN == NULL)
		return NULL;

	if ((newgN->b64_bn = BUF_strdup(ch)) == NULL)
		goto err;

	len = t_fromb64(tmp, ch);
	if ((newgN->bn = BN_bin2bn(tmp, len, NULL)))
		return newgN;

	OPENSSL_free(newgN->b64_bn);
err:
	OPENSSL_free(newgN);
	return NULL;
	}


static void SRP_gN_free(SRP_gN_cache *gN_cache)
	{
	if (gN_cache == NULL)
		return;
	OPENSSL_free(gN_cache->b64_bn);
	BN_free(gN_cache->bn);
	OPENSSL_free(gN_cache);
	}

static SRP_gN *SRP_get_gN_by_id(const char *id, STACK_OF(SRP_gN) *gN_tab)
	{
	int i;

	SRP_gN *gN;
	if (gN_tab != NULL) 
	for(i = 0; i < sk_SRP_gN_num(gN_tab); i++)
		{
		gN = sk_SRP_gN_value(gN_tab, i);
		if (gN && (id == NULL || strcmp(gN->id,id)==0))
			return gN;
		}
	
	return SRP_get_default_gN(id);
	}

static BIGNUM *SRP_gN_place_bn(STACK_OF(SRP_gN_cache) *gN_cache, char *ch)
	{
	int i;
	if (gN_cache == NULL)
		return NULL;

	/* search if we have already one... */
	for(i = 0; i < sk_SRP_gN_cache_num(gN_cache); i++)
		{
		SRP_gN_cache *cache = sk_SRP_gN_cache_value(gN_cache, i);
		if (strcmp(cache->b64_bn,ch)==0)
			return cache->bn;
		}
		{		/* it is the first time that we find it */
		SRP_gN_cache *newgN = SRP_gN_new_init(ch);
		if (newgN)
			{
			if (sk_SRP_gN_cache_insert(gN_cache,newgN,0)>0)
				return newgN->bn;
			SRP_gN_free(newgN);
			}
		}
	return NULL;
	}

/* this function parses verifier file. Format is:
 * string(index):base64(N):base64(g):0
 * string(username):base64(v):base64(salt):int(index)
 */


int SRP_VBASE_init(SRP_VBASE *vb, char *verifier_file)
	{
	int error_code ;
	STACK_OF(SRP_gN) *SRP_gN_tab = sk_SRP_gN_new_null();
	char *last_index = NULL;
	int i;
	char **pp;

	SRP_gN *gN = NULL;
	SRP_user_pwd *user_pwd = NULL ;

	TXT_DB *tmpdb = NULL;
	BIO *in = BIO_new(BIO_s_file());

	error_code = SRP_ERR_OPEN_FILE;

	if (in == NULL || BIO_read_filename(in,verifier_file) <= 0)
		goto err;

	error_code = SRP_ERR_VBASE_INCOMPLETE_FILE;

	if ((tmpdb =TXT_DB_read(in,DB_NUMBER)) == NULL)
		goto err;

	error_code = SRP_ERR_MEMORY;


	if (vb->seed_key)
		{
		last_index = SRP_get_default_gN(NULL)->id;
		}
	for (i = 0; i < sk_OPENSSL_PSTRING_num(tmpdb->data); i++)
		{
		pp = sk_OPENSSL_PSTRING_value(tmpdb->data,i);
		if (pp[DB_srptype][0] == DB_SRP_INDEX)
			{
			/*we add this couple in the internal Stack */

			if ((gN = (SRP_gN *)OPENSSL_malloc(sizeof(SRP_gN))) == NULL) 
 				goto err;

			if  (!(gN->id = BUF_strdup(pp[DB_srpid]))
	                ||  !(gN->N = SRP_gN_place_bn(vb->gN_cache,pp[DB_srpverifier]))
			||  !(gN->g = SRP_gN_place_bn(vb->gN_cache,pp[DB_srpsalt]))
			||  sk_SRP_gN_insert(SRP_gN_tab,gN,0) == 0)
				goto err;

			gN = NULL;

			if (vb->seed_key != NULL)
				{
				last_index = pp[DB_srpid];
				}
			}
		else if (pp[DB_srptype][0] == DB_SRP_VALID)
			{
			/* it is a user .... */
			SRP_gN *lgN;
			if ((lgN = SRP_get_gN_by_id(pp[DB_srpgN],SRP_gN_tab))!=NULL)
				{
				error_code = SRP_ERR_MEMORY;
				if ((user_pwd = SRP_user_pwd_new()) == NULL) 
					goto err;
				
				SRP_user_pwd_set_gN(user_pwd,lgN->g,lgN->N);
				if (!SRP_user_pwd_set_ids(user_pwd, pp[DB_srpid],pp[DB_srpinfo]))
					goto err;
				
				error_code = SRP_ERR_VBASE_BN_LIB;
				if (!SRP_user_pwd_set_sv(user_pwd, pp[DB_srpsalt],pp[DB_srpverifier]))
					goto err;

				if (sk_SRP_user_pwd_insert(vb->users_pwd, user_pwd, 0) == 0)
					goto err;
				user_pwd = NULL; /* abandon responsability */
				}
			}
		}
	
	if (last_index != NULL)
		{
		/* this means that we want to simulate a default user */

		if (((gN = SRP_get_gN_by_id(last_index,SRP_gN_tab))==NULL))
			{
			error_code = SRP_ERR_VBASE_BN_LIB;
			goto err;
			}
		vb->default_g = gN->g ;
		vb->default_N = gN->N ;
		gN = NULL ;
		}
	error_code = SRP_NO_ERROR;

 err:
	/* there may be still some leaks to fix, if this fails, the application terminates most likely */

	if (gN != NULL)
		{
		OPENSSL_free(gN->id);
		OPENSSL_free(gN);
		}

	SRP_user_pwd_free(user_pwd);

	if (tmpdb) TXT_DB_free(tmpdb);
	if (in) BIO_free_all(in);

	sk_SRP_gN_free(SRP_gN_tab);

	return error_code;

	}


SRP_user_pwd *SRP_VBASE_get_by_user(SRP_VBASE *vb, char *username)
	{
	int i;
	SRP_user_pwd *user;
	unsigned char digv[SHA_DIGEST_LENGTH];
	unsigned char digs[SHA_DIGEST_LENGTH];
	EVP_MD_CTX ctxt;

	if (vb == NULL)
		return NULL;
	for(i = 0; i < sk_SRP_user_pwd_num(vb->users_pwd); i++)
		{
		user = sk_SRP_user_pwd_value(vb->users_pwd, i);
		if (strcmp(user->id,username)==0)
			return user;
		}
	if ((vb->seed_key == NULL) ||
		(vb->default_g == NULL) ||
		(vb->default_N == NULL))
		return NULL;

/* if the user is unknown we set parameters as well if we have a seed_key */

	if ((user = SRP_user_pwd_new()) == NULL) 
		return NULL;

	SRP_user_pwd_set_gN(user,vb->default_g,vb->default_N);
				
	if (!SRP_user_pwd_set_ids(user,username,NULL))
		goto err;
		
	RAND_pseudo_bytes(digv, SHA_DIGEST_LENGTH);
	EVP_MD_CTX_init(&ctxt);
	EVP_DigestInit_ex(&ctxt, EVP_sha1(), NULL);
	EVP_DigestUpdate(&ctxt, vb->seed_key, strlen(vb->seed_key));
	EVP_DigestUpdate(&ctxt, username, strlen(username));
	EVP_DigestFinal_ex(&ctxt, digs, NULL);
	EVP_MD_CTX_cleanup(&ctxt);
	if (SRP_user_pwd_set_sv_BN(user, BN_bin2bn(digs,SHA_DIGEST_LENGTH,NULL), BN_bin2bn(digv,SHA_DIGEST_LENGTH, NULL))) 
		return user;

err:    SRP_user_pwd_free(user);
	return NULL;
	}


/*
   create a verifier (*salt,*verifier,g and N are in base64)
*/
char *SRP_create_verifier(const char *user, const char *pass, char **salt,
			  char **verifier, const char *N, const char *g)
	{
	int len;
	char * result=NULL;
	char *vf;
	BIGNUM *N_bn = NULL, *g_bn = NULL, *s = NULL, *v = NULL;
	unsigned char tmp[MAX_LEN];
	unsigned char tmp2[MAX_LEN];
	char * defgNid = NULL;

	if ((user == NULL)||
		(pass == NULL)||
		(salt == NULL)||
		(verifier == NULL))
		goto err;

	if (N)
		{
		if (!(len = t_fromb64(tmp, N))) goto err;
		N_bn = BN_bin2bn(tmp, len, NULL);
		if (!(len = t_fromb64(tmp, g))) goto err;
		g_bn = BN_bin2bn(tmp, len, NULL);
		defgNid = "*";
		}
	else
		{ 
		SRP_gN * gN = SRP_get_gN_by_id(g, NULL) ;
		if (gN == NULL)
			goto err;
		N_bn = gN->N;
		g_bn = gN->g;
		defgNid = gN->id;
		}

	if (*salt == NULL)
		{
		RAND_pseudo_bytes(tmp2, SRP_RANDOM_SALT_LEN);

		s = BN_bin2bn(tmp2, SRP_RANDOM_SALT_LEN, NULL);
		}
	else
		{
		if (!(len = t_fromb64(tmp2, *salt)))
			goto err;
		s = BN_bin2bn(tmp2, len, NULL);
		}


	if(!SRP_create_verifier_BN(user, pass, &s, &v, N_bn, g_bn)) goto err;

	BN_bn2bin(v,tmp);
	if (((vf = OPENSSL_malloc(BN_num_bytes(v)*2)) == NULL))
		goto err;
	t_tob64(vf, tmp, BN_num_bytes(v));

	*verifier = vf;
	if (*salt == NULL)
		{
		char *tmp_salt;

		if ((tmp_salt = OPENSSL_malloc(SRP_RANDOM_SALT_LEN * 2)) == NULL)
			{
			OPENSSL_free(vf);
			goto err;
			}
		t_tob64(tmp_salt, tmp2, SRP_RANDOM_SALT_LEN);
		*salt = tmp_salt;
		}

	result=defgNid;

err:
	if(N)
		{
		BN_free(N_bn);
		BN_free(g_bn);
		}
	return result;
	}

/*
   create a verifier (*salt,*verifier,g and N are BIGNUMs)
*/
int SRP_create_verifier_BN(const char *user, const char *pass, BIGNUM **salt, BIGNUM **verifier, BIGNUM *N, BIGNUM *g)
	{
	int result=0;
	BIGNUM *x = NULL;
	BN_CTX *bn_ctx = BN_CTX_new();
	unsigned char tmp2[MAX_LEN];

	if ((user == NULL)||
		(pass == NULL)||
		(salt == NULL)||
		(verifier == NULL)||
		(N == NULL)||
		(g == NULL)||
		(bn_ctx == NULL))
		goto err;

	srp_bn_print(N);
	srp_bn_print(g);

	if (*salt == NULL)
		{
		RAND_pseudo_bytes(tmp2, SRP_RANDOM_SALT_LEN);

		*salt = BN_bin2bn(tmp2,SRP_RANDOM_SALT_LEN,NULL);
		}

	x = SRP_Calc_x(*salt,user,pass);

	*verifier = BN_new();
	if(*verifier == NULL) goto err;

	if (!BN_mod_exp(*verifier,g,x,N,bn_ctx))
		{
		BN_clear_free(*verifier);
		goto err;
		}

	srp_bn_print(*verifier);

	result=1;

err:

	BN_clear_free(x);
	BN_CTX_free(bn_ctx);
	return result;
	}



#endif
