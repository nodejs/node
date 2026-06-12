/*
 * Copyright 2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef _CRYPTO_THREADS_COMMON_H_
# define _CRYPTO_THREADS_COMMON_H_

typedef enum {
    CRYPTO_THREAD_LOCAL_RCU_KEY = 0,
    CRYPTO_THREAD_LOCAL_DRBG_PRIV_KEY,
    CRYPTO_THREAD_LOCAL_DRBG_PUB_KEY,
    CRYPTO_THREAD_LOCAL_ERR_KEY,
    CRYPTO_THREAD_LOCAL_ASYNC_CTX_KEY,
    CRYPTO_THREAD_LOCAL_ASYNC_POOL_KEY,
    CRYPTO_THREAD_LOCAL_TEVENT_KEY,
    CRYPTO_THREAD_LOCAL_TANDEM_ID_KEY,
    CRYPTO_THREAD_LOCAL_FIPS_DEFERRED_KEY,
    CRYPTO_THREAD_LOCAL_KEY_MAX
} CRYPTO_THREAD_LOCAL_KEY_ID;

#define CRYPTO_THREAD_NO_CONTEXT (void *)1

void *CRYPTO_THREAD_get_local_ex(CRYPTO_THREAD_LOCAL_KEY_ID id,
                                 OSSL_LIB_CTX *ctx);
int CRYPTO_THREAD_set_local_ex(CRYPTO_THREAD_LOCAL_KEY_ID id,
                               OSSL_LIB_CTX *ctx, void *data);

void CRYPTO_THREAD_clean_local(void);

#endif
