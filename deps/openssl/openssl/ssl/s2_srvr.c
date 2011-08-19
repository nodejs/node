/* ssl/s2_srvr.c */
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
 * Copyright (c) 1998-2001 The OpenSSL Project.  All rights reserved.
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
#include <openssl/bio.h>
#include <openssl/rand.h>
#include <openssl/objects.h>
#include <openssl/evp.h>

static SSL_METHOD *ssl2_get_server_method(int ver);
static int get_client_master_key(SSL *s);
static int get_client_hello(SSL *s);
static int server_hello(SSL *s); 
static int get_client_finished(SSL *s);
static int server_verify(SSL *s);
static int server_finish(SSL *s);
static int request_certificate(SSL *s);
static int ssl_rsa_private_decrypt(CERT *c, int len, unsigned char *from,
	unsigned char *to,int padding);
#define BREAK	break

static SSL_METHOD *ssl2_get_server_method(int ver)
	{
	if (ver == SSL2_VERSION)
		return(SSLv2_server_method());
	else
		return(NULL);
	}

IMPLEMENT_ssl2_meth_func(SSLv2_server_method,
			ssl2_accept,
			ssl_undefined_function,
			ssl2_get_server_method)

int ssl2_accept(SSL *s)
	{
	unsigned long l=(unsigned long)time(NULL);
	BUF_MEM *buf=NULL;
	int ret= -1;
	long num1;
	void (*cb)(const SSL *ssl,int type,int val)=NULL;
	int new_state,state;

	RAND_add(&l,sizeof(l),0);
	ERR_clear_error();
	clear_sys_error();

	if (s->info_callback != NULL)
		cb=s->info_callback;
	else if (s->ctx->info_callback != NULL)
		cb=s->ctx->info_callback;

	/* init things to blank */
	s->in_handshake++;
	if (!SSL_in_init(s) || SSL_in_before(s)) SSL_clear(s);

	if (s->cert == NULL)
		{
		SSLerr(SSL_F_SSL2_ACCEPT,SSL_R_NO_CERTIFICATE_SET);
		return(-1);
		}

	clear_sys_error();
	for (;;)
		{
		state=s->state;

		switch (s->state)
			{
		case SSL_ST_BEFORE:
		case SSL_ST_ACCEPT:
		case SSL_ST_BEFORE|SSL_ST_ACCEPT:
		case SSL_ST_OK|SSL_ST_ACCEPT:

			s->server=1;
			if (cb != NULL) cb(s,SSL_CB_HANDSHAKE_START,1);

			s->version=SSL2_VERSION;
			s->type=SSL_ST_ACCEPT;

			buf=s->init_buf;
			if ((buf == NULL) && ((buf=BUF_MEM_new()) == NULL))
				{ ret= -1; goto end; }
			if (!BUF_MEM_grow(buf,(int)
				SSL2_MAX_RECORD_LENGTH_3_BYTE_HEADER))
				{ ret= -1; goto end; }
			s->init_buf=buf;
			s->init_num=0;
			s->ctx->stats.sess_accept++;
			s->handshake_func=ssl2_accept;
			s->state=SSL2_ST_GET_CLIENT_HELLO_A;
			BREAK;

		case SSL2_ST_GET_CLIENT_HELLO_A:
		case SSL2_ST_GET_CLIENT_HELLO_B:
		case SSL2_ST_GET_CLIENT_HELLO_C:
			s->shutdown=0;
			ret=get_client_hello(s);
			if (ret <= 0) goto end;
			s->init_num=0;
			s->state=SSL2_ST_SEND_SERVER_HELLO_A;
			BREAK;

		case SSL2_ST_SEND_SERVER_HELLO_A:
		case SSL2_ST_SEND_SERVER_HELLO_B:
			ret=server_hello(s);
			if (ret <= 0) goto end;
			s->init_num=0;
			if (!s->hit)
				{
				s->state=SSL2_ST_GET_CLIENT_MASTER_KEY_A;
				BREAK;
				}
			else
				{
				s->state=SSL2_ST_SERVER_START_ENCRYPTION;
				BREAK;
				}
		case SSL2_ST_GET_CLIENT_MASTER_KEY_A:
		case SSL2_ST_GET_CLIENT_MASTER_KEY_B:
			ret=get_client_master_key(s);
			if (ret <= 0) goto end;
			s->init_num=0;
			s->state=SSL2_ST_SERVER_START_ENCRYPTION;
			BREAK;

		case SSL2_ST_SERVER_START_ENCRYPTION:
			/* Ok we how have sent all the stuff needed to
			 * start encrypting, the next packet back will
			 * be encrypted. */
			if (!ssl2_enc_init(s,0))
				{ ret= -1; goto end; }
			s->s2->clear_text=0;
			s->state=SSL2_ST_SEND_SERVER_VERIFY_A;
			BREAK;

		case SSL2_ST_SEND_SERVER_VERIFY_A:
		case SSL2_ST_SEND_SERVER_VERIFY_B:
			ret=server_verify(s);
			if (ret <= 0) goto end;
			s->init_num=0;
			if (s->hit)
				{
				/* If we are in here, we have been
				 * buffering the output, so we need to
				 * flush it and remove buffering from
				 * future traffic */
				s->state=SSL2_ST_SEND_SERVER_VERIFY_C;
				BREAK;
				}
			else
				{
				s->state=SSL2_ST_GET_CLIENT_FINISHED_A;
				break;
				}

 		case SSL2_ST_SEND_SERVER_VERIFY_C:
 			/* get the number of bytes to write */
 			num1=BIO_ctrl(s->wbio,BIO_CTRL_INFO,0,NULL);
 			if (num1 > 0)
 				{
				s->rwstate=SSL_WRITING;
 				num1=BIO_flush(s->wbio);
 				if (num1 <= 0) { ret= -1; goto end; }
				s->rwstate=SSL_NOTHING;
				}

 			/* flushed and now remove buffering */
 			s->wbio=BIO_pop(s->wbio);

 			s->state=SSL2_ST_GET_CLIENT_FINISHED_A;
  			BREAK;

		case SSL2_ST_GET_CLIENT_FINISHED_A:
		case SSL2_ST_GET_CLIENT_FINISHED_B:
			ret=get_client_finished(s);
			if (ret <= 0)
				goto end;
			s->init_num=0;
			s->state=SSL2_ST_SEND_REQUEST_CERTIFICATE_A;
			BREAK;

		case SSL2_ST_SEND_REQUEST_CERTIFICATE_A:
		case SSL2_ST_SEND_REQUEST_CERTIFICATE_B:
		case SSL2_ST_SEND_REQUEST_CERTIFICATE_C:
		case SSL2_ST_SEND_REQUEST_CERTIFICATE_D:
			/* don't do a 'request certificate' if we
			 * don't want to, or we already have one, and
			 * we only want to do it once. */
			if (!(s->verify_mode & SSL_VERIFY_PEER) ||
				((s->session->peer != NULL) &&
				(s->verify_mode & SSL_VERIFY_CLIENT_ONCE)))
				{
				s->state=SSL2_ST_SEND_SERVER_FINISHED_A;
				break;
				}
			else
				{
				ret=request_certificate(s);
				if (ret <= 0) goto end;
				s->init_num=0;
				s->state=SSL2_ST_SEND_SERVER_FINISHED_A;
				}
			BREAK;

		case SSL2_ST_SEND_SERVER_FINISHED_A:
		case SSL2_ST_SEND_SERVER_FINISHED_B:
			ret=server_finish(s);
			if (ret <= 0) goto end;
			s->init_num=0;
			s->state=SSL_ST_OK;
			break;

		case SSL_ST_OK:
			BUF_MEM_free(s->init_buf);
			ssl_free_wbio_buffer(s);
			s->init_buf=NULL;
			s->init_num=0;
		/*	ERR_clear_error();*/

			ssl_update_cache(s,SSL_SESS_CACHE_SERVER);

			s->ctx->stats.sess_accept_good++;
			/* s->server=1; */
			ret=1;

			if (cb != NULL) cb(s,SSL_CB_HANDSHAKE_DONE,1);

			goto end;
			/* BREAK; */

		default:
			SSLerr(SSL_F_SSL2_ACCEPT,SSL_R_UNKNOWN_STATE);
			ret= -1;
			goto end;
			/* BREAK; */
			}
		
		if ((cb != NULL) && (s->state != state))
			{
			new_state=s->state;
			s->state=state;
			cb(s,SSL_CB_ACCEPT_LOOP,1);
			s->state=new_state;
			}
		}
end:
	s->in_handshake--;
	if (cb != NULL)
		cb(s,SSL_CB_ACCEPT_EXIT,ret);
	return(ret);
	}

