#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>
#include <openssl/bn.h>

#include "sparc_arch.h"

#if defined(__GNUC__) && defined(__linux)
__attribute__ ((visibility("hidden")))
#endif
unsigned int OPENSSL_sparcv9cap_P[2] = { SPARCV9_TICK_PRIVILEGED, 0 };

int bn_mul_mont(BN_ULONG *rp, const BN_ULONG *ap, const BN_ULONG *bp,
                const BN_ULONG *np, const BN_ULONG *n0, int num)
{
    int bn_mul_mont_vis3(BN_ULONG *rp, const BN_ULONG *ap, const BN_ULONG *bp,
                         const BN_ULONG *np, const BN_ULONG *n0, int num);
    int bn_mul_mont_fpu(BN_ULONG *rp, const BN_ULONG *ap, const BN_ULONG *bp,
                        const BN_ULONG *np, const BN_ULONG *n0, int num);
    int bn_mul_mont_int(BN_ULONG *rp, const BN_ULONG *ap, const BN_ULONG *bp,
                        const BN_ULONG *np, const BN_ULONG *n0, int num);

    if (!(num & 1) && num >= 6) {
        if ((num & 15) == 0 && num <= 64 &&
            (OPENSSL_sparcv9cap_P[1] & (CFR_MONTMUL | CFR_MONTSQR)) ==
            (CFR_MONTMUL | CFR_MONTSQR)) {
            typedef int (*bn_mul_mont_f) (BN_ULONG *rp, const BN_ULONG *ap,
                                          const BN_ULONG *bp,
                                          const BN_ULONG *np,
                                          const BN_ULONG *n0);
            int bn_mul_mont_t4_8(BN_ULONG *rp, const BN_ULONG *ap,
                                 const BN_ULONG *bp, const BN_ULONG *np,
                                 const BN_ULONG *n0);
            int bn_mul_mont_t4_16(BN_ULONG *rp, const BN_ULONG *ap,
                                  const BN_ULONG *bp, const BN_ULONG *np,
                                  const BN_ULONG *n0);
            int bn_mul_mont_t4_24(BN_ULONG *rp, const BN_ULONG *ap,
                                  const BN_ULONG *bp, const BN_ULONG *np,
                                  const BN_ULONG *n0);
            int bn_mul_mont_t4_32(BN_ULONG *rp, const BN_ULONG *ap,
                                  const BN_ULONG *bp, const BN_ULONG *np,
                                  const BN_ULONG *n0);
            static const bn_mul_mont_f funcs[4] = {
                bn_mul_mont_t4_8, bn_mul_mont_t4_16,
                bn_mul_mont_t4_24, bn_mul_mont_t4_32
            };
            bn_mul_mont_f worker = funcs[num / 16 - 1];

            if ((*worker) (rp, ap, bp, np, n0))
                return 1;
            /* retry once and fall back */
            if ((*worker) (rp, ap, bp, np, n0))
                return 1;
            return bn_mul_mont_vis3(rp, ap, bp, np, n0, num);
        }
        if ((OPENSSL_sparcv9cap_P[0] & SPARCV9_VIS3))
            return bn_mul_mont_vis3(rp, ap, bp, np, n0, num);
        else if (num >= 8 &&
                 (OPENSSL_sparcv9cap_P[0] &
                  (SPARCV9_PREFER_FPU | SPARCV9_VIS1)) ==
                 (SPARCV9_PREFER_FPU | SPARCV9_VIS1))
            return bn_mul_mont_fpu(rp, ap, bp, np, n0, num);
    }
    return bn_mul_mont_int(rp, ap, bp, np, n0, num);
}

unsigned long _sparcv9_rdtick(void);
void _sparcv9_vis1_probe(void);
unsigned long _sparcv9_vis1_instrument(void);
void _sparcv9_vis2_probe(void);
void _sparcv9_fmadd_probe(void);
unsigned long _sparcv9_rdcfr(void);
void _sparcv9_vis3_probe(void);
unsigned long _sparcv9_random(void);
size_t _sparcv9_vis1_instrument_bus(unsigned int *, size_t);
size_t _sparcv9_vis1_instrument_bus2(unsigned int *, size_t, size_t);

unsigned long OPENSSL_rdtsc(void)
{
    if (OPENSSL_sparcv9cap_P[0] & SPARCV9_TICK_PRIVILEGED)
#if defined(__sun) && defined(__SVR4)
        return gethrtime();
#else
        return 0;
#endif
    else
        return _sparcv9_rdtick();
}

size_t OPENSSL_instrument_bus(unsigned int *out, size_t cnt)
{
    if ((OPENSSL_sparcv9cap_P[0] & (SPARCV9_TICK_PRIVILEGED | SPARCV9_BLK)) ==
        SPARCV9_BLK)
        return _sparcv9_vis1_instrument_bus(out, cnt);
    else
        return 0;
}

