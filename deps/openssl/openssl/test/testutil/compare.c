/*
 * Copyright 2017-2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "../testutil.h"

static void strip_line_ends(char *str)
{
    size_t i;

    for (i = strlen(str);
         i > 0 && (str[i - 1] == '\n' || str[i - 1] == '\r');
         i--);

    str[i] = '\0';
}

int compare_with_reference_file(BIO *membio, const char *reffile)
{
    BIO *file = NULL, *newfile = NULL;
    char buf1[8192], buf2[8192];
    int ret = 0;
    size_t i;

    if (!TEST_ptr(reffile))
        goto err;

    file = BIO_new_file(reffile, "rb");
    if (!TEST_ptr(file))
        goto err;

    newfile = BIO_new_file("ssltraceref-new.txt", "wb");
    if (!TEST_ptr(newfile))
        goto err;

    while (BIO_gets(membio, buf2, sizeof(buf2)) > 0)
        if (BIO_puts(newfile, buf2) <= 0) {
            TEST_error("Failed writing new file data");
            goto err;
        }

    if (!TEST_int_ge(BIO_seek(membio, 0), 0))
        goto err;

    while (BIO_gets(file, buf1, sizeof(buf1)) > 0) {
        size_t line_len;

        if (BIO_gets(membio, buf2, sizeof(buf2)) <= 0) {
            TEST_error("Failed reading mem data");
            goto err;
        }
        strip_line_ends(buf1);
        strip_line_ends(buf2);
        line_len = strlen(buf1);
        if (line_len > 0 && buf1[line_len - 1] == '?') {
            /* Wildcard at the EOL means ignore anything after it */
            if (strlen(buf2) > line_len)
                buf2[line_len] = '\0';
        }
        if (line_len != strlen(buf2)) {
            TEST_error("Actual and ref line data length mismatch");
            TEST_info("%s", buf1);
            TEST_info("%s", buf2);
            goto err;
        }
        for (i = 0; i < line_len; i++) {
            /* '?' is a wild card character in the reference text */
            if (buf1[i] == '?')
                buf2[i] = '?';
        }
        if (!TEST_str_eq(buf1, buf2))
            goto err;
    }
    if (!TEST_true(BIO_eof(file))
            || !TEST_true(BIO_eof(membio)))
        goto err;

    ret = 1;
 err:
    BIO_free(file);
    BIO_free(newfile);
    return ret;
}