static int get_client_master_key(SSL *s)
	{
	int is_export,i,n,keya,ek;
	unsigned long len;
	unsigned char *p;
	SSL_CIPHER *cp;
	const EVP_CIPHER *c;
	const EVP_MD *md;

	p=(unsigned char *)s->init_buf->data;
	if (s->state == SSL2_ST_GET_CLIENT_MASTER_KEY_A)
		{
		i=ssl2_read(s,(char *)&(p[s->init_num]),10-s->init_num);

		if (i < (10-s->init_num))
			return(ssl2_part_read(s,SSL_F_GET_CLIENT_MASTER_KEY,i));
		s->init_num = 10;

		if (*(p++) != SSL2_MT_CLIENT_MASTER_KEY)
			{
			if (p[-1] != SSL2_MT_ERROR)
				{
				ssl2_return_error(s,SSL2_PE_UNDEFINED_ERROR);
				SSLerr(SSL_F_GET_CLIENT_MASTER_KEY,SSL_R_READ_WRONG_PACKET_TYPE);
				}
			else
				SSLerr(SSL_F_GET_CLIENT_MASTER_KEY, SSL_R_PEER_ERROR);
			return(-1);
			}

		cp=ssl2_get_cipher_by_char(p);
		if (cp == NULL)
			{
			ssl2_return_error(s,SSL2_PE_NO_CIPHER);
			SSLerr(SSL_F_GET_CLIENT_MASTER_KEY, SSL_R_NO_CIPHER_MATCH);
			return(-1);
			}
		s->session->cipher= cp;

		p+=3;
		n2s(p,i); s->s2->tmp.clear=i;
		n2s(p,i); s->s2->tmp.enc=i;
		n2s(p,i); s->session->key_arg_length=i;
		if(s->session->key_arg_length > SSL_MAX_KEY_ARG_LENGTH)
			{
			ssl2_return_error(s,SSL2_PE_UNDEFINED_ERROR);
			SSLerr(SSL_F_GET_CLIENT_MASTER_KEY, SSL_R_KEY_ARG_TOO_LONG);
			return -1;
			}
		s->state=SSL2_ST_GET_CLIENT_MASTER_KEY_B;
		}

	/* SSL2_ST_GET_CLIENT_MASTER_KEY_B */
	p=(unsigned char *)s->init_buf->data;
	if (s->init_buf->length < SSL2_MAX_RECORD_LENGTH_3_BYTE_HEADER)
		{
		ssl2_return_error(s,SSL2_PE_UNDEFINED_ERROR);
		SSLerr(SSL_F_GET_CLIENT_MASTER_KEY, ERR_R_INTERNAL_ERROR);
		return -1;
		}
	keya=s->session->key_arg_length;
	len = 10 + (unsigned long)s->s2->tmp.clear + (unsigned long)s->s2->tmp.enc + (unsigned long)keya;
	if (len > SSL2_MAX_RECORD_LENGTH_3_BYTE_HEADER)
		{
		ssl2_return_error(s,SSL2_PE_UNDEFINED_ERROR);
		SSLerr(SSL_F_GET_CLIENT_MASTER_KEY,SSL_R_MESSAGE_TOO_LONG);
		return -1;
		}
	n = (int)len - s->init_num;
	i = ssl2_read(s,(char *)&(p[s->init_num]),n);
	if (i != n) return(ssl2_part_read(s,SSL_F_GET_CLIENT_MASTER_KEY,i));
	if (s->msg_callback)
		s->msg_callback(0, s->version, 0, p, (size_t)len, s, s->msg_callback_arg); /* CLIENT-MASTER-KEY */
	p += 10;

	memcpy(s->session->key_arg,&(p[s->s2->tmp.clear+s->s2->tmp.enc]),
		(unsigned int)keya);

	if (s->cert->pkeys[SSL_PKEY_RSA_ENC].privatekey == NULL)
		{
		ssl2_return_error(s,SSL2_PE_UNDEFINED_ERROR);
		SSLerr(SSL_F_GET_CLIENT_MASTER_KEY,SSL_R_NO_PRIVATEKEY);
		return(-1);
		}
	i=ssl_rsa_private_decrypt(s->cert,s->s2->tmp.enc,
		&(p[s->s2->tmp.clear]),&(p[s->s2->tmp.clear]),
		(s->s2->ssl2_rollback)?RSA_SSLV23_PADDING:RSA_PKCS1_PADDING);

	is_export=SSL_C_IS_EXPORT(s->session->cipher);
	
	if (!ssl_cipher_get_evp(s->session,&c,&md,NULL))
		{
		ssl2_return_error(s,SSL2_PE_NO_CIPHER);
		SSLerr(SSL_F_GET_CLIENT_MASTER_KEY,SSL_R_PROBLEMS_MAPPING_CIPHER_FUNCTIONS);
		return(0);
		}

	if (s->session->cipher->algorithm2 & SSL2_CF_8_BYTE_ENC)
		{
		is_export=1;
		ek=8;
		}
	else
		ek=5;

	/* bad decrypt */
#if 1
	/* If a bad decrypt, continue with protocol but with a
	 * random master secret (Bleichenbacher attack) */
	if ((i < 0) ||
		((!is_export && (i != EVP_CIPHER_key_length(c)))
		|| (is_export && ((i != ek) || (s->s2->tmp.clear+(unsigned int)i !=
			(unsigned int)EVP_CIPHER_key_length(c))))))
		{
		ERR_clear_error();
		if (is_export)
			i=ek;
		else
			i=EVP_CIPHER_key_length(c);
		if (RAND_pseudo_bytes(p,i) <= 0)
			return 0;
		}
#else
	if (i < 0)
		{
		error=1;
		SSLerr(SSL_F_GET_CLIENT_MASTER_KEY,SSL_R_BAD_RSA_DECRYPT);
		}
	/* incorrect number of key bytes for non export cipher */
	else if ((!is_export && (i != EVP_CIPHER_key_length(c)))
		|| (is_export && ((i != ek) || (s->s2->tmp.clear+i !=
			EVP_CIPHER_key_length(c)))))
		{
		error=1;
		SSLerr(SSL_F_GET_CLIENT_MASTER_KEY,SSL_R_WRONG_NUMBER_OF_KEY_BITS);
		}
	if (error)
		{
		ssl2_return_error(s,SSL2_PE_UNDEFINED_ERROR);
		return(-1);
		}
#endif

	if (is_export) i+=s->s2->tmp.clear;

	if (i > SSL_MAX_MASTER_KEY_LENGTH)
		{
		ssl2_return_error(s,SSL2_PE_UNDEFINED_ERROR);
		SSLerr(SSL_F_GET_CLIENT_MASTER_KEY, ERR_R_INTERNAL_ERROR);
		return -1;
		}
	s->session->master_key_length=i;
	memcpy(s->session->master_key,p,(unsigned int)i);
	return(1);
	}

