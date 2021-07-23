/*
 * Copyright 2020 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "crypto/evp.h"

OSSL_FUNC_cipher_update_fn ossl_aes_cbc_cts_block_update;
OSSL_FUNC_cipher_final_fn ossl_aes_cbc_cts_block_final;

const char *ossl_aes_cbc_cts_mode_id2name(unsigned int id);
int ossl_aes_cbc_cts_mode_name2id(const char *name);
