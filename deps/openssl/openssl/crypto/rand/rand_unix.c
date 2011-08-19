/* crypto/rand/rand_unix.c */
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
 * Copyright (c) 1998-2006 The OpenSSL Project.  All rights reserved.
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

#define USE_SOCKETS
#include "e_os.h"
#include "cryptlib.h"
#include <openssl/rand.h>
#include "rand_lcl.h"

#if !(defined(OPENSSL_SYS_WINDOWS) || defined(OPENSSL_SYS_WIN32) || defined(OPENSSL_SYS_VMS) || defined(OPENSSL_SYS_OS2) || defined(OPENSSL_SYS_VXWORKS) || defined(OPENSSL_SYS_NETWARE))

#include <sys/types.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#if defined(OPENSSL_SYS_LINUX) /* should actually be available virtually everywhere */
# include <poll.h>
#endif
#include <limits.h>
#ifndef FD_SETSIZE
# define FD_SETSIZE (8*sizeof(fd_set))
#endif

#ifdef __OpenBSD__
int RAND_poll(void)
{
	u_int32_t rnd = 0, i;
	unsigned char buf[ENTROPY_NEEDED];

	for (i = 0; i < sizeof(buf); i++) {
		if (i % 4 == 0)
			rnd = arc4random();
		buf[i] = rnd;
		rnd >>= 8;
	}
	RAND_add(buf, sizeof(buf), ENTROPY_NEEDED);
	memset(buf, 0, sizeof(buf));

	return 1;
}
#else /* !defined(__OpenBSD__) */
int RAND_poll(void)
{
	unsigned long l;
	pid_t curr_pid = getpid();
#if defined(DEVRANDOM) || defined(DEVRANDOM_EGD)
	unsigned char tmpbuf[ENTROPY_NEEDED];
	int n = 0;
#endif
#ifdef DEVRANDOM
	static const char *randomfiles[] = { DEVRANDOM };
	struct stat randomstats[sizeof(randomfiles)/sizeof(randomfiles[0])];
	int fd;
	size_t i;
#endif
#ifdef DEVRANDOM_EGD
	static const char *egdsockets[] = { DEVRANDOM_EGD, NULL };
	const char **egdsocket = NULL;
#endif

#ifdef DEVRANDOM
	memset(randomstats,0,sizeof(randomstats));
	/* Use a random entropy pool device. Linux, FreeBSD and OpenBSD
	 * have this. Use /dev/urandom if you can as /dev/random may block
	 * if it runs out of random entries.  */

	for (i=0; i<sizeof(randomfiles)/sizeof(randomfiles[0]) && n < ENTROPY_NEEDED; i++)
		{
		if ((fd = open(randomfiles[i], O_RDONLY
#ifdef O_NONBLOCK
			|O_NONBLOCK
#endif
#ifdef O_BINARY
			|O_BINARY
#endif
#ifdef O_NOCTTY /* If it happens to be a TTY (god forbid), do not make it
		   our controlling tty */
			|O_NOCTTY
#endif
			)) >= 0)
			{
			int usec = 10*1000; /* spend 10ms on each file */
			int r;
			size_t j;
			struct stat *st=&randomstats[i];

			/* Avoid using same input... Used to be O_NOFOLLOW
			 * above, but it's not universally appropriate... */
			if (fstat(fd,st) != 0)	{ close(fd); continue; }
			for (j=0;j<i;j++)
				{
				if (randomstats[j].st_ino==st->st_ino &&
				    randomstats[j].st_dev==st->st_dev)
					break;
				}
			if (j<i)		{ close(fd); continue; }

			do
				{
				int try_read = 0;

#if defined(OPENSSL_SYS_LINUX)
				/* use poll() */
				struct pollfd pset;
				
				pset.fd = fd;
				pset.events = POLLIN;
				pset.revents = 0;

				if (poll(&pset, 1, usec / 1000) < 0)
					usec = 0;
				else
					try_read = (pset.revents & POLLIN) != 0;

#else
				/* use select() */
				fd_set fset;
				struct timeval t;
				
				t.tv_sec = 0;
				t.tv_usec = usec;

				if (FD_SETSIZE > 0 && (unsigned)fd >= FD_SETSIZE)
					{
					/* can't use select, so just try to read once anyway */
					try_read = 1;
					}
				else
					{
					FD_ZERO(&fset);
					FD_SET(fd, &fset);
					
					if (select(fd+1,&fset,NULL,NULL,&t) >= 0)
						{
						usec = t.tv_usec;
						if (FD_ISSET(fd, &fset))
							try_read = 1;
						}
					else
						usec = 0;
					}
#endif
				
				if (try_read)
					{
					r = read(fd,(unsigned char *)tmpbuf+n, ENTROPY_NEEDED-n);
					if (r > 0)
						n += r;
					}
				else
					r = -1;
				
				/* Some Unixen will update t in select(), some
				   won't.  For those who won't, or if we
				   didn't use select() in the first place,
				   give up here, otherwise, we will do
				   this once again for the remaining
				   time. */
				if (usec == 10*1000)
					usec = 0;
				}
			while ((r > 0 ||
			       (errno == EINTR || errno == EAGAIN)) && usec != 0 && n < ENTROPY_NEEDED);

			close(fd);
			}
		}
#endif /* defined(DEVRANDOM) */

#ifdef DEVRANDOM_EGD
	/* Use an EGD socket to read entropy from an EGD or PRNGD entropy
	 * collecting daemon. */

	for (egdsocket = egdsockets; *egdsocket && n < ENTROPY_NEEDED; egdsocket++)
		{
		int r;

		r = RAND_query_egd_bytes(*egdsocket, (unsigned char *)tmpbuf+n,
					 ENTROPY_NEEDED-n);
		if (r > 0)
			n += r;
		}
#endif /* defined(DEVRANDOM_EGD) */

#if defined(DEVRANDOM) || defined(DEVRANDOM_EGD)
	if (n > 0)
		{
		RAND_add(tmpbuf,sizeof tmpbuf,(double)n);
		OPENSSL_cleanse(tmpbuf,n);
		}
#endif

	/* put in some default random data, we need more than just this */
	l=curr_pid;
	RAND_add(&l,sizeof(l),0.0);
	l=getuid();
	RAND_add(&l,sizeof(l),0.0);

	l=time(NULL);
	RAND_add(&l,sizeof(l),0.0);

#if defined(DEVRANDOM) || defined(DEVRANDOM_EGD)
	return 1;
#else
	return 0;
#endif
}

#endif /* defined(__OpenBSD__) */
#endif /* !(defined(OPENSSL_SYS_WINDOWS) || defined(OPENSSL_SYS_WIN32) || defined(OPENSSL_SYS_VMS) || defined(OPENSSL_SYS_OS2) || defined(OPENSSL_SYS_VXWORKS) || defined(OPENSSL_SYS_NETWARE)) */


#if defined(OPENSSL_SYS_VXWORKS)
int RAND_poll(void)
	{
	return 0;
	}
#endif
