/* crypto/rand/rand_egd.c */
/* Written by Ulf Moeller and Lutz Jaenicke for the OpenSSL project. */
/* ====================================================================
 * Copyright (c) 1998-2000 The OpenSSL Project.  All rights reserved.
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

#include <openssl/e_os2.h>
#include <openssl/rand.h>
#include <openssl/buffer.h>

/*
 * Query the EGD <URL: http://www.lothar.com/tech/crypto/>.
 *
 * This module supplies three routines:
 *
 * RAND_query_egd_bytes(path, buf, bytes)
 *   will actually query "bytes" bytes of entropy form the egd-socket located
 *   at path and will write them to buf (if supplied) or will directly feed
 *   it to RAND_seed() if buf==NULL.
 *   The number of bytes is not limited by the maximum chunk size of EGD,
 *   which is 255 bytes. If more than 255 bytes are wanted, several chunks
 *   of entropy bytes are requested. The connection is left open until the
 *   query is competed.
 *   RAND_query_egd_bytes() returns with
 *     -1  if an error occured during connection or communication.
 *     num the number of bytes read from the EGD socket. This number is either
 *         the number of bytes requested or smaller, if the EGD pool is
 *         drained and the daemon signals that the pool is empty.
 *   This routine does not touch any RAND_status(). This is necessary, since
 *   PRNG functions may call it during initialization.
 *
 * RAND_egd_bytes(path, bytes) will query "bytes" bytes and have them
 *   used to seed the PRNG.
 *   RAND_egd_bytes() is a wrapper for RAND_query_egd_bytes() with buf=NULL.
 *   Unlike RAND_query_egd_bytes(), RAND_status() is used to test the
 *   seed status so that the return value can reflect the seed state:
 *     -1  if an error occured during connection or communication _or_
 *         if the PRNG has still not received the required seeding.
 *     num the number of bytes read from the EGD socket. This number is either
 *         the number of bytes requested or smaller, if the EGD pool is
 *         drained and the daemon signals that the pool is empty.
 *
 * RAND_egd(path) will query 255 bytes and use the bytes retreived to seed
 *   the PRNG.
 *   RAND_egd() is a wrapper for RAND_egd_bytes() with numbytes=255.
 */

#if defined(OPENSSL_SYS_WIN32) || defined(OPENSSL_SYS_VMS) || defined(OPENSSL_SYS_MSDOS) || defined(OPENSSL_SYS_VXWORKS) || defined(OPENSSL_SYS_NETWARE) || defined(OPENSSL_SYS_VOS)
int RAND_query_egd_bytes(const char *path, unsigned char *buf, int bytes)
	{
	return(-1);
	}
int RAND_egd(const char *path)
	{
	return(-1);
	}

int RAND_egd_bytes(const char *path,int bytes)
	{
	return(-1);
	}
#else
#include <openssl/opensslconf.h>
#include OPENSSL_UNISTD
#include <sys/types.h>
#include <sys/socket.h>
#ifndef NO_SYS_UN_H
# ifdef OPENSSL_SYS_VXWORKS
#   include <streams/un.h>
# else
#   include <sys/un.h>
# endif
#else
struct	sockaddr_un {
	short	sun_family;		/* AF_UNIX */
	char	sun_path[108];		/* path name (gag) */
};
#endif /* NO_SYS_UN_H */
#include <string.h>
#include <errno.h>

#ifndef offsetof
#  define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

int RAND_query_egd_bytes(const char *path, unsigned char *buf, int bytes)
	{
	int ret = 0;
	struct sockaddr_un addr;
	int len, num, numbytes;
	int fd = -1;
	int success;
	unsigned char egdbuf[2], tempbuf[255], *retrievebuf;

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	if (strlen(path) >= sizeof(addr.sun_path))
		return (-1);
	BUF_strlcpy(addr.sun_path,path,sizeof addr.sun_path);
	len = offsetof(struct sockaddr_un, sun_path) + strlen(path);
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd == -1) return (-1);
	success = 0;
	while (!success)
	    {
	    if (connect(fd, (struct sockaddr *)&addr, len) == 0)
	       success = 1;
	    else
		{
		switch (errno)
		    {
#ifdef EINTR
		    case EINTR:
#endif
#ifdef EAGAIN
		    case EAGAIN:
#endif
#ifdef EINPROGRESS
		    case EINPROGRESS:
#endif
#ifdef EALREADY
		    case EALREADY:
#endif
			/* No error, try again */
			break;
#ifdef EISCONN
		    case EISCONN:
			success = 1;
			break;
#endif
		    default:
			goto err;	/* failure */
		    }
		}
	    }

	while(bytes > 0)
	    {
	    egdbuf[0] = 1;
	    egdbuf[1] = bytes < 255 ? bytes : 255;
	    numbytes = 0;
	    while (numbytes != 2)
		{
	        num = write(fd, egdbuf + numbytes, 2 - numbytes);
	        if (num >= 0)
		    numbytes += num;
	    	else
		    {
		    switch (errno)
		    	{
#ifdef EINTR
		    	case EINTR:
#endif
#ifdef EAGAIN
		    	case EAGAIN:
#endif
			    /* No error, try again */
			    break;
		    	default:
			    ret = -1;
			    goto err;	/* failure */
			}
		    }
		}
	    numbytes = 0;
	    while (numbytes != 1)
		{
	        num = read(fd, egdbuf, 1);
	        if (num == 0)
			goto err;	/* descriptor closed */
		else if (num > 0)
		    numbytes += num;
	    	else
		    {
		    switch (errno)
		    	{
#ifdef EINTR
		    	case EINTR:
#endif
#ifdef EAGAIN
		    	case EAGAIN:
#endif
			    /* No error, try again */
			    break;
		    	default:
			    ret = -1;
			    goto err;	/* failure */
			}
		    }
		}
	    if(egdbuf[0] == 0)
		goto err;
	    if (buf)
		retrievebuf = buf + ret;
	    else
		retrievebuf = tempbuf;
	    numbytes = 0;
	    while (numbytes != egdbuf[0])
		{
	        num = read(fd, retrievebuf + numbytes, egdbuf[0] - numbytes);
		if (num == 0)
			goto err;	/* descriptor closed */
	        else if (num > 0)
		    numbytes += num;
	    	else
		    {
		    switch (errno)
		    	{
#ifdef EINTR
		    	case EINTR:
#endif
#ifdef EAGAIN
		    	case EAGAIN:
#endif
			    /* No error, try again */
			    break;
		    	default:
			    ret = -1;
			    goto err;	/* failure */
			}
		    }
		}
	    ret += egdbuf[0];
	    bytes -= egdbuf[0];
	    if (!buf)
		RAND_seed(tempbuf, egdbuf[0]);
	    }
 err:
	if (fd != -1) close(fd);
	return(ret);
	}


int RAND_egd_bytes(const char *path, int bytes)
	{
	int num, ret = 0;

	num = RAND_query_egd_bytes(path, NULL, bytes);
	if (num < 1) goto err;
	if (RAND_status() == 1)
	    ret = num;
 err:
	return(ret);
	}


int RAND_egd(const char *path)
	{
	return (RAND_egd_bytes(path, 255));
	}


#endif
