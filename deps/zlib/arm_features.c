/* arm_features.c -- ARM processor features detection.
 *
 * Copyright 2018 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the Chromium source repository LICENSE file.
 */

#include "arm_features.h"
#include "zutil.h"
#include <stdint.h>

int ZLIB_INTERNAL arm_cpu_enable_crc32 = 0;
int ZLIB_INTERNAL arm_cpu_enable_pmull = 0;

#if defined(ARMV8_OS_ANDROID) || defined(ARMV8_OS_LINUX) || defined(ARMV8_OS_FUCHSIA)
#include <pthread.h>
#endif

#if defined(ARMV8_OS_ANDROID)
#include <cpu-features.h>
#elif defined(ARMV8_OS_LINUX)
#include <asm/hwcap.h>
#include <sys/auxv.h>
#elif defined(ARMV8_OS_FUCHSIA)
#include <zircon/features.h>
#include <zircon/syscalls.h>
#include <zircon/types.h>
#elif defined(ARMV8_OS_WINDOWS)
#include <windows.h>
#else
#error arm_features.c ARM feature detection in not defined for your platform
#endif

static void _arm_check_features(void);

#if defined(ARMV8_OS_ANDROID) || defined(ARMV8_OS_LINUX) || defined(ARMV8_OS_FUCHSIA)
static pthread_once_t cpu_check_inited_once = PTHREAD_ONCE_INIT;
void ZLIB_INTERNAL arm_check_features(void)
{
    pthread_once(&cpu_check_inited_once, _arm_check_features);
}
#elif defined(ARMV8_OS_WINDOWS)
static INIT_ONCE cpu_check_inited_once = INIT_ONCE_STATIC_INIT;
static BOOL CALLBACK _arm_check_features_forwarder(PINIT_ONCE once, PVOID param, PVOID* context)
{
    _arm_check_features();
    return TRUE;
}
void ZLIB_INTERNAL arm_check_features(void)
{
    InitOnceExecuteOnce(&cpu_check_inited_once, _arm_check_features_forwarder,
                        NULL, NULL);
}
#endif

/*
 * See http://bit.ly/2CcoEsr for run-time detection of ARM features and also
 * crbug.com/931275 for android_getCpuFeatures() use in the Android sandbox.
 */
static void _arm_check_features(void)
{
#if defined(ARMV8_OS_ANDROID) && defined(__aarch64__)
    uint64_t features = android_getCpuFeatures();
    arm_cpu_enable_crc32 = !!(features & ANDROID_CPU_ARM64_FEATURE_CRC32);
    arm_cpu_enable_pmull = !!(features & ANDROID_CPU_ARM64_FEATURE_PMULL);
#elif defined(ARMV8_OS_ANDROID) /* aarch32 */
    uint64_t features = android_getCpuFeatures();
    arm_cpu_enable_crc32 = !!(features & ANDROID_CPU_ARM_FEATURE_CRC32);
    arm_cpu_enable_pmull = !!(features & ANDROID_CPU_ARM_FEATURE_PMULL);
#elif defined(ARMV8_OS_LINUX) && defined(__aarch64__)
    unsigned long features = getauxval(AT_HWCAP);
    arm_cpu_enable_crc32 = !!(features & HWCAP_CRC32);
    arm_cpu_enable_pmull = !!(features & HWCAP_PMULL);
#elif defined(ARMV8_OS_LINUX) && (defined(__ARM_NEON) || defined(__ARM_NEON__))
    /* Query HWCAP2 for ARMV8-A SoCs running in aarch32 mode */
    unsigned long features = getauxval(AT_HWCAP2);
    arm_cpu_enable_crc32 = !!(features & HWCAP2_CRC32);
    arm_cpu_enable_pmull = !!(features & HWCAP2_PMULL);
#elif defined(ARMV8_OS_FUCHSIA)
    uint32_t features;
    zx_status_t rc = zx_system_get_features(ZX_FEATURE_KIND_CPU, &features);
    if (rc != ZX_OK || (features & ZX_ARM64_FEATURE_ISA_ASIMD) == 0)
        return;  /* Report nothing if ASIMD(NEON) is missing */
    arm_cpu_enable_crc32 = !!(features & ZX_ARM64_FEATURE_ISA_CRC32);
    arm_cpu_enable_pmull = !!(features & ZX_ARM64_FEATURE_ISA_PMULL);
#elif defined(ARMV8_OS_WINDOWS)
    arm_cpu_enable_crc32 = IsProcessorFeaturePresent(PF_ARM_V8_CRC32_INSTRUCTIONS_AVAILABLE);
    arm_cpu_enable_pmull = IsProcessorFeaturePresent(PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE);
#endif
}
