/* crypto/hmac/hmactest.c */
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

#ifdef OPENSSL_NO_HMAC
int main(int argc, char *argv[])
{
    printf("No HMAC support\n");
    return (0);
}
#else
# include <openssl/hmac.h>
# ifndef OPENSSL_NO_MD5
#  include <openssl/md5.h>
# endif

# ifdef CHARSET_EBCDIC
#  include <openssl/ebcdic.h>
# endif

# ifndef OPENSSL_NO_MD5
static struct test_st {
    unsigned char key[16];
    int key_len;
    unsigned char data[64];
    int data_len;
    unsigned char *digest;
} test[4] = {
    {
        "", 0, "More text test vectors to stuff up EBCDIC machines :-)", 54,
        (unsigned char *)"e9139d1e6ee064ef8cf514fc7dc83e86",
    },
    {
        {
            0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
            0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
        }, 16, "Hi There", 8,
        (unsigned char *)"9294727a3638bb1c13f48ef8158bfc9d",
    },
    {
        "Jefe", 4, "what do ya want for nothing?", 28,
        (unsigned char *)"750c783e6ab0b503eaa86e310a5db738",
    },
    {
        {
            0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
            0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
        }, 16, {
            0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
            0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
            0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
            0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
            0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd
        }, 50, (unsigned char *)"56be34521d144c88dbb8c733f0e8b3f6",
    },
};
# endif

static char *pt(unsigned char *md);
int main(int argc, char *argv[])
{
# ifndef OPENSSL_NO_MD5
    int i;
    char *p;
# endif
    int err = 0;

# ifdef OPENSSL_NO_MD5
    printf("test skipped: MD5 disabled\n");
# else

#  ifdef CHARSET_EBCDIC
    ebcdic2ascii(test[0].data, test[0].data, test[0].data_len);
    ebcdic2ascii(test[1].data, test[1].data, test[1].data_len);
    ebcdic2ascii(test[2].key, test[2].key, test[2].key_len);
    ebcdic2ascii(test[2].data, test[2].data, test[2].data_len);
#  endif

    for (i = 0; i < 4; i++) {
        p = pt(HMAC(EVP_md5(),
                    test[i].key, test[i].key_len,
                    test[i].data, test[i].data_len, NULL, NULL));

        if (strcmp(p, (char *)test[i].digest) != 0) {
            printf("error calculating HMAC on %d entry'\n", i);
            printf("got %s instead of %s\n", p, test[i].digest);
            err++;
        } else
            printf("test %d ok\n", i);
    }
# endif                         /* OPENSSL_NO_MD5 */
    EXIT(err);
    return (0);
}

# ifndef OPENSSL_NO_MD5
static char *pt(unsigned char *md)
{
    int i;
    static char buf[80];

    for (i = 0; i < MD5_DIGEST_LENGTH; i++)
        sprintf(&(buf[i * 2]), "%02x", md[i]);
    return (buf);
}
# endif
#endif
