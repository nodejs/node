// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "src/base/cpu/cpu.h"

#if defined(V8_OS_STARBOARD)
#include "starboard/cpu_features.h"
#endif

#if V8_LIBC_MSVCRT
#include <intrin.h>  // __cpuid()
#endif
#if V8_OS_LINUX
#include <linux/auxvec.h>  // AT_HWCAP
#include <sys/auxv.h>      // getauxval()
#endif
#if V8_OS_QNX
#include <sys/syspage.h>  // cpuinfo
#endif
#if V8_OS_DARWIN
#include <sys/sysctl.h>  // sysctlbyname
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
#if V8_OS_WIN
#include <windows.h>
#endif

namespace v8::base {

#if V8_OS_LINUX
#if V8_HOST_ARCH_ARM

// See <uapi/asm/hwcap.h> kernel header.
/*
 * HWCAP flags - for elf_hwcap (in kernel) and AT_HWCAP
 */
#define HWCAP_SWP (1 << 0)
#define HWCAP_HALF (1 << 1)
#define HWCAP_THUMB (1 << 2)
#define HWCAP_26BIT (1 << 3) /* Play it safe */
#define HWCAP_FAST_MULT (1 << 4)
#define HWCAP_FPA (1 << 5)
#define HWCAP_VFP (1 << 6)
#define HWCAP_EDSP (1 << 7)
#define HWCAP_JAVA (1 << 8)
#define HWCAP_IWMMXT (1 << 9)
#define HWCAP_CRUNCH (1 << 10)
#define HWCAP_THUMBEE (1 << 11)
#define HWCAP_NEON (1 << 12)
#define HWCAP_VFPv3 (1 << 13)
#define HWCAP_VFPv3D16 (1 << 14) /* also set for VFPv4-D16 */
#define HWCAP_TLS (1 << 15)
#define HWCAP_VFPv4 (1 << 16)
#define HWCAP_IDIVA (1 << 17)
#define HWCAP_IDIVT (1 << 18)
#define HWCAP_VFPD32 (1 << 19) /* set if VFP has 32 regs (not 16) */
#define HWCAP_IDIV (HWCAP_IDIVA | HWCAP_IDIVT)
#define HWCAP_LPAE (1 << 20)

#endif  // V8_HOST_ARCH_ARM

#if V8_HOST_ARCH_ARM64

// See <uapi/asm/hwcap.h> kernel header.
/*
 * HWCAP flags - for elf_hwcap (in kernel) and AT_HWCAP
 */
#define HWCAP_FP (1 << 0)
#define HWCAP_ASIMD (1 << 1)
#define HWCAP_EVTSTRM (1 << 2)
#define HWCAP_AES (1 << 3)
#define HWCAP_PMULL (1 << 4)
#define HWCAP_SHA1 (1 << 5)
#define HWCAP_SHA2 (1 << 6)
#define HWCAP_CRC32 (1 << 7)
#define HWCAP_ATOMICS (1 << 8)
#define HWCAP_FPHP (1 << 9)
#define HWCAP_ASIMDHP (1 << 10)
#define HWCAP_CPUID (1 << 11)
#define HWCAP_ASIMDRDM (1 << 12)
#define HWCAP_JSCVT (1 << 13)
#define HWCAP_FCMA (1 << 14)
#define HWCAP_LRCPC (1 << 15)
#define HWCAP_DCPOP (1 << 16)
#define HWCAP_SHA3 (1 << 17)
#define HWCAP_SM3 (1 << 18)
#define HWCAP_SM4 (1 << 19)
#define HWCAP_ASIMDDP (1 << 20)
#define HWCAP_SHA512 (1 << 21)
#define HWCAP_SVE (1 << 22)
#define HWCAP_ASIMDFHM (1 << 23)
#define HWCAP_DIT (1 << 24)
#define HWCAP_USCAT (1 << 25)
#define HWCAP_ILRCPC (1 << 26)
#define HWCAP_FLAGM (1 << 27)
#define HWCAP_SSBS (1 << 28)
#define HWCAP_SB (1 << 29)
#define HWCAP_PACA (1 << 30)
#define HWCAP_PACG (1UL << 31)
// See <uapi/asm/hwcap.h> kernel header.
/*
 * HWCAP2 flags - for elf_hwcap2 (in kernel) and AT_HWCAP2
 */
#define HWCAP2_SVEBITPERM (1 << 4)
#define HWCAP2_MTE (1 << 18)
#define HWCAP2_CSSC (1UL << 34)
#define HWCAP2_MOPS (1UL << 43)
#define HWCAP2_HBC (1UL << 44)
#endif  // V8_HOST_ARCH_ARM64

#if V8_HOST_ARCH_ARM || V8_HOST_ARCH_ARM64

static std::tuple<uint64_t, uint64_t> ReadELFHWCaps() {
  uint64_t hwcap = 0;
  uint64_t hwcap2 = 0;
#if V8_OS_LINUX && defined(AT_HWCAP)
  hwcap = static_cast<uint64_t>(getauxval(AT_HWCAP));
#if defined(AT_HWCAP2)
  hwcap2 = static_cast<uint64_t>(getauxval(AT_HWCAP2));
#endif  // AT_HWCAP2
#else
  // Read the ELF HWCAP flags by parsing /proc/self/auxv.
  // If getauxval is not available, the kernel/libc is also not new enough to
  // expose hwcap2.
  FILE* fp = base::Fopen("/proc/self/auxv", "r");
  if (fp != nullptr) {
    struct {
      uint32_t tag;
      uint32_t value;
    } entry;
    for (;;) {
      size_t n = fread(&entry, sizeof(entry), 1, fp);
      if (n == 0 || (entry.tag == 0 && entry.value == 0)) {
        break;
      }
      if (entry.tag == AT_HWCAP) {
        hwcap = entry.value;
        break;
      }
    }
    base::Fclose(fp);
  }
#endif
  return std::make_tuple(hwcap, hwcap2);
}
#endif  // V8_HOST_ARCH_ARM || V8_HOST_ARCH_ARM64
#endif  // V8_OS_LINUX

void CPU::DetectFeatures() {
#if V8_HOST_ARCH_ARM

#if V8_OS_LINUX

  CPUInfo cpu_info;

  // Extract implementor from the "CPU implementer" field.
  char* implementer = cpu_info.ExtractField("CPU implementer");
  if (implementer != nullptr) {
    char* end;
    implementer_ = strtol(implementer, &end, 0);
    if (end == implementer) {
      implementer_ = 0;
    }
    delete[] implementer;
  }

  char* variant = cpu_info.ExtractField("CPU variant");
  if (variant != nullptr) {
    char* end;
    variant_ = strtol(variant, &end, 0);
    if (end == variant) {
      variant_ = -1;
    }
    delete[] variant;
  }

  // Extract part number from the "CPU part" field.
  char* part = cpu_info.ExtractField("CPU part");
  if (part != nullptr) {
    char* end;
    part_ = strtol(part, &end, 0);
    if (end == part) {
      part_ = 0;
    }
    delete[] part;
  }

  // Extract architecture from the "CPU Architecture" field.
  // The list is well-known, unlike the output of
  // the 'Processor' field which can vary greatly.
  // See the definition of the 'proc_arch' array in
  // $KERNEL/arch/arm/kernel/setup.c and the 'c_show' function in
  // same file.
  char* architecture = cpu_info.ExtractField("CPU architecture");
  if (architecture != nullptr) {
    char* end;
    architecture_ = strtol(architecture, &end, 10);
    if (end == architecture) {
      // Kernels older than 3.18 report "CPU architecture: AArch64" on ARMv8.
      if (strcmp(architecture, "AArch64") == 0) {
        architecture_ = 8;
      } else {
        architecture_ = 0;
      }
    }
    delete[] architecture;

    // Unfortunately, it seems that certain ARMv6-based CPUs
    // report an incorrect architecture number of 7!
    //
    // See http://code.google.com/p/android/issues/detail?id=10812
    //
    // We try to correct this by looking at the 'elf_platform'
    // field reported by the 'Processor' field, which is of the
    // form of "(v7l)" for an ARMv7-based CPU, and "(v6l)" for
    // an ARMv6-one. For example, the Raspberry Pi is one popular
    // ARMv6 device that reports architecture 7.
    if (architecture_ == 7) {
      char* processor = cpu_info.ExtractField("Processor");
      if (HasListItem(processor, "(v6l)")) {
        architecture_ = 6;
      }
      delete[] processor;
    }

    // elf_platform moved to the model name field in Linux v3.8.
    if (architecture_ == 7) {
      char* processor = cpu_info.ExtractField("model name");
      if (HasListItem(processor, "(v6l)")) {
        architecture_ = 6;
      }
      delete[] processor;
    }
  }

  // Try to extract the list of CPU features from ELF hwcaps.
  uint64_t hwcaps = 0;
  uint64_t hwcaps2 = 0;
  std::tie(hwcaps, hwcaps2) = ReadELFHWCaps();
  if (hwcaps != 0) {
    has_idiva_ = (hwcaps & HWCAP_IDIVA) != 0;
    has_neon_ = (hwcaps & HWCAP_NEON) != 0;
    has_vfp_ = (hwcaps & HWCAP_VFP) != 0;
    has_vfp3_ = (hwcaps & (HWCAP_VFPv3 | HWCAP_VFPv3D16 | HWCAP_VFPv4)) != 0;
    has_vfp3_d32_ = (has_vfp3_ && ((hwcaps & HWCAP_VFPv3D16) == 0 ||
                                   (hwcaps & HWCAP_VFPD32) != 0));
  } else {
    // Try to fallback to "Features" CPUInfo field.
    char* features = cpu_info.ExtractField("Features");
    has_idiva_ = HasListItem(features, "idiva");
    has_neon_ = HasListItem(features, "neon");
    has_thumb2_ = HasListItem(features, "thumb2");
    has_vfp_ = HasListItem(features, "vfp");
    if (HasListItem(features, "vfpv3d16")) {
      has_vfp3_ = true;
    } else if (HasListItem(features, "vfpv3")) {
      has_vfp3_ = true;
      has_vfp3_d32_ = true;
    }
    delete[] features;
  }

  // Some old kernels will report vfp not vfpv3. Here we make an attempt
  // to detect vfpv3 by checking for vfp *and* neon, since neon is only
  // available on architectures with vfpv3. Checking neon on its own is
  // not enough as it is possible to have neon without vfp.
  if (has_vfp_ && has_neon_) {
    has_vfp3_ = true;
  }

  // VFPv3 implies ARMv7, see ARM DDI 0406B, page A1-6.
  if (architecture_ < 7 && has_vfp3_) {
    architecture_ = 7;
  }

  // ARMv7 implies Thumb2.
  if (architecture_ >= 7) {
    has_thumb2_ = true;
  }

  // The earliest architecture with Thumb2 is ARMv6T2.
  if (has_thumb2_ && architecture_ < 6) {
    architecture_ = 6;
  }

  // We don't support any FPUs other than VFP.
  has_fpu_ = has_vfp_;

#elif V8_OS_QNX

  uint32_t cpu_flags = SYSPAGE_ENTRY(cpuinfo)->flags;
  if (cpu_flags & ARM_CPU_FLAG_V7) {
    architecture_ = 7;
    has_thumb2_ = true;
  } else if (cpu_flags & ARM_CPU_FLAG_V6) {
    architecture_ = 6;
    // QNX doesn't say if Thumb2 is available.
    // Assume false for the architectures older than ARMv7.
  }
  DCHECK_GE(architecture_, 6);
  has_fpu_ = (cpu_flags & CPU_FLAG_FPU) != 0;
  has_vfp_ = has_fpu_;
  if (cpu_flags & ARM_CPU_FLAG_NEON) {
    has_neon_ = true;
    has_vfp3_ = has_vfp_;
#ifdef ARM_CPU_FLAG_VFP_D32
    has_vfp3_d32_ = (cpu_flags & ARM_CPU_FLAG_VFP_D32) != 0;
#endif
  }
  has_idiva_ = (cpu_flags & ARM_CPU_FLAG_IDIV) != 0;

#endif  // V8_OS_LINUX
#elif V8_HOST_ARCH_ARM64
#ifdef V8_OS_WIN
  // Windows makes high-resolution thread timing information available in
  // user-space.
  has_non_stop_time_stamp_counter_ = true;

  // Defined in winnt.h, but only in 10.0.20348.0 version of the Windows SDK.
  // Copy the value here to support older versions as well.
#if !defined(PF_ARM_V83_JSCVT_INSTRUCTIONS_AVAILABLE)
  constexpr int PF_ARM_V83_JSCVT_INSTRUCTIONS_AVAILABLE = 44;
#endif
#if !defined(PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE)
  constexpr int PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE = 43;
#endif
#if !defined(PF_ARM_V81_ATOMIC_INSTRUCTIONS_AVAILABLE)
  constexpr int PF_ARM_V81_ATOMIC_INSTRUCTIONS_AVAILABLE = 34;
#endif
#if !defined(PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE)
  constexpr int PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE = 30;
#endif
#if !defined(PF_ARM_SHA3_INSTRUCTIONS_AVAILABLE)
  constexpr int PF_ARM_SHA3_INSTRUCTIONS_AVAILABLE = 64;
#endif
#if !defined(PF_ARM_SVE_BITPERM_INSTRUCTIONS_AVAILABLE)
  constexpr int PF_ARM_SVE_BITPERM_INSTRUCTIONS_AVAILABLE = 51;
#endif
#if !defined(PF_ARM_SVE_INSTRUCTIONS_AVAILABLE)
  constexpr int PF_ARM_SVE_INSTRUCTIONS_AVAILABLE = 46;
#endif
#if !defined(PF_ARM_V82_FP16_INSTRUCTIONS_AVAILABLE)
  constexpr int PF_ARM_V82_FP16_INSTRUCTIONS_AVAILABLE = 67;
#endif

  has_jscvt_ =
      IsProcessorFeaturePresent(PF_ARM_V83_JSCVT_INSTRUCTIONS_AVAILABLE);
  has_dot_prod_ =
      IsProcessorFeaturePresent(PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE);
  has_lse_ =
      IsProcessorFeaturePresent(PF_ARM_V81_ATOMIC_INSTRUCTIONS_AVAILABLE);
  has_pmull1q_ =
      IsProcessorFeaturePresent(PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE);
  has_fp16_ = IsProcessorFeaturePresent(PF_ARM_V82_FP16_INSTRUCTIONS_AVAILABLE);
  has_sha3_ = IsProcessorFeaturePresent(PF_ARM_SHA3_INSTRUCTIONS_AVAILABLE);
  has_sve_ = IsProcessorFeaturePresent(PF_ARM_SVE_INSTRUCTIONS_AVAILABLE);
  has_svebitperm_ =
      IsProcessorFeaturePresent(PF_ARM_SVE_BITPERM_INSTRUCTIONS_AVAILABLE);

#elif V8_OS_LINUX
  // Try to extract the list of CPU features from ELF hwcaps.
  uint64_t hwcaps, hwcaps2;
  std::tie(hwcaps, hwcaps2) = ReadELFHWCaps();
  has_cssc_ = (hwcaps2 & HWCAP2_CSSC) != 0;
  has_mte_ = (hwcaps2 & HWCAP2_MTE) != 0;
  has_hbc_ = (hwcaps2 & HWCAP2_HBC) != 0;
  has_mops_ = (hwcaps2 & HWCAP2_MOPS) != 0;
  has_svebitperm_ = (hwcaps2 & HWCAP2_SVEBITPERM) != 0;
  if (hwcaps != 0) {
    has_jscvt_ = (hwcaps & HWCAP_JSCVT) != 0;
    has_dot_prod_ = (hwcaps & HWCAP_ASIMDDP) != 0;
    has_lse_ = (hwcaps & HWCAP_ATOMICS) != 0;
    has_pmull1q_ = (hwcaps & HWCAP_PMULL) != 0;
    has_fp16_ = (hwcaps & HWCAP_FPHP) != 0;
    has_sha3_ = (hwcaps & HWCAP_SHA3) != 0;
    has_sve_ = (hwcaps & HWCAP_SVE) != 0;
  } else {
    // Try to fallback to "Features" CPUInfo field
    CPUInfo cpu_info;
    char* features = cpu_info.ExtractField("Features");
    has_jscvt_ = HasListItem(features, "jscvt");
    has_dot_prod_ = HasListItem(features, "asimddp");
    has_lse_ = HasListItem(features, "atomics");
    has_pmull1q_ = HasListItem(features, "pmull");
    has_fp16_ = HasListItem(features, "half");
    has_sha3_ = HasListItem(features, "sha3");
    has_sve_ = HasListItem(features, "sve");
    delete[] features;
  }
#elif V8_OS_DARWIN
#if V8_OS_IOS
  int64_t feat_jscvt = 0;
  size_t feat_jscvt_size = sizeof(feat_jscvt);
  if (sysctlbyname("hw.optional.arm.FEAT_JSCVT", &feat_jscvt, &feat_jscvt_size,
                   nullptr, 0) == -1) {
    has_jscvt_ = false;
  } else {
    has_jscvt_ = feat_jscvt;
  }
  int64_t feat_dot_prod = 0;
  size_t feat_dot_prod_size = sizeof(feat_dot_prod);
  if (sysctlbyname("hw.optional.arm.FEAT_DotProd", &feat_dot_prod,
                   &feat_dot_prod_size, nullptr, 0) == -1) {
    has_dot_prod_ = false;
  } else {
    has_dot_prod_ = feat_dot_prod;
  }
  int64_t feat_lse = 0;
  size_t feat_lse_size = sizeof(feat_lse);
  if (sysctlbyname("hw.optional.arm.FEAT_LSE", &feat_lse, &feat_lse_size,
                   nullptr, 0) == -1) {
    has_lse_ = false;
  } else {
    has_lse_ = feat_lse;
  }
  int64_t feat_pmull = 0;
  size_t feat_pmull_size = sizeof(feat_pmull);
  if (sysctlbyname("hw.optional.arm.FEAT_PMULL", &feat_pmull, &feat_pmull_size,
                   nullptr, 0) == -1) {
    has_pmull1q_ = false;
  } else {
    has_pmull1q_ = feat_pmull;
  }
  int64_t fp16 = 0;
  size_t fp16_size = sizeof(fp16);
  if (sysctlbyname("hw.optional.arm.FEAT_FP16", &fp16, &fp16_size, nullptr,
                   0) == -1) {
    has_fp16_ = false;
  } else {
    has_fp16_ = fp16;
  }
  int64_t feat_sha3 = 0;
  size_t feat_sha3_size = sizeof(feat_sha3);
  if (sysctlbyname("hw.optional.arm.FEAT_SHA3", &feat_sha3, &feat_sha3_size,
                   nullptr, 0) == -1) {
    has_sha3_ = false;
  } else {
    has_sha3_ = feat_sha3;
  }
#else
  // ARM64 Macs always have the following features.
  has_jscvt_ = true;
  has_dot_prod_ = true;
  has_lse_ = true;
  has_pmull1q_ = true;
  has_fp16_ = true;
  has_sha3_ = true;
#endif  // V8_OS_IOS
  int64_t feat_cssc = 0;
  size_t feat_cssc_size = sizeof(feat_cssc);
  if (sysctlbyname("hw.optional.arm.FEAT_CSSC", &feat_cssc, &feat_cssc_size,
                   nullptr, 0) == -1) {
    has_cssc_ = false;
  } else {
    has_cssc_ = feat_cssc;
  }
#endif  // V8_OS_WIN
#endif  // V8_HOST_ARCH_ARM64
}
}  // namespace v8::base
