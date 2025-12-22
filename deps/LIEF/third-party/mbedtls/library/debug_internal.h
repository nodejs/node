/**
 * \file debug_internal.h
 *
 * \brief Internal part of the public "debug.h".
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#ifndef MBEDTLS_DEBUG_INTERNAL_H
#define MBEDTLS_DEBUG_INTERNAL_H

#include "mbedtls/debug.h"

/**
 * \brief    Print a message to the debug output. This function is always used
 *          through the MBEDTLS_SSL_DEBUG_MSG() macro, which supplies the ssl
 *          context, file and line number parameters.
 *
 * \param ssl       SSL context
 * \param level     error level of the debug message
 * \param file      file the message has occurred in
 * \param line      line number the message has occurred at
 * \param format    format specifier, in printf format
 * \param ...       variables used by the format specifier
 *
 * \attention       This function is intended for INTERNAL usage within the
 *                  library only.
 */
void mbedtls_debug_print_msg(const mbedtls_ssl_context *ssl, int level,
                             const char *file, int line,
                             const char *format, ...) MBEDTLS_PRINTF_ATTRIBUTE(5, 6);

/**
 * \brief   Print the return value of a function to the debug output. This
 *          function is always used through the MBEDTLS_SSL_DEBUG_RET() macro,
 *          which supplies the ssl context, file and line number parameters.
 *
 * \param ssl       SSL context
 * \param level     error level of the debug message
 * \param file      file the error has occurred in
 * \param line      line number the error has occurred in
 * \param text      the name of the function that returned the error
 * \param ret       the return code value
 *
 * \attention       This function is intended for INTERNAL usage within the
 *                  library only.
 */
void mbedtls_debug_print_ret(const mbedtls_ssl_context *ssl, int level,
                             const char *file, int line,
                             const char *text, int ret);

/**
 * \brief   Output a buffer of size len bytes to the debug output. This function
 *          is always used through the MBEDTLS_SSL_DEBUG_BUF() macro,
 *          which supplies the ssl context, file and line number parameters.
 *
 * \param ssl       SSL context
 * \param level     error level of the debug message
 * \param file      file the error has occurred in
 * \param line      line number the error has occurred in
 * \param text      a name or label for the buffer being dumped. Normally the
 *                  variable or buffer name
 * \param buf       the buffer to be outputted
 * \param len       length of the buffer
 *
 * \attention       This function is intended for INTERNAL usage within the
 *                  library only.
 */
void mbedtls_debug_print_buf(const mbedtls_ssl_context *ssl, int level,
                             const char *file, int line, const char *text,
                             const unsigned char *buf, size_t len);

#if defined(MBEDTLS_BIGNUM_C)
/**
 * \brief   Print a MPI variable to the debug output. This function is always
 *          used through the MBEDTLS_SSL_DEBUG_MPI() macro, which supplies the
 *          ssl context, file and line number parameters.
 *
 * \param ssl       SSL context
 * \param level     error level of the debug message
 * \param file      file the error has occurred in
 * \param line      line number the error has occurred in
 * \param text      a name or label for the MPI being output. Normally the
 *                  variable name
 * \param X         the MPI variable
 *
 * \attention       This function is intended for INTERNAL usage within the
 *                  library only.
 */
void mbedtls_debug_print_mpi(const mbedtls_ssl_context *ssl, int level,
                             const char *file, int line,
                             const char *text, const mbedtls_mpi *X);
#endif

#if defined(MBEDTLS_ECP_LIGHT)
/**
 * \brief   Print an ECP point to the debug output. This function is always
 *          used through the MBEDTLS_SSL_DEBUG_ECP() macro, which supplies the
 *          ssl context, file and line number parameters.
 *
 * \param ssl       SSL context
 * \param level     error level of the debug message
 * \param file      file the error has occurred in
 * \param line      line number the error has occurred in
 * \param text      a name or label for the ECP point being output. Normally the
 *                  variable name
 * \param X         the ECP point
 *
 * \attention       This function is intended for INTERNAL usage within the
 *                  library only.
 */
void mbedtls_debug_print_ecp(const mbedtls_ssl_context *ssl, int level,
                             const char *file, int line,
                             const char *text, const mbedtls_ecp_point *X);
#endif

#if defined(MBEDTLS_X509_CRT_PARSE_C) && !defined(MBEDTLS_X509_REMOVE_INFO)
/**
 * \brief   Print a X.509 certificate structure to the debug output. This
 *          function is always used through the MBEDTLS_SSL_DEBUG_CRT() macro,
 *          which supplies the ssl context, file and line number parameters.
 *
 * \param ssl       SSL context
 * \param level     error level of the debug message
 * \param file      file the error has occurred in
 * \param line      line number the error has occurred in
 * \param text      a name or label for the certificate being output
 * \param crt       X.509 certificate structure
 *
 * \attention       This function is intended for INTERNAL usage within the
 *                  library only.
 */
void mbedtls_debug_print_crt(const mbedtls_ssl_context *ssl, int level,
                             const char *file, int line,
                             const char *text, const mbedtls_x509_crt *crt);
#endif

/* Note: the MBEDTLS_ECDH_C guard here is mandatory because this debug function
         only works for the built-in implementation. */
#if defined(MBEDTLS_KEY_EXCHANGE_SOME_ECDH_OR_ECDHE_ANY_ENABLED) && \
    defined(MBEDTLS_ECDH_C)
typedef enum {
    MBEDTLS_DEBUG_ECDH_Q,
    MBEDTLS_DEBUG_ECDH_QP,
    MBEDTLS_DEBUG_ECDH_Z,
} mbedtls_debug_ecdh_attr;

/**
 * \brief   Print a field of the ECDH structure in the SSL context to the debug
 *          output. This function is always used through the
 *          MBEDTLS_SSL_DEBUG_ECDH() macro, which supplies the ssl context, file
 *          and line number parameters.
 *
 * \param ssl       SSL context
 * \param level     error level of the debug message
 * \param file      file the error has occurred in
 * \param line      line number the error has occurred in
 * \param ecdh      the ECDH context
 * \param attr      the identifier of the attribute being output
 *
 * \attention       This function is intended for INTERNAL usage within the
 *                  library only.
 */
void mbedtls_debug_printf_ecdh(const mbedtls_ssl_context *ssl, int level,
                               const char *file, int line,
                               const mbedtls_ecdh_context *ecdh,
                               mbedtls_debug_ecdh_attr attr);
#endif /* MBEDTLS_KEY_EXCHANGE_SOME_ECDH_OR_ECDHE_ANY_ENABLED &&
          MBEDTLS_ECDH_C */

#endif /* MBEDTLS_DEBUG_INTERNAL_H */
