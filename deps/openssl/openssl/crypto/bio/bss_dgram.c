/* crypto/bio/bio_dgram.c */
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
#include <errno.h>
#define USE_SOCKETS
#include "cryptlib.h"

#include <openssl/bio.h>
#ifndef OPENSSL_NO_DGRAM

#if defined(OPENSSL_SYS_WIN32) || defined(OPENSSL_SYS_VMS)
#include <sys/timeb.h>
#endif

#ifndef OPENSSL_NO_SCTP
#include <netinet/sctp.h>
#include <fcntl.h>
#define OPENSSL_SCTP_DATA_CHUNK_TYPE            0x00
#define OPENSSL_SCTP_FORWARD_CUM_TSN_CHUNK_TYPE 0xc0
#endif

#if defined(OPENSSL_SYS_LINUX) && !defined(IP_MTU)
#define IP_MTU      14 /* linux is lame */
#endif

#if defined(__FreeBSD__) && defined(IN6_IS_ADDR_V4MAPPED)
/* Standard definition causes type-punning problems. */
#undef IN6_IS_ADDR_V4MAPPED
#define s6_addr32 __u6_addr.__u6_addr32
#define IN6_IS_ADDR_V4MAPPED(a)               \
        (((a)->s6_addr32[0] == 0) &&          \
         ((a)->s6_addr32[1] == 0) &&          \
         ((a)->s6_addr32[2] == htonl(0x0000ffff)))
#endif

#ifdef WATT32
#define sock_write SockWrite  /* Watt-32 uses same names */
#define sock_read  SockRead
#define sock_puts  SockPuts
#endif

static int dgram_write(BIO *h, const char *buf, int num);
static int dgram_read(BIO *h, char *buf, int size);
static int dgram_puts(BIO *h, const char *str);
static long dgram_ctrl(BIO *h, int cmd, long arg1, void *arg2);
static int dgram_new(BIO *h);
static int dgram_free(BIO *data);
static int dgram_clear(BIO *bio);

#ifndef OPENSSL_NO_SCTP
static int dgram_sctp_write(BIO *h, const char *buf, int num);
static int dgram_sctp_read(BIO *h, char *buf, int size);
static int dgram_sctp_puts(BIO *h, const char *str);
static long dgram_sctp_ctrl(BIO *h, int cmd, long arg1, void *arg2);
static int dgram_sctp_new(BIO *h);
static int dgram_sctp_free(BIO *data);
#ifdef SCTP_AUTHENTICATION_EVENT
static void dgram_sctp_handle_auth_free_key_event(BIO *b, union sctp_notification *snp);
#endif
#endif

static int BIO_dgram_should_retry(int s);

static void get_current_time(struct timeval *t);

static BIO_METHOD methods_dgramp=
	{
	BIO_TYPE_DGRAM,
	"datagram socket",
	dgram_write,
	dgram_read,
	dgram_puts,
	NULL, /* dgram_gets, */
	dgram_ctrl,
	dgram_new,
	dgram_free,
	NULL,
	};

#ifndef OPENSSL_NO_SCTP
static BIO_METHOD methods_dgramp_sctp=
	{
	BIO_TYPE_DGRAM_SCTP,
	"datagram sctp socket",
	dgram_sctp_write,
	dgram_sctp_read,
	dgram_sctp_puts,
	NULL, /* dgram_gets, */
	dgram_sctp_ctrl,
	dgram_sctp_new,
	dgram_sctp_free,
	NULL,
	};
#endif

typedef struct bio_dgram_data_st
	{
	union {
		struct sockaddr sa;
		struct sockaddr_in sa_in;
#if OPENSSL_USE_IPV6
		struct sockaddr_in6 sa_in6;
#endif
	} peer;
	unsigned int connected;
	unsigned int _errno;
	unsigned int mtu;
	struct timeval next_timeout;
	struct timeval socket_timeout;
	} bio_dgram_data;

#ifndef OPENSSL_NO_SCTP
typedef struct bio_dgram_sctp_save_message_st
	{
        BIO *bio;
        char *data;
        int length;
	} bio_dgram_sctp_save_message;

typedef struct bio_dgram_sctp_data_st
	{
	union {
		struct sockaddr sa;
		struct sockaddr_in sa_in;
#if OPENSSL_USE_IPV6
		struct sockaddr_in6 sa_in6;
#endif
	} peer;
	unsigned int connected;
	unsigned int _errno;
	unsigned int mtu;
	struct bio_dgram_sctp_sndinfo sndinfo;
	struct bio_dgram_sctp_rcvinfo rcvinfo;
	struct bio_dgram_sctp_prinfo prinfo;
	void (*handle_notifications)(BIO *bio, void *context, void *buf);
	void* notification_context;
	int in_handshake;
	int ccs_rcvd;
	int ccs_sent;
	int save_shutdown;
	int peer_auth_tested;
	bio_dgram_sctp_save_message saved_message;
	} bio_dgram_sctp_data;
#endif

BIO_METHOD *BIO_s_datagram(void)
	{
	return(&methods_dgramp);
	}

BIO *BIO_new_dgram(int fd, int close_flag)
	{
	BIO *ret;

	ret=BIO_new(BIO_s_datagram());
	if (ret == NULL) return(NULL);
	BIO_set_fd(ret,fd,close_flag);
	return(ret);
	}

static int dgram_new(BIO *bi)
	{
	bio_dgram_data *data = NULL;

	bi->init=0;
	bi->num=0;
	data = OPENSSL_malloc(sizeof(bio_dgram_data));
	if (data == NULL)
		return 0;
	memset(data, 0x00, sizeof(bio_dgram_data));
    bi->ptr = data;

	bi->flags=0;
	return(1);
	}

static int dgram_free(BIO *a)
	{
	bio_dgram_data *data;

	if (a == NULL) return(0);
	if ( ! dgram_clear(a))
		return 0;

	data = (bio_dgram_data *)a->ptr;
	if(data != NULL) OPENSSL_free(data);

	return(1);
	}

static int dgram_clear(BIO *a)
	{
	if (a == NULL) return(0);
	if (a->shutdown)
		{
		if (a->init)
			{
			SHUTDOWN2(a->num);
			}
		a->init=0;
		a->flags=0;
		}
	return(1);
	}

static void dgram_adjust_rcv_timeout(BIO *b)
	{
#if defined(SO_RCVTIMEO)
	bio_dgram_data *data = (bio_dgram_data *)b->ptr;
	union { size_t s; int i; } sz = {0};

	/* Is a timer active? */
	if (data->next_timeout.tv_sec > 0 || data->next_timeout.tv_usec > 0)
		{
		struct timeval timenow, timeleft;

		/* Read current socket timeout */
#ifdef OPENSSL_SYS_WINDOWS
		int timeout;

		sz.i = sizeof(timeout);
		if (getsockopt(b->num, SOL_SOCKET, SO_RCVTIMEO,
					   (void*)&timeout, &sz.i) < 0)
			{ perror("getsockopt"); }
		else
			{
			data->socket_timeout.tv_sec = timeout / 1000;
			data->socket_timeout.tv_usec = (timeout % 1000) * 1000;
			}
#else
		sz.i = sizeof(data->socket_timeout);
		if ( getsockopt(b->num, SOL_SOCKET, SO_RCVTIMEO, 
						&(data->socket_timeout), (void *)&sz) < 0)
			{ perror("getsockopt"); }
		else if (sizeof(sz.s)!=sizeof(sz.i) && sz.i==0)
			OPENSSL_assert(sz.s<=sizeof(data->socket_timeout));
#endif

		/* Get current time */
		get_current_time(&timenow);

		/* Calculate time left until timer expires */
		memcpy(&timeleft, &(data->next_timeout), sizeof(struct timeval));
		timeleft.tv_sec -= timenow.tv_sec;
		timeleft.tv_usec -= timenow.tv_usec;
		if (timeleft.tv_usec < 0)
			{
			timeleft.tv_sec--;
			timeleft.tv_usec += 1000000;
			}

		if (timeleft.tv_sec < 0)
			{
			timeleft.tv_sec = 0;
			timeleft.tv_usec = 1;
			}

		/* Adjust socket timeout if next handhake message timer
		 * will expire earlier.
		 */
		if ((data->socket_timeout.tv_sec == 0 && data->socket_timeout.tv_usec == 0) ||
			(data->socket_timeout.tv_sec > timeleft.tv_sec) ||
			(data->socket_timeout.tv_sec == timeleft.tv_sec &&
			 data->socket_timeout.tv_usec >= timeleft.tv_usec))
			{
#ifdef OPENSSL_SYS_WINDOWS
			timeout = timeleft.tv_sec * 1000 + timeleft.tv_usec / 1000;
			if (setsockopt(b->num, SOL_SOCKET, SO_RCVTIMEO,
						   (void*)&timeout, sizeof(timeout)) < 0)
				{ perror("setsockopt"); }
#else
			if ( setsockopt(b->num, SOL_SOCKET, SO_RCVTIMEO, &timeleft,
							sizeof(struct timeval)) < 0)
				{ perror("setsockopt"); }
#endif
			}
		}
#endif
	}

