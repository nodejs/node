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

#include <stdio.h>
#include <openssl/objects.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/ocsp.h>
#include <openssl/rand.h>
#include "ssl_locl.h"

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
	tls1_export_keying_material,
	};

long tls1_default_timeout(void)
	{
	/* 2 hours, the 24 hours mentioned in the TLSv1 spec
	 * is way too long for http, the cache would over fill */
	return(60*60*2);
	}

int tls1_new(SSL *s)
	{
	if (!ssl3_new(s)) return(0);
	s->method->ssl_clear(s);
	return(1);
	}

void tls1_free(SSL *s)
	{
#ifndef OPENSSL_NO_TLSEXT
	if (s->tlsext_session_ticket)
		{
		OPENSSL_free(s->tlsext_session_ticket);
		}
#endif /* OPENSSL_NO_TLSEXT */
	ssl3_free(s);
	}

void tls1_clear(SSL *s)
	{
	ssl3_clear(s);
	s->version = s->method->version;
	}

#ifndef OPENSSL_NO_EC

static int nid_list[] =
	{
		NID_sect163k1, /* sect163k1 (1) */
		NID_sect163r1, /* sect163r1 (2) */
		NID_sect163r2, /* sect163r2 (3) */
		NID_sect193r1, /* sect193r1 (4) */ 
		NID_sect193r2, /* sect193r2 (5) */ 
		NID_sect233k1, /* sect233k1 (6) */
		NID_sect233r1, /* sect233r1 (7) */ 
		NID_sect239k1, /* sect239k1 (8) */ 
		NID_sect283k1, /* sect283k1 (9) */
		NID_sect283r1, /* sect283r1 (10) */ 
		NID_sect409k1, /* sect409k1 (11) */ 
		NID_sect409r1, /* sect409r1 (12) */
		NID_sect571k1, /* sect571k1 (13) */ 
		NID_sect571r1, /* sect571r1 (14) */ 
		NID_secp160k1, /* secp160k1 (15) */
		NID_secp160r1, /* secp160r1 (16) */ 
		NID_secp160r2, /* secp160r2 (17) */ 
		NID_secp192k1, /* secp192k1 (18) */
		NID_X9_62_prime192v1, /* secp192r1 (19) */ 
		NID_secp224k1, /* secp224k1 (20) */ 
		NID_secp224r1, /* secp224r1 (21) */
		NID_secp256k1, /* secp256k1 (22) */ 
		NID_X9_62_prime256v1, /* secp256r1 (23) */ 
		NID_secp384r1, /* secp384r1 (24) */
		NID_secp521r1  /* secp521r1 (25) */	
	};

static int pref_list[] =
	{
		NID_sect571r1, /* sect571r1 (14) */ 
		NID_sect571k1, /* sect571k1 (13) */ 
		NID_secp521r1, /* secp521r1 (25) */	
		NID_sect409k1, /* sect409k1 (11) */ 
		NID_sect409r1, /* sect409r1 (12) */
		NID_secp384r1, /* secp384r1 (24) */
		NID_sect283k1, /* sect283k1 (9) */
		NID_sect283r1, /* sect283r1 (10) */ 
		NID_secp256k1, /* secp256k1 (22) */ 
		NID_X9_62_prime256v1, /* secp256r1 (23) */ 
		NID_sect239k1, /* sect239k1 (8) */ 
		NID_sect233k1, /* sect233k1 (6) */
		NID_sect233r1, /* sect233r1 (7) */ 
		NID_secp224k1, /* secp224k1 (20) */ 
		NID_secp224r1, /* secp224r1 (21) */
		NID_sect193r1, /* sect193r1 (4) */ 
		NID_sect193r2, /* sect193r2 (5) */ 
		NID_secp192k1, /* secp192k1 (18) */
		NID_X9_62_prime192v1, /* secp192r1 (19) */ 
		NID_sect163k1, /* sect163k1 (1) */
		NID_sect163r1, /* sect163r1 (2) */
		NID_sect163r2, /* sect163r2 (3) */
		NID_secp160k1, /* secp160k1 (15) */
		NID_secp160r1, /* secp160r1 (16) */ 
		NID_secp160r2, /* secp160r2 (17) */ 
	};

int tls1_ec_curve_id2nid(int curve_id)
	{
	/* ECC curves from draft-ietf-tls-ecc-12.txt (Oct. 17, 2005) */
	if ((curve_id < 1) || ((unsigned int)curve_id >
				sizeof(nid_list)/sizeof(nid_list[0])))
		return 0;
	return nid_list[curve_id-1];
	}

int tls1_ec_nid2curve_id(int nid)
	{
	/* ECC curves from draft-ietf-tls-ecc-12.txt (Oct. 17, 2005) */
	switch (nid)
		{
	case NID_sect163k1: /* sect163k1 (1) */
		return 1;
	case NID_sect163r1: /* sect163r1 (2) */
		return 2;
	case NID_sect163r2: /* sect163r2 (3) */
		return 3;
	case NID_sect193r1: /* sect193r1 (4) */ 
		return 4;
	case NID_sect193r2: /* sect193r2 (5) */ 
		return 5;
	case NID_sect233k1: /* sect233k1 (6) */
		return 6;
	case NID_sect233r1: /* sect233r1 (7) */ 
		return 7;
	case NID_sect239k1: /* sect239k1 (8) */ 
		return 8;
	case NID_sect283k1: /* sect283k1 (9) */
		return 9;
	case NID_sect283r1: /* sect283r1 (10) */ 
		return 10;
	case NID_sect409k1: /* sect409k1 (11) */ 
		return 11;
	case NID_sect409r1: /* sect409r1 (12) */
		return 12;
	case NID_sect571k1: /* sect571k1 (13) */ 
		return 13;
	case NID_sect571r1: /* sect571r1 (14) */ 
		return 14;
	case NID_secp160k1: /* secp160k1 (15) */
		return 15;
	case NID_secp160r1: /* secp160r1 (16) */ 
		return 16;
	case NID_secp160r2: /* secp160r2 (17) */ 
		return 17;
	case NID_secp192k1: /* secp192k1 (18) */
		return 18;
	case NID_X9_62_prime192v1: /* secp192r1 (19) */ 
		return 19;
	case NID_secp224k1: /* secp224k1 (20) */ 
		return 20;
	case NID_secp224r1: /* secp224r1 (21) */
		return 21;
	case NID_secp256k1: /* secp256k1 (22) */ 
		return 22;
	case NID_X9_62_prime256v1: /* secp256r1 (23) */ 
		return 23;
	case NID_secp384r1: /* secp384r1 (24) */
		return 24;
	case NID_secp521r1:  /* secp521r1 (25) */	
		return 25;
	default:
		return 0;
		}
	}
#endif /* OPENSSL_NO_EC */

#ifndef OPENSSL_NO_TLSEXT

/* List of supported signature algorithms and hashes. Should make this
 * customisable at some point, for now include everything we support.
 */

#ifdef OPENSSL_NO_RSA
#define tlsext_sigalg_rsa(md) /* */
#else
#define tlsext_sigalg_rsa(md) md, TLSEXT_signature_rsa,
#endif

#ifdef OPENSSL_NO_DSA
#define tlsext_sigalg_dsa(md) /* */
#else
#define tlsext_sigalg_dsa(md) md, TLSEXT_signature_dsa,
#endif

#ifdef OPENSSL_NO_ECDSA
#define tlsext_sigalg_ecdsa(md) /* */
#else
#define tlsext_sigalg_ecdsa(md) md, TLSEXT_signature_ecdsa,
#endif

#define tlsext_sigalg(md) \
		tlsext_sigalg_rsa(md) \
		tlsext_sigalg_dsa(md) \
		tlsext_sigalg_ecdsa(md)

static unsigned char tls12_sigalgs[] = {
#ifndef OPENSSL_NO_SHA512
	tlsext_sigalg(TLSEXT_hash_sha512)
	tlsext_sigalg(TLSEXT_hash_sha384)
#endif
#ifndef OPENSSL_NO_SHA256
	tlsext_sigalg(TLSEXT_hash_sha256)
	tlsext_sigalg(TLSEXT_hash_sha224)
#endif
#ifndef OPENSSL_NO_SHA
	tlsext_sigalg(TLSEXT_hash_sha1)
#endif
};

int tls12_get_req_sig_algs(SSL *s, unsigned char *p)
	{
	size_t slen = sizeof(tls12_sigalgs);
	if (p)
		memcpy(p, tls12_sigalgs, slen);
	return (int)slen;
	}

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
        if (s->renegotiate)
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

