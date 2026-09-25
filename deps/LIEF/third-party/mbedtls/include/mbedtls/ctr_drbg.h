/**
 * \file ctr_drbg.h
 *
 * \brief    This file contains definitions and functions for the
 *           CTR_DRBG pseudorandom generator.
 *
 * CTR_DRBG is a standardized way of building a PRNG from a block-cipher
 * in counter mode operation, as defined in <em>NIST SP 800-90A:
 * Recommendation for Random Number Generation Using Deterministic Random
 * Bit Generators</em>.
 *
 * The Mbed TLS implementation of CTR_DRBG uses AES-256 (default) or AES-128
 * (if \c MBEDTLS_CTR_DRBG_USE_128_BIT_KEY is enabled at compile time)
 * as the underlying block cipher, with a derivation function.
 *
 * The security strength as defined in NIST SP 800-90A is
 * 128 bits when AES-128 is used (\c MBEDTLS_CTR_DRBG_USE_128_BIT_KEY enabled)
 * and 256 bits otherwise, provided that #MBEDTLS_CTR_DRBG_ENTROPY_LEN is
 * kept at its default value (and not overridden in mbedtls_config.h) and that the
 * DRBG instance is set up with default parameters.
 * See the documentation of mbedtls_ctr_drbg_seed() for more
 * information.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_CTR_DRBG_H
#define MBEDTLS_CTR_DRBG_H
#include "mbedtls/private_access.h"

#include "mbedtls/build_info.h"

/* The CTR_DRBG implementation can either directly call the low-level AES
 * module (gated by MBEDTLS_AES_C) or call the PSA API to perform AES
 * operations. Calling the AES module directly is the default, both for
 * maximum backward compatibility and because it's a bit more efficient
 * (less glue code).
 *
 * When MBEDTLS_AES_C is disabled, the CTR_DRBG module calls PSA crypto and
 * thus benefits from the PSA AES accelerator driver.
 * It is technically possible to enable MBEDTLS_CTR_DRBG_USE_PSA_CRYPTO
 * to use PSA even when MBEDTLS_AES_C is enabled, but there is very little
 * reason to do so other than testing purposes and this is not officially
 * supported.
 */
#if !defined(MBEDTLS_AES_C)
#define MBEDTLS_CTR_DRBG_USE_PSA_CRYPTO
#endif

#if defined(MBEDTLS_CTR_DRBG_USE_PSA_CRYPTO)
#include "psa/crypto.h"
#else
#include "mbedtls/aes.h"
#endif

#include "entropy.h"

#if defined(MBEDTLS_THREADING_C)
#include "mbedtls/threading.h"
#endif

/** The entropy source failed. */
#define MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED        -0x0034
/** The requested random buffer length is too big. */
#define MBEDTLS_ERR_CTR_DRBG_REQUEST_TOO_BIG              -0x0036
/** The input (entropy + additional data) is too large. */
#define MBEDTLS_ERR_CTR_DRBG_INPUT_TOO_BIG                -0x0038
/** Read or write error in file. */
#define MBEDTLS_ERR_CTR_DRBG_FILE_IO_ERROR                -0x003A

#define MBEDTLS_CTR_DRBG_BLOCKSIZE          16 /**< The block size used by the cipher. */

#if defined(MBEDTLS_CTR_DRBG_USE_128_BIT_KEY)
#define MBEDTLS_CTR_DRBG_KEYSIZE            16
/**< The key size in bytes used by the cipher.
 *
 * Compile-time choice: 16 bytes (128 bits)
 * because #MBEDTLS_CTR_DRBG_USE_128_BIT_KEY is enabled.
 */
#else
#define MBEDTLS_CTR_DRBG_KEYSIZE            32
/**< The key size in bytes used by the cipher.
 *
 * Compile-time choice: 32 bytes (256 bits)
 * because \c MBEDTLS_CTR_DRBG_USE_128_BIT_KEY is disabled.
 */
#endif

#define MBEDTLS_CTR_DRBG_KEYBITS            (MBEDTLS_CTR_DRBG_KEYSIZE * 8)   /**< The key size for the DRBG operation, in bits. */
#define MBEDTLS_CTR_DRBG_SEEDLEN            (MBEDTLS_CTR_DRBG_KEYSIZE + MBEDTLS_CTR_DRBG_BLOCKSIZE)   /**< The seed length, calculated as (counter + AES key). */