static void dgram_reset_rcv_timeout(BIO *b)
	{
#if defined(SO_RCVTIMEO)
	bio_dgram_data *data = (bio_dgram_data *)b->ptr;

	/* Is a timer active? */
	if (data->next_timeout.tv_sec > 0 || data->next_timeout.tv_usec > 0)
		{
#ifdef OPENSSL_SYS_WINDOWS
		int timeout = data->socket_timeout.tv_sec * 1000 +
					  data->socket_timeout.tv_usec / 1000;
		if (setsockopt(b->num, SOL_SOCKET, SO_RCVTIMEO,
					   (void*)&timeout, sizeof(timeout)) < 0)
			{ perror("setsockopt"); }
#else
		if ( setsockopt(b->num, SOL_SOCKET, SO_RCVTIMEO, &(data->socket_timeout),
						sizeof(struct timeval)) < 0)
			{ perror("setsockopt"); }
#endif
		}
#endif
	}

static int dgram_read(BIO *b, char *out, int outl)
	{
	int ret=0;
	bio_dgram_data *data = (bio_dgram_data *)b->ptr;

	struct	{
	/*
	 * See commentary in b_sock.c. <appro>
	 */
	union	{ size_t s; int i; } len;
	union	{
		struct sockaddr sa;
		struct sockaddr_in sa_in;
#if OPENSSL_USE_IPV6
		struct sockaddr_in6 sa_in6;
#endif
		} peer;
	} sa;

	sa.len.s=0;
	sa.len.i=sizeof(sa.peer);

	if (out != NULL)
		{
		clear_socket_error();
		memset(&sa.peer, 0x00, sizeof(sa.peer));
		dgram_adjust_rcv_timeout(b);
		ret=recvfrom(b->num,out,outl,0,&sa.peer.sa,(void *)&sa.len);
		if (sizeof(sa.len.i)!=sizeof(sa.len.s) && sa.len.i==0)
			{
			OPENSSL_assert(sa.len.s<=sizeof(sa.peer));
			sa.len.i = (int)sa.len.s;
			}

		if ( ! data->connected  && ret >= 0)
			BIO_ctrl(b, BIO_CTRL_DGRAM_SET_PEER, 0, &sa.peer);

		BIO_clear_retry_flags(b);
		if (ret < 0)
			{
			if (BIO_dgram_should_retry(ret))
				{
				BIO_set_retry_read(b);
				data->_errno = get_last_socket_error();
				}
			}

		dgram_reset_rcv_timeout(b);
		}
	return(ret);
	}

static int dgram_write(BIO *b, const char *in, int inl)
	{
	int ret;
	bio_dgram_data *data = (bio_dgram_data *)b->ptr;
	clear_socket_error();

	if ( data->connected )
		ret=writesocket(b->num,in,inl);
	else
		{
		int peerlen = sizeof(data->peer);

		if (data->peer.sa.sa_family == AF_INET)
			peerlen = sizeof(data->peer.sa_in);
#if OPENSSL_USE_IPV6
		else if (data->peer.sa.sa_family == AF_INET6)
			peerlen = sizeof(data->peer.sa_in6);
#endif
#if defined(NETWARE_CLIB) && defined(NETWARE_BSDSOCK)
		ret=sendto(b->num, (char *)in, inl, 0, &data->peer.sa, peerlen);
#else
		ret=sendto(b->num, in, inl, 0, &data->peer.sa, peerlen);
#endif
		}

	BIO_clear_retry_flags(b);
	if (ret <= 0)
		{
		if (BIO_dgram_should_retry(ret))
			{
			BIO_set_retry_write(b);  
			data->_errno = get_last_socket_error();

#if 0 /* higher layers are responsible for querying MTU, if necessary */
			if ( data->_errno == EMSGSIZE)
				/* retrieve the new MTU */
				BIO_ctrl(b, BIO_CTRL_DGRAM_QUERY_MTU, 0, NULL);
#endif
			}
		}
	return(ret);
	}

