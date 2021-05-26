/*
 * Copyright 2020 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* This is the C source file where we include this header directly */
#include <openssl/sslerr_legacy.h>
#include "sslerr.h"

#ifndef OPENSSL_NO_DEPRECATED_3_0
int ERR_load_SSL_strings(void)
{
    return err_load_SSL_strings_int();
}
#else
NON_EMPTY_TRANSLATION_UNIT
#endif
