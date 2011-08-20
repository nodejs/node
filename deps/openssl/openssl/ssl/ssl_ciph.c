/* ssl/ssl_ciph.c */
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
 * Copyright (c) 1998-2006 The OpenSSL Project.  All rights reserved.
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
#include <openssl/objects.h>
#ifndef OPENSSL_NO_COMP
#include <openssl/comp.h>
#endif

#include "ssl_locl.h"

#define SSL_ENC_DES_IDX		0
#define SSL_ENC_3DES_IDX	1
#define SSL_ENC_RC4_IDX		2
#define SSL_ENC_RC2_IDX		3
#define SSL_ENC_IDEA_IDX	4
#define SSL_ENC_eFZA_IDX	5
#define SSL_ENC_NULL_IDX	6
#define SSL_ENC_AES128_IDX	7
#define SSL_ENC_AES256_IDX	8
#define SSL_ENC_CAMELLIA128_IDX	9
#define SSL_ENC_CAMELLIA256_IDX	10
#define SSL_ENC_SEED_IDX    	11
#define SSL_ENC_NUM_IDX		12


static const EVP_CIPHER *ssl_cipher_methods[SSL_ENC_NUM_IDX]={
	NULL,NULL,NULL,NULL,NULL,NULL,
	};

#define SSL_COMP_NULL_IDX	0
#define SSL_COMP_ZLIB_IDX	1
#define SSL_COMP_NUM_IDX	2

static STACK_OF(SSL_COMP) *ssl_comp_methods=NULL;

#define SSL_MD_MD5_IDX	0
#define SSL_MD_SHA1_IDX	1
#define SSL_MD_NUM_IDX	2
static const EVP_MD *ssl_digest_methods[SSL_MD_NUM_IDX]={
	NULL,NULL,
	};

#define CIPHER_ADD	1
#define CIPHER_KILL	2
#define CIPHER_DEL	3
#define CIPHER_ORD	4
#define CIPHER_SPECIAL	5

typedef struct cipher_order_st
	{
	SSL_CIPHER *cipher;
	int active;
	int dead;
	struct cipher_order_st *next,*prev;
	} CIPHER_ORDER;

static const SSL_CIPHER cipher_aliases[]={
	/* Don't include eNULL unless specifically enabled. */
	/* Don't include ECC in ALL because these ciphers are not yet official. */
	{0,SSL_TXT_ALL, 0,SSL_ALL & ~SSL_eNULL & ~SSL_kECDH & ~SSL_kECDHE, SSL_ALL ,0,0,0,SSL_ALL,SSL_ALL}, /* must be first */
	/* TODO: COMPLEMENT OF ALL and COMPLEMENT OF DEFAULT do not have ECC cipher suites handled properly. */
	{0,SSL_TXT_CMPALL,0,SSL_eNULL,0,0,0,0,SSL_ENC_MASK,0},  /* COMPLEMENT OF ALL */
	{0,SSL_TXT_CMPDEF,0,SSL_ADH, 0,0,0,0,SSL_AUTH_MASK,0},
	{0,SSL_TXT_kKRB5,0,SSL_kKRB5,0,0,0,0,SSL_MKEY_MASK,0},  /* VRS Kerberos5 */
	{0,SSL_TXT_kRSA,0,SSL_kRSA,  0,0,0,0,SSL_MKEY_MASK,0},
	{0,SSL_TXT_kDHr,0,SSL_kDHr,  0,0,0,0,SSL_MKEY_MASK,0},
	{0,SSL_TXT_kDHd,0,SSL_kDHd,  0,0,0,0,SSL_MKEY_MASK,0},
	{0,SSL_TXT_kEDH,0,SSL_kEDH,  0,0,0,0,SSL_MKEY_MASK,0},
	{0,SSL_TXT_kFZA,0,SSL_kFZA,  0,0,0,0,SSL_MKEY_MASK,0},
	{0,SSL_TXT_DH,	0,SSL_DH,    0,0,0,0,SSL_MKEY_MASK,0},
	{0,SSL_TXT_ECC,	0,(SSL_kECDH|SSL_kECDHE), 0,0,0,0,SSL_MKEY_MASK,0},
	{0,SSL_TXT_EDH,	0,SSL_EDH,   0,0,0,0,SSL_MKEY_MASK|SSL_AUTH_MASK,0},
	{0,SSL_TXT_aKRB5,0,SSL_aKRB5,0,0,0,0,SSL_AUTH_MASK,0},  /* VRS Kerberos5 */
	{0,SSL_TXT_aRSA,0,SSL_aRSA,  0,0,0,0,SSL_AUTH_MASK,0},
	{0,SSL_TXT_aDSS,0,SSL_aDSS,  0,0,0,0,SSL_AUTH_MASK,0},
	{0,SSL_TXT_aFZA,0,SSL_aFZA,  0,0,0,0,SSL_AUTH_MASK,0},
	{0,SSL_TXT_aNULL,0,SSL_aNULL,0,0,0,0,SSL_AUTH_MASK,0},
	{0,SSL_TXT_aDH, 0,SSL_aDH,   0,0,0,0,SSL_AUTH_MASK,0},
	{0,SSL_TXT_DSS,	0,SSL_DSS,   0,0,0,0,SSL_AUTH_MASK,0},

	{0,SSL_TXT_DES,	0,SSL_DES,   0,0,0,0,SSL_ENC_MASK,0},
	{0,SSL_TXT_3DES,0,SSL_3DES,  0,0,0,0,SSL_ENC_MASK,0},
	{0,SSL_TXT_RC4,	0,SSL_RC4,   0,0,0,0,SSL_ENC_MASK,0},
	{0,SSL_TXT_RC2,	0,SSL_RC2,   0,0,0,0,SSL_ENC_MASK,0},
#ifndef OPENSSL_NO_IDEA
	{0,SSL_TXT_IDEA,0,SSL_IDEA,  0,0,0,0,SSL_ENC_MASK,0},
#endif
	{0,SSL_TXT_SEED,0,SSL_SEED,  0,0,0,0,SSL_ENC_MASK,0},
	{0,SSL_TXT_eNULL,0,SSL_eNULL,0,0,0,0,SSL_ENC_MASK,0},
	{0,SSL_TXT_eFZA,0,SSL_eFZA,  0,0,0,0,SSL_ENC_MASK,0},
	{0,SSL_TXT_AES,	0,SSL_AES,   0,0,0,0,SSL_ENC_MASK,0},
	{0,SSL_TXT_CAMELLIA,0,SSL_CAMELLIA, 0,0,0,0,SSL_ENC_MASK,0},

	{0,SSL_TXT_MD5,	0,SSL_MD5,   0,0,0,0,SSL_MAC_MASK,0},
	{0,SSL_TXT_SHA1,0,SSL_SHA1,  0,0,0,0,SSL_MAC_MASK,0},
	{0,SSL_TXT_SHA,	0,SSL_SHA,   0,0,0,0,SSL_MAC_MASK,0},

	{0,SSL_TXT_NULL,0,SSL_NULL,  0,0,0,0,SSL_ENC_MASK,0},
	{0,SSL_TXT_KRB5,0,SSL_KRB5,  0,0,0,0,SSL_AUTH_MASK|SSL_MKEY_MASK,0},
	{0,SSL_TXT_RSA,	0,SSL_RSA,   0,0,0,0,SSL_AUTH_MASK|SSL_MKEY_MASK,0},
	{0,SSL_TXT_ADH,	0,SSL_ADH,   0,0,0,0,SSL_AUTH_MASK|SSL_MKEY_MASK,0},
	{0,SSL_TXT_FZA,	0,SSL_FZA,   0,0,0,0,SSL_AUTH_MASK|SSL_MKEY_MASK|SSL_ENC_MASK,0},

	{0,SSL_TXT_SSLV2, 0,SSL_SSLV2, 0,0,0,0,SSL_SSL_MASK,0},
	{0,SSL_TXT_SSLV3, 0,SSL_SSLV3, 0,0,0,0,SSL_SSL_MASK,0},
	{0,SSL_TXT_TLSV1, 0,SSL_TLSV1, 0,0,0,0,SSL_SSL_MASK,0},

	{0,SSL_TXT_EXP   ,0, 0,SSL_EXPORT, 0,0,0,0,SSL_EXP_MASK},
	{0,SSL_TXT_EXPORT,0, 0,SSL_EXPORT, 0,0,0,0,SSL_EXP_MASK},
	{0,SSL_TXT_EXP40, 0, 0, SSL_EXP40, 0,0,0,0,SSL_STRONG_MASK},
	{0,SSL_TXT_EXP56, 0, 0, SSL_EXP56, 0,0,0,0,SSL_STRONG_MASK},
	{0,SSL_TXT_LOW,   0, 0,   SSL_LOW, 0,0,0,0,SSL_STRONG_MASK},
	{0,SSL_TXT_MEDIUM,0, 0,SSL_MEDIUM, 0,0,0,0,SSL_STRONG_MASK},
	{0,SSL_TXT_HIGH,  0, 0,  SSL_HIGH, 0,0,0,0,SSL_STRONG_MASK},
	{0,SSL_TXT_FIPS,  0, 0,  SSL_FIPS, 0,0,0,0,SSL_FIPS|SSL_STRONG_NONE},
	};

