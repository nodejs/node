/*
 * Copyright 1998-2020 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * DES low level APIs are deprecated for public use, but still ok for internal
 * use.
 */
#include "internal/deprecated.h"

#include <openssl/des.h>
#include <openssl/rand.h>

int DES_random_key(DES_cblock *ret)
{
    do {
        if (RAND_priv_bytes((unsigned char *)ret, sizeof(DES_cblock)) != 1)
            return 0;
    } while (DES_is_weak_key(ret));
    DES_set_odd_parity(ret);
    return 1;
}
