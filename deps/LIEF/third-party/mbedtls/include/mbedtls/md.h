/**
 * \file md.h
 *
 * \brief   This file contains the generic functions for message-digest
 *          (hashing) and HMAC.
 *
 * \author Adriaan de Jong <dejong@fox-it.com>
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_MD_H
#define MBEDTLS_MD_H
#include "mbedtls/private_access.h"

#include <stddef.h>

#include "mbedtls/build_info.h"
#include "mbedtls/platform_util.h"

/** The selected feature is not available. */
#define MBEDTLS_ERR_MD_FEATURE_UNAVAILABLE                -0x5080
/** Bad input parameters to function. */
#define MBEDTLS_ERR_MD_BAD_INPUT_DATA                     -0x5100
/** Failed to allocate memory. */
#define MBEDTLS_ERR_MD_ALLOC_FAILED                       -0x5180
/** Opening or reading of file failed. */
#define MBEDTLS_ERR_MD_FILE_IO_ERROR                      -0x5200

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief     Supported message digests.
 *
 * \warning   MD5 and SHA-1 are considered weak message digests and
 *            their use constitutes a security risk. We recommend considering
 *            stronger message digests instead.
 *
 */
/* Note: these are aligned with the definitions of PSA_ALG_ macros for hashes,
 * in order to enable an efficient implementation of conversion functions.
 * This is tested by md_to_from_psa() in test_suite_md. */
typedef enum {
    MBEDTLS_MD_NONE=0,    /**< None. */
    MBEDTLS_MD_MD5=0x03,       /**< The MD5 message digest. */
    MBEDTLS_MD_RIPEMD160=0x04, /**< The RIPEMD-160 message digest. */
    MBEDTLS_MD_SHA1=0x05,      /**< The SHA-1 message digest. */
    MBEDTLS_MD_SHA224=0x08,    /**< The SHA-224 message digest. */
    MBEDTLS_MD_SHA256=0x09,    /**< The SHA-256 message digest. */
    MBEDTLS_MD_SHA384=0x0a,    /**< The SHA-384 message digest. */
    MBEDTLS_MD_SHA512=0x0b,    /**< The SHA-512 message digest. */
    MBEDTLS_MD_SHA3_224=0x10,  /**< The SHA3-224 message digest. */
    MBEDTLS_MD_SHA3_256=0x11,  /**< The SHA3-256 message digest. */
    MBEDTLS_MD_SHA3_384=0x12,  /**< The SHA3-384 message digest. */
    MBEDTLS_MD_SHA3_512=0x13,  /**< The SHA3-512 message digest. */
} mbedtls_md_type_t;

/* Note: this should always be >= PSA_HASH_MAX_SIZE
 * in all builds with both CRYPTO_C and MD_LIGHT.
 *
 * This is to make things easier for modules such as TLS that may define a
 * buffer size using MD_MAX_SIZE in a part of the code that's common to PSA
 * and legacy, then assume the buffer's size is PSA_HASH_MAX_SIZE in another
 * part of the code based on PSA.
 */
#if defined(MBEDTLS_MD_CAN_SHA512) || defined(MBEDTLS_MD_CAN_SHA3_512)
#define MBEDTLS_MD_MAX_SIZE         64  /* longest known is SHA512 */
#elif defined(MBEDTLS_MD_CAN_SHA384) || defined(MBEDTLS_MD_CAN_SHA3_384)
#define MBEDTLS_MD_MAX_SIZE         48  /* longest known is SHA384 */
#elif defined(MBEDTLS_MD_CAN_SHA256) || defined(MBEDTLS_MD_CAN_SHA3_256)
#define MBEDTLS_MD_MAX_SIZE         32  /* longest known is SHA256 */
#elif defined(MBEDTLS_MD_CAN_SHA224) || defined(MBEDTLS_MD_CAN_SHA3_224)
#define MBEDTLS_MD_MAX_SIZE         28  /* longest known is SHA224 */
#else
#define MBEDTLS_MD_MAX_SIZE         20  /* longest known is SHA1 or RIPE MD-160
                                           or smaller (MD5 and earlier) */
#endif

