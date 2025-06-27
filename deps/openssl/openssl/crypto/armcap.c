/*
 * Copyright 2011-2022 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <signal.h>
#include <openssl/crypto.h>
#ifdef __APPLE__
#include <sys/sysctl.h>
#endif
#include "internal/cryptlib.h"

#include "arm_arch.h"

unsigned int OPENSSL_armcap_P = 0;
unsigned int OPENSSL_arm_midr = 0;
unsigned int OPENSSL_armv8_rsa_neonized = 0;

#if __ARM_MAX_ARCH__<7
void OPENSSL_cpuid_setup(void)
{
}

uint32_t OPENSSL_rdtsc(void)
{
    return 0;
}
#else
static sigset_t all_masked;

static sigjmp_buf ill_jmp;
static void ill_handler(int sig)
{
    siglongjmp(ill_jmp, sig);
}

/*
 * Following subroutines could have been inlined, but it's not all
 * ARM compilers support inline assembler...
 */
void _armv7_neon_probe(void);
void _armv8_aes_probe(void);
void _armv8_sha1_probe(void);
void _armv8_sha256_probe(void);
void _armv8_pmull_probe(void);
# ifdef __aarch64__
void _armv8_sha512_probe(void);
unsigned int _armv8_cpuid_probe(void);
# endif
uint32_t _armv7_tick(void);

uint32_t OPENSSL_rdtsc(void)
{
    if (OPENSSL_armcap_P & ARMV7_TICK)
        return _armv7_tick();
    else
        return 0;
}

# if defined(__GNUC__) && __GNUC__>=2
void OPENSSL_cpuid_setup(void) __attribute__ ((constructor));
# endif

# if defined(__GLIBC__) && defined(__GLIBC_PREREQ)
#  if __GLIBC_PREREQ(2, 16)
#   include <sys/auxv.h>
#   define OSSL_IMPLEMENT_GETAUXVAL
#  endif
# elif defined(__ANDROID_API__)
/* see https://developer.android.google.cn/ndk/guides/cpu-features */
#  if __ANDROID_API__ >= 18
#   include <sys/auxv.h>
#   define OSSL_IMPLEMENT_GETAUXVAL
#  endif
# endif
# if defined(__FreeBSD__)
#  include <sys/param.h>
#  if __FreeBSD_version >= 1200000
#   include <sys/auxv.h>
#   define OSSL_IMPLEMENT_GETAUXVAL

static unsigned long getauxval(unsigned long key)
{
  unsigned long val = 0ul;

  if (elf_aux_info((int)key, &val, sizeof(val)) != 0)
    return 0ul;

  return val;
}
#  endif
# endif

/*
 * Android: according to https://developer.android.com/ndk/guides/cpu-features,
 * getauxval is supported starting with API level 18
 */
#  if defined(__ANDROID__) && defined(__ANDROID_API__) && __ANDROID_API__ >= 18
#   include <sys/auxv.h>
#   define OSSL_IMPLEMENT_GETAUXVAL
#  endif

/*
 * ARM puts the feature bits for Crypto Extensions in AT_HWCAP2, whereas
 * AArch64 used AT_HWCAP.
 */
# ifndef AT_HWCAP
#  define AT_HWCAP               16
# endif
# ifndef AT_HWCAP2
#  define AT_HWCAP2              26
# endif
# if defined(__arm__) || defined (__arm)
#  define HWCAP                  AT_HWCAP
#  define HWCAP_NEON             (1 << 12)

#  define HWCAP_CE               AT_HWCAP2
#  define HWCAP_CE_AES           (1 << 0)
#  define HWCAP_CE_PMULL         (1 << 1)
#  define HWCAP_CE_SHA1          (1 << 2)
#  define HWCAP_CE_SHA256        (1 << 3)
# elif defined(__aarch64__)
#  define HWCAP                  AT_HWCAP
#  define HWCAP_NEON             (1 << 1)

#  define HWCAP_CE               HWCAP
#  define HWCAP_CE_AES           (1 << 3)
#  define HWCAP_CE_PMULL         (1 << 4)
#  define HWCAP_CE_SHA1          (1 << 5)
#  define HWCAP_CE_SHA256        (1 << 6)
#  define HWCAP_CPUID            (1 << 11)
#  define HWCAP_CE_SHA512        (1 << 21)
# endif

