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
        if (!EVP_Digest(&(P[0][0]), strlen((char *)*P), md, NULL, EVP_md4(),
            NULL)) {
            printf("EVP Digest error.\n");
            EXIT(1);
        }
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
