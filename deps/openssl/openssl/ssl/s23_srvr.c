/* ssl/s23_srvr.c */
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

#include <stdio.h>
#include "ssl_locl.h"
#include <openssl/buffer.h>
#include <openssl/rand.h>
#include <openssl/objects.h>
#include <openssl/evp.h>

static SSL_METHOD *ssl23_get_server_method(int ver);
int ssl23_get_client_hello(SSL *s);
static SSL_METHOD *ssl23_get_server_method(int ver)
	{
#ifndef OPENSSL_NO_SSL2
	if (ver == SSL2_VERSION)
		return(SSLv2_server_method());
#endif
	if (ver == SSL3_VERSION)
		return(SSLv3_server_method());
	else if (ver == TLS1_VERSION)
		return(TLSv1_server_method());
	else
		return(NULL);
	}

IMPLEMENT_ssl23_meth_func(SSLv23_server_method,
			ssl23_accept,
			ssl_undefined_function,
			ssl23_get_server_method)

int ssl23_accept(SSL *s)
	{
	BUF_MEM *buf;
	unsigned long Time=(unsigned long)time(NULL);
	void (*cb)(const SSL *ssl,int type,int val)=NULL;
	int ret= -1;
	int new_state,state;

	RAND_add(&Time,sizeof(Time),0);
	ERR_clear_error();
	clear_sys_error();

	if (s->info_callback != NULL)
		cb=s->info_callback;
	else if (s->ctx->info_callback != NULL)
		cb=s->ctx->info_callback;
	
	s->in_handshake++;
	if (!SSL_in_init(s) || SSL_in_before(s)) SSL_clear(s); 

	for (;;)
		{
		state=s->state;

		switch(s->state)
			{
		case SSL_ST_BEFORE:
		case SSL_ST_ACCEPT:
		case SSL_ST_BEFORE|SSL_ST_ACCEPT:
		case SSL_ST_OK|SSL_ST_ACCEPT:

			s->server=1;
			if (cb != NULL) cb(s,SSL_CB_HANDSHAKE_START,1);

			/* s->version=SSL3_VERSION; */
			s->type=SSL_ST_ACCEPT;

			if (s->init_buf == NULL)
				{
				if ((buf=BUF_MEM_new()) == NULL)
					{
					ret= -1;
					goto end;
					}
				if (!BUF_MEM_grow(buf,SSL3_RT_MAX_PLAIN_LENGTH))
					{
					ret= -1;
					goto end;
					}
				s->init_buf=buf;
				}

			ssl3_init_finished_mac(s);

			s->state=SSL23_ST_SR_CLNT_HELLO_A;
			s->ctx->stats.sess_accept++;
			s->init_num=0;
			break;

		case SSL23_ST_SR_CLNT_HELLO_A:
		case SSL23_ST_SR_CLNT_HELLO_B:

			s->shutdown=0;
			ret=ssl23_get_client_hello(s);
			if (ret >= 0) cb=NULL;
			goto end;
			/* break; */

		default:
			SSLerr(SSL_F_SSL23_ACCEPT,SSL_R_UNKNOWN_STATE);
			ret= -1;
			goto end;
			/* break; */
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


int ssl23_get_client_hello(SSL *s)
	{
	char buf_space[11]; /* Request this many bytes in initial read.
	                     * We can detect SSL 3.0/TLS 1.0 Client Hellos
	                     * ('type == 3') correctly only when the following
	                     * is in a single record, which is not guaranteed by
	                     * the protocol specification:
	                     * Byte  Content
	                     *  0     type            \
	                     *  1/2   version          > record header
	                     *  3/4   length          /
	                     *  5     msg_type        \
	                     *  6-8   length           > Client Hello message
	                     *  9/10  client_version  /
	                     */
	char *buf= &(buf_space[0]);
	unsigned char *p,*d,*d_len,*dd;
	unsigned int i;
	unsigned int csl,sil,cl;
	int n=0,j;
	int type=0;
	int v[2];

	if (s->state ==	SSL23_ST_SR_CLNT_HELLO_A)
		{
		/* read the initial header */
		v[0]=v[1]=0;

		if (!ssl3_setup_buffers(s)) goto err;

		n=ssl23_read_bytes(s, sizeof buf_space);
		if (n != sizeof buf_space) return(n); /* n == -1 || n == 0 */

		p=s->packet;

		memcpy(buf,p,n);

		if ((p[0] & 0x80) && (p[2] == SSL2_MT_CLIENT_HELLO))
			{
			/*
			 * SSLv2 header
			 */
			if ((p[3] == 0x00) && (p[4] == 0x02))
				{
				v[0]=p[3]; v[1]=p[4];
				/* SSLv2 */
				if (!(s->options & SSL_OP_NO_SSLv2))
					type=1;
				}
			else if (p[3] == SSL3_VERSION_MAJOR)
				{
				v[0]=p[3]; v[1]=p[4];
				/* SSLv3/TLSv1 */
				if (p[4] >= TLS1_VERSION_MINOR)
					{
					if (!(s->options & SSL_OP_NO_TLSv1))
						{
						s->version=TLS1_VERSION;
						/* type=2; */ /* done later to survive restarts */
						s->state=SSL23_ST_SR_CLNT_HELLO_B;
						}
					else if (!(s->options & SSL_OP_NO_SSLv3))
						{
						s->version=SSL3_VERSION;
						/* type=2; */
						s->state=SSL23_ST_SR_CLNT_HELLO_B;
						}
					else if (!(s->options & SSL_OP_NO_SSLv2))
						{
						type=1;
						}
					}
				else if (!(s->options & SSL_OP_NO_SSLv3))
					{
					s->version=SSL3_VERSION;
					/* type=2; */
					s->state=SSL23_ST_SR_CLNT_HELLO_B;
					}
				else if (!(s->options & SSL_OP_NO_SSLv2))
					type=1;

				}
			}
		else if ((p[0] == SSL3_RT_HANDSHAKE) &&
			 (p[1] == SSL3_VERSION_MAJOR) &&
			 (p[5] == SSL3_MT_CLIENT_HELLO) &&
			 ((p[3] == 0 && p[4] < 5 /* silly record length? */)
				|| (p[9] >= p[1])))
			{
			/*
			 * SSLv3 or tls1 header
			 */
			
			v[0]=p[1]; /* major version (= SSL3_VERSION_MAJOR) */
			/* We must look at client_version inside the Client Hello message
			 * to get the correct minor version.
			 * However if we have only a pathologically small fragment of the
			 * Client Hello message, this would be difficult, and we'd have
			 * to read more records to find out.
			 * No known SSL 3.0 client fragments ClientHello like this,
			 * so we simply assume TLS 1.0 to avoid protocol version downgrade
			 * attacks. */
			if (p[3] == 0 && p[4] < 6)
				{
#if 0
				SSLerr(SSL_F_SSL23_GET_CLIENT_HELLO,SSL_R_RECORD_TOO_SMALL);
				goto err;
#else
				v[1] = TLS1_VERSION_MINOR;
#endif
				}
			/* if major version number > 3 set minor to a value
			 * which will use the highest version 3 we support.
			 * If TLS 2.0 ever appears we will need to revise
			 * this....
			 */
			else if (p[9] > SSL3_VERSION_MAJOR)
				v[1]=0xff;
			else
				v[1]=p[10]; /* minor version according to client_version */
			if (v[1] >= TLS1_VERSION_MINOR)
				{
				if (!(s->options & SSL_OP_NO_TLSv1))
					{
					s->version=TLS1_VERSION;
					type=3;
					}
				else if (!(s->options & SSL_OP_NO_SSLv3))
					{
					s->version=SSL3_VERSION;
					type=3;
					}
				}
			else
				{
				/* client requests SSL 3.0 */
				if (!(s->options & SSL_OP_NO_SSLv3))
					{
					s->version=SSL3_VERSION;
					type=3;
					}
				else if (!(s->options & SSL_OP_NO_TLSv1))
					{
					/* we won't be able to use TLS of course,
					 * but this will send an appropriate alert */
					s->version=TLS1_VERSION;
					type=3;
					}
				}
			}
		else if ((strncmp("GET ", (char *)p,4) == 0) ||
			 (strncmp("POST ",(char *)p,5) == 0) ||
			 (strncmp("HEAD ",(char *)p,5) == 0) ||
			 (strncmp("PUT ", (char *)p,4) == 0))
			{
			SSLerr(SSL_F_SSL23_GET_CLIENT_HELLO,SSL_R_HTTP_REQUEST);
			goto err;
			}
		else if (strncmp("CONNECT",(char *)p,7) == 0)
			{
			SSLerr(SSL_F_SSL23_GET_CLIENT_HELLO,SSL_R_HTTPS_PROXY_REQUEST);
			goto err;
			}
		}

#ifdef OPENSSL_FIPS
	if (FIPS_mode() && (s->version < TLS1_VERSION))
		{
		SSLerr(SSL_F_SSL23_GET_CLIENT_HELLO,
					SSL_R_ONLY_TLS_ALLOWED_IN_FIPS_MODE);
		goto err;
		}
#endif

	if (s->state == SSL23_ST_SR_CLNT_HELLO_B)
		{
		/* we have SSLv3/TLSv1 in an SSLv2 header
		 * (other cases skip this state) */

		type=2;
		p=s->packet;
		v[0] = p[3]; /* == SSL3_VERSION_MAJOR */
		v[1] = p[4];

		n=((p[0]&0x7f)<<8)|p[1];
		if (n > (1024*4))
			{
			SSLerr(SSL_F_SSL23_GET_CLIENT_HELLO,SSL_R_RECORD_TOO_LARGE);
			goto err;
			}

		j=ssl23_read_bytes(s,n+2);
		if (j <= 0) return(j);

		ssl3_finish_mac(s, s->packet+2, s->packet_length-2);
		if (s->msg_callback)
			s->msg_callback(0, SSL2_VERSION, 0, s->packet+2, s->packet_length-2, s, s->msg_callback_arg); /* CLIENT-HELLO */

		p=s->packet;
		p+=5;
		n2s(p,csl);
		n2s(p,sil);
		n2s(p,cl);
		d=(unsigned char *)s->init_buf->data;
		if ((csl+sil+cl+11) != s->packet_length)
			{
			SSLerr(SSL_F_SSL23_GET_CLIENT_HELLO,SSL_R_RECORD_LENGTH_MISMATCH);
			goto err;
			}

		/* record header: msg_type ... */
		*(d++) = SSL3_MT_CLIENT_HELLO;
		/* ... and length (actual value will be written later) */
		d_len = d;
		d += 3;

		/* client_version */
		*(d++) = SSL3_VERSION_MAJOR; /* == v[0] */
		*(d++) = v[1];

		/* lets populate the random area */
		/* get the challenge_length */
		i=(cl > SSL3_RANDOM_SIZE)?SSL3_RANDOM_SIZE:cl;
		memset(d,0,SSL3_RANDOM_SIZE);
		memcpy(&(d[SSL3_RANDOM_SIZE-i]),&(p[csl+sil]),i);
		d+=SSL3_RANDOM_SIZE;

		/* no session-id reuse */
		*(d++)=0;

		/* ciphers */
		j=0;
		dd=d;
		d+=2;
		for (i=0; i<csl; i+=3)
			{
			if (p[i] != 0) continue;
			*(d++)=p[i+1];
			*(d++)=p[i+2];
			j+=2;
			}
		s2n(j,dd);

		/* COMPRESSION */
		*(d++)=1;
		*(d++)=0;
		
		i = (d-(unsigned char *)s->init_buf->data) - 4;
		l2n3((long)i, d_len);

		/* get the data reused from the init_buf */
		s->s3->tmp.reuse_message=1;
		s->s3->tmp.message_type=SSL3_MT_CLIENT_HELLO;
		s->s3->tmp.message_size=i;
		}

	/* imaginary new state (for program structure): */
	/* s->state = SSL23_SR_CLNT_HELLO_C */

	if (type == 1)
		{
#ifdef OPENSSL_NO_SSL2
		SSLerr(SSL_F_SSL23_GET_CLIENT_HELLO,SSL_R_UNSUPPORTED_PROTOCOL);
		goto err;
#else
		/* we are talking sslv2 */
		/* we need to clean up the SSLv3/TLSv1 setup and put in the
		 * sslv2 stuff. */

		if (s->s2 == NULL)
			{
			if (!ssl2_new(s))
				goto err;
			}
		else
			ssl2_clear(s);

		if (s->s3 != NULL) ssl3_free(s);

		if (!BUF_MEM_grow_clean(s->init_buf,
			SSL2_MAX_RECORD_LENGTH_3_BYTE_HEADER))
			{
			goto err;
			}

		s->state=SSL2_ST_GET_CLIENT_HELLO_A;
		if (s->options & SSL_OP_NO_TLSv1 && s->options & SSL_OP_NO_SSLv3)
			s->s2->ssl2_rollback=0;
		else
			/* reject SSL 2.0 session if client supports SSL 3.0 or TLS 1.0
			 * (SSL 3.0 draft/RFC 2246, App. E.2) */
			s->s2->ssl2_rollback=1;

		/* setup the n bytes we have read so we get them from
		 * the sslv2 buffer */
		s->rstate=SSL_ST_READ_HEADER;
		s->packet_length=n;
		s->packet= &(s->s2->rbuf[0]);
		memcpy(s->packet,buf,n);
		s->s2->rbuf_left=n;
		s->s2->rbuf_offs=0;

		s->method=SSLv2_server_method();
		s->handshake_func=s->method->ssl_accept;
#endif
		}

	if ((type == 2) || (type == 3))
		{
		/* we have SSLv3/TLSv1 (type 2: SSL2 style, type 3: SSL3/TLS style) */

		if (!ssl_init_wbio_buffer(s,1)) goto err;

		/* we are in this state */
		s->state=SSL3_ST_SR_CLNT_HELLO_A;

		if (type == 3)
			{
			/* put the 'n' bytes we have read into the input buffer
			 * for SSLv3 */
			s->rstate=SSL_ST_READ_HEADER;
			s->packet_length=n;
			s->packet= &(s->s3->rbuf.buf[0]);
			memcpy(s->packet,buf,n);
			s->s3->rbuf.left=n;
			s->s3->rbuf.offset=0;
			}
		else
			{
			s->packet_length=0;
			s->s3->rbuf.left=0;
			s->s3->rbuf.offset=0;
			}

		if (s->version == TLS1_VERSION)
			s->method = TLSv1_server_method();
		else
			s->method = SSLv3_server_method();
#if 0 /* ssl3_get_client_hello does this */
		s->client_version=(v[0]<<8)|v[1];
#endif
		s->handshake_func=s->method->ssl_accept;
		}
	
	if ((type < 1) || (type > 3))
		{
		/* bad, very bad */
		SSLerr(SSL_F_SSL23_GET_CLIENT_HELLO,SSL_R_UNKNOWN_PROTOCOL);
		goto err;
		}
	s->init_num=0;

	if (buf != buf_space) OPENSSL_free(buf);
	return(SSL_accept(s));
err:
	if (buf != buf_space) OPENSSL_free(buf);
	return(-1);
	}
