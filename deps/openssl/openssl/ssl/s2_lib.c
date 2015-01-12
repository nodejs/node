/* ssl/s2_lib.c */
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
 * Copyright (c) 1998-2007 The OpenSSL Project.  All rights reserved.
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

#include "ssl_locl.h"
#ifndef OPENSSL_NO_SSL2
#include <stdio.h>
#include <openssl/objects.h>
#include <openssl/evp.h>
#include <openssl/md5.h>

const char ssl2_version_str[]="SSLv2" OPENSSL_VERSION_PTEXT;

#define SSL2_NUM_CIPHERS (sizeof(ssl2_ciphers)/sizeof(SSL_CIPHER))

/* list of available SSLv2 ciphers (sorted by id) */
OPENSSL_GLOBAL const SSL_CIPHER ssl2_ciphers[]={
#if 0
/* NULL_WITH_MD5 v3 */
	{
	1,
	SSL2_TXT_NULL_WITH_MD5,
	SSL2_CK_NULL_WITH_MD5,
	SSL_kRSA,
	SSL_aRSA,
	SSL_eNULL,
	SSL_MD5,
	SSL_SSLV2,
	SSL_EXPORT|SSL_EXP40|SSL_STRONG_NONE,
	0,
	0,
	0,
	},
#endif

/* RC4_128_WITH_MD5 */
	{
	1,
	SSL2_TXT_RC4_128_WITH_MD5,
	SSL2_CK_RC4_128_WITH_MD5,
	SSL_kRSA,
	SSL_aRSA,
	SSL_RC4,
	SSL_MD5,
	SSL_SSLV2,
	SSL_NOT_EXP|SSL_MEDIUM,
	0,
	128,
	128,
	},

/* RC4_128_EXPORT40_WITH_MD5 */
	{
	1,
	SSL2_TXT_RC4_128_EXPORT40_WITH_MD5,
	SSL2_CK_RC4_128_EXPORT40_WITH_MD5,
	SSL_kRSA,
	SSL_aRSA,
	SSL_RC4,
	SSL_MD5,
	SSL_SSLV2,
	SSL_EXPORT|SSL_EXP40,
	SSL2_CF_5_BYTE_ENC,
	40,
	128,
	},

/* RC2_128_CBC_WITH_MD5 */
	{
	1,
	SSL2_TXT_RC2_128_CBC_WITH_MD5,
	SSL2_CK_RC2_128_CBC_WITH_MD5,
	SSL_kRSA,
	SSL_aRSA,
	SSL_RC2,
	SSL_MD5,
	SSL_SSLV2,
	SSL_NOT_EXP|SSL_MEDIUM,
	0,
	128,
	128,
	},

/* RC2_128_CBC_EXPORT40_WITH_MD5 */
	{
	1,
	SSL2_TXT_RC2_128_CBC_EXPORT40_WITH_MD5,
	SSL2_CK_RC2_128_CBC_EXPORT40_WITH_MD5,
	SSL_kRSA,
	SSL_aRSA,
	SSL_RC2,
	SSL_MD5,
	SSL_SSLV2,
	SSL_EXPORT|SSL_EXP40,
	SSL2_CF_5_BYTE_ENC,
	40,
	128,
	},

#ifndef OPENSSL_NO_IDEA
/* IDEA_128_CBC_WITH_MD5 */
	{
	1,
	SSL2_TXT_IDEA_128_CBC_WITH_MD5,
	SSL2_CK_IDEA_128_CBC_WITH_MD5,
	SSL_kRSA,
	SSL_aRSA,
	SSL_IDEA,
	SSL_MD5,
	SSL_SSLV2,
	SSL_NOT_EXP|SSL_MEDIUM,
	0,
	128,
	128,
	},
#endif

/* DES_64_CBC_WITH_MD5 */
	{
	1,
	SSL2_TXT_DES_64_CBC_WITH_MD5,
	SSL2_CK_DES_64_CBC_WITH_MD5,
	SSL_kRSA,
	SSL_aRSA,
	SSL_DES,
	SSL_MD5,
	SSL_SSLV2,
	SSL_NOT_EXP|SSL_LOW,
	0,
	56,
	56,
	},

/* DES_192_EDE3_CBC_WITH_MD5 */
	{
	1,
	SSL2_TXT_DES_192_EDE3_CBC_WITH_MD5,
	SSL2_CK_DES_192_EDE3_CBC_WITH_MD5,
	SSL_kRSA,
	SSL_aRSA,
	SSL_3DES,
	SSL_MD5,
	SSL_SSLV2,
	SSL_NOT_EXP|SSL_HIGH,
	0,
	112,
	168,
	},

#if 0
/* RC4_64_WITH_MD5 */
	{
	1,
	SSL2_TXT_RC4_64_WITH_MD5,
	SSL2_CK_RC4_64_WITH_MD5,
	SSL_kRSA,
	SSL_aRSA,
	SSL_RC4,
	SSL_MD5,
	SSL_SSLV2,
	SSL_NOT_EXP|SSL_LOW,
	SSL2_CF_8_BYTE_ENC,
	64,
	64,
	},
#endif

#if 0
/* NULL SSLeay (testing) */
	{	
	0,
	SSL2_TXT_NULL,
	SSL2_CK_NULL,
	0,
	0,
	0,
	0,
	SSL_SSLV2,
	SSL_STRONG_NONE,
	0,
	0,
	0,
	},
#endif

/* end of list :-) */
	};

