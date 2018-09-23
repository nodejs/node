/*
 * Copyright 2000-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/opensslconf.h>
#ifdef OPENSSL_NO_EGD
NON_EMPTY_TRANSLATION_UNIT
#else

# include <openssl/crypto.h>
# include <openssl/e_os2.h>
# include <openssl/rand.h>

/*-
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
 *     -1  if an error occurred during connection or communication.
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
 *     -1  if an error occurred during connection or communication _or_
 *         if the PRNG has still not received the required seeding.
 *     num the number of bytes read from the EGD socket. This number is either
 *         the number of bytes requested or smaller, if the EGD pool is
 *         drained and the daemon signals that the pool is empty.
 *
 * RAND_egd(path) will query 255 bytes and use the bytes retrieved to seed
 *   the PRNG.
 *   RAND_egd() is a wrapper for RAND_egd_bytes() with numbytes=255.
 */

# if defined(OPENSSL_SYS_WIN32) || defined(OPENSSL_SYS_VMS) || defined(OPENSSL_SYS_MSDOS) || defined(OPENSSL_SYS_VXWORKS) || defined(OPENSSL_SYS_VOS) || defined(OPENSSL_SYS_UEFI)
int RAND_query_egd_bytes(const char *path, unsigned char *buf, int bytes)
{
    return (-1);
}

int RAND_egd(const char *path)
{
    return (-1);
}

int RAND_egd_bytes(const char *path, int bytes)
{
    return (-1);
}
# else
#  include <openssl/opensslconf.h>
#  include OPENSSL_UNISTD
#  include <stddef.h>
#  include <sys/types.h>
#  include <sys/socket.h>
#  ifndef NO_SYS_UN_H
#   ifdef OPENSSL_SYS_VXWORKS
#    include <streams/un.h>
#   else
#    include <sys/un.h>
#   endif
#  else
struct sockaddr_un {
    short sun_family;           /* AF_UNIX */
    char sun_path[108];         /* path name (gag) */
};
#  endif                         /* NO_SYS_UN_H */
#  include <string.h>
#  include <errno.h>

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
    OPENSSL_strlcpy(addr.sun_path, path, sizeof(addr.sun_path));
    len = offsetof(struct sockaddr_un, sun_path) + strlen(path);
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1)
        return (-1);
    success = 0;
    while (!success) {
        if (connect(fd, (struct sockaddr *)&addr, len) == 0)
            success = 1;
        else {
            switch (errno) {
#  ifdef EINTR
            case EINTR:
#  endif
#  ifdef EAGAIN
            case EAGAIN:
#  endif
#  ifdef EINPROGRESS
            case EINPROGRESS:
#  endif
#  ifdef EALREADY
            case EALREADY:
#  endif
                /* No error, try again */
                break;
#  ifdef EISCONN
            case EISCONN:
                success = 1;
                break;
#  endif
            default:
                ret = -1;
                goto err;       /* failure */
            }
        }
    }

    while (bytes > 0) {
        egdbuf[0] = 1;
        egdbuf[1] = bytes < 255 ? bytes : 255;
        numbytes = 0;
        while (numbytes != 2) {
            num = write(fd, egdbuf + numbytes, 2 - numbytes);
            if (num >= 0)
                numbytes += num;
            else {
                switch (errno) {
#  ifdef EINTR
                case EINTR:
#  endif
#  ifdef EAGAIN
                case EAGAIN:
#  endif
                    /* No error, try again */
                    break;
                default:
                    ret = -1;
                    goto err;   /* failure */
                }
            }
        }
        numbytes = 0;
        while (numbytes != 1) {
            num = read(fd, egdbuf, 1);
            if (num == 0)
                goto err;       /* descriptor closed */
            else if (num > 0)
                numbytes += num;
            else {
                switch (errno) {
#  ifdef EINTR
                case EINTR:
#  endif
#  ifdef EAGAIN
                case EAGAIN:
#  endif
                    /* No error, try again */
                    break;
                default:
                    ret = -1;
                    goto err;   /* failure */
                }
            }
        }
        if (egdbuf[0] == 0)
            goto err;
        if (buf)
            retrievebuf = buf + ret;
        else
            retrievebuf = tempbuf;
        numbytes = 0;
        while (numbytes != egdbuf[0]) {
            num = read(fd, retrievebuf + numbytes, egdbuf[0] - numbytes);
            if (num == 0)
                goto err;       /* descriptor closed */
            else if (num > 0)
                numbytes += num;
            else {
                switch (errno) {
#  ifdef EINTR
                case EINTR:
#  endif
#  ifdef EAGAIN
                case EAGAIN:
#  endif
                    /* No error, try again */
                    break;
                default:
                    ret = -1;
                    goto err;   /* failure */
                }
            }
        }
        ret += egdbuf[0];
        bytes -= egdbuf[0];
        if (!buf)
            RAND_seed(tempbuf, egdbuf[0]);
    }
 err:
    if (fd != -1)
        close(fd);
    return (ret);
}

int RAND_egd_bytes(const char *path, int bytes)
{
    int num, ret = -1;

    num = RAND_query_egd_bytes(path, NULL, bytes);
    if (num < 0)
        goto err;
    if (RAND_status() == 1)
        ret = num;
 err:
    return (ret);
}

int RAND_egd(const char *path)
{
    return (RAND_egd_bytes(path, 255));
}

# endif

#endif
