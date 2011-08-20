/* ssl/t1_lib.c */
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
#include <openssl/objects.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/ocsp.h>
#include "ssl_locl.h"
#include "fnv1a64.h"

const char tls1_version_str[]="TLSv1" OPENSSL_VERSION_PTEXT;

#ifndef OPENSSL_NO_TLSEXT
static int tls_decrypt_ticket(SSL *s, const unsigned char *tick, int ticklen,
				const unsigned char *sess_id, int sesslen,
				SSL_SESSION **psess);
#endif

SSL3_ENC_METHOD TLSv1_enc_data={
	tls1_enc,
	tls1_mac,
	tls1_setup_key_block,
	tls1_generate_master_secret,
	tls1_change_cipher_state,
	tls1_final_finish_mac,
	TLS1_FINISH_MAC_LENGTH,
	tls1_cert_verify_mac,
	TLS_MD_CLIENT_FINISH_CONST,TLS_MD_CLIENT_FINISH_CONST_SIZE,
	TLS_MD_SERVER_FINISH_CONST,TLS_MD_SERVER_FINISH_CONST_SIZE,
	tls1_alert_code,
	};

long tls1_default_timeout(void)
	{
	/* 2 hours, the 24 hours mentioned in the TLSv1 spec
	 * is way too long for http, the cache would over fill */
	return(60*60*2);
	}

IMPLEMENT_tls1_meth_func(tlsv1_base_method,
			ssl_undefined_function,
			ssl_undefined_function,
			ssl_bad_method)

int tls1_new(SSL *s)
	{
	if (!ssl3_new(s)) return(0);
	s->method->ssl_clear(s);
	return(1);
	}

void tls1_free(SSL *s)
	{
	ssl3_free(s);
	}

void tls1_clear(SSL *s)
	{
	ssl3_clear(s);
	s->version=TLS1_VERSION;
	}

#if 0
long tls1_ctrl(SSL *s, int cmd, long larg, char *parg)
	{
	return(0);
	}

long tls1_callback_ctrl(SSL *s, int cmd, void *(*fp)())
	{
	return(0);
	}
#endif

#ifndef OPENSSL_NO_TLSEXT
unsigned char *ssl_add_clienthello_tlsext(SSL *s, unsigned char *p, unsigned char *limit)
	{
	int extdatalen=0;
	unsigned char *ret = p;

	/* don't add extensions for SSLv3 unless doing secure renegotiation */
	if (s->client_version == SSL3_VERSION
					&& !s->s3->send_connection_binding)
		return p;

	ret+=2;

	if (ret>=limit) return NULL; /* this really never occurs, but ... */

 	if (s->tlsext_hostname != NULL)
		{ 
		/* Add TLS extension servername to the Client Hello message */
		unsigned long size_str;
		long lenmax; 

		/* check for enough space.
		   4 for the servername type and entension length
		   2 for servernamelist length
		   1 for the hostname type
		   2 for hostname length
		   + hostname length 
		*/
		   
		if ((lenmax = limit - ret - 9) < 0 
		|| (size_str = strlen(s->tlsext_hostname)) > (unsigned long)lenmax) 
			return NULL;
			
		/* extension type and length */
		s2n(TLSEXT_TYPE_server_name,ret); 
		s2n(size_str+5,ret);
		
		/* length of servername list */
		s2n(size_str+3,ret);
	
		/* hostname type, length and hostname */
		*(ret++) = (unsigned char) TLSEXT_NAMETYPE_host_name;
		s2n(size_str,ret);
		memcpy(ret, s->tlsext_hostname, size_str);
		ret+=size_str;

		}
 
        /* Add RI if renegotiating */
        if (s->new_session)
          {
          int el;
          
          if(!ssl_add_clienthello_renegotiate_ext(s, 0, &el, 0))
              {
              SSLerr(SSL_F_SSL_ADD_CLIENTHELLO_TLSEXT, ERR_R_INTERNAL_ERROR);
              return NULL;
              }

          if((limit - p - 4 - el) < 0) return NULL;
          
          s2n(TLSEXT_TYPE_renegotiate,ret);
          s2n(el,ret);

          if(!ssl_add_clienthello_renegotiate_ext(s, ret, &el, el))
              {
              SSLerr(SSL_F_SSL_ADD_CLIENTHELLO_TLSEXT, ERR_R_INTERNAL_ERROR);
              return NULL;
              }

          ret += el;
        }

           
	if (!(SSL_get_options(s) & SSL_OP_NO_TICKET))
		{
		int ticklen;
		if (!s->new_session && s->session && s->session->tlsext_tick)
			ticklen = s->session->tlsext_ticklen;
		else
			ticklen = 0;
		/* Check for enough room 2 for extension type, 2 for len
 		 * rest for ticket
  		 */
		if (limit - ret - 4 - ticklen < 0)
			return NULL;
		s2n(TLSEXT_TYPE_session_ticket,ret); 
		s2n(ticklen,ret);
		if (ticklen)
			{
			memcpy(ret, s->session->tlsext_tick, ticklen);
			ret += ticklen;
			}
		}

	if (s->tlsext_status_type == TLSEXT_STATUSTYPE_ocsp &&
	    s->version != DTLS1_VERSION)
		{
		int i;
		long extlen, idlen, itmp;
		OCSP_RESPID *id;

		idlen = 0;
		for (i = 0; i < sk_OCSP_RESPID_num(s->tlsext_ocsp_ids); i++)
			{
			id = sk_OCSP_RESPID_value(s->tlsext_ocsp_ids, i);
			itmp = i2d_OCSP_RESPID(id, NULL);
			if (itmp <= 0)
				return NULL;
			idlen += itmp + 2;
			}

		if (s->tlsext_ocsp_exts)
			{
			extlen = i2d_X509_EXTENSIONS(s->tlsext_ocsp_exts, NULL);
			if (extlen < 0)
				return NULL;
			}
		else
			extlen = 0;
			
		if ((long)(limit - ret - 7 - extlen - idlen) < 0) return NULL;
		s2n(TLSEXT_TYPE_status_request, ret);
		if (extlen + idlen > 0xFFF0)
			return NULL;
		s2n(extlen + idlen + 5, ret);
		*(ret++) = TLSEXT_STATUSTYPE_ocsp;
		s2n(idlen, ret);
		for (i = 0; i < sk_OCSP_RESPID_num(s->tlsext_ocsp_ids); i++)
			{
			/* save position of id len */
			unsigned char *q = ret;
			id = sk_OCSP_RESPID_value(s->tlsext_ocsp_ids, i);
			/* skip over id len */
			ret += 2;
			itmp = i2d_OCSP_RESPID(id, &ret);
			/* write id len */
			s2n(itmp, q);
			}
		s2n(extlen, ret);
		if (extlen > 0)
			i2d_X509_EXTENSIONS(s->tlsext_ocsp_exts, &ret);
		}

	if (s->ctx->next_proto_select_cb)
		{
		/* The client advertises an emtpy extension to indicate its
		 * support for Next Protocol Negotiation */
		if (limit - ret - 4 < 0)
			return NULL;
		s2n(TLSEXT_TYPE_next_proto_neg,ret);
		s2n(0,ret);
		}

	if ((extdatalen = ret-p-2)== 0) 
		return p;

	s2n(extdatalen,p);
	return ret;
	}