long ssl2_default_timeout(void)
	{
	return(300);
	}

int ssl2_num_ciphers(void)
	{
	return(SSL2_NUM_CIPHERS);
	}

const SSL_CIPHER *ssl2_get_cipher(unsigned int u)
	{
	if (u < SSL2_NUM_CIPHERS)
		return(&(ssl2_ciphers[SSL2_NUM_CIPHERS-1-u]));
	else
		return(NULL);
	}

int ssl2_pending(const SSL *s)
	{
	return SSL_in_init(s) ? 0 : s->s2->ract_data_length;
	}

int ssl2_new(SSL *s)
	{
	SSL2_STATE *s2;

	if ((s2=OPENSSL_malloc(sizeof *s2)) == NULL) goto err;
	memset(s2,0,sizeof *s2);

#if SSL2_MAX_RECORD_LENGTH_3_BYTE_HEADER + 3 > SSL2_MAX_RECORD_LENGTH_2_BYTE_HEADER + 2
#  error "assertion failed"
#endif

	if ((s2->rbuf=OPENSSL_malloc(
		SSL2_MAX_RECORD_LENGTH_2_BYTE_HEADER+2)) == NULL) goto err;
	/* wbuf needs one byte more because when using two-byte headers,
	 * we leave the first byte unused in do_ssl_write (s2_pkt.c) */
	if ((s2->wbuf=OPENSSL_malloc(
		SSL2_MAX_RECORD_LENGTH_2_BYTE_HEADER+3)) == NULL) goto err;
	s->s2=s2;

	ssl2_clear(s);
	return(1);
err:
	if (s2 != NULL)
		{
		if (s2->wbuf != NULL) OPENSSL_free(s2->wbuf);
		if (s2->rbuf != NULL) OPENSSL_free(s2->rbuf);
		OPENSSL_free(s2);
		}
	return(0);
	}

void ssl2_free(SSL *s)
	{
	SSL2_STATE *s2;

	if(s == NULL)
	    return;

	s2=s->s2;
	if (s2->rbuf != NULL) OPENSSL_free(s2->rbuf);
	if (s2->wbuf != NULL) OPENSSL_free(s2->wbuf);
	OPENSSL_cleanse(s2,sizeof *s2);
	OPENSSL_free(s2);
	s->s2=NULL;
	}

void ssl2_clear(SSL *s)
	{
	SSL2_STATE *s2;
	unsigned char *rbuf,*wbuf;

	s2=s->s2;

	rbuf=s2->rbuf;
	wbuf=s2->wbuf;

	memset(s2,0,sizeof *s2);

	s2->rbuf=rbuf;
	s2->wbuf=wbuf;
	s2->clear_text=1;
	s->packet=s2->rbuf;
	s->version=SSL2_VERSION;
	s->packet_length=0;
	}

long ssl2_ctrl(SSL *s, int cmd, long larg, void *parg)
	{
	int ret=0;

	switch(cmd)
		{
	case SSL_CTRL_GET_SESSION_REUSED:
		ret=s->hit;
		break;
	case SSL_CTRL_CHECK_PROTO_VERSION:
		return ssl3_ctrl(s, SSL_CTRL_CHECK_PROTO_VERSION, larg, parg);
	default:
		break;
		}
	return(ret);
	}

long ssl2_callback_ctrl(SSL *s, int cmd, void (*fp)(void))
	{
	return(0);
	}

long ssl2_ctx_ctrl(SSL_CTX *ctx, int cmd, long larg, void *parg)
	{
	return(0);
	}

long ssl2_ctx_callback_ctrl(SSL_CTX *ctx, int cmd, void (*fp)(void))
	{
	return(0);
	}