#if defined(MBEDTLS_MD_CAN_SHA3_224)
#define MBEDTLS_MD_MAX_BLOCK_SIZE         144 /* the longest known is SHA3-224 */
#elif defined(MBEDTLS_MD_CAN_SHA3_256)
#define MBEDTLS_MD_MAX_BLOCK_SIZE         136
#elif defined(MBEDTLS_MD_CAN_SHA512) || defined(MBEDTLS_MD_CAN_SHA384)
#define MBEDTLS_MD_MAX_BLOCK_SIZE         128
#elif defined(MBEDTLS_MD_CAN_SHA3_384)
#define MBEDTLS_MD_MAX_BLOCK_SIZE         104
#elif defined(MBEDTLS_MD_CAN_SHA3_512)
#define MBEDTLS_MD_MAX_BLOCK_SIZE         72
#else
#define MBEDTLS_MD_MAX_BLOCK_SIZE         64
#endif

/**
 * Opaque struct.
 *
 * Constructed using either #mbedtls_md_info_from_string or
 * #mbedtls_md_info_from_type.
 *
 * Fields can be accessed with #mbedtls_md_get_size,
 * #mbedtls_md_get_type and #mbedtls_md_get_name.
 */
/* Defined internally in library/md_wrap.h. */
typedef struct mbedtls_md_info_t mbedtls_md_info_t;

/**
 * Used internally to indicate whether a context uses legacy or PSA.
 *
 * Internal use only.
 */
typedef enum {
    MBEDTLS_MD_ENGINE_LEGACY = 0,
    MBEDTLS_MD_ENGINE_PSA,
} mbedtls_md_engine_t;

/**
 * The generic message-digest context.
 */
typedef struct mbedtls_md_context_t {
    /** Information about the associated message digest. */
    const mbedtls_md_info_t *MBEDTLS_PRIVATE(md_info);

#if defined(MBEDTLS_MD_SOME_PSA)
    /** Are hash operations dispatched to PSA or legacy? */
    mbedtls_md_engine_t MBEDTLS_PRIVATE(engine);
#endif

    /** The digest-specific context (legacy) or the PSA operation. */
    void *MBEDTLS_PRIVATE(md_ctx);

#if defined(MBEDTLS_MD_C)
    /** The HMAC part of the context. */
    void *MBEDTLS_PRIVATE(hmac_ctx);
#endif
} mbedtls_md_context_t;

/**
 * \brief           This function returns the message-digest information
 *                  associated with the given digest type.
 *
 * \param md_type   The type of digest to search for.
 *
 * \return          The message-digest information associated with \p md_type.
 * \return          NULL if the associated message-digest information is not found.
 */
const mbedtls_md_info_t *mbedtls_md_info_from_type(mbedtls_md_type_t md_type);

/**
 * \brief           This function initializes a message-digest context without
 *                  binding it to a particular message-digest algorithm.
 *
 *                  This function should always be called first. It prepares the
 *                  context for mbedtls_md_setup() for binding it to a
 *                  message-digest algorithm.
 */
void mbedtls_md_init(mbedtls_md_context_t *ctx);

/**
 * \brief           This function clears the internal structure of \p ctx and
 *                  frees any embedded internal structure, but does not free
 *                  \p ctx itself.
 *
 *                  If you have called mbedtls_md_setup() on \p ctx, you must
 *                  call mbedtls_md_free() when you are no longer using the
 *                  context.
 *                  Calling this function if you have previously
 *                  called mbedtls_md_init() and nothing else is optional.
 *                  You must not call this function if you have not called
 *                  mbedtls_md_init().
 */
void mbedtls_md_free(mbedtls_md_context_t *ctx);


/**
 * \brief           This function selects the message digest algorithm to use,
 *                  and allocates internal structures.
 *
 *                  It should be called after mbedtls_md_init() or
 *                  mbedtls_md_free(). Makes it necessary to call
 *                  mbedtls_md_free() later.
 *
 * \param ctx       The context to set up.
 * \param md_info   The information structure of the message-digest algorithm
 *                  to use.
 * \param hmac      Defines if HMAC is used. 0: HMAC is not used (saves some memory),
 *                  or non-zero: HMAC is used with this context.
 *
 * \return          \c 0 on success.
 * \return          #MBEDTLS_ERR_MD_BAD_INPUT_DATA on parameter-verification
 *                  failure.
 * \return          #MBEDTLS_ERR_MD_ALLOC_FAILED on memory-allocation failure.
 */