size_t OPENSSL_instrument_bus2(unsigned int *out, size_t cnt, size_t max)
{
    if ((OPENSSL_sparcv9cap_P[0] & (SPARCV9_TICK_PRIVILEGED | SPARCV9_BLK)) ==
        SPARCV9_BLK)
        return _sparcv9_vis1_instrument_bus2(out, cnt, max);
    else
        return 0;
}

#if 0 && defined(__sun) && defined(__SVR4)
/*
 * This code path is disabled, because of incompatibility of libdevinfo.so.1
 * and libmalloc.so.1 (see below for details)
 */
# include <malloc.h>
# include <dlfcn.h>
# include <libdevinfo.h>
# include <sys/systeminfo.h>

typedef di_node_t(*di_init_t) (const char *, uint_t);
typedef void (*di_fini_t) (di_node_t);
typedef char *(*di_node_name_t) (di_node_t);
typedef int (*di_walk_node_t) (di_node_t, uint_t, di_node_name_t,
                               int (*)(di_node_t, di_node_name_t));

# define DLLINK(h,name) (name=(name##_t)dlsym((h),#name))

static int walk_nodename(di_node_t node, di_node_name_t di_node_name)
{
    char *name = (*di_node_name) (node);

    /* This is expected to catch all UltraSPARC flavors prior T1 */
    if (!strcmp(name, "SUNW,UltraSPARC") ||
        /* covers II,III,IV */
        !strncmp(name, "SUNW,UltraSPARC-I", 17)) {
        OPENSSL_sparcv9cap_P[0] |= SPARCV9_PREFER_FPU | SPARCV9_VIS1;

        /* %tick is privileged only on UltraSPARC-I/II, but not IIe */
        if (name[14] != '\0' && name[17] != '\0' && name[18] != '\0')
            OPENSSL_sparcv9cap_P[0] &= ~SPARCV9_TICK_PRIVILEGED;

        return DI_WALK_TERMINATE;
    }
    /* This is expected to catch remaining UltraSPARCs, such as T1 */
    else if (!strncmp(name, "SUNW,UltraSPARC", 15)) {
        OPENSSL_sparcv9cap_P[0] &= ~SPARCV9_TICK_PRIVILEGED;

        return DI_WALK_TERMINATE;
    }

    return DI_WALK_CONTINUE;
}

void OPENSSL_cpuid_setup(void)
{
    void *h;
    char *e, si[256];
    static int trigger = 0;

    if (trigger)
        return;
    trigger = 1;

    if ((e = getenv("OPENSSL_sparcv9cap"))) {
        OPENSSL_sparcv9cap_P[0] = strtoul(e, NULL, 0);
        return;
    }

    if (sysinfo(SI_MACHINE, si, sizeof(si)) > 0) {
        if (strcmp(si, "sun4v"))
            /* FPU is preferred for all CPUs, but US-T1/2 */
            OPENSSL_sparcv9cap_P[0] |= SPARCV9_PREFER_FPU;
    }

    if (sysinfo(SI_ISALIST, si, sizeof(si)) > 0) {
        if (strstr(si, "+vis"))
            OPENSSL_sparcv9cap_P[0] |= SPARCV9_VIS1 | SPARCV9_BLK;
        if (strstr(si, "+vis2")) {
            OPENSSL_sparcv9cap_P[0] |= SPARCV9_VIS2;
            OPENSSL_sparcv9cap_P[0] &= ~SPARCV9_TICK_PRIVILEGED;
            return;
        }
    }
# ifdef M_KEEP
    /*
     * Solaris libdevinfo.so.1 is effectively incomatible with
     * libmalloc.so.1. Specifically, if application is linked with
     * -lmalloc, it crashes upon startup with SIGSEGV in
     * free(3LIBMALLOC) called by di_fini. Prior call to
     * mallopt(M_KEEP,0) somehow helps... But not always...
     */
    if ((h = dlopen(NULL, RTLD_LAZY))) {
        union {
            void *p;
            int (*f) (int, int);
        } sym;
        if ((sym.p = dlsym(h, "mallopt")))
            (*sym.f) (M_KEEP, 0);
        dlclose(h);
    }
# endif
    if ((h = dlopen("libdevinfo.so.1", RTLD_LAZY)))
        do {
            di_init_t di_init;
            di_fini_t di_fini;
            di_walk_node_t di_walk_node;
            di_node_name_t di_node_name;
            di_node_t root_node;

            if (!DLLINK(h, di_init))
                break;
            if (!DLLINK(h, di_fini))
                break;
            if (!DLLINK(h, di_walk_node))
                break;
            if (!DLLINK(h, di_node_name))
                break;

            if ((root_node = (*di_init) ("/", DINFOSUBTREE)) != DI_NODE_NIL) {
                (*di_walk_node) (root_node, DI_WALK_SIBFIRST,
                                 di_node_name, walk_nodename);
                (*di_fini) (root_node);
            }
        } while (0);

    if (h)
        dlclose(h);
}

