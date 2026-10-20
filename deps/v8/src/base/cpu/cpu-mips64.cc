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
void CPU::DetectFeatures() {
#if V8_HOST_ARCH_MIPS64
  // Simple detection of FPU at runtime for Linux.
  // It is based on /proc/cpuinfo, which reveals hardware configuration
  // to user-space applications.  According to MIPS (early 2010), no similar
  // facility is universally available on the MIPS architectures,
  // so it's up to individual OSes to provide such.
  CPUInfo cpu_info;
  char* cpu_model = cpu_info.ExtractField("cpu model");
  has_fpu_ = HasListItem(cpu_model, "FPU");
  char* ASEs = cpu_info.ExtractField("ASEs implemented");
  has_msa_ = HasListItem(ASEs, "msa");
  delete[] cpu_model;
  delete[] ASEs;
#endif  // V8_HOST_ARCH_MIPS64
}
}  // namespace v8::base
