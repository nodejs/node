/*
 * Copyright 2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * https://www.openssl.org/source/license.html
 * or in the file LICENSE in the source distribution.
 */
#include <limits.h>
#include <openssl/ech.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <openssl/e_os2.h>
#include <openssl/byteorder.h>
#include "fuzzer.h"

static void parse_one(const uint8_t *buf, int len)
{
    OSSL_ECHSTORE *es;
    BIO *in;

    es = OSSL_ECHSTORE_new(NULL, NULL);
    if (es == NULL)
        return;

    in = BIO_new_mem_buf(buf, len);
    if (in == NULL) {
        OSSL_ECHSTORE_free(es);
        return;
    }

    OSSL_ECHSTORE_read_echconfiglist(es, in);

    OSSL_ECHSTORE_free(es);
    BIO_free(in);
}

int FuzzerInitialize(int *argc, char ***argv)
{
    return 1;
}

int FuzzerTestOneInput(const uint8_t *buf, size_t len)
{
    uint8_t *fixed_buf = NULL;
    int bio_len;
    uint16_t outer_len, inner_len;

    if (len > INT_MAX)
        return 0;
    bio_len = (int)len;

    /* Target raw without any fixup */
    parse_one(buf, bio_len);

    /*
     * ech_decode_and_flatten has a strict size check:
     * OSSL_ECH_MIN_ECHCONFIG_LEN = 32
     * OSSL_ECH_MAX_ECHCONFIG_LEN = 1500
     */
    if (len < OSSL_ECH_MIN_ECHCONFIG_LEN || len >= OSSL_ECH_MAX_ECHCONFIG_LEN)
        goto end;
    outer_len = (uint16_t)(len - 2);
    inner_len = (uint16_t)(len - 6);

    fixed_buf = OPENSSL_memdup(buf, len);
    if (fixed_buf == NULL)
        goto end;

    /* Fix up to pass initial checks*/
    OPENSSL_store_u16_be(fixed_buf, outer_len);
    OPENSSL_store_u16_be(fixed_buf + 2, OSSL_ECH_RFC9849_VERSION);
    OPENSSL_store_u16_be(fixed_buf + 4, inner_len);

    parse_one(fixed_buf, bio_len);

end:
    OPENSSL_free(fixed_buf);
    ERR_clear_error();

    return 0;
}

void FuzzerCleanup(void)
{
}