void ssl_load_ciphers(void)
	{
	ssl_cipher_methods[SSL_ENC_DES_IDX]= 
		EVP_get_cipherbyname(SN_des_cbc);
	ssl_cipher_methods[SSL_ENC_3DES_IDX]=
		EVP_get_cipherbyname(SN_des_ede3_cbc);
	ssl_cipher_methods[SSL_ENC_RC4_IDX]=
		EVP_get_cipherbyname(SN_rc4);
	ssl_cipher_methods[SSL_ENC_RC2_IDX]= 
		EVP_get_cipherbyname(SN_rc2_cbc);
#ifndef OPENSSL_NO_IDEA
	ssl_cipher_methods[SSL_ENC_IDEA_IDX]= 
		EVP_get_cipherbyname(SN_idea_cbc);
#else
	ssl_cipher_methods[SSL_ENC_IDEA_IDX]= NULL;
#endif
	ssl_cipher_methods[SSL_ENC_AES128_IDX]=
	  EVP_get_cipherbyname(SN_aes_128_cbc);
	ssl_cipher_methods[SSL_ENC_AES256_IDX]=
	  EVP_get_cipherbyname(SN_aes_256_cbc);
	ssl_cipher_methods[SSL_ENC_CAMELLIA128_IDX]=
	  EVP_get_cipherbyname(SN_camellia_128_cbc);
	ssl_cipher_methods[SSL_ENC_CAMELLIA256_IDX]=
	  EVP_get_cipherbyname(SN_camellia_256_cbc);
	ssl_cipher_methods[SSL_ENC_SEED_IDX]=
	  EVP_get_cipherbyname(SN_seed_cbc);

	ssl_digest_methods[SSL_MD_MD5_IDX]=
		EVP_get_digestbyname(SN_md5);
	ssl_digest_methods[SSL_MD_SHA1_IDX]=
		EVP_get_digestbyname(SN_sha1);
	}


#ifndef OPENSSL_NO_COMP

static int sk_comp_cmp(const SSL_COMP * const *a,
			const SSL_COMP * const *b)
	{
	return((*a)->id-(*b)->id);
	}

