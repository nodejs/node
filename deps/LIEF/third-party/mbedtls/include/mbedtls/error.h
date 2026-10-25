/**
 * \file error.h
 *
 * \brief Error to string translation
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#ifndef MBEDTLS_ERROR_H
#define MBEDTLS_ERROR_H

#include "mbedtls/build_info.h"
#include "mbedtls/private/error_common.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Translate an Mbed TLS error code into a string representation.
 *        The result is truncated if necessary and always includes a
 *        terminating null byte.
 *
 * \param errnum    error code
 * \param buffer    buffer to place representation in
 * \param buflen    length of the buffer
 */
void mbedtls_strerror(int errnum, char *buffer, size_t buflen);

#ifdef __cplusplus
}
#endif

#endif /* error.h */
