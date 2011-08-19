/* crypto/evp/bio_ber.c */
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
#include <errno.h>
#include "cryptlib.h"
#include <openssl/buffer.h>
#include <openssl/evp.h>

static int ber_write(BIO *h,char *buf,int num);
static int ber_read(BIO *h,char *buf,int size);
/*static int ber_puts(BIO *h,char *str); */
/*static int ber_gets(BIO *h,char *str,int size); */
static long ber_ctrl(BIO *h,int cmd,long arg1,char *arg2);
static int ber_new(BIO *h);
static int ber_free(BIO *data);
static long ber_callback_ctrl(BIO *h,int cmd,void *(*fp)());
#define BER_BUF_SIZE	(32)

/* This is used to hold the state of the BER objects being read. */
typedef struct ber_struct
	{
	int tag;
	int class;
	long length;
	int inf;
	int num_left;
	int depth;
	} BER_CTX;

typedef struct bio_ber_struct
	{
	int tag;
	int class;
	long length;
	int inf;

	/* most of the following are used when doing non-blocking IO */
	/* reading */
	long num_left;	/* number of bytes still to read/write in block */
	int depth;	/* used with indefinite encoding. */
	int finished;	/* No more read data */

	/* writting */ 
	char *w_addr;
	int w_offset;
	int w_left;

	int buf_len;
	int buf_off;
	unsigned char buf[BER_BUF_SIZE];
	} BIO_BER_CTX;

static BIO_METHOD methods_ber=
	{
	BIO_TYPE_CIPHER,"cipher",
	ber_write,
	ber_read,
	NULL, /* ber_puts, */
	NULL, /* ber_gets, */
	ber_ctrl,
	ber_new,
	ber_free,
	ber_callback_ctrl,
	};

BIO_METHOD *BIO_f_ber(void)
	{
	return(&methods_ber);
	}

static int ber_new(BIO *bi)
	{
	BIO_BER_CTX *ctx;

	ctx=(BIO_BER_CTX *)OPENSSL_malloc(sizeof(BIO_BER_CTX));
	if (ctx == NULL) return(0);

	memset((char *)ctx,0,sizeof(BIO_BER_CTX));

	bi->init=0;
	bi->ptr=(char *)ctx;
	bi->flags=0;
	return(1);
	}

static int ber_free(BIO *a)
	{
	BIO_BER_CTX *b;

	if (a == NULL) return(0);
	b=(BIO_BER_CTX *)a->ptr;
	OPENSSL_cleanse(a->ptr,sizeof(BIO_BER_CTX));
	OPENSSL_free(a->ptr);
	a->ptr=NULL;
	a->init=0;
	a->flags=0;
	return(1);
	}

int bio_ber_get_header(BIO *bio, BIO_BER_CTX *ctx)
	{
	char buf[64];
	int i,j,n;
	int ret;
	unsigned char *p;
	unsigned long length
	int tag;
	int class;
	long max;

	BIO_clear_retry_flags(b);

	/* Pack the buffer down if there is a hole at the front */
	if (ctx->buf_off != 0)
		{
		p=ctx->buf;
		j=ctx->buf_off;
		n=ctx->buf_len-j;
		for (i=0; i<n; i++)
			{
			p[0]=p[j];
			p++;
			}
		ctx->buf_len-j;
		ctx->buf_off=0;
		}

	/* If there is more room, read some more data */
	i=BER_BUF_SIZE-ctx->buf_len;
	if (i)
		{
		i=BIO_read(bio->next_bio,&(ctx->buf[ctx->buf_len]),i);
		if (i <= 0)
			{
			BIO_copy_next_retry(b);
			return(i);
			}
		else
			ctx->buf_len+=i;
		}

	max=ctx->buf_len;
	p=ctx->buf;
	ret=ASN1_get_object(&p,&length,&tag,&class,max);

	if (ret & 0x80)
		{
		if ((ctx->buf_len < BER_BUF_SIZE) &&
			(ERR_GET_REASON(ERR_peek_error()) == ASN1_R_TOO_LONG))
			{
			ERR_clear_error(); /* clear the error */
			BIO_set_retry_read(b);
			}
		return(-1);
		}

	/* We have no error, we have a header, so make use of it */

	if ((ctx->tag  >= 0) && (ctx->tag != tag))
		{
		BIOerr(BIO_F_BIO_BER_GET_HEADER,BIO_R_TAG_MISMATCH);
		sprintf(buf,"tag=%d, got %d",ctx->tag,tag);
		ERR_add_error_data(1,buf);
		return(-1);
		}
	if (ret & 0x01)
	if (ret & V_ASN1_CONSTRUCTED)
	}
	
