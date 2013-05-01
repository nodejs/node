/* ssl/d1_lib.c */
/* 
 * DTLS implementation written by Nagendra Modadugu
 * (nagendra@cs.stanford.edu) for the OpenSSL project 2005.  
 */
/* ====================================================================
 * Copyright (c) 1999-2005 The OpenSSL Project.  All rights reserved.
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
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
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
#define USE_SOCKETS
#include <openssl/objects.h>
#include "ssl_locl.h"

#if defined(OPENSSL_SYS_WIN32) || defined(OPENSSL_SYS_VMS)
#include <sys/timeb.h>
#endif

static void get_current_time(struct timeval *t);
const char dtls1_version_str[]="DTLSv1" OPENSSL_VERSION_PTEXT;
int dtls1_listen(SSL *s, struct sockaddr *client);

SSL3_ENC_METHOD DTLSv1_enc_data={
    dtls1_enc,
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

long dtls1_default_timeout(void)
	{
	/* 2 hours, the 24 hours mentioned in the DTLSv1 spec
	 * is way too long for http, the cache would over fill */
	return(60*60*2);
	}

int dtls1_new(SSL *s)
	{
	DTLS1_STATE *d1;

	if (!ssl3_new(s)) return(0);
	if ((d1=OPENSSL_malloc(sizeof *d1)) == NULL) return (0);
	memset(d1,0, sizeof *d1);

	/* d1->handshake_epoch=0; */

	d1->unprocessed_rcds.q=pqueue_new();
	d1->processed_rcds.q=pqueue_new();
	d1->buffered_messages = pqueue_new();
	d1->sent_messages=pqueue_new();
	d1->buffered_app_data.q=pqueue_new();

	if ( s->server)
		{
		d1->cookie_len = sizeof(s->d1->cookie);
		}

	if( ! d1->unprocessed_rcds.q || ! d1->processed_rcds.q 
        || ! d1->buffered_messages || ! d1->sent_messages || ! d1->buffered_app_data.q)
		{
        if ( d1->unprocessed_rcds.q) pqueue_free(d1->unprocessed_rcds.q);
        if ( d1->processed_rcds.q) pqueue_free(d1->processed_rcds.q);
        if ( d1->buffered_messages) pqueue_free(d1->buffered_messages);
		if ( d1->sent_messages) pqueue_free(d1->sent_messages);
		if ( d1->buffered_app_data.q) pqueue_free(d1->buffered_app_data.q);
		OPENSSL_free(d1);
		return (0);
		}

	s->d1=d1;
	s->method->ssl_clear(s);
	return(1);
	}

static void dtls1_clear_queues(SSL *s)
	{
    pitem *item = NULL;
    hm_fragment *frag = NULL;
	DTLS1_RECORD_DATA *rdata;

    while( (item = pqueue_pop(s->d1->unprocessed_rcds.q)) != NULL)
        {
		rdata = (DTLS1_RECORD_DATA *) item->data;
		if (rdata->rbuf.buf)
			{
			OPENSSL_free(rdata->rbuf.buf);
			}
        OPENSSL_free(item->data);
        pitem_free(item);
        }

    while( (item = pqueue_pop(s->d1->processed_rcds.q)) != NULL)
        {
		rdata = (DTLS1_RECORD_DATA *) item->data;
		if (rdata->rbuf.buf)
			{
			OPENSSL_free(rdata->rbuf.buf);
			}
        OPENSSL_free(item->data);
        pitem_free(item);
        }

    while( (item = pqueue_pop(s->d1->buffered_messages)) != NULL)
        {
        frag = (hm_fragment *)item->data;
        OPENSSL_free(frag->fragment);
        OPENSSL_free(frag);
        pitem_free(item);
        }

    while ( (item = pqueue_pop(s->d1->sent_messages)) != NULL)
        {
        frag = (hm_fragment *)item->data;
        OPENSSL_free(frag->fragment);
        OPENSSL_free(frag);
        pitem_free(item);
        }

	while ( (item = pqueue_pop(s->d1->buffered_app_data.q)) != NULL)
		{
		frag = (hm_fragment *)item->data;
		OPENSSL_free(frag->fragment);
		OPENSSL_free(frag);
		pitem_free(item);
		}
	}

void dtls1_free(SSL *s)
	{
	ssl3_free(s);

	dtls1_clear_queues(s);

    pqueue_free(s->d1->unprocessed_rcds.q);
    pqueue_free(s->d1->processed_rcds.q);
    pqueue_free(s->d1->buffered_messages);
	pqueue_free(s->d1->sent_messages);
	pqueue_free(s->d1->buffered_app_data.q);

	OPENSSL_free(s->d1);
	}

