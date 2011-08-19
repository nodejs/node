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
	};

long dtls1_default_timeout(void)
	{
	/* 2 hours, the 24 hours mentioned in the DTLSv1 spec
	 * is way too long for http, the cache would over fill */
	return(60*60*2);
	}

IMPLEMENT_dtls1_meth_func(dtlsv1_base_method,
			ssl_undefined_function,
			ssl_undefined_function,
			ssl_bad_method)

int dtls1_new(SSL *s)
	{
	DTLS1_STATE *d1;

	if (!ssl3_new(s)) return(0);
	if ((d1=OPENSSL_malloc(sizeof *d1)) == NULL) return (0);
	memset(d1,0, sizeof *d1);

	/* d1->handshake_epoch=0; */
#if defined(OPENSSL_SYS_VMS) || defined(VMS_TEST)
	d1->bitmap.length=64;
#else
	d1->bitmap.length=sizeof(d1->bitmap.map) * 8;
#endif
	pq_64bit_init(&(d1->bitmap.map));
	pq_64bit_init(&(d1->bitmap.max_seq_num));
	
	d1->next_bitmap.length = d1->bitmap.length;
	pq_64bit_init(&(d1->next_bitmap.map));
	pq_64bit_init(&(d1->next_bitmap.max_seq_num));

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

void dtls1_free(SSL *s)
	{
    pitem *item = NULL;
    hm_fragment *frag = NULL;

	ssl3_free(s);

    while( (item = pqueue_pop(s->d1->unprocessed_rcds.q)) != NULL)
        {
        OPENSSL_free(item->data);
        pitem_free(item);
        }
    pqueue_free(s->d1->unprocessed_rcds.q);

    while( (item = pqueue_pop(s->d1->processed_rcds.q)) != NULL)
        {
        OPENSSL_free(item->data);
        pitem_free(item);
        }
    pqueue_free(s->d1->processed_rcds.q);

    while( (item = pqueue_pop(s->d1->buffered_messages)) != NULL)
        {
        frag = (hm_fragment *)item->data;
        OPENSSL_free(frag->fragment);
        OPENSSL_free(frag);
        pitem_free(item);
        }
    pqueue_free(s->d1->buffered_messages);

    while ( (item = pqueue_pop(s->d1->sent_messages)) != NULL)
        {
        frag = (hm_fragment *)item->data;
        OPENSSL_free(frag->fragment);
        OPENSSL_free(frag);
        pitem_free(item);
        }
	pqueue_free(s->d1->sent_messages);

	while ( (item = pqueue_pop(s->d1->buffered_app_data.q)) != NULL)
	{
		frag = (hm_fragment *)item->data;
		OPENSSL_free(frag->fragment);
		OPENSSL_free(frag);
		pitem_free(item);
	}
	pqueue_free(s->d1->buffered_app_data.q);
	
	pq_64bit_free(&(s->d1->bitmap.map));
	pq_64bit_free(&(s->d1->bitmap.max_seq_num));

	pq_64bit_free(&(s->d1->next_bitmap.map));
	pq_64bit_free(&(s->d1->next_bitmap.max_seq_num));

	OPENSSL_free(s->d1);
	}

void dtls1_clear(SSL *s)
	{
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
SSL_CIPHER *dtls1_get_cipher(unsigned int u)
	{
	SSL_CIPHER *ciph = ssl3_get_cipher(u);

	if (ciph != NULL)
		{
		if ((ciph->algorithms&SSL_ENC_MASK) == SSL_RC4)
			return NULL;
		}

	return ciph;
	}

void dtls1_start_timer(SSL *s)
	{
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
	memset(&(s->d1->next_timeout), 0, sizeof(struct timeval));
	s->d1->timeout_duration = 1;
	BIO_ctrl(SSL_get_rbio(s), BIO_CTRL_DGRAM_SET_NEXT_TIMEOUT, 0, &(s->d1->next_timeout));
	}

int dtls1_handle_timeout(SSL *s)
	{
	DTLS1_STATE *state;

	/* if no timer is expired, don't do anything */
	if (!dtls1_is_timer_expired(s))
		{
		return 0;
		}

	dtls1_double_timeout(s);
	state = s->d1;
	state->timeout.num_alerts++;
	if ( state->timeout.num_alerts > DTLS1_TMO_ALERT_COUNT)
		{
		/* fail the connection, enough alerts have been sent */
		SSLerr(SSL_F_DTLS1_HANDLE_TIMEOUT,SSL_R_READ_TIMEOUT_EXPIRED);
		return 0;
		}

	state->timeout.read_timeouts++;
	if ( state->timeout.read_timeouts > DTLS1_TMO_READ_COUNT)
		{
		state->timeout.read_timeouts = 1;
		}

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
