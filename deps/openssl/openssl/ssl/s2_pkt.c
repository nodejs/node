/* ssl/s2_pkt.c */
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
#include <errno.h>
#define USE_SOCKETS

static int read_n(SSL *s,unsigned int n,unsigned int max,unsigned int extend);
static int n_do_ssl_write(SSL *s, const unsigned char *buf, unsigned int len);
static int write_pending(SSL *s, const unsigned char *buf, unsigned int len);
static int ssl_mt_error(int n);


/* SSL 2.0 imlementation for SSL_read/SSL_peek -
 * This routine will return 0 to len bytes, decrypted etc if required.
 */
static int ssl2_read_internal(SSL *s, void *buf, int len, int peek)
	{
	int n;
	unsigned char mac[MAX_MAC_SIZE];
	unsigned char *p;
	int i;
	int mac_size;

 ssl2_read_again:
	if (SSL_in_init(s) && !s->in_handshake)
		{
		n=s->handshake_func(s);
		if (n < 0) return(n);
		if (n == 0)
			{
			SSLerr(SSL_F_SSL2_READ_INTERNAL,SSL_R_SSL_HANDSHAKE_FAILURE);
			return(-1);
			}
		}

	clear_sys_error();
	s->rwstate=SSL_NOTHING;
	if (len <= 0) return(len);

	if (s->s2->ract_data_length != 0) /* read from buffer */
		{
		if (len > s->s2->ract_data_length)
			n=s->s2->ract_data_length;
		else
			n=len;

		memcpy(buf,s->s2->ract_data,(unsigned int)n);
		if (!peek)
			{
			s->s2->ract_data_length-=n;
			s->s2->ract_data+=n;
			if (s->s2->ract_data_length == 0)
				s->rstate=SSL_ST_READ_HEADER;
			}

		return(n);
		}

	/* s->s2->ract_data_length == 0
	 * 
	 * Fill the buffer, then goto ssl2_read_again.
	 */

	if (s->rstate == SSL_ST_READ_HEADER)
		{
		if (s->first_packet)
			{
			n=read_n(s,5,SSL2_MAX_RECORD_LENGTH_2_BYTE_HEADER+2,0);
			if (n <= 0) return(n); /* error or non-blocking */
			s->first_packet=0;
			p=s->packet;
			if (!((p[0] & 0x80) && (
				(p[2] == SSL2_MT_CLIENT_HELLO) ||
				(p[2] == SSL2_MT_SERVER_HELLO))))
				{
				SSLerr(SSL_F_SSL2_READ_INTERNAL,SSL_R_NON_SSLV2_INITIAL_PACKET);
				return(-1);
				}
			}
		else
			{
			n=read_n(s,2,SSL2_MAX_RECORD_LENGTH_2_BYTE_HEADER+2,0);
			if (n <= 0) return(n); /* error or non-blocking */
			}
		/* part read stuff */

		s->rstate=SSL_ST_READ_BODY;
		p=s->packet;
		/* Do header */
		/*s->s2->padding=0;*/
		s->s2->escape=0;
		s->s2->rlength=(((unsigned int)p[0])<<8)|((unsigned int)p[1]);
		if ((p[0] & TWO_BYTE_BIT))		/* Two byte header? */
			{
			s->s2->three_byte_header=0;
			s->s2->rlength&=TWO_BYTE_MASK;	
			}
		else
			{
			s->s2->three_byte_header=1;
			s->s2->rlength&=THREE_BYTE_MASK;

			/* security >s2->escape */
			s->s2->escape=((p[0] & SEC_ESC_BIT))?1:0;
			}
		}

	if (s->rstate == SSL_ST_READ_BODY)
		{
		n=s->s2->rlength+2+s->s2->three_byte_header;
		if (n > (int)s->packet_length)
			{
			n-=s->packet_length;
			i=read_n(s,(unsigned int)n,(unsigned int)n,1);
			if (i <= 0) return(i); /* ERROR */
			}

		p= &(s->packet[2]);
		s->rstate=SSL_ST_READ_HEADER;
		if (s->s2->three_byte_header)
			s->s2->padding= *(p++);
		else	s->s2->padding=0;

		/* Data portion */
		if (s->s2->clear_text)
			{
			mac_size = 0;
			s->s2->mac_data=p;
			s->s2->ract_data=p;
			if (s->s2->padding)
				{
				SSLerr(SSL_F_SSL2_READ_INTERNAL,SSL_R_ILLEGAL_PADDING);
				return(-1);
				}
			}
		else
			{
			mac_size=EVP_MD_CTX_size(s->read_hash);
			if (mac_size < 0)
				return -1;
			OPENSSL_assert(mac_size <= MAX_MAC_SIZE);
			s->s2->mac_data=p;
			s->s2->ract_data= &p[mac_size];
			if (s->s2->padding + mac_size > s->s2->rlength)
				{
				SSLerr(SSL_F_SSL2_READ_INTERNAL,SSL_R_ILLEGAL_PADDING);
				return(-1);
				}
			}

		s->s2->ract_data_length=s->s2->rlength;
		/* added a check for length > max_size in case
		 * encryption was not turned on yet due to an error */
		if ((!s->s2->clear_text) &&
			(s->s2->rlength >= (unsigned int)mac_size))
			{
			ssl2_enc(s,0);
			s->s2->ract_data_length-=mac_size;
			ssl2_mac(s,mac,0);
			s->s2->ract_data_length-=s->s2->padding;
			if (	(memcmp(mac,s->s2->mac_data,
				(unsigned int)mac_size) != 0) ||
				(s->s2->rlength%EVP_CIPHER_CTX_block_size(s->enc_read_ctx) != 0))
				{
				SSLerr(SSL_F_SSL2_READ_INTERNAL,SSL_R_BAD_MAC_DECODE);
				return(-1);
				}
			}
		INC32(s->s2->read_sequence); /* expect next number */
		/* s->s2->ract_data is now available for processing */

		/* Possibly the packet that we just read had 0 actual data bytes.
		 * (SSLeay/OpenSSL itself never sends such packets; see ssl2_write.)
		 * In this case, returning 0 would be interpreted by the caller
		 * as indicating EOF, so it's not a good idea.  Instead, we just
		 * continue reading; thus ssl2_read_internal may have to process
		 * multiple packets before it can return.
		 *
		 * [Note that using select() for blocking sockets *never* guarantees
		 * that the next SSL_read will not block -- the available
		 * data may contain incomplete packets, and except for SSL 2,
		 * renegotiation can confuse things even more.] */

		goto ssl2_read_again; /* This should really be
		                       * "return ssl2_read(s,buf,len)",
		                       * but that would allow for
		                       * denial-of-service attacks if a
		                       * C compiler is used that does not
		                       * recognize end-recursion. */
		}
	else
		{
		SSLerr(SSL_F_SSL2_READ_INTERNAL,SSL_R_BAD_STATE);
			return(-1);
		}
	}

