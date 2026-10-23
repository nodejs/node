/*
 * Copyright 2025-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdlib.h>
#include <string.h>

#include <openssl/opensslconf.h>
#include <openssl/sha.h>
#include "arch/loongarch_arch.h"

void sha256_block_data_order_la64v100(void *ctx, const void *in, size_t num);
void sha256_block_data_order_lsx(void *ctx, const void *in, size_t num);
void sha256_block_data_order(void *ctx, const void *in, size_t num);

void sha256_block_data_order(void *ctx, const void *in, size_t num)
{
    if (OPENSSL_loongarch_hwcap_P & LOONGARCH_HWCAP_LSX) {
        sha256_block_data_order_lsx(ctx, in, num);
    } else {
        sha256_block_data_order_la64v100(ctx, in, num);
    }
}

void sha512_block_data_order_la64v100(void *ctx, const void *in, size_t num);
void sha512_block_data_order_lsx(void *ctx, const void *in, size_t num);
void sha512_block_data_order(void *ctx, const void *in, size_t num);

void sha512_block_data_order(void *ctx, const void *in, size_t num)
{
    if (OPENSSL_loongarch_hwcap_P & LOONGARCH_HWCAP_LSX) {
        sha512_block_data_order_lsx(ctx, in, num);
    } else {
        sha512_block_data_order_la64v100(ctx, in, num);
    }
}