static long dgram_ctrl(BIO *b, int cmd, long num, void *ptr)
	{
	long ret=1;
	int *ip;
	struct sockaddr *to = NULL;
	bio_dgram_data *data = NULL;
#if defined(OPENSSL_SYS_LINUX) && (defined(IP_MTU_DISCOVER) || defined(IP_MTU))
	int sockopt_val = 0;
	socklen_t sockopt_len;	/* assume that system supporting IP_MTU is
				 * modern enough to define socklen_t */
	socklen_t addr_len;
	union	{
		struct sockaddr	sa;
		struct sockaddr_in s4;
#if OPENSSL_USE_IPV6
		struct sockaddr_in6 s6;
#endif
		} addr;
#endif

	data = (bio_dgram_data *)b->ptr;

	switch (cmd)
		{
	case BIO_CTRL_RESET:
		num=0;
	case BIO_C_FILE_SEEK:
		ret=0;
		break;
	case BIO_C_FILE_TELL:
	case BIO_CTRL_INFO:
		ret=0;
		break;
	case BIO_C_SET_FD:
		dgram_clear(b);
		b->num= *((int *)ptr);
		b->shutdown=(int)num;
		b->init=1;
		break;
	case BIO_C_GET_FD:
		if (b->init)
			{
			ip=(int *)ptr;
			if (ip != NULL) *ip=b->num;
			ret=b->num;
			}
		else
			ret= -1;
		break;
	case BIO_CTRL_GET_CLOSE:
		ret=b->shutdown;
		break;
	case BIO_CTRL_SET_CLOSE:
		b->shutdown=(int)num;
		break;
	case BIO_CTRL_PENDING:
	case BIO_CTRL_WPENDING:
		ret=0;
		break;
	case BIO_CTRL_DUP:
	case BIO_CTRL_FLUSH:
		ret=1;
		break;
	case BIO_CTRL_DGRAM_CONNECT:
		to = (struct sockaddr *)ptr;
#if 0
		if (connect(b->num, to, sizeof(struct sockaddr)) < 0)
			{ perror("connect"); ret = 0; }
		else
			{
#endif
			switch (to->sa_family)
				{
				case AF_INET:
					memcpy(&data->peer,to,sizeof(data->peer.sa_in));
					break;
#if OPENSSL_USE_IPV6
				case AF_INET6:
					memcpy(&data->peer,to,sizeof(data->peer.sa_in6));
					break;
#endif
				default:
					memcpy(&data->peer,to,sizeof(data->peer.sa));
					break;
				}
#if 0
			}
#endif
		break;
		/* (Linux)kernel sets DF bit on outgoing IP packets */
	case BIO_CTRL_DGRAM_MTU_DISCOVER:
#if defined(OPENSSL_SYS_LINUX) && defined(IP_MTU_DISCOVER) && defined(IP_PMTUDISC_DO)
		addr_len = (socklen_t)sizeof(addr);
		memset((void *)&addr, 0, sizeof(addr));
		if (getsockname(b->num, &addr.sa, &addr_len) < 0)
			{
			ret = 0;
			break;
			}
		switch (addr.sa.sa_family)
			{
		case AF_INET:
			sockopt_val = IP_PMTUDISC_DO;
			if ((ret = setsockopt(b->num, IPPROTO_IP, IP_MTU_DISCOVER,
				&sockopt_val, sizeof(sockopt_val))) < 0)
				perror("setsockopt");
			break;
#if OPENSSL_USE_IPV6 && defined(IPV6_MTU_DISCOVER) && defined(IPV6_PMTUDISC_DO)
		case AF_INET6:
			sockopt_val = IPV6_PMTUDISC_DO;
			if ((ret = setsockopt(b->num, IPPROTO_IPV6, IPV6_MTU_DISCOVER,
				&sockopt_val, sizeof(sockopt_val))) < 0)
				perror("setsockopt");
			break;
#endif
		default:
			ret = -1;
			break;
			}
		ret = -1;
#else
		break;
#endif
	case BIO_CTRL_DGRAM_QUERY_MTU:
#if defined(OPENSSL_SYS_LINUX) && defined(IP_MTU)
		addr_len = (socklen_t)sizeof(addr);
		memset((void *)&addr, 0, sizeof(addr));
		if (getsockname(b->num, &addr.sa, &addr_len) < 0)
			{
			ret = 0;
			break;
			}
		sockopt_len = sizeof(sockopt_val);
		switch (addr.sa.sa_family)
			{
		case AF_INET:
			if ((ret = getsockopt(b->num, IPPROTO_IP, IP_MTU, (void *)&sockopt_val,
				&sockopt_len)) < 0 || sockopt_val < 0)
				{
				ret = 0;
				}
			else
				{
				/* we assume that the transport protocol is UDP and no
				 * IP options are used.
				 */
				data->mtu = sockopt_val - 8 - 20;
				ret = data->mtu;
				}
			break;
#if OPENSSL_USE_IPV6 && defined(IPV6_MTU)
		case AF_INET6:
			if ((ret = getsockopt(b->num, IPPROTO_IPV6, IPV6_MTU, (void *)&sockopt_val,
				&sockopt_len)) < 0 || sockopt_val < 0)
				{
				ret = 0;
				}
			else
				{
				/* we assume that the transport protocol is UDP and no
				 * IPV6 options are used.
				 */
				data->mtu = sockopt_val - 8 - 40;
				ret = data->mtu;
				}
			break;
#endif
		default:
			ret = 0;
			break;
			}
#else
		ret = 0;
#endif
		break;
	case BIO_CTRL_DGRAM_GET_FALLBACK_MTU:
		switch (data->peer.sa.sa_family)
			{
			case AF_INET:
				ret = 576 - 20 - 8;
				break;
#if OPENSSL_USE_IPV6
			case AF_INET6:
#ifdef IN6_IS_ADDR_V4MAPPED
				if (IN6_IS_ADDR_V4MAPPED(&data->peer.sa_in6.sin6_addr))
					ret = 576 - 20 - 8;
				else
#endif
					ret = 1280 - 40 - 8;
				break;
#endif
			default:
				ret = 576 - 20 - 8;
				break;
			}
		break;
	case BIO_CTRL_DGRAM_GET_MTU:
		return data->mtu;
		break;
	case BIO_CTRL_DGRAM_SET_MTU:
		data->mtu = num;
		ret = num;
		break;
	case BIO_CTRL_DGRAM_SET_CONNECTED:
		to = (struct sockaddr *)ptr;

		if ( to != NULL)
			{
			data->connected = 1;
			switch (to->sa_family)
				{
				case AF_INET:
					memcpy(&data->peer,to,sizeof(data->peer.sa_in));
					break;
#if OPENSSL_USE_IPV6
				case AF_INET6:
					memcpy(&data->peer,to,sizeof(data->peer.sa_in6));
					break;
#endif
				default:
					memcpy(&data->peer,to,sizeof(data->peer.sa));
					break;
				}
			}
		else
			{
			data->connected = 0;
			memset(&(data->peer), 0x00, sizeof(data->peer));
			}
		break;
	case BIO_CTRL_DGRAM_GET_PEER:
		switch (data->peer.sa.sa_family)
			{
			case AF_INET:
				ret=sizeof(data->peer.sa_in);
				break;
#if OPENSSL_USE_IPV6
			case AF_INET6:
				ret=sizeof(data->peer.sa_in6);
				break;
#endif
			default:
				ret=sizeof(data->peer.sa);
				break;
			}
		if (num==0 || num>ret)
			num=ret;
		memcpy(ptr,&data->peer,(ret=num));
		break;
	case BIO_CTRL_DGRAM_SET_PEER:
		to = (struct sockaddr *) ptr;
		switch (to->sa_family)
			{
			case AF_INET:
				memcpy(&data->peer,to,sizeof(data->peer.sa_in));
				break;
#if OPENSSL_USE_IPV6
			case AF_INET6:
				memcpy(&data->peer,to,sizeof(data->peer.sa_in6));
				break;
#endif
			default:
				memcpy(&data->peer,to,sizeof(data->peer.sa));
				break;
			}
		break;
	case BIO_CTRL_DGRAM_SET_NEXT_TIMEOUT:
		memcpy(&(data->next_timeout), ptr, sizeof(struct timeval));
		break;
#if defined(SO_RCVTIMEO)
	case BIO_CTRL_DGRAM_SET_RECV_TIMEOUT:
#ifdef OPENSSL_SYS_WINDOWS
		{
		struct timeval *tv = (struct timeval *)ptr;
		int timeout = tv->tv_sec * 1000 + tv->tv_usec/1000;
		if (setsockopt(b->num, SOL_SOCKET, SO_RCVTIMEO,
			(void*)&timeout, sizeof(timeout)) < 0)
			{ perror("setsockopt"); ret = -1; }
		}
#else
		if ( setsockopt(b->num, SOL_SOCKET, SO_RCVTIMEO, ptr,
			sizeof(struct timeval)) < 0)
			{ perror("setsockopt");	ret = -1; }
#endif
		break;
	case BIO_CTRL_DGRAM_GET_RECV_TIMEOUT:
		{
		union { size_t s; int i; } sz = {0};
#ifdef OPENSSL_SYS_WINDOWS
		int timeout;
		struct timeval *tv = (struct timeval *)ptr;

		sz.i = sizeof(timeout);
		if (getsockopt(b->num, SOL_SOCKET, SO_RCVTIMEO,
			(void*)&timeout, &sz.i) < 0)
			{ perror("getsockopt"); ret = -1; }
		else
			{
			tv->tv_sec = timeout / 1000;
			tv->tv_usec = (timeout % 1000) * 1000;
			ret = sizeof(*tv);
			}
#else
		sz.i = sizeof(struct timeval);
		if ( getsockopt(b->num, SOL_SOCKET, SO_RCVTIMEO, 
			ptr, (void *)&sz) < 0)
			{ perror("getsockopt"); ret = -1; }
		else if (sizeof(sz.s)!=sizeof(sz.i) && sz.i==0)
			{
			OPENSSL_assert(sz.s<=sizeof(struct timeval));
			ret = (int)sz.s;
			}
		else
			ret = sz.i;
#endif
		}
		break;
#endif
#if defined(SO_SNDTIMEO)
	case BIO_CTRL_DGRAM_SET_SEND_TIMEOUT:
#ifdef OPENSSL_SYS_WINDOWS
		{
		struct timeval *tv = (struct timeval *)ptr;
		int timeout = tv->tv_sec * 1000 + tv->tv_usec/1000;
		if (setsockopt(b->num, SOL_SOCKET, SO_SNDTIMEO,
			(void*)&timeout, sizeof(timeout)) < 0)
			{ perror("setsockopt"); ret = -1; }
		}
#else
		if ( setsockopt(b->num, SOL_SOCKET, SO_SNDTIMEO, ptr,
			sizeof(struct timeval)) < 0)
			{ perror("setsockopt");	ret = -1; }
#endif
		break;
	case BIO_CTRL_DGRAM_GET_SEND_TIMEOUT:
		{
		union { size_t s; int i; } sz = {0};
#ifdef OPENSSL_SYS_WINDOWS
		int timeout;
		struct timeval *tv = (struct timeval *)ptr;

		sz.i = sizeof(timeout);
		if (getsockopt(b->num, SOL_SOCKET, SO_SNDTIMEO,
			(void*)&timeout, &sz.i) < 0)
			{ perror("getsockopt"); ret = -1; }
		else
			{
			tv->tv_sec = timeout / 1000;
			tv->tv_usec = (timeout % 1000) * 1000;
			ret = sizeof(*tv);
			}
#else
		sz.i = sizeof(struct timeval);
		if ( getsockopt(b->num, SOL_SOCKET, SO_SNDTIMEO, 
			ptr, (void *)&sz) < 0)
			{ perror("getsockopt"); ret = -1; }
		else if (sizeof(sz.s)!=sizeof(sz.i) && sz.i==0)
			{
			OPENSSL_assert(sz.s<=sizeof(struct timeval));
			ret = (int)sz.s;
			}
		else
			ret = sz.i;
#endif
		}
		break;
#endif
	case BIO_CTRL_DGRAM_GET_SEND_TIMER_EXP:
		/* fall-through */
	case BIO_CTRL_DGRAM_GET_RECV_TIMER_EXP:
#ifdef OPENSSL_SYS_WINDOWS
		if ( data->_errno == WSAETIMEDOUT)
#else
		if ( data->_errno == EAGAIN)
#endif
			{
			ret = 1;
			data->_errno = 0;
			}
		else
			ret = 0;
		break;
#ifdef EMSGSIZE
	case BIO_CTRL_DGRAM_MTU_EXCEEDED:
		if ( data->_errno == EMSGSIZE)
			{
			ret = 1;
			data->_errno = 0;
			}
		else
			ret = 0;
		break;
#endif
	default:
		ret=0;
		break;
		}
	return(ret);
	}

