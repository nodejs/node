/*
 * Copyright 2011-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <e_os.h>
#include <openssl/err.h>
#ifdef OPENSSL_FIPS
# include <openssl/fips.h>
# include <openssl/rand.h>
#endif

/*
 * Perform any essential OpenSSL initialization operations. Currently only
 * sets FIPS callbacks
 */

void OPENSSL_init(void)
{
    static int done = 0;
    if (done)
        return;
    done = 1;
#ifdef OPENSSL_FIPS
    FIPS_set_locking_callbacks(CRYPTO_lock, CRYPTO_add_lock);
    FIPS_set_error_callbacks(ERR_put_error, ERR_add_error_vdata);
    FIPS_set_malloc_callbacks(CRYPTO_malloc, CRYPTO_free);
    RAND_init_fips();
#endif
}
