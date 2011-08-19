/* crypto/x509/by_dir.c */
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

#ifndef NO_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef MAC_OS_pre_X
# include <stat.h>
#else
# include <sys/stat.h>
#endif

#include <openssl/lhash.h>
#include <openssl/x509.h>

#ifdef _WIN32
#define stat	_stat
#endif

typedef struct lookup_dir_st
	{
	BUF_MEM *buffer;
	int num_dirs;
	char **dirs;
	int *dirs_type;
	int num_dirs_alloced;
	} BY_DIR;

static int dir_ctrl(X509_LOOKUP *ctx, int cmd, const char *argp, long argl,
	char **ret);
static int new_dir(X509_LOOKUP *lu);
static void free_dir(X509_LOOKUP *lu);
static int add_cert_dir(BY_DIR *ctx,const char *dir,int type);
static int get_cert_by_subject(X509_LOOKUP *xl,int type,X509_NAME *name,
	X509_OBJECT *ret);
X509_LOOKUP_METHOD x509_dir_lookup=
	{
	"Load certs from files in a directory",
	new_dir,		/* new */
	free_dir,		/* free */
	NULL, 			/* init */
	NULL,			/* shutdown */
	dir_ctrl,		/* ctrl */
	get_cert_by_subject,	/* get_by_subject */
	NULL,			/* get_by_issuer_serial */
	NULL,			/* get_by_fingerprint */
	NULL,			/* get_by_alias */
	};

X509_LOOKUP_METHOD *X509_LOOKUP_hash_dir(void)
	{
	return(&x509_dir_lookup);
	}

static int dir_ctrl(X509_LOOKUP *ctx, int cmd, const char *argp, long argl,
	     char **retp)
	{
	int ret=0;
	BY_DIR *ld;
	char *dir = NULL;

	ld=(BY_DIR *)ctx->method_data;

	switch (cmd)
		{
	case X509_L_ADD_DIR:
		if (argl == X509_FILETYPE_DEFAULT)
			{
			dir=(char *)Getenv(X509_get_default_cert_dir_env());
			if (dir)
				ret=add_cert_dir(ld,dir,X509_FILETYPE_PEM);
			else
				ret=add_cert_dir(ld,X509_get_default_cert_dir(),
					X509_FILETYPE_PEM);
			if (!ret)
				{
				X509err(X509_F_DIR_CTRL,X509_R_LOADING_CERT_DIR);
				}
			}
		else
			ret=add_cert_dir(ld,argp,(int)argl);
		break;
		}
	return(ret);
	}

static int new_dir(X509_LOOKUP *lu)
	{
	BY_DIR *a;

	if ((a=(BY_DIR *)OPENSSL_malloc(sizeof(BY_DIR))) == NULL)
		return(0);
	if ((a->buffer=BUF_MEM_new()) == NULL)
		{
		OPENSSL_free(a);
		return(0);
		}
	a->num_dirs=0;
	a->dirs=NULL;
	a->dirs_type=NULL;
	a->num_dirs_alloced=0;
	lu->method_data=(char *)a;
	return(1);
	}

static void free_dir(X509_LOOKUP *lu)
	{
	BY_DIR *a;
	int i;

	a=(BY_DIR *)lu->method_data;
	for (i=0; i<a->num_dirs; i++)
		if (a->dirs[i] != NULL) OPENSSL_free(a->dirs[i]);
	if (a->dirs != NULL) OPENSSL_free(a->dirs);
	if (a->dirs_type != NULL) OPENSSL_free(a->dirs_type);
	if (a->buffer != NULL) BUF_MEM_free(a->buffer);
	OPENSSL_free(a);
	}

static int add_cert_dir(BY_DIR *ctx, const char *dir, int type)
	{
	int j,len;
	int *ip;
	const char *s,*ss,*p;
	char **pp;

	if (dir == NULL || !*dir)
	    {
	    X509err(X509_F_ADD_CERT_DIR,X509_R_INVALID_DIRECTORY);
	    return 0;
	    }

	s=dir;
	p=s;
	for (;;p++)
		{
		if ((*p == LIST_SEPARATOR_CHAR) || (*p == '\0'))
			{
			ss=s;
			s=p+1;
			len=(int)(p-ss);
			if (len == 0) continue;
			for (j=0; j<ctx->num_dirs; j++)
				if (strlen(ctx->dirs[j]) == (size_t)len &&
				    strncmp(ctx->dirs[j],ss,(unsigned int)len) == 0)
					break;
			if (j<ctx->num_dirs)
				continue;
			if (ctx->num_dirs_alloced < (ctx->num_dirs+1))
				{
				ctx->num_dirs_alloced+=10;
				pp=(char **)OPENSSL_malloc(ctx->num_dirs_alloced*
					sizeof(char *));
				ip=(int *)OPENSSL_malloc(ctx->num_dirs_alloced*
					sizeof(int));
				if ((pp == NULL) || (ip == NULL))
					{
					X509err(X509_F_ADD_CERT_DIR,ERR_R_MALLOC_FAILURE);
					return(0);
					}
				memcpy(pp,ctx->dirs,(ctx->num_dirs_alloced-10)*
					sizeof(char *));
				memcpy(ip,ctx->dirs_type,(ctx->num_dirs_alloced-10)*
					sizeof(int));
				if (ctx->dirs != NULL)
					OPENSSL_free(ctx->dirs);
				if (ctx->dirs_type != NULL)
					OPENSSL_free(ctx->dirs_type);
				ctx->dirs=pp;
				ctx->dirs_type=ip;
				}
			ctx->dirs_type[ctx->num_dirs]=type;
			ctx->dirs[ctx->num_dirs]=(char *)OPENSSL_malloc((unsigned int)len+1);
			if (ctx->dirs[ctx->num_dirs] == NULL) return(0);
			strncpy(ctx->dirs[ctx->num_dirs],ss,(unsigned int)len);
			ctx->dirs[ctx->num_dirs][len]='\0';
			ctx->num_dirs++;
			}
		if (*p == '\0') break;
		}
	return(1);
	}

