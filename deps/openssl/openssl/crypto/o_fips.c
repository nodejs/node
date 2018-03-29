/*
 * Copyright 2011-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "internal/cryptlib.h"
#ifdef OPENSSL_FIPS
# include <openssl/fips.h>
#endif

int FIPS_mode(void)
{
#ifdef OPENSSL_FIPS
    return FIPS_module_mode();
#else
    return 0;
#endif
}

int FIPS_mode_set(int r)
{
#ifdef OPENSSL_FIPS
    return FIPS_module_mode_set(r);
#else
    if (r == 0)
        return 1;
    CRYPTOerr(CRYPTO_F_FIPS_MODE_SET, CRYPTO_R_FIPS_MODE_NOT_SUPPORTED);
    return 0;
#endif
}
