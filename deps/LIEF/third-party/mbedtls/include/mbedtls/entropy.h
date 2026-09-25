/**
 * \file entropy.h
 *
 * \brief Entropy accumulator implementation
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#ifndef MBEDTLS_ENTROPY_H
#define MBEDTLS_ENTROPY_H
#include "mbedtls/private_access.h"

#include "mbedtls/build_info.h"

#include <stddef.h>

#include "md.h"

#if (defined(MBEDTLS_MD_CAN_SHA512) || defined(PSA_WANT_ALG_SHA_512)) && \
    !defined(MBEDTLS_ENTROPY_FORCE_SHA256)
#define MBEDTLS_ENTROPY_SHA512_ACCUMULATOR
#define MBEDTLS_ENTROPY_MD  MBEDTLS_MD_SHA512
#define MBEDTLS_ENTROPY_BLOCK_SIZE      64      /**< Block size of entropy accumulator (SHA-512) */
#else
#if (defined(MBEDTLS_MD_CAN_SHA256) || defined(PSA_WANT_ALG_SHA_256))
#define MBEDTLS_ENTROPY_SHA256_ACCUMULATOR
#define MBEDTLS_ENTROPY_MD  MBEDTLS_MD_SHA256
#define MBEDTLS_ENTROPY_BLOCK_SIZE      32      /**< Block size of entropy accumulator (SHA-256) */
#endif
#endif

#if defined(MBEDTLS_THREADING_C)
#include "mbedtls/threading.h"
#endif


/** Critical entropy source failure. */
#define MBEDTLS_ERR_ENTROPY_SOURCE_FAILED                 -0x003C
/** No more sources can be added. */
#define MBEDTLS_ERR_ENTROPY_MAX_SOURCES                   -0x003E
/** No sources have been added to poll. */
#define MBEDTLS_ERR_ENTROPY_NO_SOURCES_DEFINED            -0x0040
/** No strong sources have been added to poll. */
#define MBEDTLS_ERR_ENTROPY_NO_STRONG_SOURCE              -0x003D
/** Read/write error in file. */
#define MBEDTLS_ERR_ENTROPY_FILE_IO_ERROR                 -0x003F

/**
 * \name SECTION: Module settings
 *
 * The configuration options you can set for this module are in this section.
 * Either change them in mbedtls_config.h or define them on the compiler command line.
 * \{
 */

#if !defined(MBEDTLS_ENTROPY_MAX_SOURCES)
#define MBEDTLS_ENTROPY_MAX_SOURCES     20      /**< Maximum number of sources supported */
#endif

#if !defined(MBEDTLS_ENTROPY_MAX_GATHER)
#define MBEDTLS_ENTROPY_MAX_GATHER      128     /**< Maximum amount requested from entropy sources */
#endif

/** \} name SECTION: Module settings */

#define MBEDTLS_ENTROPY_MAX_SEED_SIZE   1024    /**< Maximum size of seed we read from seed file */
#define MBEDTLS_ENTROPY_SOURCE_MANUAL   MBEDTLS_ENTROPY_MAX_SOURCES

#define MBEDTLS_ENTROPY_SOURCE_STRONG   1       /**< Entropy source is strong   */
#define MBEDTLS_ENTROPY_SOURCE_WEAK     0       /**< Entropy source is weak     */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Entropy poll callback pointer
 *
 * \param data      Callback-specific data pointer
 * \param output    Data to fill
 * \param len       Maximum size to provide
 * \param olen      The actual amount of bytes put into the buffer (Can be 0)
 *
 * \return          0 if no critical failures occurred,
 *                  MBEDTLS_ERR_ENTROPY_SOURCE_FAILED otherwise
 */
typedef int (*mbedtls_entropy_f_source_ptr)(void *data, unsigned char *output, size_t len,
                                            size_t *olen);

/**
 * \brief           Entropy source state
 */
typedef struct mbedtls_entropy_source_state {
    mbedtls_entropy_f_source_ptr    MBEDTLS_PRIVATE(f_source);   /**< The entropy source callback */
    void *MBEDTLS_PRIVATE(p_source);             /**< The callback data pointer */
    size_t          MBEDTLS_PRIVATE(size);       /**< Amount received in bytes */
    size_t          MBEDTLS_PRIVATE(threshold);  /**< Minimum bytes required before release */
    int             MBEDTLS_PRIVATE(strong);     /**< Is the source strong? */
}
mbedtls_entropy_source_state;

/**
 * \brief           Entropy context structure
 */
typedef struct mbedtls_entropy_context {
    mbedtls_md_context_t  MBEDTLS_PRIVATE(accumulator);
    int MBEDTLS_PRIVATE(accumulator_started); /* 0 after init.
                                               * 1 after the first update.
                                               * -1 after free. */
    int             MBEDTLS_PRIVATE(source_count); /* Number of entries used in source. */
    mbedtls_entropy_source_state    MBEDTLS_PRIVATE(source)[MBEDTLS_ENTROPY_MAX_SOURCES];
#if defined(MBEDTLS_THREADING_C)
    mbedtls_threading_mutex_t MBEDTLS_PRIVATE(mutex);    /*!< mutex                  */
#endif
#if defined(MBEDTLS_ENTROPY_NV_SEED)
    int MBEDTLS_PRIVATE(initial_entropy_run);
#endif
}
mbedtls_entropy_context;

#if !defined(MBEDTLS_NO_PLATFORM_ENTROPY)
/**
 * \brief           Platform-specific entropy poll callback
 */
