/*
 * Copyright 2020-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#if !defined(APPS_INCLUDE_EC_COMMON_H)
#define APPS_INCLUDE_EC_COMMON_H

#ifndef OPENSSL_NO_EC
#include <stddef.h>

static const char *point_format_options[] = {
    "uncompressed",
    "compressed",
    "hybrid",
    NULL
};

static const char *asn1_encoding_options[] = {
    "named_curve",
    "explicit",
    NULL
};
#endif

#endif /* !defined(APPS_INCLUDE_EC_COMMON_H) */
