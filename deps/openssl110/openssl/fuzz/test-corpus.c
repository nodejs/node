/*
 * Copyright 2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL licenses, (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * https://www.openssl.org/source/license.html
 * or in the file LICENSE in the source distribution.
 */

/*
 * Given a list of files, run each of them through the fuzzer.  Note that
 * failure will be indicated by some kind of crash. Switching on things like
 * asan improves the test.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <openssl/crypto.h>
#include "fuzzer.h"

int main(int argc, char **argv) {
    int n;

    FuzzerInitialize(&argc, &argv);

    for (n = 1; n < argc; ++n) {
        struct stat st;
        FILE *f;
        unsigned char *buf;
        size_t s;

        stat(argv[n], &st);
        f = fopen(argv[n], "rb");
        if (f == NULL)
            continue;
        buf = malloc(st.st_size);
        s = fread(buf, 1, st.st_size, f);
        OPENSSL_assert(s == (size_t)st.st_size);
        FuzzerTestOneInput(buf, s);
        free(buf);
        fclose(f);
    }
    return 0;
}