static int ber_read(BIO *b, char *out, int outl)
	{
	int ret=0,i,n;
	BIO_BER_CTX *ctx;

	BIO_clear_retry_flags(b);

	if (out == NULL) return(0);
	ctx=(BIO_BER_CTX *)b->ptr;

	if ((ctx == NULL) || (b->next_bio == NULL)) return(0);

	if (ctx->finished) return(0);

again:
	/* First see if we are half way through reading a block */
	if (ctx->num_left > 0)
		{
		if (ctx->num_left < outl)
			n=ctx->num_left;
		else
			n=outl;
		i=BIO_read(b->next_bio,out,n);
		if (i <= 0)
			{
			BIO_copy_next_retry(b);
			return(i);
			}
		ctx->num_left-=i;
		outl-=i;
		ret+=i;
		if (ctx->num_left <= 0)
			{
			ctx->depth--;
			if (ctx->depth <= 0)
				ctx->finished=1;
			}
		if (outl <= 0)
			return(ret);
		else
			goto again;
		}
	else	/* we need to read another BER header */
		{
		}
	}

static int ber_write(BIO *b, char *in, int inl)
	{
	int ret=0,n,i;
	BIO_ENC_CTX *ctx;

	ctx=(BIO_ENC_CTX *)b->ptr;
	ret=inl;

	BIO_clear_retry_flags(b);
	n=ctx->buf_len-ctx->buf_off;
	while (n > 0)
		{
		i=BIO_write(b->next_bio,&(ctx->buf[ctx->buf_off]),n);
		if (i <= 0)
			{
			BIO_copy_next_retry(b);
			return(i);
			}
		ctx->buf_off+=i;
		n-=i;
		}
	/* at this point all pending data has been written */

	if ((in == NULL) || (inl <= 0)) return(0);

	ctx->buf_off=0;
	while (inl > 0)
		{
		n=(inl > ENC_BLOCK_SIZE)?ENC_BLOCK_SIZE:inl;
		EVP_CipherUpdate(&(ctx->cipher),
			(unsigned char *)ctx->buf,&ctx->buf_len,
			(unsigned char *)in,n);
		inl-=n;
		in+=n;

		ctx->buf_off=0;
		n=ctx->buf_len;
		while (n > 0)
			{
			i=BIO_write(b->next_bio,&(ctx->buf[ctx->buf_off]),n);
			if (i <= 0)
				{
				BIO_copy_next_retry(b);
				return(i);
				}
			n-=i;
			ctx->buf_off+=i;
			}
		ctx->buf_len=0;
		ctx->buf_off=0;
		}
	BIO_copy_next_retry(b);
	return(ret);
	}