static int get_client_hello(SSL *s)
	{
	int i,n;
	unsigned long len;
	unsigned char *p;
	STACK_OF(SSL_CIPHER) *cs; /* a stack of SSL_CIPHERS */
	STACK_OF(SSL_CIPHER) *cl; /* the ones we want to use */
	STACK_OF(SSL_CIPHER) *prio, *allow;
	int z;

	/* This is a bit of a hack to check for the correct packet
	 * type the first time round. */
	if (s->state == SSL2_ST_GET_CLIENT_HELLO_A)
		{
		s->first_packet=1;
		s->state=SSL2_ST_GET_CLIENT_HELLO_B;
		}

	p=(unsigned char *)s->init_buf->data;
	if (s->state == SSL2_ST_GET_CLIENT_HELLO_B)
		{
		i=ssl2_read(s,(char *)&(p[s->init_num]),9-s->init_num);
		if (i < (9-s->init_num)) 
			return(ssl2_part_read(s,SSL_F_GET_CLIENT_HELLO,i));
		s->init_num = 9;
	
		if (*(p++) != SSL2_MT_CLIENT_HELLO)
			{
			if (p[-1] != SSL2_MT_ERROR)
				{
				ssl2_return_error(s,SSL2_PE_UNDEFINED_ERROR);
				SSLerr(SSL_F_GET_CLIENT_HELLO,SSL_R_READ_WRONG_PACKET_TYPE);
				}
			else
				SSLerr(SSL_F_GET_CLIENT_HELLO,SSL_R_PEER_ERROR);
			return(-1);
			}
		n2s(p,i);
		if (i < s->version) s->version=i;
		n2s(p,i); s->s2->tmp.cipher_spec_length=i;
		n2s(p,i); s->s2->tmp.session_id_length=i;
		n2s(p,i); s->s2->challenge_length=i;
		if (	(i < SSL2_MIN_CHALLENGE_LENGTH) ||
			(i > SSL2_MAX_CHALLENGE_LENGTH))
			{
			ssl2_return_error(s,SSL2_PE_UNDEFINED_ERROR);
			SSLerr(SSL_F_GET_CLIENT_HELLO,SSL_R_INVALID_CHALLENGE_LENGTH);
			return(-1);
			}
		s->state=SSL2_ST_GET_CLIENT_HELLO_C;
		}

	/* SSL2_ST_GET_CLIENT_HELLO_C */
	p=(unsigned char *)s->init_buf->data;
	len = 9 + (unsigned long)s->s2->tmp.cipher_spec_length + (unsigned long)s->s2->challenge_length + (unsigned long)s->s2->tmp.session_id_length;
	if (len > SSL2_MAX_RECORD_LENGTH_3_BYTE_HEADER)
		{
		ssl2_return_error(s,SSL2_PE_UNDEFINED_ERROR);
		SSLerr(SSL_F_GET_CLIENT_HELLO,SSL_R_MESSAGE_TOO_LONG);
		return -1;
		}
	n = (int)len - s->init_num;
	i = ssl2_read(s,(char *)&(p[s->init_num]),n);
	if (i != n) return(ssl2_part_read(s,SSL_F_GET_CLIENT_HELLO,i));
	if (s->msg_callback)
		s->msg_callback(0, s->version, 0, p, (size_t)len, s, s->msg_callback_arg); /* CLIENT-HELLO */
	p += 9;

	/* get session-id before cipher stuff so we can get out session
	 * structure if it is cached */
	/* session-id */
	if ((s->s2->tmp.session_id_length != 0) && 
		(s->s2->tmp.session_id_length != SSL2_SSL_SESSION_ID_LENGTH))
		{
		ssl2_return_error(s,SSL2_PE_UNDEFINED_ERROR);
		SSLerr(SSL_F_GET_CLIENT_HELLO,SSL_R_BAD_SSL_SESSION_ID_LENGTH);
		return(-1);
		}

	if (s->s2->tmp.session_id_length == 0)
		{
		if (!ssl_get_new_session(s,1))
			{
			ssl2_return_error(s,SSL2_PE_UNDEFINED_ERROR);
			return(-1);
			}
		}
	else
		{
		i=ssl_get_prev_session(s,&(p[s->s2->tmp.cipher_spec_length]),
			s->s2->tmp.session_id_length, NULL);
		if (i == 1)
			{ /* previous session */
			s->hit=1;
			}
		else if (i == -1)
			{
			ssl2_return_error(s,SSL2_PE_UNDEFINED_ERROR);
			return(-1);
			}
		else
			{
			if (s->cert == NULL)
				{
				ssl2_return_error(s,SSL2_PE_NO_CERTIFICATE);
				SSLerr(SSL_F_GET_CLIENT_HELLO,SSL_R_NO_CERTIFICATE_SET);
				return(-1);
				}

			if (!ssl_get_new_session(s,1))
				{
				ssl2_return_error(s,SSL2_PE_UNDEFINED_ERROR);
				return(-1);
				}
			}
		}

	if (!s->hit)
		{
		cs=ssl_bytes_to_cipher_list(s,p,s->s2->tmp.cipher_spec_length,
			&s->session->ciphers);
		if (cs == NULL) goto mem_err;

		cl=SSL_get_ciphers(s);

		if (s->options & SSL_OP_CIPHER_SERVER_PREFERENCE)
		    {
		    prio=sk_SSL_CIPHER_dup(cl);
		    if (prio == NULL) goto mem_err;
		    allow = cs;
		    }
		else
		    {
		    prio = cs;
		    allow = cl;
		    }
		for (z=0; z<sk_SSL_CIPHER_num(prio); z++)
			{
			if (sk_SSL_CIPHER_find(allow,sk_SSL_CIPHER_value(prio,z)) < 0)
				{
				(void)sk_SSL_CIPHER_delete(prio,z);
				z--;
				}
			}
		if (s->options & SSL_OP_CIPHER_SERVER_PREFERENCE)
		    {
		    sk_SSL_CIPHER_free(s->session->ciphers);
		    s->session->ciphers = prio;
		    }
		/* s->session->ciphers should now have a list of
		 * ciphers that are on both the client and server.
		 * This list is ordered by the order the client sent
		 * the ciphers or in the order of the server's preference
		 * if SSL_OP_CIPHER_SERVER_PREFERENCE was set.
		 */
		}
	p+=s->s2->tmp.cipher_spec_length;
	/* done cipher selection */

	/* session id extracted already */
	p+=s->s2->tmp.session_id_length;

	/* challenge */
	if (s->s2->challenge_length > sizeof s->s2->challenge)
		{
		ssl2_return_error(s,SSL2_PE_UNDEFINED_ERROR);
		SSLerr(SSL_F_GET_CLIENT_HELLO, ERR_R_INTERNAL_ERROR);
		return -1;
		}
	memcpy(s->s2->challenge,p,(unsigned int)s->s2->challenge_length);
	return(1);
mem_err:
	SSLerr(SSL_F_GET_CLIENT_HELLO,ERR_R_MALLOC_FAILURE);
	return(0);
	}

