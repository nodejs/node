/*
 *  Version information
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#include "common.h"

#if defined(MBEDTLS_VERSION_C)

#include "mbedtls/version.h"
#include <string.h>

unsigned int mbedtls_version_get_number(void)
{
    return MBEDTLS_VERSION_NUMBER;
}

void mbedtls_version_get_string(char *string)
{
    memcpy(string, MBEDTLS_VERSION_STRING,
           sizeof(MBEDTLS_VERSION_STRING));
}

void mbedtls_version_get_string_full(char *string)
{
    memcpy(string, MBEDTLS_VERSION_STRING_FULL,
           sizeof(MBEDTLS_VERSION_STRING_FULL));
}

#endif /* MBEDTLS_VERSION_C */