void dtls1_clear(SSL *s)
	{
    pqueue unprocessed_rcds;
    pqueue processed_rcds;
    pqueue buffered_messages;
	pqueue sent_messages;
	pqueue buffered_app_data;
	unsigned int mtu;

	if (s->d1)
		{
		unprocessed_rcds = s->d1->unprocessed_rcds.q;
		processed_rcds = s->d1->processed_rcds.q;
		buffered_messages = s->d1->buffered_messages;
		sent_messages = s->d1->sent_messages;
		buffered_app_data = s->d1->buffered_app_data.q;
		mtu = s->d1->mtu;

		dtls1_clear_queues(s);

		memset(s->d1, 0, sizeof(*(s->d1)));

		if (s->server)
			{
			s->d1->cookie_len = sizeof(s->d1->cookie);
			}

		if (SSL_get_options(s) & SSL_OP_NO_QUERY_MTU)
			{
			s->d1->mtu = mtu;
			}

		s->d1->unprocessed_rcds.q = unprocessed_rcds;
		s->d1->processed_rcds.q = processed_rcds;
		s->d1->buffered_messages = buffered_messages;
		s->d1->sent_messages = sent_messages;
		s->d1->buffered_app_data.q = buffered_app_data;
		}

	ssl3_clear(s);
	if (s->options & SSL_OP_CISCO_ANYCONNECT)
		s->version=DTLS1_BAD_VER;
	else
		s->version=DTLS1_VERSION;
	}

long dtls1_ctrl(SSL *s, int cmd, long larg, void *parg)
	{
	int ret=0;

	switch (cmd)
		{
	case DTLS_CTRL_GET_TIMEOUT:
		if (dtls1_get_timeout(s, (struct timeval*) parg) != NULL)
			{
			ret = 1;
			}
		break;
	case DTLS_CTRL_HANDLE_TIMEOUT:
		ret = dtls1_handle_timeout(s);
		break;
	case DTLS_CTRL_LISTEN:
		ret = dtls1_listen(s, parg);
		break;

	default:
		ret = ssl3_ctrl(s, cmd, larg, parg);
		break;
		}
	return(ret);
	}

/*
 * As it's impossible to use stream ciphers in "datagram" mode, this
 * simple filter is designed to disengage them in DTLS. Unfortunately
 * there is no universal way to identify stream SSL_CIPHER, so we have
 * to explicitly list their SSL_* codes. Currently RC4 is the only one
 * available, but if new ones emerge, they will have to be added...
 */
const SSL_CIPHER *dtls1_get_cipher(unsigned int u)
	{
	const SSL_CIPHER *ciph = ssl3_get_cipher(u);

	if (ciph != NULL)
		{
		if (ciph->algorithm_enc == SSL_RC4)
			return NULL;
		}

	return ciph;
	}

void dtls1_start_timer(SSL *s)
	{
#ifndef OPENSSL_NO_SCTP
	/* Disable timer for SCTP */
	if (BIO_dgram_is_sctp(SSL_get_wbio(s)))
		{
		memset(&(s->d1->next_timeout), 0, sizeof(struct timeval));
		return;
		}
#endif

	/* If timer is not set, initialize duration with 1 second */
	if (s->d1->next_timeout.tv_sec == 0 && s->d1->next_timeout.tv_usec == 0)
		{
		s->d1->timeout_duration = 1;
		}
	
	/* Set timeout to current time */
	get_current_time(&(s->d1->next_timeout));

	/* Add duration to current time */
	s->d1->next_timeout.tv_sec += s->d1->timeout_duration;
	BIO_ctrl(SSL_get_rbio(s), BIO_CTRL_DGRAM_SET_NEXT_TIMEOUT, 0, &(s->d1->next_timeout));
	}