static void load_builtin_compressions(void)
	{
	int got_write_lock = 0;

	CRYPTO_r_lock(CRYPTO_LOCK_SSL);
	if (ssl_comp_methods == NULL)
		{
		CRYPTO_r_unlock(CRYPTO_LOCK_SSL);
		CRYPTO_w_lock(CRYPTO_LOCK_SSL);
		got_write_lock = 1;
		
		if (ssl_comp_methods == NULL)
			{
			SSL_COMP *comp = NULL;

			MemCheck_off();
			ssl_comp_methods=sk_SSL_COMP_new(sk_comp_cmp);
			if (ssl_comp_methods != NULL)
				{
				comp=(SSL_COMP *)OPENSSL_malloc(sizeof(SSL_COMP));
				if (comp != NULL)
					{
					comp->method=COMP_zlib();
					if (comp->method
						&& comp->method->type == NID_undef)
						OPENSSL_free(comp);
					else
						{
						comp->id=SSL_COMP_ZLIB_IDX;
						comp->name=comp->method->name;
						sk_SSL_COMP_push(ssl_comp_methods,comp);
						}
					}
				}
			MemCheck_on();
			}
		}
	
	if (got_write_lock)
		CRYPTO_w_unlock(CRYPTO_LOCK_SSL);
	else
		CRYPTO_r_unlock(CRYPTO_LOCK_SSL);
	}
#endif

int ssl_cipher_get_evp(const SSL_SESSION *s, const EVP_CIPHER **enc,
	     const EVP_MD **md, SSL_COMP **comp)
	{
	int i;
	SSL_CIPHER *c;

	c=s->cipher;
	if (c == NULL) return(0);
	if (comp != NULL)
		{
		SSL_COMP ctmp;
#ifndef OPENSSL_NO_COMP
		load_builtin_compressions();
#endif

		*comp=NULL;
		ctmp.id=s->compress_meth;
		if (ssl_comp_methods != NULL)
			{
			i=sk_SSL_COMP_find(ssl_comp_methods,&ctmp);
			if (i >= 0)
				*comp=sk_SSL_COMP_value(ssl_comp_methods,i);
			else
				*comp=NULL;
			}
		}

	if ((enc == NULL) || (md == NULL)) return(0);

	switch (c->algorithms & SSL_ENC_MASK)
		{
	case SSL_DES:
		i=SSL_ENC_DES_IDX;
		break;
	case SSL_3DES:
		i=SSL_ENC_3DES_IDX;
		break;
	case SSL_RC4:
		i=SSL_ENC_RC4_IDX;
		break;
	case SSL_RC2:
		i=SSL_ENC_RC2_IDX;
		break;
	case SSL_IDEA:
		i=SSL_ENC_IDEA_IDX;
		break;
	case SSL_eNULL:
		i=SSL_ENC_NULL_IDX;
		break;
	case SSL_AES:
		switch(c->alg_bits)
			{
		case 128: i=SSL_ENC_AES128_IDX; break;
		case 256: i=SSL_ENC_AES256_IDX; break;
		default: i=-1; break;
			}
		break;
	case SSL_CAMELLIA:
		switch(c->alg_bits)
			{
		case 128: i=SSL_ENC_CAMELLIA128_IDX; break;
		case 256: i=SSL_ENC_CAMELLIA256_IDX; break;
		default: i=-1; break;
			}
		break;
	case SSL_SEED:
		i=SSL_ENC_SEED_IDX;
		break;

	default:
		i= -1;
		break;
		}

	if ((i < 0) || (i > SSL_ENC_NUM_IDX))
		*enc=NULL;
	else
		{
		if (i == SSL_ENC_NULL_IDX)
			*enc=EVP_enc_null();
		else
			*enc=ssl_cipher_methods[i];
		}

	switch (c->algorithms & SSL_MAC_MASK)
		{
	case SSL_MD5:
		i=SSL_MD_MD5_IDX;
		break;
	case SSL_SHA1:
		i=SSL_MD_SHA1_IDX;
		break;
	default:
		i= -1;
		break;
		}
	if ((i < 0) || (i > SSL_MD_NUM_IDX))
		*md=NULL;
	else
		*md=ssl_digest_methods[i];

	if ((*enc != NULL) && (*md != NULL))
		return(1);
	else
		return(0);
	}

#define ITEM_SEP(a) \
	(((a) == ':') || ((a) == ' ') || ((a) == ';') || ((a) == ','))

static void ll_append_tail(CIPHER_ORDER **head, CIPHER_ORDER *curr,
	     CIPHER_ORDER **tail)
	{
	if (curr == *tail) return;
	if (curr == *head)
		*head=curr->next;
	if (curr->prev != NULL)
		curr->prev->next=curr->next;
	if (curr->next != NULL) /* should always be true */
		curr->next->prev=curr->prev;
	(*tail)->next=curr;
	curr->prev= *tail;
	curr->next=NULL;
	*tail=curr;
	}

struct disabled_masks { /* This is a kludge no longer needed with OpenSSL 0.9.9,
                         * where 128-bit and 256-bit algorithms simply will get
                         * separate bits. */
  unsigned long mask; /* everything except m256 */
  unsigned long m256; /* applies to 256-bit algorithms only */
};

static struct disabled_masks ssl_cipher_get_disabled(void)
	{
	unsigned long mask;
	unsigned long m256;
	struct disabled_masks ret;

	mask = SSL_kFZA;
#ifdef OPENSSL_NO_RSA
	mask |= SSL_aRSA|SSL_kRSA;
#endif
#ifdef OPENSSL_NO_DSA
	mask |= SSL_aDSS;
#endif
#ifdef OPENSSL_NO_DH
	mask |= SSL_kDHr|SSL_kDHd|SSL_kEDH|SSL_aDH;
#endif
#ifdef OPENSSL_NO_KRB5
	mask |= SSL_kKRB5|SSL_aKRB5;
#endif
#ifdef OPENSSL_NO_ECDH
	mask |= SSL_kECDH|SSL_kECDHE;
#endif
#ifdef SSL_FORBID_ENULL
	mask |= SSL_eNULL;
#endif

	mask |= (ssl_cipher_methods[SSL_ENC_DES_IDX ] == NULL) ? SSL_DES :0;
	mask |= (ssl_cipher_methods[SSL_ENC_3DES_IDX] == NULL) ? SSL_3DES:0;
	mask |= (ssl_cipher_methods[SSL_ENC_RC4_IDX ] == NULL) ? SSL_RC4 :0;
	mask |= (ssl_cipher_methods[SSL_ENC_RC2_IDX ] == NULL) ? SSL_RC2 :0;
	mask |= (ssl_cipher_methods[SSL_ENC_IDEA_IDX] == NULL) ? SSL_IDEA:0;
	mask |= (ssl_cipher_methods[SSL_ENC_eFZA_IDX] == NULL) ? SSL_eFZA:0;
	mask |= (ssl_cipher_methods[SSL_ENC_SEED_IDX] == NULL) ? SSL_SEED:0;

	mask |= (ssl_digest_methods[SSL_MD_MD5_IDX ] == NULL) ? SSL_MD5 :0;
	mask |= (ssl_digest_methods[SSL_MD_SHA1_IDX] == NULL) ? SSL_SHA1:0;

	/* finally consider algorithms where mask and m256 differ */
	m256 = mask;
	mask |= (ssl_cipher_methods[SSL_ENC_AES128_IDX] == NULL) ? SSL_AES:0;
	mask |= (ssl_cipher_methods[SSL_ENC_CAMELLIA128_IDX] == NULL) ? SSL_CAMELLIA:0;
	m256 |= (ssl_cipher_methods[SSL_ENC_AES256_IDX] == NULL) ? SSL_AES:0;
	m256 |= (ssl_cipher_methods[SSL_ENC_CAMELLIA256_IDX] == NULL) ? SSL_CAMELLIA:0;

	ret.mask = mask;
	ret.m256 = m256;
	return ret;
	}

