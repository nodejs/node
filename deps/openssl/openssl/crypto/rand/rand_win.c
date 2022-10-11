/*
 * Copyright 1995-2022 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "internal/cryptlib.h"
#include <openssl/rand.h>
#include "rand_local.h"
#include "crypto/rand.h"
#if defined(OPENSSL_SYS_WINDOWS) || defined(OPENSSL_SYS_WIN32)

# ifndef OPENSSL_RAND_SEED_OS
#  error "Unsupported seeding method configured; must be os"
# endif

# include <windows.h>
/* On Windows Vista or higher use BCrypt instead of the legacy CryptoAPI */
# if defined(_MSC_VER) && _MSC_VER > 1500 /* 1500 = Visual Studio 2008 */ \
     && defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0600
#  define USE_BCRYPTGENRANDOM
# endif

# ifdef USE_BCRYPTGENRANDOM
#  include <bcrypt.h>
#  ifdef _MSC_VER
#   pragma comment(lib, "bcrypt.lib")
#  endif
#  ifndef STATUS_SUCCESS
#   define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#  endif
# else
#  include <wincrypt.h>
/*
 * Intel hardware RNG CSP -- available from
 * http://developer.intel.com/design/security/rng/redist_license.htm
 */
#  define PROV_INTEL_SEC 22
#  define INTEL_DEF_PROV L"Intel Hardware Cryptographic Service Provider"
# endif

size_t rand_pool_acquire_entropy(RAND_POOL *pool)
{
# ifndef USE_BCRYPTGENRANDOM
    HCRYPTPROV hProvider;
# endif
    unsigned char *buffer;
    size_t bytes_needed;
    size_t entropy_available = 0;


# ifdef OPENSSL_RAND_SEED_RDTSC
    entropy_available = rand_acquire_entropy_from_tsc(pool);
    if (entropy_available > 0)
        return entropy_available;
# endif

# ifdef OPENSSL_RAND_SEED_RDCPU
    entropy_available = rand_acquire_entropy_from_cpu(pool);
    if (entropy_available > 0)
        return entropy_available;
# endif

# ifdef USE_BCRYPTGENRANDOM
    bytes_needed = rand_pool_bytes_needed(pool, 1 /*entropy_factor*/);
    buffer = rand_pool_add_begin(pool, bytes_needed);
    if (buffer != NULL) {
        size_t bytes = 0;
        if (BCryptGenRandom(NULL, buffer, bytes_needed,
                            BCRYPT_USE_SYSTEM_PREFERRED_RNG) == STATUS_SUCCESS)
            bytes = bytes_needed;

        rand_pool_add_end(pool, bytes, 8 * bytes);
        entropy_available = rand_pool_entropy_available(pool);
    }
    if (entropy_available > 0)
        return entropy_available;
# else
    bytes_needed = rand_pool_bytes_needed(pool, 1 /*entropy_factor*/);
    buffer = rand_pool_add_begin(pool, bytes_needed);
    if (buffer != NULL) {
        size_t bytes = 0;
        /* poll the CryptoAPI PRNG */
        if (CryptAcquireContextW(&hProvider, NULL, NULL, PROV_RSA_FULL,
                                 CRYPT_VERIFYCONTEXT | CRYPT_SILENT) != 0) {
            if (CryptGenRandom(hProvider, bytes_needed, buffer) != 0)
                bytes = bytes_needed;

            CryptReleaseContext(hProvider, 0);
        }

        rand_pool_add_end(pool, bytes, 8 * bytes);
        entropy_available = rand_pool_entropy_available(pool);
    }
    if (entropy_available > 0)
        return entropy_available;

    bytes_needed = rand_pool_bytes_needed(pool, 1 /*entropy_factor*/);
    buffer = rand_pool_add_begin(pool, bytes_needed);
    if (buffer != NULL) {
        size_t bytes = 0;
        /* poll the Pentium PRG with CryptoAPI */
        if (CryptAcquireContextW(&hProvider, NULL,
                                 INTEL_DEF_PROV, PROV_INTEL_SEC,
                                 CRYPT_VERIFYCONTEXT | CRYPT_SILENT) != 0) {
            if (CryptGenRandom(hProvider, bytes_needed, buffer) != 0)
                bytes = bytes_needed;

            CryptReleaseContext(hProvider, 0);
        }
        rand_pool_add_end(pool, bytes, 8 * bytes);
        entropy_available = rand_pool_entropy_available(pool);
    }
    if (entropy_available > 0)
        return entropy_available;
# endif

    return rand_pool_entropy_available(pool);
}


int rand_pool_add_nonce_data(RAND_POOL *pool)
{
    struct {
        DWORD pid;
        DWORD tid;
        FILETIME time;
    } data = { 0 };

    /*
     * Add process id, thread id, and a high resolution timestamp to
     * ensure that the nonce is unique with high probability for
     * different process instances.
     */
    data.pid = GetCurrentProcessId();
    data.tid = GetCurrentThreadId();
    GetSystemTimeAsFileTime(&data.time);

    return rand_pool_add(pool, (unsigned char *)&data, sizeof(data), 0);
}

int rand_pool_add_additional_data(RAND_POOL *pool)
{
    struct {
        DWORD tid;
        LARGE_INTEGER time;
    } data = { 0 };

    /*
     * Add some noise from the thread id and a high resolution timer.
     * The thread id adds a little randomness if the drbg is accessed
     * concurrently (which is the case for the <master> drbg).
     */
    data.tid = GetCurrentThreadId();
    QueryPerformanceCounter(&data.time);
    return rand_pool_add(pool, (unsigned char *)&data, sizeof(data), 0);
}

# if OPENSSL_API_COMPAT < 0x10100000L
int RAND_event(UINT iMsg, WPARAM wParam, LPARAM lParam)
{
    RAND_poll();
    return RAND_status();
}

void RAND_screen(void)
{
    RAND_poll();
}
# endif

int rand_pool_init(void)
{
    return 1;
}

void rand_pool_cleanup(void)
{
}

void rand_pool_keep_random_devices_open(int keep)
{
}

#endif