unsigned char *ssl_add_serverhello_tlsext(SSL *s, unsigned char *p, unsigned char *limit)
	{
	int extdatalen=0;
	unsigned char *ret = p;
	char next_proto_neg_seen;

	/* don't add extensions for SSLv3, unless doing secure renegotiation */
	if (s->version == SSL3_VERSION && !s->s3->send_connection_binding)
		return p;
	
	ret+=2;
	if (ret>=limit) return NULL; /* this really never occurs, but ... */

	if (!s->hit && s->servername_done == 1 && s->session->tlsext_hostname != NULL)
		{ 
		if (limit - ret - 4 < 0) return NULL; 

		s2n(TLSEXT_TYPE_server_name,ret);
		s2n(0,ret);
		}

	if(s->s3->send_connection_binding)
        {
          int el;
          
          if(!ssl_add_serverhello_renegotiate_ext(s, 0, &el, 0))
              {
              SSLerr(SSL_F_SSL_ADD_SERVERHELLO_TLSEXT, ERR_R_INTERNAL_ERROR);
              return NULL;
              }

          if((limit - p - 4 - el) < 0) return NULL;
          
          s2n(TLSEXT_TYPE_renegotiate,ret);
          s2n(el,ret);

          if(!ssl_add_serverhello_renegotiate_ext(s, ret, &el, el))
              {
              SSLerr(SSL_F_SSL_ADD_SERVERHELLO_TLSEXT, ERR_R_INTERNAL_ERROR);
              return NULL;
              }

          ret += el;
        }
	
	if (s->tlsext_ticket_expected
		&& !(SSL_get_options(s) & SSL_OP_NO_TICKET)) 
		{ 
		if (limit - ret - 4 < 0) return NULL; 
		s2n(TLSEXT_TYPE_session_ticket,ret);
		s2n(0,ret);
		}

	if (s->tlsext_status_expected)
		{ 
		if ((long)(limit - ret - 4) < 0) return NULL; 
		s2n(TLSEXT_TYPE_status_request,ret);
		s2n(0,ret);
		}

	next_proto_neg_seen = s->s3->next_proto_neg_seen;
	s->s3->next_proto_neg_seen = 0;
	if (next_proto_neg_seen && s->ctx->next_protos_advertised_cb)
		{
		const unsigned char *npa;
		unsigned int npalen;
		int r;

		r = s->ctx->next_protos_advertised_cb(s, &npa, &npalen, s->ctx->next_protos_advertised_cb_arg);
		if (r == SSL_TLSEXT_ERR_OK)
			{
			if ((long)(limit - ret - 4 - npalen) < 0) return NULL;
			s2n(TLSEXT_TYPE_next_proto_neg,ret);
			s2n(npalen,ret);
			memcpy(ret, npa, npalen);
			ret += npalen;
			s->s3->next_proto_neg_seen = 1;
			}
		}

	if (s->s3->snap_start_ext_seen)
		{
		if ((long)(limit - ret - 14) < 0) return NULL;
		s2n(TLSEXT_TYPE_snap_start,ret);
		s2n(10,ret); /* extension length */
		memcpy(ret, s->ctx->snap_start_orbit, 8);
		ret += 8;
		/* This is the ciphersuite that we would pick in the event of a
		 * Snap Start handshake. (Maybe the server wants to do EDH
		 * unless the client is Snap Start capable). At the moment we
		 * don't have any logic to pick a different cipher suite so we
		 * repeat the choice from the ServerHello. */
		s2n(s->s3->tmp.new_cipher->id & 0xffff,ret);
		}

	if ((extdatalen = ret-p-2)== 0) 
		return p;

	s2n(extdatalen,p);
	return ret;
	}