static void ssl_cipher_collect_ciphers(const SSL_METHOD *ssl_method,
		int num_of_ciphers, unsigned long mask, unsigned long m256,
		CIPHER_ORDER *co_list, CIPHER_ORDER **head_p,
		CIPHER_ORDER **tail_p)
	{
	int i, co_list_num;
	SSL_CIPHER *c;

	/*
	 * We have num_of_ciphers descriptions compiled in, depending on the
	 * method selected (SSLv2 and/or SSLv3, TLSv1 etc).
	 * These will later be sorted in a linked list with at most num
	 * entries.
	 */

	/* Get the initial list of ciphers */
	co_list_num = 0;	/* actual count of ciphers */
	for (i = 0; i < num_of_ciphers; i++)
		{
		c = ssl_method->get_cipher(i);
#define IS_MASKED(c) ((c)->algorithms & (((c)->alg_bits == 256) ? m256 : mask))
		/* drop those that use any of that is not available */
#ifdef OPENSSL_FIPS
		if ((c != NULL) && c->valid && !IS_MASKED(c)
			&& (!FIPS_mode() || (c->algo_strength & SSL_FIPS)))
#else
		if ((c != NULL) && c->valid && !IS_MASKED(c))
#endif
			{
			co_list[co_list_num].cipher = c;
			co_list[co_list_num].next = NULL;
			co_list[co_list_num].prev = NULL;
			co_list[co_list_num].active = 0;
			co_list_num++;
#ifdef KSSL_DEBUG
			printf("\t%d: %s %lx %lx\n",i,c->name,c->id,c->algorithms);
#endif	/* KSSL_DEBUG */
			/*
			if (!sk_push(ca_list,(char *)c)) goto err;
			*/
			}
		}

	/*
	 * Prepare linked list from list entries
	 */	
	for (i = 1; i < co_list_num - 1; i++)
		{
		co_list[i].prev = &(co_list[i-1]);
		co_list[i].next = &(co_list[i+1]);
		}
	if (co_list_num > 0)
		{
		(*head_p) = &(co_list[0]);
		(*head_p)->prev = NULL;
		(*head_p)->next = &(co_list[1]);
		(*tail_p) = &(co_list[co_list_num - 1]);
		(*tail_p)->prev = &(co_list[co_list_num - 2]);
		(*tail_p)->next = NULL;
		}
	}

static void ssl_cipher_collect_aliases(SSL_CIPHER **ca_list,
			int num_of_group_aliases, unsigned long mask,
			CIPHER_ORDER *head)
	{
	CIPHER_ORDER *ciph_curr;
	SSL_CIPHER **ca_curr;
	int i;

	/*
	 * First, add the real ciphers as already collected
	 */
	ciph_curr = head;
	ca_curr = ca_list;
	while (ciph_curr != NULL)
		{
		*ca_curr = ciph_curr->cipher;
		ca_curr++;
		ciph_curr = ciph_curr->next;
		}

	/*
	 * Now we add the available ones from the cipher_aliases[] table.
	 * They represent either an algorithm, that must be fully
	 * supported (not match any bit in mask) or represent a cipher
	 * strength value (will be added in any case because algorithms=0).
	 */
	for (i = 0; i < num_of_group_aliases; i++)
		{
		if ((i == 0) ||		/* always fetch "ALL" */
		    !(cipher_aliases[i].algorithms & mask))
			{
			*ca_curr = (SSL_CIPHER *)(cipher_aliases + i);
			ca_curr++;
			}
		}

	*ca_curr = NULL;	/* end of list */
	}

