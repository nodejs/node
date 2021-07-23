/*
 * Copyright 2019-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "apps.h"

/* shim that avoids sucking in too much from apps/apps.c */

void *app_malloc(size_t sz, const char *what)
{
    void *vp = OPENSSL_malloc(sz);

    return vp;
}
