/*
 * Copyright 2019-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* This header can move into provider when legacy support is removed */
#ifndef OSSL_INTERNAL_SHA3_H
# define OSSL_INTERNAL_SHA3_H
# pragma once

# include <openssl/e_os2.h>
# include <stddef.h>

# define KECCAK1600_WIDTH 1600
# define SHA3_MDSIZE(bitlen)    (bitlen / 8)
# define KMAC_MDSIZE(bitlen)    2 * (bitlen / 8)
# define SHA3_BLOCKSIZE(bitlen) (KECCAK1600_WIDTH - bitlen * 2) / 8

typedef struct keccak_st KECCAK1600_CTX;

typedef size_t (sha3_absorb_fn)(void *vctx, const void *inp, size_t len);
typedef int (sha3_final_fn)(unsigned char *md, void *vctx);

typedef struct prov_sha3_meth_st
{
    sha3_absorb_fn *absorb;
    sha3_final_fn *final;
} PROV_SHA3_METHOD;

struct keccak_st {
    uint64_t A[5][5];
    size_t block_size;          /* cached ctx->digest->block_size */
    size_t md_size;             /* output length, variable in XOF */
    size_t bufsz;               /* used bytes in below buffer */
    unsigned char buf[KECCAK1600_WIDTH / 8 - 32];
    unsigned char pad;
    PROV_SHA3_METHOD meth;
};

void ossl_sha3_reset(KECCAK1600_CTX *ctx);
int ossl_sha3_init(KECCAK1600_CTX *ctx, unsigned char pad, size_t bitlen);
int ossl_keccak_kmac_init(KECCAK1600_CTX *ctx, unsigned char pad,
                          size_t bitlen);
int ossl_sha3_update(KECCAK1600_CTX *ctx, const void *_inp, size_t len);
int ossl_sha3_final(unsigned char *md, KECCAK1600_CTX *ctx);

size_t SHA3_absorb(uint64_t A[5][5], const unsigned char *inp, size_t len,
                   size_t r);

#endif /* OSSL_INTERNAL_SHA3_H */