static int dgram_puts(BIO *bp, const char *str)
	{
	int n,ret;

	n=strlen(str);
	ret=dgram_write(bp,str,n);
	return(ret);
	}

#ifndef OPENSSL_NO_SCTP
BIO_METHOD *BIO_s_datagram_sctp(void)
	{
	return(&methods_dgramp_sctp);
	}

BIO *BIO_new_dgram_sctp(int fd, int close_flag)
	{
	BIO *bio;
	int ret, optval = 20000;
	int auth_data = 0, auth_forward = 0;
	unsigned char *p;
	struct sctp_authchunk auth;
	struct sctp_authchunks *authchunks;
	socklen_t sockopt_len;
#ifdef SCTP_AUTHENTICATION_EVENT
#ifdef SCTP_EVENT
	struct sctp_event event;
#else
	struct sctp_event_subscribe event;
#endif
#endif

	bio=BIO_new(BIO_s_datagram_sctp());
	if (bio == NULL) return(NULL);
	BIO_set_fd(bio,fd,close_flag);

	/* Activate SCTP-AUTH for DATA and FORWARD-TSN chunks */
	auth.sauth_chunk = OPENSSL_SCTP_DATA_CHUNK_TYPE;
	ret = setsockopt(fd, IPPROTO_SCTP, SCTP_AUTH_CHUNK, &auth, sizeof(struct sctp_authchunk));
	OPENSSL_assert(ret >= 0);
	auth.sauth_chunk = OPENSSL_SCTP_FORWARD_CUM_TSN_CHUNK_TYPE;
	ret = setsockopt(fd, IPPROTO_SCTP, SCTP_AUTH_CHUNK, &auth, sizeof(struct sctp_authchunk));
	OPENSSL_assert(ret >= 0);

	/* Test if activation was successful. When using accept(),
	 * SCTP-AUTH has to be activated for the listening socket
	 * already, otherwise the connected socket won't use it. */
	sockopt_len = (socklen_t)(sizeof(sctp_assoc_t) + 256 * sizeof(uint8_t));
	authchunks = OPENSSL_malloc(sockopt_len);
	memset(authchunks, 0, sizeof(sockopt_len));
	ret = getsockopt(fd, IPPROTO_SCTP, SCTP_LOCAL_AUTH_CHUNKS, authchunks, &sockopt_len);
	OPENSSL_assert(ret >= 0);
	
	for (p = (unsigned char*) authchunks + sizeof(sctp_assoc_t);
	     p < (unsigned char*) authchunks + sockopt_len;
	     p += sizeof(uint8_t))
		{
		if (*p == OPENSSL_SCTP_DATA_CHUNK_TYPE) auth_data = 1;
		if (*p == OPENSSL_SCTP_FORWARD_CUM_TSN_CHUNK_TYPE) auth_forward = 1;
		}
		
	OPENSSL_free(authchunks);

	OPENSSL_assert(auth_data);
	OPENSSL_assert(auth_forward);

#ifdef SCTP_AUTHENTICATION_EVENT
#ifdef SCTP_EVENT
	memset(&event, 0, sizeof(struct sctp_event));
	event.se_assoc_id = 0;
	event.se_type = SCTP_AUTHENTICATION_EVENT;
	event.se_on = 1;
	ret = setsockopt(fd, IPPROTO_SCTP, SCTP_EVENT, &event, sizeof(struct sctp_event));
	OPENSSL_assert(ret >= 0);
#else
	sockopt_len = (socklen_t) sizeof(struct sctp_event_subscribe);
	ret = getsockopt(fd, IPPROTO_SCTP, SCTP_EVENTS, &event, &sockopt_len);
	OPENSSL_assert(ret >= 0);

	event.sctp_authentication_event = 1;

	ret = setsockopt(fd, IPPROTO_SCTP, SCTP_EVENTS, &event, sizeof(struct sctp_event_subscribe));
	OPENSSL_assert(ret >= 0);
#endif
#endif

	/* Disable partial delivery by setting the min size
	 * larger than the max record size of 2^14 + 2048 + 13
	 */
	ret = setsockopt(fd, IPPROTO_SCTP, SCTP_PARTIAL_DELIVERY_POINT, &optval, sizeof(optval));
	OPENSSL_assert(ret >= 0);

	return(bio);
	}

int BIO_dgram_is_sctp(BIO *bio)
	{
	return (BIO_method_type(bio) == BIO_TYPE_DGRAM_SCTP);
	}

static int dgram_sctp_new(BIO *bi)
	{
	bio_dgram_sctp_data *data = NULL;

	bi->init=0;
	bi->num=0;
	data = OPENSSL_malloc(sizeof(bio_dgram_sctp_data));
	if (data == NULL)
		return 0;
	memset(data, 0x00, sizeof(bio_dgram_sctp_data));
#ifdef SCTP_PR_SCTP_NONE
	data->prinfo.pr_policy = SCTP_PR_SCTP_NONE;
#endif
    bi->ptr = data;

	bi->flags=0;
	return(1);
	}

static int dgram_sctp_free(BIO *a)
	{
	bio_dgram_sctp_data *data;

	if (a == NULL) return(0);
	if ( ! dgram_clear(a))
		return 0;

	data = (bio_dgram_sctp_data *)a->ptr;
	if(data != NULL) OPENSSL_free(data);

	return(1);
	}