static int ssl_hash_snap_start_client_hello(SSL* s,
					    const char* data,
					    unsigned len,
					    unsigned ext_len)
	{
	/* We walk the ClientHello from the beginning, writing
	 * adjusted lengths into |b| and hashing as we go.
	 *
	 * The resulting ClientHello is going to be shorter by the length of
	 * this extension, which is |ext_len + 4| (two bytes for the type and two for
	 * the length). */

	const unsigned char *p;
	unsigned remaining;
	unsigned char b[3], *c;
	unsigned long l;

	p = (unsigned char*) data;
	remaining = len;
	/* Handshake header: type */
	if (!remaining)
		return 0;
	ssl3_finish_mac(s, p, 1);
	p++;
	remaining--;
	/* Handshake header: length */
	if (remaining < 3)
		return 0;
	n2l3(p, l);
	l -= ext_len + 4;
	c = b;
	l2n3(l, c);
	ssl3_finish_mac(s, b, 3);
	remaining -= 3;
	/* ClientHello: version and random */
	if (remaining < 34)
		return 0;
	ssl3_finish_mac(s, p, 34);
	p += 34;
	remaining -= 34;
	/* ClientHello: session id length */
	if (!remaining)
		return 0;
	l = *p;
	ssl3_finish_mac(s, p, 1);
	p++;
	remaining--;
	/* ClientHello: session id */
	if (remaining < l)
		return 0;
	ssl3_finish_mac(s, p, l);
	p += l;
	remaining -= l;
	/* ClientHello: cipher suites length */
	if (remaining < 2)
		return 0;
	ssl3_finish_mac(s, p, 2);
	n2s(p, l);
	remaining -= 2;
	/* ClientHello: cipher suites */
	if (remaining < l)
		return 0;
	ssl3_finish_mac(s, p, l);
	p += l;
	remaining -= l;
	/* ClientHello: compression methods length */
	if (!remaining)
		return 0;
	l = *p;
	ssl3_finish_mac(s, p, 1);
	p++;
	remaining--;
	/* ClientHello: compression methods */
	if (remaining < l)
		return 0;
	ssl3_finish_mac(s, p, l);
	p += l;
	remaining -= l;
	/* ClientHello: extensions length (must exist given that we're already
	 * parsing the extensions from it */
	if (remaining < 2)
		return 0;
	n2s(p, l);
	remaining -= 2;
	if (l != remaining || l < ext_len + 4)
		return 0;
	l -= ext_len + 4;
	c = b;
	s2n(l, c);
	ssl3_finish_mac(s, b, 2);

	while (remaining)
		{
		unsigned long extension_type, extension_len;
		if (remaining < 4)
			return 0;
		n2s(p, extension_type);
		n2s(p, extension_len);
		remaining -= 4;
		if (remaining < extension_len)
			return 0;
		if (extension_type != TLSEXT_TYPE_snap_start)
			ssl3_finish_mac(s, p - 4, extension_len + 4);
		p += extension_len;
		remaining -= extension_len;
		}

	return 1;
	}

