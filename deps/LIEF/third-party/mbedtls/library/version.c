/*
 *  Version information
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#include "ssl_misc.h"

#if defined(MBEDTLS_VERSION_C)

#include "mbedtls/version.h"
#include <string.h>

unsigned int mbedtls_version_get_number(void)
{
    return MBEDTLS_VERSION_NUMBER;
}

const char *mbedtls_version_get_string(void)
{
    return MBEDTLS_VERSION_STRING;
}

const char *mbedtls_version_get_string_full(void)
{
    return MBEDTLS_VERSION_STRING_FULL;
}

#endif /* MBEDTLS_VERSION_C */
