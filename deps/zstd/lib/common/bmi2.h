/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef ZSTD_BMI2_H
#define ZSTD_BMI2_H

#include "portability_macros.h"

#if defined(ZSTD_USE_KERNEL_CPU_FEATURES)
#  include <asm/cpufeature.h>
#endif

/*
 * Select between the BMI2 and default implementations at the final dispatch
 * site. Integrations may ignore the caller-provided hint and consult their own
 * CPU-feature policy instead.
 *
 * ZSTD_USE_BMI2() may not evaluate its argument. ZSTD_SET_BMI2() may not
 * evaluate its value argument, which avoids executing the userspace CPUID
 * probe when the integration supplies the dispatch decision.
 */
#if !DYNAMIC_BMI2
#  define ZSTD_USE_BMI2(bmi2) 0
#  define ZSTD_SET_BMI2(state, value) do { } while (0)
#elif defined(ZSTD_USE_KERNEL_CPU_FEATURES)
#  define ZSTD_USE_BMI2(bmi2) cpu_feature_enabled(X86_FEATURE_BMI2)
#  define ZSTD_SET_BMI2(state, value) do { (state) = 0; } while (0)
#else
#  define ZSTD_USE_BMI2(bmi2) (bmi2)
#  define ZSTD_SET_BMI2(state, value) do { (state) = (value); } while (0)
#endif

#endif /* ZSTD_BMI2_H */
