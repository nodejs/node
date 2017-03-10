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
#include <openssl/evp.h>
#include <openssl/sha.h>

#ifdef CHARSET_EBCDIC
# include <openssl/ebcdic.h>
#endif

static char test[][80] = {
    { "abc" },
    { "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq" }
};

static char *ret[] = {
    "a9993e364706816aba3e25717850c26c9cd0d89d",
    "84983e441c3bd26ebaae4aa1f95129e5e54670f1",
};

static char *bigret = "34aa973cd4c4daa4f61eeb2bdbad27316534016f";

static char *pt(unsigned char *md);
int main(int argc, char *argv[])
{
    unsigned int i;
    int err = 0;
    char **R;
    static unsigned char buf[1000];
    char *p, *r;
    EVP_MD_CTX *c;
    unsigned char md[SHA_DIGEST_LENGTH];

    c = EVP_MD_CTX_new();
    R = ret;
    for (i = 0; i < OSSL_NELEM(test); i++) {
# ifdef CHARSET_EBCDIC
        ebcdic2ascii(test[i], test[i], strlen(test[i]));
# endif
        if (!EVP_Digest(test[i], strlen(test[i]), md, NULL, EVP_sha1(),
            NULL)) {
            printf("EVP_Digest() error\n");
            err++;
            goto err;
        }
        p = pt(md);
        if (strcmp(p, (char *)*R) != 0) {
            printf("error calculating SHA1 on '%s'\n", test[i]);
            printf("got %s instead of %s\n", p, *R);
            err++;
        } else
            printf("test %d ok\n", i + 1);
        R++;
    }

    memset(buf, 'a', 1000);
#ifdef CHARSET_EBCDIC
    ebcdic2ascii(buf, buf, 1000);
#endif                         /* CHARSET_EBCDIC */
    if (!EVP_DigestInit_ex(c, EVP_sha1(), NULL)) {
        printf("EVP_DigestInit_ex() error\n");
        err++;
        goto err;
    }
    for (i = 0; i < 1000; i++) {
        if (!EVP_DigestUpdate(c, buf, 1000)) {
            printf("EVP_DigestUpdate() error\n");
            err++;
            goto err;
        }
    }
    if (!EVP_DigestFinal_ex(c, md, NULL)) {
            printf("EVP_DigestFinal() error\n");
            err++;
            goto err;
    }
    p = pt(md);

    r = bigret;
    if (strcmp(p, r) != 0) {
        printf("error calculating SHA1 on 'a' * 1000\n");
        printf("got %s instead of %s\n", p, r);
        err++;
    } else
        printf("test 3 ok\n");
 err:
    EVP_MD_CTX_free(c);
    EXIT(err);
    return (0);
}

static char *pt(unsigned char *md)
{
    int i;
    static char buf[80];

    for (i = 0; i < SHA_DIGEST_LENGTH; i++)
        sprintf(&(buf[i * 2]), "%02x", md[i]);
    return (buf);
}