struct timeval* dtls1_get_timeout(SSL *s, struct timeval* timeleft)
	{
	struct timeval timenow;

	/* If no timeout is set, just return NULL */
	if (s->d1->next_timeout.tv_sec == 0 && s->d1->next_timeout.tv_usec == 0)
		{
		return NULL;
		}

	/* Get current time */
	get_current_time(&timenow);

	/* If timer already expired, set remaining time to 0 */
	if (s->d1->next_timeout.tv_sec < timenow.tv_sec ||
		(s->d1->next_timeout.tv_sec == timenow.tv_sec &&
		 s->d1->next_timeout.tv_usec <= timenow.tv_usec))
		{
		memset(timeleft, 0, sizeof(struct timeval));
		return timeleft;
		}

	/* Calculate time left until timer expires */
	memcpy(timeleft, &(s->d1->next_timeout), sizeof(struct timeval));
	timeleft->tv_sec -= timenow.tv_sec;
	timeleft->tv_usec -= timenow.tv_usec;
	if (timeleft->tv_usec < 0)
		{
		timeleft->tv_sec--;
		timeleft->tv_usec += 1000000;
		}

	/* If remaining time is less than 15 ms, set it to 0
	 * to prevent issues because of small devergences with
	 * socket timeouts.
	 */
	if (timeleft->tv_sec == 0 && timeleft->tv_usec < 15000)
		{
		memset(timeleft, 0, sizeof(struct timeval));
		}
	

	return timeleft;
	}

int dtls1_is_timer_expired(SSL *s)
	{
	struct timeval timeleft;

	/* Get time left until timeout, return false if no timer running */
	if (dtls1_get_timeout(s, &timeleft) == NULL)
		{
		return 0;
		}

	/* Return false if timer is not expired yet */
	if (timeleft.tv_sec > 0 || timeleft.tv_usec > 0)
		{
		return 0;
		}

	/* Timer expired, so return true */	
	return 1;
	}

void dtls1_double_timeout(SSL *s)
	{
	s->d1->timeout_duration *= 2;
	if (s->d1->timeout_duration > 60)
		s->d1->timeout_duration = 60;
	dtls1_start_timer(s);
	}

void dtls1_stop_timer(SSL *s)
	{
	/* Reset everything */
	memset(&(s->d1->timeout), 0, sizeof(struct dtls1_timeout_st));
	memset(&(s->d1->next_timeout), 0, sizeof(struct timeval));
	s->d1->timeout_duration = 1;
	BIO_ctrl(SSL_get_rbio(s), BIO_CTRL_DGRAM_SET_NEXT_TIMEOUT, 0, &(s->d1->next_timeout));
	/* Clear retransmission buffer */
	dtls1_clear_record_buffer(s);
	}

int dtls1_check_timeout_num(SSL *s)
	{
	s->d1->timeout.num_alerts++;

	/* Reduce MTU after 2 unsuccessful retransmissions */
	if (s->d1->timeout.num_alerts > 2)
		{
		s->d1->mtu = BIO_ctrl(SSL_get_wbio(s), BIO_CTRL_DGRAM_GET_FALLBACK_MTU, 0, NULL);		
		}

	if (s->d1->timeout.num_alerts > DTLS1_TMO_ALERT_COUNT)
		{
		/* fail the connection, enough alerts have been sent */
		SSLerr(SSL_F_DTLS1_CHECK_TIMEOUT_NUM,SSL_R_READ_TIMEOUT_EXPIRED);
		return -1;
		}

	return 0;
	}

int dtls1_handle_timeout(SSL *s)
	{
	/* if no timer is expired, don't do anything */
	if (!dtls1_is_timer_expired(s))
		{
		return 0;
		}

	dtls1_double_timeout(s);

	if (dtls1_check_timeout_num(s) < 0)
		return -1;

	s->d1->timeout.read_timeouts++;
	if (s->d1->timeout.read_timeouts > DTLS1_TMO_READ_COUNT)
		{
		s->d1->timeout.read_timeouts = 1;
		}

#ifndef OPENSSL_NO_HEARTBEATS
	if (s->tlsext_hb_pending)
		{
		s->tlsext_hb_pending = 0;
		return dtls1_heartbeat(s);
		}
#endif

	dtls1_start_timer(s);
	return dtls1_retransmit_buffered_messages(s);
	}

static void get_current_time(struct timeval *t)
{
#ifdef OPENSSL_SYS_WIN32
	struct _timeb tb;
	_ftime(&tb);
	t->tv_sec = (long)tb.time;
	t->tv_usec = (long)tb.millitm * 1000;
#elif defined(OPENSSL_SYS_VMS)
	struct timeb tb;
	ftime(&tb);
	t->tv_sec = (long)tb.time;
	t->tv_usec = (long)tb.millitm * 1000;
#else
	gettimeofday(t, NULL);
#endif
}

int dtls1_listen(SSL *s, struct sockaddr *client)
	{
	int ret;

	SSL_set_options(s, SSL_OP_COOKIE_EXCHANGE);
	s->d1->listen = 1;

	ret = SSL_accept(s);
	if (ret <= 0) return ret;
	
	(void) BIO_dgram_get_peer(SSL_get_rbio(s), client);
	return 1;
	}
