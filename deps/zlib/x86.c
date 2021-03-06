/*
 * x86 feature check
 *
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
 * Author:
 *  Jim Kukunas
 *
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "x86.h"
#include "zutil.h"

int ZLIB_INTERNAL x86_cpu_enable_ssse3 = 0;
int ZLIB_INTERNAL x86_cpu_enable_simd = 0;

#ifndef _MSC_VER
#include <pthread.h>

pthread_once_t cpu_check_inited_once = PTHREAD_ONCE_INIT;
static void _x86_check_features(void);

void x86_check_features(void)
{
  pthread_once(&cpu_check_inited_once, _x86_check_features);
}

static void _x86_check_features(void)
{
    int x86_cpu_has_sse2;
    int x86_cpu_has_ssse3;
    int x86_cpu_has_sse42;
    int x86_cpu_has_pclmulqdq;
    unsigned eax, ebx, ecx, edx;

    eax = 1;
#ifdef __i386__
    __asm__ __volatile__ (
        "xchg %%ebx, %1\n\t"
        "cpuid\n\t"
        "xchg %1, %%ebx\n\t"
    : "+a" (eax), "=S" (ebx), "=c" (ecx), "=d" (edx)
    );
#else
    __asm__ __volatile__ (
        "cpuid\n\t"
    : "+a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
    );
#endif  /* (__i386__) */

    x86_cpu_has_sse2 = edx & 0x4000000;
    x86_cpu_has_ssse3 = ecx & 0x000200;
    x86_cpu_has_sse42 = ecx & 0x100000;
    x86_cpu_has_pclmulqdq = ecx & 0x2;

    x86_cpu_enable_ssse3 = x86_cpu_has_ssse3;

    x86_cpu_enable_simd = x86_cpu_has_sse2 &&
                          x86_cpu_has_sse42 &&
                          x86_cpu_has_pclmulqdq;
}
#else
#include <intrin.h>
#include <windows.h>

static BOOL CALLBACK _x86_check_features(PINIT_ONCE once,
                                         PVOID param,
                                         PVOID *context);
static INIT_ONCE cpu_check_inited_once = INIT_ONCE_STATIC_INIT;

void x86_check_features(void)
{
    InitOnceExecuteOnce(&cpu_check_inited_once, _x86_check_features,
                        NULL, NULL);
}

static BOOL CALLBACK _x86_check_features(PINIT_ONCE once,
                                         PVOID param,
                                         PVOID *context)
{
    int x86_cpu_has_sse2;
    int x86_cpu_has_ssse3;
    int x86_cpu_has_sse42;
    int x86_cpu_has_pclmulqdq;
    int regs[4];

    __cpuid(regs, 1);

    x86_cpu_has_sse2 = regs[3] & 0x4000000;
    x86_cpu_has_ssse3 = regs[2] & 0x000200;
    x86_cpu_has_sse42 = regs[2] & 0x100000;
    x86_cpu_has_pclmulqdq = regs[2] & 0x2;

    x86_cpu_enable_ssse3 = x86_cpu_has_ssse3;

    x86_cpu_enable_simd = x86_cpu_has_sse2 &&
                          x86_cpu_has_sse42 &&
                          x86_cpu_has_pclmulqdq;
    return TRUE;
}
#endif  /* _MSC_VER */
