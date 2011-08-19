/* ssl/ssl_stat.c */
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
#include "ssl_locl.h"

const char *SSL_state_string_long(const SSL *s)
	{
	const char *str;

	switch (s->state)
		{
case SSL_ST_BEFORE: str="before SSL initialization"; break;
case SSL_ST_ACCEPT: str="before accept initialization"; break;
case SSL_ST_CONNECT: str="before connect initialization"; break;
case SSL_ST_OK: str="SSL negotiation finished successfully"; break;
case SSL_ST_RENEGOTIATE:	str="SSL renegotiate ciphers"; break;
case SSL_ST_BEFORE|SSL_ST_CONNECT: str="before/connect initialization"; break;
case SSL_ST_OK|SSL_ST_CONNECT: str="ok/connect SSL initialization"; break;
case SSL_ST_BEFORE|SSL_ST_ACCEPT: str="before/accept initialization"; break;
case SSL_ST_OK|SSL_ST_ACCEPT: str="ok/accept SSL initialization"; break;
#ifndef OPENSSL_NO_SSL2
case SSL2_ST_CLIENT_START_ENCRYPTION: str="SSLv2 client start encryption"; break;
case SSL2_ST_SERVER_START_ENCRYPTION: str="SSLv2 server start encryption"; break;
case SSL2_ST_SEND_CLIENT_HELLO_A: str="SSLv2 write client hello A"; break;
case SSL2_ST_SEND_CLIENT_HELLO_B: str="SSLv2 write client hello B"; break;
case SSL2_ST_GET_SERVER_HELLO_A: str="SSLv2 read server hello A"; break;
case SSL2_ST_GET_SERVER_HELLO_B: str="SSLv2 read server hello B"; break;
case SSL2_ST_SEND_CLIENT_MASTER_KEY_A: str="SSLv2 write client master key A"; break;
case SSL2_ST_SEND_CLIENT_MASTER_KEY_B: str="SSLv2 write client master key B"; break;
case SSL2_ST_SEND_CLIENT_FINISHED_A: str="SSLv2 write client finished A"; break;
case SSL2_ST_SEND_CLIENT_FINISHED_B: str="SSLv2 write client finished B"; break;
case SSL2_ST_SEND_CLIENT_CERTIFICATE_A: str="SSLv2 write client certificate A"; break;
case SSL2_ST_SEND_CLIENT_CERTIFICATE_B: str="SSLv2 write client certificate B"; break;
case SSL2_ST_SEND_CLIENT_CERTIFICATE_C: str="SSLv2 write client certificate C"; break;
case SSL2_ST_SEND_CLIENT_CERTIFICATE_D: str="SSLv2 write client certificate D"; break;
case SSL2_ST_GET_SERVER_VERIFY_A: str="SSLv2 read server verify A"; break;
case SSL2_ST_GET_SERVER_VERIFY_B: str="SSLv2 read server verify B"; break;
case SSL2_ST_GET_SERVER_FINISHED_A: str="SSLv2 read server finished A"; break;
case SSL2_ST_GET_SERVER_FINISHED_B: str="SSLv2 read server finished B"; break;
case SSL2_ST_GET_CLIENT_HELLO_A: str="SSLv2 read client hello A"; break;
case SSL2_ST_GET_CLIENT_HELLO_B: str="SSLv2 read client hello B"; break;
case SSL2_ST_GET_CLIENT_HELLO_C: str="SSLv2 read client hello C"; break;
case SSL2_ST_SEND_SERVER_HELLO_A: str="SSLv2 write server hello A"; break;
case SSL2_ST_SEND_SERVER_HELLO_B: str="SSLv2 write server hello B"; break;
case SSL2_ST_GET_CLIENT_MASTER_KEY_A: str="SSLv2 read client master key A"; break;
case SSL2_ST_GET_CLIENT_MASTER_KEY_B: str="SSLv2 read client master key B"; break;
case SSL2_ST_SEND_SERVER_VERIFY_A: str="SSLv2 write server verify A"; break;
case SSL2_ST_SEND_SERVER_VERIFY_B: str="SSLv2 write server verify B"; break;
case SSL2_ST_SEND_SERVER_VERIFY_C: str="SSLv2 write server verify C"; break;
case SSL2_ST_GET_CLIENT_FINISHED_A: str="SSLv2 read client finished A"; break;
case SSL2_ST_GET_CLIENT_FINISHED_B: str="SSLv2 read client finished B"; break;
case SSL2_ST_SEND_SERVER_FINISHED_A: str="SSLv2 write server finished A"; break;
case SSL2_ST_SEND_SERVER_FINISHED_B: str="SSLv2 write server finished B"; break;
case SSL2_ST_SEND_REQUEST_CERTIFICATE_A: str="SSLv2 write request certificate A"; break;
case SSL2_ST_SEND_REQUEST_CERTIFICATE_B: str="SSLv2 write request certificate B"; break;
case SSL2_ST_SEND_REQUEST_CERTIFICATE_C: str="SSLv2 write request certificate C"; break;
case SSL2_ST_SEND_REQUEST_CERTIFICATE_D: str="SSLv2 write request certificate D"; break;
case SSL2_ST_X509_GET_SERVER_CERTIFICATE: str="SSLv2 X509 read server certificate"; break;
case SSL2_ST_X509_GET_CLIENT_CERTIFICATE: str="SSLv2 X509 read client certificate"; break;
#endif

#ifndef OPENSSL_NO_SSL3
/* SSLv3 additions */
case SSL3_ST_CW_CLNT_HELLO_A:	str="SSLv3 write client hello A"; break;
case SSL3_ST_CW_CLNT_HELLO_B:	str="SSLv3 write client hello B"; break;
case SSL3_ST_CR_SRVR_HELLO_A:	str="SSLv3 read server hello A"; break;
case SSL3_ST_CR_SRVR_HELLO_B:	str="SSLv3 read server hello B"; break;
case SSL3_ST_CR_CERT_A:		str="SSLv3 read server certificate A"; break;
case SSL3_ST_CR_CERT_B:		str="SSLv3 read server certificate B"; break;
case SSL3_ST_CR_KEY_EXCH_A:	str="SSLv3 read server key exchange A"; break;
case SSL3_ST_CR_KEY_EXCH_B:	str="SSLv3 read server key exchange B"; break;
case SSL3_ST_CR_CERT_REQ_A:	str="SSLv3 read server certificate request A"; break;
case SSL3_ST_CR_CERT_REQ_B:	str="SSLv3 read server certificate request B"; break;
case SSL3_ST_CR_SESSION_TICKET_A: str="SSLv3 read server session ticket A";break;
case SSL3_ST_CR_SESSION_TICKET_B: str="SSLv3 read server session ticket B";break;
case SSL3_ST_CR_SRVR_DONE_A:	str="SSLv3 read server done A"; break;
case SSL3_ST_CR_SRVR_DONE_B:	str="SSLv3 read server done B"; break;
case SSL3_ST_CW_CERT_A:		str="SSLv3 write client certificate A"; break;
case SSL3_ST_CW_CERT_B:		str="SSLv3 write client certificate B"; break;
case SSL3_ST_CW_CERT_C:		str="SSLv3 write client certificate C"; break;
case SSL3_ST_CW_CERT_D:		str="SSLv3 write client certificate D"; break;
case SSL3_ST_CW_KEY_EXCH_A:	str="SSLv3 write client key exchange A"; break;
case SSL3_ST_CW_KEY_EXCH_B:	str="SSLv3 write client key exchange B"; break;
case SSL3_ST_CW_CERT_VRFY_A:	str="SSLv3 write certificate verify A"; break;
case SSL3_ST_CW_CERT_VRFY_B:	str="SSLv3 write certificate verify B"; break;

case SSL3_ST_CW_CHANGE_A:
case SSL3_ST_SW_CHANGE_A:	str="SSLv3 write change cipher spec A"; break;
case SSL3_ST_CW_CHANGE_B:	
case SSL3_ST_SW_CHANGE_B:	str="SSLv3 write change cipher spec B"; break;
case SSL3_ST_CW_FINISHED_A:	
case SSL3_ST_SW_FINISHED_A:	str="SSLv3 write finished A"; break;
case SSL3_ST_CW_FINISHED_B:	
case SSL3_ST_SW_FINISHED_B:	str="SSLv3 write finished B"; break;
case SSL3_ST_CR_CHANGE_A:	
case SSL3_ST_SR_CHANGE_A:	str="SSLv3 read change cipher spec A"; break;
case SSL3_ST_CR_CHANGE_B:	
case SSL3_ST_SR_CHANGE_B:	str="SSLv3 read change cipher spec B"; break;
case SSL3_ST_CR_FINISHED_A:	
case SSL3_ST_SR_FINISHED_A:	str="SSLv3 read finished A"; break;
case SSL3_ST_CR_FINISHED_B:	
case SSL3_ST_SR_FINISHED_B:	str="SSLv3 read finished B"; break;

case SSL3_ST_CW_FLUSH:
case SSL3_ST_SW_FLUSH:		str="SSLv3 flush data"; break;

case SSL3_ST_SR_CLNT_HELLO_A:	str="SSLv3 read client hello A"; break;
case SSL3_ST_SR_CLNT_HELLO_B:	str="SSLv3 read client hello B"; break;
case SSL3_ST_SR_CLNT_HELLO_C:	str="SSLv3 read client hello C"; break;
case SSL3_ST_SW_HELLO_REQ_A:	str="SSLv3 write hello request A"; break;
case SSL3_ST_SW_HELLO_REQ_B:	str="SSLv3 write hello request B"; break;
case SSL3_ST_SW_HELLO_REQ_C:	str="SSLv3 write hello request C"; break;
case SSL3_ST_SW_SRVR_HELLO_A:	str="SSLv3 write server hello A"; break;
case SSL3_ST_SW_SRVR_HELLO_B:	str="SSLv3 write server hello B"; break;
case SSL3_ST_SW_CERT_A:		str="SSLv3 write certificate A"; break;
case SSL3_ST_SW_CERT_B:		str="SSLv3 write certificate B"; break;
case SSL3_ST_SW_KEY_EXCH_A:	str="SSLv3 write key exchange A"; break;
case SSL3_ST_SW_KEY_EXCH_B:	str="SSLv3 write key exchange B"; break;
case SSL3_ST_SW_CERT_REQ_A:	str="SSLv3 write certificate request A"; break;
case SSL3_ST_SW_CERT_REQ_B:	str="SSLv3 write certificate request B"; break;
case SSL3_ST_SW_SESSION_TICKET_A: str="SSLv3 write session ticket A"; break;
case SSL3_ST_SW_SESSION_TICKET_B: str="SSLv3 write session ticket B"; break;
case SSL3_ST_SW_SRVR_DONE_A:	str="SSLv3 write server done A"; break;
case SSL3_ST_SW_SRVR_DONE_B:	str="SSLv3 write server done B"; break;
case SSL3_ST_SR_CERT_A:		str="SSLv3 read client certificate A"; break;
case SSL3_ST_SR_CERT_B:		str="SSLv3 read client certificate B"; break;
case SSL3_ST_SR_KEY_EXCH_A:	str="SSLv3 read client key exchange A"; break;
case SSL3_ST_SR_KEY_EXCH_B:	str="SSLv3 read client key exchange B"; break;
case SSL3_ST_SR_CERT_VRFY_A:	str="SSLv3 read certificate verify A"; break;
case SSL3_ST_SR_CERT_VRFY_B:	str="SSLv3 read certificate verify B"; break;
#endif

#if !defined(OPENSSL_NO_SSL2) && !defined(OPENSSL_NO_SSL3)
/* SSLv2/v3 compatibility states */
/* client */
case SSL23_ST_CW_CLNT_HELLO_A:	str="SSLv2/v3 write client hello A"; break;
case SSL23_ST_CW_CLNT_HELLO_B:	str="SSLv2/v3 write client hello B"; break;
case SSL23_ST_CR_SRVR_HELLO_A:	str="SSLv2/v3 read server hello A"; break;
case SSL23_ST_CR_SRVR_HELLO_B:	str="SSLv2/v3 read server hello B"; break;
/* server */
case SSL23_ST_SR_CLNT_HELLO_A:	str="SSLv2/v3 read client hello A"; break;
case SSL23_ST_SR_CLNT_HELLO_B:	str="SSLv2/v3 read client hello B"; break;
#endif

/* DTLS */
case DTLS1_ST_CR_HELLO_VERIFY_REQUEST_A: str="DTLS1 read hello verify request A"; break;
case DTLS1_ST_CR_HELLO_VERIFY_REQUEST_B: str="DTLS1 read hello verify request B"; break;
case DTLS1_ST_SW_HELLO_VERIFY_REQUEST_A: str="DTLS1 write hello verify request A"; break;
case DTLS1_ST_SW_HELLO_VERIFY_REQUEST_B: str="DTLS1 write hello verify request B"; break;

default:	str="unknown state"; break;
		}
	return(str);
	}