/**
 * \name SECTION: Module settings
 *
 * The configuration options you can set for this module are in this section.
 * Either change them in mbedtls_config.h or define them using the compiler command
 * line.
 * \{
 */

/** \def MBEDTLS_CTR_DRBG_ENTROPY_LEN
 *
 * \brief The amount of entropy used per seed by default, in bytes.
 */
#if !defined(MBEDTLS_CTR_DRBG_ENTROPY_LEN)
#if defined(MBEDTLS_ENTROPY_SHA512_ACCUMULATOR)
/** This is 48 bytes because the entropy module uses SHA-512.
 */
#define MBEDTLS_CTR_DRBG_ENTROPY_LEN        48

#else /* MBEDTLS_ENTROPY_SHA512_ACCUMULATOR */

/** This is 32 bytes because the entropy module uses SHA-256.
 */
#if !defined(MBEDTLS_CTR_DRBG_USE_128_BIT_KEY)
/** \warning To achieve a 256-bit security strength, you must pass a nonce
 *           to mbedtls_ctr_drbg_seed().
 */
#endif /* !defined(MBEDTLS_CTR_DRBG_USE_128_BIT_KEY) */
#define MBEDTLS_CTR_DRBG_ENTROPY_LEN        32
#endif /* MBEDTLS_ENTROPY_SHA512_ACCUMULATOR */
#endif /* !defined(MBEDTLS_CTR_DRBG_ENTROPY_LEN) */

#if !defined(MBEDTLS_CTR_DRBG_RESEED_INTERVAL)
#define MBEDTLS_CTR_DRBG_RESEED_INTERVAL    10000
/**< The interval before reseed is performed by default. */
#endif

#if !defined(MBEDTLS_CTR_DRBG_MAX_INPUT)
#define MBEDTLS_CTR_DRBG_MAX_INPUT          256
/**< The maximum number of additional input Bytes. */
#endif

#if !defined(MBEDTLS_CTR_DRBG_MAX_REQUEST)
#define MBEDTLS_CTR_DRBG_MAX_REQUEST        1024
/**< The maximum number of requested Bytes per call. */
#endif

#if !defined(MBEDTLS_CTR_DRBG_MAX_SEED_INPUT)
#define MBEDTLS_CTR_DRBG_MAX_SEED_INPUT     384
/**< The maximum size of seed or reseed buffer in bytes. */
#endif

/** \} name SECTION: Module settings */

#define MBEDTLS_CTR_DRBG_PR_OFF             0
/**< Prediction resistance is disabled. */
#define MBEDTLS_CTR_DRBG_PR_ON              1
/**< Prediction resistance is enabled. */

