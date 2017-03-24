/*
 * Copyright 2015-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/crypto.h>
#include <stdio.h>

#define SECS_PER_DAY (24 * 60 * 60)

/*
 * Time checking test code. Check times are identical for a wide range of
 * offsets. This should be run on a machine with 64 bit time_t or it will
 * trigger the very errors the routines fix.
 */

static int check_time(long offset)
{
    struct tm tm1, tm2, o1;
    int off_day, off_sec;
    long toffset;
    time_t t1, t2;
    time(&t1);

    t2 = t1 + offset;
    OPENSSL_gmtime(&t2, &tm2);
    OPENSSL_gmtime(&t1, &tm1);
    o1 = tm1;
    OPENSSL_gmtime_adj(&tm1, 0, offset);
    if ((tm1.tm_year != tm2.tm_year) ||
        (tm1.tm_mon != tm2.tm_mon) ||
        (tm1.tm_mday != tm2.tm_mday) ||
        (tm1.tm_hour != tm2.tm_hour) ||
        (tm1.tm_min != tm2.tm_min) || (tm1.tm_sec != tm2.tm_sec)) {
        fprintf(stderr, "TIME ERROR!!\n");
        fprintf(stderr, "Time1: %d/%d/%d, %d:%02d:%02d\n",
                tm2.tm_mday, tm2.tm_mon + 1, tm2.tm_year + 1900,
                tm2.tm_hour, tm2.tm_min, tm2.tm_sec);
        fprintf(stderr, "Time2: %d/%d/%d, %d:%02d:%02d\n",
                tm1.tm_mday, tm1.tm_mon + 1, tm1.tm_year + 1900,
                tm1.tm_hour, tm1.tm_min, tm1.tm_sec);
        return 0;
    }
    if (!OPENSSL_gmtime_diff(&off_day, &off_sec, &o1, &tm1))
        return 0;
    toffset = (long)off_day *SECS_PER_DAY + off_sec;
    if (offset != toffset) {
        fprintf(stderr, "TIME OFFSET ERROR!!\n");
        fprintf(stderr, "Expected %ld, Got %ld (%d:%d)\n",
                offset, toffset, off_day, off_sec);
        return 0;
    }
    return 1;
}

int main(int argc, char **argv)
{
    long offset;
    int fails;

    if (sizeof(time_t) < 8) {
        fprintf(stderr, "Skipping; time_t is less than 64-bits\n");
        return 0;
    }
    for (fails = 0, offset = 0; offset < 1000000; offset++) {
        if (!check_time(offset))
            fails++;
        if (!check_time(-offset))
            fails++;
        if (!check_time(offset * 1000))
            fails++;
        if (!check_time(-offset * 1000))
            fails++;
    }

    return fails ? 1 : 0;
}
