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

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include "src/base/logging.h"
#include "src/base/platform/wrappers.h"
#if V8_OS_WIN
#include <windows.h>
#endif

namespace v8::base {
#if V8_HOST_ARCH_IA32 || V8_HOST_ARCH_X64
// Define __cpuid() for non-MSVC libraries.
#if !V8_LIBC_MSVCRT

static V8_INLINE void __cpuid(int cpu_info[4], int info_type) {
// Clear ecx to align with __cpuid() of MSVC:
// https://msdn.microsoft.com/en-us/library/hskdteyh.aspx
#if defined(__i386__) && defined(__pic__)
  // Make sure to preserve ebx, which contains the pointer
  // to the GOT in case we're generating PIC.
  __asm__ volatile(
      "mov %%ebx, %%edi\n\t"
      "cpuid\n\t"
      "xchg %%edi, %%ebx\n\t"
      : "=a"(cpu_info[0]), "=D"(cpu_info[1]), "=c"(cpu_info[2]),
        "=d"(cpu_info[3])
      : "a"(info_type), "c"(0));
#else
  __asm__ volatile("cpuid \n\t"
                   : "=a"(cpu_info[0]), "=b"(cpu_info[1]), "=c"(cpu_info[2]),
                     "=d"(cpu_info[3])
                   : "a"(info_type), "c"(0));
#endif  // defined(__i386__) && defined(__pic__)
}

static V8_INLINE void __cpuidex(int cpu_info[4], int info_type,
                                int sub_info_type) {
// Gather additional information about the processor.
// Set the value of the ECX register to sub_info_type before it generates the
// cpuid instruction, align with __cpuidex() of MSVC:
// https://msdn.microsoft.com/en-us/library/hskdteyh.aspx
#if defined(__i386__) && defined(__pic__)
  // Make sure to preserve ebx, which contains the pointer
  // to the GOT in case we're generating PIC.
  __asm__ volatile(
      "mov %%ebx, %%edi\n\t"
      "cpuid\n\t"
      "xchg %%edi, %%ebx\n\t"
      : "=a"(cpu_info[0]), "=D"(cpu_info[1]), "=c"(cpu_info[2]),
        "=d"(cpu_info[3])
      : "a"(info_type), "c"(sub_info_type));
#else
  __asm__ volatile("cpuid \n\t"
                   : "=a"(cpu_info[0]), "=b"(cpu_info[1]), "=c"(cpu_info[2]),
                     "=d"(cpu_info[3])
                   : "a"(info_type), "c"(sub_info_type));
#endif  // defined(__i386__) && defined(__pic__)
}

#endif  // !V8_LIBC_MSVCRT
#endif  // V8_HOST_ARCH_IA32 || V8_HOST_ARCH_X64

void CPU::DetectFeatures() {
#if V8_HOST_ARCH_IA32 || V8_HOST_ARCH_X64
  int cpu_info[4];

  // __cpuid with an InfoType argument of 0 returns the number of
  // valid Ids in CPUInfo[0] and the CPU identification string in
  // the other three array elements. The CPU identification string is
  // not in linear order. The code below arranges the information
  // in a human readable form. The human readable order is CPUInfo[1] |
  // CPUInfo[3] | CPUInfo[2]. CPUInfo[2] and CPUInfo[3] are swapped
  // before using memcpy to copy these three array elements to cpu_string.
  __cpuid(cpu_info, 0);
  unsigned num_ids = cpu_info[0];
  std::swap(cpu_info[2], cpu_info[3]);
  memcpy(vendor_, cpu_info + 1, 12);
  vendor_[12] = '\0';

  // Interpret CPU feature information.
  if (num_ids > 0) {
    __cpuid(cpu_info, 1);

    int cpu_info70[4] = {0};
    int cpu_info71[4] = {0};
    if (num_ids >= 7) {
      __cpuid(cpu_info70, 7);
      // Check the maximum input value for supported leaf 7 sub-leaves
      if (cpu_info70[0] >= 1) {
        __cpuidex(cpu_info71, 7, 1);
      }
    }

    stepping_ = cpu_info[0] & 0xF;
    model_ = ((cpu_info[0] >> 4) & 0xF) + ((cpu_info[0] >> 12) & 0xF0);
    family_ = (cpu_info[0] >> 8) & 0xF;
    type_ = (cpu_info[0] >> 12) & 0x3;
    ext_model_ = (cpu_info[0] >> 16) & 0xF;
    ext_family_ = (cpu_info[0] >> 20) & 0xFF;
    has_fpu_ = (cpu_info[3] & 0x00000001) != 0;
    has_cmov_ = (cpu_info[3] & 0x00008000) != 0;
    has_mmx_ = (cpu_info[3] & 0x00800000) != 0;
    has_sse_ = (cpu_info[3] & 0x02000000) != 0;
    has_sse2_ = (cpu_info[3] & 0x04000000) != 0;
    has_sse3_ = (cpu_info[2] & 0x00000001) != 0;
    has_ssse3_ = (cpu_info[2] & 0x00000200) != 0;
    has_sse41_ = (cpu_info[2] & 0x00080000) != 0;
    has_sse42_ = (cpu_info[2] & 0x00100000) != 0;
    has_popcnt_ = (cpu_info[2] & 0x00800000) != 0;
    has_osxsave_ = (cpu_info[2] & 0x08000000) != 0;
    has_avx_ = (cpu_info[2] & 0x10000000) != 0;
    has_avx2_ = (cpu_info70[1] & 0x00000020) != 0;
    has_avx_vnni_ = (cpu_info71[0] & 0x00000010) != 0;
    has_avx_vnni_int8_ = (cpu_info71[3] & 0x00000020) != 0;
    has_fma3_ = (cpu_info[2] & 0x00001000) != 0;
    has_f16c_ = (cpu_info[2] & 0x20000000) != 0;
    has_apx_f_ = (cpu_info71[3] & 0x00200000) != 0;
    // CET shadow stack feature flag. See
    // https://en.wikipedia.org/wiki/CPUID#EAX=7,_ECX=0:_Extended_Features
    has_cetss_ = (cpu_info70[2] & 0x00000080) != 0;
    // "Hypervisor Present Bit: Bit 31 of ECX of CPUID leaf 0x1."
    // See https://lwn.net/Articles/301888/
    // This is checking for any hypervisor. Hypervisors may choose not to
    // announce themselves. Hypervisors trap CPUID and sometimes return
    // different results to underlying hardware.
    is_running_in_vm_ = (cpu_info[2] & 0x80000000) != 0;

    if (family_ == 0x6) {
      switch (model_) {
        case 0x1C:  // SLT
        case 0x26:
        case 0x36:
        case 0x27:
        case 0x35:
        case 0x37:  // SLM
        case 0x4A:
        case 0x4D:
        case 0x4C:  // AMT
        case 0x6E:
          is_atom_ = true;
      }

      // CPUs that are affected by Intel JCC erratum:
      // https://www.intel.com/content/dam/support/us/en/documents/processors/mitigations-jump-conditional-code-erratum.pdf
      switch (model_) {
        case 0x4E:
          has_intel_jcc_erratum_ = stepping_ == 0x3;
          break;
        case 0x55:
          has_intel_jcc_erratum_ = stepping_ == 0x4 || stepping_ == 0x7;
          break;
        case 0x5E:
          has_intel_jcc_erratum_ = stepping_ == 0x3;
          break;
        case 0x8E:
          has_intel_jcc_erratum_ = stepping_ == 0x9 || stepping_ == 0xA ||
                                   stepping_ == 0xB || stepping_ == 0xC;
          break;
        case 0x9E:
          has_intel_jcc_erratum_ = stepping_ == 0x9 || stepping_ == 0xA ||
                                   stepping_ == 0xB || stepping_ == 0xD;
          break;
        case 0xA6:
          has_intel_jcc_erratum_ = stepping_ == 0x0;
          break;
        case 0xAE:
          has_intel_jcc_erratum_ = stepping_ == 0xA;
          break;
      }
    }
  }

  // There are separate feature flags for VEX-encoded GPR instructions.
  if (num_ids >= 7) {
    __cpuid(cpu_info, 7);
    has_bmi1_ = (cpu_info[1] & 0x00000008) != 0;
    has_bmi2_ = (cpu_info[1] & 0x00000100) != 0;
  }

  // Query extended IDs.
  __cpuid(cpu_info, 0x80000000);
  unsigned num_ext_ids = cpu_info[0];

  // Interpret extended CPU feature information.
  if (num_ext_ids > 0x80000000) {
    __cpuid(cpu_info, 0x80000001);
    has_lzcnt_ = (cpu_info[2] & 0x00000020) != 0;
    // SAHF must be probed in long mode.
    has_sahf_ = (cpu_info[2] & 0x00000001) != 0;
  }

  // Check if CPU has non stoppable time stamp counter.
  const unsigned parameter_containing_non_stop_time_stamp_counter = 0x80000007;
  if (num_ext_ids >= parameter_containing_non_stop_time_stamp_counter) {
    __cpuid(cpu_info, parameter_containing_non_stop_time_stamp_counter);
    has_non_stop_time_stamp_counter_ = (cpu_info[3] & (1 << 8)) != 0;
  }

  const unsigned virtual_physical_address_bits = 0x80000008;
  if (num_ext_ids >= virtual_physical_address_bits) {
    __cpuid(cpu_info, virtual_physical_address_bits);
    num_virtual_address_bits_ = (cpu_info[0] >> 8) & 0xff;
  }

  // This logic is replicated from cpu.cc present in chromium.src
  if (!has_non_stop_time_stamp_counter_ && is_running_in_vm_) {
    int cpu_info_hv[4] = {};
    __cpuid(cpu_info_hv, 0x40000000);
    if (cpu_info_hv[1] == 0x7263694D &&  // Micr
        cpu_info_hv[2] == 0x666F736F &&  // osof
        cpu_info_hv[3] == 0x76482074) {  // t Hv
      // If CPUID says we have a variant TSC and a hypervisor has identified
      // itself and the hypervisor says it is Microsoft Hyper-V, then treat
      // TSC as invariant.
      //
      // Microsoft Hyper-V hypervisor reports variant TSC as there are some
      // scenarios (eg. VM live migration) where the TSC is variant, but for
      // our purposes we can treat it as invariant.
      has_non_stop_time_stamp_counter_ = true;
    }
  }
#endif
}
}  // namespace v8::base
