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
#include <openssl/opensslconf.h> /* To see if OPENSSL_NO_CAST is defined */

#include "../e_os.h"

#ifdef OPENSSL_NO_CAST
int main(int argc, char *argv[])
{
    printf("No CAST support\n");
    return (0);
}
#else
# include <openssl/cast.h>

# define FULL_TEST

static unsigned char k[16] = {
    0x01, 0x23, 0x45, 0x67, 0x12, 0x34, 0x56, 0x78,
    0x23, 0x45, 0x67, 0x89, 0x34, 0x56, 0x78, 0x9A
};

static unsigned char in[8] =
    { 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF };

static int k_len[3] = { 16, 10, 5 };

static unsigned char c[3][8] = {
    {0x23, 0x8B, 0x4F, 0xE5, 0x84, 0x7E, 0x44, 0xB2},
    {0xEB, 0x6A, 0x71, 0x1A, 0x2C, 0x02, 0x27, 0x1B},
    {0x7A, 0xC8, 0x16, 0xD1, 0x6E, 0x9B, 0x30, 0x2E},
};

static unsigned char out[80];

static unsigned char in_a[16] = {
    0x01, 0x23, 0x45, 0x67, 0x12, 0x34, 0x56, 0x78,
    0x23, 0x45, 0x67, 0x89, 0x34, 0x56, 0x78, 0x9A
};

static unsigned char in_b[16] = {
    0x01, 0x23, 0x45, 0x67, 0x12, 0x34, 0x56, 0x78,
    0x23, 0x45, 0x67, 0x89, 0x34, 0x56, 0x78, 0x9A
};

static unsigned char c_a[16] = {
    0xEE, 0xA9, 0xD0, 0xA2, 0x49, 0xFD, 0x3B, 0xA6,
    0xB3, 0x43, 0x6F, 0xB8, 0x9D, 0x6D, 0xCA, 0x92
};

static unsigned char c_b[16] = {
    0xB2, 0xC9, 0x5E, 0xB0, 0x0C, 0x31, 0xAD, 0x71,
    0x80, 0xAC, 0x05, 0xB8, 0xE8, 0x3D, 0x69, 0x6E
};

int main(int argc, char *argv[])
{
# ifdef FULL_TEST
    long l;
    CAST_KEY key_b;
# endif
    int i, z, err = 0;
    CAST_KEY key;

    for (z = 0; z < 3; z++) {
        CAST_set_key(&key, k_len[z], k);

        CAST_ecb_encrypt(in, out, &key, CAST_ENCRYPT);
        if (memcmp(out, &(c[z][0]), 8) != 0) {
            printf("ecb cast error encrypting for keysize %d\n",
                   k_len[z] * 8);
            printf("got     :");
            for (i = 0; i < 8; i++)
                printf("%02X ", out[i]);
            printf("\n");
            printf("expected:");
            for (i = 0; i < 8; i++)
                printf("%02X ", c[z][i]);
            err = 20;
            printf("\n");
        }

        CAST_ecb_encrypt(out, out, &key, CAST_DECRYPT);
        if (memcmp(out, in, 8) != 0) {
            printf("ecb cast error decrypting for keysize %d\n",
                   k_len[z] * 8);
            printf("got     :");
            for (i = 0; i < 8; i++)
                printf("%02X ", out[i]);
            printf("\n");
            printf("expected:");
            for (i = 0; i < 8; i++)
                printf("%02X ", in[i]);
            printf("\n");
            err = 3;
        }
    }
    if (err == 0)
        printf("ecb cast5 ok\n");

# ifdef FULL_TEST
    {
        unsigned char out_a[16], out_b[16];
        static char *hex = "0123456789ABCDEF";

        printf("This test will take some time....");
        fflush(stdout);
        memcpy(out_a, in_a, sizeof(in_a));
        memcpy(out_b, in_b, sizeof(in_b));
        i = 1;

        for (l = 0; l < 1000000L; l++) {
            CAST_set_key(&key_b, 16, out_b);
            CAST_ecb_encrypt(&(out_a[0]), &(out_a[0]), &key_b, CAST_ENCRYPT);
            CAST_ecb_encrypt(&(out_a[8]), &(out_a[8]), &key_b, CAST_ENCRYPT);
            CAST_set_key(&key, 16, out_a);
            CAST_ecb_encrypt(&(out_b[0]), &(out_b[0]), &key, CAST_ENCRYPT);
            CAST_ecb_encrypt(&(out_b[8]), &(out_b[8]), &key, CAST_ENCRYPT);
            if ((l & 0xffff) == 0xffff) {
                printf("%c", hex[i & 0x0f]);
                fflush(stdout);
                i++;
            }
        }

        if ((memcmp(out_a, c_a, sizeof(c_a)) != 0) ||
            (memcmp(out_b, c_b, sizeof(c_b)) != 0)) {
            printf("\n");
            printf("Error\n");

            printf("A out =");
            for (i = 0; i < 16; i++)
                printf("%02X ", out_a[i]);
            printf("\nactual=");
            for (i = 0; i < 16; i++)
                printf("%02X ", c_a[i]);
            printf("\n");

            printf("B out =");
            for (i = 0; i < 16; i++)
                printf("%02X ", out_b[i]);
            printf("\nactual=");
            for (i = 0; i < 16; i++)
                printf("%02X ", c_b[i]);
            printf("\n");
        } else
            printf(" ok\n");
    }
# endif

    EXIT(err);
}
#endif