static char ssl_parse_snap_start_tlsext(SSL *s, const unsigned char *data, unsigned short len)
	{
	ptrdiff_t extension_offset = data - (unsigned char *) s->init_buf->data;

	if (len > 0 && len < 36)
		return 0;
	s->s3->snap_start_ext_seen = 1;
	if (len == 0)
		return 1;

	fnv1a64_init((FNV1A64*) s->s3->response_hash);

	/* We need to make a copy of the ClientHello because we'll be hashing a
	 * modified version. However, if we enter recovery then we need to hash
	 * the unchanged message.
	 *
	 * We are adding 4 bytes to the length here because we're including the
	 * handshake header. */
	s->s3->snap_start_client_hello.left = s->init_num + 4;
	s->s3->snap_start_client_hello.offset = 0;
	s->s3->snap_start_client_hello.buf = OPENSSL_malloc(s->init_num + 4);
	if (!s->s3->snap_start_client_hello.buf)
		{
		/* If we're out of memory then we pretend that we
		 * didn't see the extension. */
		s->s3->snap_start_ext_seen = 0;
		return 1;
		}

	memcpy(s->s3->snap_start_client_hello.buf, s->init_buf->data, s->init_num + 4);
	memcpy(s->s3->server_random, s->s3->client_random, 4); /* time */
	memcpy(s->s3->server_random + 4, data, 28); /* orbit and random bytes */
	memcpy(s->s3->predicted_response_hash, data + 28, 8);

	/* Point snap_start_records to within the copy of the ClientHello */
	s->s3->snap_start_records.offset = 0;
	s->s3->snap_start_records.left = len - 36;
	s->s3->snap_start_records.buf = s->s3->snap_start_client_hello.buf + extension_offset + 36;

	/* Reset the handshake hash */
	ssl3_init_finished_mac(s);

	/* Need to hash the ClientHello as if the snap start extension wasn't
	 * included. */
	if (!ssl_hash_snap_start_client_hello(
			s,
			s->init_buf->data,
			s->init_num + 4 /* four bytes of handshake header */,
			len))
		{
		return 0;
		}

	s->s3->snap_start_requested = 1;
	return 1;
	}

