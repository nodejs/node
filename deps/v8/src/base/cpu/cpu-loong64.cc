// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/cpu/cpu.h"

#if V8_OS_LINUX
#include <linux/auxvec.h>  // AT_HWCAP
#include <sys/auxv.h>      // getauxval()
#endif
#if V8_OS_POSIX
#include <unistd.h>  // sysconf()
#endif

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include "src/base/cpu/cpu-helper.h"
#include "src/base/logging.h"
#include "src/base/platform/wrappers.h"

namespace v8::base {
#if V8_OS_LINUX
#if V8_HOST_ARCH_LOONG64

#define LOONGARCH_CFG2 0x2
#define LOONGARCH_CFG2_LSX (1 << 6)
#define LOONGARCH_CFG2_LASX (1 << 7)

static int cpu_flags_cpucfg(int cfg) {
  int flags = 0;

  __asm__ volatile("cpucfg %0, %1 \n\t" : "+&r"(flags) : "r"(cfg));

  return flags;
}
#endif  // V8_HOST_ARCH_LOONG64
#endif  // V8_OS_LINUX

void CPU::DetectFeatures() {
#if V8_HOST_ARCH_LOONG64
  CPUInfo cpu_info;
  int flags = cpu_flags_cpucfg(LOONGARCH_CFG2);
  if (flags != 0) {
    has_lsx_ = (flags & LOONGARCH_CFG2_LSX) != 0;
    has_lasx_ = (flags & LOONGARCH_CFG2_LASX) != 0;
  } else {
    char* features = cpu_info.ExtractField("features");
    has_lsx_ = HasListItem(features, "lsx");
    has_lasx_ = HasListItem(features, "lasx");
    delete[] features;
  }
#endif  // V8_HOST_ARCH_LOONG64
}
}  // namespace v8::base
