/*
 * Copyright 2018 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <openssl/opensslv.h>
#include <openssl/crypto.h>

/* A simple helper for the perl function OpenSSL::Test::openssl_versions */
int main(void)
{
    printf("Build version: 0x%08lX\n", OPENSSL_VERSION_NUMBER);
    printf("Library version: 0x%08lX\n", OpenSSL_version_num());
    return 0;
}