#ifdef SCTP_AUTHENTICATION_EVENT
void dgram_sctp_handle_auth_free_key_event(BIO *b, union sctp_notification *snp)
	{
	int ret;
	struct sctp_authkey_event* authkeyevent = &snp->sn_auth_event;

	if (authkeyevent->auth_indication == SCTP_AUTH_FREE_KEY)
		{
		struct sctp_authkeyid authkeyid;

		/* delete key */
		authkeyid.scact_keynumber = authkeyevent->auth_keynumber;
		ret = setsockopt(b->num, IPPROTO_SCTP, SCTP_AUTH_DELETE_KEY,
		      &authkeyid, sizeof(struct sctp_authkeyid));
		}
	}
#endif

static int dgram_sctp_read(BIO *b, char *out, int outl)
	{
	int ret = 0, n = 0, i, optval;
	socklen_t optlen;
	bio_dgram_sctp_data *data = (bio_dgram_sctp_data *)b->ptr;
	union sctp_notification *snp;
	struct msghdr msg;
	struct iovec iov;
	struct cmsghdr *cmsg;
	char cmsgbuf[512];

	if (out != NULL)
		{
		clear_socket_error();

		do
			{
			memset(&data->rcvinfo, 0x00, sizeof(struct bio_dgram_sctp_rcvinfo));
			iov.iov_base = out;
			iov.iov_len = outl;
			msg.msg_name = NULL;
			msg.msg_namelen = 0;
			msg.msg_iov = &iov;
			msg.msg_iovlen = 1;
			msg.msg_control = cmsgbuf;
			msg.msg_controllen = 512;
			msg.msg_flags = 0;
			n = recvmsg(b->num, &msg, 0);

			if (msg.msg_controllen > 0)
				{
				for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg))
					{
					if (cmsg->cmsg_level != IPPROTO_SCTP)
						continue;
#ifdef SCTP_RCVINFO
					if (cmsg->cmsg_type == SCTP_RCVINFO)
						{
						struct sctp_rcvinfo *rcvinfo;

						rcvinfo = (struct sctp_rcvinfo *)CMSG_DATA(cmsg);
						data->rcvinfo.rcv_sid = rcvinfo->rcv_sid;
						data->rcvinfo.rcv_ssn = rcvinfo->rcv_ssn;
						data->rcvinfo.rcv_flags = rcvinfo->rcv_flags;
						data->rcvinfo.rcv_ppid = rcvinfo->rcv_ppid;
						data->rcvinfo.rcv_tsn = rcvinfo->rcv_tsn;
						data->rcvinfo.rcv_cumtsn = rcvinfo->rcv_cumtsn;
						data->rcvinfo.rcv_context = rcvinfo->rcv_context;
						}
#endif
#ifdef SCTP_SNDRCV
					if (cmsg->cmsg_type == SCTP_SNDRCV)
						{
						struct sctp_sndrcvinfo *sndrcvinfo;

						sndrcvinfo = (struct sctp_sndrcvinfo *)CMSG_DATA(cmsg);
						data->rcvinfo.rcv_sid = sndrcvinfo->sinfo_stream;
						data->rcvinfo.rcv_ssn = sndrcvinfo->sinfo_ssn;
						data->rcvinfo.rcv_flags = sndrcvinfo->sinfo_flags;
						data->rcvinfo.rcv_ppid = sndrcvinfo->sinfo_ppid;
						data->rcvinfo.rcv_tsn = sndrcvinfo->sinfo_tsn;
						data->rcvinfo.rcv_cumtsn = sndrcvinfo->sinfo_cumtsn;
						data->rcvinfo.rcv_context = sndrcvinfo->sinfo_context;
						}
#endif
					}
				}

			if (n <= 0)
				{
				if (n < 0)
					ret = n;
				break;
				}

			if (msg.msg_flags & MSG_NOTIFICATION)
				{
				snp = (union sctp_notification*) out;
				if (snp->sn_header.sn_type == SCTP_SENDER_DRY_EVENT)
					{
#ifdef SCTP_EVENT
					struct sctp_event event;
#else
					struct sctp_event_subscribe event;
					socklen_t eventsize;
#endif
					/* If a message has been delayed until the socket
					 * is dry, it can be sent now.
					 */
					if (data->saved_message.length > 0)
						{
						dgram_sctp_write(data->saved_message.bio, data->saved_message.data,
						                 data->saved_message.length);
						OPENSSL_free(data->saved_message.data);
						data->saved_message.length = 0;
						}

					/* disable sender dry event */
#ifdef SCTP_EVENT
					memset(&event, 0, sizeof(struct sctp_event));
					event.se_assoc_id = 0;
					event.se_type = SCTP_SENDER_DRY_EVENT;
					event.se_on = 0;
					i = setsockopt(b->num, IPPROTO_SCTP, SCTP_EVENT, &event, sizeof(struct sctp_event));
					OPENSSL_assert(i >= 0);
#else
					eventsize = sizeof(struct sctp_event_subscribe);
					i = getsockopt(b->num, IPPROTO_SCTP, SCTP_EVENTS, &event, &eventsize);
					OPENSSL_assert(i >= 0);

					event.sctp_sender_dry_event = 0;

					i = setsockopt(b->num, IPPROTO_SCTP, SCTP_EVENTS, &event, sizeof(struct sctp_event_subscribe));
					OPENSSL_assert(i >= 0);
#endif
					}

#ifdef SCTP_AUTHENTICATION_EVENT
				if (snp->sn_header.sn_type == SCTP_AUTHENTICATION_EVENT)
					dgram_sctp_handle_auth_free_key_event(b, snp);
#endif

				if (data->handle_notifications != NULL)
					data->handle_notifications(b, data->notification_context, (void*) out);

				memset(out, 0, outl);
				}
			else
				ret += n;
			}
		while ((msg.msg_flags & MSG_NOTIFICATION) && (msg.msg_flags & MSG_EOR) && (ret < outl));

		if (ret > 0 && !(msg.msg_flags & MSG_EOR))
			{
			/* Partial message read, this should never happen! */

			/* The buffer was too small, this means the peer sent
			 * a message that was larger than allowed. */
			if (ret == outl)
				return -1;

			/* Test if socket buffer can handle max record
			 * size (2^14 + 2048 + 13)
			 */
			optlen = (socklen_t) sizeof(int);
			ret = getsockopt(b->num, SOL_SOCKET, SO_RCVBUF, &optval, &optlen);
			OPENSSL_assert(ret >= 0);
			OPENSSL_assert(optval >= 18445);

			/* Test if SCTP doesn't partially deliver below
			 * max record size (2^14 + 2048 + 13)
			 */
			optlen = (socklen_t) sizeof(int);
			ret = getsockopt(b->num, IPPROTO_SCTP, SCTP_PARTIAL_DELIVERY_POINT,
			                 &optval, &optlen);
			OPENSSL_assert(ret >= 0);
			OPENSSL_assert(optval >= 18445);

			/* Partially delivered notification??? Probably a bug.... */
			OPENSSL_assert(!(msg.msg_flags & MSG_NOTIFICATION));

			/* Everything seems ok till now, so it's most likely
			 * a message dropped by PR-SCTP.
			 */
			memset(out, 0, outl);
			BIO_set_retry_read(b);
			return -1;
			}

		BIO_clear_retry_flags(b);
		if (ret < 0)
			{
			if (BIO_dgram_should_retry(ret))
				{
				BIO_set_retry_read(b);
				data->_errno = get_last_socket_error();
				}
			}

		/* Test if peer uses SCTP-AUTH before continuing */
		if (!data->peer_auth_tested)
			{
			int ii, auth_data = 0, auth_forward = 0;
			unsigned char *p;
			struct sctp_authchunks *authchunks;

			optlen = (socklen_t)(sizeof(sctp_assoc_t) + 256 * sizeof(uint8_t));
			authchunks = OPENSSL_malloc(optlen);
			memset(authchunks, 0, sizeof(optlen));
			ii = getsockopt(b->num, IPPROTO_SCTP, SCTP_PEER_AUTH_CHUNKS, authchunks, &optlen);
			OPENSSL_assert(ii >= 0);

			for (p = (unsigned char*) authchunks + sizeof(sctp_assoc_t);
				 p < (unsigned char*) authchunks + optlen;
				 p += sizeof(uint8_t))
				{
				if (*p == OPENSSL_SCTP_DATA_CHUNK_TYPE) auth_data = 1;
				if (*p == OPENSSL_SCTP_FORWARD_CUM_TSN_CHUNK_TYPE) auth_forward = 1;
				}

			OPENSSL_free(authchunks);

			if (!auth_data || !auth_forward)
				{
				BIOerr(BIO_F_DGRAM_SCTP_READ,BIO_R_CONNECT_ERROR);
				return -1;
				}

			data->peer_auth_tested = 1;
			}
		}
	return(ret);
	}