int ssl2_read(SSL *s, void *buf, int len)
	{
	return ssl2_read_internal(s, buf, len, 0);
	}

int ssl2_peek(SSL *s, void *buf, int len)
	{
	return ssl2_read_internal(s, buf, len, 1);
	}

static int read_n(SSL *s, unsigned int n, unsigned int max,
	     unsigned int extend)
	{
	int i,off,newb;

	/* if there is stuff still in the buffer from a previous read,
	 * and there is more than we want, take some. */
	if (s->s2->rbuf_left >= (int)n)
		{
		if (extend)
			s->packet_length+=n;
		else
			{
			s->packet= &(s->s2->rbuf[s->s2->rbuf_offs]);
			s->packet_length=n;
			}
		s->s2->rbuf_left-=n;
		s->s2->rbuf_offs+=n;
		return(n);
		}

	if (!s->read_ahead) max=n;
	if (max > (unsigned int)(SSL2_MAX_RECORD_LENGTH_2_BYTE_HEADER+2))
		max=SSL2_MAX_RECORD_LENGTH_2_BYTE_HEADER+2;
	

	/* Else we want more than we have.
	 * First, if there is some left or we want to extend */
	off=0;
	if ((s->s2->rbuf_left != 0) || ((s->packet_length != 0) && extend))
		{
		newb=s->s2->rbuf_left;
		if (extend)
			{
			off=s->packet_length;
			if (s->packet != s->s2->rbuf)
				memcpy(s->s2->rbuf,s->packet,
					(unsigned int)newb+off);
			}
		else if (s->s2->rbuf_offs != 0)
			{
			memcpy(s->s2->rbuf,&(s->s2->rbuf[s->s2->rbuf_offs]),
				(unsigned int)newb);
			s->s2->rbuf_offs=0;
			}
		s->s2->rbuf_left=0;
		}
	else
		newb=0;

	/* off is the offset to start writing too.
	 * r->s2->rbuf_offs is the 'unread data', now 0. 
	 * newb is the number of new bytes so far
	 */
	s->packet=s->s2->rbuf;
	while (newb < (int)n)
		{
		clear_sys_error();
		if (s->rbio != NULL)
			{
			s->rwstate=SSL_READING;
			i=BIO_read(s->rbio,(char *)&(s->s2->rbuf[off+newb]),
				max-newb);
			}
		else
			{
			SSLerr(SSL_F_READ_N,SSL_R_READ_BIO_NOT_SET);
			i= -1;
			}
#ifdef PKT_DEBUG
		if (s->debug & 0x01) sleep(1);
#endif
		if (i <= 0)
			{
			s->s2->rbuf_left+=newb;
			return(i);
			}
		newb+=i;
		}

	/* record unread data */
	if (newb > (int)n)
		{
		s->s2->rbuf_offs=n+off;
		s->s2->rbuf_left=newb-n;
		}
	else
		{
		s->s2->rbuf_offs=0;
		s->s2->rbuf_left=0;
		}
	if (extend)
		s->packet_length+=n;
	else
		s->packet_length=n;
	s->rwstate=SSL_NOTHING;
	return(n);
	}

