/*
 * Copyright 1995-2020 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * CAST low level APIs are deprecated for public use, but still ok for
 * internal use.
 */
#include "internal/deprecated.h"

#include <openssl/cast.h>
#include "cast_local.h"
#include <openssl/opensslv.h>

void CAST_ecb_encrypt(const unsigned char *in, unsigned char *out,
                      const CAST_KEY *ks, int enc)
{
    CAST_LONG l, d[2];

    n2l(in, l);
    d[0] = l;
    n2l(in, l);
    d[1] = l;
    if (enc)
        CAST_encrypt(d, ks);
    else
        CAST_decrypt(d, ks);
    l = d[0];
    l2n(l, out);
    l = d[1];
    l2n(l, out);
    l = d[0] = d[1] = 0;
}