static int dgram_sctp_write(BIO *b, const char *in, int inl)
	{
	int ret;
	bio_dgram_sctp_data *data = (bio_dgram_sctp_data *)b->ptr;
	struct bio_dgram_sctp_sndinfo *sinfo = &(data->sndinfo);
	struct bio_dgram_sctp_prinfo *pinfo = &(data->prinfo);
	struct bio_dgram_sctp_sndinfo handshake_sinfo;
	struct iovec iov[1];
	struct msghdr msg;
	struct cmsghdr *cmsg;
#if defined(SCTP_SNDINFO) && defined(SCTP_PRINFO)
	char cmsgbuf[CMSG_SPACE(sizeof(struct sctp_sndinfo)) + CMSG_SPACE(sizeof(struct sctp_prinfo))];
	struct sctp_sndinfo *sndinfo;
	struct sctp_prinfo *prinfo;
#else
	char cmsgbuf[CMSG_SPACE(sizeof(struct sctp_sndrcvinfo))];
	struct sctp_sndrcvinfo *sndrcvinfo;
#endif

	clear_socket_error();

	/* If we're send anything else than application data,
	 * disable all user parameters and flags.
	 */
	if (in[0] != 23) {
		memset(&handshake_sinfo, 0x00, sizeof(struct bio_dgram_sctp_sndinfo));
#ifdef SCTP_SACK_IMMEDIATELY
		handshake_sinfo.snd_flags = SCTP_SACK_IMMEDIATELY;
#endif
		sinfo = &handshake_sinfo;
	}

	/* If we have to send a shutdown alert message and the
	 * socket is not dry yet, we have to save it and send it
	 * as soon as the socket gets dry.
	 */
	if (data->save_shutdown && !BIO_dgram_sctp_wait_for_dry(b))
	{
		data->saved_message.bio = b;
		data->saved_message.length = inl;
		data->saved_message.data = OPENSSL_malloc(inl);
		memcpy(data->saved_message.data, in, inl);
		return inl;
	}

	iov[0].iov_base = (char *)in;
	iov[0].iov_len = inl;
	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;
	msg.msg_control = (caddr_t)cmsgbuf;
	msg.msg_controllen = 0;
	msg.msg_flags = 0;
#if defined(SCTP_SNDINFO) && defined(SCTP_PRINFO)
	cmsg = (struct cmsghdr *)cmsgbuf;
	cmsg->cmsg_level = IPPROTO_SCTP;
	cmsg->cmsg_type = SCTP_SNDINFO;
	cmsg->cmsg_len = CMSG_LEN(sizeof(struct sctp_sndinfo));
	sndinfo = (struct sctp_sndinfo *)CMSG_DATA(cmsg);
	memset(sndinfo, 0, sizeof(struct sctp_sndinfo));
	sndinfo->snd_sid = sinfo->snd_sid;
	sndinfo->snd_flags = sinfo->snd_flags;
	sndinfo->snd_ppid = sinfo->snd_ppid;
	sndinfo->snd_context = sinfo->snd_context;
	msg.msg_controllen += CMSG_SPACE(sizeof(struct sctp_sndinfo));

	cmsg = (struct cmsghdr *)&cmsgbuf[CMSG_SPACE(sizeof(struct sctp_sndinfo))];
	cmsg->cmsg_level = IPPROTO_SCTP;
	cmsg->cmsg_type = SCTP_PRINFO;
	cmsg->cmsg_len = CMSG_LEN(sizeof(struct sctp_prinfo));
	prinfo = (struct sctp_prinfo *)CMSG_DATA(cmsg);
	memset(prinfo, 0, sizeof(struct sctp_prinfo));
	prinfo->pr_policy = pinfo->pr_policy;
	prinfo->pr_value = pinfo->pr_value;
	msg.msg_controllen += CMSG_SPACE(sizeof(struct sctp_prinfo));
#else
	cmsg = (struct cmsghdr *)cmsgbuf;
	cmsg->cmsg_level = IPPROTO_SCTP;
	cmsg->cmsg_type = SCTP_SNDRCV;
	cmsg->cmsg_len = CMSG_LEN(sizeof(struct sctp_sndrcvinfo));
	sndrcvinfo = (struct sctp_sndrcvinfo *)CMSG_DATA(cmsg);
	memset(sndrcvinfo, 0, sizeof(struct sctp_sndrcvinfo));
	sndrcvinfo->sinfo_stream = sinfo->snd_sid;
	sndrcvinfo->sinfo_flags = sinfo->snd_flags;
#ifdef __FreeBSD__
	sndrcvinfo->sinfo_flags |= pinfo->pr_policy;
#endif
	sndrcvinfo->sinfo_ppid = sinfo->snd_ppid;
	sndrcvinfo->sinfo_context = sinfo->snd_context;
	sndrcvinfo->sinfo_timetolive = pinfo->pr_value;
	msg.msg_controllen += CMSG_SPACE(sizeof(struct sctp_sndrcvinfo));
#endif

	ret = sendmsg(b->num, &msg, 0);

	BIO_clear_retry_flags(b);
	if (ret <= 0)
		{
		if (BIO_dgram_should_retry(ret))
			{
			BIO_set_retry_write(b);  
			data->_errno = get_last_socket_error();
			}
		}
	return(ret);
	}

