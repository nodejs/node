/*
 * Copyright 2002-2020 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <string.h> /* strlen */
#include <openssl/crypto.h>
#include "ec_local.h"

static const char *HEX_DIGITS = "0123456789ABCDEF";

/* the return value must be freed (using OPENSSL_free()) */
char *EC_POINT_point2hex(const EC_GROUP *group,
                         const EC_POINT *point,
                         point_conversion_form_t form, BN_CTX *ctx)
{
    char *ret, *p;
    size_t buf_len = 0, i;
    unsigned char *buf = NULL, *pbuf;

    buf_len = EC_POINT_point2buf(group, point, form, &buf, ctx);

    if (buf_len == 0)
        return NULL;

    ret = OPENSSL_malloc(buf_len * 2 + 2);
    if (ret == NULL) {
        OPENSSL_free(buf);
        return NULL;
    }
    p = ret;
    pbuf = buf;
    for (i = buf_len; i > 0; i--) {
        int v = (int)*(pbuf++);
        *(p++) = HEX_DIGITS[v >> 4];
        *(p++) = HEX_DIGITS[v & 0x0F];
    }
    *p = '\0';

    OPENSSL_free(buf);

    return ret;
}

EC_POINT *EC_POINT_hex2point(const EC_GROUP *group,
                             const char *hex, EC_POINT *point, BN_CTX *ctx)
{
    int ok = 0;
    unsigned char *oct_buf = NULL;
    size_t len, oct_buf_len = 0;
    EC_POINT *pt = NULL;

    if (group == NULL || hex == NULL)
        return NULL;

    if (point == NULL) {
        pt = EC_POINT_new(group);
        if (pt == NULL)
            goto err;
    } else {
        pt = point;
    }

    len = strlen(hex) / 2;
    oct_buf = OPENSSL_malloc(len);
    if (oct_buf == NULL)
        goto err;

    if (!OPENSSL_hexstr2buf_ex(oct_buf, len, &oct_buf_len, hex, '\0')
        || !EC_POINT_oct2point(group, pt, oct_buf, oct_buf_len, ctx))
        goto err;
    ok = 1;
err:
    OPENSSL_clear_free(oct_buf, oct_buf_len);
    if (!ok) {
        if (pt != point)
            EC_POINT_clear_free(pt);
        pt = NULL;
    }
    return pt;
}