#ifndef OPENSSL_NO_SRP
	/* Add SRP username if there is one */
	if (s->srp_ctx.login != NULL)
		{ /* Add TLS extension SRP username to the Client Hello message */

		int login_len = strlen(s->srp_ctx.login);	
		if (login_len > 255 || login_len == 0)
			{
			SSLerr(SSL_F_SSL_ADD_CLIENTHELLO_TLSEXT, ERR_R_INTERNAL_ERROR);
			return NULL;
			} 

		/* check for enough space.
		   4 for the srp type type and entension length
		   1 for the srp user identity
		   + srp user identity length 
		*/
		if ((limit - ret - 5 - login_len) < 0) return NULL; 

		/* fill in the extension */
		s2n(TLSEXT_TYPE_srp,ret);
		s2n(login_len+1,ret);
		(*ret++) = (unsigned char) login_len;
		memcpy(ret, s->srp_ctx.login, login_len);
		ret+=login_len;
		}
#endif

#ifndef OPENSSL_NO_EC
	if (s->tlsext_ecpointformatlist != NULL &&
	    s->version != DTLS1_VERSION)
		{
		/* Add TLS extension ECPointFormats to the ClientHello message */
		long lenmax; 

		if ((lenmax = limit - ret - 5) < 0) return NULL; 
		if (s->tlsext_ecpointformatlist_length > (unsigned long)lenmax) return NULL;
		if (s->tlsext_ecpointformatlist_length > 255)
			{
			SSLerr(SSL_F_SSL_ADD_CLIENTHELLO_TLSEXT, ERR_R_INTERNAL_ERROR);
			return NULL;
			}
		
		s2n(TLSEXT_TYPE_ec_point_formats,ret);
		s2n(s->tlsext_ecpointformatlist_length + 1,ret);
		*(ret++) = (unsigned char) s->tlsext_ecpointformatlist_length;
		memcpy(ret, s->tlsext_ecpointformatlist, s->tlsext_ecpointformatlist_length);
		ret+=s->tlsext_ecpointformatlist_length;
		}
	if (s->tlsext_ellipticcurvelist != NULL &&
	    s->version != DTLS1_VERSION)
		{
		/* Add TLS extension EllipticCurves to the ClientHello message */
		long lenmax; 

		if ((lenmax = limit - ret - 6) < 0) return NULL; 
		if (s->tlsext_ellipticcurvelist_length > (unsigned long)lenmax) return NULL;
		if (s->tlsext_ellipticcurvelist_length > 65532)
			{
			SSLerr(SSL_F_SSL_ADD_CLIENTHELLO_TLSEXT, ERR_R_INTERNAL_ERROR);
			return NULL;
			}
		
		s2n(TLSEXT_TYPE_elliptic_curves,ret);
		s2n(s->tlsext_ellipticcurvelist_length + 2, ret);

		/* NB: draft-ietf-tls-ecc-12.txt uses a one-byte prefix for
		 * elliptic_curve_list, but the examples use two bytes.
		 * http://www1.ietf.org/mail-archive/web/tls/current/msg00538.html
		 * resolves this to two bytes.
		 */
		s2n(s->tlsext_ellipticcurvelist_length, ret);
		memcpy(ret, s->tlsext_ellipticcurvelist, s->tlsext_ellipticcurvelist_length);
		ret+=s->tlsext_ellipticcurvelist_length;
		}
#endif /* OPENSSL_NO_EC */

	if (!(SSL_get_options(s) & SSL_OP_NO_TICKET))
		{
		int ticklen;
		if (!s->new_session && s->session && s->session->tlsext_tick)
			ticklen = s->session->tlsext_ticklen;
		else if (s->session && s->tlsext_session_ticket &&
			 s->tlsext_session_ticket->data)
			{
			ticklen = s->tlsext_session_ticket->length;
			s->session->tlsext_tick = OPENSSL_malloc(ticklen);
			if (!s->session->tlsext_tick)
				return NULL;
			memcpy(s->session->tlsext_tick,
			       s->tlsext_session_ticket->data,
			       ticklen);
			s->session->tlsext_ticklen = ticklen;
			}
		else
			ticklen = 0;
		if (ticklen == 0 && s->tlsext_session_ticket &&
		    s->tlsext_session_ticket->data == NULL)
			goto skip_ext;
		/* Check for enough room 2 for extension type, 2 for len
 		 * rest for ticket
  		 */
		if ((long)(limit - ret - 4 - ticklen) < 0) return NULL;
		s2n(TLSEXT_TYPE_session_ticket,ret); 
		s2n(ticklen,ret);
		if (ticklen)
			{
			memcpy(ret, s->session->tlsext_tick, ticklen);
			ret += ticklen;
			}
		}
		skip_ext:

	if (TLS1_get_client_version(s) >= TLS1_2_VERSION)
		{
		if ((size_t)(limit - ret) < sizeof(tls12_sigalgs) + 6)
			return NULL; 
		s2n(TLSEXT_TYPE_signature_algorithms,ret);
		s2n(sizeof(tls12_sigalgs) + 2, ret);
		s2n(sizeof(tls12_sigalgs), ret);
		memcpy(ret, tls12_sigalgs, sizeof(tls12_sigalgs));
		ret += sizeof(tls12_sigalgs);
		}

#ifdef TLSEXT_TYPE_opaque_prf_input
	if (s->s3->client_opaque_prf_input != NULL &&
	    s->version != DTLS1_VERSION)
		{
		size_t col = s->s3->client_opaque_prf_input_len;
		
		if ((long)(limit - ret - 6 - col < 0))
			return NULL;
		if (col > 0xFFFD) /* can't happen */
			return NULL;

		s2n(TLSEXT_TYPE_opaque_prf_input, ret); 
		s2n(col + 2, ret);
		s2n(col, ret);
		memcpy(ret, s->s3->client_opaque_prf_input, col);
		ret += col;
		}
#endif

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

#ifndef OPENSSL_NO_HEARTBEATS
	/* Add Heartbeat extension */
	s2n(TLSEXT_TYPE_heartbeat,ret);
	s2n(1,ret);
	/* Set mode:
	 * 1: peer may send requests
	 * 2: peer not allowed to send requests
	 */
	if (s->tlsext_heartbeat & SSL_TLSEXT_HB_DONT_RECV_REQUESTS)
		*(ret++) = SSL_TLSEXT_HB_DONT_SEND_REQUESTS;
	else
		*(ret++) = SSL_TLSEXT_HB_ENABLED;
#endif

#ifndef OPENSSL_NO_NEXTPROTONEG
	if (s->ctx->next_proto_select_cb && !s->s3->tmp.finish_md_len)
		{
		/* The client advertises an emtpy extension to indicate its
		 * support for Next Protocol Negotiation */
		if (limit - ret - 4 < 0)
			return NULL;
		s2n(TLSEXT_TYPE_next_proto_neg,ret);
		s2n(0,ret);
		}
#endif

#ifndef OPENSSL_NO_SRTP
        if(SSL_get_srtp_profiles(s))
                {
                int el;

                ssl_add_clienthello_use_srtp_ext(s, 0, &el, 0);
                
                if((limit - p - 4 - el) < 0) return NULL;

                s2n(TLSEXT_TYPE_use_srtp,ret);
                s2n(el,ret);

                if(ssl_add_clienthello_use_srtp_ext(s, ret, &el, el))
			{
			SSLerr(SSL_F_SSL_ADD_CLIENTHELLO_TLSEXT, ERR_R_INTERNAL_ERROR);
			return NULL;
			}
                ret += el;
                }
#endif

#ifdef TLSEXT_TYPE_padding
	/* Add padding to workaround bugs in F5 terminators.
	 * See https://tools.ietf.org/html/draft-agl-tls-padding-03
	 *
	 * NB: because this code works out the length of all existing
	 * extensions it MUST always appear last.
	 */
	{
	int hlen = ret - (unsigned char *)s->init_buf->data;
	/* The code in s23_clnt.c to build ClientHello messages includes the
	 * 5-byte record header in the buffer, while the code in s3_clnt.c does
	 * not. */
	if (s->state == SSL23_ST_CW_CLNT_HELLO_A)
		hlen -= 5;
	if (hlen > 0xff && hlen < 0x200)
		{
		hlen = 0x200 - hlen;
		if (hlen >= 4)
			hlen -= 4;
		else
			hlen = 0;

		s2n(TLSEXT_TYPE_padding, ret);
		s2n(hlen, ret);
		memset(ret, 0, hlen);
		ret += hlen;
		}
	}
#endif

	if ((extdatalen = ret-p-2)== 0) 
		return p;

	s2n(extdatalen,p);
	return ret;
	}