int ssl_parse_clienthello_tlsext(SSL *s, unsigned char **p, unsigned char *d, int n, int *al)
	{
	unsigned short type;
	unsigned short size;
	unsigned short len;
	unsigned char *data = *p;
	int renegotiate_seen = 0;

	s->servername_done = 0;
	s->tlsext_status_type = -1;

	if (data >= (d+n-2))
		goto ri_check;

	n2s(data,len);

	if (data > (d+n-len)) 
		goto ri_check;

	while (data <= (d+n-4))
		{
		n2s(data,type);
		n2s(data,size);

		if (data+size > (d+n))
	   		goto ri_check;

		if (s->tlsext_debug_cb)
			s->tlsext_debug_cb(s, 0, type, data, size,
						s->tlsext_debug_arg);
/* The servername extension is treated as follows:

   - Only the hostname type is supported with a maximum length of 255.
   - The servername is rejected if too long or if it contains zeros,
     in which case an fatal alert is generated.
   - The servername field is maintained together with the session cache.
   - When a session is resumed, the servername call back invoked in order
     to allow the application to position itself to the right context. 
   - The servername is acknowledged if it is new for a session or when 
     it is identical to a previously used for the same session. 
     Applications can control the behaviour.  They can at any time
     set a 'desirable' servername for a new SSL object. This can be the
     case for example with HTTPS when a Host: header field is received and
     a renegotiation is requested. In this case, a possible servername
     presented in the new client hello is only acknowledged if it matches
     the value of the Host: field. 
   - Applications must  use SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION
     if they provide for changing an explicit servername context for the session,
     i.e. when the session has been established with a servername extension. 
   - On session reconnect, the servername extension may be absent. 

*/      

		if (type == TLSEXT_TYPE_server_name)
			{
			unsigned char *sdata;
			int servname_type;
			int dsize; 
		
			if (size < 2) 
				{
				*al = SSL_AD_DECODE_ERROR;
				return 0;
				}
			n2s(data,dsize);  
			size -= 2;
			if (dsize > size  ) 
				{
				*al = SSL_AD_DECODE_ERROR;
				return 0;
				} 

			sdata = data;
			while (dsize > 3) 
				{
	 			servname_type = *(sdata++); 
				n2s(sdata,len);
				dsize -= 3;

				if (len > dsize) 
					{
					*al = SSL_AD_DECODE_ERROR;
					return 0;
					}
				if (s->servername_done == 0)
				switch (servname_type)
					{
				case TLSEXT_NAMETYPE_host_name:
					if (!s->hit)
						{
						if(s->session->tlsext_hostname)
							{
							*al = SSL_AD_DECODE_ERROR;
							return 0;
							}
						if (len > TLSEXT_MAXLEN_host_name)
							{
							*al = TLS1_AD_UNRECOGNIZED_NAME;
							return 0;
							}
						if ((s->session->tlsext_hostname = OPENSSL_malloc(len+1)) == NULL)
							{
							*al = TLS1_AD_INTERNAL_ERROR;
							return 0;
							}
						memcpy(s->session->tlsext_hostname, sdata, len);
						s->session->tlsext_hostname[len]='\0';
						if (strlen(s->session->tlsext_hostname) != len) {
							OPENSSL_free(s->session->tlsext_hostname);
							s->session->tlsext_hostname = NULL;
							*al = TLS1_AD_UNRECOGNIZED_NAME;
							return 0;
						}
						s->servername_done = 1; 

						}
					else 
						s->servername_done = s->session->tlsext_hostname
							&& strlen(s->session->tlsext_hostname) == len 
							&& strncmp(s->session->tlsext_hostname, (char *)sdata, len) == 0;
					
					break;

				default:
					break;
					}
				 
				dsize -= len;
				}
			if (dsize != 0) 
				{
				*al = SSL_AD_DECODE_ERROR;
				return 0;
				}

			}
		else if (type == TLSEXT_TYPE_renegotiate)
			{
			if(!ssl_parse_clienthello_renegotiate_ext(s, data, size, al))
				return 0;
			renegotiate_seen = 1;
			}
		else if (type == TLSEXT_TYPE_status_request &&
		         s->version != DTLS1_VERSION && s->ctx->tlsext_status_cb)
			{
		
			if (size < 5) 
				{
				*al = SSL_AD_DECODE_ERROR;
				return 0;
				}

			s->tlsext_status_type = *data++;
			size--;
			if (s->tlsext_status_type == TLSEXT_STATUSTYPE_ocsp)
				{
				const unsigned char *sdata;
				int dsize;
				/* Read in responder_id_list */
				n2s(data,dsize);
				size -= 2;
				if (dsize > size  ) 
					{
					*al = SSL_AD_DECODE_ERROR;
					return 0;
					}
				while (dsize > 0)
					{
					OCSP_RESPID *id;
					int idsize;
					if (dsize < 4)
						{
						*al = SSL_AD_DECODE_ERROR;
						return 0;
						}
					n2s(data, idsize);
					dsize -= 2 + idsize;
					size -= 2 + idsize;
					if (dsize < 0)
						{
						*al = SSL_AD_DECODE_ERROR;
						return 0;
						}
					sdata = data;
					data += idsize;
					id = d2i_OCSP_RESPID(NULL,
								&sdata, idsize);
					if (!id)
						{
						*al = SSL_AD_DECODE_ERROR;
						return 0;
						}
					if (data != sdata)
						{
						OCSP_RESPID_free(id);
						*al = SSL_AD_DECODE_ERROR;
						return 0;
						}
					if (!s->tlsext_ocsp_ids
						&& !(s->tlsext_ocsp_ids =
						sk_OCSP_RESPID_new_null()))
						{
						OCSP_RESPID_free(id);
						*al = SSL_AD_INTERNAL_ERROR;
						return 0;
						}
					if (!sk_OCSP_RESPID_push(
							s->tlsext_ocsp_ids, id))
						{
						OCSP_RESPID_free(id);
						*al = SSL_AD_INTERNAL_ERROR;
						return 0;
						}
					}

				/* Read in request_extensions */
				if (size < 2)
					{
					*al = SSL_AD_DECODE_ERROR;
					return 0;
					}
				n2s(data,dsize);
				size -= 2;
				if (dsize != size)
					{
					*al = SSL_AD_DECODE_ERROR;
					return 0;
					}
				sdata = data;
				if (dsize > 0)
					{
					s->tlsext_ocsp_exts =
						d2i_X509_EXTENSIONS(NULL,
							&sdata, dsize);
					if (!s->tlsext_ocsp_exts
						|| (data + dsize != sdata))
						{
						*al = SSL_AD_DECODE_ERROR;
						return 0;
						}
					}
				}
				/* We don't know what to do with any other type
 			 	* so ignore it.
 			 	*/
				else
					s->tlsext_status_type = -1;
			}
		else if (type == TLSEXT_TYPE_next_proto_neg)
			{
			/* We shouldn't accept this extension on a
			 * renegotiation, but we currently do.
			 *
			 * s->new_session will be set on renegotiation, but we
			 * probably shouldn't rely that it couldn't be set on
			 * the initial renegotation too in certain cases (when
			 * there's some other reason to disallow resuming an
			 * earlier session -- the current code won't be doing
			 * anything like that, but this might change).

			 * A valid sign that there's been a previous handshake
			 * in this connection is if s->s3->tmp.finish_md_len >
			 * 0.  (We are talking about a check that will happen
			 * in the Hello protocol round, well before a new
			 * Finished message could have been computed.) */
			s->s3->next_proto_neg_seen = 1;
			}

		else if (type == TLSEXT_TYPE_snap_start && s->ctx->snap_start_orbit_valid)
			{
			if (ssl_parse_snap_start_tlsext(s, data, size) == 0)
				return 0;
			}

		/* session ticket processed earlier */

		data+=size;		
		}
	*p = data;

	ri_check:

	/* Need RI if renegotiating */

	if (!renegotiate_seen && s->new_session &&
		!(s->options & SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION))
		{
		*al = SSL_AD_HANDSHAKE_FAILURE;
	 	SSLerr(SSL_F_SSL_PARSE_CLIENTHELLO_TLSEXT,
				SSL_R_UNSAFE_LEGACY_RENEGOTIATION_DISABLED);
		return 0;
		}

	return 1;
	}

/* ssl_next_proto_validate validates a Next Protocol Negotiation block. No
 * elements of zero length are allowed and the set of elements must exactly fill
 * the length of the block. */
