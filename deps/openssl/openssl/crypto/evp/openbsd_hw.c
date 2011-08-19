/* Written by Ben Laurie, 2001 */
/*
 * Copyright (c) 2001 The OpenSSL Project.  All rights reserved.
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
 */

#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/rsa.h>
#include "evp_locl.h"

/* This stuff should now all be supported through
 * crypto/engine/hw_openbsd_dev_crypto.c unless I botched it up */
static void *dummy=&dummy;

#if 0

/* check flag after OpenSSL headers to ensure make depend works */
#ifdef OPENSSL_OPENBSD_DEV_CRYPTO

#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <crypto/cryptodev.h>
#include <unistd.h>
#include <assert.h>

/* longest key supported in hardware */
#define MAX_HW_KEY	24
#define MAX_HW_IV	8

#define MD5_DIGEST_LENGTH	16
#define MD5_CBLOCK		64

static int fd;
static int dev_failed;

typedef struct session_op session_op;

#define CDATA(ctx) EVP_C_DATA(session_op,ctx)

static void err(const char *str)
    {
    fprintf(stderr,"%s: errno %d\n",str,errno);
    }

static int dev_crypto_init(session_op *ses)
    {
    if(dev_failed)
	return 0;
    if(!fd)
	{
	int cryptodev_fd;

        if ((cryptodev_fd=open("/dev/crypto",O_RDWR,0)) < 0)
	    {
	    err("/dev/crypto");
	    dev_failed=1;
	    return 0;
	    }
        if (ioctl(cryptodev_fd,CRIOGET,&fd) == -1)
	    {
	    err("CRIOGET failed");
	    close(cryptodev_fd);
	    dev_failed=1;
	    return 0;
	    }
	close(cryptodev_fd);
	}
    assert(ses);
    memset(ses,'\0',sizeof *ses);

    return 1;
    }

static int dev_crypto_cleanup(EVP_CIPHER_CTX *ctx)
    {
    if(ioctl(fd,CIOCFSESSION,&CDATA(ctx)->ses) == -1)
	err("CIOCFSESSION failed");

    OPENSSL_free(CDATA(ctx)->key);

    return 1;
    }

static int dev_crypto_init_key(EVP_CIPHER_CTX *ctx,int cipher,
			       const unsigned char *key,int klen)
    {
    if(!dev_crypto_init(CDATA(ctx)))
	return 0;

    CDATA(ctx)->key=OPENSSL_malloc(MAX_HW_KEY);

    assert(ctx->cipher->iv_len <= MAX_HW_IV);

    memcpy(CDATA(ctx)->key,key,klen);
    
    CDATA(ctx)->cipher=cipher;
    CDATA(ctx)->keylen=klen;

    if (ioctl(fd,CIOCGSESSION,CDATA(ctx)) == -1)
	{
	err("CIOCGSESSION failed");
	return 0;
	}
    return 1;
    }

