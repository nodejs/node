/* crypto/stack/stack.c */
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

/* Code for stacks
 * Author - Eric Young v 1.0
 * 1.2 eay 12-Mar-97 -	Modified sk_find so that it _DOES_ return the
 *			lowest index for the searched item.
 *
 * 1.1 eay - Take from netdb and added to SSLeay
 *
 * 1.0 eay - First version 29/07/92
 */
#include <stdio.h>
#include "cryptlib.h"
#include <openssl/stack.h>
#include <openssl/objects.h>

#undef MIN_NODES
#define MIN_NODES	4

const char STACK_version[]="Stack" OPENSSL_VERSION_PTEXT;

#include <errno.h>

int (*sk_set_cmp_func(STACK *sk, int (*c)(const char * const *,const char * const *)))
		(const char * const *, const char * const *)
	{
	int (*old)(const char * const *,const char * const *)=sk->comp;

	if (sk->comp != c)
		sk->sorted=0;
	sk->comp=c;

	return old;
	}

STACK *sk_dup(STACK *sk)
	{
	STACK *ret;
	char **s;

	if ((ret=sk_new(sk->comp)) == NULL) goto err;
	s=(char **)OPENSSL_realloc((char *)ret->data,
		(unsigned int)sizeof(char *)*sk->num_alloc);
	if (s == NULL) goto err;
	ret->data=s;

	ret->num=sk->num;
	memcpy(ret->data,sk->data,sizeof(char *)*sk->num);
	ret->sorted=sk->sorted;
	ret->num_alloc=sk->num_alloc;
	ret->comp=sk->comp;
	return(ret);
err:
	if(ret)
		sk_free(ret);
	return(NULL);
	}

STACK *sk_new_null(void)
	{
	return sk_new((int (*)(const char * const *, const char * const *))0);
	}

STACK *sk_new(int (*c)(const char * const *, const char * const *))
	{
	STACK *ret;
	int i;

	if ((ret=(STACK *)OPENSSL_malloc(sizeof(STACK))) == NULL)
		goto err;
	if ((ret->data=(char **)OPENSSL_malloc(sizeof(char *)*MIN_NODES)) == NULL)
		goto err;
	for (i=0; i<MIN_NODES; i++)
		ret->data[i]=NULL;
	ret->comp=c;
	ret->num_alloc=MIN_NODES;
	ret->num=0;
	ret->sorted=0;
	return(ret);
err:
	if(ret)
		OPENSSL_free(ret);
	return(NULL);
	}

int sk_insert(STACK *st, char *data, int loc)
	{
	char **s;

	if(st == NULL) return 0;
	if (st->num_alloc <= st->num+1)
		{
		s=(char **)OPENSSL_realloc((char *)st->data,
			(unsigned int)sizeof(char *)*st->num_alloc*2);
		if (s == NULL)
			return(0);
		st->data=s;
		st->num_alloc*=2;
		}
	if ((loc >= (int)st->num) || (loc < 0))
		st->data[st->num]=data;
	else
		{
		int i;
		char **f,**t;

		f=(char **)st->data;
		t=(char **)&(st->data[1]);
		for (i=st->num; i>=loc; i--)
			t[i]=f[i];
			
#ifdef undef /* no memmove on sunos :-( */
		memmove( (char *)&(st->data[loc+1]),
			(char *)&(st->data[loc]),
			sizeof(char *)*(st->num-loc));
#endif
		st->data[loc]=data;
		}
	st->num++;
	st->sorted=0;
	return(st->num);
	}

char *sk_delete_ptr(STACK *st, char *p)
	{
	int i;

	for (i=0; i<st->num; i++)
		if (st->data[i] == p)
			return(sk_delete(st,i));
	return(NULL);
	}