static long ber_ctrl(BIO *b, int cmd, long num, char *ptr)
	{
	BIO *dbio;
	BIO_ENC_CTX *ctx,*dctx;
	long ret=1;
	int i;

	ctx=(BIO_ENC_CTX *)b->ptr;

	switch (cmd)
		{
	case BIO_CTRL_RESET:
		ctx->ok=1;
		ctx->finished=0;
		EVP_CipherInit_ex(&(ctx->cipher),NULL,NULL,NULL,NULL,
			ctx->cipher.berrypt);
		ret=BIO_ctrl(b->next_bio,cmd,num,ptr);
		break;
	case BIO_CTRL_EOF:	/* More to read */
		if (ctx->cont <= 0)
			ret=1;
		else
			ret=BIO_ctrl(b->next_bio,cmd,num,ptr);
		break;
	case BIO_CTRL_WPENDING:
		ret=ctx->buf_len-ctx->buf_off;
		if (ret <= 0)
			ret=BIO_ctrl(b->next_bio,cmd,num,ptr);
		break;
	case BIO_CTRL_PENDING: /* More to read in buffer */
		ret=ctx->buf_len-ctx->buf_off;
		if (ret <= 0)
			ret=BIO_ctrl(b->next_bio,cmd,num,ptr);
		break;
	case BIO_CTRL_FLUSH:
		/* do a final write */
again:
		while (ctx->buf_len != ctx->buf_off)
			{
			i=ber_write(b,NULL,0);
			if (i < 0)
				{
				ret=i;
				break;
				}
			}

		if (!ctx->finished)
			{
			ctx->finished=1;
			ctx->buf_off=0;
			ret=EVP_CipherFinal_ex(&(ctx->cipher),
				(unsigned char *)ctx->buf,
				&(ctx->buf_len));
			ctx->ok=(int)ret;
			if (ret <= 0) break;

			/* push out the bytes */
			goto again;
			}
		
		/* Finally flush the underlying BIO */
		ret=BIO_ctrl(b->next_bio,cmd,num,ptr);
		break;
	case BIO_C_GET_CIPHER_STATUS:
		ret=(long)ctx->ok;
		break;
	case BIO_C_DO_STATE_MACHINE:
		BIO_clear_retry_flags(b);
		ret=BIO_ctrl(b->next_bio,cmd,num,ptr);
		BIO_copy_next_retry(b);
		break;

	case BIO_CTRL_DUP:
		dbio=(BIO *)ptr;
		dctx=(BIO_ENC_CTX *)dbio->ptr;
		memcpy(&(dctx->cipher),&(ctx->cipher),sizeof(ctx->cipher));
		dbio->init=1;
		break;
	default:
		ret=BIO_ctrl(b->next_bio,cmd,num,ptr);
		break;
		}
	return(ret);
	}

static long ber_callback_ctrl(BIO *b, int cmd, void *(*fp)())
	{
	long ret=1;

	if (b->next_bio == NULL) return(0);
	switch (cmd)
		{
	default:
		ret=BIO_callback_ctrl(b->next_bio,cmd,fp);
		break;
		}
	return(ret);
	}

/*
void BIO_set_cipher_ctx(b,c)
BIO *b;
EVP_CIPHER_ctx *c;
	{
	if (b == NULL) return;

	if ((b->callback != NULL) &&
		(b->callback(b,BIO_CB_CTRL,(char *)c,BIO_CTRL_SET,e,0L) <= 0))
		return;

	b->init=1;
	ctx=(BIO_ENC_CTX *)b->ptr;
	memcpy(ctx->cipher,c,sizeof(EVP_CIPHER_CTX));
	
	if (b->callback != NULL)
		b->callback(b,BIO_CB_CTRL,(char *)c,BIO_CTRL_SET,e,1L);
	}
*/

void BIO_set_cipher(BIO *b, EVP_CIPHER *c, unsigned char *k, unsigned char *i,
	     int e)
	{
	BIO_ENC_CTX *ctx;

	if (b == NULL) return;

	if ((b->callback != NULL) &&
		(b->callback(b,BIO_CB_CTRL,(char *)c,BIO_CTRL_SET,e,0L) <= 0))
		return;

	b->init=1;
	ctx=(BIO_ENC_CTX *)b->ptr;
	EVP_CipherInit_ex(&(ctx->cipher),c,NULL,k,i,e);
	
	if (b->callback != NULL)
		b->callback(b,BIO_CB_CTRL,(char *)c,BIO_CTRL_SET,e,1L);
	}