static int server_hello(SSL *s)
	{
	unsigned char *p,*d;
	int n,hit;
	STACK_OF(SSL_CIPHER) *sk;

	p=(unsigned char *)s->init_buf->data;
	if (s->state == SSL2_ST_SEND_SERVER_HELLO_A)
		{
		d=p+11;
		*(p++)=SSL2_MT_SERVER_HELLO;		/* type */
		hit=s->hit;
		*(p++)=(unsigned char)hit;
#if 1
		if (!hit)
			{
			if (s->session->sess_cert != NULL)
				/* This can't really happen because get_client_hello
				 * has called ssl_get_new_session, which does not set
				 * sess_cert. */
				ssl_sess_cert_free(s->session->sess_cert);
			s->session->sess_cert = ssl_sess_cert_new();
			if (s->session->sess_cert == NULL)
				{
				SSLerr(SSL_F_SERVER_HELLO, ERR_R_MALLOC_FAILURE);
				return(-1);
				}
			}
		/* If 'hit' is set, then s->sess_cert may be non-NULL or NULL,
		 * depending on whether it survived in the internal cache
		 * or was retrieved from an external cache.
		 * If it is NULL, we cannot put any useful data in it anyway,
		 * so we don't touch it.
		 */

#else /* That's what used to be done when cert_st and sess_cert_st were
	   * the same. */
		if (!hit)
			{			/* else add cert to session */
			CRYPTO_add(&s->cert->references,1,CRYPTO_LOCK_SSL_CERT);
			if (s->session->sess_cert != NULL)
				ssl_cert_free(s->session->sess_cert);
			s->session->sess_cert=s->cert;		
			}
		else	/* We have a session id-cache hit, if the
			 * session-id has no certificate listed against
			 * the 'cert' structure, grab the 'old' one
			 * listed against the SSL connection */
			{
			if (s->session->sess_cert == NULL)
				{
				CRYPTO_add(&s->cert->references,1,
					CRYPTO_LOCK_SSL_CERT);
				s->session->sess_cert=s->cert;
				}
			}
#endif

		if (s->cert == NULL)
			{
			ssl2_return_error(s,SSL2_PE_NO_CERTIFICATE);
			SSLerr(SSL_F_SERVER_HELLO,SSL_R_NO_CERTIFICATE_SPECIFIED);
			return(-1);
			}

		if (hit)
			{
			*(p++)=0;		/* no certificate type */
			s2n(s->version,p);	/* version */
			s2n(0,p);		/* cert len */
			s2n(0,p);		/* ciphers len */
			}
		else
			{
			/* EAY EAY */
			/* put certificate type */
			*(p++)=SSL2_CT_X509_CERTIFICATE;
			s2n(s->version,p);	/* version */
			n=i2d_X509(s->cert->pkeys[SSL_PKEY_RSA_ENC].x509,NULL);
			s2n(n,p);		/* certificate length */
			i2d_X509(s->cert->pkeys[SSL_PKEY_RSA_ENC].x509,&d);
			n=0;
			
			/* lets send out the ciphers we like in the
			 * prefered order */
			sk= s->session->ciphers;
			n=ssl_cipher_list_to_bytes(s,s->session->ciphers,d,0);
			d+=n;
			s2n(n,p);		/* add cipher length */
			}

		/* make and send conn_id */
		s2n(SSL2_CONNECTION_ID_LENGTH,p);	/* add conn_id length */
		s->s2->conn_id_length=SSL2_CONNECTION_ID_LENGTH;
		if (RAND_pseudo_bytes(s->s2->conn_id,(int)s->s2->conn_id_length) <= 0)
			return -1;
		memcpy(d,s->s2->conn_id,SSL2_CONNECTION_ID_LENGTH);
		d+=SSL2_CONNECTION_ID_LENGTH;

		s->state=SSL2_ST_SEND_SERVER_HELLO_B;
		s->init_num=d-(unsigned char *)s->init_buf->data;
		s->init_off=0;
		}
	/* SSL2_ST_SEND_SERVER_HELLO_B */
 	/* If we are using TCP/IP, the performance is bad if we do 2
 	 * writes without a read between them.  This occurs when
 	 * Session-id reuse is used, so I will put in a buffering module
 	 */
 	if (s->hit)
 		{
		if (!ssl_init_wbio_buffer(s,1)) return(-1);
 		}
 
	return(ssl2_do_write(s));
	}