MBEDTLS_CHECK_RETURN_TYPICAL
int mbedtls_md_setup(mbedtls_md_context_t *ctx, const mbedtls_md_info_t *md_info, int hmac);

/**
 * \brief           This function clones the state of a message-digest
 *                  context.
 *
 * \note            You must call mbedtls_md_setup() on \c dst before calling
 *                  this function.
 *
 * \note            The two contexts must have the same type,
 *                  for example, both are SHA-256.
 *
 * \warning         This function clones the message-digest state, not the
 *                  HMAC state.
 *
 * \param dst       The destination context.
 * \param src       The context to be cloned.
 *
 * \return          \c 0 on success.
 * \return          #MBEDTLS_ERR_MD_BAD_INPUT_DATA on parameter-verification failure.
 * \return          #MBEDTLS_ERR_MD_FEATURE_UNAVAILABLE if both contexts are
 *                  not using the same engine. This can be avoided by moving
 *                  the call to psa_crypto_init() before the first call to
 *                  mbedtls_md_setup().
 */
MBEDTLS_CHECK_RETURN_TYPICAL
int mbedtls_md_clone(mbedtls_md_context_t *dst,
                     const mbedtls_md_context_t *src);

/**
 * \brief           This function extracts the message-digest size from the
 *                  message-digest information structure.
 *
 * \param md_info   The information structure of the message-digest algorithm
 *                  to use.
 *
 * \return          The size of the message-digest output in Bytes.
 */
unsigned char mbedtls_md_get_size(const mbedtls_md_info_t *md_info);

/**
 * \brief           This function gives the message-digest size associated to
 *                  message-digest type.
 *
 * \param md_type   The message-digest type.
 *
 * \return          The size of the message-digest output in Bytes,
 *                  or 0 if the message-digest type is not known.
 */
static inline unsigned char mbedtls_md_get_size_from_type(mbedtls_md_type_t md_type)
{
    return mbedtls_md_get_size(mbedtls_md_info_from_type(md_type));
}

/**
 * \brief           This function extracts the message-digest type from the
 *                  message-digest information structure.
 *
 * \param md_info   The information structure of the message-digest algorithm
 *                  to use.
 *
 * \return          The type of the message digest.
 */
mbedtls_md_type_t mbedtls_md_get_type(const mbedtls_md_info_t *md_info);

/**
 * \brief           This function starts a message-digest computation.
 *
 *                  You must call this function after setting up the context
 *                  with mbedtls_md_setup(), and before passing data with
 *                  mbedtls_md_update().
 *
 * \param ctx       The generic message-digest context.
 *
 * \return          \c 0 on success.
 * \return          #MBEDTLS_ERR_MD_BAD_INPUT_DATA on parameter-verification
 *                  failure.
 */
MBEDTLS_CHECK_RETURN_TYPICAL
int mbedtls_md_starts(mbedtls_md_context_t *ctx);

/**
 * \brief           This function feeds an input buffer into an ongoing
 *                  message-digest computation.
 *
 *                  You must call mbedtls_md_starts() before calling this
 *                  function. You may call this function multiple times.
 *                  Afterwards, call mbedtls_md_finish().
 *
 * \param ctx       The generic message-digest context.
 * \param input     The buffer holding the input data.
 * \param ilen      The length of the input data.
 *
 * \return          \c 0 on success.
 * \return          #MBEDTLS_ERR_MD_BAD_INPUT_DATA on parameter-verification
 *                  failure.
 */
MBEDTLS_CHECK_RETURN_TYPICAL
int mbedtls_md_update(mbedtls_md_context_t *ctx, const unsigned char *input, size_t ilen);