static void ssl_cipher_apply_rule(unsigned long cipher_id, unsigned long ssl_version,
		unsigned long algorithms, unsigned long mask,
		unsigned long algo_strength, unsigned long mask_strength,
		int rule, int strength_bits, CIPHER_ORDER *co_list,
		CIPHER_ORDER **head_p, CIPHER_ORDER **tail_p)
	{
	CIPHER_ORDER *head, *tail, *curr, *curr2, *tail2;
	SSL_CIPHER *cp;
	unsigned long ma, ma_s;

#ifdef CIPHER_DEBUG
	printf("Applying rule %d with %08lx %08lx %08lx %08lx (%d)\n",
		rule, algorithms, mask, algo_strength, mask_strength,
		strength_bits);
#endif

	curr = head = *head_p;
	curr2 = head;
	tail2 = tail = *tail_p;
	for (;;)
		{
		if ((curr == NULL) || (curr == tail2)) break;
		curr = curr2;
		curr2 = curr->next;

		cp = curr->cipher;

		/* If explicit cipher suite, match only that one for its own protocol version.
		 * Usual selection criteria will be used for similar ciphersuites from other version! */

		if (cipher_id && (cp->algorithms & SSL_SSL_MASK) == ssl_version)
			{
			if (cp->id != cipher_id)
				continue;
			}

		/*
		 * Selection criteria is either the number of strength_bits
		 * or the algorithm used.
		 */
		else if (strength_bits == -1)
			{
			ma = mask & cp->algorithms;
			ma_s = mask_strength & cp->algo_strength;

#ifdef CIPHER_DEBUG
			printf("\nName: %s:\nAlgo = %08lx Algo_strength = %08lx\nMask = %08lx Mask_strength %08lx\n", cp->name, cp->algorithms, cp->algo_strength, mask, mask_strength);
			printf("ma = %08lx ma_s %08lx, ma&algo=%08lx, ma_s&algos=%08lx\n", ma, ma_s, ma&algorithms, ma_s&algo_strength);
#endif
			/*
			 * Select: if none of the mask bit was met from the
			 * cipher or not all of the bits were met, the
			 * selection does not apply.
			 */
			if (((ma == 0) && (ma_s == 0)) ||
			    ((ma & algorithms) != ma) ||
			    ((ma_s & algo_strength) != ma_s))
				continue; /* does not apply */
			}
		else if (strength_bits != cp->strength_bits)
			continue;	/* does not apply */

#ifdef CIPHER_DEBUG
		printf("Action = %d\n", rule);
#endif

		/* add the cipher if it has not been added yet. */
		if (rule == CIPHER_ADD)
			{
			if (!curr->active)
				{
				int add_this_cipher = 1;

				if (((cp->algorithms & (SSL_kECDHE|SSL_kECDH|SSL_aECDSA)) != 0))
					{
					/* Make sure "ECCdraft" ciphersuites are activated only if
					 * *explicitly* requested, but not implicitly (such as
					 * as part of the "AES" alias). */

					add_this_cipher = (mask & (SSL_kECDHE|SSL_kECDH|SSL_aECDSA)) != 0 || cipher_id != 0;
					}
				
				if (add_this_cipher)
					{
					ll_append_tail(&head, curr, &tail);
					curr->active = 1;
					}
				}
			}
		/* Move the added cipher to this location */
		else if (rule == CIPHER_ORD)
			{
			if (curr->active)
				{
				ll_append_tail(&head, curr, &tail);
				}
			}
		else if	(rule == CIPHER_DEL)
			curr->active = 0;
		else if (rule == CIPHER_KILL)
			{
			if (head == curr)
				head = curr->next;
			else
				curr->prev->next = curr->next;
			if (tail == curr)
				tail = curr->prev;
			curr->active = 0;
			if (curr->next != NULL)
				curr->next->prev = curr->prev;
			if (curr->prev != NULL)
				curr->prev->next = curr->next;
			curr->next = NULL;
			curr->prev = NULL;
			}
		}

	*head_p = head;
	*tail_p = tail;
	}

static int ssl_cipher_strength_sort(CIPHER_ORDER *co_list,
				    CIPHER_ORDER **head_p,
				    CIPHER_ORDER **tail_p)
	{
	int max_strength_bits, i, *number_uses;
	CIPHER_ORDER *curr;

	/*
	 * This routine sorts the ciphers with descending strength. The sorting
	 * must keep the pre-sorted sequence, so we apply the normal sorting
	 * routine as '+' movement to the end of the list.
	 */
	max_strength_bits = 0;
	curr = *head_p;
	while (curr != NULL)
		{
		if (curr->active &&
		    (curr->cipher->strength_bits > max_strength_bits))
		    max_strength_bits = curr->cipher->strength_bits;
		curr = curr->next;
		}

	number_uses = OPENSSL_malloc((max_strength_bits + 1) * sizeof(int));
	if (!number_uses)
	{
		SSLerr(SSL_F_SSL_CIPHER_STRENGTH_SORT,ERR_R_MALLOC_FAILURE);
		return(0);
	}
	memset(number_uses, 0, (max_strength_bits + 1) * sizeof(int));

	/*
	 * Now find the strength_bits values actually used
	 */
	curr = *head_p;
	while (curr != NULL)
		{
		if (curr->active)
			number_uses[curr->cipher->strength_bits]++;
		curr = curr->next;
		}
	/*
	 * Go through the list of used strength_bits values in descending
	 * order.
	 */
	for (i = max_strength_bits; i >= 0; i--)
		if (number_uses[i] > 0)
			ssl_cipher_apply_rule(0, 0, 0, 0, 0, 0, CIPHER_ORD, i,
					co_list, head_p, tail_p);

	OPENSSL_free(number_uses);
	return(1);
	}