char *sk_delete(STACK *st, int loc)
	{
	char *ret;
	int i,j;

	if(!st || (loc < 0) || (loc >= st->num)) return NULL;

	ret=st->data[loc];
	if (loc != st->num-1)
		{
		j=st->num-1;
		for (i=loc; i<j; i++)
			st->data[i]=st->data[i+1];
		/* In theory memcpy is not safe for this
		 * memcpy( &(st->data[loc]),
		 *	&(st->data[loc+1]),
		 *	sizeof(char *)*(st->num-loc-1));
		 */
		}
	st->num--;
	return(ret);
	}

static int internal_find(STACK *st, char *data, int ret_val_options)
	{
	char **r;
	int i;
	int (*comp_func)(const void *,const void *);
	if(st == NULL) return -1;

	if (st->comp == NULL)
		{
		for (i=0; i<st->num; i++)
			if (st->data[i] == data)
				return(i);
		return(-1);
		}
	sk_sort(st);
	if (data == NULL) return(-1);
	/* This (and the "qsort" below) are the two places in OpenSSL
	 * where we need to convert from our standard (type **,type **)
	 * compare callback type to the (void *,void *) type required by
	 * bsearch. However, the "data" it is being called(back) with are
	 * not (type *) pointers, but the *pointers* to (type *) pointers,
	 * so we get our extra level of pointer dereferencing that way. */
	comp_func=(int (*)(const void *,const void *))(st->comp);
	r=(char **)OBJ_bsearch_ex((char *)&data,(char *)st->data,
		st->num,sizeof(char *),comp_func,ret_val_options);
	if (r == NULL) return(-1);
	return((int)(r-st->data));
	}

int sk_find(STACK *st, char *data)
	{
	return internal_find(st, data, OBJ_BSEARCH_FIRST_VALUE_ON_MATCH);
	}
int sk_find_ex(STACK *st, char *data)
	{
	return internal_find(st, data, OBJ_BSEARCH_VALUE_ON_NOMATCH);
	}

int sk_push(STACK *st, char *data)
	{
	return(sk_insert(st,data,st->num));
	}

int sk_unshift(STACK *st, char *data)
	{
	return(sk_insert(st,data,0));
	}

char *sk_shift(STACK *st)
	{
	if (st == NULL) return(NULL);
	if (st->num <= 0) return(NULL);
	return(sk_delete(st,0));
	}

char *sk_pop(STACK *st)
	{
	if (st == NULL) return(NULL);
	if (st->num <= 0) return(NULL);
	return(sk_delete(st,st->num-1));
	}

void sk_zero(STACK *st)
	{
	if (st == NULL) return;
	if (st->num <= 0) return;
	memset((char *)st->data,0,sizeof(st->data)*st->num);
	st->num=0;
	}

void sk_pop_free(STACK *st, void (*func)(void *))
	{
	int i;

	if (st == NULL) return;
	for (i=0; i<st->num; i++)
		if (st->data[i] != NULL)
			func(st->data[i]);
	sk_free(st);
	}

void sk_free(STACK *st)
	{
	if (st == NULL) return;
	if (st->data != NULL) OPENSSL_free(st->data);
	OPENSSL_free(st);
	}

int sk_num(const STACK *st)
{
	if(st == NULL) return -1;
	return st->num;
}

char *sk_value(const STACK *st, int i)
{
	if(!st || (i < 0) || (i >= st->num)) return NULL;
	return st->data[i];
}

char *sk_set(STACK *st, int i, char *value)
{
	if(!st || (i < 0) || (i >= st->num)) return NULL;
	return (st->data[i] = value);
}

void sk_sort(STACK *st)
	{
	if (st && !st->sorted)
		{
		int (*comp_func)(const void *,const void *);

		/* same comment as in sk_find ... previously st->comp was declared
		 * as a (void*,void*) callback type, but this made the population
		 * of the callback pointer illogical - our callbacks compare
		 * type** with type**, so we leave the casting until absolutely
		 * necessary (ie. "now"). */
		comp_func=(int (*)(const void *,const void *))(st->comp);
		qsort(st->data,st->num,sizeof(char *), comp_func);
		st->sorted=1;
		}
	}

int sk_is_sorted(const STACK *st)
	{
	if (!st)
		return 1;
	return st->sorted;
	}