unsigned char *ssl_add_serverhello_tlsext(SSL *s, unsigned char *p, unsigned char *limit)
	{
	int extdatalen=0;
	unsigned char *ret = p;
#ifndef OPENSSL_NO_NEXTPROTONEG
	int next_proto_neg_seen;
#endif

	/* don't add extensions for SSLv3, unless doing secure renegotiation */
	if (s->version == SSL3_VERSION && !s->s3->send_connection_binding)
		return p;
	
	ret+=2;
	if (ret>=limit) return NULL; /* this really never occurs, but ... */

	if (!s->hit && s->servername_done == 1 && s->session->tlsext_hostname != NULL)
		{ 
		if ((long)(limit - ret - 4) < 0) return NULL; 

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

#ifndef OPENSSL_NO_EC
	if (s->tlsext_ecpointformatlist != NULL &&
	    s->version != DTLS1_VERSION)
		{
		/* Add TLS extension ECPointFormats to the ServerHello message */
		long lenmax; 

		if ((lenmax = limit - ret - 5) < 0) return NULL; 
		if (s->tlsext_ecpointformatlist_length > (unsigned long)lenmax) return NULL;
		if (s->tlsext_ecpointformatlist_length > 255)
			{
			SSLerr(SSL_F_SSL_ADD_SERVERHELLO_TLSEXT, ERR_R_INTERNAL_ERROR);
			return NULL;
			}
		
		s2n(TLSEXT_TYPE_ec_point_formats,ret);
		s2n(s->tlsext_ecpointformatlist_length + 1,ret);
		*(ret++) = (unsigned char) s->tlsext_ecpointformatlist_length;
		memcpy(ret, s->tlsext_ecpointformatlist, s->tlsext_ecpointformatlist_length);
		ret+=s->tlsext_ecpointformatlist_length;

		}
	/* Currently the server should not respond with a SupportedCurves extension */
#endif /* OPENSSL_NO_EC */

	if (s->tlsext_ticket_expected
		&& !(SSL_get_options(s) & SSL_OP_NO_TICKET)) 
		{ 
		if ((long)(limit - ret - 4) < 0) return NULL; 
		s2n(TLSEXT_TYPE_session_ticket,ret);
		s2n(0,ret);
		}

	if (s->tlsext_status_expected)
		{ 
		if ((long)(limit - ret - 4) < 0) return NULL; 
		s2n(TLSEXT_TYPE_status_request,ret);
		s2n(0,ret);
		}

#ifdef TLSEXT_TYPE_opaque_prf_input
	if (s->s3->server_opaque_prf_input != NULL &&
	    s->version != DTLS1_VERSION)
		{
		size_t sol = s->s3->server_opaque_prf_input_len;
		
		if ((long)(limit - ret - 6 - sol) < 0)
			return NULL;
		if (sol > 0xFFFD) /* can't happen */
			return NULL;

		s2n(TLSEXT_TYPE_opaque_prf_input, ret); 
		s2n(sol + 2, ret);
		s2n(sol, ret);
		memcpy(ret, s->s3->server_opaque_prf_input, sol);
		ret += sol;
		}
#endif

#ifndef OPENSSL_NO_SRTP
        if(s->srtp_profile)
                {
                int el;

                ssl_add_serverhello_use_srtp_ext(s, 0, &el, 0);
                
                if((limit - p - 4 - el) < 0) return NULL;

                s2n(TLSEXT_TYPE_use_srtp,ret);
                s2n(el,ret);

                if(ssl_add_serverhello_use_srtp_ext(s, ret, &el, el))
			{
			SSLerr(SSL_F_SSL_ADD_SERVERHELLO_TLSEXT, ERR_R_INTERNAL_ERROR);
			return NULL;
			}
                ret+=el;
                }
#endif

	if (((s->s3->tmp.new_cipher->id & 0xFFFF)==0x80 || (s->s3->tmp.new_cipher->id & 0xFFFF)==0x81) 
		&& (SSL_get_options(s) & SSL_OP_CRYPTOPRO_TLSEXT_BUG))
		{ const unsigned char cryptopro_ext[36] = {
			0xfd, 0xe8, /*65000*/
			0x00, 0x20, /*32 bytes length*/
			0x30, 0x1e, 0x30, 0x08, 0x06, 0x06, 0x2a, 0x85, 
			0x03,   0x02, 0x02, 0x09, 0x30, 0x08, 0x06, 0x06, 
			0x2a, 0x85, 0x03, 0x02, 0x02, 0x16, 0x30, 0x08, 
			0x06, 0x06, 0x2a, 0x85, 0x03, 0x02, 0x02, 0x17};
			if (limit-ret<36) return NULL;
			memcpy(ret,cryptopro_ext,36);
			ret+=36;

		}

#ifndef OPENSSL_NO_HEARTBEATS
	/* Add Heartbeat extension if we've received one */
	if (s->tlsext_heartbeat & SSL_TLSEXT_HB_ENABLED)
		{
		s2n(TLSEXT_TYPE_heartbeat,ret);
		s2n(1,ret);
		/* Set mode:
		 * 1: peer may send requests
		 * 2: peer not allowed to send requests
		 */
		if (s->tlsext_heartbeat & SSL_TLSEXT_HB_DONT_RECV_REQUESTS)
			*(ret++) = SSL_TLSEXT_HB_DONT_SEND_REQUESTS;
		else
			*(ret++) = SSL_TLSEXT_HB_ENABLED;

		}
#endif

#ifndef OPENSSL_NO_NEXTPROTONEG
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
#endif

	if ((extdatalen = ret-p-2)== 0) 
		return p;

	s2n(extdatalen,p);
	return ret;
	}

#ifndef OPENSSL_NO_EC
/* ssl_check_for_safari attempts to fingerprint Safari using OS X
 * SecureTransport using the TLS extension block in |d|, of length |n|.
 * Safari, since 10.6, sends exactly these extensions, in this order:
 *   SNI,
 *   elliptic_curves
 *   ec_point_formats
 *
 * We wish to fingerprint Safari because they broke ECDHE-ECDSA support in 10.8,
 * but they advertise support. So enabling ECDHE-ECDSA ciphers breaks them.
 * Sadly we cannot differentiate 10.6, 10.7 and 10.8.4 (which work), from
 * 10.8..10.8.3 (which don't work).
 */
static void ssl_check_for_safari(SSL *s, const unsigned char *data, const unsigned char *d, int n) {
	unsigned short type, size;
	static const unsigned char kSafariExtensionsBlock[] = {
		0x00, 0x0a,  /* elliptic_curves extension */
		0x00, 0x08,  /* 8 bytes */
		0x00, 0x06,  /* 6 bytes of curve ids */
		0x00, 0x17,  /* P-256 */
		0x00, 0x18,  /* P-384 */
		0x00, 0x19,  /* P-521 */

		0x00, 0x0b,  /* ec_point_formats */
		0x00, 0x02,  /* 2 bytes */
		0x01,        /* 1 point format */
		0x00,        /* uncompressed */
	};

	/* The following is only present in TLS 1.2 */
	static const unsigned char kSafariTLS12ExtensionsBlock[] = {
		0x00, 0x0d,  /* signature_algorithms */
		0x00, 0x0c,  /* 12 bytes */
		0x00, 0x0a,  /* 10 bytes */
		0x05, 0x01,  /* SHA-384/RSA */
		0x04, 0x01,  /* SHA-256/RSA */
		0x02, 0x01,  /* SHA-1/RSA */
		0x04, 0x03,  /* SHA-256/ECDSA */
		0x02, 0x03,  /* SHA-1/ECDSA */
	};

	if (data >= (d+n-2))
		return;
	data += 2;

	if (data > (d+n-4))
		return;
	n2s(data,type);
	n2s(data,size);

	if (type != TLSEXT_TYPE_server_name)
		return;

	if (data+size > d+n)
		return;
	data += size;

	if (TLS1_get_client_version(s) >= TLS1_2_VERSION)
		{
		const size_t len1 = sizeof(kSafariExtensionsBlock);
		const size_t len2 = sizeof(kSafariTLS12ExtensionsBlock);

		if (data + len1 + len2 != d+n)
			return;
		if (memcmp(data, kSafariExtensionsBlock, len1) != 0)
			return;
		if (memcmp(data + len1, kSafariTLS12ExtensionsBlock, len2) != 0)
			return;
		}
	else
		{
		const size_t len = sizeof(kSafariExtensionsBlock);

		if (data + len != d+n)
			return;
		if (memcmp(data, kSafariExtensionsBlock, len) != 0)
			return;
		}

	s->s3->is_probably_safari = 1;
}
#endif /* !OPENSSL_NO_EC */

int ssl_parse_clienthello_tlsext(SSL *s, unsigned char **p, unsigned char *d, int n, int *al)
	{
	unsigned short type;
	unsigned short size;
	unsigned short len;
	unsigned char *data = *p;
	int renegotiate_seen = 0;
	int sigalg_seen = 0;

	s->servername_done = 0;
	s->tlsext_status_type = -1;
#ifndef OPENSSL_NO_NEXTPROTONEG
	s->s3->next_proto_neg_seen = 0;
#endif

#ifndef OPENSSL_NO_HEARTBEATS
	s->tlsext_heartbeat &= ~(SSL_TLSEXT_HB_ENABLED |
	                       SSL_TLSEXT_HB_DONT_SEND_REQUESTS);
#endif

#ifndef OPENSSL_NO_EC
	if (s->options & SSL_OP_SAFARI_ECDHE_ECDSA_BUG)
		ssl_check_for_safari(s, data, d, n);
#endif /* !OPENSSL_NO_EC */

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
#if 0
		fprintf(stderr,"Received extension type %d size %d\n",type,size);
#endif
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
#ifndef OPENSSL_NO_SRP
		else if (type == TLSEXT_TYPE_srp)
			{
			if (size <= 0 || ((len = data[0])) != (size -1))
				{
				*al = SSL_AD_DECODE_ERROR;
				return 0;
				}
			if (s->srp_ctx.login != NULL)
				{
				*al = SSL_AD_DECODE_ERROR;
				return 0;
				}
			if ((s->srp_ctx.login = OPENSSL_malloc(len+1)) == NULL)
				return -1;
			memcpy(s->srp_ctx.login, &data[1], len);
			s->srp_ctx.login[len]='\0';
  
			if (strlen(s->srp_ctx.login) != len) 
				{
				*al = SSL_AD_DECODE_ERROR;
				return 0;
				}
			}
#endif

#ifndef OPENSSL_NO_EC
		else if (type == TLSEXT_TYPE_ec_point_formats &&
	             s->version != DTLS1_VERSION)
			{
			unsigned char *sdata = data;
			int ecpointformatlist_length = *(sdata++);

			if (ecpointformatlist_length != size - 1)
				{
				*al = TLS1_AD_DECODE_ERROR;
				return 0;
				}
			if (!s->hit)
				{
				if(s->session->tlsext_ecpointformatlist)
					{
					OPENSSL_free(s->session->tlsext_ecpointformatlist);
					s->session->tlsext_ecpointformatlist = NULL;
					}
				s->session->tlsext_ecpointformatlist_length = 0;
				if ((s->session->tlsext_ecpointformatlist = OPENSSL_malloc(ecpointformatlist_length)) == NULL)
					{
					*al = TLS1_AD_INTERNAL_ERROR;
					return 0;
					}
				s->session->tlsext_ecpointformatlist_length = ecpointformatlist_length;
				memcpy(s->session->tlsext_ecpointformatlist, sdata, ecpointformatlist_length);
				}
#if 0
			fprintf(stderr,"ssl_parse_clienthello_tlsext s->session->tlsext_ecpointformatlist (length=%i) ", s->session->tlsext_ecpointformatlist_length);
			sdata = s->session->tlsext_ecpointformatlist;
			for (i = 0; i < s->session->tlsext_ecpointformatlist_length; i++)
				fprintf(stderr,"%i ",*(sdata++));
			fprintf(stderr,"\n");
#endif
			}
		else if (type == TLSEXT_TYPE_elliptic_curves &&
	             s->version != DTLS1_VERSION)
			{
			unsigned char *sdata = data;
			int ellipticcurvelist_length = (*(sdata++) << 8);
			ellipticcurvelist_length += (*(sdata++));

			if (ellipticcurvelist_length != size - 2 ||
				ellipticcurvelist_length < 1)
				{
				*al = TLS1_AD_DECODE_ERROR;
				return 0;
				}
			if (!s->hit)
				{
				if(s->session->tlsext_ellipticcurvelist)
					{
					*al = TLS1_AD_DECODE_ERROR;
					return 0;
					}
				s->session->tlsext_ellipticcurvelist_length = 0;
				if ((s->session->tlsext_ellipticcurvelist = OPENSSL_malloc(ellipticcurvelist_length)) == NULL)
					{
					*al = TLS1_AD_INTERNAL_ERROR;
					return 0;
					}
				s->session->tlsext_ellipticcurvelist_length = ellipticcurvelist_length;
				memcpy(s->session->tlsext_ellipticcurvelist, sdata, ellipticcurvelist_length);
				}
#if 0
			fprintf(stderr,"ssl_parse_clienthello_tlsext s->session->tlsext_ellipticcurvelist (length=%i) ", s->session->tlsext_ellipticcurvelist_length);
			sdata = s->session->tlsext_ellipticcurvelist;
			for (i = 0; i < s->session->tlsext_ellipticcurvelist_length; i++)
				fprintf(stderr,"%i ",*(sdata++));
			fprintf(stderr,"\n");
#endif
			}
#endif /* OPENSSL_NO_EC */
#ifdef TLSEXT_TYPE_opaque_prf_input
		else if (type == TLSEXT_TYPE_opaque_prf_input &&
	             s->version != DTLS1_VERSION)
			{
			unsigned char *sdata = data;

			if (size < 2)
				{
				*al = SSL_AD_DECODE_ERROR;
				return 0;
				}
			n2s(sdata, s->s3->client_opaque_prf_input_len);
			if (s->s3->client_opaque_prf_input_len != size - 2)
				{
				*al = SSL_AD_DECODE_ERROR;
				return 0;
				}

			if (s->s3->client_opaque_prf_input != NULL) /* shouldn't really happen */
				OPENSSL_free(s->s3->client_opaque_prf_input);
			if (s->s3->client_opaque_prf_input_len == 0)
				s->s3->client_opaque_prf_input = OPENSSL_malloc(1); /* dummy byte just to get non-NULL */
			else
				s->s3->client_opaque_prf_input = BUF_memdup(sdata, s->s3->client_opaque_prf_input_len);
			if (s->s3->client_opaque_prf_input == NULL)
				{
				*al = TLS1_AD_INTERNAL_ERROR;
				return 0;
				}
			}
#endif
		else if (type == TLSEXT_TYPE_session_ticket)
			{
			if (s->tls_session_ticket_ext_cb &&
			    !s->tls_session_ticket_ext_cb(s, data, size, s->tls_session_ticket_ext_cb_arg))
				{
				*al = TLS1_AD_INTERNAL_ERROR;
				return 0;
				}
			}
		else if (type == TLSEXT_TYPE_renegotiate)
			{
			if(!ssl_parse_clienthello_renegotiate_ext(s, data, size, al))
				return 0;
			renegotiate_seen = 1;
			}
		else if (type == TLSEXT_TYPE_signature_algorithms)
			{
			int dsize;
			if (sigalg_seen || size < 2) 
				{
				*al = SSL_AD_DECODE_ERROR;
				return 0;
				}
			sigalg_seen = 1;
			n2s(data,dsize);
			size -= 2;
			if (dsize != size || dsize & 1) 
				{
				*al = SSL_AD_DECODE_ERROR;
				return 0;
				}
			if (!tls1_process_sigalgs(s, data, dsize))
				{
				*al = SSL_AD_DECODE_ERROR;
				return 0;
				}
			}
		else if (type == TLSEXT_TYPE_status_request &&
		         s->version != DTLS1_VERSION)
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
					if (s->tlsext_ocsp_exts)
						{
						sk_X509_EXTENSION_pop_free(s->tlsext_ocsp_exts,
									   X509_EXTENSION_free);
						}

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
#ifndef OPENSSL_NO_HEARTBEATS
		else if (type == TLSEXT_TYPE_heartbeat)
			{
			switch(data[0])
				{
				case 0x01:	/* Client allows us to send HB requests */
							s->tlsext_heartbeat |= SSL_TLSEXT_HB_ENABLED;
							break;
				case 0x02:	/* Client doesn't accept HB requests */
							s->tlsext_heartbeat |= SSL_TLSEXT_HB_ENABLED;
							s->tlsext_heartbeat |= SSL_TLSEXT_HB_DONT_SEND_REQUESTS;
							break;
				default:	*al = SSL_AD_ILLEGAL_PARAMETER;
							return 0;
				}
			}
#endif
#ifndef OPENSSL_NO_NEXTPROTONEG
		else if (type == TLSEXT_TYPE_next_proto_neg &&
			 s->s3->tmp.finish_md_len == 0)
			{
			/* We shouldn't accept this extension on a
			 * renegotiation.
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
#endif

		/* session ticket processed earlier */
#ifndef OPENSSL_NO_SRTP
		else if (type == TLSEXT_TYPE_use_srtp)
			{
			if(ssl_parse_clienthello_use_srtp_ext(s, data, size,
							      al))
				return 0;
			}
#endif

		data+=size;
		}
				
	*p = data;

	ri_check:

	/* Need RI if renegotiating */

	if (!renegotiate_seen && s->renegotiate &&
		!(s->options & SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION))
		{
		*al = SSL_AD_HANDSHAKE_FAILURE;
	 	SSLerr(SSL_F_SSL_PARSE_CLIENTHELLO_TLSEXT,
				SSL_R_UNSAFE_LEGACY_RENEGOTIATION_DISABLED);
		return 0;
		}

	return 1;
	}

#ifndef OPENSSL_NO_NEXTPROTONEG
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
#endif

int ssl_parse_serverhello_tlsext(SSL *s, unsigned char **p, unsigned char *d, int n, int *al)
	{
	unsigned short length;
	unsigned short type;
	unsigned short size;
	unsigned char *data = *p;
	int tlsext_servername = 0;
	int renegotiate_seen = 0;

#ifndef OPENSSL_NO_NEXTPROTONEG
	s->s3->next_proto_neg_seen = 0;
#endif

#ifndef OPENSSL_NO_HEARTBEATS
	s->tlsext_heartbeat &= ~(SSL_TLSEXT_HB_ENABLED |
	                       SSL_TLSEXT_HB_DONT_SEND_REQUESTS);
#endif

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

#ifndef OPENSSL_NO_EC
		else if (type == TLSEXT_TYPE_ec_point_formats &&
	             s->version != DTLS1_VERSION)
			{
			unsigned char *sdata = data;
			int ecpointformatlist_length = *(sdata++);

			if (ecpointformatlist_length != size - 1 || 
				ecpointformatlist_length < 1)
				{
				*al = TLS1_AD_DECODE_ERROR;
				return 0;
				}
			s->session->tlsext_ecpointformatlist_length = 0;
			if (s->session->tlsext_ecpointformatlist != NULL) OPENSSL_free(s->session->tlsext_ecpointformatlist);
			if ((s->session->tlsext_ecpointformatlist = OPENSSL_malloc(ecpointformatlist_length)) == NULL)
				{
				*al = TLS1_AD_INTERNAL_ERROR;
				return 0;
				}
			s->session->tlsext_ecpointformatlist_length = ecpointformatlist_length;
			memcpy(s->session->tlsext_ecpointformatlist, sdata, ecpointformatlist_length);
#if 0
			fprintf(stderr,"ssl_parse_serverhello_tlsext s->session->tlsext_ecpointformatlist ");
			sdata = s->session->tlsext_ecpointformatlist;
			for (i = 0; i < s->session->tlsext_ecpointformatlist_length; i++)
				fprintf(stderr,"%i ",*(sdata++));
			fprintf(stderr,"\n");
#endif
			}
#endif /* OPENSSL_NO_EC */

		else if (type == TLSEXT_TYPE_session_ticket)
			{
			if (s->tls_session_ticket_ext_cb &&
			    !s->tls_session_ticket_ext_cb(s, data, size, s->tls_session_ticket_ext_cb_arg))
				{
				*al = TLS1_AD_INTERNAL_ERROR;
				return 0;
				}
			if ((SSL_get_options(s) & SSL_OP_NO_TICKET)
				|| (size > 0))
				{
				*al = TLS1_AD_UNSUPPORTED_EXTENSION;
				return 0;
				}
			s->tlsext_ticket_expected = 1;
			}
#ifdef TLSEXT_TYPE_opaque_prf_input
		else if (type == TLSEXT_TYPE_opaque_prf_input &&
	             s->version != DTLS1_VERSION)
			{
			unsigned char *sdata = data;

			if (size < 2)
				{
				*al = SSL_AD_DECODE_ERROR;
				return 0;
				}
			n2s(sdata, s->s3->server_opaque_prf_input_len);
			if (s->s3->server_opaque_prf_input_len != size - 2)
				{
				*al = SSL_AD_DECODE_ERROR;
				return 0;
				}
			
			if (s->s3->server_opaque_prf_input != NULL) /* shouldn't really happen */
				OPENSSL_free(s->s3->server_opaque_prf_input);
			if (s->s3->server_opaque_prf_input_len == 0)
				s->s3->server_opaque_prf_input = OPENSSL_malloc(1); /* dummy byte just to get non-NULL */
			else
				s->s3->server_opaque_prf_input = BUF_memdup(sdata, s->s3->server_opaque_prf_input_len);

			if (s->s3->server_opaque_prf_input == NULL)
				{
				*al = TLS1_AD_INTERNAL_ERROR;
				return 0;
				}
			}
#endif
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
#ifndef OPENSSL_NO_NEXTPROTONEG
		else if (type == TLSEXT_TYPE_next_proto_neg &&
			 s->s3->tmp.finish_md_len == 0)
			{
			unsigned char *selected;
			unsigned char selected_len;

			/* We must have requested it. */
			if (s->ctx->next_proto_select_cb == NULL)
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
			s->s3->next_proto_neg_seen = 1;
			}
#endif
		else if (type == TLSEXT_TYPE_renegotiate)
			{
			if(!ssl_parse_serverhello_renegotiate_ext(s, data, size, al))
				return 0;
			renegotiate_seen = 1;
			}
#ifndef OPENSSL_NO_HEARTBEATS
		else if (type == TLSEXT_TYPE_heartbeat)
			{
			switch(data[0])
				{
				case 0x01:	/* Server allows us to send HB requests */
							s->tlsext_heartbeat |= SSL_TLSEXT_HB_ENABLED;
							break;
				case 0x02:	/* Server doesn't accept HB requests */
							s->tlsext_heartbeat |= SSL_TLSEXT_HB_ENABLED;
							s->tlsext_heartbeat |= SSL_TLSEXT_HB_DONT_SEND_REQUESTS;
							break;
				default:	*al = SSL_AD_ILLEGAL_PARAMETER;
							return 0;
				}
			}
#endif
#ifndef OPENSSL_NO_SRTP
		else if (type == TLSEXT_TYPE_use_srtp)
			{
                        if(ssl_parse_serverhello_use_srtp_ext(s, data, size,
							      al))
                                return 0;
			}
#endif

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


int ssl_prepare_clienthello_tlsext(SSL *s)
	{
#ifndef OPENSSL_NO_EC
	/* If we are client and using an elliptic curve cryptography cipher suite, send the point formats 
	 * and elliptic curves we support.
	 */
	int using_ecc = 0;
	int i;
	unsigned char *j;
	unsigned long alg_k, alg_a;
	STACK_OF(SSL_CIPHER) *cipher_stack = SSL_get_ciphers(s);

	for (i = 0; i < sk_SSL_CIPHER_num(cipher_stack); i++)
		{
		SSL_CIPHER *c = sk_SSL_CIPHER_value(cipher_stack, i);

		alg_k = c->algorithm_mkey;
		alg_a = c->algorithm_auth;
		if ((alg_k & (SSL_kEECDH|SSL_kECDHr|SSL_kECDHe) || (alg_a & SSL_aECDSA)))
			{
			using_ecc = 1;
			break;
			}
		}
	using_ecc = using_ecc && (s->version >= TLS1_VERSION);
	if (using_ecc)
		{
		if (s->tlsext_ecpointformatlist != NULL) OPENSSL_free(s->tlsext_ecpointformatlist);
		if ((s->tlsext_ecpointformatlist = OPENSSL_malloc(3)) == NULL)
			{
			SSLerr(SSL_F_SSL_PREPARE_CLIENTHELLO_TLSEXT,ERR_R_MALLOC_FAILURE);
			return -1;
			}
		s->tlsext_ecpointformatlist_length = 3;
		s->tlsext_ecpointformatlist[0] = TLSEXT_ECPOINTFORMAT_uncompressed;
		s->tlsext_ecpointformatlist[1] = TLSEXT_ECPOINTFORMAT_ansiX962_compressed_prime;
		s->tlsext_ecpointformatlist[2] = TLSEXT_ECPOINTFORMAT_ansiX962_compressed_char2;

		/* we support all named elliptic curves in draft-ietf-tls-ecc-12 */
		if (s->tlsext_ellipticcurvelist != NULL) OPENSSL_free(s->tlsext_ellipticcurvelist);
		s->tlsext_ellipticcurvelist_length = sizeof(pref_list)/sizeof(pref_list[0]) * 2;
		if ((s->tlsext_ellipticcurvelist = OPENSSL_malloc(s->tlsext_ellipticcurvelist_length)) == NULL)
			{
			s->tlsext_ellipticcurvelist_length = 0;
			SSLerr(SSL_F_SSL_PREPARE_CLIENTHELLO_TLSEXT,ERR_R_MALLOC_FAILURE);
			return -1;
			}
		for (i = 0, j = s->tlsext_ellipticcurvelist; (unsigned int)i <
				sizeof(pref_list)/sizeof(pref_list[0]); i++)
			{
			int id = tls1_ec_nid2curve_id(pref_list[i]);
			s2n(id,j);
			}
		}
#endif /* OPENSSL_NO_EC */

#ifdef TLSEXT_TYPE_opaque_prf_input
 	{
		int r = 1;
	
		if (s->ctx->tlsext_opaque_prf_input_callback != 0)
			{
			r = s->ctx->tlsext_opaque_prf_input_callback(s, NULL, 0, s->ctx->tlsext_opaque_prf_input_callback_arg);
			if (!r)
				return -1;
			}

		if (s->tlsext_opaque_prf_input != NULL)
			{
			if (s->s3->client_opaque_prf_input != NULL) /* shouldn't really happen */
				OPENSSL_free(s->s3->client_opaque_prf_input);

			if (s->tlsext_opaque_prf_input_len == 0)
				s->s3->client_opaque_prf_input = OPENSSL_malloc(1); /* dummy byte just to get non-NULL */
			else
				s->s3->client_opaque_prf_input = BUF_memdup(s->tlsext_opaque_prf_input, s->tlsext_opaque_prf_input_len);
			if (s->s3->client_opaque_prf_input == NULL)
				{
				SSLerr(SSL_F_SSL_PREPARE_CLIENTHELLO_TLSEXT,ERR_R_MALLOC_FAILURE);
				return -1;
				}
			s->s3->client_opaque_prf_input_len = s->tlsext_opaque_prf_input_len;
			}

		if (r == 2)
			/* at callback's request, insist on receiving an appropriate server opaque PRF input */
			s->s3->server_opaque_prf_input_len = s->tlsext_opaque_prf_input_len;
	}
#endif

	return 1;
	}

int ssl_prepare_serverhello_tlsext(SSL *s)
	{
#ifndef OPENSSL_NO_EC
	/* If we are server and using an ECC cipher suite, send the point formats we support 
	 * if the client sent us an ECPointsFormat extension.  Note that the server is not
	 * supposed to send an EllipticCurves extension.
	 */

	unsigned long alg_k = s->s3->tmp.new_cipher->algorithm_mkey;
	unsigned long alg_a = s->s3->tmp.new_cipher->algorithm_auth;
	int using_ecc = (alg_k & (SSL_kEECDH|SSL_kECDHr|SSL_kECDHe)) || (alg_a & SSL_aECDSA);
	using_ecc = using_ecc && (s->session->tlsext_ecpointformatlist != NULL);
	
	if (using_ecc)
		{
		if (s->tlsext_ecpointformatlist != NULL) OPENSSL_free(s->tlsext_ecpointformatlist);
		if ((s->tlsext_ecpointformatlist = OPENSSL_malloc(3)) == NULL)
			{
			SSLerr(SSL_F_SSL_PREPARE_SERVERHELLO_TLSEXT,ERR_R_MALLOC_FAILURE);
			return -1;
			}
		s->tlsext_ecpointformatlist_length = 3;
		s->tlsext_ecpointformatlist[0] = TLSEXT_ECPOINTFORMAT_uncompressed;
		s->tlsext_ecpointformatlist[1] = TLSEXT_ECPOINTFORMAT_ansiX962_compressed_prime;
		s->tlsext_ecpointformatlist[2] = TLSEXT_ECPOINTFORMAT_ansiX962_compressed_char2;
		}
#endif /* OPENSSL_NO_EC */

	return 1;
	}

int ssl_check_clienthello_tlsext_early(SSL *s)
	{
	int ret=SSL_TLSEXT_ERR_NOACK;
	int al = SSL_AD_UNRECOGNIZED_NAME;

#ifndef OPENSSL_NO_EC
	/* The handling of the ECPointFormats extension is done elsewhere, namely in 
	 * ssl3_choose_cipher in s3_lib.c.
	 */
	/* The handling of the EllipticCurves extension is done elsewhere, namely in 
	 * ssl3_choose_cipher in s3_lib.c.
	 */
#endif

	if (s->ctx != NULL && s->ctx->tlsext_servername_callback != 0) 
		ret = s->ctx->tlsext_servername_callback(s, &al, s->ctx->tlsext_servername_arg);
	else if (s->initial_ctx != NULL && s->initial_ctx->tlsext_servername_callback != 0) 		
		ret = s->initial_ctx->tlsext_servername_callback(s, &al, s->initial_ctx->tlsext_servername_arg);

#ifdef TLSEXT_TYPE_opaque_prf_input
 	{
		/* This sort of belongs into ssl_prepare_serverhello_tlsext(),
		 * but we might be sending an alert in response to the client hello,
		 * so this has to happen here in
		 * ssl_check_clienthello_tlsext_early(). */

		int r = 1;
	
		if (s->ctx->tlsext_opaque_prf_input_callback != 0)
			{
			r = s->ctx->tlsext_opaque_prf_input_callback(s, NULL, 0, s->ctx->tlsext_opaque_prf_input_callback_arg);
			if (!r)
				{
				ret = SSL_TLSEXT_ERR_ALERT_FATAL;
				al = SSL_AD_INTERNAL_ERROR;
				goto err;
				}
			}

		if (s->s3->server_opaque_prf_input != NULL) /* shouldn't really happen */
			OPENSSL_free(s->s3->server_opaque_prf_input);
		s->s3->server_opaque_prf_input = NULL;

		if (s->tlsext_opaque_prf_input != NULL)
			{
			if (s->s3->client_opaque_prf_input != NULL &&
				s->s3->client_opaque_prf_input_len == s->tlsext_opaque_prf_input_len)
				{
				/* can only use this extension if we have a server opaque PRF input
				 * of the same length as the client opaque PRF input! */

				if (s->tlsext_opaque_prf_input_len == 0)
					s->s3->server_opaque_prf_input = OPENSSL_malloc(1); /* dummy byte just to get non-NULL */
				else
					s->s3->server_opaque_prf_input = BUF_memdup(s->tlsext_opaque_prf_input, s->tlsext_opaque_prf_input_len);
				if (s->s3->server_opaque_prf_input == NULL)
					{
					ret = SSL_TLSEXT_ERR_ALERT_FATAL;
					al = SSL_AD_INTERNAL_ERROR;
					goto err;
					}
				s->s3->server_opaque_prf_input_len = s->tlsext_opaque_prf_input_len;
				}
			}

		if (r == 2 && s->s3->server_opaque_prf_input == NULL)
			{
			/* The callback wants to enforce use of the extension,
			 * but we can't do that with the client opaque PRF input;
			 * abort the handshake.
			 */
			ret = SSL_TLSEXT_ERR_ALERT_FATAL;
			al = SSL_AD_HANDSHAKE_FAILURE;
			}
	}

 err:
#endif
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

int ssl_check_clienthello_tlsext_late(SSL *s)
	{
	int ret = SSL_TLSEXT_ERR_OK;
	int al;

	/* If status request then ask callback what to do.
 	 * Note: this must be called after servername callbacks in case 
 	 * the certificate has changed, and must be called after the cipher
	 * has been chosen because this may influence which certificate is sent
 	 */
	if ((s->tlsext_status_type != -1) && s->ctx && s->ctx->tlsext_status_cb)
		{
		int r;
		CERT_PKEY *certpkey;
		certpkey = ssl_get_server_send_pkey(s);
		/* If no certificate can't return certificate status */
		if (certpkey == NULL)
			{
			s->tlsext_status_expected = 0;
			return 1;
			}
		/* Set current certificate to one we will use so
		 * SSL_get_certificate et al can pick it up.
		 */
		s->cert->key = certpkey;
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

		default:
			return 1;
		}
	}

int ssl_check_serverhello_tlsext(SSL *s)
	{
	int ret=SSL_TLSEXT_ERR_NOACK;
	int al = SSL_AD_UNRECOGNIZED_NAME;

#ifndef OPENSSL_NO_EC
	/* If we are client and using an elliptic curve cryptography cipher
	 * suite, then if server returns an EC point formats lists extension
	 * it must contain uncompressed.
	 */
	unsigned long alg_k = s->s3->tmp.new_cipher->algorithm_mkey;
	unsigned long alg_a = s->s3->tmp.new_cipher->algorithm_auth;
	if ((s->tlsext_ecpointformatlist != NULL) && (s->tlsext_ecpointformatlist_length > 0) && 
	    (s->session->tlsext_ecpointformatlist != NULL) && (s->session->tlsext_ecpointformatlist_length > 0) && 
	    ((alg_k & (SSL_kEECDH|SSL_kECDHr|SSL_kECDHe)) || (alg_a & SSL_aECDSA)))
		{
		/* we are using an ECC cipher */
		size_t i;
		unsigned char *list;
		int found_uncompressed = 0;
		list = s->session->tlsext_ecpointformatlist;
		for (i = 0; i < s->session->tlsext_ecpointformatlist_length; i++)
			{
			if (*(list++) == TLSEXT_ECPOINTFORMAT_uncompressed)
				{
				found_uncompressed = 1;
				break;
				}
			}
		if (!found_uncompressed)
			{
			SSLerr(SSL_F_SSL_CHECK_SERVERHELLO_TLSEXT,SSL_R_TLS_INVALID_ECPOINTFORMAT_LIST);
			return -1;
			}
		}
	ret = SSL_TLSEXT_ERR_OK;
#endif /* OPENSSL_NO_EC */

	if (s->ctx != NULL && s->ctx->tlsext_servername_callback != 0) 
		ret = s->ctx->tlsext_servername_callback(s, &al, s->ctx->tlsext_servername_arg);
	else if (s->initial_ctx != NULL && s->initial_ctx->tlsext_servername_callback != 0) 		
		ret = s->initial_ctx->tlsext_servername_callback(s, &al, s->initial_ctx->tlsext_servername_arg);

#ifdef TLSEXT_TYPE_opaque_prf_input
	if (s->s3->server_opaque_prf_input_len > 0)
		{
		/* This case may indicate that we, as a client, want to insist on using opaque PRF inputs.
		 * So first verify that we really have a value from the server too. */

		if (s->s3->server_opaque_prf_input == NULL)
			{
			ret = SSL_TLSEXT_ERR_ALERT_FATAL;
			al = SSL_AD_HANDSHAKE_FAILURE;
			}
		
		/* Anytime the server *has* sent an opaque PRF input, we need to check
		 * that we have a client opaque PRF input of the same size. */
		if (s->s3->client_opaque_prf_input == NULL ||
		    s->s3->client_opaque_prf_input_len != s->s3->server_opaque_prf_input_len)
			{
			ret = SSL_TLSEXT_ERR_ALERT_FATAL;
			al = SSL_AD_ILLEGAL_PARAMETER;
			}
		}
#endif

	/* If we've requested certificate status and we wont get one
 	 * tell the callback
 	 */
	if ((s->tlsext_status_type != -1) && !(s->tlsext_status_expected)
			&& s->ctx && s->ctx->tlsext_status_cb)
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

/* Since the server cache lookup is done early on in the processing of the
 * ClientHello, and other operations depend on the result, we need to handle
 * any TLS session ticket extension at the same time.
 *
 *   session_id: points at the session ID in the ClientHello. This code will
 *       read past the end of this in order to parse out the session ticket
 *       extension, if any.
 *   len: the length of the session ID.
 *   limit: a pointer to the first byte after the ClientHello.
 *   ret: (output) on return, if a ticket was decrypted, then this is set to
 *       point to the resulting session.
 *
 * If s->tls_session_secret_cb is set then we are expecting a pre-shared key
 * ciphersuite, in which case we have no use for session tickets and one will
 * never be decrypted, nor will s->tlsext_ticket_expected be set to 1.
 *
 * Returns:
 *   -1: fatal error, either from parsing or decrypting the ticket.
 *    0: no ticket was found (or was ignored, based on settings).
 *    1: a zero length extension was found, indicating that the client supports
 *       session tickets but doesn't currently have one to offer.
 *    2: either s->tls_session_secret_cb was set, or a ticket was offered but
 *       couldn't be decrypted because of a non-fatal error.
 *    3: a ticket was successfully decrypted and *ret was set.
 *
 * Side effects:
 *   Sets s->tlsext_ticket_expected to 1 if the server will have to issue
 *   a new session ticket to the client because the client indicated support
 *   (and s->tls_session_secret_cb is NULL) but the client either doesn't have
 *   a session ticket or we couldn't use the one it gave us, or if
 *   s->ctx->tlsext_ticket_key_cb asked to renew the client's ticket.
 *   Otherwise, s->tlsext_ticket_expected is set to 0.
 */
int tls1_process_ticket(SSL *s, unsigned char *session_id, int len,
			const unsigned char *limit, SSL_SESSION **ret)
	{
	/* Point after session ID in client hello */
	const unsigned char *p = session_id + len;
	unsigned short i;

	*ret = NULL;
	s->tlsext_ticket_expected = 0;

	/* If tickets disabled behave as if no ticket present
	 * to permit stateful resumption.
	 */
	if (SSL_get_options(s) & SSL_OP_NO_TICKET)
		return 0;
	if ((s->version <= SSL3_VERSION) || !limit)
		return 0;
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
		return 0;
	n2s(p, i);
	while ((p + 4) <= limit)
		{
		unsigned short type, size;
		n2s(p, type);
		n2s(p, size);
		if (p + size > limit)
			return 0;
		if (type == TLSEXT_TYPE_session_ticket)
			{
			int r;
			if (size == 0)
				{
				/* The client will accept a ticket but doesn't
				 * currently have one. */
				s->tlsext_ticket_expected = 1;
				return 1;
				}
			if (s->tls_session_secret_cb)
				{
				/* Indicate that the ticket couldn't be
				 * decrypted rather than generating the session
				 * from ticket now, trigger abbreviated
				 * handshake based on external mechanism to
				 * calculate the master secret later. */
				return 2;
				}
			r = tls_decrypt_ticket(s, p, size, session_id, len, ret);
			switch (r)
				{
				case 2: /* ticket couldn't be decrypted */
					s->tlsext_ticket_expected = 1;
					return 2;
				case 3: /* ticket was decrypted */
					return r;
				case 4: /* ticket decrypted but need to renew */
					s->tlsext_ticket_expected = 1;
					return 3;
				default: /* fatal error */
					return -1;
				}
			}
		p += size;
		}
	return 0;
	}

/* tls_decrypt_ticket attempts to decrypt a session ticket.
 *
 *   etick: points to the body of the session ticket extension.
 *   eticklen: the length of the session tickets extenion.
 *   sess_id: points at the session ID.
 *   sesslen: the length of the session ID.
 *   psess: (output) on return, if a ticket was decrypted, then this is set to
 *       point to the resulting session.
 *
 * Returns:
 *   -1: fatal error, either from parsing or decrypting the ticket.
 *    2: the ticket couldn't be decrypted.
 *    3: a ticket was successfully decrypted and *psess was set.
 *    4: same as 3, but the ticket needs to be renewed.
 */
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
		return 2;
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
			return 2;
		if (rv == 2)
			renew_ticket = 1;
		}
	else
		{
		/* Check key name matches */
		if (memcmp(etick, tctx->tlsext_tick_key_name, 16))
			return 2;
		HMAC_Init_ex(&hctx, tctx->tlsext_tick_hmac_key, 16,
					tlsext_tick_md(), NULL);
		EVP_DecryptInit_ex(&ctx, EVP_aes_128_cbc(), NULL,
				tctx->tlsext_tick_aes_key, etick + 16);
		}
	/* Attempt to process session ticket, first conduct sanity and
	 * integrity checks on ticket.
	 */
	mlen = HMAC_size(&hctx);
	if (mlen < 0)
		{
		EVP_CIPHER_CTX_cleanup(&ctx);
		return -1;
		}
	eticklen -= mlen;
	/* Check HMAC of encrypted ticket */
	HMAC_Update(&hctx, etick, eticklen);
	HMAC_Final(&hctx, tick_hmac, NULL);
	HMAC_CTX_cleanup(&hctx);
	if (CRYPTO_memcmp(tick_hmac, etick + eticklen, mlen))
		return 2;
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
		return 2;
	slen += mlen;
	EVP_CIPHER_CTX_cleanup(&ctx);
	p = sdec;

	sess = d2i_SSL_SESSION(NULL, &p, slen);
	OPENSSL_free(sdec);
	if (sess)
		{
		/* The session ID, if non-empty, is used by some clients to
		 * detect that the ticket has been accepted. So we copy it to
		 * the session structure. If it is empty set length to zero
		 * as required by standard.
		 */
		if (sesslen)
			memcpy(sess->session_id, sess_id, sesslen);
		sess->session_id_length = sesslen;
		*psess = sess;
		if (renew_ticket)
			return 4;
		else
			return 3;
		}
        ERR_clear_error();
	/* For session parse failure, indicate that we need to send a new
	 * ticket. */
	return 2;
	}

/* Tables to translate from NIDs to TLS v1.2 ids */

typedef struct 
	{
	int nid;
	int id;
	} tls12_lookup;

static tls12_lookup tls12_md[] = {
#ifndef OPENSSL_NO_MD5
	{NID_md5, TLSEXT_hash_md5},
#endif
#ifndef OPENSSL_NO_SHA
	{NID_sha1, TLSEXT_hash_sha1},
#endif
#ifndef OPENSSL_NO_SHA256
	{NID_sha224, TLSEXT_hash_sha224},
	{NID_sha256, TLSEXT_hash_sha256},
#endif
#ifndef OPENSSL_NO_SHA512
	{NID_sha384, TLSEXT_hash_sha384},
	{NID_sha512, TLSEXT_hash_sha512}
#endif
};

static tls12_lookup tls12_sig[] = {
#ifndef OPENSSL_NO_RSA
	{EVP_PKEY_RSA, TLSEXT_signature_rsa},
#endif
#ifndef OPENSSL_NO_DSA
	{EVP_PKEY_DSA, TLSEXT_signature_dsa},
#endif
#ifndef OPENSSL_NO_ECDSA
	{EVP_PKEY_EC, TLSEXT_signature_ecdsa}
#endif
};

static int tls12_find_id(int nid, tls12_lookup *table, size_t tlen)
	{
	size_t i;
	for (i = 0; i < tlen; i++)
		{
		if (table[i].nid == nid)
			return table[i].id;
		}
	return -1;
	}
#if 0
static int tls12_find_nid(int id, tls12_lookup *table, size_t tlen)
	{
	size_t i;
	for (i = 0; i < tlen; i++)
		{
		if (table[i].id == id)
			return table[i].nid;
		}
	return -1;
	}
#endif

int tls12_get_sigandhash(unsigned char *p, const EVP_PKEY *pk, const EVP_MD *md)
	{
	int sig_id, md_id;
	if (!md)
		return 0;
	md_id = tls12_find_id(EVP_MD_type(md), tls12_md,
				sizeof(tls12_md)/sizeof(tls12_lookup));
	if (md_id == -1)
		return 0;
	sig_id = tls12_get_sigid(pk);
	if (sig_id == -1)
		return 0;
	p[0] = (unsigned char)md_id;
	p[1] = (unsigned char)sig_id;
	return 1;
	}

int tls12_get_sigid(const EVP_PKEY *pk)
	{
	return tls12_find_id(pk->type, tls12_sig,
				sizeof(tls12_sig)/sizeof(tls12_lookup));
	}

const EVP_MD *tls12_get_hash(unsigned char hash_alg)
	{
	switch(hash_alg)
		{
#ifndef OPENSSL_NO_SHA
		case TLSEXT_hash_sha1:
		return EVP_sha1();
#endif
#ifndef OPENSSL_NO_SHA256
		case TLSEXT_hash_sha224:
		return EVP_sha224();

		case TLSEXT_hash_sha256:
		return EVP_sha256();
#endif
#ifndef OPENSSL_NO_SHA512
		case TLSEXT_hash_sha384:
		return EVP_sha384();

		case TLSEXT_hash_sha512:
		return EVP_sha512();
#endif
		default:
		return NULL;

		}
	}

/* Set preferred digest for each key type */

int tls1_process_sigalgs(SSL *s, const unsigned char *data, int dsize)
	{
	int i, idx;
	const EVP_MD *md;
	CERT *c = s->cert;
	/* Extension ignored for TLS versions below 1.2 */
	if (TLS1_get_version(s) < TLS1_2_VERSION)
		return 1;
	/* Should never happen */
	if (!c)
		return 0;

	c->pkeys[SSL_PKEY_DSA_SIGN].digest = NULL;
	c->pkeys[SSL_PKEY_RSA_SIGN].digest = NULL;
	c->pkeys[SSL_PKEY_RSA_ENC].digest = NULL;
	c->pkeys[SSL_PKEY_ECC].digest = NULL;

	for (i = 0; i < dsize; i += 2)
		{
		unsigned char hash_alg = data[i], sig_alg = data[i+1];

		switch(sig_alg)
			{
#ifndef OPENSSL_NO_RSA
			case TLSEXT_signature_rsa:
			idx = SSL_PKEY_RSA_SIGN;
			break;
#endif
#ifndef OPENSSL_NO_DSA
			case TLSEXT_signature_dsa:
			idx = SSL_PKEY_DSA_SIGN;
			break;
#endif
#ifndef OPENSSL_NO_ECDSA
			case TLSEXT_signature_ecdsa:
			idx = SSL_PKEY_ECC;
			break;
#endif
			default:
			continue;
			}

		if (c->pkeys[idx].digest == NULL)
			{
			md = tls12_get_hash(hash_alg);
			if (md)
				{
				c->pkeys[idx].digest = md;
				if (idx == SSL_PKEY_RSA_SIGN)
					c->pkeys[SSL_PKEY_RSA_ENC].digest = md;
				}
			}

		}


	/* Set any remaining keys to default values. NOTE: if alg is not
	 * supported it stays as NULL.
	 */
#ifndef OPENSSL_NO_DSA
	if (!c->pkeys[SSL_PKEY_DSA_SIGN].digest)
		c->pkeys[SSL_PKEY_DSA_SIGN].digest = EVP_sha1();
#endif
#ifndef OPENSSL_NO_RSA
	if (!c->pkeys[SSL_PKEY_RSA_SIGN].digest)
		{
		c->pkeys[SSL_PKEY_RSA_SIGN].digest = EVP_sha1();
		c->pkeys[SSL_PKEY_RSA_ENC].digest = EVP_sha1();
		}
#endif
#ifndef OPENSSL_NO_ECDSA
	if (!c->pkeys[SSL_PKEY_ECC].digest)
		c->pkeys[SSL_PKEY_ECC].digest = EVP_sha1();
#endif
	return 1;
	}

#endif

#ifndef OPENSSL_NO_HEARTBEATS
int
tls1_process_heartbeat(SSL *s)
	{
	unsigned char *p = &s->s3->rrec.data[0], *pl;
	unsigned short hbtype;
	unsigned int payload;
	unsigned int padding = 16; /* Use minimum padding */

	if (s->msg_callback)
		s->msg_callback(0, s->version, TLS1_RT_HEARTBEAT,
			&s->s3->rrec.data[0], s->s3->rrec.length,
			s, s->msg_callback_arg);

	/* Read type and payload length first */
	if (1 + 2 + 16 > s->s3->rrec.length)
		return 0; /* silently discard */
	hbtype = *p++;
	n2s(p, payload);
	if (1 + 2 + payload + 16 > s->s3->rrec.length)
		return 0; /* silently discard per RFC 6520 sec. 4 */
	pl = p;

	if (hbtype == TLS1_HB_REQUEST)
		{
		unsigned char *buffer, *bp;
		int r;

		/* Allocate memory for the response, size is 1 bytes
		 * message type, plus 2 bytes payload length, plus
		 * payload, plus padding
		 */
		buffer = OPENSSL_malloc(1 + 2 + payload + padding);
		bp = buffer;
		
		/* Enter response type, length and copy payload */
		*bp++ = TLS1_HB_RESPONSE;
		s2n(payload, bp);
		memcpy(bp, pl, payload);
		bp += payload;
		/* Random padding */
		RAND_pseudo_bytes(bp, padding);

		r = ssl3_write_bytes(s, TLS1_RT_HEARTBEAT, buffer, 3 + payload + padding);

		if (r >= 0 && s->msg_callback)
			s->msg_callback(1, s->version, TLS1_RT_HEARTBEAT,
				buffer, 3 + payload + padding,
				s, s->msg_callback_arg);

		OPENSSL_free(buffer);

		if (r < 0)
			return r;
		}
	else if (hbtype == TLS1_HB_RESPONSE)
		{
		unsigned int seq;
		
		/* We only send sequence numbers (2 bytes unsigned int),
		 * and 16 random bytes, so we just try to read the
		 * sequence number */
		n2s(pl, seq);
		
		if (payload == 18 && seq == s->tlsext_hb_seq)
			{
			s->tlsext_hb_seq++;
			s->tlsext_hb_pending = 0;
			}
		}

	return 0;
	}

int
tls1_heartbeat(SSL *s)
	{
	unsigned char *buf, *p;
	int ret;
	unsigned int payload = 18; /* Sequence number + random bytes */
	unsigned int padding = 16; /* Use minimum padding */

	/* Only send if peer supports and accepts HB requests... */
	if (!(s->tlsext_heartbeat & SSL_TLSEXT_HB_ENABLED) ||
	    s->tlsext_heartbeat & SSL_TLSEXT_HB_DONT_SEND_REQUESTS)
		{
		SSLerr(SSL_F_TLS1_HEARTBEAT,SSL_R_TLS_HEARTBEAT_PEER_DOESNT_ACCEPT);
		return -1;
		}

	/* ...and there is none in flight yet... */
	if (s->tlsext_hb_pending)
		{
		SSLerr(SSL_F_TLS1_HEARTBEAT,SSL_R_TLS_HEARTBEAT_PENDING);
		return -1;
		}
		
	/* ...and no handshake in progress. */
	if (SSL_in_init(s) || s->in_handshake)
		{
		SSLerr(SSL_F_TLS1_HEARTBEAT,SSL_R_UNEXPECTED_MESSAGE);
		return -1;
		}
		
	/* Check if padding is too long, payload and padding
	 * must not exceed 2^14 - 3 = 16381 bytes in total.
	 */
	OPENSSL_assert(payload + padding <= 16381);

	/* Create HeartBeat message, we just use a sequence number
	 * as payload to distuingish different messages and add
	 * some random stuff.
	 *  - Message Type, 1 byte
	 *  - Payload Length, 2 bytes (unsigned int)
	 *  - Payload, the sequence number (2 bytes uint)
	 *  - Payload, random bytes (16 bytes uint)
	 *  - Padding
	 */
	buf = OPENSSL_malloc(1 + 2 + payload + padding);
	p = buf;
	/* Message Type */
	*p++ = TLS1_HB_REQUEST;
	/* Payload length (18 bytes here) */
	s2n(payload, p);
	/* Sequence number */
	s2n(s->tlsext_hb_seq, p);
	/* 16 random bytes */
	RAND_pseudo_bytes(p, 16);
	p += 16;
	/* Random padding */
	RAND_pseudo_bytes(p, padding);

	ret = ssl3_write_bytes(s, TLS1_RT_HEARTBEAT, buf, 3 + payload + padding);
	if (ret >= 0)
		{
		if (s->msg_callback)
			s->msg_callback(1, s->version, TLS1_RT_HEARTBEAT,
				buf, 3 + payload + padding,
				s, s->msg_callback_arg);

		s->tlsext_hb_pending = 1;
		}
		
	OPENSSL_free(buf);

	return ret;
	}
#endif