int ssl2_write(SSL *s, const void *_buf, int len)
	{
	const unsigned char *buf=_buf;
	unsigned int n,tot;
	int i;

	if (SSL_in_init(s) && !s->in_handshake)
		{
		i=s->handshake_func(s);
		if (i < 0) return(i);
		if (i == 0)
			{
			SSLerr(SSL_F_SSL2_WRITE,SSL_R_SSL_HANDSHAKE_FAILURE);
			return(-1);
			}
		}

	if (s->error)
		{
		ssl2_write_error(s);
		if (s->error)
			return(-1);
		}

	clear_sys_error();
	s->rwstate=SSL_NOTHING;
	if (len <= 0) return(len);

	tot=s->s2->wnum;
	s->s2->wnum=0;

	n=(len-tot);
	for (;;)
		{
		i=n_do_ssl_write(s,&(buf[tot]),n);
		if (i <= 0)
			{
			s->s2->wnum=tot;
			return(i);
			}
		if ((i == (int)n) ||
			(s->mode & SSL_MODE_ENABLE_PARTIAL_WRITE))
			{
			return(tot+i);
			}
		
		n-=i;
		tot+=i;
		}
	}

static int write_pending(SSL *s, const unsigned char *buf, unsigned int len)
	{
	int i;

	/* s->s2->wpend_len != 0 MUST be true. */

	/* check that they have given us the same buffer to
	 * write */
	if ((s->s2->wpend_tot > (int)len) ||
		((s->s2->wpend_buf != buf) &&
		 !(s->mode & SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER)))
		{
		SSLerr(SSL_F_WRITE_PENDING,SSL_R_BAD_WRITE_RETRY);
		return(-1);
		}

	for (;;)
		{
		clear_sys_error();
		if (s->wbio != NULL)
			{
			s->rwstate=SSL_WRITING;
			i=BIO_write(s->wbio,
				(char *)&(s->s2->write_ptr[s->s2->wpend_off]),
				(unsigned int)s->s2->wpend_len);
			}
		else
			{
			SSLerr(SSL_F_WRITE_PENDING,SSL_R_WRITE_BIO_NOT_SET);
			i= -1;
			}
#ifdef PKT_DEBUG
		if (s->debug & 0x01) sleep(1);
#endif
		if (i == s->s2->wpend_len)
			{
			s->s2->wpend_len=0;
			s->rwstate=SSL_NOTHING;
			return(s->s2->wpend_ret);
			}
		else if (i <= 0)
			return(i);
		s->s2->wpend_off+=i;
		s->s2->wpend_len-=i;
		}
	}