static char ssl_next_proto_validate(unsigned char *d, unsigned len)
	{
	unsigned int off = 0;

	while (off < len)
		{
		if (d[off] == 0)
			return 0;
		off += d[off];
		off++;
		}

	return off == len;
	}

int ssl_parse_serverhello_tlsext(SSL *s, unsigned char **p, unsigned char *d, int n, int *al)
	{
	unsigned short length;
	unsigned short type;
	unsigned short size;
	unsigned char *data = *p;
	int tlsext_servername = 0;
	int renegotiate_seen = 0;

	if (data >= (d+n-2))
		goto ri_check;

	n2s(data,length);
	if (data+length != d+n)
		{
		*al = SSL_AD_DECODE_ERROR;
		return 0;
		}

	while(data <= (d+n-4))
		{
		n2s(data,type);
		n2s(data,size);

		if (data+size > (d+n))
	   		goto ri_check;

		if (s->tlsext_debug_cb)
			s->tlsext_debug_cb(s, 1, type, data, size,
						s->tlsext_debug_arg);

		if (type == TLSEXT_TYPE_server_name)
			{
			if (s->tlsext_hostname == NULL || size > 0)
				{
				*al = TLS1_AD_UNRECOGNIZED_NAME;
				return 0;
				}
			tlsext_servername = 1;   
			}
		else if (type == TLSEXT_TYPE_session_ticket)
			{
			if ((SSL_get_options(s) & SSL_OP_NO_TICKET)
				|| (size > 0))
				{
				*al = TLS1_AD_UNSUPPORTED_EXTENSION;
				return 0;
				}
			s->tlsext_ticket_expected = 1;
			}
		else if (type == TLSEXT_TYPE_status_request &&
		         s->version != DTLS1_VERSION)
			{
			/* MUST be empty and only sent if we've requested
			 * a status request message.
			 */ 
			if ((s->tlsext_status_type == -1) || (size > 0))
				{
				*al = TLS1_AD_UNSUPPORTED_EXTENSION;
				return 0;
				}
			/* Set flag to expect CertificateStatus message */
			s->tlsext_status_expected = 1;
			}
		else if (type == TLSEXT_TYPE_next_proto_neg)
			{
			unsigned char *selected;
			unsigned char selected_len;

			/* We must have requested it. */
			if ((s->ctx->next_proto_select_cb == NULL))
				{
				*al = TLS1_AD_UNSUPPORTED_EXTENSION;
				return 0;
				}
			/* The data must be valid */
			if (!ssl_next_proto_validate(data, size))
				{
				*al = TLS1_AD_DECODE_ERROR;
				return 0;
				}
			if (s->ctx->next_proto_select_cb(s, &selected, &selected_len, data, size, s->ctx->next_proto_select_cb_arg) != SSL_TLSEXT_ERR_OK)
				{
				*al = TLS1_AD_INTERNAL_ERROR;
				return 0;
				}
			s->next_proto_negotiated = OPENSSL_malloc(selected_len);
			if (!s->next_proto_negotiated)
				{
				*al = TLS1_AD_INTERNAL_ERROR;
				return 0;
				}
			memcpy(s->next_proto_negotiated, selected, selected_len);
			s->next_proto_negotiated_len = selected_len;
			}
		else if (type == TLSEXT_TYPE_renegotiate)
			{
			if(!ssl_parse_serverhello_renegotiate_ext(s, data, size, al))
				return 0;
			renegotiate_seen = 1;
			}
		data+=size;		
		}

	if (data != d+n)
		{
		*al = SSL_AD_DECODE_ERROR;
		return 0;
		}

	if (!s->hit && tlsext_servername == 1)
		{
 		if (s->tlsext_hostname)
			{
			if (s->session->tlsext_hostname == NULL)
				{
				s->session->tlsext_hostname = BUF_strdup(s->tlsext_hostname);	
				if (!s->session->tlsext_hostname)
					{
					*al = SSL_AD_UNRECOGNIZED_NAME;
					return 0;
					}
				}
			else 
				{
				*al = SSL_AD_DECODE_ERROR;
				return 0;
				}
			}
		}

	*p = data;

	ri_check:

	/* Determine if we need to see RI. Strictly speaking if we want to
	 * avoid an attack we should *always* see RI even on initial server
	 * hello because the client doesn't see any renegotiation during an
	 * attack. However this would mean we could not connect to any server
	 * which doesn't support RI so for the immediate future tolerate RI
	 * absence on initial connect only.
	 */
	if (!renegotiate_seen
		&& !(s->options & SSL_OP_LEGACY_SERVER_CONNECT)
		&& !(s->options & SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION))
		{
		*al = SSL_AD_HANDSHAKE_FAILURE;
		SSLerr(SSL_F_SSL_PARSE_SERVERHELLO_TLSEXT,
				SSL_R_UNSAFE_LEGACY_RENEGOTIATION_DISABLED);
		return 0;
		}

	return 1;
	}

