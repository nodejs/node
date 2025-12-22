/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

/**
 * \file mps_common.h
 *
 * \brief Common functions and macros used by MPS
 */

#ifndef MBEDTLS_MPS_COMMON_H
#define MBEDTLS_MPS_COMMON_H

#include "mps_error.h"

#include <stdio.h>

/**
 * \name SECTION:       MPS Configuration
 *
 * \{
 */

/*! This flag controls whether the MPS-internal components
 *  (reader, writer, Layer 1-3) perform validation of the
 *  expected abstract state at the entry of API calls.
 *
 *  Context: All MPS API functions impose assumptions/preconditions on the
 *  context on which they operate. For example, every structure has a notion of
 *  state integrity which is established by `xxx_init()` and preserved by any
 *  calls to the MPS API which satisfy their preconditions and either succeed,
 *  or fail with an error code which is explicitly documented to not corrupt
 *  structure integrity (such as WANT_READ and WANT_WRITE);
 *  apart from `xxx_init()` any function assumes state integrity as a
 *  precondition (but usually more). If any of the preconditions is violated,
 *  the function's behavior is entirely undefined.
 *  In addition to state integrity, all MPS structures have a more refined
 *  notion of abstract state that the API operates on. For example, all layers
 *  have a notion of 'abstract read state' which indicates if incoming data has
 *  been passed to the user, e.g. through mps_l2_read_start() for Layer 2
 *  or mps_l3_read() in Layer 3. After such a call, it doesn't make sense to
 *  call these reading functions again until the incoming data has been
 *  explicitly 'consumed', e.g. through mps_l2_read_consume() for Layer 2 or
 *  mps_l3_read_consume() on Layer 3. However, even if it doesn't make sense,
 *  it's a design choice whether the API should fail gracefully on such
 *  non-sensical calls or not, and that's what this option is about:
 *
 *  This option determines whether the expected abstract state
 *  is part of the API preconditions or not: If the option is set,
 *  then the abstract state is not part of the precondition and is
 *  thus required to be validated by the implementation. If an unexpected
 *  abstract state is encountered, the implementation must fail gracefully
 *  with error #MBEDTLS_ERR_MPS_OPERATION_UNEXPECTED.
 *  Conversely, if this option is not set, then the expected abstract state
 *  is included in the preconditions of the respective API calls, and
 *  an implementation's behaviour is undefined if the abstract state is
 *  not as expected.
 *
 *  For example: Enabling this makes mps_l2_read_done() fail if
 *  no incoming record is currently open; disabling this would
 *  lead to undefined behavior in this case.
 *
 *  Comment this to remove state validation.
 */
#define MBEDTLS_MPS_STATE_VALIDATION

/*! This flag enables/disables assertions on the internal state of MPS.
 *
 *  Assertions are sanity checks that should never trigger when MPS
 *  is used within the bounds of its API and preconditions.
 *
 *  Enabling this increases security by limiting the scope of
 *  potential bugs, but comes at the cost of increased code size.
 *
 *  Note: So far, there is no guiding principle as to what
 *  expected conditions merit an assertion, and which don't.
 *
 *  Comment this to disable assertions.
 */
#define MBEDTLS_MPS_ENABLE_ASSERTIONS

/*! This flag controls whether tracing for MPS should be enabled. */
//#define MBEDTLS_MPS_ENABLE_TRACE

#if defined(MBEDTLS_MPS_STATE_VALIDATION)

#define MBEDTLS_MPS_STATE_VALIDATE_RAW(cond, string)                         \
    do                                                                         \
    {                                                                          \
        if (!(cond))                                                          \
        {                                                                      \
            MBEDTLS_MPS_TRACE(MBEDTLS_MPS_TRACE_TYPE_ERROR, string);         \
            MBEDTLS_MPS_TRACE_RETURN(MBEDTLS_ERR_MPS_OPERATION_UNEXPECTED);  \
        }                                                                      \
    } while (0)

#else /* MBEDTLS_MPS_STATE_VALIDATION */

#define MBEDTLS_MPS_STATE_VALIDATE_RAW(cond, string)           \
    do                                                           \
    {                                                            \
        (cond);                                                \
    } while (0)

#endif /* MBEDTLS_MPS_STATE_VALIDATION */

#if defined(MBEDTLS_MPS_ENABLE_ASSERTIONS)

#define MBEDTLS_MPS_ASSERT_RAW(cond, string)                          \
    do                                                                  \
    {                                                                   \
        if (!(cond))                                                   \
        {                                                               \
            MBEDTLS_MPS_TRACE(MBEDTLS_MPS_TRACE_TYPE_ERROR, string);  \
            MBEDTLS_MPS_TRACE_RETURN(MBEDTLS_ERR_MPS_INTERNAL_ERROR); \
        }                                                               \
    } while (0)

#else /* MBEDTLS_MPS_ENABLE_ASSERTIONS */

#define MBEDTLS_MPS_ASSERT_RAW(cond, string) do {} while (0)

#endif /* MBEDTLS_MPS_ENABLE_ASSERTIONS */


/* \} name SECTION: MPS Configuration */

/**
 * \name SECTION:       Common types
 *
 * Various common types used throughout MPS.
 * \{
 */

/** \brief   The type of buffer sizes and offsets used in MPS structures.
 *
 *           This is an unsigned integer type that should be large enough to
 *           hold the length of any buffer or message processed by MPS.
 *
 *           The reason to pick a value as small as possible here is
 *           to reduce the size of MPS structures.
 *
 * \warning  Care has to be taken when using a narrower type
 *           than ::mbedtls_mps_size_t here because of
 *           potential truncation during conversion.
 *
 * \warning  Handshake messages in TLS may be up to 2^24 ~ 16Mb in size.
 *           If mbedtls_mps_[opt_]stored_size_t is smaller than that, the
 *           maximum handshake message is restricted accordingly.
 *
 * For now, we use the default type of size_t throughout, and the use of
 * smaller types or different types for ::mbedtls_mps_size_t and
 * ::mbedtls_mps_stored_size_t is not yet supported.
 *
 */
typedef size_t mbedtls_mps_stored_size_t;
#define MBEDTLS_MPS_STORED_SIZE_MAX  (SIZE_MAX)

/** \brief The type of buffer sizes and offsets used in the MPS API
 *         and implementation.
 *
 *         This must be at least as wide as ::mbedtls_stored_size_t but
 *         may be chosen to be strictly larger if more suitable for the
 *         target architecture.
 *
 *         For example, in a test build for ARM Thumb, using uint_fast16_t
 *         instead of uint16_t reduced the code size from 1060 Byte to 962 Byte,
 *         so almost 10%.
 */
typedef size_t mbedtls_mps_size_t;
#define MBEDTLS_MPS_SIZE_MAX  (SIZE_MAX)

#if MBEDTLS_MPS_STORED_SIZE_MAX > MBEDTLS_MPS_SIZE_MAX
#error "Misconfiguration of mbedtls_mps_size_t and mbedtls_mps_stored_size_t."
#endif

/* \} SECTION: Common types */


#endif /* MBEDTLS_MPS_COMMON_H */