static int dev_crypto_cipher(EVP_CIPHER_CTX *ctx,unsigned char *out,
			     const unsigned char *in,unsigned int inl)
    {
    struct crypt_op cryp;
    unsigned char lb[MAX_HW_IV];

    if(!inl)
	return 1;

    assert(CDATA(ctx));
    assert(!dev_failed);

    memset(&cryp,'\0',sizeof cryp);
    cryp.ses=CDATA(ctx)->ses;
    cryp.op=ctx->encrypt ? COP_ENCRYPT : COP_DECRYPT;
    cryp.flags=0;
    cryp.len=inl;
    assert((inl&(ctx->cipher->block_size-1)) == 0);
    cryp.src=(caddr_t)in;
    cryp.dst=(caddr_t)out;
    cryp.mac=0;
    if(ctx->cipher->iv_len)
	cryp.iv=(caddr_t)ctx->iv;

    if(!ctx->encrypt)
	memcpy(lb,&in[cryp.len-ctx->cipher->iv_len],ctx->cipher->iv_len);

    if(ioctl(fd, CIOCCRYPT, &cryp) == -1)
	{
	if(errno == EINVAL) /* buffers are misaligned */
	    {
	    unsigned int cinl=0;
	    char *cin=NULL;
	    char *cout=NULL;

	    /* NB: this can only make cinl != inl with stream ciphers */
	    cinl=(inl+3)/4*4;

	    if(((unsigned long)in&3) || cinl != inl)
		{
		cin=OPENSSL_malloc(cinl);
		memcpy(cin,in,inl);
		cryp.src=cin;
		}

	    if(((unsigned long)out&3) || cinl != inl)
		{
		cout=OPENSSL_malloc(cinl);
		cryp.dst=cout;
		}

	    cryp.len=cinl;

	    if(ioctl(fd, CIOCCRYPT, &cryp) == -1)
		{
		err("CIOCCRYPT(2) failed");
		printf("src=%p dst=%p\n",cryp.src,cryp.dst);
		abort();
		return 0;
		}
		
	    if(cout)
		{
		memcpy(out,cout,inl);
		OPENSSL_free(cout);
		}
	    if(cin)
		OPENSSL_free(cin);
	    }
	else 
	    {	    
	    err("CIOCCRYPT failed");
	    abort();
	    return 0;
	    }
	}

    if(ctx->encrypt)
	memcpy(ctx->iv,&out[cryp.len-ctx->cipher->iv_len],ctx->cipher->iv_len);
    else
	memcpy(ctx->iv,lb,ctx->cipher->iv_len);

    return 1;
    }

static int dev_crypto_des_ede3_init_key(EVP_CIPHER_CTX *ctx,
					const unsigned char *key,
					const unsigned char *iv, int enc)
    { return dev_crypto_init_key(ctx,CRYPTO_3DES_CBC,key,24); }

#define dev_crypto_des_ede3_cbc_cipher dev_crypto_cipher

BLOCK_CIPHER_def_cbc(dev_crypto_des_ede3, session_op, NID_des_ede3, 8, 24, 8,
		     0, dev_crypto_des_ede3_init_key,
		     dev_crypto_cleanup, 
		     EVP_CIPHER_set_asn1_iv,
		     EVP_CIPHER_get_asn1_iv,
		     NULL)

static int dev_crypto_rc4_init_key(EVP_CIPHER_CTX *ctx,
					const unsigned char *key,
					const unsigned char *iv, int enc)
    { return dev_crypto_init_key(ctx,CRYPTO_ARC4,key,16); }

static const EVP_CIPHER r4_cipher=
    {
    NID_rc4,
    1,16,0,	/* FIXME: key should be up to 256 bytes */
    EVP_CIPH_VARIABLE_LENGTH,
    dev_crypto_rc4_init_key,
    dev_crypto_cipher,
    dev_crypto_cleanup,
    sizeof(session_op),
    NULL,
    NULL,
    NULL
    };

const EVP_CIPHER *EVP_dev_crypto_rc4(void)
    { return &r4_cipher; }

typedef struct
    {
    session_op sess;
    char *data;
    int len;
    unsigned char md[EVP_MAX_MD_SIZE];
    } MD_DATA;

static int dev_crypto_init_digest(MD_DATA *md_data,int mac)
    {
    if(!dev_crypto_init(&md_data->sess))
	return 0;

    md_data->len=0;
    md_data->data=NULL;

    md_data->sess.mac=mac;

    if (ioctl(fd,CIOCGSESSION,&md_data->sess) == -1)
	{
	err("CIOCGSESSION failed");
	return 0;
	}
    return 1;
    }

static int dev_crypto_cleanup_digest(MD_DATA *md_data)
    {
    if (ioctl(fd,CIOCFSESSION,&md_data->sess.ses) == -1)
	{
	err("CIOCFSESSION failed");
	return 0;
	}

    return 1;
    }

/* FIXME: if device can do chained MACs, then don't accumulate */
/* FIXME: move accumulation to the framework */
static int dev_crypto_md5_init(EVP_MD_CTX *ctx)
    { return dev_crypto_init_digest(ctx->md_data,CRYPTO_MD5); }