static int ssl_cipher_process_rulestr(const char *rule_str,
		CIPHER_ORDER *co_list, CIPHER_ORDER **head_p,
		CIPHER_ORDER **tail_p, SSL_CIPHER **ca_list)
	{
	unsigned long algorithms, mask, algo_strength, mask_strength;
	const char *l, *buf;
	int j, multi, found, rule, retval, ok, buflen;
	unsigned long cipher_id = 0, ssl_version = 0;
	char ch;

	retval = 1;
	l = rule_str;
	for (;;)
		{
		ch = *l;

		if (ch == '\0')
			break;		/* done */
		if (ch == '-')
			{ rule = CIPHER_DEL; l++; }
		else if (ch == '+')
			{ rule = CIPHER_ORD; l++; }
		else if (ch == '!')
			{ rule = CIPHER_KILL; l++; }
		else if (ch == '@')
			{ rule = CIPHER_SPECIAL; l++; }
		else
			{ rule = CIPHER_ADD; }

		if (ITEM_SEP(ch))
			{
			l++;
			continue;
			}

		algorithms = mask = algo_strength = mask_strength = 0;

		for (;;)
			{
			ch = *l;
			buf = l;
			buflen = 0;
#ifndef CHARSET_EBCDIC
			while (	((ch >= 'A') && (ch <= 'Z')) ||
				((ch >= '0') && (ch <= '9')) ||
				((ch >= 'a') && (ch <= 'z')) ||
				 (ch == '-'))
#else
			while (	isalnum(ch) || (ch == '-'))
#endif
				 {
				 ch = *(++l);
				 buflen++;
				 }

			if (buflen == 0)
				{
				/*
				 * We hit something we cannot deal with,
				 * it is no command or separator nor
				 * alphanumeric, so we call this an error.
				 */
				SSLerr(SSL_F_SSL_CIPHER_PROCESS_RULESTR,
				       SSL_R_INVALID_COMMAND);
				retval = found = 0;
				l++;
				break;
				}

			if (rule == CIPHER_SPECIAL)
				{
				found = 0; /* unused -- avoid compiler warning */
				break;	/* special treatment */
				}

			/* check for multi-part specification */
			if (ch == '+')
				{
				multi=1;
				l++;
				}
			else
				multi=0;

			/*
			 * Now search for the cipher alias in the ca_list. Be careful
			 * with the strncmp, because the "buflen" limitation
			 * will make the rule "ADH:SOME" and the cipher
			 * "ADH-MY-CIPHER" look like a match for buflen=3.
			 * So additionally check whether the cipher name found
			 * has the correct length. We can save a strlen() call:
			 * just checking for the '\0' at the right place is
			 * sufficient, we have to strncmp() anyway. (We cannot
			 * use strcmp(), because buf is not '\0' terminated.)
			 */
			 j = found = 0;
			 cipher_id = 0;
			 ssl_version = 0;
			 while (ca_list[j])
				{
				if (!strncmp(buf, ca_list[j]->name, buflen) &&
				    (ca_list[j]->name[buflen] == '\0'))
					{
					found = 1;
					break;
					}
				else
					j++;
				}
			if (!found)
				break;	/* ignore this entry */

			/* New algorithms:
			 *  1 - any old restrictions apply outside new mask
			 *  2 - any new restrictions apply outside old mask
			 *  3 - enforce old & new where masks intersect
			 */
			algorithms = (algorithms & ~ca_list[j]->mask) |		/* 1 */
			             (ca_list[j]->algorithms & ~mask) |		/* 2 */
			             (algorithms & ca_list[j]->algorithms);	/* 3 */
			mask |= ca_list[j]->mask;
			algo_strength = (algo_strength & ~ca_list[j]->mask_strength) |
			                (ca_list[j]->algo_strength & ~mask_strength) |
			                (algo_strength & ca_list[j]->algo_strength);
			mask_strength |= ca_list[j]->mask_strength;

			/* explicit ciphersuite found */
			if (ca_list[j]->valid)
				{
				cipher_id = ca_list[j]->id;
				ssl_version = ca_list[j]->algorithms & SSL_SSL_MASK;
				break;
				}

			if (!multi) break;
			}

		/*
		 * Ok, we have the rule, now apply it
		 */
		if (rule == CIPHER_SPECIAL)
			{	/* special command */
			ok = 0;
			if ((buflen == 8) &&
				!strncmp(buf, "STRENGTH", 8))
				ok = ssl_cipher_strength_sort(co_list,
					head_p, tail_p);
			else
				SSLerr(SSL_F_SSL_CIPHER_PROCESS_RULESTR,
					SSL_R_INVALID_COMMAND);
			if (ok == 0)
				retval = 0;
			/*
			 * We do not support any "multi" options
			 * together with "@", so throw away the
			 * rest of the command, if any left, until
			 * end or ':' is found.
			 */
			while ((*l != '\0') && !ITEM_SEP(*l))
				l++;
			}
		else if (found)
			{
			ssl_cipher_apply_rule(cipher_id, ssl_version, algorithms, mask,
				algo_strength, mask_strength, rule, -1,
				co_list, head_p, tail_p);
			}
		else
			{
			while ((*l != '\0') && !ITEM_SEP(*l))
				l++;
			}
		if (*l == '\0') break; /* done */
		}

	return(retval);
	}

