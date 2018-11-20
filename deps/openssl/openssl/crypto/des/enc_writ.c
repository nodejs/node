/* crypto/des/enc_writ.c */
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

#include <errno.h>
#include <time.h>
#include <stdio.h>
#include "cryptlib.h"
#include "des_locl.h"
#include <openssl/rand.h>

/*-
 * WARNINGS:
 *
 *  -  The data format used by DES_enc_write() and DES_enc_read()
 *     has a cryptographic weakness: When asked to write more
 *     than MAXWRITE bytes, DES_enc_write will split the data
 *     into several chunks that are all encrypted
 *     using the same IV.  So don't use these functions unless you
 *     are sure you know what you do (in which case you might
 *     not want to use them anyway).
 *
 *  -  This code cannot handle non-blocking sockets.
 */

int DES_enc_write(int fd, const void *_buf, int len,
                  DES_key_schedule *sched, DES_cblock *iv)
{
#if defined(OPENSSL_NO_POSIX_IO)
    return (-1);
#else
# ifdef _LIBC
    extern unsigned long time();
    extern int write();
# endif
    const unsigned char *buf = _buf;
    long rnum;
    int i, j, k, outnum;
    static unsigned char *outbuf = NULL;
    unsigned char shortbuf[8];
    unsigned char *p;
    const unsigned char *cp;
    static int start = 1;

    if (outbuf == NULL) {
        outbuf = OPENSSL_malloc(BSIZE + HDRSIZE);
        if (outbuf == NULL)
            return (-1);
    }
    /*
     * If we are sending less than 8 bytes, the same char will look the same
     * if we don't pad it out with random bytes
     */
    if (start) {
        start = 0;
    }

    /* lets recurse if we want to send the data in small chunks */
    if (len > MAXWRITE) {
        j = 0;
        for (i = 0; i < len; i += k) {
            k = DES_enc_write(fd, &(buf[i]),
                              ((len - i) > MAXWRITE) ? MAXWRITE : (len - i),
                              sched, iv);
            if (k < 0)
                return (k);
            else
                j += k;
        }
        return (j);
    }

    /* write length first */
    p = outbuf;
    l2n(len, p);

    /* pad short strings */
    if (len < 8) {
        cp = shortbuf;
        memcpy(shortbuf, buf, len);
        RAND_pseudo_bytes(shortbuf + len, 8 - len);
        rnum = 8;
    } else {
        cp = buf;
        rnum = ((len + 7) / 8 * 8); /* round up to nearest eight */
    }

    if (DES_rw_mode & DES_PCBC_MODE)
        DES_pcbc_encrypt(cp, &(outbuf[HDRSIZE]), (len < 8) ? 8 : len, sched,
                         iv, DES_ENCRYPT);
    else
        DES_cbc_encrypt(cp, &(outbuf[HDRSIZE]), (len < 8) ? 8 : len, sched,
                        iv, DES_ENCRYPT);

    /* output */
    outnum = rnum + HDRSIZE;

    for (j = 0; j < outnum; j += i) {
        /*
         * eay 26/08/92 I was not doing writing from where we got up to.
         */
# ifndef _WIN32
        i = write(fd, (void *)&(outbuf[j]), outnum - j);
# else
        i = _write(fd, (void *)&(outbuf[j]), outnum - j);
# endif
        if (i == -1) {
# ifdef EINTR
            if (errno == EINTR)
                i = 0;
            else
# endif
                /*
                 * This is really a bad error - very bad It will stuff-up
                 * both ends.
                 */
                return (-1);
        }
    }

    return (len);
#endif                          /* OPENSSL_NO_POSIX_IO */
}
