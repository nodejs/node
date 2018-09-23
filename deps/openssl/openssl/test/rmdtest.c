/*
 * Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../e_os.h"

#ifdef OPENSSL_NO_RMD160
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

static char test[][100] = {
    { "" },
    { "a" },
    { "abc" },
    { "message digest" },
    { "abcdefghijklmnopqrstuvwxyz" },
    { "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq" },
    { "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789" },
    { "12345678901234567890123456789012345678901234567890123456789012345678901234567890" }
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
    unsigned int i;
    int err = 0;
    char **R;
    char *p;
    unsigned char md[RIPEMD160_DIGEST_LENGTH];

    R = ret;
    for (i = 0; i < OSSL_NELEM(test); i++) {
# ifdef CHARSET_EBCDIC
        ebcdic2ascii(test[i], test[i], strlen(test[i]));
# endif
        if (!EVP_Digest(test[i], strlen(test[i]), md, NULL, EVP_ripemd160(),
                        NULL)) {
            printf("EVP Digest error.\n");
            EXIT(1);
        }
        p = pt(md);
        if (strcmp(p, (char *)*R) != 0) {
            printf("error calculating RIPEMD160 on '%s'\n", test[i]);
            printf("got %s instead of %s\n", p, *R);
            err++;
        } else
            printf("test %d ok\n", i + 1);
        R++;
    }
    EXIT(err);
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
