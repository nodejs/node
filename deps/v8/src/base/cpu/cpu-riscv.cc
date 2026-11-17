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

#include <asm/unistd.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include "src/base/cpu/cpu-helper.h"
#include "src/base/logging.h"
#include "src/base/platform/wrappers.h"

#ifdef V8_HOST_ARCH_RISCV64
#include <asm/hwprobe.h>
#include <riscv_vector.h>
// The __riscv_vlenb intrinsic is only available when compiling with the RVV
// extension enabled. Use the 'target' attribute to tell the compiler to
// compile this function with RVV enabled.
// We must not call this function when RVV is not supported by the CPU.
__attribute__((target("arch=+v"))) static unsigned vlen_intrinsic() {
  return static_cast<unsigned>(__riscv_vlenb() * 8);
}

#if V8_OS_LINUX
// Checks whether the given feature appears as a whole underscore-separated
// item in a RISC-V "isa" string (e.g. "rv64imafdcv_zba_zbb").
static bool IsaHasFeature(const char* isa, const char* feature) {
  if (isa == nullptr) {
    return false;
  }
  size_t feature_len = strlen(feature);
  const char* p = isa;
  while (*p != '\0') {
    while (*p == '_') ++p;
    const char* q = p;
    while (*q != '\0' && *q != '_') ++q;
    if (static_cast<size_t>(q - p) == feature_len &&
        memcmp(p, feature, feature_len) == 0) {
      return true;
    }
    p = q;
  }
  return false;
}
#endif  // V8_OS_LINUX
#endif

namespace v8::base {
void CPU::DetectFeatures() {
#ifdef V8_HOST_ARCH_RISCV64
#if V8_OS_LINUX
  CPUInfo cpu_info;
  riscv_hwprobe pairs[] = {{RISCV_HWPROBE_KEY_IMA_EXT_0, 0}};
  if (!syscall(__NR_riscv_hwprobe, &pairs,
               sizeof(pairs) / sizeof(riscv_hwprobe), 0, nullptr, 0)) {
    if (pairs[0].value & RISCV_HWPROBE_IMA_V) {
      has_rvv_ = true;
    }
    if (pairs[0].value & RISCV_HWPROBE_IMA_FD) {
      has_fpu_ = true;
    }
    if (pairs[0].value & RISCV_HWPROBE_EXT_ZBA) {
      has_zba_ = true;
    }
    if (pairs[0].value & RISCV_HWPROBE_EXT_ZBB) {
      has_zbb_ = true;
    }
    if (pairs[0].value & RISCV_HWPROBE_EXT_ZBS) {
      has_zbs_ = true;
    }
    if (pairs[0].value & RISCV_HWPROBE_EXT_ZFA) {
      has_zfa_ = true;
    }
    if (pairs[0].value & RISCV_HWPROBE_EXT_ZFH) {
      has_zfh_ = true;
    }
    if (pairs[0].value & RISCV_HWPROBE_EXT_ZVFH) {
      has_zvfh_ = true;
    }
    if (pairs[0].value & RISCV_HWPROBE_IMA_C) {
      has_rvc_ = true;
    }
  } else {
    // The riscv_hwprobe syscall was introduced in Linux 6.4. On older
    // kernels (e.g. vendor BSP kernels that backport RVV 1.0 but not
    // hwprobe), fall back to the "isa" line of /proc/cpuinfo. This restores
    // the fallback that src/base/cpu.cc had before the split into
    // per-architecture files (89ad1535).
    char* isa = cpu_info.ExtractField("isa");

    if (IsaHasFeature(isa, "rv64imafdc")) {
      has_fpu_ = true;
    }
    if (IsaHasFeature(isa, "rv64imafdcv")) {
      has_fpu_ = true;
      has_rvv_ = true;
    }
  }

  char* mmu = cpu_info.ExtractField("mmu");
  if (HasListItem(mmu, "sv48")) {
    riscv_mmu_ = RV_MMU_MODE::kRiscvSV48;
  }
  if (HasListItem(mmu, "sv39")) {
    riscv_mmu_ = RV_MMU_MODE::kRiscvSV39;
  }
  if (HasListItem(mmu, "sv57")) {
    riscv_mmu_ = RV_MMU_MODE::kRiscvSV57;
  }
#endif
  if (has_rvv_) {
    vlen_ = vlen_intrinsic();
  }
#endif
}
}  // namespace v8::base
