/*
 * Copyright 2018-2026 The OpenSSL Project Authors. All Rights Reserved.
 * Copyright (c) 2018, Oracle and/or its affiliates.  All rights reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef OSSL_CRYPTO_SHA_H
#define OSSL_CRYPTO_SHA_H
#pragma once

#include <openssl/sha.h>

#ifndef OPENSSL_NO_DEPRECATED_3_0
/* This is inside a deprecated block because SHA256_CTX was marked deprecated */
int ossl_sha256_192_init(SHA256_CTX *c);
int sha512_224_init(SHA512_CTX *);
int sha512_256_init(SHA512_CTX *);
int ossl_sha1_ctrl(SHA_CTX *ctx, int cmd, int mslen, void *ms);
#endif

unsigned char *ossl_sha1(const unsigned char *d, size_t n, unsigned char *md);

int ossl_sp800_185_right_encode(unsigned char *out,
    size_t out_max_len, size_t *out_len,
    size_t bits);
int ossl_sp800_185_encode_string(unsigned char *out,
    size_t out_max_len, size_t *out_len,
    const unsigned char *in, size_t in_len);
int ossl_sp800_185_bytepad(unsigned char *out, size_t out_len_max, size_t *out_len,
    const unsigned char *in1, size_t in1_len,
    const unsigned char *in2, size_t in2_len,
    size_t w);

#endif
