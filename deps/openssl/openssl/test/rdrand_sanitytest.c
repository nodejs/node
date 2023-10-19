/*
 * Copyright 2018 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "testutil.h"
#include "internal/cryptlib.h"

#if (defined(__i386)   || defined(__i386__)   || defined(_M_IX86) || \
     defined(__x86_64) || defined(__x86_64__) || \
     defined(_M_AMD64) || defined (_M_X64)) && defined(OPENSSL_CPUID_OBJ)

size_t OPENSSL_ia32_rdrand_bytes(unsigned char *buf, size_t len);
size_t OPENSSL_ia32_rdseed_bytes(unsigned char *buf, size_t len);

static int sanity_check_bytes(size_t (*rng)(unsigned char *, size_t),
    int rounds, int min_failures, int max_retries, int max_zero_words)
{
    int testresult = 0;
    unsigned char prior[31] = {0}, buf[31] = {0}, check[7];
    int failures = 0, zero_words = 0;

    int i;
    for (i = 0; i < rounds; i++) {
        size_t generated = 0;

        int retry;
        for (retry = 0; retry < max_retries; retry++) {
            generated = rng(buf, sizeof(buf));
            if (generated == sizeof(buf))
                break;
            failures++;
        }

        /*-
         * Verify that we don't have too many unexpected runs of zeroes,
         * implying that we might be accidentally using the 32-bit RDRAND
         * instead of the 64-bit one on 64-bit systems.
         */
        size_t j;
        for (j = 0; j < sizeof(buf) - 1; j++) {
            if (buf[j] == 0 && buf[j+1] == 0) {
                zero_words++;
            }
        }

        if (!TEST_int_eq(generated, sizeof(buf)))
            goto end;
        if (!TEST_false(!memcmp(prior, buf, sizeof(buf))))
            goto end;

        /* Verify that the last 7 bytes of buf aren't all the same value */
        unsigned char *tail = &buf[sizeof(buf) - sizeof(check)];
        memset(check, tail[0], 7);
        if (!TEST_false(!memcmp(check, tail, sizeof(check))))
            goto end;

        /* Save the result and make sure it's different next time */
        memcpy(prior, buf, sizeof(buf));
    }

    if (!TEST_int_le(zero_words, max_zero_words))
        goto end;

    if (!TEST_int_ge(failures, min_failures))
        goto end;

    testresult = 1;
end:
    return testresult;
}

static int sanity_check_rdrand_bytes(void)
{
    return sanity_check_bytes(OPENSSL_ia32_rdrand_bytes, 1000, 0, 10, 10);
}

static int sanity_check_rdseed_bytes(void)
{
    /*-
     * RDSEED may take many retries to succeed; note that this is effectively
     * multiplied by the 8x retry loop in asm, and failure probabilities are
     * increased by the fact that we need either 4 or 8 samples depending on
     * the platform.
     */
    return sanity_check_bytes(OPENSSL_ia32_rdseed_bytes, 1000, 1, 10000, 10);
}

int setup_tests(void)
{
    OPENSSL_cpuid_setup();

    int have_rdseed = (OPENSSL_ia32cap_P[2] & (1 << 18)) != 0;
    int have_rdrand = (OPENSSL_ia32cap_P[1] & (1 << (62 - 32))) != 0;

    if (have_rdrand) {
        ADD_TEST(sanity_check_rdrand_bytes);
    }

    if (have_rdseed) {
        ADD_TEST(sanity_check_rdseed_bytes);
    }

    return 1;
}


#else

int setup_tests(void)
{
    return 1;
}

#endif
