/**
 * \file ssl_cache.h
 *
 * \brief SSL session cache implementation
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#ifndef MBEDTLS_SSL_CACHE_H
#define MBEDTLS_SSL_CACHE_H
#include "mbedtls/private_access.h"

#include "mbedtls/build_info.h"

#include "mbedtls/ssl.h"

#if defined(MBEDTLS_THREADING_C)
#include "mbedtls/threading.h"
#endif

/**
 * \name SECTION: Module settings
 *
 * The configuration options you can set for this module are in this section.
 * Either change them in mbedtls_config.h or define them on the compiler command line.
 * \{
 */

#if !defined(MBEDTLS_SSL_CACHE_DEFAULT_TIMEOUT)
#define MBEDTLS_SSL_CACHE_DEFAULT_TIMEOUT       86400   /*!< 1 day  */
#endif

#if !defined(MBEDTLS_SSL_CACHE_DEFAULT_MAX_ENTRIES)
#define MBEDTLS_SSL_CACHE_DEFAULT_MAX_ENTRIES      50   /*!< Maximum entries in cache */
#endif

/** \} name SECTION: Module settings */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mbedtls_ssl_cache_context mbedtls_ssl_cache_context;
typedef struct mbedtls_ssl_cache_entry mbedtls_ssl_cache_entry;

/**
 * \brief   This structure is used for storing cache entries
 */
struct mbedtls_ssl_cache_entry {
#if defined(MBEDTLS_HAVE_TIME)
    mbedtls_time_t MBEDTLS_PRIVATE(timestamp);           /*!< entry timestamp    */
#endif

    unsigned char MBEDTLS_PRIVATE(session_id)[32];       /*!< session ID         */
    size_t MBEDTLS_PRIVATE(session_id_len);

    unsigned char *MBEDTLS_PRIVATE(session);             /*!< serialized session */
    size_t MBEDTLS_PRIVATE(session_len);

    mbedtls_ssl_cache_entry *MBEDTLS_PRIVATE(next);      /*!< chain pointer      */
};

/**
 * \brief Cache context
 */
struct mbedtls_ssl_cache_context {
    mbedtls_ssl_cache_entry *MBEDTLS_PRIVATE(chain);     /*!< start of the chain     */
    int MBEDTLS_PRIVATE(timeout);                /*!< cache entry timeout    */
    int MBEDTLS_PRIVATE(max_entries);            /*!< maximum entries        */
#if defined(MBEDTLS_THREADING_C)
    mbedtls_threading_mutex_t MBEDTLS_PRIVATE(mutex);    /*!< mutex                  */
#endif
};

/**
 * \brief          Initialize an SSL cache context
 *
 * \param cache    SSL cache context
 */
void mbedtls_ssl_cache_init(mbedtls_ssl_cache_context *cache);

/**
 * \brief          Cache get callback implementation
 *                 (Thread-safe if MBEDTLS_THREADING_C is enabled)
 *
 * \param data            The SSL cache context to use.
 * \param session_id      The pointer to the buffer holding the session ID
 *                        for the session to load.
 * \param session_id_len  The length of \p session_id in bytes.
 * \param session         The address at which to store the session
 *                        associated with \p session_id, if present.
 *
 * \return                \c 0 on success.
 * \return                #MBEDTLS_ERR_SSL_CACHE_ENTRY_NOT_FOUND if there is
 *                        no cache entry with specified session ID found, or
 *                        any other negative error code for other failures.
 */
int mbedtls_ssl_cache_get(void *data,
                          unsigned char const *session_id,
                          size_t session_id_len,
                          mbedtls_ssl_session *session);

/**
 * \brief          Cache set callback implementation
 *                 (Thread-safe if MBEDTLS_THREADING_C is enabled)
 *
 * \param data            The SSL cache context to use.
 * \param session_id      The pointer to the buffer holding the session ID
 *                        associated to \p session.
 * \param session_id_len  The length of \p session_id in bytes.
 * \param session         The session to store.
 *
 * \return                \c 0 on success.
 * \return                A negative error code on failure.
 */
int mbedtls_ssl_cache_set(void *data,
                          unsigned char const *session_id,
                          size_t session_id_len,
                          const mbedtls_ssl_session *session);

/**
 * \brief          Remove the cache entry by the session ID
 *                 (Thread-safe if MBEDTLS_THREADING_C is enabled)
 *
 * \param data            The SSL cache context to use.
 * \param session_id      The pointer to the buffer holding the session ID
 *                        associated to session.
 * \param session_id_len  The length of \p session_id in bytes.
 *
 * \return                \c 0 on success. This indicates the cache entry for
 *                        the session with provided ID is removed or does not
 *                        exist.
 * \return                A negative error code on failure.
 */
int mbedtls_ssl_cache_remove(void *data,
                             unsigned char const *session_id,
                             size_t session_id_len);

#if defined(MBEDTLS_HAVE_TIME)
/**
 * \brief          Set the cache timeout
 *                 (Default: MBEDTLS_SSL_CACHE_DEFAULT_TIMEOUT (1 day))
 *
 *                 A timeout of 0 indicates no timeout.
 *
 * \param cache    SSL cache context
 * \param timeout  cache entry timeout in seconds
 */
void mbedtls_ssl_cache_set_timeout(mbedtls_ssl_cache_context *cache, int timeout);

/**
 * \brief          Get the cache timeout
 *
 *                 A timeout of 0 indicates no timeout.
 *
 * \param cache    SSL cache context
 *
 * \return         cache entry timeout in seconds
 */
static inline int mbedtls_ssl_cache_get_timeout(mbedtls_ssl_cache_context *cache)
{
    return cache->MBEDTLS_PRIVATE(timeout);
}
#endif /* MBEDTLS_HAVE_TIME */

/**
 * \brief          Set the maximum number of cache entries
 *                 (Default: MBEDTLS_SSL_CACHE_DEFAULT_MAX_ENTRIES (50))
 *
 * \param cache    SSL cache context
 * \param max      cache entry maximum
 */
void mbedtls_ssl_cache_set_max_entries(mbedtls_ssl_cache_context *cache, int max);

/**
 * \brief          Free referenced items in a cache context and clear memory
 *
 * \param cache    SSL cache context
 */
void mbedtls_ssl_cache_free(mbedtls_ssl_cache_context *cache);

#ifdef __cplusplus
}
#endif

#endif /* ssl_cache.h */