const char *SSL_rstate_string_long(const SSL *s)
	{
	const char *str;

	switch (s->rstate)
		{
	case SSL_ST_READ_HEADER: str="read header"; break;
	case SSL_ST_READ_BODY: str="read body"; break;
	case SSL_ST_READ_DONE: str="read done"; break;
	default: str="unknown"; break;
		}
	return(str);
	}

const char *SSL_state_string(const SSL *s)
	{
	const char *str;

	switch (s->state)
		{
case SSL_ST_BEFORE:				str="PINIT "; break;
case SSL_ST_ACCEPT:				str="AINIT "; break;
case SSL_ST_CONNECT:				str="CINIT "; break;
case SSL_ST_OK:			 		str="SSLOK "; break;
#ifndef OPENSSL_NO_SSL2
case SSL2_ST_CLIENT_START_ENCRYPTION:		str="2CSENC"; break;
case SSL2_ST_SERVER_START_ENCRYPTION:		str="2SSENC"; break;
case SSL2_ST_SEND_CLIENT_HELLO_A:		str="2SCH_A"; break;
case SSL2_ST_SEND_CLIENT_HELLO_B:		str="2SCH_B"; break;
case SSL2_ST_GET_SERVER_HELLO_A:		str="2GSH_A"; break;
case SSL2_ST_GET_SERVER_HELLO_B:		str="2GSH_B"; break;
case SSL2_ST_SEND_CLIENT_MASTER_KEY_A:		str="2SCMKA"; break;
case SSL2_ST_SEND_CLIENT_MASTER_KEY_B:		str="2SCMKB"; break;
case SSL2_ST_SEND_CLIENT_FINISHED_A:		str="2SCF_A"; break;
case SSL2_ST_SEND_CLIENT_FINISHED_B:		str="2SCF_B"; break;
case SSL2_ST_SEND_CLIENT_CERTIFICATE_A:		str="2SCC_A"; break;
case SSL2_ST_SEND_CLIENT_CERTIFICATE_B:		str="2SCC_B"; break;
case SSL2_ST_SEND_CLIENT_CERTIFICATE_C:		str="2SCC_C"; break;
case SSL2_ST_SEND_CLIENT_CERTIFICATE_D:		str="2SCC_D"; break;
case SSL2_ST_GET_SERVER_VERIFY_A:		str="2GSV_A"; break;
case SSL2_ST_GET_SERVER_VERIFY_B:		str="2GSV_B"; break;
case SSL2_ST_GET_SERVER_FINISHED_A:		str="2GSF_A"; break;
case SSL2_ST_GET_SERVER_FINISHED_B:		str="2GSF_B"; break;
case SSL2_ST_GET_CLIENT_HELLO_A:		str="2GCH_A"; break;
case SSL2_ST_GET_CLIENT_HELLO_B:		str="2GCH_B"; break;
case SSL2_ST_GET_CLIENT_HELLO_C:		str="2GCH_C"; break;
case SSL2_ST_SEND_SERVER_HELLO_A:		str="2SSH_A"; break;
case SSL2_ST_SEND_SERVER_HELLO_B:		str="2SSH_B"; break;
case SSL2_ST_GET_CLIENT_MASTER_KEY_A:		str="2GCMKA"; break;
case SSL2_ST_GET_CLIENT_MASTER_KEY_B:		str="2GCMKA"; break;
case SSL2_ST_SEND_SERVER_VERIFY_A:		str="2SSV_A"; break;
case SSL2_ST_SEND_SERVER_VERIFY_B:		str="2SSV_B"; break;
case SSL2_ST_SEND_SERVER_VERIFY_C:		str="2SSV_C"; break;
case SSL2_ST_GET_CLIENT_FINISHED_A:		str="2GCF_A"; break;
case SSL2_ST_GET_CLIENT_FINISHED_B:		str="2GCF_B"; break;
case SSL2_ST_SEND_SERVER_FINISHED_A:		str="2SSF_A"; break;
case SSL2_ST_SEND_SERVER_FINISHED_B:		str="2SSF_B"; break;
case SSL2_ST_SEND_REQUEST_CERTIFICATE_A:	str="2SRC_A"; break;
case SSL2_ST_SEND_REQUEST_CERTIFICATE_B:	str="2SRC_B"; break;
case SSL2_ST_SEND_REQUEST_CERTIFICATE_C:	str="2SRC_C"; break;
case SSL2_ST_SEND_REQUEST_CERTIFICATE_D:	str="2SRC_D"; break;
case SSL2_ST_X509_GET_SERVER_CERTIFICATE:	str="2X9GSC"; break;
case SSL2_ST_X509_GET_CLIENT_CERTIFICATE:	str="2X9GCC"; break;
#endif

#ifndef OPENSSL_NO_SSL3
/* SSLv3 additions */
case SSL3_ST_SW_FLUSH:
case SSL3_ST_CW_FLUSH:				str="3FLUSH"; break;
case SSL3_ST_CW_CLNT_HELLO_A:			str="3WCH_A"; break;
case SSL3_ST_CW_CLNT_HELLO_B:			str="3WCH_B"; break;
case SSL3_ST_CR_SRVR_HELLO_A:			str="3RSH_A"; break;
case SSL3_ST_CR_SRVR_HELLO_B:			str="3RSH_B"; break;
case SSL3_ST_CR_CERT_A:				str="3RSC_A"; break;
case SSL3_ST_CR_CERT_B:				str="3RSC_B"; break;
case SSL3_ST_CR_KEY_EXCH_A:			str="3RSKEA"; break;
case SSL3_ST_CR_KEY_EXCH_B:			str="3RSKEB"; break;
case SSL3_ST_CR_CERT_REQ_A:			str="3RCR_A"; break;
case SSL3_ST_CR_CERT_REQ_B:			str="3RCR_B"; break;
case SSL3_ST_CR_SRVR_DONE_A:			str="3RSD_A"; break;
case SSL3_ST_CR_SRVR_DONE_B:			str="3RSD_B"; break;
case SSL3_ST_CW_CERT_A:				str="3WCC_A"; break;
case SSL3_ST_CW_CERT_B:				str="3WCC_B"; break;
case SSL3_ST_CW_CERT_C:				str="3WCC_C"; break;
case SSL3_ST_CW_CERT_D:				str="3WCC_D"; break;
case SSL3_ST_CW_KEY_EXCH_A:			str="3WCKEA"; break;
case SSL3_ST_CW_KEY_EXCH_B:			str="3WCKEB"; break;
case SSL3_ST_CW_CERT_VRFY_A:			str="3WCV_A"; break;
case SSL3_ST_CW_CERT_VRFY_B:			str="3WCV_B"; break;

case SSL3_ST_SW_CHANGE_A:
case SSL3_ST_CW_CHANGE_A:			str="3WCCSA"; break;
case SSL3_ST_SW_CHANGE_B:
case SSL3_ST_CW_CHANGE_B:			str="3WCCSB"; break;
case SSL3_ST_SW_FINISHED_A:
case SSL3_ST_CW_FINISHED_A:			str="3WFINA"; break;
case SSL3_ST_SW_FINISHED_B:
case SSL3_ST_CW_FINISHED_B:			str="3WFINB"; break;
case SSL3_ST_SR_CHANGE_A:
case SSL3_ST_CR_CHANGE_A:			str="3RCCSA"; break;
case SSL3_ST_SR_CHANGE_B:
case SSL3_ST_CR_CHANGE_B:			str="3RCCSB"; break;
case SSL3_ST_SR_FINISHED_A:
case SSL3_ST_CR_FINISHED_A:			str="3RFINA"; break;
case SSL3_ST_SR_FINISHED_B:
case SSL3_ST_CR_FINISHED_B:			str="3RFINB"; break;

case SSL3_ST_SW_HELLO_REQ_A:			str="3WHR_A"; break;
case SSL3_ST_SW_HELLO_REQ_B:			str="3WHR_B"; break;
case SSL3_ST_SW_HELLO_REQ_C:			str="3WHR_C"; break;
case SSL3_ST_SR_CLNT_HELLO_A:			str="3RCH_A"; break;
case SSL3_ST_SR_CLNT_HELLO_B:			str="3RCH_B"; break;
case SSL3_ST_SR_CLNT_HELLO_C:			str="3RCH_C"; break;
case SSL3_ST_SW_SRVR_HELLO_A:			str="3WSH_A"; break;
case SSL3_ST_SW_SRVR_HELLO_B:			str="3WSH_B"; break;
case SSL3_ST_SW_CERT_A:				str="3WSC_A"; break;
case SSL3_ST_SW_CERT_B:				str="3WSC_B"; break;
case SSL3_ST_SW_KEY_EXCH_A:			str="3WSKEA"; break;
case SSL3_ST_SW_KEY_EXCH_B:			str="3WSKEB"; break;
case SSL3_ST_SW_CERT_REQ_A:			str="3WCR_A"; break;
case SSL3_ST_SW_CERT_REQ_B:			str="3WCR_B"; break;
case SSL3_ST_SW_SRVR_DONE_A:			str="3WSD_A"; break;
case SSL3_ST_SW_SRVR_DONE_B:			str="3WSD_B"; break;
case SSL3_ST_SR_CERT_A:				str="3RCC_A"; break;
case SSL3_ST_SR_CERT_B:				str="3RCC_B"; break;
case SSL3_ST_SR_KEY_EXCH_A:			str="3RCKEA"; break;
case SSL3_ST_SR_KEY_EXCH_B:			str="3RCKEB"; break;
case SSL3_ST_SR_CERT_VRFY_A:			str="3RCV_A"; break;
case SSL3_ST_SR_CERT_VRFY_B:			str="3RCV_B"; break;
#endif

#if !defined(OPENSSL_NO_SSL2) && !defined(OPENSSL_NO_SSL3)
/* SSLv2/v3 compatibility states */
/* client */
case SSL23_ST_CW_CLNT_HELLO_A:			str="23WCHA"; break;
case SSL23_ST_CW_CLNT_HELLO_B:			str="23WCHB"; break;
case SSL23_ST_CR_SRVR_HELLO_A:			str="23RSHA"; break;
case SSL23_ST_CR_SRVR_HELLO_B:			str="23RSHA"; break;
/* server */
case SSL23_ST_SR_CLNT_HELLO_A:			str="23RCHA"; break;
case SSL23_ST_SR_CLNT_HELLO_B:			str="23RCHB"; break;
#endif
/* DTLS */
case DTLS1_ST_CR_HELLO_VERIFY_REQUEST_A: str="DRCHVA"; break;
case DTLS1_ST_CR_HELLO_VERIFY_REQUEST_B: str="DRCHVB"; break;
case DTLS1_ST_SW_HELLO_VERIFY_REQUEST_A: str="DWCHVA"; break;
case DTLS1_ST_SW_HELLO_VERIFY_REQUEST_B: str="DWCHVB"; break;

default:					str="UNKWN "; break;
		}
	return(str);
	}