STACK_OF(SSL_CIPHER) *ssl_create_cipher_list(const SSL_METHOD *ssl_method,
		STACK_OF(SSL_CIPHER) **cipher_list,
		STACK_OF(SSL_CIPHER) **cipher_list_by_id,
		const char *rule_str)
	{
	int ok, num_of_ciphers, num_of_alias_max, num_of_group_aliases;
	unsigned long disabled_mask;
	unsigned long disabled_m256;
	STACK_OF(SSL_CIPHER) *cipherstack, *tmp_cipher_list;
	const char *rule_p;
	CIPHER_ORDER *co_list = NULL, *head = NULL, *tail = NULL, *curr;
	SSL_CIPHER **ca_list = NULL;

	/*
	 * Return with error if nothing to do.
	 */
	if (rule_str == NULL || cipher_list == NULL || cipher_list_by_id == NULL)
		return NULL;

	/*
	 * To reduce the work to do we only want to process the compiled
	 * in algorithms, so we first get the mask of disabled ciphers.
	 */
	{
		struct disabled_masks d;
		d = ssl_cipher_get_disabled();
		disabled_mask = d.mask;
		disabled_m256 = d.m256;
	}

	/*
	 * Now we have to collect the available ciphers from the compiled
	 * in ciphers. We cannot get more than the number compiled in, so
	 * it is used for allocation.
	 */
	num_of_ciphers = ssl_method->num_ciphers();
#ifdef KSSL_DEBUG
	printf("ssl_create_cipher_list() for %d ciphers\n", num_of_ciphers);
#endif    /* KSSL_DEBUG */
	co_list = (CIPHER_ORDER *)OPENSSL_malloc(sizeof(CIPHER_ORDER) * num_of_ciphers);
	if (co_list == NULL)
		{
		SSLerr(SSL_F_SSL_CREATE_CIPHER_LIST,ERR_R_MALLOC_FAILURE);
		return(NULL);	/* Failure */
		}

	ssl_cipher_collect_ciphers(ssl_method, num_of_ciphers, disabled_mask,
				   disabled_m256, co_list, &head, &tail);

	/*
	 * We also need cipher aliases for selecting based on the rule_str.
	 * There might be two types of entries in the rule_str: 1) names
	 * of ciphers themselves 2) aliases for groups of ciphers.
	 * For 1) we need the available ciphers and for 2) the cipher
	 * groups of cipher_aliases added together in one list (otherwise
	 * we would be happy with just the cipher_aliases table).
	 */
	num_of_group_aliases = sizeof(cipher_aliases) / sizeof(SSL_CIPHER);
	num_of_alias_max = num_of_ciphers + num_of_group_aliases + 1;
	ca_list =
		(SSL_CIPHER **)OPENSSL_malloc(sizeof(SSL_CIPHER *) * num_of_alias_max);
	if (ca_list == NULL)
		{
		OPENSSL_free(co_list);
		SSLerr(SSL_F_SSL_CREATE_CIPHER_LIST,ERR_R_MALLOC_FAILURE);
		return(NULL);	/* Failure */
		}
	ssl_cipher_collect_aliases(ca_list, num_of_group_aliases,
				   (disabled_mask & disabled_m256), head);

	/*
	 * If the rule_string begins with DEFAULT, apply the default rule
	 * before using the (possibly available) additional rules.
	 */
	ok = 1;
	rule_p = rule_str;
	if (strncmp(rule_str,"DEFAULT",7) == 0)
		{
		ok = ssl_cipher_process_rulestr(SSL_DEFAULT_CIPHER_LIST,
			co_list, &head, &tail, ca_list);
		rule_p += 7;
		if (*rule_p == ':')
			rule_p++;
		}

	if (ok && (strlen(rule_p) > 0))
		ok = ssl_cipher_process_rulestr(rule_p, co_list, &head, &tail,
						ca_list);

	OPENSSL_free(ca_list);	/* Not needed anymore */

	if (!ok)
		{	/* Rule processing failure */
		OPENSSL_free(co_list);
		return(NULL);
		}
	/*
	 * Allocate new "cipherstack" for the result, return with error
	 * if we cannot get one.
	 */
	if ((cipherstack = sk_SSL_CIPHER_new_null()) == NULL)
		{
		OPENSSL_free(co_list);
		return(NULL);
		}

	/*
	 * The cipher selection for the list is done. The ciphers are added
	 * to the resulting precedence to the STACK_OF(SSL_CIPHER).
	 */
	for (curr = head; curr != NULL; curr = curr->next)
		{
#ifdef OPENSSL_FIPS
		if (curr->active && (!FIPS_mode() || curr->cipher->algo_strength & SSL_FIPS))
#else
		if (curr->active)
#endif
			{
			sk_SSL_CIPHER_push(cipherstack, curr->cipher);
#ifdef CIPHER_DEBUG
			printf("<%s>\n",curr->cipher->name);
#endif
			}
		}
	OPENSSL_free(co_list);	/* Not needed any longer */

	tmp_cipher_list = sk_SSL_CIPHER_dup(cipherstack);
	if (tmp_cipher_list == NULL)
		{
		sk_SSL_CIPHER_free(cipherstack);
		return NULL;
		}
	if (*cipher_list != NULL)
		sk_SSL_CIPHER_free(*cipher_list);
	*cipher_list = cipherstack;
	if (*cipher_list_by_id != NULL)
		sk_SSL_CIPHER_free(*cipher_list_by_id);
	*cipher_list_by_id = tmp_cipher_list;
	(void)sk_SSL_CIPHER_set_cmp_func(*cipher_list_by_id,ssl_cipher_ptr_id_cmp);

	sk_SSL_CIPHER_sort(*cipher_list_by_id);
	return(cipherstack);
	}

char *SSL_CIPHER_description(const SSL_CIPHER *cipher, char *buf, int len)
	{
	int is_export,pkl,kl;
	const char *ver,*exp_str;
	const char *kx,*au,*enc,*mac;
	unsigned long alg,alg2;
#ifdef KSSL_DEBUG
	static const char *format="%-23s %s Kx=%-8s Au=%-4s Enc=%-9s Mac=%-4s%s AL=%lx\n";
#else
	static const char *format="%-23s %s Kx=%-8s Au=%-4s Enc=%-9s Mac=%-4s%s\n";
#endif /* KSSL_DEBUG */

	alg=cipher->algorithms;
	alg2=cipher->algorithm2;

	is_export=SSL_C_IS_EXPORT(cipher);
	pkl=SSL_C_EXPORT_PKEYLENGTH(cipher);
	kl=SSL_C_EXPORT_KEYLENGTH(cipher);
	exp_str=is_export?" export":"";
	
	if (alg & SSL_SSLV2)
		ver="SSLv2";
	else if (alg & SSL_SSLV3)
		ver="SSLv3";
	else
		ver="unknown";

	switch (alg&SSL_MKEY_MASK)
		{
	case SSL_kRSA:
		kx=is_export?(pkl == 512 ? "RSA(512)" : "RSA(1024)"):"RSA";
		break;
	case SSL_kDHr:
		kx="DH/RSA";
		break;
	case SSL_kDHd:
		kx="DH/DSS";
		break;
        case SSL_kKRB5:         /* VRS */
        case SSL_KRB5:          /* VRS */
            kx="KRB5";
            break;
	case SSL_kFZA:
		kx="Fortezza";
		break;
	case SSL_kEDH:
		kx=is_export?(pkl == 512 ? "DH(512)" : "DH(1024)"):"DH";
		break;
	case SSL_kECDH:
	case SSL_kECDHE:
		kx=is_export?"ECDH(<=163)":"ECDH";
		break;
	default:
		kx="unknown";
		}

	switch (alg&SSL_AUTH_MASK)
		{
	case SSL_aRSA:
		au="RSA";
		break;
	case SSL_aDSS:
		au="DSS";
		break;
	case SSL_aDH:
		au="DH";
		break;
        case SSL_aKRB5:         /* VRS */
        case SSL_KRB5:          /* VRS */
            au="KRB5";
            break;
	case SSL_aFZA:
	case SSL_aNULL:
		au="None";
		break;
	case SSL_aECDSA:
		au="ECDSA";
		break;
	default:
		au="unknown";
		break;
		}

	switch (alg&SSL_ENC_MASK)
		{
	case SSL_DES:
		enc=(is_export && kl == 5)?"DES(40)":"DES(56)";
		break;
	case SSL_3DES:
		enc="3DES(168)";
		break;
	case SSL_RC4:
		enc=is_export?(kl == 5 ? "RC4(40)" : "RC4(56)")
		  :((alg2&SSL2_CF_8_BYTE_ENC)?"RC4(64)":"RC4(128)");
		break;
	case SSL_RC2:
		enc=is_export?(kl == 5 ? "RC2(40)" : "RC2(56)"):"RC2(128)";
		break;
	case SSL_IDEA:
		enc="IDEA(128)";
		break;
	case SSL_eFZA:
		enc="Fortezza";
		break;
	case SSL_eNULL:
		enc="None";
		break;
	case SSL_AES:
		switch(cipher->strength_bits)
			{
		case 128: enc="AES(128)"; break;
		case 192: enc="AES(192)"; break;
		case 256: enc="AES(256)"; break;
		default: enc="AES(?""?""?)"; break;
			}
		break;
	case SSL_CAMELLIA:
		switch(cipher->strength_bits)
			{
		case 128: enc="Camellia(128)"; break;
		case 256: enc="Camellia(256)"; break;
		default: enc="Camellia(?""?""?)"; break;
			}
		break;
	case SSL_SEED:
		enc="SEED(128)";
		break;

	default:
		enc="unknown";
		break;
		}

	switch (alg&SSL_MAC_MASK)
		{
	case SSL_MD5:
		mac="MD5";
		break;
	case SSL_SHA1:
		mac="SHA1";
		break;
	default:
		mac="unknown";
		break;
		}

	if (buf == NULL)
		{
		len=128;
		buf=OPENSSL_malloc(len);
		if (buf == NULL) return("OPENSSL_malloc Error");
		}
	else if (len < 128)
		return("Buffer too small");

#ifdef KSSL_DEBUG
	BIO_snprintf(buf,len,format,cipher->name,ver,kx,au,enc,mac,exp_str,alg);
#else
	BIO_snprintf(buf,len,format,cipher->name,ver,kx,au,enc,mac,exp_str);
#endif /* KSSL_DEBUG */
	return(buf);
	}

