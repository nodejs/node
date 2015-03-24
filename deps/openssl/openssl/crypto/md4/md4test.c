/* crypto/md4/md4test.c */
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

#ifdef OPENSSL_NO_MD4
int main(int argc, char *argv[])
{
    printf("No MD4 support\n");
    return (0);
}
#else
# include <openssl/evp.h>
# include <openssl/md4.h>

static char *test[] = {
    "",
    "a",
    "abc",
    "message digest",
    "abcdefghijklmnopqrstuvwxyz",
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
    "12345678901234567890123456789012345678901234567890123456789012345678901234567890",
    NULL,
};

static char *ret[] = {
    "31d6cfe0d16ae931b73c59d7e0c089c0",
    "bde52cb31de33e46245e05fbdbd6fb24",
    "a448017aaf21d8525fc10ae87aa6729d",
    "d9130a8164549fe818874806e1c7014b",
    "d79e1c308aa5bbcdeea8ed63df412da9",
    "043f8582f241db351ce627e153e7f0e4",
    "e33b4ddc9c38f2199c3e7b164fcc0536",
};

static char *pt(unsigned char *md);
int main(int argc, char *argv[])
{
    int i, err = 0;
    char **P, **R;
    char *p;
    unsigned char md[MD4_DIGEST_LENGTH];

    P = test;
    R = ret;
    i = 1;
    while (*P != NULL) {
        EVP_Digest(&(P[0][0]), strlen((char *)*P), md, NULL, EVP_md4(), NULL);
        p = pt(md);
        if (strcmp(p, (char *)*R) != 0) {
            printf("error calculating MD4 on '%s'\n", *P);
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

    for (i = 0; i < MD4_DIGEST_LENGTH; i++)
        sprintf(&(buf[i * 2]), "%02x", md[i]);
    return (buf);
}
#endif