const char *SSL_alert_type_string_long(int value)
	{
	value>>=8;
	if (value == SSL3_AL_WARNING)
		return("warning");
	else if (value == SSL3_AL_FATAL)
		return("fatal");
	else
		return("unknown");
	}

const char *SSL_alert_type_string(int value)
	{
	value>>=8;
	if (value == SSL3_AL_WARNING)
		return("W");
	else if (value == SSL3_AL_FATAL)
		return("F");
	else
		return("U");
	}

const char *SSL_alert_desc_string(int value)
	{
	const char *str;

	switch (value & 0xff)
		{
	case SSL3_AD_CLOSE_NOTIFY:		str="CN"; break;
	case SSL3_AD_UNEXPECTED_MESSAGE:	str="UM"; break;
	case SSL3_AD_BAD_RECORD_MAC:		str="BM"; break;
	case SSL3_AD_DECOMPRESSION_FAILURE:	str="DF"; break;
	case SSL3_AD_HANDSHAKE_FAILURE:		str="HF"; break;
	case SSL3_AD_NO_CERTIFICATE:		str="NC"; break;
	case SSL3_AD_BAD_CERTIFICATE:		str="BC"; break;
	case SSL3_AD_UNSUPPORTED_CERTIFICATE:	str="UC"; break;
	case SSL3_AD_CERTIFICATE_REVOKED:	str="CR"; break;
	case SSL3_AD_CERTIFICATE_EXPIRED:	str="CE"; break;
	case SSL3_AD_CERTIFICATE_UNKNOWN:	str="CU"; break;
	case SSL3_AD_ILLEGAL_PARAMETER:		str="IP"; break;
	case TLS1_AD_DECRYPTION_FAILED:		str="DC"; break;
	case TLS1_AD_RECORD_OVERFLOW:		str="RO"; break;
	case TLS1_AD_UNKNOWN_CA:		str="CA"; break;
	case TLS1_AD_ACCESS_DENIED:		str="AD"; break;
	case TLS1_AD_DECODE_ERROR:		str="DE"; break;
	case TLS1_AD_DECRYPT_ERROR:		str="CY"; break;
	case TLS1_AD_EXPORT_RESTRICTION:	str="ER"; break;
	case TLS1_AD_PROTOCOL_VERSION:		str="PV"; break;
	case TLS1_AD_INSUFFICIENT_SECURITY:	str="IS"; break;
	case TLS1_AD_INTERNAL_ERROR:		str="IE"; break;
	case TLS1_AD_USER_CANCELLED:		str="US"; break;
	case TLS1_AD_NO_RENEGOTIATION:		str="NR"; break;
	default:				str="UK"; break;
		}
	return(str);
	}

