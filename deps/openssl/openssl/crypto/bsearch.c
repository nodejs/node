/*
 * Copyright 2019-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stddef.h>
#include "internal/cryptlib.h"

typedef int (*cmpthunk_fn)(const void *, const void *);
const void *ossl_bsearch(const void *key, const void *base, int num,
    int size, int (*cmp)(const void *, const void *),
    int (*cmp_thunk)(cmpthunk_fn real_cmp_fn, const void *, const void *),
    int flags)
{
    const char *base_ = base;
    int l, h, i = 0, c = 0;
    const char *p = NULL;

    if (num == 0)
        return NULL;
    l = 0;
    h = num;
    while (l < h) {
        i = l + (h - l) / 2;
        p = &(base_[i * size]);
        if (cmp_thunk != NULL)
            c = cmp_thunk((cmpthunk_fn)cmp, key, (const void *)p);
        else
            c = cmp(key, p);
        if (c < 0)
            h = i;
        else if (c > 0)
            l = i + 1;
        else
            break;
    }
    if (c != 0 && !(flags & OSSL_BSEARCH_VALUE_ON_NOMATCH))
        p = NULL;
    else if (c == 0 && (flags & OSSL_BSEARCH_FIRST_VALUE_ON_MATCH)) {
        while (i > 0) {
            if (cmp_thunk != NULL) {
                if (cmp_thunk((cmpthunk_fn)cmp, key, (const void *)&(base_[(i - 1) * size])))
                    break;
            } else {
                if (cmp(key, &(base_[(i - 1) * size])))
                    break;
            }
            i--;
        }
        p = &(base_[i * size]);
    }
    return p;
}
