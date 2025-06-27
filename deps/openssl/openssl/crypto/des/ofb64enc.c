/*
 * Copyright 1995-2020 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * DES low level APIs are deprecated for public use, but still ok for internal
 * use.
 */
#include "internal/deprecated.h"

#include "des_local.h"

/*
 * The input and output encrypted as though 64bit ofb mode is being used.
 * The extra state information to record how much of the 64bit block we have
 * used is contained in *num;
 */
void DES_ofb64_encrypt(register const unsigned char *in,
                       register unsigned char *out, long length,
                       DES_key_schedule *schedule, DES_cblock *ivec, int *num)
{
    register DES_LONG v0, v1, t;
    register int n = *num;
    register long l = length;
    DES_cblock d;
    register unsigned char *dp;
    DES_LONG ti[2];
    unsigned char *iv;
    int save = 0;

    iv = &(*ivec)[0];
    c2l(iv, v0);
    c2l(iv, v1);
    ti[0] = v0;
    ti[1] = v1;
    dp = d;
    l2c(v0, dp);
    l2c(v1, dp);
    while (l--) {
        if (n == 0) {
            DES_encrypt1(ti, schedule, DES_ENCRYPT);
            dp = d;
            t = ti[0];
            l2c(t, dp);
            t = ti[1];
            l2c(t, dp);
            save++;
        }
        *(out++) = *(in++) ^ d[n];
        n = (n + 1) & 0x07;
    }
    if (save) {
        v0 = ti[0];
        v1 = ti[1];
        iv = &(*ivec)[0];
        l2c(v0, iv);
        l2c(v1, iv);
    }
    t = v0 = v1 = ti[0] = ti[1] = 0;
    *num = n;
}
