/*
 * Copyright 2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL licenses, (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * https://www.openssl.org/source/license.html
 * or in the file LICENSE in the source distribution.
 */
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <openssl/opensslconf.h>
#include "fuzzer.h"

#ifndef OPENSSL_NO_FUZZ_LIBFUZZER

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    if (FuzzerInitialize)
        return FuzzerInitialize(argc, argv);
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len) {
    return FuzzerTestOneInput(buf, len);
}

#elif !defined(OPENSSL_NO_FUZZ_AFL)

#define BUF_SIZE 65536

int main(int argc, char** argv)
{
    if (FuzzerInitialize)
        FuzzerInitialize(&argc, &argv);

    while (__AFL_LOOP(10000)) {
        uint8_t *buf = malloc(BUF_SIZE);
        size_t size = read(0, buf, BUF_SIZE);

        FuzzerTestOneInput(buf, size);
        free(buf);
    }
    return 0;
}

#else

#error "Unsupported fuzzer"

#endif
