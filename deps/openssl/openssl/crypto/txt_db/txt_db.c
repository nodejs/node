/* crypto/txt_db/txt_db.c */
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
#include <stdlib.h>
#include <string.h>
#include "cryptlib.h"
#include <openssl/buffer.h>
#include <openssl/txt_db.h>

#undef BUFSIZE
#define BUFSIZE	512

const char TXT_DB_version[]="TXT_DB" OPENSSL_VERSION_PTEXT;

TXT_DB *TXT_DB_read(BIO *in, int num)
	{
	TXT_DB *ret=NULL;
	int er=1;
	int esc=0;
	long ln=0;
	int i,add,n;
	int size=BUFSIZE;
	int offset=0;
	char *p,**pp,*f;
	BUF_MEM *buf=NULL;

	if ((buf=BUF_MEM_new()) == NULL) goto err;
	if (!BUF_MEM_grow(buf,size)) goto err;

	if ((ret=(TXT_DB *)OPENSSL_malloc(sizeof(TXT_DB))) == NULL)
		goto err;
	ret->num_fields=num;
	ret->index=NULL;
	ret->qual=NULL;
	if ((ret->data=sk_new_null()) == NULL)
		goto err;
	if ((ret->index=(LHASH **)OPENSSL_malloc(sizeof(LHASH *)*num)) == NULL)
		goto err;
	if ((ret->qual=(int (**)(char **))OPENSSL_malloc(sizeof(int (**)(char **))*num)) == NULL)
		goto err;
	for (i=0; i<num; i++)
		{
		ret->index[i]=NULL;
		ret->qual[i]=NULL;
		}

	add=(num+1)*sizeof(char *);
	buf->data[size-1]='\0';
	offset=0;
	for (;;)
		{
		if (offset != 0)
			{
			size+=BUFSIZE;
			if (!BUF_MEM_grow_clean(buf,size)) goto err;
			}
		buf->data[offset]='\0';
		BIO_gets(in,&(buf->data[offset]),size-offset);
		ln++;
		if (buf->data[offset] == '\0') break;
		if ((offset == 0) && (buf->data[0] == '#')) continue;
		i=strlen(&(buf->data[offset]));
		offset+=i;
		if (buf->data[offset-1] != '\n')
			continue;
		else
			{
			buf->data[offset-1]='\0'; /* blat the '\n' */
			if (!(p=(char *)OPENSSL_malloc(add+offset))) goto err;
			offset=0;
			}
		pp=(char **)p;
		p+=add;
		n=0;
		pp[n++]=p;
		i=0;
		f=buf->data;

		esc=0;
		for (;;)
			{
			if (*f == '\0') break;
			if (*f == '\t')
				{
				if (esc)
					p--;
				else
					{	
					*(p++)='\0';
					f++;
					if (n >=  num) break;
					pp[n++]=p;
					continue;
					}
				}
			esc=(*f == '\\');
			*(p++)= *(f++);
			}
		*(p++)='\0';
		if ((n != num) || (*f != '\0'))
			{
#if !defined(OPENSSL_NO_STDIO) && !defined(OPENSSL_SYS_WIN16)	/* temporaty fix :-( */
			fprintf(stderr,"wrong number of fields on line %ld (looking for field %d, got %d, '%s' left)\n",ln,num,n,f);
#endif
			er=2;
			goto err;
			}
		pp[n]=p;
		if (!sk_push(ret->data,(char *)pp))
			{
#if !defined(OPENSSL_NO_STDIO) && !defined(OPENSSL_SYS_WIN16)	/* temporaty fix :-( */
			fprintf(stderr,"failure in sk_push\n");
#endif
			er=2;
			goto err;
			}
		}
	er=0;
err:
	BUF_MEM_free(buf);
	if (er)
		{
#if !defined(OPENSSL_NO_STDIO) && !defined(OPENSSL_SYS_WIN16)
		if (er == 1) fprintf(stderr,"OPENSSL_malloc failure\n");
#endif
		if (ret != NULL)
			{
			if (ret->data != NULL) sk_free(ret->data);
			if (ret->index != NULL) OPENSSL_free(ret->index);
			if (ret->qual != NULL) OPENSSL_free(ret->qual);
			if (ret != NULL) OPENSSL_free(ret);
			}
		return(NULL);
		}
	else
		return(ret);
	}

char **TXT_DB_get_by_index(TXT_DB *db, int idx, char **value)
	{
	char **ret;
	LHASH *lh;

	if (idx >= db->num_fields)
		{
		db->error=DB_ERROR_INDEX_OUT_OF_RANGE;
		return(NULL);
		}
	lh=db->index[idx];
	if (lh == NULL)
		{
		db->error=DB_ERROR_NO_INDEX;
		return(NULL);
		}
	ret=(char **)lh_retrieve(lh,value);
	db->error=DB_ERROR_OK;
	return(ret);
	}