static int get_cert_by_subject(X509_LOOKUP *xl, int type, X509_NAME *name,
	     X509_OBJECT *ret)
	{
	BY_DIR *ctx;
	union	{
		struct	{
			X509 st_x509;
			X509_CINF st_x509_cinf;
			} x509;
		struct	{
			X509_CRL st_crl;
			X509_CRL_INFO st_crl_info;
			} crl;
		} data;
	int ok=0;
	int i,j,k;
	unsigned long h;
	BUF_MEM *b=NULL;
	struct stat st;
	X509_OBJECT stmp,*tmp;
	const char *postfix="";

	if (name == NULL) return(0);

	stmp.type=type;
	if (type == X509_LU_X509)
		{
		data.x509.st_x509.cert_info= &data.x509.st_x509_cinf;
		data.x509.st_x509_cinf.subject=name;
		stmp.data.x509= &data.x509.st_x509;
		postfix="";
		}
	else if (type == X509_LU_CRL)
		{
		data.crl.st_crl.crl= &data.crl.st_crl_info;
		data.crl.st_crl_info.issuer=name;
		stmp.data.crl= &data.crl.st_crl;
		postfix="r";
		}
	else
		{
		X509err(X509_F_GET_CERT_BY_SUBJECT,X509_R_WRONG_LOOKUP_TYPE);
		goto finish;
		}

	if ((b=BUF_MEM_new()) == NULL)
		{
		X509err(X509_F_GET_CERT_BY_SUBJECT,ERR_R_BUF_LIB);
		goto finish;
		}
	
	ctx=(BY_DIR *)xl->method_data;

	h=X509_NAME_hash(name);
	for (i=0; i<ctx->num_dirs; i++)
		{
		j=strlen(ctx->dirs[i])+1+8+6+1+1;
		if (!BUF_MEM_grow(b,j))
			{
			X509err(X509_F_GET_CERT_BY_SUBJECT,ERR_R_MALLOC_FAILURE);
			goto finish;
			}
		k=0;
		for (;;)
			{
			char c = '/';
#ifdef OPENSSL_SYS_VMS
			c = ctx->dirs[i][strlen(ctx->dirs[i])-1];
			if (c != ':' && c != '>' && c != ']')
				{
				/* If no separator is present, we assume the
				   directory specifier is a logical name, and
				   add a colon.  We really should use better
				   VMS routines for merging things like this,
				   but this will do for now...
				   -- Richard Levitte */
				c = ':';
				}
			else
				{
				c = '\0';
				}
#endif
			if (c == '\0')
				{
				/* This is special.  When c == '\0', no
				   directory separator should be added. */
				BIO_snprintf(b->data,b->max,
					"%s%08lx.%s%d",ctx->dirs[i],h,
					postfix,k);
				}
			else
				{
				BIO_snprintf(b->data,b->max,
					"%s%c%08lx.%s%d",ctx->dirs[i],c,h,
					postfix,k);
				}
			k++;
			if (stat(b->data,&st) < 0)
				break;
			/* found one. */
			if (type == X509_LU_X509)
				{
				if ((X509_load_cert_file(xl,b->data,
					ctx->dirs_type[i])) == 0)
					break;
				}
			else if (type == X509_LU_CRL)
				{
				if ((X509_load_crl_file(xl,b->data,
					ctx->dirs_type[i])) == 0)
					break;
				}
			/* else case will caught higher up */
			}

		/* we have added it to the cache so now pull
		 * it out again */
		CRYPTO_w_lock(CRYPTO_LOCK_X509_STORE);
		j = sk_X509_OBJECT_find(xl->store_ctx->objs,&stmp);
		if(j != -1) tmp=sk_X509_OBJECT_value(xl->store_ctx->objs,j);
		else tmp = NULL;
		CRYPTO_w_unlock(CRYPTO_LOCK_X509_STORE);

		if (tmp != NULL)
			{
			ok=1;
			ret->type=tmp->type;
			memcpy(&ret->data,&tmp->data,sizeof(ret->data));
			/* If we were going to up the reference count,
			 * we would need to do it on a perl 'type'
			 * basis */
	/*		CRYPTO_add(&tmp->data.x509->references,1,
				CRYPTO_LOCK_X509);*/
			goto finish;
			}
		}
finish:
	if (b != NULL) BUF_MEM_free(b);
	return(ok);
	}