/* This function needs to check if the ciphers required are actually
 * available */
const SSL_CIPHER *ssl2_get_cipher_by_char(const unsigned char *p)
	{
	SSL_CIPHER c;
	const SSL_CIPHER *cp;
	unsigned long id;

	id=0x02000000L|((unsigned long)p[0]<<16L)|
		((unsigned long)p[1]<<8L)|(unsigned long)p[2];
	c.id=id;
	cp = OBJ_bsearch_ssl_cipher_id(&c, ssl2_ciphers, SSL2_NUM_CIPHERS);
	if ((cp == NULL) || (cp->valid == 0))
		return NULL;
	else
		return cp;
	}

int ssl2_put_cipher_by_char(const SSL_CIPHER *c, unsigned char *p)
	{
	long l;

	if (p != NULL)
		{
		l=c->id;
		if ((l & 0xff000000) != 0x02000000 && l != SSL3_CK_FALLBACK_SCSV) return(0);
		p[0]=((unsigned char)(l>>16L))&0xFF;
		p[1]=((unsigned char)(l>> 8L))&0xFF;
		p[2]=((unsigned char)(l     ))&0xFF;
		}
	return(3);
	}

int ssl2_generate_key_material(SSL *s)
	{
	unsigned int i;
	EVP_MD_CTX ctx;
	unsigned char *km;
	unsigned char c='0';
	const EVP_MD *md5;
	int md_size;

	md5 = EVP_md5();

#ifdef CHARSET_EBCDIC
	c = os_toascii['0']; /* Must be an ASCII '0', not EBCDIC '0',
				see SSLv2 docu */
#endif
	EVP_MD_CTX_init(&ctx);
	km=s->s2->key_material;

 	if (s->session->master_key_length < 0 ||
			s->session->master_key_length > (int)sizeof(s->session->master_key))
 		{
 		SSLerr(SSL_F_SSL2_GENERATE_KEY_MATERIAL, ERR_R_INTERNAL_ERROR);
 		return 0;
 		}
	md_size = EVP_MD_size(md5);
	if (md_size < 0)
	    return 0;
	for (i=0; i<s->s2->key_material_length; i += md_size)
		{
		if (((km - s->s2->key_material) + md_size) >
				(int)sizeof(s->s2->key_material))
			{
			/* EVP_DigestFinal_ex() below would write beyond buffer */
			SSLerr(SSL_F_SSL2_GENERATE_KEY_MATERIAL, ERR_R_INTERNAL_ERROR);
			return 0;
			}

		EVP_DigestInit_ex(&ctx, md5, NULL);

		OPENSSL_assert(s->session->master_key_length >= 0
		    && s->session->master_key_length
		    < (int)sizeof(s->session->master_key));
		EVP_DigestUpdate(&ctx,s->session->master_key,s->session->master_key_length);
		EVP_DigestUpdate(&ctx,&c,1);
		c++;
		EVP_DigestUpdate(&ctx,s->s2->challenge,s->s2->challenge_length);
		EVP_DigestUpdate(&ctx,s->s2->conn_id,s->s2->conn_id_length);
		EVP_DigestFinal_ex(&ctx,km,NULL);
		km += md_size;
		}

	EVP_MD_CTX_cleanup(&ctx);
	return 1;
	}

void ssl2_return_error(SSL *s, int err)
	{
	if (!s->error)
		{
		s->error=3;
		s->error_code=err;

		ssl2_write_error(s);
		}
	}


void ssl2_write_error(SSL *s)
	{
	unsigned char buf[3];
	int i,error;

	buf[0]=SSL2_MT_ERROR;
	buf[1]=(s->error_code>>8)&0xff;
	buf[2]=(s->error_code)&0xff;

/*	state=s->rwstate;*/

	error=s->error; /* number of bytes left to write */
	s->error=0;
	OPENSSL_assert(error >= 0 && error <= (int)sizeof(buf));
	i=ssl2_write(s,&(buf[3-error]),error);

/*	if (i == error) s->rwstate=state; */

	if (i < 0)
		s->error=error;
	else
		{
		s->error=error-i;

		if (s->error == 0)
			if (s->msg_callback)
				s->msg_callback(1, s->version, 0, buf, 3, s, s->msg_callback_arg); /* ERROR */
		}
	}

int ssl2_shutdown(SSL *s)
	{
	s->shutdown=(SSL_SENT_SHUTDOWN|SSL_RECEIVED_SHUTDOWN);
	return(1);
	}
#else /* !OPENSSL_NO_SSL2 */

# if PEDANTIC
static void *dummy=&dummy;
# endif

#endif
