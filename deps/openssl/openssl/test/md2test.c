/*
 * Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../e_os.h"

#ifdef OPENSSL_NO_MD2
int main(int argc, char *argv[])
{
    printf("No MD2 support\n");
    return (0);
}
#else
# include <openssl/evp.h>
# include <openssl/md2.h>

# ifdef CHARSET_EBCDIC
#  include <openssl/ebcdic.h>
# endif

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
    "8350e5a3e24c153df2275c9f80692773",
    "32ec01ec4a6dac72c0ab96fb34c0b5d1",
    "da853b0d3f88d99b30283a69e6ded6bb",
    "ab4f496bfb2a530b219ff33031fe06b0",
    "4e8ddff3650292ab5a4108c3aa47940b",
    "da33def2a42df13975352846c30338cd",
    "d5976f79d83d3a0dc9806c3c66f3efd8",
};

static char *pt(unsigned char *md);
int main(int argc, char *argv[])
{
    int i, err = 0;
    char **P, **R;
    char *p;
    unsigned char md[MD2_DIGEST_LENGTH];

    P = test;
    R = ret;
    i = 1;
    while (*P != NULL) {
        if (!EVP_Digest((unsigned char *)*P, strlen(*P), md, NULL, EVP_md2(),
                        NULL)) {
            printf("EVP Digest error.\n");
            EXIT(1);
        }
        p = pt(md);
        if (strcmp(p, *R) != 0) {
            printf("error calculating MD2 on '%s'\n", *P);
            printf("got %s instead of %s\n", p, *R);
            err++;
        } else
            printf("test %d ok\n", i);
        i++;
        R++;
        P++;
    }
    EXIT(err);
    return err;
}

static char *pt(unsigned char *md)
{
    int i;
    static char buf[80];

    for (i = 0; i < MD2_DIGEST_LENGTH; i++)
        sprintf(&(buf[i * 2]), "%02x", md[i]);
    return (buf);
}
#endif