#else

static sigjmp_buf common_jmp;
static void common_handler(int sig)
{
    siglongjmp(common_jmp, sig);
}

void OPENSSL_cpuid_setup(void)
{
    char *e;
    struct sigaction common_act, ill_oact, bus_oact;
    sigset_t all_masked, oset;
    static int trigger = 0;

    if (trigger)
        return;
    trigger = 1;

    if ((e = getenv("OPENSSL_sparcv9cap"))) {
        OPENSSL_sparcv9cap_P[0] = strtoul(e, NULL, 0);
        if ((e = strchr(e, ':')))
            OPENSSL_sparcv9cap_P[1] = strtoul(e + 1, NULL, 0);
        return;
    }

    /* Initial value, fits UltraSPARC-I&II... */
    OPENSSL_sparcv9cap_P[0] = SPARCV9_PREFER_FPU | SPARCV9_TICK_PRIVILEGED;

    sigfillset(&all_masked);
    sigdelset(&all_masked, SIGILL);
    sigdelset(&all_masked, SIGTRAP);
# ifdef SIGEMT
    sigdelset(&all_masked, SIGEMT);
# endif
    sigdelset(&all_masked, SIGFPE);
    sigdelset(&all_masked, SIGBUS);
    sigdelset(&all_masked, SIGSEGV);
    sigprocmask(SIG_SETMASK, &all_masked, &oset);

    memset(&common_act, 0, sizeof(common_act));
    common_act.sa_handler = common_handler;
    common_act.sa_mask = all_masked;

    sigaction(SIGILL, &common_act, &ill_oact);
    sigaction(SIGBUS, &common_act, &bus_oact); /* T1 fails 16-bit ldda [on
                                                * Linux] */

    if (sigsetjmp(common_jmp, 1) == 0) {
        _sparcv9_rdtick();
        OPENSSL_sparcv9cap_P[0] &= ~SPARCV9_TICK_PRIVILEGED;
    }

    if (sigsetjmp(common_jmp, 1) == 0) {
        _sparcv9_vis1_probe();
        OPENSSL_sparcv9cap_P[0] |= SPARCV9_VIS1 | SPARCV9_BLK;
        /* detect UltraSPARC-Tx, see sparccpud.S for details... */
        if (_sparcv9_vis1_instrument() >= 12)
            OPENSSL_sparcv9cap_P[0] &= ~(SPARCV9_VIS1 | SPARCV9_PREFER_FPU);
        else {
            _sparcv9_vis2_probe();
            OPENSSL_sparcv9cap_P[0] |= SPARCV9_VIS2;
        }
    }

    if (sigsetjmp(common_jmp, 1) == 0) {
        _sparcv9_fmadd_probe();
        OPENSSL_sparcv9cap_P[0] |= SPARCV9_FMADD;
    }

    /*
     * VIS3 flag is tested independently from VIS1, unlike VIS2 that is,
     * because VIS3 defines even integer instructions.
     */
    if (sigsetjmp(common_jmp, 1) == 0) {
        _sparcv9_vis3_probe();
        OPENSSL_sparcv9cap_P[0] |= SPARCV9_VIS3;
    }
# if 0                          /* was planned at some point but never
                                 * implemented in hardware */
    if (sigsetjmp(common_jmp, 1) == 0) {
        (void)_sparcv9_random();
        OPENSSL_sparcv9cap_P[0] |= SPARCV9_RANDOM;
    }
# endif

    /*
     * In wait for better solution _sparcv9_rdcfr is masked by
     * VIS3 flag, because it goes to uninterruptable endless
     * loop on UltraSPARC II running Solaris. Things might be
     * different on Linux...
     */
    if ((OPENSSL_sparcv9cap_P[0] & SPARCV9_VIS3) &&
        sigsetjmp(common_jmp, 1) == 0) {
        OPENSSL_sparcv9cap_P[1] = (unsigned int)_sparcv9_rdcfr();
    }

    sigaction(SIGBUS, &bus_oact, NULL);
    sigaction(SIGILL, &ill_oact, NULL);

    sigprocmask(SIG_SETMASK, &oset, NULL);

    if (sizeof(size_t) == 8)
        OPENSSL_sparcv9cap_P[0] |= SPARCV9_64BIT_STACK;
# ifdef __linux
    else {
        int ret = syscall(340);

        if (ret >= 0 && ret & 1)
            OPENSSL_sparcv9cap_P[0] |= SPARCV9_64BIT_STACK;
    }
# endif
}

#endif