void OPENSSL_cpuid_setup(void)
{
    const char *e;
    struct sigaction ill_oact, ill_act;
    sigset_t oset;
    static int trigger = 0;

    if (trigger)
        return;
    trigger = 1;

    OPENSSL_armcap_P = 0;

    if ((e = getenv("OPENSSL_armcap"))) {
        OPENSSL_armcap_P = (unsigned int)strtoul(e, NULL, 0);
        return;
    }

# if defined(__APPLE__)
#   if !defined(__aarch64__)
    /*
     * Capability probing by catching SIGILL appears to be problematic
     * on iOS. But since Apple universe is "monocultural", it's actually
     * possible to simply set pre-defined processor capability mask.
     */
    if (1) {
        OPENSSL_armcap_P = ARMV7_NEON;
        return;
    }
    /*
     * One could do same even for __aarch64__ iOS builds. It's not done
     * exclusively for reasons of keeping code unified across platforms.
     * Unified code works because it never triggers SIGILL on Apple
     * devices...
     */
#   else
    {
        unsigned int sha512;
        size_t len = sizeof(sha512);

        if (sysctlbyname("hw.optional.armv8_2_sha512", &sha512, &len, NULL, 0) == 0 && sha512 == 1)
            OPENSSL_armcap_P |= ARMV8_SHA512;
    }
#   endif
# endif

# ifdef OSSL_IMPLEMENT_GETAUXVAL
    if (getauxval(HWCAP) & HWCAP_NEON) {
        unsigned long hwcap = getauxval(HWCAP_CE);

        OPENSSL_armcap_P |= ARMV7_NEON;

        if (hwcap & HWCAP_CE_AES)
            OPENSSL_armcap_P |= ARMV8_AES;

        if (hwcap & HWCAP_CE_PMULL)
            OPENSSL_armcap_P |= ARMV8_PMULL;

        if (hwcap & HWCAP_CE_SHA1)
            OPENSSL_armcap_P |= ARMV8_SHA1;

        if (hwcap & HWCAP_CE_SHA256)
            OPENSSL_armcap_P |= ARMV8_SHA256;

#  ifdef __aarch64__
        if (hwcap & HWCAP_CE_SHA512)
            OPENSSL_armcap_P |= ARMV8_SHA512;

        if (hwcap & HWCAP_CPUID)
            OPENSSL_armcap_P |= ARMV8_CPUID;
#  endif
    }
# endif

    sigfillset(&all_masked);
    sigdelset(&all_masked, SIGILL);
    sigdelset(&all_masked, SIGTRAP);
    sigdelset(&all_masked, SIGFPE);
    sigdelset(&all_masked, SIGBUS);
    sigdelset(&all_masked, SIGSEGV);

    memset(&ill_act, 0, sizeof(ill_act));
    ill_act.sa_handler = ill_handler;
    ill_act.sa_mask = all_masked;

    sigprocmask(SIG_SETMASK, &ill_act.sa_mask, &oset);
    sigaction(SIGILL, &ill_act, &ill_oact);

    /* If we used getauxval, we already have all the values */
# ifndef OSSL_IMPLEMENT_GETAUXVAL
    if (sigsetjmp(ill_jmp, 1) == 0) {
        _armv7_neon_probe();
        OPENSSL_armcap_P |= ARMV7_NEON;
        if (sigsetjmp(ill_jmp, 1) == 0) {
            _armv8_pmull_probe();
            OPENSSL_armcap_P |= ARMV8_PMULL | ARMV8_AES;
        } else if (sigsetjmp(ill_jmp, 1) == 0) {
            _armv8_aes_probe();
            OPENSSL_armcap_P |= ARMV8_AES;
        }
        if (sigsetjmp(ill_jmp, 1) == 0) {
            _armv8_sha1_probe();
            OPENSSL_armcap_P |= ARMV8_SHA1;
        }
        if (sigsetjmp(ill_jmp, 1) == 0) {
            _armv8_sha256_probe();
            OPENSSL_armcap_P |= ARMV8_SHA256;
        }
#  if defined(__aarch64__) && !defined(__APPLE__)
        if (sigsetjmp(ill_jmp, 1) == 0) {
            _armv8_sha512_probe();
            OPENSSL_armcap_P |= ARMV8_SHA512;
        }
#  endif
    }
# endif

    /*
     * Probing for ARMV7_TICK is known to produce unreliable results,
     * so we will only use the feature when the user explicitly enables
     * it with OPENSSL_armcap.
     */

    sigaction(SIGILL, &ill_oact, NULL);
    sigprocmask(SIG_SETMASK, &oset, NULL);

# ifdef __aarch64__
    if (OPENSSL_armcap_P & ARMV8_CPUID)
        OPENSSL_arm_midr = _armv8_cpuid_probe();

    if ((MIDR_IS_CPU_MODEL(OPENSSL_arm_midr, ARM_CPU_IMP_ARM, ARM_CPU_PART_CORTEX_A72) ||
         MIDR_IS_CPU_MODEL(OPENSSL_arm_midr, ARM_CPU_IMP_ARM, ARM_CPU_PART_N1)) &&
        (OPENSSL_armcap_P & ARMV7_NEON)) {
            OPENSSL_armv8_rsa_neonized = 1;
    }
# endif
}
#endif