int ssl_check_clienthello_tlsext(SSL *s)
	{
	int ret=SSL_TLSEXT_ERR_NOACK;
	int al = SSL_AD_UNRECOGNIZED_NAME;

	if (s->ctx != NULL && s->ctx->tlsext_servername_callback != 0) 
		ret = s->ctx->tlsext_servername_callback(s, &al, s->ctx->tlsext_servername_arg);
	else if (s->initial_ctx != NULL && s->initial_ctx->tlsext_servername_callback != 0) 		
		ret = s->initial_ctx->tlsext_servername_callback(s, &al, s->initial_ctx->tlsext_servername_arg);

	/* If status request then ask callback what to do.
 	 * Note: this must be called after servername callbacks in case 
 	 * the certificate has changed.
 	 */
	if ((s->tlsext_status_type != -1) && s->ctx->tlsext_status_cb)
		{
		int r;
		r = s->ctx->tlsext_status_cb(s, s->ctx->tlsext_status_arg);
		switch (r)
			{
			/* We don't want to send a status request response */
			case SSL_TLSEXT_ERR_NOACK:
				s->tlsext_status_expected = 0;
				break;
			/* status request response should be sent */
			case SSL_TLSEXT_ERR_OK:
				if (s->tlsext_ocsp_resp)
					s->tlsext_status_expected = 1;
				else
					s->tlsext_status_expected = 0;
				break;
			/* something bad happened */
			case SSL_TLSEXT_ERR_ALERT_FATAL:
				ret = SSL_TLSEXT_ERR_ALERT_FATAL;
				al = SSL_AD_INTERNAL_ERROR;
				goto err;
			}
		}
	else
		s->tlsext_status_expected = 0;
	err:
	switch (ret)
		{
		case SSL_TLSEXT_ERR_ALERT_FATAL:
			ssl3_send_alert(s,SSL3_AL_FATAL,al); 
			return -1;

		case SSL_TLSEXT_ERR_ALERT_WARNING:
			ssl3_send_alert(s,SSL3_AL_WARNING,al);
			return 1; 
					
		case SSL_TLSEXT_ERR_NOACK:
			s->servername_done=0;
			default:
		return 1;
		}
	}

int ssl_check_serverhello_tlsext(SSL *s)
	{
	int ret=SSL_TLSEXT_ERR_NOACK;
	int al = SSL_AD_UNRECOGNIZED_NAME;

	if (s->ctx != NULL && s->ctx->tlsext_servername_callback != 0) 
		ret = s->ctx->tlsext_servername_callback(s, &al, s->ctx->tlsext_servername_arg);
	else if (s->initial_ctx != NULL && s->initial_ctx->tlsext_servername_callback != 0) 		
		ret = s->initial_ctx->tlsext_servername_callback(s, &al, s->initial_ctx->tlsext_servername_arg);

	/* If we've requested certificate status and we wont get one
 	 * tell the callback
 	 */
	if ((s->tlsext_status_type != -1) && !(s->tlsext_status_expected)
			&& s->ctx->tlsext_status_cb)
		{
		int r;
		/* Set resp to NULL, resplen to -1 so callback knows
 		 * there is no response.
 		 */
		if (s->tlsext_ocsp_resp)
			{
			OPENSSL_free(s->tlsext_ocsp_resp);
			s->tlsext_ocsp_resp = NULL;
			}
		s->tlsext_ocsp_resplen = -1;
		r = s->ctx->tlsext_status_cb(s, s->ctx->tlsext_status_arg);
		if (r == 0)
			{
			al = SSL_AD_BAD_CERTIFICATE_STATUS_RESPONSE;
			ret = SSL_TLSEXT_ERR_ALERT_FATAL;
			}
		if (r < 0)
			{
			al = SSL_AD_INTERNAL_ERROR;
			ret = SSL_TLSEXT_ERR_ALERT_FATAL;
			}
		}

	switch (ret)
		{
		case SSL_TLSEXT_ERR_ALERT_FATAL:
			ssl3_send_alert(s,SSL3_AL_FATAL,al); 
			return -1;

		case SSL_TLSEXT_ERR_ALERT_WARNING:
			ssl3_send_alert(s,SSL3_AL_WARNING,al);
			return 1; 
					
		case SSL_TLSEXT_ERR_NOACK:
			s->servername_done=0;
			default:
		return 1;
		}
	}

/* Since the server cache lookup is done early on in the processing of client
 * hello and other operations depend on the result we need to handle any TLS
 * session ticket extension at the same time.
 */

