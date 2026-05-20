/**
 * \file cipher_invasive.h
 *
 * \brief Cipher module: interfaces for invasive testing only.
 *
 * The interfaces in this file are intended for testing purposes only.
 * They SHOULD NOT be made available in library integrations except when
 * building the library for testing.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#ifndef MBEDTLS_CIPHER_INVASIVE_H
#define MBEDTLS_CIPHER_INVASIVE_H

#include "common.h"

#if defined(MBEDTLS_TEST_HOOKS) && defined(MBEDTLS_CIPHER_C)

MBEDTLS_STATIC_TESTABLE int mbedtls_get_pkcs_padding(unsigned char *input,
                                                     size_t input_len,
                                                     size_t *data_len);

#endif

#endif /* MBEDTLS_CIPHER_INVASIVE_H */
