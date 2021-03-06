/*
 * Copyright 2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL licenses, (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * https://www.openssl.org/source/license.html
 * or in the file LICENSE in the source distribution.
 */
#include <openssl/rand.h>

static int fuzz_bytes(unsigned char *buf, int num)
{
    unsigned char val = 1;

    while (--num >= 0)
        *buf++ = val++;
    return 1;
}

static int fuzz_status(void)
{
    return 1;
}

static RAND_METHOD fuzz_rand_method = {
    NULL,
    fuzz_bytes,
    NULL,
    NULL,
    fuzz_bytes,
    fuzz_status
};

void FuzzerSetRand(void)
{
    RAND_set_rand_method(&fuzz_rand_method);
}


