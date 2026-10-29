/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

/**
 * \file mps_error.h
 *
 * \brief Error codes used by MPS
 */

#ifndef MBEDTLS_MPS_ERROR_H
#define MBEDTLS_MPS_ERROR_H


/* TODO: The error code allocation needs to be revisited:
 *
 * - Should we make (some of) the MPS Reader error codes public?
 *   If so, we need to adjust MBEDTLS_MPS_READER_MAKE_ERROR() to hit
 *   a gap in the Mbed TLS public error space.
 *   If not, we have to make sure we don't forward those errors
 *   at the level of the public API -- no risk at the moment as
 *   long as MPS is an experimental component not accessible from
 *   public API.
 */

/**
 * \name SECTION:       MPS general error codes
 *
 * \{
 */

#ifndef MBEDTLS_MPS_ERR_BASE
#define MBEDTLS_MPS_ERR_BASE (0)
#endif

#define MBEDTLS_MPS_MAKE_ERROR(code) \
    (-(MBEDTLS_MPS_ERR_BASE | (code)))

#define MBEDTLS_ERR_MPS_OPERATION_UNEXPECTED  MBEDTLS_MPS_MAKE_ERROR(0x1)
#define MBEDTLS_ERR_MPS_INTERNAL_ERROR        MBEDTLS_MPS_MAKE_ERROR(0x2)

/* \} name SECTION: MPS general error codes */

/**
 * \name SECTION:       MPS Reader error codes
 *
 * \{
 */

#ifndef MBEDTLS_MPS_READER_ERR_BASE
#define MBEDTLS_MPS_READER_ERR_BASE (1 << 8)
#endif

#define MBEDTLS_MPS_READER_MAKE_ERROR(code) \
    (-(MBEDTLS_MPS_READER_ERR_BASE | (code)))

/*! An attempt to reclaim the data buffer from a reader failed because
 *  the user hasn't yet read and committed all of it. */
#define MBEDTLS_ERR_MPS_READER_DATA_LEFT             MBEDTLS_MPS_READER_MAKE_ERROR(0x1)

/*! An invalid argument was passed to the reader. */
#define MBEDTLS_ERR_MPS_READER_INVALID_ARG           MBEDTLS_MPS_READER_MAKE_ERROR(0x2)

/*! An attempt to move a reader to consuming mode through mbedtls_mps_reader_feed()
 *  after pausing failed because the provided data is not sufficient to serve the
 *  read requests that led to the pausing. */
#define MBEDTLS_ERR_MPS_READER_NEED_MORE             MBEDTLS_MPS_READER_MAKE_ERROR(0x3)

/*! A get request failed because not enough data is available in the reader. */
#define MBEDTLS_ERR_MPS_READER_OUT_OF_DATA           MBEDTLS_MPS_READER_MAKE_ERROR(0x4)

/*!< A get request after pausing and reactivating the reader failed because
 *   the request is not in line with the request made prior to pausing. The user
 *   must not change it's 'strategy' after pausing and reactivating a reader. */
#define MBEDTLS_ERR_MPS_READER_INCONSISTENT_REQUESTS MBEDTLS_MPS_READER_MAKE_ERROR(0x5)

/*! An attempt to reclaim the data buffer from a reader failed because the reader
 *  has no accumulator it can use to backup the data that hasn't been processed. */
#define MBEDTLS_ERR_MPS_READER_NEED_ACCUMULATOR      MBEDTLS_MPS_READER_MAKE_ERROR(0x6)

/*! An attempt to reclaim the data buffer from a reader failed because the
 *  accumulator passed to the reader is not large enough to hold both the
 *  data that hasn't been processed and the excess of the last read-request. */
#define MBEDTLS_ERR_MPS_READER_ACCUMULATOR_TOO_SMALL MBEDTLS_MPS_READER_MAKE_ERROR(0x7)

/* \} name SECTION: MPS Reader error codes */

#endif /* MBEDTLS_MPS_ERROR_H */