static int get_client_finished(SSL *s)
	{
	unsigned char *p;
	int i, n;
	unsigned long len;

	p=(unsigned char *)s->init_buf->data;
	if (s->state == SSL2_ST_GET_CLIENT_FINISHED_A)
		{
		i=ssl2_read(s,(char *)&(p[s->init_num]),1-s->init_num);
		if (i < 1-s->init_num)
			return(ssl2_part_read(s,SSL_F_GET_CLIENT_FINISHED,i));
		s->init_num += i;

		if (*p != SSL2_MT_CLIENT_FINISHED)
			{
			if (*p != SSL2_MT_ERROR)
				{
				ssl2_return_error(s,SSL2_PE_UNDEFINED_ERROR);
				SSLerr(SSL_F_GET_CLIENT_FINISHED,SSL_R_READ_WRONG_PACKET_TYPE);
				}
			else
				{
				SSLerr(SSL_F_GET_CLIENT_FINISHED,SSL_R_PEER_ERROR);
				/* try to read the error message */
				i=ssl2_read(s,(char *)&(p[s->init_num]),3-s->init_num);
				return ssl2_part_read(s,SSL_F_GET_SERVER_VERIFY,i);
				}
			return(-1);
			}
		s->state=SSL2_ST_GET_CLIENT_FINISHED_B;
		}

	/* SSL2_ST_GET_CLIENT_FINISHED_B */
	if (s->s2->conn_id_length > sizeof s->s2->conn_id)
		{
		ssl2_return_error(s,SSL2_PE_UNDEFINED_ERROR);
		SSLerr(SSL_F_GET_CLIENT_FINISHED, ERR_R_INTERNAL_ERROR);
		return -1;
		}
	len = 1 + (unsigned long)s->s2->conn_id_length;
	n = (int)len - s->init_num;
	i = ssl2_read(s,(char *)&(p[s->init_num]),n);
	if (i < n)
		{
		return(ssl2_part_read(s,SSL_F_GET_CLIENT_FINISHED,i));
		}
	if (s->msg_callback)
		s->msg_callback(0, s->version, 0, p, len, s, s->msg_callback_arg); /* CLIENT-FINISHED */
	p += 1;
	if (memcmp(p,s->s2->conn_id,s->s2->conn_id_length) != 0)
		{
		ssl2_return_error(s,SSL2_PE_UNDEFINED_ERROR);
		SSLerr(SSL_F_GET_CLIENT_FINISHED,SSL_R_CONNECTION_ID_IS_DIFFERENT);
		return(-1);
		}
	return(1);
	}