int mbedtls_platform_entropy_poll(void *data,
                                  unsigned char *output, size_t len, size_t *olen);
#endif

/**
 * \brief           Initialize the context
 *
 * \param ctx       Entropy context to initialize
 */
void mbedtls_entropy_init(mbedtls_entropy_context *ctx);

/**
 * \brief           Free the data in the context
 *
 * \param ctx       Entropy context to free
 */
void mbedtls_entropy_free(mbedtls_entropy_context *ctx);

/**
 * \brief           Adds an entropy source to poll
 *                  (Thread-safe if MBEDTLS_THREADING_C is enabled)
 *
 * \param ctx       Entropy context
 * \param f_source  Entropy function
 * \param p_source  Function data
 * \param threshold Minimum required from source before entropy is released
 *                  ( with mbedtls_entropy_func() ) (in bytes)
 * \param strong    MBEDTLS_ENTROPY_SOURCE_STRONG or
 *                  MBEDTLS_ENTROPY_SOURCE_WEAK.
 *                  At least one strong source needs to be added.
 *                  Weaker sources (such as the cycle counter) can be used as
 *                  a complement.
 *
 * \return          0 if successful or MBEDTLS_ERR_ENTROPY_MAX_SOURCES
 */
int mbedtls_entropy_add_source(mbedtls_entropy_context *ctx,
                               mbedtls_entropy_f_source_ptr f_source, void *p_source,
                               size_t threshold, int strong);

/**
 * \brief           Trigger an extra gather poll for the accumulator
 *                  (Thread-safe if MBEDTLS_THREADING_C is enabled)
 *
 * \param ctx       Entropy context
 *
 * \return          0 if successful, or MBEDTLS_ERR_ENTROPY_SOURCE_FAILED
 */
int mbedtls_entropy_gather(mbedtls_entropy_context *ctx);

/**
 * \brief           Retrieve entropy from the accumulator
 *                  (Maximum length: MBEDTLS_ENTROPY_BLOCK_SIZE)
 *                  (Thread-safe if MBEDTLS_THREADING_C is enabled)
 *
 * \param data      Entropy context
 * \param output    Buffer to fill
 * \param len       Number of bytes desired, must be at most MBEDTLS_ENTROPY_BLOCK_SIZE
 *
 * \return          0 if successful, or MBEDTLS_ERR_ENTROPY_SOURCE_FAILED
 */
int mbedtls_entropy_func(void *data, unsigned char *output, size_t len);

/**
 * \brief           Add data to the accumulator manually
 *                  (Thread-safe if MBEDTLS_THREADING_C is enabled)
 *
 * \param ctx       Entropy context
 * \param data      Data to add
 * \param len       Length of data
 *
 * \return          0 if successful
 */
int mbedtls_entropy_update_manual(mbedtls_entropy_context *ctx,
                                  const unsigned char *data, size_t len);

#if defined(MBEDTLS_ENTROPY_NV_SEED)
/**
 * \brief           Trigger an update of the seed file in NV by using the
 *                  current entropy pool.
 *
 * \param ctx       Entropy context
 *
 * \return          0 if successful
 */
int mbedtls_entropy_update_nv_seed(mbedtls_entropy_context *ctx);
#endif /* MBEDTLS_ENTROPY_NV_SEED */

#if defined(MBEDTLS_FS_IO)
/**
 * \brief               Write a seed file
 *
 * \param ctx           Entropy context
 * \param path          Name of the file
 *
 * \return              0 if successful,
 *                      MBEDTLS_ERR_ENTROPY_FILE_IO_ERROR on file error, or
 *                      MBEDTLS_ERR_ENTROPY_SOURCE_FAILED
 */
int mbedtls_entropy_write_seed_file(mbedtls_entropy_context *ctx, const char *path);

/**
 * \brief               Read and update a seed file. Seed is added to this
 *                      instance. No more than MBEDTLS_ENTROPY_MAX_SEED_SIZE bytes are
 *                      read from the seed file. The rest is ignored.
 *
 * \param ctx           Entropy context
 * \param path          Name of the file
 *
 * \return              0 if successful,
 *                      MBEDTLS_ERR_ENTROPY_FILE_IO_ERROR on file error,
 *                      MBEDTLS_ERR_ENTROPY_SOURCE_FAILED
 */
int mbedtls_entropy_update_seed_file(mbedtls_entropy_context *ctx, const char *path);
#endif /* MBEDTLS_FS_IO */

#if defined(MBEDTLS_SELF_TEST)
/**
 * \brief          Checkup routine
 *
 *                 This module self-test also calls the entropy self-test,
 *                 mbedtls_entropy_source_self_test();
 *
 * \return         0 if successful, or 1 if a test failed
 */
int mbedtls_entropy_self_test(int verbose);

#if defined(MBEDTLS_ENTROPY_HARDWARE_ALT)
/**
 * \brief          Checkup routine
 *
 *                 Verifies the integrity of the hardware entropy source
 *                 provided by the function 'mbedtls_hardware_poll()'.
 *
 *                 Note this is the only hardware entropy source that is known
 *                 at link time, and other entropy sources configured
 *                 dynamically at runtime by the function
 *                 mbedtls_entropy_add_source() will not be tested.
 *
 * \return         0 if successful, or 1 if a test failed
 */
int mbedtls_entropy_source_self_test(int verbose);
#endif /* MBEDTLS_ENTROPY_HARDWARE_ALT */
#endif /* MBEDTLS_SELF_TEST */

#ifdef __cplusplus
}
#endif

#endif /* entropy.h */