static long dgram_sctp_ctrl(BIO *b, int cmd, long num, void *ptr)
	{
	long ret=1;
	bio_dgram_sctp_data *data = NULL;
	socklen_t sockopt_len = 0;
	struct sctp_authkeyid authkeyid;
	struct sctp_authkey *authkey;

	data = (bio_dgram_sctp_data *)b->ptr;

	switch (cmd)
		{
	case BIO_CTRL_DGRAM_QUERY_MTU:
		/* Set to maximum (2^14)
		 * and ignore user input to enable transport
		 * protocol fragmentation.
		 * Returns always 2^14.
		 */
		data->mtu = 16384;
		ret = data->mtu;
		break;
	case BIO_CTRL_DGRAM_SET_MTU:
		/* Set to maximum (2^14)
		 * and ignore input to enable transport
		 * protocol fragmentation.
		 * Returns always 2^14.
		 */
		data->mtu = 16384;
		ret = data->mtu;
		break;
	case BIO_CTRL_DGRAM_SET_CONNECTED:
	case BIO_CTRL_DGRAM_CONNECT:
		/* Returns always -1. */
		ret = -1;
		break;
	case BIO_CTRL_DGRAM_SET_NEXT_TIMEOUT:
		/* SCTP doesn't need the DTLS timer
		 * Returns always 1.
		 */
		break;
	case BIO_CTRL_DGRAM_SCTP_SET_IN_HANDSHAKE:
		if (num > 0)
			data->in_handshake = 1;
		else
			data->in_handshake = 0;

		ret = setsockopt(b->num, IPPROTO_SCTP, SCTP_NODELAY, &data->in_handshake, sizeof(int));
		break;
	case BIO_CTRL_DGRAM_SCTP_ADD_AUTH_KEY:
		/* New shared key for SCTP AUTH.
		 * Returns 0 on success, -1 otherwise.
		 */

		/* Get active key */
		sockopt_len = sizeof(struct sctp_authkeyid);
		ret = getsockopt(b->num, IPPROTO_SCTP, SCTP_AUTH_ACTIVE_KEY, &authkeyid, &sockopt_len);
		if (ret < 0) break;

		/* Add new key */
		sockopt_len = sizeof(struct sctp_authkey) + 64 * sizeof(uint8_t);
		authkey = OPENSSL_malloc(sockopt_len);
		memset(authkey, 0x00, sockopt_len);
		authkey->sca_keynumber = authkeyid.scact_keynumber + 1;
#ifndef __FreeBSD__
		/* This field is missing in FreeBSD 8.2 and earlier,
		 * and FreeBSD 8.3 and higher work without it.
		 */
		authkey->sca_keylength = 64;
#endif
		memcpy(&authkey->sca_key[0], ptr, 64 * sizeof(uint8_t));

		ret = setsockopt(b->num, IPPROTO_SCTP, SCTP_AUTH_KEY, authkey, sockopt_len);
		if (ret < 0) break;

		/* Reset active key */
		ret = setsockopt(b->num, IPPROTO_SCTP, SCTP_AUTH_ACTIVE_KEY,
		      &authkeyid, sizeof(struct sctp_authkeyid));
		if (ret < 0) break;

		break;
	case BIO_CTRL_DGRAM_SCTP_NEXT_AUTH_KEY:
		/* Returns 0 on success, -1 otherwise. */

		/* Get active key */
		sockopt_len = sizeof(struct sctp_authkeyid);
		ret = getsockopt(b->num, IPPROTO_SCTP, SCTP_AUTH_ACTIVE_KEY, &authkeyid, &sockopt_len);
		if (ret < 0) break;

		/* Set active key */
		authkeyid.scact_keynumber = authkeyid.scact_keynumber + 1;
		ret = setsockopt(b->num, IPPROTO_SCTP, SCTP_AUTH_ACTIVE_KEY,
		      &authkeyid, sizeof(struct sctp_authkeyid));
		if (ret < 0) break;

		/* CCS has been sent, so remember that and fall through
		 * to check if we need to deactivate an old key
		 */
		data->ccs_sent = 1;

	case BIO_CTRL_DGRAM_SCTP_AUTH_CCS_RCVD:
		/* Returns 0 on success, -1 otherwise. */

		/* Has this command really been called or is this just a fall-through? */
		if (cmd == BIO_CTRL_DGRAM_SCTP_AUTH_CCS_RCVD)
			data->ccs_rcvd = 1;

		/* CSS has been both, received and sent, so deactivate an old key */
		if (data->ccs_rcvd == 1 && data->ccs_sent == 1)
			{
			/* Get active key */
			sockopt_len = sizeof(struct sctp_authkeyid);
			ret = getsockopt(b->num, IPPROTO_SCTP, SCTP_AUTH_ACTIVE_KEY, &authkeyid, &sockopt_len);
			if (ret < 0) break;

			/* Deactivate key or delete second last key if
			 * SCTP_AUTHENTICATION_EVENT is not available.
			 */
			authkeyid.scact_keynumber = authkeyid.scact_keynumber - 1;
#ifdef SCTP_AUTH_DEACTIVATE_KEY
			sockopt_len = sizeof(struct sctp_authkeyid);
			ret = setsockopt(b->num, IPPROTO_SCTP, SCTP_AUTH_DEACTIVATE_KEY,
			      &authkeyid, sockopt_len);
			if (ret < 0) break;
#endif
#ifndef SCTP_AUTHENTICATION_EVENT
			if (authkeyid.scact_keynumber > 0)
				{
				authkeyid.scact_keynumber = authkeyid.scact_keynumber - 1;
				ret = setsockopt(b->num, IPPROTO_SCTP, SCTP_AUTH_DELETE_KEY,
					  &authkeyid, sizeof(struct sctp_authkeyid));
				if (ret < 0) break;
				}
#endif

			data->ccs_rcvd = 0;
			data->ccs_sent = 0;
			}
		break;
	case BIO_CTRL_DGRAM_SCTP_GET_SNDINFO:
		/* Returns the size of the copied struct. */
		if (num > (long) sizeof(struct bio_dgram_sctp_sndinfo))
			num = sizeof(struct bio_dgram_sctp_sndinfo);

		memcpy(ptr, &(data->sndinfo), num);
		ret = num;
		break;
	case BIO_CTRL_DGRAM_SCTP_SET_SNDINFO:
		/* Returns the size of the copied struct. */
		if (num > (long) sizeof(struct bio_dgram_sctp_sndinfo))
			num = sizeof(struct bio_dgram_sctp_sndinfo);

		memcpy(&(data->sndinfo), ptr, num);
		break;
	case BIO_CTRL_DGRAM_SCTP_GET_RCVINFO:
		/* Returns the size of the copied struct. */
		if (num > (long) sizeof(struct bio_dgram_sctp_rcvinfo))
			num = sizeof(struct bio_dgram_sctp_rcvinfo);

		memcpy(ptr, &data->rcvinfo, num);

		ret = num;
		break;
	case BIO_CTRL_DGRAM_SCTP_SET_RCVINFO:
		/* Returns the size of the copied struct. */
		if (num > (long) sizeof(struct bio_dgram_sctp_rcvinfo))
			num = sizeof(struct bio_dgram_sctp_rcvinfo);

		memcpy(&(data->rcvinfo), ptr, num);
		break;
	case BIO_CTRL_DGRAM_SCTP_GET_PRINFO:
		/* Returns the size of the copied struct. */
		if (num > (long) sizeof(struct bio_dgram_sctp_prinfo))
			num = sizeof(struct bio_dgram_sctp_prinfo);

		memcpy(ptr, &(data->prinfo), num);
		ret = num;
		break;
	case BIO_CTRL_DGRAM_SCTP_SET_PRINFO:
		/* Returns the size of the copied struct. */
		if (num > (long) sizeof(struct bio_dgram_sctp_prinfo))
			num = sizeof(struct bio_dgram_sctp_prinfo);

		memcpy(&(data->prinfo), ptr, num);
		break;
	case BIO_CTRL_DGRAM_SCTP_SAVE_SHUTDOWN:
		/* Returns always 1. */
		if (num > 0)
			data->save_shutdown = 1;
		else
			data->save_shutdown = 0;
		break;

	default:
		/* Pass to default ctrl function to
		 * process SCTP unspecific commands
		 */
		ret=dgram_ctrl(b, cmd, num, ptr);
		break;
		}
	return(ret);
	}

int BIO_dgram_sctp_notification_cb(BIO *b,
                                   void (*handle_notifications)(BIO *bio, void *context, void *buf),
                                   void *context)
	{
	bio_dgram_sctp_data *data = (bio_dgram_sctp_data *) b->ptr;

	if (handle_notifications != NULL)
		{
		data->handle_notifications = handle_notifications;
		data->notification_context = context;
		}
	else
		return -1;

	return 0;
	}

