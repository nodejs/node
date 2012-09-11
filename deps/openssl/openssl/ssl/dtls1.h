/* ssl/dtls1.h */
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

#ifndef HEADER_DTLS1_H 
#define HEADER_DTLS1_H 

#include <openssl/buffer.h>
#include <openssl/pqueue.h>
#ifdef OPENSSL_SYS_VMS
#include <resource.h>
#include <sys/timeb.h>
#endif
#ifdef OPENSSL_SYS_WIN32
/* Needed for struct timeval */
#include <winsock.h>
#elif defined(OPENSSL_SYS_NETWARE) && !defined(_WINSOCK2API_)
#include <sys/timeval.h>
#else
#include <sys/time.h>
#endif

#ifdef  __cplusplus
extern "C" {
#endif

#define DTLS1_VERSION			0xFEFF
#define DTLS1_BAD_VER			0x0100

#if 0
/* this alert description is not specified anywhere... */
#define DTLS1_AD_MISSING_HANDSHAKE_MESSAGE    110
#endif

/* lengths of messages */
#define DTLS1_COOKIE_LENGTH                     256

#define DTLS1_RT_HEADER_LENGTH                  13

#define DTLS1_HM_HEADER_LENGTH                  12

#define DTLS1_HM_BAD_FRAGMENT                   -2
#define DTLS1_HM_FRAGMENT_RETRY                 -3

#define DTLS1_CCS_HEADER_LENGTH                  1

#ifdef DTLS1_AD_MISSING_HANDSHAKE_MESSAGE
#define DTLS1_AL_HEADER_LENGTH                   7
#else
#define DTLS1_AL_HEADER_LENGTH                   2
#endif

#ifndef OPENSSL_NO_SSL_INTERN

#ifndef OPENSSL_NO_SCTP
#define DTLS1_SCTP_AUTH_LABEL	"EXPORTER_DTLS_OVER_SCTP"
#endif

typedef struct dtls1_bitmap_st
	{
	unsigned long map;		/* track 32 packets on 32-bit systems
					   and 64 - on 64-bit systems */
	unsigned char max_seq_num[8];	/* max record number seen so far,
					   64-bit value in big-endian
					   encoding */
	} DTLS1_BITMAP;

struct dtls1_retransmit_state
	{
	EVP_CIPHER_CTX *enc_write_ctx;	/* cryptographic state */
	EVP_MD_CTX *write_hash;			/* used for mac generation */
#ifndef OPENSSL_NO_COMP
	COMP_CTX *compress;				/* compression */
#else
	char *compress;	
#endif
	SSL_SESSION *session;
	unsigned short epoch;
	};

struct hm_header_st
	{
	unsigned char type;
	unsigned long msg_len;
	unsigned short seq;
	unsigned long frag_off;
	unsigned long frag_len;
	unsigned int is_ccs;
	struct dtls1_retransmit_state saved_retransmit_state;
	};

struct ccs_header_st
	{
	unsigned char type;
	unsigned short seq;
	};

struct dtls1_timeout_st
	{
	/* Number of read timeouts so far */
	unsigned int read_timeouts;
	
	/* Number of write timeouts so far */
	unsigned int write_timeouts;
	
	/* Number of alerts received so far */
	unsigned int num_alerts;
	};

typedef struct record_pqueue_st
	{
	unsigned short epoch;
	pqueue q;
	} record_pqueue;

typedef struct hm_fragment_st
	{
	struct hm_header_st msg_header;
	unsigned char *fragment;
	unsigned char *reassembly;
	} hm_fragment;

typedef struct dtls1_state_st
	{
	unsigned int send_cookie;
	unsigned char cookie[DTLS1_COOKIE_LENGTH];
	unsigned char rcvd_cookie[DTLS1_COOKIE_LENGTH];
	unsigned int cookie_len;

	/* 
	 * The current data and handshake epoch.  This is initially
	 * undefined, and starts at zero once the initial handshake is
	 * completed 
	 */
	unsigned short r_epoch;
	unsigned short w_epoch;

	/* records being received in the current epoch */
	DTLS1_BITMAP bitmap;

	/* renegotiation starts a new set of sequence numbers */
	DTLS1_BITMAP next_bitmap;

	/* handshake message numbers */
	unsigned short handshake_write_seq;
	unsigned short next_handshake_write_seq;

	unsigned short handshake_read_seq;

	/* save last sequence number for retransmissions */
	unsigned char last_write_sequence[8];

	/* Received handshake records (processed and unprocessed) */
	record_pqueue unprocessed_rcds;
	record_pqueue processed_rcds;

	/* Buffered handshake messages */
	pqueue buffered_messages;

	/* Buffered (sent) handshake records */
	pqueue sent_messages;

	/* Buffered application records.
	 * Only for records between CCS and Finished
	 * to prevent either protocol violation or
	 * unnecessary message loss.
	 */
	record_pqueue buffered_app_data;

	/* Is set when listening for new connections with dtls1_listen() */
	unsigned int listen;

	unsigned int mtu; /* max DTLS packet size */

	struct hm_header_st w_msg_hdr;
	struct hm_header_st r_msg_hdr;

	struct dtls1_timeout_st timeout;

	/* Indicates when the last handshake msg or heartbeat sent will timeout */
	struct timeval next_timeout;

	/* Timeout duration */
	unsigned short timeout_duration;

	/* storage for Alert/Handshake protocol data received but not
	 * yet processed by ssl3_read_bytes: */
	unsigned char alert_fragment[DTLS1_AL_HEADER_LENGTH];
	unsigned int alert_fragment_len;
	unsigned char handshake_fragment[DTLS1_HM_HEADER_LENGTH];
	unsigned int handshake_fragment_len;

	unsigned int retransmitting;
	unsigned int change_cipher_spec_ok;

#ifndef OPENSSL_NO_SCTP
	/* used when SSL_ST_XX_FLUSH is entered */
	int next_state;

	int shutdown_received;
#endif

	} DTLS1_STATE;

typedef struct dtls1_record_data_st
	{
	unsigned char *packet;
	unsigned int   packet_length;
	SSL3_BUFFER    rbuf;
	SSL3_RECORD    rrec;
#ifndef OPENSSL_NO_SCTP
	struct bio_dgram_sctp_rcvinfo recordinfo;
#endif
	} DTLS1_RECORD_DATA;

#endif

/* Timeout multipliers (timeout slice is defined in apps/timeouts.h */
#define DTLS1_TMO_READ_COUNT                      2
#define DTLS1_TMO_WRITE_COUNT                     2

#define DTLS1_TMO_ALERT_COUNT                     12

#ifdef  __cplusplus
}
#endif
#endif