const char *SSL_alert_desc_string_long(int value)
	{
	const char *str;

	switch (value & 0xff)
		{
	case SSL3_AD_CLOSE_NOTIFY:
		str="close notify";
		break;
	case SSL3_AD_UNEXPECTED_MESSAGE:
		str="unexpected_message";
		break;
	case SSL3_AD_BAD_RECORD_MAC:
		str="bad record mac";
		break;
	case SSL3_AD_DECOMPRESSION_FAILURE:
		str="decompression failure";
		break;
	case SSL3_AD_HANDSHAKE_FAILURE:
		str="handshake failure";
		break;
	case SSL3_AD_NO_CERTIFICATE:
		str="no certificate";
		break;
	case SSL3_AD_BAD_CERTIFICATE:
		str="bad certificate";
		break;
	case SSL3_AD_UNSUPPORTED_CERTIFICATE:
		str="unsupported certificate";
		break;
	case SSL3_AD_CERTIFICATE_REVOKED:
		str="certificate revoked";
		break;
	case SSL3_AD_CERTIFICATE_EXPIRED:
		str="certificate expired";
		break;
	case SSL3_AD_CERTIFICATE_UNKNOWN:
		str="certificate unknown";
		break;
	case SSL3_AD_ILLEGAL_PARAMETER:
		str="illegal parameter";
		break;
	case TLS1_AD_DECRYPTION_FAILED:
		str="decryption failed";
		break;
	case TLS1_AD_RECORD_OVERFLOW:
		str="record overflow";
		break;
	case TLS1_AD_UNKNOWN_CA:
		str="unknown CA";
		break;
	case TLS1_AD_ACCESS_DENIED:
		str="access denied";
		break;
	case TLS1_AD_DECODE_ERROR:
		str="decode error";
		break;
	case TLS1_AD_DECRYPT_ERROR:
		str="decrypt error";
		break;
	case TLS1_AD_EXPORT_RESTRICTION:
		str="export restriction";
		break;
	case TLS1_AD_PROTOCOL_VERSION:
		str="protocol version";
		break;
	case TLS1_AD_INSUFFICIENT_SECURITY:
		str="insufficient security";
		break;
	case TLS1_AD_INTERNAL_ERROR:
		str="internal error";
		break;
	case TLS1_AD_USER_CANCELLED:
		str="user canceled";
		break;
	case TLS1_AD_NO_RENEGOTIATION:
		str="no renegotiation";
		break;
	default: str="unknown"; break;
		}
	return(str);
	}

const char *SSL_rstate_string(const SSL *s)
	{
	const char *str;

	switch (s->rstate)
		{
	case SSL_ST_READ_HEADER:str="RH"; break;
	case SSL_ST_READ_BODY:	str="RB"; break;
	case SSL_ST_READ_DONE:	str="RD"; break;
	default: str="unknown"; break;
		}
	return(str);
	}
