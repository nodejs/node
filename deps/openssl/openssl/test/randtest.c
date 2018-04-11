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
#include <openssl/rand.h>

#include "../e_os.h"

/* some FIPS 140-1 random number test */
/* some simple tests */

int main(int argc, char **argv)
{
    unsigned char buf[2500];
    int i, j, k, s, sign, nsign, err = 0;
    unsigned long n1;
    unsigned long n2[16];
    unsigned long runs[2][34];
    /*
     * double d;
     */
    long d;

    i = RAND_bytes(buf, 2500);
    if (i <= 0) {
        printf("init failed, the rand method is not properly installed\n");
        err++;
        goto err;
    }

    n1 = 0;
    for (i = 0; i < 16; i++)
        n2[i] = 0;
    for (i = 0; i < 34; i++)
        runs[0][i] = runs[1][i] = 0;

    /* test 1 and 2 */
    sign = 0;
    nsign = 0;
    for (i = 0; i < 2500; i++) {
        j = buf[i];

        n2[j & 0x0f]++;
        n2[(j >> 4) & 0x0f]++;

        for (k = 0; k < 8; k++) {
            s = (j & 0x01);
            if (s == sign)
                nsign++;
            else {
                if (nsign > 34)
                    nsign = 34;
                if (nsign != 0) {
                    runs[sign][nsign - 1]++;
                    if (nsign > 6)
                        runs[sign][5]++;
                }
                sign = s;
                nsign = 1;
            }

            if (s)
                n1++;
            j >>= 1;
        }
    }
    if (nsign > 34)
        nsign = 34;
    if (nsign != 0)
        runs[sign][nsign - 1]++;

    /* test 1 */
    if (!((9654 < n1) && (n1 < 10346))) {
        printf("test 1 failed, X=%lu\n", n1);
        err++;
    }
    printf("test 1 done\n");

    /* test 2 */
    d = 0;
    for (i = 0; i < 16; i++)
        d += n2[i] * n2[i];
    d = (d * 8) / 25 - 500000;
    if (!((103 < d) && (d < 5740))) {
        printf("test 2 failed, X=%ld.%02ld\n", d / 100L, d % 100L);
        err++;
    }
    printf("test 2 done\n");

    /* test 3 */
    for (i = 0; i < 2; i++) {
        if (!((2267 < runs[i][0]) && (runs[i][0] < 2733))) {
            printf("test 3 failed, bit=%d run=%d num=%lu\n",
                   i, 1, runs[i][0]);
            err++;
        }
        if (!((1079 < runs[i][1]) && (runs[i][1] < 1421))) {
            printf("test 3 failed, bit=%d run=%d num=%lu\n",
                   i, 2, runs[i][1]);
            err++;
        }
        if (!((502 < runs[i][2]) && (runs[i][2] < 748))) {
            printf("test 3 failed, bit=%d run=%d num=%lu\n",
                   i, 3, runs[i][2]);
            err++;
        }
        if (!((223 < runs[i][3]) && (runs[i][3] < 402))) {
            printf("test 3 failed, bit=%d run=%d num=%lu\n",
                   i, 4, runs[i][3]);
            err++;
        }
        if (!((90 < runs[i][4]) && (runs[i][4] < 223))) {
            printf("test 3 failed, bit=%d run=%d num=%lu\n",
                   i, 5, runs[i][4]);
            err++;
        }
        if (!((90 < runs[i][5]) && (runs[i][5] < 223))) {
            printf("test 3 failed, bit=%d run=%d num=%lu\n",
                   i, 6, runs[i][5]);
            err++;
        }
    }
    printf("test 3 done\n");

    /* test 4 */
    if (runs[0][33] != 0) {
        printf("test 4 failed, bit=%d run=%d num=%lu\n", 0, 34, runs[0][33]);
        err++;
    }
    if (runs[1][33] != 0) {
        printf("test 4 failed, bit=%d run=%d num=%lu\n", 1, 34, runs[1][33]);
        err++;
    }
    printf("test 4 done\n");
 err:
    err = ((err) ? 1 : 0);
    EXIT(err);
}
