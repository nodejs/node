/*
 * Copyright 2016-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License");
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
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include "fuzzer.h"
#include "internal/o_dir.h"
#include "mfail.h"

#if defined(_WIN32) && defined(_MAX_PATH) && !defined(PATH_MAX)
#define PATH_MAX _MAX_PATH
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#if !defined(S_ISREG)
#define S_ISREG(m) ((m) & S_IFREG)
#endif

static double secs_since(clock_t start)
{
    return (double)(clock() - start) / CLOCKS_PER_SEC;
}

static void run_baseline(const unsigned char *buf, size_t s)
{
    FuzzerTestOneInput(buf, s);
}

static void run_mfail(const unsigned char *buf, size_t s,
    const char *path, int file_idx)
{
    mfail_init(file_idx, MFAIL_FLAG_COUNT);
    while (mfail_has_next()) {
        if (mfail_get_phase() == MFAIL_PHASE_COUNTING)
            fprintf(stderr,
                "# MFAIL_BEGIN file_idx=%d phase=count\n", file_idx);
        else
            fprintf(stderr,
                "# MFAIL_BEGIN file_idx=%d point=%d/%d\n",
                file_idx, mfail_get_point(), mfail_get_total());

        mfail_start();
        FuzzerTestOneInput(buf, s);
        mfail_end();

        if (mfail_get_phase() == MFAIL_PHASE_COUNTING) {
            fprintf(stderr, "# %s: %d allocations\n", path, mfail_get_count());
        } else {
            fprintf(stderr, "# %s: point %d/%d %s\n", path,
                mfail_get_point(), mfail_get_total(),
                mfail_was_triggered() ? "hit" : "unreached");
        }
        ERR_clear_error();
    }
}

static void testfile(const char *pathname, int file_idx)
{
    struct stat st;
    FILE *f;
    unsigned char *buf;
    size_t s;

    if (stat(pathname, &st) < 0 || !S_ISREG(st.st_mode))
        return;

    fprintf(stderr, "# CORPUS_FILE file_idx=%d size=%lld path=%s\n",
        file_idx, (long long)st.st_size, pathname);
    f = fopen(pathname, "rb");
    if (f == NULL)
        return;
    buf = malloc(st.st_size);
    if (buf == NULL) {
        fclose(f);
        return;
    }
    s = fread(buf, 1, st.st_size, f);
    OPENSSL_assert(s == (size_t)st.st_size);

    if (mfail_is_installed())
        run_mfail(buf, s, pathname, file_idx);
    else
        run_baseline(buf, s);

    free(buf);
    fclose(f);
}

int main(int argc, char **argv)
{
    int n, mfi_rc;
    int file_idx = 0;
    clock_t corpus_start;

    mfi_rc = mfail_install(1);
    if (mfi_rc < 0) {
        fprintf(stderr, "mfail: failed to install allocator hooks\n");
        return 1;
    } else if (mfi_rc > 0) {
        /* Disable buffering for better crash analysis */
        setvbuf(stdout, NULL, _IOLBF, 0);
        setvbuf(stderr, NULL, _IOLBF, 0);
    }

    if (FuzzerInitialize(&argc, &argv) < 0) {
        if (mfail_is_installed())
            return 0; /* init failure under mfail is expected */
        return 1;
    }

    corpus_start = clock();

    for (n = 1; n < argc; ++n) {
        size_t dirname_len = strlen(argv[n]);
        const char *filename = NULL;
        char *pathname = NULL;
        OPENSSL_DIR_CTX *ctx = NULL;
        int wasdir = 0;

        /*
         * We start with trying to read the given path as a directory.
         */
        while ((filename = OPENSSL_DIR_read(&ctx, argv[n])) != NULL) {
            wasdir = 1;
            if (pathname == NULL) {
                pathname = malloc(PATH_MAX);
                if (pathname == NULL)
                    break;
                strcpy(pathname, argv[n]);
#ifdef __VMS
                if (strchr(":<]", pathname[dirname_len - 1]) == NULL)
#endif
                    pathname[dirname_len++] = '/';
                pathname[dirname_len] = '\0';
            }
            strcpy(pathname + dirname_len, filename);
            testfile(pathname, file_idx++);
        }
        OPENSSL_DIR_end(&ctx);

        /* If it wasn't a directory, treat it as a file instead */
        if (!wasdir)
            testfile(argv[n], file_idx++);

        free(pathname);
    }

    if (!mfail_is_installed() || mfail_is_count_only())
        fprintf(stderr, "# corpus_time: %.6f\n", secs_since(corpus_start));

    FuzzerCleanup();
    return 0;
}