char *SSL_CIPHER_get_version(const SSL_CIPHER *c)
	{
	int i;

	if (c == NULL) return("(NONE)");
	i=(int)(c->id>>24L);
	if (i == 3)
		return("TLSv1/SSLv3");
	else if (i == 2)
		return("SSLv2");
	else
		return("unknown");
	}

/* return the actual cipher being used */
const char *SSL_CIPHER_get_name(const SSL_CIPHER *c)
	{
	if (c != NULL)
		return(c->name);
	return("(NONE)");
	}

/* number of bits for symmetric cipher */
int SSL_CIPHER_get_bits(const SSL_CIPHER *c, int *alg_bits)
	{
	int ret=0;

	if (c != NULL)
		{
		if (alg_bits != NULL) *alg_bits = c->alg_bits;
		ret = c->strength_bits;
		}
	return(ret);
	}

SSL_COMP *ssl3_comp_find(STACK_OF(SSL_COMP) *sk, int n)
	{
	SSL_COMP *ctmp;
	int i,nn;

	if ((n == 0) || (sk == NULL)) return(NULL);
	nn=sk_SSL_COMP_num(sk);
	for (i=0; i<nn; i++)
		{
		ctmp=sk_SSL_COMP_value(sk,i);
		if (ctmp->id == n)
			return(ctmp);
		}
	return(NULL);
	}

#ifdef OPENSSL_NO_COMP
void *SSL_COMP_get_compression_methods(void)
	{
	return NULL;
	}
int SSL_COMP_add_compression_method(int id, void *cm)
	{
	return 1;
	}

const char *SSL_COMP_get_name(const void *comp)
	{
	return NULL;
	}
#else
STACK_OF(SSL_COMP) *SSL_COMP_get_compression_methods(void)
	{
	load_builtin_compressions();
	return(ssl_comp_methods);
	}

int SSL_COMP_add_compression_method(int id, COMP_METHOD *cm)
	{
	SSL_COMP *comp;

        if (cm == NULL || cm->type == NID_undef)
                return 1;

	/* According to draft-ietf-tls-compression-04.txt, the
	   compression number ranges should be the following:

	   0 to 63:    methods defined by the IETF
	   64 to 192:  external party methods assigned by IANA
	   193 to 255: reserved for private use */
	if (id < 193 || id > 255)
		{
		SSLerr(SSL_F_SSL_COMP_ADD_COMPRESSION_METHOD,SSL_R_COMPRESSION_ID_NOT_WITHIN_PRIVATE_RANGE);
		return 0;
		}

	MemCheck_off();
	comp=(SSL_COMP *)OPENSSL_malloc(sizeof(SSL_COMP));
	comp->id=id;
	comp->method=cm;
	load_builtin_compressions();
	if (ssl_comp_methods
		&& sk_SSL_COMP_find(ssl_comp_methods,comp) >= 0)
		{
		OPENSSL_free(comp);
		MemCheck_on();
		SSLerr(SSL_F_SSL_COMP_ADD_COMPRESSION_METHOD,SSL_R_DUPLICATE_COMPRESSION_ID);
		return(1);
		}
	else if ((ssl_comp_methods == NULL)
		|| !sk_SSL_COMP_push(ssl_comp_methods,comp))
		{
		OPENSSL_free(comp);
		MemCheck_on();
		SSLerr(SSL_F_SSL_COMP_ADD_COMPRESSION_METHOD,ERR_R_MALLOC_FAILURE);
		return(1);
		}
	else
		{
		MemCheck_on();
		return(0);
		}
	}

const char *SSL_COMP_get_name(const COMP_METHOD *comp)
	{
	if (comp)
		return comp->name;
	return NULL;
	}

#endif
