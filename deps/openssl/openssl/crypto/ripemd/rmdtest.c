/* crypto/ripemd/rmdtest.c */
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
#include <string.h>
#include <stdlib.h>

#include "../e_os.h"

#ifdef OPENSSL_NO_RIPEMD
int main(int argc, char *argv[])
{
    printf("No ripemd support\n");
    return (0);
}
#else
# include <openssl/ripemd.h>
# include <openssl/evp.h>

# ifdef CHARSET_EBCDIC
#  include <openssl/ebcdic.h>
# endif

static char *test[] = {
    "",
    "a",
    "abc",
    "message digest",
    "abcdefghijklmnopqrstuvwxyz",
    "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
    "12345678901234567890123456789012345678901234567890123456789012345678901234567890",
    NULL,
};

static char *ret[] = {
    "9c1185a5c5e9fc54612808977ee8f548b2258d31",
    "0bdc9d2d256b3ee9daae347be6f4dc835a467ffe",
    "8eb208f7e05d987a9b044a8e98c6b087f15a0bfc",
    "5d0689ef49d2fae572b881b123a85ffa21595f36",
    "f71c27109c692c1b56bbdceb5b9d2865b3708dbc",
    "12a053384a9c0c88e405a06c27dcf49ada62eb2b",
    "b0e20b6e3116640286ed3a87a5713079b21f5189",
    "9b752e45573d4b39f4dbd3323cab82bf63326bfb",
};

static char *pt(unsigned char *md);
int main(int argc, char *argv[])
{
    int i, err = 0;
    char **P, **R;
    char *p;
    unsigned char md[RIPEMD160_DIGEST_LENGTH];

    P = test;
    R = ret;
    i = 1;
    while (*P != NULL) {
# ifdef CHARSET_EBCDIC
        ebcdic2ascii((char *)*P, (char *)*P, strlen((char *)*P));
# endif
        EVP_Digest(&(P[0][0]), strlen((char *)*P), md, NULL, EVP_ripemd160(),
                   NULL);
        p = pt(md);
        if (strcmp(p, (char *)*R) != 0) {
            printf("error calculating RIPEMD160 on '%s'\n", *P);
            printf("got %s instead of %s\n", p, *R);
            err++;
        } else
            printf("test %d ok\n", i);
        i++;
        R++;
        P++;
    }
    EXIT(err);
    return (0);
}

static char *pt(unsigned char *md)
{
    int i;
    static char buf[80];

    for (i = 0; i < RIPEMD160_DIGEST_LENGTH; i++)
        sprintf(&(buf[i * 2]), "%02x", md[i]);
    return (buf);
}
#endif