int tls1_process_ticket(SSL *s, unsigned char *session_id, int len,
				const unsigned char *limit, SSL_SESSION **ret)
	{
	/* Point after session ID in client hello */
	const unsigned char *p = session_id + len;
	unsigned short i;

	/* If tickets disabled behave as if no ticket present
 	 * to permit stateful resumption.
 	 */
	if (SSL_get_options(s) & SSL_OP_NO_TICKET)
		return 1;

	if ((s->version <= SSL3_VERSION) || !limit)
		return 1;
	if (p >= limit)
		return -1;
	/* Skip past DTLS cookie */
	if (s->version == DTLS1_VERSION || s->version == DTLS1_BAD_VER)
		{
		i = *(p++);
		p+= i;
		if (p >= limit)
			return -1;
		}
	/* Skip past cipher list */
	n2s(p, i);
	p+= i;
	if (p >= limit)
		return -1;
	/* Skip past compression algorithm list */
	i = *(p++);
	p += i;
	if (p > limit)
		return -1;
	/* Now at start of extensions */
	if ((p + 2) >= limit)
		return 1;
	n2s(p, i);
	while ((p + 4) <= limit)
		{
		unsigned short type, size;
		n2s(p, type);
		n2s(p, size);
		if (p + size > limit)
			return 1;
		if (type == TLSEXT_TYPE_session_ticket)
			{
			/* If zero length note client will accept a ticket
 			 * and indicate cache miss to trigger full handshake
 			 */
			if (size == 0)
				{
				s->tlsext_ticket_expected = 1;
				return 0;	/* Cache miss */
				}
			return tls_decrypt_ticket(s, p, size, session_id, len,
									ret);
			}
		p += size;
		}
	return 1;
	}

static int tls_decrypt_ticket(SSL *s, const unsigned char *etick, int eticklen,
				const unsigned char *sess_id, int sesslen,
				SSL_SESSION **psess)
	{
	SSL_SESSION *sess;
	unsigned char *sdec;
	const unsigned char *p;
	int slen, mlen, renew_ticket = 0;
	unsigned char tick_hmac[EVP_MAX_MD_SIZE];
	HMAC_CTX hctx;
	EVP_CIPHER_CTX ctx;
	SSL_CTX *tctx = s->initial_ctx;
	/* Need at least keyname + iv + some encrypted data */
	if (eticklen < 48)
		goto tickerr;
	/* Initialize session ticket encryption and HMAC contexts */
	HMAC_CTX_init(&hctx);
	EVP_CIPHER_CTX_init(&ctx);
	if (tctx->tlsext_ticket_key_cb)
		{
		unsigned char *nctick = (unsigned char *)etick;
		int rv = tctx->tlsext_ticket_key_cb(s, nctick, nctick + 16,
							&ctx, &hctx, 0);
		if (rv < 0)
			return -1;
		if (rv == 0)
			goto tickerr;
		if (rv == 2)
			renew_ticket = 1;
		}
	else
		{
		/* Check key name matches */
		if (memcmp(etick, tctx->tlsext_tick_key_name, 16))
			goto tickerr;
		HMAC_Init_ex(&hctx, tctx->tlsext_tick_hmac_key, 16,
					tlsext_tick_md(), NULL);
		EVP_DecryptInit_ex(&ctx, EVP_aes_128_cbc(), NULL,
				tctx->tlsext_tick_aes_key, etick + 16);
		}
	/* Attempt to process session ticket, first conduct sanity and
 	 * integrity checks on ticket.
 	 */
	mlen = HMAC_size(&hctx);
	eticklen -= mlen;
	/* Check HMAC of encrypted ticket */
	HMAC_Update(&hctx, etick, eticklen);
	HMAC_Final(&hctx, tick_hmac, NULL);
	HMAC_CTX_cleanup(&hctx);
	if (memcmp(tick_hmac, etick + eticklen, mlen))
		goto tickerr;
	/* Attempt to decrypt session data */
	/* Move p after IV to start of encrypted ticket, update length */
	p = etick + 16 + EVP_CIPHER_CTX_iv_length(&ctx);
	eticklen -= 16 + EVP_CIPHER_CTX_iv_length(&ctx);
	sdec = OPENSSL_malloc(eticklen);
	if (!sdec)
		{
		EVP_CIPHER_CTX_cleanup(&ctx);
		return -1;
		}
	EVP_DecryptUpdate(&ctx, sdec, &slen, p, eticklen);
	if (EVP_DecryptFinal(&ctx, sdec + slen, &mlen) <= 0)
		goto tickerr;
	slen += mlen;
	EVP_CIPHER_CTX_cleanup(&ctx);
	p = sdec;
		
	sess = d2i_SSL_SESSION(NULL, &p, slen);
	OPENSSL_free(sdec);
	if (sess)
		{
		/* The session ID if non-empty is used by some clients to
 		 * detect that the ticket has been accepted. So we copy it to
 		 * the session structure. If it is empty set length to zero
 		 * as required by standard.
 		 */
		if (sesslen)
			memcpy(sess->session_id, sess_id, sesslen);
		sess->session_id_length = sesslen;
		*psess = sess;
		s->tlsext_ticket_expected = renew_ticket;
		return 1;
		}
	/* If session decrypt failure indicate a cache miss and set state to
 	 * send a new ticket
 	 */
	tickerr:	
	s->tlsext_ticket_expected = 1;
	return 0;
	}

#endif