static int n_do_ssl_write(SSL *s, const unsigned char *buf, unsigned int len)
	{
	unsigned int j,k,olen,p,bs;
	int mac_size;
	register unsigned char *pp;

	olen=len;

	/* first check if there is data from an encryption waiting to
	 * be sent - it must be sent because the other end is waiting.
	 * This will happen with non-blocking IO.  We print it and then
	 * return.
	 */
	if (s->s2->wpend_len != 0) return(write_pending(s,buf,len));

	/* set mac_size to mac size */
	if (s->s2->clear_text)
		mac_size=0;
	else
		{
		mac_size=EVP_MD_CTX_size(s->write_hash);
		if (mac_size < 0)
			return -1;
		}

	/* lets set the pad p */
	if (s->s2->clear_text)
		{
		if (len > SSL2_MAX_RECORD_LENGTH_2_BYTE_HEADER)
			len=SSL2_MAX_RECORD_LENGTH_2_BYTE_HEADER;
		p=0;
		s->s2->three_byte_header=0;
		/* len=len; */
		}
	else
		{
		bs=EVP_CIPHER_CTX_block_size(s->enc_read_ctx);
		j=len+mac_size;
		/* Two-byte headers allow for a larger record length than
		 * three-byte headers, but we can't use them if we need
		 * padding or if we have to set the escape bit. */
		if ((j > SSL2_MAX_RECORD_LENGTH_3_BYTE_HEADER) &&
			(!s->s2->escape))
			{
			if (j > SSL2_MAX_RECORD_LENGTH_2_BYTE_HEADER)
				j=SSL2_MAX_RECORD_LENGTH_2_BYTE_HEADER;
			/* set k to the max number of bytes with 2
			 * byte header */
			k=j-(j%bs);
			/* how many data bytes? */
			len=k-mac_size; 
			s->s2->three_byte_header=0;
			p=0;
			}
		else if ((bs <= 1) && (!s->s2->escape))
			{
			/* j <= SSL2_MAX_RECORD_LENGTH_3_BYTE_HEADER, thus
			 * j < SSL2_MAX_RECORD_LENGTH_2_BYTE_HEADER */
			s->s2->three_byte_header=0;
			p=0;
			}
		else /* we may have to use a 3 byte header */
			{
			/* If s->s2->escape is not set, then
			 * j <= SSL2_MAX_RECORD_LENGTH_3_BYTE_HEADER, and thus
			 * j < SSL2_MAX_RECORD_LENGTH_2_BYTE_HEADER. */
			p=(j%bs);
			p=(p == 0)?0:(bs-p);
			if (s->s2->escape)
				{
				s->s2->three_byte_header=1;
				if (j > SSL2_MAX_RECORD_LENGTH_3_BYTE_HEADER)
					j=SSL2_MAX_RECORD_LENGTH_3_BYTE_HEADER;
				}
			else
				s->s2->three_byte_header=(p == 0)?0:1;
			}
		}

	/* Now
	 *      j <= SSL2_MAX_RECORD_LENGTH_2_BYTE_HEADER
	 * holds, and if s->s2->three_byte_header is set, then even
	 *      j <= SSL2_MAX_RECORD_LENGTH_3_BYTE_HEADER.
	 */

	/* mac_size is the number of MAC bytes
	 * len is the number of data bytes we are going to send
	 * p is the number of padding bytes
	 * (if it is a two-byte header, then p == 0) */

	s->s2->wlength=len;
	s->s2->padding=p;
	s->s2->mac_data= &(s->s2->wbuf[3]);
	s->s2->wact_data= &(s->s2->wbuf[3+mac_size]);
	/* we copy the data into s->s2->wbuf */
	memcpy(s->s2->wact_data,buf,len);
	if (p)
		memset(&(s->s2->wact_data[len]),0,p); /* arbitrary padding */

	if (!s->s2->clear_text)
		{
		s->s2->wact_data_length=len+p;
		ssl2_mac(s,s->s2->mac_data,1);
		s->s2->wlength+=p+mac_size;
		ssl2_enc(s,1);
		}

	/* package up the header */
	s->s2->wpend_len=s->s2->wlength;
	if (s->s2->three_byte_header) /* 3 byte header */
		{
		pp=s->s2->mac_data;
		pp-=3;
		pp[0]=(s->s2->wlength>>8)&(THREE_BYTE_MASK>>8);
		if (s->s2->escape) pp[0]|=SEC_ESC_BIT;
		pp[1]=s->s2->wlength&0xff;
		pp[2]=s->s2->padding;
		s->s2->wpend_len+=3;
		}
	else
		{
		pp=s->s2->mac_data;
		pp-=2;
		pp[0]=((s->s2->wlength>>8)&(TWO_BYTE_MASK>>8))|TWO_BYTE_BIT;
		pp[1]=s->s2->wlength&0xff;
		s->s2->wpend_len+=2;
		}
	s->s2->write_ptr=pp;
	
	INC32(s->s2->write_sequence); /* expect next number */

	/* lets try to actually write the data */
	s->s2->wpend_tot=olen;
	s->s2->wpend_buf=buf;

	s->s2->wpend_ret=len;

	s->s2->wpend_off=0;
	return(write_pending(s,buf,olen));
	}