/**
 * \brief           This function finishes the digest operation,
 *                  and writes the result to the output buffer.
 *
 *                  Call this function after a call to mbedtls_md_starts(),
 *                  followed by any number of calls to mbedtls_md_update().
 *                  Afterwards, you may either clear the context with
 *                  mbedtls_md_free(), or call mbedtls_md_starts() to reuse
 *                  the context for another digest operation with the same
 *                  algorithm.
 *
 * \param ctx       The generic message-digest context.
 * \param output    The buffer for the generic message-digest checksum result.
 *
 * \return          \c 0 on success.
 * \return          #MBEDTLS_ERR_MD_BAD_INPUT_DATA on parameter-verification
 *                  failure.
 */
MBEDTLS_CHECK_RETURN_TYPICAL
int mbedtls_md_finish(mbedtls_md_context_t *ctx, unsigned char *output);

/**
 * \brief          This function calculates the message-digest of a buffer,
 *                 with respect to a configurable message-digest algorithm
 *                 in a single call.
 *
 *                 The result is calculated as
 *                 Output = message_digest(input buffer).
 *
 * \param md_info  The information structure of the message-digest algorithm
 *                 to use.
 * \param input    The buffer holding the data.
 * \param ilen     The length of the input data.
 * \param output   The generic message-digest checksum result.
 *
 * \return         \c 0 on success.
 * \return         #MBEDTLS_ERR_MD_BAD_INPUT_DATA on parameter-verification
 *                 failure.
 */
MBEDTLS_CHECK_RETURN_TYPICAL
int mbedtls_md(const mbedtls_md_info_t *md_info, const unsigned char *input, size_t ilen,
               unsigned char *output);

/**
 * \brief           This function returns the list of digests supported by the
 *                  generic digest module.
 *
 * \note            The list starts with the strongest available hashes.
 *
 * \return          A statically allocated array of digests. Each element
 *                  in the returned list is an integer belonging to the
 *                  message-digest enumeration #mbedtls_md_type_t.
 *                  The last entry is 0.
 */
const int *mbedtls_md_list(void);

/**
 * \brief           This function returns the message-digest information
 *                  associated with the given digest name.
 *
 * \param md_name   The name of the digest to search for.
 *
 * \return          The message-digest information associated with \p md_name.
 * \return          NULL if the associated message-digest information is not found.
 */
const mbedtls_md_info_t *mbedtls_md_info_from_string(const char *md_name);

/**
 * \brief           This function returns the name of the message digest for
 *                  the message-digest information structure given.
 *
 * \param md_info   The information structure of the message-digest algorithm
 *                  to use.
 *
 * \return          The name of the message digest.
 */
const char *mbedtls_md_get_name(const mbedtls_md_info_t *md_info);

/**
 * \brief           This function returns the message-digest information
 *                  from the given context.
 *
 * \param ctx       The context from which to extract the information.
 *                  This must be initialized (or \c NULL).
 *
 * \return          The message-digest information associated with \p ctx.
 * \return          \c NULL if \p ctx is \c NULL.
 */
const mbedtls_md_info_t *mbedtls_md_info_from_ctx(
    const mbedtls_md_context_t *ctx);

#if defined(MBEDTLS_FS_IO)
/**
 * \brief          This function calculates the message-digest checksum
 *                 result of the contents of the provided file.
 *
 *                 The result is calculated as
 *                 Output = message_digest(file contents).
 *
 * \param md_info  The information structure of the message-digest algorithm
 *                 to use.
 * \param path     The input file name.
 * \param output   The generic message-digest checksum result.
 *
 * \return         \c 0 on success.
 * \return         #MBEDTLS_ERR_MD_FILE_IO_ERROR on an I/O error accessing
 *                 the file pointed by \p path.
 * \return         #MBEDTLS_ERR_MD_BAD_INPUT_DATA if \p md_info was NULL.
 */
MBEDTLS_CHECK_RETURN_TYPICAL
int mbedtls_md_file(const mbedtls_md_info_t *md_info, const char *path,
                    unsigned char *output);
#endif /* MBEDTLS_FS_IO */