int TXT_DB_create_index(TXT_DB *db, int field, int (*qual)(char **),
		LHASH_HASH_FN_TYPE hash, LHASH_COMP_FN_TYPE cmp)
	{
	LHASH *idx;
	char **r;
	int i,n;

	if (field >= db->num_fields)
		{
		db->error=DB_ERROR_INDEX_OUT_OF_RANGE;
		return(0);
		}
	if ((idx=lh_new(hash,cmp)) == NULL)
		{
		db->error=DB_ERROR_MALLOC;
		return(0);
		}
	n=sk_num(db->data);
	for (i=0; i<n; i++)
		{
		r=(char **)sk_value(db->data,i);
		if ((qual != NULL) && (qual(r) == 0)) continue;
		if ((r=lh_insert(idx,r)) != NULL)
			{
			db->error=DB_ERROR_INDEX_CLASH;
			db->arg1=sk_find(db->data,(char *)r);
			db->arg2=i;
			lh_free(idx);
			return(0);
			}
		}
	if (db->index[field] != NULL) lh_free(db->index[field]);
	db->index[field]=idx;
	db->qual[field]=qual;
	return(1);
	}

long TXT_DB_write(BIO *out, TXT_DB *db)
	{
	long i,j,n,nn,l,tot=0;
	char *p,**pp,*f;
	BUF_MEM *buf=NULL;
	long ret= -1;

	if ((buf=BUF_MEM_new()) == NULL)
		goto err;
	n=sk_num(db->data);
	nn=db->num_fields;
	for (i=0; i<n; i++)
		{
		pp=(char **)sk_value(db->data,i);

		l=0;
		for (j=0; j<nn; j++)
			{
			if (pp[j] != NULL)
				l+=strlen(pp[j]);
			}
		if (!BUF_MEM_grow_clean(buf,(int)(l*2+nn))) goto err;

		p=buf->data;
		for (j=0; j<nn; j++)
			{
			f=pp[j];
			if (f != NULL)
				for (;;) 
					{
					if (*f == '\0') break;
					if (*f == '\t') *(p++)='\\';
					*(p++)= *(f++);
					}
			*(p++)='\t';
			}
		p[-1]='\n';
		j=p-buf->data;
		if (BIO_write(out,buf->data,(int)j) != j)
			goto err;
		tot+=j;
		}
	ret=tot;
err:
	if (buf != NULL) BUF_MEM_free(buf);
	return(ret);
	}

int TXT_DB_insert(TXT_DB *db, char **row)
	{
	int i;
	char **r;

	for (i=0; i<db->num_fields; i++)
		{
		if (db->index[i] != NULL)
			{
			if ((db->qual[i] != NULL) &&
				(db->qual[i](row) == 0)) continue;
			r=(char **)lh_retrieve(db->index[i],row);
			if (r != NULL)
				{
				db->error=DB_ERROR_INDEX_CLASH;
				db->arg1=i;
				db->arg_row=r;
				goto err;
				}
			}
		}
	/* We have passed the index checks, now just append and insert */
	if (!sk_push(db->data,(char *)row))
		{
		db->error=DB_ERROR_MALLOC;
		goto err;
		}

	for (i=0; i<db->num_fields; i++)
		{
		if (db->index[i] != NULL)
			{
			if ((db->qual[i] != NULL) &&
				(db->qual[i](row) == 0)) continue;
			lh_insert(db->index[i],row);
			}
		}
	return(1);
err:
	return(0);
	}

void TXT_DB_free(TXT_DB *db)
	{
	int i,n;
	char **p,*max;

	if(db == NULL)
	    return;

	if (db->index != NULL)
		{
		for (i=db->num_fields-1; i>=0; i--)
			if (db->index[i] != NULL) lh_free(db->index[i]);
		OPENSSL_free(db->index);
		}
	if (db->qual != NULL)
		OPENSSL_free(db->qual);
	if (db->data != NULL)
		{
		for (i=sk_num(db->data)-1; i>=0; i--)
			{
			/* check if any 'fields' have been allocated
			 * from outside of the initial block */
			p=(char **)sk_value(db->data,i);
			max=p[db->num_fields]; /* last address */
			if (max == NULL) /* new row */
				{
				for (n=0; n<db->num_fields; n++)
					if (p[n] != NULL) OPENSSL_free(p[n]);
				}
			else
				{
				for (n=0; n<db->num_fields; n++)
					{
					if (((p[n] < (char *)p) || (p[n] > max))
						&& (p[n] != NULL))
						OPENSSL_free(p[n]);
					}
				}
			OPENSSL_free(sk_value(db->data,i));
			}
		sk_free(db->data);
		}
	OPENSSL_free(db);
	}