static int server_verify(SSL *s)
	{
	unsigned char *p;

	if (s->state == SSL2_ST_SEND_SERVER_VERIFY_A)
		{
		p=(unsigned char *)s->init_buf->data;
		*(p++)=SSL2_MT_SERVER_VERIFY;
		if (s->s2->challenge_length > sizeof s->s2->challenge)
			{
			SSLerr(SSL_F_SERVER_VERIFY, ERR_R_INTERNAL_ERROR);
			return -1;
			}
		memcpy(p,s->s2->challenge,(unsigned int)s->s2->challenge_length);
		/* p+=s->s2->challenge_length; */

		s->state=SSL2_ST_SEND_SERVER_VERIFY_B;
		s->init_num=s->s2->challenge_length+1;
		s->init_off=0;
		}
	return(ssl2_do_write(s));
	}

static int server_finish(SSL *s)
	{
	unsigned char *p;

	if (s->state == SSL2_ST_SEND_SERVER_FINISHED_A)
		{
		p=(unsigned char *)s->init_buf->data;
		*(p++)=SSL2_MT_SERVER_FINISHED;

		if (s->session->session_id_length > sizeof s->session->session_id)
			{
			SSLerr(SSL_F_SERVER_FINISH, ERR_R_INTERNAL_ERROR);
			return -1;
			}
		memcpy(p,s->session->session_id, (unsigned int)s->session->session_id_length);
		/* p+=s->session->session_id_length; */

		s->state=SSL2_ST_SEND_SERVER_FINISHED_B;
		s->init_num=s->session->session_id_length+1;
		s->init_off=0;
		}

	/* SSL2_ST_SEND_SERVER_FINISHED_B */
	return(ssl2_do_write(s));
	}