/**
 * \brief           This function sets the HMAC key and prepares to
 *                  authenticate a new message.
 *
 *                  Call this function after mbedtls_md_setup(), to use
 *                  the MD context for an HMAC calculation, then call
 *                  mbedtls_md_hmac_update() to provide the input data, and
 *                  mbedtls_md_hmac_finish() to get the HMAC value.
 *
 * \param ctx       The message digest context containing an embedded HMAC
 *                  context.
 * \param key       The HMAC secret key.
 * \param keylen    The length of the HMAC key in Bytes.
 *
 * \return          \c 0 on success.
 * \return          #MBEDTLS_ERR_MD_BAD_INPUT_DATA on parameter-verification
 *                  failure.
 */
MBEDTLS_CHECK_RETURN_TYPICAL
int mbedtls_md_hmac_starts(mbedtls_md_context_t *ctx, const unsigned char *key,
                           size_t keylen);

/**
 * \brief           This function feeds an input buffer into an ongoing HMAC
 *                  computation.
 *
 *                  Call mbedtls_md_hmac_starts() or mbedtls_md_hmac_reset()
 *                  before calling this function.
 *                  You may call this function multiple times to pass the
 *                  input piecewise.
 *                  Afterwards, call mbedtls_md_hmac_finish().
 *
 * \param ctx       The message digest context containing an embedded HMAC
 *                  context.
 * \param input     The buffer holding the input data.
 * \param ilen      The length of the input data.
 *
 * \return          \c 0 on success.
 * \return          #MBEDTLS_ERR_MD_BAD_INPUT_DATA on parameter-verification
 *                  failure.
 */
MBEDTLS_CHECK_RETURN_TYPICAL
int mbedtls_md_hmac_update(mbedtls_md_context_t *ctx, const unsigned char *input,
                           size_t ilen);

/**
 * \brief           This function finishes the HMAC operation, and writes
 *                  the result to the output buffer.
 *
 *                  Call this function after mbedtls_md_hmac_starts() and
 *                  mbedtls_md_hmac_update() to get the HMAC value. Afterwards
 *                  you may either call mbedtls_md_free() to clear the context,
 *                  or call mbedtls_md_hmac_reset() to reuse the context with
 *                  the same HMAC key.
 *
 * \param ctx       The message digest context containing an embedded HMAC
 *                  context.
 * \param output    The generic HMAC checksum result.
 *
 * \return          \c 0 on success.
 * \return          #MBEDTLS_ERR_MD_BAD_INPUT_DATA on parameter-verification
 *                  failure.
 */
MBEDTLS_CHECK_RETURN_TYPICAL
int mbedtls_md_hmac_finish(mbedtls_md_context_t *ctx, unsigned char *output);

/**
 * \brief           This function prepares to authenticate a new message with
 *                  the same key as the previous HMAC operation.
 *
 *                  You may call this function after mbedtls_md_hmac_finish().
 *                  Afterwards call mbedtls_md_hmac_update() to pass the new
 *                  input.
 *
 * \param ctx       The message digest context containing an embedded HMAC
 *                  context.
 *
 * \return          \c 0 on success.
 * \return          #MBEDTLS_ERR_MD_BAD_INPUT_DATA on parameter-verification
 *                  failure.
 */
MBEDTLS_CHECK_RETURN_TYPICAL
int mbedtls_md_hmac_reset(mbedtls_md_context_t *ctx);

/**
 * \brief          This function calculates the full generic HMAC
 *                 on the input buffer with the provided key.
 *
 *                 The function allocates the context, performs the
 *                 calculation, and frees the context.
 *
 *                 The HMAC result is calculated as
 *                 output = generic HMAC(hmac key, input buffer).
 *
 * \param md_info  The information structure of the message-digest algorithm
 *                 to use.
 * \param key      The HMAC secret key.
 * \param keylen   The length of the HMAC secret key in Bytes.
 * \param input    The buffer holding the input data.
 * \param ilen     The length of the input data.
 * \param output   The generic HMAC result.
 *
 * \return         \c 0 on success.
 * \return         #MBEDTLS_ERR_MD_BAD_INPUT_DATA on parameter-verification
 *                 failure.
 */
MBEDTLS_CHECK_RETURN_TYPICAL
int mbedtls_md_hmac(const mbedtls_md_info_t *md_info, const unsigned char *key, size_t keylen,
                    const unsigned char *input, size_t ilen,
                    unsigned char *output);

#ifdef __cplusplus
}
#endif

#endif /* MBEDTLS_MD_H */
