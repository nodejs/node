#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <signal.h>
#include <crypto.h>

#include "arm_arch.h"

unsigned int OPENSSL_armcap_P = 0;

#if __ARM_MAX_ARCH__<7
void OPENSSL_cpuid_setup(void)
{
}

unsigned long OPENSSL_rdtsc(void)
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
unsigned long _armv7_tick(void);

unsigned long OPENSSL_rdtsc(void)
{
    if (OPENSSL_armcap_P & ARMV7_TICK)
        return _armv7_tick();
    else
        return 0;
}

/*
 * Use a weak reference to getauxval() so we can use it if it is available but
 * don't break the build if it is not.
 */
# if defined(__GNUC__) && __GNUC__>=2
void OPENSSL_cpuid_setup(void) __attribute__ ((constructor));
extern unsigned long getauxval(unsigned long type) __attribute__ ((weak));
# else
static unsigned long (*getauxval) (unsigned long) = NULL;
# endif

/*
 * ARM puts the the feature bits for Crypto Extensions in AT_HWCAP2, whereas
 * AArch64 used AT_HWCAP.
 */
# if defined(__arm__) || defined (__arm)
#  define HWCAP                  16
                                  /* AT_HWCAP */
#  define HWCAP_NEON             (1 << 12)

#  define HWCAP_CE               26
                                  /* AT_HWCAP2 */
#  define HWCAP_CE_AES           (1 << 0)
#  define HWCAP_CE_PMULL         (1 << 1)
#  define HWCAP_CE_SHA1          (1 << 2)
#  define HWCAP_CE_SHA256        (1 << 3)
# elif defined(__aarch64__)
#  define HWCAP                  16
                                  /* AT_HWCAP */
#  define HWCAP_NEON             (1 << 1)

#  define HWCAP_CE               HWCAP
#  define HWCAP_CE_AES           (1 << 3)
#  define HWCAP_CE_PMULL         (1 << 4)
#  define HWCAP_CE_SHA1          (1 << 5)
#  define HWCAP_CE_SHA256        (1 << 6)
# endif

void OPENSSL_cpuid_setup(void)
{
    char *e;
    struct sigaction ill_oact, ill_act;
    sigset_t oset;
    static int trigger = 0;

    if (trigger)
        return;
    trigger = 1;

    if ((e = getenv("OPENSSL_armcap"))) {
        OPENSSL_armcap_P = (unsigned int)strtoul(e, NULL, 0);
        return;
    }

    sigfillset(&all_masked);
    sigdelset(&all_masked, SIGILL);
    sigdelset(&all_masked, SIGTRAP);
    sigdelset(&all_masked, SIGFPE);
    sigdelset(&all_masked, SIGBUS);
    sigdelset(&all_masked, SIGSEGV);

    OPENSSL_armcap_P = 0;

    memset(&ill_act, 0, sizeof(ill_act));
    ill_act.sa_handler = ill_handler;
    ill_act.sa_mask = all_masked;

    sigprocmask(SIG_SETMASK, &ill_act.sa_mask, &oset);
    sigaction(SIGILL, &ill_act, &ill_oact);

    if (getauxval != NULL) {
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
        }
    } else if (sigsetjmp(ill_jmp, 1) == 0) {
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
    }
    if (sigsetjmp(ill_jmp, 1) == 0) {
        _armv7_tick();
        OPENSSL_armcap_P |= ARMV7_TICK;
    }

    sigaction(SIGILL, &ill_oact, NULL);
    sigprocmask(SIG_SETMASK, &oset, NULL);
}
#endif
