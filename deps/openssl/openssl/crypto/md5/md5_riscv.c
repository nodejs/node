/*
 * Copyright 2025-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/opensslconf.h>
#include <openssl/md5.h>
#include "arch/riscv_arch.h"

void ossl_md5_block_asm_data_order(void *c, const void *p, size_t num);
void ossl_md5_block_asm_data_order_zbb(void *c, const void *p, size_t num);
void ossl_md5_block_asm_data_order_riscv64(void *c, const void *p, size_t num);
void ossl_md5_block_asm_data_order(void *c, const void *p, size_t num)
{
    if (RISCV_HAS_ZBB()) {
        ossl_md5_block_asm_data_order_zbb(c, p, num);
    } else {
        ossl_md5_block_asm_data_order_riscv64(c, p, num);
    }
}
