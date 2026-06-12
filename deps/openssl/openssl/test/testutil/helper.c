/*
 * Copyright 2020-2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */
#include <time.h>

#include <openssl/asn1t.h>
#include <openssl/posix_time.h>

#include "../testutil.h"

int test_asn1_string_to_time_t(const char *asn1_string, time_t *out_time_t)
{
    int ret = 0;
    ASN1_TIME *timestamp_asn1 = NULL;
    struct tm timestamp_tm;

    timestamp_asn1 = ASN1_TIME_new();
    if(timestamp_asn1 == NULL)
        goto err;

    if (!ASN1_TIME_set_string(timestamp_asn1, asn1_string))
        goto err;

    if (!(ASN1_TIME_to_tm(timestamp_asn1, &timestamp_tm)))
        goto err;

    if (!OPENSSL_timegm(&timestamp_tm, out_time_t))
        goto err;

    ret = 1;

err:
    ASN1_TIME_free(timestamp_asn1);
    return ret;
}
