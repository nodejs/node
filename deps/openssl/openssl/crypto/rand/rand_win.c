/*
 * Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "internal/cryptlib.h"
#include <openssl/rand.h>
#include "rand_lcl.h"

#if defined(OPENSSL_SYS_WINDOWS) || defined(OPENSSL_SYS_WIN32)
# include <windows.h>
/* On Windows 7 or higher use BCrypt instead of the legacy CryptoAPI */
# if defined(_MSC_VER) && defined(_WIN32_WINNT) && _WIN32_WINNT>=0x0601
#  define RAND_WINDOWS_USE_BCRYPT
# endif

# ifdef RAND_WINDOWS_USE_BCRYPT
#  include <bcrypt.h>
#  pragma comment(lib, "bcrypt.lib")
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

static void readtimer(void);

int RAND_poll(void)
{
    MEMORYSTATUS mst;
# ifndef RAND_WINDOWS_USE_BCRYPT
    HCRYPTPROV hProvider;
# endif
    DWORD w;
    BYTE buf[64];

# ifdef RAND_WINDOWS_USE_BCRYPT
    if (BCryptGenRandom(NULL, buf, (ULONG)sizeof(buf), BCRYPT_USE_SYSTEM_PREFERRED_RNG) == STATUS_SUCCESS) {
        RAND_add(buf, sizeof(buf), sizeof(buf));
    }
# else
    /* poll the CryptoAPI PRNG */
    /* The CryptoAPI returns sizeof(buf) bytes of randomness */
    if (CryptAcquireContextW(&hProvider, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) {
        if (CryptGenRandom(hProvider, (DWORD)sizeof(buf), buf) != 0) {
            RAND_add(buf, sizeof(buf), sizeof(buf));
        }
        CryptReleaseContext(hProvider, 0);
    }

    /* poll the Pentium PRG with CryptoAPI */
    if (CryptAcquireContextW(&hProvider, NULL, INTEL_DEF_PROV, PROV_INTEL_SEC, CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) {
        if (CryptGenRandom(hProvider, (DWORD)sizeof(buf), buf) != 0) {
            RAND_add(buf, sizeof(buf), sizeof(buf));
        }
        CryptReleaseContext(hProvider, 0);
    }
# endif

    /* timer data */
    readtimer();

    /* memory usage statistics */
    GlobalMemoryStatus(&mst);
    RAND_add(&mst, sizeof(mst), 1);

    /* process ID */
    w = GetCurrentProcessId();
    RAND_add(&w, sizeof(w), 1);

    return (1);
}

#if OPENSSL_API_COMPAT < 0x10100000L
int RAND_event(UINT iMsg, WPARAM wParam, LPARAM lParam)
{
    RAND_poll();
    return RAND_status();
}

void RAND_screen(void)
{
    RAND_poll();
}
#endif

/* feed timing information to the PRNG */
static void readtimer(void)
{
    DWORD w;
    LARGE_INTEGER l;
    static int have_perfc = 1;
# if defined(_MSC_VER) && defined(_M_X86)
    static int have_tsc = 1;
    DWORD cyclecount;

    if (have_tsc) {
        __try {
            __asm {
            _emit 0x0f _emit 0x31 mov cyclecount, eax}
            RAND_add(&cyclecount, sizeof(cyclecount), 1);
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            have_tsc = 0;
        }
    }
# else
#  define have_tsc 0
# endif

    if (have_perfc) {
        if (QueryPerformanceCounter(&l) == 0)
            have_perfc = 0;
        else
            RAND_add(&l, sizeof(l), 0);
    }

    if (!have_tsc && !have_perfc) {
        w = GetTickCount();
        RAND_add(&w, sizeof(w), 0);
    }
}

#endif