static int do_digest(int ses,unsigned char *md,const void *data,int len)
    {
    struct crypt_op cryp;
    static unsigned char md5zero[16]=
	{
	0xd4,0x1d,0x8c,0xd9,0x8f,0x00,0xb2,0x04,
	0xe9,0x80,0x09,0x98,0xec,0xf8,0x42,0x7e
	};

    /* some cards can't do zero length */
    if(!len)
	{
	memcpy(md,md5zero,16);
	return 1;
	}

    memset(&cryp,'\0',sizeof cryp);
    cryp.ses=ses;
    cryp.op=COP_ENCRYPT;/* required to do the MAC rather than check it */
    cryp.len=len;
    cryp.src=(caddr_t)data;
    cryp.dst=(caddr_t)data; // FIXME!!!
    cryp.mac=(caddr_t)md;

    if(ioctl(fd, CIOCCRYPT, &cryp) == -1)
	{
	if(errno == EINVAL) /* buffer is misaligned */
	    {
	    char *dcopy;

	    dcopy=OPENSSL_malloc(len);
	    memcpy(dcopy,data,len);
	    cryp.src=dcopy;
	    cryp.dst=cryp.src; // FIXME!!!

	    if(ioctl(fd, CIOCCRYPT, &cryp) == -1)
		{
		err("CIOCCRYPT(MAC2) failed");
		abort();
		return 0;
		}
	    OPENSSL_free(dcopy);
	    }
	else
	    {
	    err("CIOCCRYPT(MAC) failed");
	    abort();
	    return 0;
	    }
	}
    //    printf("done\n");

    return 1;
    }

static int dev_crypto_md5_update(EVP_MD_CTX *ctx,const void *data,
				 unsigned long len)
    {
    MD_DATA *md_data=ctx->md_data;

    if(ctx->flags&EVP_MD_CTX_FLAG_ONESHOT)
	return do_digest(md_data->sess.ses,md_data->md,data,len);

    md_data->data=OPENSSL_realloc(md_data->data,md_data->len+len);
    memcpy(md_data->data+md_data->len,data,len);
    md_data->len+=len;

    return 1;
    }	

static int dev_crypto_md5_final(EVP_MD_CTX *ctx,unsigned char *md)
    {
    int ret;
    MD_DATA *md_data=ctx->md_data;

    if(ctx->flags&EVP_MD_CTX_FLAG_ONESHOT)
	{
	memcpy(md,md_data->md,MD5_DIGEST_LENGTH);
	ret=1;
	}
    else
	{
	ret=do_digest(md_data->sess.ses,md,md_data->data,md_data->len);
	OPENSSL_free(md_data->data);
	md_data->data=NULL;
	md_data->len=0;
	}

    return ret;
    }

static int dev_crypto_md5_copy(EVP_MD_CTX *to,const EVP_MD_CTX *from)
    {
    const MD_DATA *from_md=from->md_data;
    MD_DATA *to_md=to->md_data;

    // How do we copy sessions?
    assert(from->digest->flags&EVP_MD_FLAG_ONESHOT);

    to_md->data=OPENSSL_malloc(from_md->len);
    memcpy(to_md->data,from_md->data,from_md->len);

    return 1;
    }

static int dev_crypto_md5_cleanup(EVP_MD_CTX *ctx)
    {
    return dev_crypto_cleanup_digest(ctx->md_data);
    }

static const EVP_MD md5_md=
    {
    NID_md5,
    NID_md5WithRSAEncryption,
    MD5_DIGEST_LENGTH,
    EVP_MD_FLAG_ONESHOT,	// XXX: set according to device info...
    dev_crypto_md5_init,
    dev_crypto_md5_update,
    dev_crypto_md5_final,
    dev_crypto_md5_copy,
    dev_crypto_md5_cleanup,
    EVP_PKEY_RSA_method,
    MD5_CBLOCK,
    sizeof(MD_DATA),
    };

const EVP_MD *EVP_dev_crypto_md5(void)
    { return &md5_md; }

#endif
#endif