/* send the request and check the response */
static int request_certificate(SSL *s)
	{
	const unsigned char *cp;
	unsigned char *p,*p2,*buf2;
	unsigned char *ccd;
	int i,j,ctype,ret= -1;
	unsigned long len;
	X509 *x509=NULL;
	STACK_OF(X509) *sk=NULL;

	ccd=s->s2->tmp.ccl;
	if (s->state == SSL2_ST_SEND_REQUEST_CERTIFICATE_A)
		{
		p=(unsigned char *)s->init_buf->data;
		*(p++)=SSL2_MT_REQUEST_CERTIFICATE;
		*(p++)=SSL2_AT_MD5_WITH_RSA_ENCRYPTION;
		if (RAND_pseudo_bytes(ccd,SSL2_MIN_CERT_CHALLENGE_LENGTH) <= 0)
			return -1;
		memcpy(p,ccd,SSL2_MIN_CERT_CHALLENGE_LENGTH);

		s->state=SSL2_ST_SEND_REQUEST_CERTIFICATE_B;
		s->init_num=SSL2_MIN_CERT_CHALLENGE_LENGTH+2;
		s->init_off=0;
		}

	if (s->state == SSL2_ST_SEND_REQUEST_CERTIFICATE_B)
		{
		i=ssl2_do_write(s);
		if (i <= 0)
			{
			ret=i;
			goto end;
			}

		s->init_num=0;
		s->state=SSL2_ST_SEND_REQUEST_CERTIFICATE_C;
		}

	if (s->state == SSL2_ST_SEND_REQUEST_CERTIFICATE_C)
		{
		p=(unsigned char *)s->init_buf->data;
		i=ssl2_read(s,(char *)&(p[s->init_num]),6-s->init_num); /* try to read 6 octets ... */
		if (i < 3-s->init_num) /* ... but don't call ssl2_part_read now if we got at least 3
		                        * (probably NO-CERTIFICATE-ERROR) */
			{
			ret=ssl2_part_read(s,SSL_F_REQUEST_CERTIFICATE,i);
			goto end;
			}
		s->init_num += i;

		if ((s->init_num >= 3) && (p[0] == SSL2_MT_ERROR))
			{
			n2s(p,i);
			if (i != SSL2_PE_NO_CERTIFICATE)
				{
				/* not the error message we expected -- let ssl2_part_read handle it */
				s->init_num -= 3;
				ret = ssl2_part_read(s,SSL_F_REQUEST_CERTIFICATE, 3);
				goto end;
				}

			if (s->msg_callback)
				s->msg_callback(0, s->version, 0, p, 3, s, s->msg_callback_arg); /* ERROR */

			/* this is the one place where we can recover from an SSL 2.0 error */

			if (s->verify_mode & SSL_VERIFY_FAIL_IF_NO_PEER_CERT)
				{
				ssl2_return_error(s,SSL2_PE_BAD_CERTIFICATE);
				SSLerr(SSL_F_REQUEST_CERTIFICATE,SSL_R_PEER_DID_NOT_RETURN_A_CERTIFICATE);
				goto end;
				}
			ret=1;
			goto end;
			}
		if ((*(p++) != SSL2_MT_CLIENT_CERTIFICATE) || (s->init_num < 6))
			{
			ssl2_return_error(s,SSL2_PE_UNDEFINED_ERROR);
			SSLerr(SSL_F_REQUEST_CERTIFICATE,SSL_R_SHORT_READ);
			goto end;
			}
		if (s->init_num != 6)
			{
			SSLerr(SSL_F_REQUEST_CERTIFICATE, ERR_R_INTERNAL_ERROR);
			goto end;
			}
		
		/* ok we have a response */
		/* certificate type, there is only one right now. */
		ctype= *(p++);
		if (ctype != SSL2_AT_MD5_WITH_RSA_ENCRYPTION)
			{
			ssl2_return_error(s,SSL2_PE_UNSUPPORTED_CERTIFICATE_TYPE);
			SSLerr(SSL_F_REQUEST_CERTIFICATE,SSL_R_BAD_RESPONSE_ARGUMENT);
			goto end;
			}
		n2s(p,i); s->s2->tmp.clen=i;
		n2s(p,i); s->s2->tmp.rlen=i;
		s->state=SSL2_ST_SEND_REQUEST_CERTIFICATE_D;
		}

	/* SSL2_ST_SEND_REQUEST_CERTIFICATE_D */
	p=(unsigned char *)s->init_buf->data;
	len = 6 + (unsigned long)s->s2->tmp.clen + (unsigned long)s->s2->tmp.rlen;
	if (len > SSL2_MAX_RECORD_LENGTH_3_BYTE_HEADER)
		{
		SSLerr(SSL_F_REQUEST_CERTIFICATE,SSL_R_MESSAGE_TOO_LONG);
		goto end;
		}
	j = (int)len - s->init_num;
	i = ssl2_read(s,(char *)&(p[s->init_num]),j);
	if (i < j) 
		{
		ret=ssl2_part_read(s,SSL_F_REQUEST_CERTIFICATE,i);
		goto end;
		}
	if (s->msg_callback)
		s->msg_callback(0, s->version, 0, p, len, s, s->msg_callback_arg); /* CLIENT-CERTIFICATE */
	p += 6;

	cp = p;
	x509=(X509 *)d2i_X509(NULL,&cp,(long)s->s2->tmp.clen);
	if (x509 == NULL)
		{
		SSLerr(SSL_F_REQUEST_CERTIFICATE,ERR_R_X509_LIB);
		goto msg_end;
		}

	if (((sk=sk_X509_new_null()) == NULL) || (!sk_X509_push(sk,x509)))
		{
		SSLerr(SSL_F_REQUEST_CERTIFICATE,ERR_R_MALLOC_FAILURE);
		goto msg_end;
		}

	i=ssl_verify_cert_chain(s,sk);

	if (i > 0)	/* we like the packet, now check the chksum */
		{
		EVP_MD_CTX ctx;
		EVP_PKEY *pkey=NULL;

		EVP_MD_CTX_init(&ctx);
		EVP_VerifyInit_ex(&ctx,s->ctx->rsa_md5, NULL);
		EVP_VerifyUpdate(&ctx,s->s2->key_material,
				 s->s2->key_material_length);
		EVP_VerifyUpdate(&ctx,ccd,SSL2_MIN_CERT_CHALLENGE_LENGTH);

		i=i2d_X509(s->cert->pkeys[SSL_PKEY_RSA_ENC].x509,NULL);
		buf2=OPENSSL_malloc((unsigned int)i);
		if (buf2 == NULL)
			{
			SSLerr(SSL_F_REQUEST_CERTIFICATE,ERR_R_MALLOC_FAILURE);
			goto msg_end;
			}
		p2=buf2;
		i=i2d_X509(s->cert->pkeys[SSL_PKEY_RSA_ENC].x509,&p2);
		EVP_VerifyUpdate(&ctx,buf2,(unsigned int)i);
		OPENSSL_free(buf2);

		pkey=X509_get_pubkey(x509);
		if (pkey == NULL) goto end;
		i=EVP_VerifyFinal(&ctx,cp,s->s2->tmp.rlen,pkey);
		EVP_PKEY_free(pkey);
		EVP_MD_CTX_cleanup(&ctx);

		if (i > 0)
			{
			if (s->session->peer != NULL)
				X509_free(s->session->peer);
			s->session->peer=x509;
			CRYPTO_add(&x509->references,1,CRYPTO_LOCK_X509);
			s->session->verify_result = s->verify_result;
			ret=1;
			goto end;
			}
		else
			{
			SSLerr(SSL_F_REQUEST_CERTIFICATE,SSL_R_BAD_CHECKSUM);
			goto msg_end;
			}
		}
	else
		{
msg_end:
		ssl2_return_error(s,SSL2_PE_BAD_CERTIFICATE);
		}
end:
	sk_X509_free(sk);
	X509_free(x509);
	return(ret);
	}

static int ssl_rsa_private_decrypt(CERT *c, int len, unsigned char *from,
	     unsigned char *to, int padding)
	{
	RSA *rsa;
	int i;

	if ((c == NULL) || (c->pkeys[SSL_PKEY_RSA_ENC].privatekey == NULL))
		{
		SSLerr(SSL_F_SSL_RSA_PRIVATE_DECRYPT,SSL_R_NO_PRIVATEKEY);
		return(-1);
		}
	if (c->pkeys[SSL_PKEY_RSA_ENC].privatekey->type != EVP_PKEY_RSA)
		{
		SSLerr(SSL_F_SSL_RSA_PRIVATE_DECRYPT,SSL_R_PUBLIC_KEY_IS_NOT_RSA);
		return(-1);
		}
	rsa=c->pkeys[SSL_PKEY_RSA_ENC].privatekey->pkey.rsa;

	/* we have the public key */
	i=RSA_private_decrypt(len,from,to,rsa,padding);
	if (i < 0)
		SSLerr(SSL_F_SSL_RSA_PRIVATE_DECRYPT,ERR_R_RSA_LIB);
	return(i);
	}
#else /* !OPENSSL_NO_SSL2 */

# if PEDANTIC
static void *dummy=&dummy;
# endif

#endif