#ifdef __cplusplus
extern "C" {
#endif

#if MBEDTLS_CTR_DRBG_ENTROPY_LEN >= MBEDTLS_CTR_DRBG_KEYSIZE * 3 / 2
/** The default length of the nonce read from the entropy source.
 *
 * This is \c 0 because a single read from the entropy source is sufficient
 * to include a nonce.
 * See the documentation of mbedtls_ctr_drbg_seed() for more information.
 */
#define MBEDTLS_CTR_DRBG_ENTROPY_NONCE_LEN 0
#else
/** The default length of the nonce read from the entropy source.
 *
 * This is half of the default entropy length because a single read from
 * the entropy source does not provide enough material to form a nonce.
 * See the documentation of mbedtls_ctr_drbg_seed() for more information.
 */
#define MBEDTLS_CTR_DRBG_ENTROPY_NONCE_LEN (MBEDTLS_CTR_DRBG_ENTROPY_LEN + 1) / 2
#endif

#if defined(MBEDTLS_CTR_DRBG_USE_PSA_CRYPTO)
typedef struct mbedtls_ctr_drbg_psa_context {
    mbedtls_svc_key_id_t key_id;
    psa_cipher_operation_t operation;
} mbedtls_ctr_drbg_psa_context;
#endif

/**
 * \brief          The CTR_DRBG context structure.
 */
typedef struct mbedtls_ctr_drbg_context {
    unsigned char MBEDTLS_PRIVATE(counter)[16];  /*!< The counter (V). */
    int MBEDTLS_PRIVATE(reseed_counter);         /*!< The reseed counter.
                                                  * This is the number of requests that have
                                                  * been made since the last (re)seeding,
                                                  * minus one.
                                                  * Before the initial seeding, this field
                                                  * contains the amount of entropy in bytes
                                                  * to use as a nonce for the initial seeding,
                                                  * or -1 if no nonce length has been explicitly
                                                  * set (see mbedtls_ctr_drbg_set_nonce_len()).
                                                  */
    int MBEDTLS_PRIVATE(prediction_resistance);  /*!< This determines whether prediction
                                                    resistance is enabled, that is
                                                    whether to systematically reseed before
                                                    each random generation. */
    size_t MBEDTLS_PRIVATE(entropy_len);         /*!< The amount of entropy grabbed on each
                                                    seed or reseed operation, in bytes. */
    int MBEDTLS_PRIVATE(reseed_interval);        /*!< The reseed interval.
                                                  * This is the maximum number of requests
                                                  * that can be made between reseedings. */

#if defined(MBEDTLS_CTR_DRBG_USE_PSA_CRYPTO)
    mbedtls_ctr_drbg_psa_context MBEDTLS_PRIVATE(psa_ctx); /*!< The PSA context. */
#else
    mbedtls_aes_context MBEDTLS_PRIVATE(aes_ctx);        /*!< The AES context. */
#endif

    /*
     * Callbacks (Entropy)
     */
    int(*MBEDTLS_PRIVATE(f_entropy))(void *, unsigned char *, size_t);
    /*!< The entropy callback function. */

    void *MBEDTLS_PRIVATE(p_entropy);            /*!< The context for the entropy function. */

#if defined(MBEDTLS_THREADING_C)
    /* Invariant: the mutex is initialized if and only if f_entropy != NULL.
     * This means that the mutex is initialized during the initial seeding
     * in mbedtls_ctr_drbg_seed() and freed in mbedtls_ctr_drbg_free().
     *
     * Note that this invariant may change without notice. Do not rely on it
     * and do not access the mutex directly in application code.
     */
    mbedtls_threading_mutex_t MBEDTLS_PRIVATE(mutex);
#endif
}
mbedtls_ctr_drbg_context;

/**
 * \brief               This function initializes the CTR_DRBG context,
 *                      and prepares it for mbedtls_ctr_drbg_seed()
 *                      or mbedtls_ctr_drbg_free().
 *
 * \note                The reseed interval is
 *                      #MBEDTLS_CTR_DRBG_RESEED_INTERVAL by default.
 *                      You can override it by calling
 *                      mbedtls_ctr_drbg_set_reseed_interval().
 *
 * \param ctx           The CTR_DRBG context to initialize.
 */
void mbedtls_ctr_drbg_init(mbedtls_ctr_drbg_context *ctx);

/**
 * \brief               This function seeds and sets up the CTR_DRBG
 *                      entropy source for future reseeds.
 *
 * A typical choice for the \p f_entropy and \p p_entropy parameters is
 * to use the entropy module:
 * - \p f_entropy is mbedtls_entropy_func();
 * - \p p_entropy is an instance of ::mbedtls_entropy_context initialized
 *   with mbedtls_entropy_init() (which registers the platform's default
 *   entropy sources).
 *
 * The entropy length is #MBEDTLS_CTR_DRBG_ENTROPY_LEN by default.
 * You can override it by calling mbedtls_ctr_drbg_set_entropy_len().
 *
 * The entropy nonce length is:
 * - \c 0 if the entropy length is at least 3/2 times the entropy length,
 *   which guarantees that the security strength is the maximum permitted
 *   by the key size and entropy length according to NIST SP 800-90A §10.2.1;
 * - Half the entropy length otherwise.
 * You can override it by calling mbedtls_ctr_drbg_set_nonce_len().
 * With the default entropy length, the entropy nonce length is
 * #MBEDTLS_CTR_DRBG_ENTROPY_NONCE_LEN.
 *
 * You can provide a nonce and personalization string in addition to the
 * entropy source, to make this instantiation as unique as possible.
 * See SP 800-90A §8.6.7 for more details about nonces.
 *
 * The _seed_material_ value passed to the derivation function in
 * the CTR_DRBG Instantiate Process described in NIST SP 800-90A §10.2.1.3.2
 * is the concatenation of the following strings:
 * - A string obtained by calling \p f_entropy function for the entropy
 *   length.
 */
#if MBEDTLS_CTR_DRBG_ENTROPY_NONCE_LEN == 0
/**
 * - If mbedtls_ctr_drbg_set_nonce_len() has been called, a string
 *   obtained by calling \p f_entropy function for the specified length.
 */
#else
/**
 * - A string obtained by calling \p f_entropy function for the entropy nonce
 *   length. If the entropy nonce length is \c 0, this function does not
 *   make a second call to \p f_entropy.
 */
#endif
#if defined(MBEDTLS_THREADING_C)
/**
 * \note                When Mbed TLS is built with threading support,
 *                      after this function returns successfully,
 *                      it is safe to call mbedtls_ctr_drbg_random()
 *                      from multiple threads. Other operations, including
 *                      reseeding, are not thread-safe.
 */
#endif /* MBEDTLS_THREADING_C */
/**
 * - The \p custom string.
 *
 * \note                To achieve the nominal security strength permitted
 *                      by CTR_DRBG, the entropy length must be:
 *                      - at least 16 bytes for a 128-bit strength
 *                      (maximum achievable strength when using AES-128);
 *                      - at least 32 bytes for a 256-bit strength
 *                      (maximum achievable strength when using AES-256).
 *
 *                      In addition, if you do not pass a nonce in \p custom,
 *                      the sum of the entropy length
 *                      and the entropy nonce length must be:
 *                      - at least 24 bytes for a 128-bit strength
 *                      (maximum achievable strength when using AES-128);
 *                      - at least 48 bytes for a 256-bit strength
 *                      (maximum achievable strength when using AES-256).
 *
 * \param ctx           The CTR_DRBG context to seed.
 *                      It must have been initialized with
 *                      mbedtls_ctr_drbg_init().
 *                      After a successful call to mbedtls_ctr_drbg_seed(),
 *                      you may not call mbedtls_ctr_drbg_seed() again on
 *                      the same context unless you call
 *                      mbedtls_ctr_drbg_free() and mbedtls_ctr_drbg_init()
 *                      again first.
 *                      After a failed call to mbedtls_ctr_drbg_seed(),
 *                      you must call mbedtls_ctr_drbg_free().
 * \param f_entropy     The entropy callback, taking as arguments the
 *                      \p p_entropy context, the buffer to fill, and the
 *                      length of the buffer.
 *                      \p f_entropy is always called with a buffer size
 *                      less than or equal to the entropy length.
 * \param p_entropy     The entropy context to pass to \p f_entropy.
 * \param custom        The personalization string.
 *                      This can be \c NULL, in which case the personalization
 *                      string is empty regardless of the value of \p len.
 * \param len           The length of the personalization string.
 *                      This must be at most
 *                      #MBEDTLS_CTR_DRBG_MAX_SEED_INPUT
 *                      - #MBEDTLS_CTR_DRBG_ENTROPY_LEN.
 *
 * \return              \c 0 on success.
 * \return              #MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED on failure.
 */
int mbedtls_ctr_drbg_seed(mbedtls_ctr_drbg_context *ctx,
                          int (*f_entropy)(void *, unsigned char *, size_t),
                          void *p_entropy,
                          const unsigned char *custom,
                          size_t len);

/**
 * \brief               This function resets CTR_DRBG context to the state immediately
 *                      after initial call of mbedtls_ctr_drbg_init().
 *
 * \param ctx           The CTR_DRBG context to clear.
 */
void mbedtls_ctr_drbg_free(mbedtls_ctr_drbg_context *ctx);

/**
 * \brief               This function turns prediction resistance on or off.
 *                      The default value is off.
 *
 * \note                If enabled, entropy is gathered at the beginning of
 *                      every call to mbedtls_ctr_drbg_random_with_add()
 *                      or mbedtls_ctr_drbg_random().
 *                      Only use this if your entropy source has sufficient
 *                      throughput.
 *
 * \param ctx           The CTR_DRBG context.
 * \param resistance    #MBEDTLS_CTR_DRBG_PR_ON or #MBEDTLS_CTR_DRBG_PR_OFF.
 */
void mbedtls_ctr_drbg_set_prediction_resistance(mbedtls_ctr_drbg_context *ctx,
                                                int resistance);

/**
 * \brief               This function sets the amount of entropy grabbed on each
 *                      seed or reseed.
 *
 * The default value is #MBEDTLS_CTR_DRBG_ENTROPY_LEN.
 *
 * \note                The security strength of CTR_DRBG is bounded by the
 *                      entropy length. Thus:
 *                      - When using AES-256
 *                        (\c MBEDTLS_CTR_DRBG_USE_128_BIT_KEY is disabled,
 *                        which is the default),
 *                        \p len must be at least 32 (in bytes)
 *                        to achieve a 256-bit strength.
 *                      - When using AES-128
 *                        (\c MBEDTLS_CTR_DRBG_USE_128_BIT_KEY is enabled)
 *                        \p len must be at least 16 (in bytes)
 *                        to achieve a 128-bit strength.
 *
 * \param ctx           The CTR_DRBG context.
 * \param len           The amount of entropy to grab, in bytes.
 *                      This must be at most #MBEDTLS_CTR_DRBG_MAX_SEED_INPUT
 *                      and at most the maximum length accepted by the
 *                      entropy function that is set in the context.
 */
void mbedtls_ctr_drbg_set_entropy_len(mbedtls_ctr_drbg_context *ctx,
                                      size_t len);

/**
 * \brief               This function sets the amount of entropy grabbed
 *                      as a nonce for the initial seeding.
 *
 * Call this function before calling mbedtls_ctr_drbg_seed() to read
 * a nonce from the entropy source during the initial seeding.
 *
 * \param ctx           The CTR_DRBG context.
 * \param len           The amount of entropy to grab for the nonce, in bytes.
 *                      This must be at most #MBEDTLS_CTR_DRBG_MAX_SEED_INPUT
 *                      and at most the maximum length accepted by the
 *                      entropy function that is set in the context.
 *
 * \return              \c 0 on success.
 * \return              #MBEDTLS_ERR_CTR_DRBG_INPUT_TOO_BIG if \p len is
 *                      more than #MBEDTLS_CTR_DRBG_MAX_SEED_INPUT.
 * \return              #MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED
 *                      if the initial seeding has already taken place.
 */
int mbedtls_ctr_drbg_set_nonce_len(mbedtls_ctr_drbg_context *ctx,
                                   size_t len);

/**
 * \brief               This function sets the reseed interval.
 *
 * The reseed interval is the number of calls to mbedtls_ctr_drbg_random()
 * or mbedtls_ctr_drbg_random_with_add() after which the entropy function
 * is called again.
 *
 * The default value is #MBEDTLS_CTR_DRBG_RESEED_INTERVAL.
 *
 * \param ctx           The CTR_DRBG context.
 * \param interval      The reseed interval.
 */
void mbedtls_ctr_drbg_set_reseed_interval(mbedtls_ctr_drbg_context *ctx,
                                          int interval);

/**
 * \brief               This function reseeds the CTR_DRBG context, that is
 *                      extracts data from the entropy source.
 *
 * \note                This function is not thread-safe. It is not safe
 *                      to call this function if another thread might be
 *                      concurrently obtaining random numbers from the same
 *                      context or updating or reseeding the same context.
 *
 * \param ctx           The CTR_DRBG context.
 * \param additional    Additional data to add to the state. Can be \c NULL.
 * \param len           The length of the additional data.
 *                      This must be less than
 *                      #MBEDTLS_CTR_DRBG_MAX_SEED_INPUT - \c entropy_len
 *                      where \c entropy_len is the entropy length
 *                      configured for the context.
 *
 * \return              \c 0 on success.
 * \return              #MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED on failure.
 */
int mbedtls_ctr_drbg_reseed(mbedtls_ctr_drbg_context *ctx,
                            const unsigned char *additional, size_t len);

/**
 * \brief              This function updates the state of the CTR_DRBG context.
 *
 * \note                This function is not thread-safe. It is not safe
 *                      to call this function if another thread might be
 *                      concurrently obtaining random numbers from the same
 *                      context or updating or reseeding the same context.
 *
 * \param ctx          The CTR_DRBG context.
 * \param additional   The data to update the state with. This must not be
 *                     \c NULL unless \p add_len is \c 0.
 * \param add_len      Length of \p additional in bytes. This must be at
 *                     most #MBEDTLS_CTR_DRBG_MAX_SEED_INPUT.
 *
 * \return             \c 0 on success.
 * \return             #MBEDTLS_ERR_CTR_DRBG_INPUT_TOO_BIG if
 *                     \p add_len is more than
 *                     #MBEDTLS_CTR_DRBG_MAX_SEED_INPUT.
 * \return             An error from the underlying AES cipher on failure.
 */
int mbedtls_ctr_drbg_update(mbedtls_ctr_drbg_context *ctx,
                            const unsigned char *additional,
                            size_t add_len);

/**
 * \brief   This function updates a CTR_DRBG instance with additional
 *          data and uses it to generate random data.
 *
 * This function automatically reseeds if the reseed counter is exceeded
 * or prediction resistance is enabled.
 *
 * \note                This function is not thread-safe. It is not safe
 *                      to call this function if another thread might be
 *                      concurrently obtaining random numbers from the same
 *                      context or updating or reseeding the same context.
 *
 * \param p_rng         The CTR_DRBG context. This must be a pointer to a
 *                      #mbedtls_ctr_drbg_context structure.
 * \param output        The buffer to fill.
 * \param output_len    The length of the buffer in bytes.
 * \param additional    Additional data to update. Can be \c NULL, in which
 *                      case the additional data is empty regardless of
 *                      the value of \p add_len.
 * \param add_len       The length of the additional data
 *                      if \p additional is not \c NULL.
 *                      This must be less than #MBEDTLS_CTR_DRBG_MAX_INPUT
 *                      and less than
 *                      #MBEDTLS_CTR_DRBG_MAX_SEED_INPUT - \c entropy_len
 *                      where \c entropy_len is the entropy length
 *                      configured for the context.
 *
 * \return    \c 0 on success.
 * \return    #MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED or
 *            #MBEDTLS_ERR_CTR_DRBG_REQUEST_TOO_BIG on failure.
 */
int mbedtls_ctr_drbg_random_with_add(void *p_rng,
                                     unsigned char *output, size_t output_len,
                                     const unsigned char *additional, size_t add_len);

/**
 * \brief   This function uses CTR_DRBG to generate random data.
 *
 * This function automatically reseeds if the reseed counter is exceeded
 * or prediction resistance is enabled.
 */
#if defined(MBEDTLS_THREADING_C)
/**
 * \note                When Mbed TLS is built with threading support,
 *                      it is safe to call mbedtls_ctr_drbg_random()
 *                      from multiple threads. Other operations, including
 *                      reseeding, are not thread-safe.
 */
#endif /* MBEDTLS_THREADING_C */
/**
 * \param p_rng         The CTR_DRBG context. This must be a pointer to a
 *                      #mbedtls_ctr_drbg_context structure.
 * \param output        The buffer to fill.
 * \param output_len    The length of the buffer in bytes.
 *
 * \return              \c 0 on success.
 * \return              #MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED or
 *                      #MBEDTLS_ERR_CTR_DRBG_REQUEST_TOO_BIG on failure.
 */
int mbedtls_ctr_drbg_random(void *p_rng,
                            unsigned char *output, size_t output_len);

#if defined(MBEDTLS_FS_IO)
/**
 * \brief               This function writes a seed file.
 *
 * \param ctx           The CTR_DRBG context.
 * \param path          The name of the file.
 *
 * \return              \c 0 on success.
 * \return              #MBEDTLS_ERR_CTR_DRBG_FILE_IO_ERROR on file error.
 * \return              #MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED on reseed
 *                      failure.
 */
int mbedtls_ctr_drbg_write_seed_file(mbedtls_ctr_drbg_context *ctx, const char *path);

/**
 * \brief               This function reads and updates a seed file. The seed
 *                      is added to this instance.
 *
 * \param ctx           The CTR_DRBG context.
 * \param path          The name of the file.
 *
 * \return              \c 0 on success.
 * \return              #MBEDTLS_ERR_CTR_DRBG_FILE_IO_ERROR on file error.
 * \return              #MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED on
 *                      reseed failure.
 * \return              #MBEDTLS_ERR_CTR_DRBG_INPUT_TOO_BIG if the existing
 *                      seed file is too large.
 */
int mbedtls_ctr_drbg_update_seed_file(mbedtls_ctr_drbg_context *ctx, const char *path);
#endif /* MBEDTLS_FS_IO */

#if defined(MBEDTLS_SELF_TEST)

/**
 * \brief               The CTR_DRBG checkup routine.
 *
 * \return              \c 0 on success.
 * \return              \c 1 on failure.
 */
int mbedtls_ctr_drbg_self_test(int verbose);

#endif /* MBEDTLS_SELF_TEST */

#ifdef __cplusplus
}
#endif

#endif /* ctr_drbg.h */