int ssl2_part_read(SSL *s, unsigned long f, int i)
	{
	unsigned char *p;
	int j;

	if (i < 0)
		{
		/* ssl2_return_error(s); */
		/* for non-blocking io,
		 * this is not necessarily fatal */
		return(i);
		}
	else
		{
		s->init_num+=i;

		/* Check for error.  While there are recoverable errors,
		 * this function is not called when those must be expected;
		 * any error detected here is fatal. */
		if (s->init_num >= 3)
			{
			p=(unsigned char *)s->init_buf->data;
			if (p[0] == SSL2_MT_ERROR)
				{
				j=(p[1]<<8)|p[2];
				SSLerr((int)f,ssl_mt_error(j));
				s->init_num -= 3;
				if (s->init_num > 0)
					memmove(p, p+3, s->init_num);
				}
			}

		/* If it's not an error message, we have some error anyway --
		 * the message was shorter than expected.  This too is treated
		 * as fatal (at least if SSL_get_error is asked for its opinion). */
		return(0);
		}
	}

int ssl2_do_write(SSL *s)
	{
	int ret;

	ret=ssl2_write(s,&s->init_buf->data[s->init_off],s->init_num);
	if (ret == s->init_num)
		{
		if (s->msg_callback)
			s->msg_callback(1, s->version, 0, s->init_buf->data, (size_t)(s->init_off + s->init_num), s, s->msg_callback_arg);
		return(1);
		}
	if (ret < 0)
		return(-1);
	s->init_off+=ret;
	s->init_num-=ret;
	return(0);
	}

static int ssl_mt_error(int n)
	{
	int ret;

	switch (n)
		{
	case SSL2_PE_NO_CIPHER:
		ret=SSL_R_PEER_ERROR_NO_CIPHER;
		break;
	case SSL2_PE_NO_CERTIFICATE:
		ret=SSL_R_PEER_ERROR_NO_CERTIFICATE;
		break;
	case SSL2_PE_BAD_CERTIFICATE:
		ret=SSL_R_PEER_ERROR_CERTIFICATE;
		break;
	case SSL2_PE_UNSUPPORTED_CERTIFICATE_TYPE:
		ret=SSL_R_PEER_ERROR_UNSUPPORTED_CERTIFICATE_TYPE;
		break;
	default:
		ret=SSL_R_UNKNOWN_REMOTE_ERROR_TYPE;
		break;
		}
	return(ret);
	}
#else /* !OPENSSL_NO_SSL2 */

# if PEDANTIC
static void *dummy=&dummy;
# endif

#endif
