/*
 * Copyright 2002-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <string.h>
#include "ui_locl.h"

#ifndef BUFSIZ
#define BUFSIZ 256
#endif

int UI_UTIL_read_pw_string(char *buf, int length, const char *prompt,
                           int verify)
{
    char buff[BUFSIZ];
    int ret;

    ret =
        UI_UTIL_read_pw(buf, buff, (length > BUFSIZ) ? BUFSIZ : length,
                        prompt, verify);
    OPENSSL_cleanse(buff, BUFSIZ);
    return (ret);
}

int UI_UTIL_read_pw(char *buf, char *buff, int size, const char *prompt,
                    int verify)
{
    int ok = 0;
    UI *ui;

    if (size < 1)
        return -1;

    ui = UI_new();
    if (ui != NULL) {
        ok = UI_add_input_string(ui, prompt, 0, buf, 0, size - 1);
        if (ok >= 0 && verify)
            ok = UI_add_verify_string(ui, prompt, 0, buff, 0, size - 1, buf);
        if (ok >= 0)
            ok = UI_process(ui);
        UI_free(ui);
    }
    if (ok > 0)
        ok = 0;
    return (ok);
}