int BIO_dgram_sctp_wait_for_dry(BIO *b)
{
	int is_dry = 0;
	int n, sockflags, ret;
	union sctp_notification snp;
	struct msghdr msg;
	struct iovec iov;
#ifdef SCTP_EVENT
	struct sctp_event event;
#else
	struct sctp_event_subscribe event;
	socklen_t eventsize;
#endif
	bio_dgram_sctp_data *data = (bio_dgram_sctp_data *)b->ptr;

	/* set sender dry event */
#ifdef SCTP_EVENT
	memset(&event, 0, sizeof(struct sctp_event));
	event.se_assoc_id = 0;
	event.se_type = SCTP_SENDER_DRY_EVENT;
	event.se_on = 1;
	ret = setsockopt(b->num, IPPROTO_SCTP, SCTP_EVENT, &event, sizeof(struct sctp_event));
#else
	eventsize = sizeof(struct sctp_event_subscribe);
	ret = getsockopt(b->num, IPPROTO_SCTP, SCTP_EVENTS, &event, &eventsize);
	if (ret < 0)
		return -1;
	
	event.sctp_sender_dry_event = 1;
	
	ret = setsockopt(b->num, IPPROTO_SCTP, SCTP_EVENTS, &event, sizeof(struct sctp_event_subscribe));
#endif
	if (ret < 0)
		return -1;

	/* peek for notification */
	memset(&snp, 0x00, sizeof(union sctp_notification));
	iov.iov_base = (char *)&snp;
	iov.iov_len = sizeof(union sctp_notification);
	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags = 0;

	n = recvmsg(b->num, &msg, MSG_PEEK);
	if (n <= 0)
		{
		if ((n < 0) && (get_last_socket_error() != EAGAIN) && (get_last_socket_error() != EWOULDBLOCK))
			return -1;
		else
			return 0;
		}

	/* if we find a notification, process it and try again if necessary */
	while (msg.msg_flags & MSG_NOTIFICATION)
		{
		memset(&snp, 0x00, sizeof(union sctp_notification));
		iov.iov_base = (char *)&snp;
		iov.iov_len = sizeof(union sctp_notification);
		msg.msg_name = NULL;
		msg.msg_namelen = 0;
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		msg.msg_control = NULL;
		msg.msg_controllen = 0;
		msg.msg_flags = 0;

		n = recvmsg(b->num, &msg, 0);
		if (n <= 0)
			{
			if ((n < 0) && (get_last_socket_error() != EAGAIN) && (get_last_socket_error() != EWOULDBLOCK))
				return -1;
			else
				return is_dry;
			}
		
		if (snp.sn_header.sn_type == SCTP_SENDER_DRY_EVENT)
			{
			is_dry = 1;

			/* disable sender dry event */
#ifdef SCTP_EVENT
			memset(&event, 0, sizeof(struct sctp_event));
			event.se_assoc_id = 0;
			event.se_type = SCTP_SENDER_DRY_EVENT;
			event.se_on = 0;
			ret = setsockopt(b->num, IPPROTO_SCTP, SCTP_EVENT, &event, sizeof(struct sctp_event));
#else
			eventsize = (socklen_t) sizeof(struct sctp_event_subscribe);
			ret = getsockopt(b->num, IPPROTO_SCTP, SCTP_EVENTS, &event, &eventsize);
			if (ret < 0)
				return -1;

			event.sctp_sender_dry_event = 0;

			ret = setsockopt(b->num, IPPROTO_SCTP, SCTP_EVENTS, &event, sizeof(struct sctp_event_subscribe));
#endif
			if (ret < 0)
				return -1;
			}

#ifdef SCTP_AUTHENTICATION_EVENT
		if (snp.sn_header.sn_type == SCTP_AUTHENTICATION_EVENT)
			dgram_sctp_handle_auth_free_key_event(b, &snp);
#endif

		if (data->handle_notifications != NULL)
			data->handle_notifications(b, data->notification_context, (void*) &snp);

		/* found notification, peek again */
		memset(&snp, 0x00, sizeof(union sctp_notification));
		iov.iov_base = (char *)&snp;
		iov.iov_len = sizeof(union sctp_notification);
		msg.msg_name = NULL;
		msg.msg_namelen = 0;
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		msg.msg_control = NULL;
		msg.msg_controllen = 0;
		msg.msg_flags = 0;

		/* if we have seen the dry already, don't wait */
		if (is_dry)
			{
			sockflags = fcntl(b->num, F_GETFL, 0);
			fcntl(b->num, F_SETFL, O_NONBLOCK);
			}

		n = recvmsg(b->num, &msg, MSG_PEEK);

		if (is_dry)
			{
			fcntl(b->num, F_SETFL, sockflags);
			}

		if (n <= 0)
			{
			if ((n < 0) && (get_last_socket_error() != EAGAIN) && (get_last_socket_error() != EWOULDBLOCK))
				return -1;
			else
				return is_dry;
			}
		}

	/* read anything else */
	return is_dry;
}

int BIO_dgram_sctp_msg_waiting(BIO *b)
	{
	int n, sockflags;
	union sctp_notification snp;
	struct msghdr msg;
	struct iovec iov;
	bio_dgram_sctp_data *data = (bio_dgram_sctp_data *)b->ptr;

	/* Check if there are any messages waiting to be read */
	do
		{
		memset(&snp, 0x00, sizeof(union sctp_notification));
		iov.iov_base = (char *)&snp;
		iov.iov_len = sizeof(union sctp_notification);
		msg.msg_name = NULL;
		msg.msg_namelen = 0;
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		msg.msg_control = NULL;
		msg.msg_controllen = 0;
		msg.msg_flags = 0;

		sockflags = fcntl(b->num, F_GETFL, 0);
		fcntl(b->num, F_SETFL, O_NONBLOCK);
		n = recvmsg(b->num, &msg, MSG_PEEK);
		fcntl(b->num, F_SETFL, sockflags);

		/* if notification, process and try again */
		if (n > 0 && (msg.msg_flags & MSG_NOTIFICATION))
			{
#ifdef SCTP_AUTHENTICATION_EVENT
			if (snp.sn_header.sn_type == SCTP_AUTHENTICATION_EVENT)
				dgram_sctp_handle_auth_free_key_event(b, &snp);
#endif

			memset(&snp, 0x00, sizeof(union sctp_notification));
			iov.iov_base = (char *)&snp;
			iov.iov_len = sizeof(union sctp_notification);
			msg.msg_name = NULL;
			msg.msg_namelen = 0;
			msg.msg_iov = &iov;
			msg.msg_iovlen = 1;
			msg.msg_control = NULL;
			msg.msg_controllen = 0;
			msg.msg_flags = 0;
			n = recvmsg(b->num, &msg, 0);

			if (data->handle_notifications != NULL)
				data->handle_notifications(b, data->notification_context, (void*) &snp);
			}

		} while (n > 0 && (msg.msg_flags & MSG_NOTIFICATION));

	/* Return 1 if there is a message to be read, return 0 otherwise. */
	if (n > 0)
		return 1;
	else
		return 0;
	}

static int dgram_sctp_puts(BIO *bp, const char *str)
	{
	int n,ret;

	n=strlen(str);
	ret=dgram_sctp_write(bp,str,n);
	return(ret);
	}
#endif

static int BIO_dgram_should_retry(int i)
	{
	int err;

	if ((i == 0) || (i == -1))
		{
		err=get_last_socket_error();

#if defined(OPENSSL_SYS_WINDOWS)
	/* If the socket return value (i) is -1
	 * and err is unexpectedly 0 at this point,
	 * the error code was overwritten by
	 * another system call before this error
	 * handling is called.
	 */
#endif

		return(BIO_dgram_non_fatal_error(err));
		}
	return(0);
	}

int BIO_dgram_non_fatal_error(int err)
	{
	switch (err)
		{
#if defined(OPENSSL_SYS_WINDOWS)
# if defined(WSAEWOULDBLOCK)
	case WSAEWOULDBLOCK:
# endif

# if 0 /* This appears to always be an error */
#  if defined(WSAENOTCONN)
	case WSAENOTCONN:
#  endif
# endif
#endif

#ifdef EWOULDBLOCK
# ifdef WSAEWOULDBLOCK
#  if WSAEWOULDBLOCK != EWOULDBLOCK
	case EWOULDBLOCK:
#  endif
# else
	case EWOULDBLOCK:
# endif
#endif

#ifdef EINTR
	case EINTR:
#endif

#ifdef EAGAIN
#if EWOULDBLOCK != EAGAIN
	case EAGAIN:
# endif
#endif

#ifdef EPROTO
	case EPROTO:
#endif

#ifdef EINPROGRESS
	case EINPROGRESS:
#endif

#ifdef EALREADY
	case EALREADY:
#endif

		return(1);
		/* break; */
	default:
		break;
		}
	return(0);
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

#endif
