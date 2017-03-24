/*
 * Copyright 2015-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#define declare_dh_bn(x) \
    const extern BIGNUM _bignum_dh##x##_p;              \
    const extern BIGNUM _bignum_dh##x##_g;              \
    const extern BIGNUM _bignum_dh##x##_q;

declare_dh_bn(1024_160)
declare_dh_bn(2048_224)
declare_dh_bn(2048_256)
