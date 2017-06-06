// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/cpu.h"

#if V8_LIBC_MSVCRT
#include <intrin.h>  // __cpuid()
#endif
#if V8_OS_LINUX
#include <linux/auxvec.h>  // AT_HWCAP
#endif
#if V8_GLIBC_PREREQ(2, 16)
#include <sys/auxv.h>  // getauxval()
#endif
#if V8_OS_QNX
#include <sys/syspage.h>  // cpuinfo
#endif
#if V8_OS_LINUX && V8_HOST_ARCH_PPC
#include <elf.h>
#endif
#if V8_OS_AIX
#include <sys/systemcfg.h>  // _system_configuration
#ifndef POWER_8
#define POWER_8 0x10000
#endif
#ifndef POWER_9
#define POWER_9 0x20000
#endif
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

#include "src/base/logging.h"
#if V8_OS_WIN
#include "src/base/win32-headers.h"  // NOLINT
#endif

namespace v8 {
namespace base {

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

#endif  // !V8_LIBC_MSVCRT

#elif V8_HOST_ARCH_ARM || V8_HOST_ARCH_ARM64 \
    || V8_HOST_ARCH_MIPS || V8_HOST_ARCH_MIPS64

#if V8_OS_LINUX

#if V8_HOST_ARCH_ARM

// See <uapi/asm/hwcap.h> kernel header.
/*
 * HWCAP flags - for elf_hwcap (in kernel) and AT_HWCAP
 */
#define HWCAP_SWP (1 << 0)
#define HWCAP_HALF  (1 << 1)
#define HWCAP_THUMB (1 << 2)
#define HWCAP_26BIT (1 << 3)  /* Play it safe */
#define HWCAP_FAST_MULT (1 << 4)
#define HWCAP_FPA (1 << 5)
#define HWCAP_VFP (1 << 6)
#define HWCAP_EDSP  (1 << 7)
#define HWCAP_JAVA  (1 << 8)
#define HWCAP_IWMMXT  (1 << 9)
#define HWCAP_CRUNCH  (1 << 10)
#define HWCAP_THUMBEE (1 << 11)
#define HWCAP_NEON  (1 << 12)
#define HWCAP_VFPv3 (1 << 13)
#define HWCAP_VFPv3D16  (1 << 14) /* also set for VFPv4-D16 */
#define HWCAP_TLS (1 << 15)
#define HWCAP_VFPv4 (1 << 16)
#define HWCAP_IDIVA (1 << 17)
#define HWCAP_IDIVT (1 << 18)
#define HWCAP_VFPD32  (1 << 19) /* set if VFP has 32 regs (not 16) */
#define HWCAP_IDIV  (HWCAP_IDIVA | HWCAP_IDIVT)
#define HWCAP_LPAE  (1 << 20)

static uint32_t ReadELFHWCaps() {
  uint32_t result = 0;
#if V8_GLIBC_PREREQ(2, 16)
  result = static_cast<uint32_t>(getauxval(AT_HWCAP));
#else
  // Read the ELF HWCAP flags by parsing /proc/self/auxv.
  FILE* fp = fopen("/proc/self/auxv", "r");
  if (fp != NULL) {
    struct { uint32_t tag; uint32_t value; } entry;
    for (;;) {
      size_t n = fread(&entry, sizeof(entry), 1, fp);
      if (n == 0 || (entry.tag == 0 && entry.value == 0)) {
        break;
      }
      if (entry.tag == AT_HWCAP) {
        result = entry.value;
        break;
      }
    }
    fclose(fp);
  }
#endif
  return result;
}

#endif  // V8_HOST_ARCH_ARM

#if V8_HOST_ARCH_MIPS
int __detect_fp64_mode(void) {
  double result = 0;
  // Bit representation of (double)1 is 0x3FF0000000000000.
  __asm__ volatile(
      ".set push\n\t"
      ".set noreorder\n\t"
      ".set oddspreg\n\t"
      "lui $t0, 0x3FF0\n\t"
      "ldc1 $f0, %0\n\t"
      "mtc1 $t0, $f1\n\t"
      "sdc1 $f0, %0\n\t"
      ".set pop\n\t"
      : "+m"(result)
      :
      : "t0", "$f0", "$f1", "memory");

  return !(result == 1);
}


int __detect_mips_arch_revision(void) {
  // TODO(dusmil): Do the specific syscall as soon as it is implemented in mips
  // kernel.
  uint32_t result = 0;
  __asm__ volatile(
      "move $v0, $zero\n\t"
      // Encoding for "addi $v0, $v0, 1" on non-r6,
      // which is encoding for "bovc $v0, %v0, 1" on r6.
      // Use machine code directly to avoid compilation errors with different
      // toolchains and maintain compatibility.
      ".word 0x20420001\n\t"
      "sw $v0, %0\n\t"
      : "=m"(result)
      :
      : "v0", "memory");
  // Result is 0 on r6 architectures, 1 on other architecture revisions.
  // Fall-back to the least common denominator which is mips32 revision 1.
  return result ? 1 : 6;
}
#endif

// Extract the information exposed by the kernel via /proc/cpuinfo.
class CPUInfo final {
 public:
  CPUInfo() : datalen_(0) {
    // Get the size of the cpuinfo file by reading it until the end. This is
    // required because files under /proc do not always return a valid size
    // when using fseek(0, SEEK_END) + ftell(). Nor can the be mmap()-ed.
    static const char PATHNAME[] = "/proc/cpuinfo";
    FILE* fp = fopen(PATHNAME, "r");
    if (fp != NULL) {
      for (;;) {
        char buffer[256];
        size_t n = fread(buffer, 1, sizeof(buffer), fp);
        if (n == 0) {
          break;
        }
        datalen_ += n;
      }
      fclose(fp);
    }

    // Read the contents of the cpuinfo file.
    data_ = new char[datalen_ + 1];
    fp = fopen(PATHNAME, "r");
    if (fp != NULL) {
      for (size_t offset = 0; offset < datalen_; ) {
        size_t n = fread(data_ + offset, 1, datalen_ - offset, fp);
        if (n == 0) {
          break;
        }
        offset += n;
      }
      fclose(fp);
    }

    // Zero-terminate the data.
    data_[datalen_] = '\0';
  }

  ~CPUInfo() {
    delete[] data_;
  }

  // Extract the content of a the first occurence of a given field in
  // the content of the cpuinfo file and return it as a heap-allocated
  // string that must be freed by the caller using delete[].
  // Return NULL if not found.
  char* ExtractField(const char* field) const {
    DCHECK(field != NULL);

    // Look for first field occurence, and ensure it starts the line.
    size_t fieldlen = strlen(field);
    char* p = data_;
    for (;;) {
      p = strstr(p, field);
      if (p == NULL) {
        return NULL;
      }
      if (p == data_ || p[-1] == '\n') {
        break;
      }
      p += fieldlen;
    }

    // Skip to the first colon followed by a space.
    p = strchr(p + fieldlen, ':');
    if (p == NULL || !isspace(p[1])) {
      return NULL;
    }
    p += 2;

    // Find the end of the line.
    char* q = strchr(p, '\n');
    if (q == NULL) {
      q = data_ + datalen_;
    }

    // Copy the line into a heap-allocated buffer.
    size_t len = q - p;
    char* result = new char[len + 1];
    if (result != NULL) {
      memcpy(result, p, len);
      result[len] = '\0';
    }
    return result;
  }

 private:
  char* data_;
  size_t datalen_;
};

#if V8_HOST_ARCH_ARM || V8_HOST_ARCH_MIPS || V8_HOST_ARCH_MIPS64

// Checks that a space-separated list of items contains one given 'item'.
static bool HasListItem(const char* list, const char* item) {
  ssize_t item_len = strlen(item);
  const char* p = list;
  if (p != NULL) {
    while (*p != '\0') {
      // Skip whitespace.
      while (isspace(*p)) ++p;

      // Find end of current list item.
      const char* q = p;
      while (*q != '\0' && !isspace(*q)) ++q;

      if (item_len == q - p && memcmp(p, item, item_len) == 0) {
        return true;
      }

      // Skip to next item.
      p = q;
    }
  }
  return false;
}

#endif  // V8_HOST_ARCH_ARM || V8_HOST_ARCH_MIPS || V8_HOST_ARCH_MIPS64

#endif  // V8_OS_LINUX

#endif  // V8_HOST_ARCH_IA32 || V8_HOST_ARCH_X64

CPU::CPU()
    : stepping_(0),
      model_(0),
      ext_model_(0),
      family_(0),
      ext_family_(0),
      type_(0),
      implementer_(0),
      architecture_(0),
      variant_(-1),
      part_(0),
      icache_line_size_(UNKNOWN_CACHE_LINE_SIZE),
      dcache_line_size_(UNKNOWN_CACHE_LINE_SIZE),
      has_fpu_(false),
      has_cmov_(false),
      has_sahf_(false),
      has_mmx_(false),
      has_sse_(false),
      has_sse2_(false),
      has_sse3_(false),
      has_ssse3_(false),
      has_sse41_(false),
      has_sse42_(false),
      is_atom_(false),
      has_osxsave_(false),
      has_avx_(false),
      has_fma3_(false),
      has_bmi1_(false),
      has_bmi2_(false),
      has_lzcnt_(false),
      has_popcnt_(false),
      has_idiva_(false),
      has_neon_(false),
      has_thumb2_(false),
      has_vfp_(false),
      has_vfp3_(false),
      has_vfp3_d32_(false),
      is_fp64_mode_(false),
      has_non_stop_time_stamp_counter_(false) {
  memcpy(vendor_, "Unknown", 8);
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
    stepping_ = cpu_info[0] & 0xf;
    model_ = ((cpu_info[0] >> 4) & 0xf) + ((cpu_info[0] >> 12) & 0xf0);
    family_ = (cpu_info[0] >> 8) & 0xf;
    type_ = (cpu_info[0] >> 12) & 0x3;
    ext_model_ = (cpu_info[0] >> 16) & 0xf;
    ext_family_ = (cpu_info[0] >> 20) & 0xff;
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
    has_fma3_ = (cpu_info[2] & 0x00001000) != 0;

    if (family_ == 0x6) {
      switch (model_) {
        case 0x1c:  // SLT
        case 0x26:
        case 0x36:
        case 0x27:
        case 0x35:
        case 0x37:  // SLM
        case 0x4a:
        case 0x4d:
        case 0x4c:  // AMT
        case 0x6e:
          is_atom_ = true;
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

#elif V8_HOST_ARCH_ARM

#if V8_OS_LINUX

  CPUInfo cpu_info;

  // Extract implementor from the "CPU implementer" field.
  char* implementer = cpu_info.ExtractField("CPU implementer");
  if (implementer != NULL) {
    char* end;
    implementer_ = strtol(implementer, &end, 0);
    if (end == implementer) {
      implementer_ = 0;
    }
    delete[] implementer;
  }

  char* variant = cpu_info.ExtractField("CPU variant");
  if (variant != NULL) {
    char* end;
    variant_ = strtol(variant, &end, 0);
    if (end == variant) {
      variant_ = -1;
    }
    delete[] variant;
  }

  // Extract part number from the "CPU part" field.
  char* part = cpu_info.ExtractField("CPU part");
  if (part != NULL) {
    char* end;
    part_ = strtol(part, &end, 0);
    if (end == part) {
      part_ = 0;
    }
    delete[] part;
  }

  // Extract architecture from the "CPU Architecture" field.
  // The list is well-known, unlike the the output of
  // the 'Processor' field which can vary greatly.
  // See the definition of the 'proc_arch' array in
  // $KERNEL/arch/arm/kernel/setup.c and the 'c_show' function in
  // same file.
  char* architecture = cpu_info.ExtractField("CPU architecture");
  if (architecture != NULL) {
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
  uint32_t hwcaps = ReadELFHWCaps();
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
  DCHECK(architecture_ >= 6);
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

#elif V8_HOST_ARCH_MIPS || V8_HOST_ARCH_MIPS64

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
#ifdef V8_HOST_ARCH_MIPS
  is_fp64_mode_ = __detect_fp64_mode();
  architecture_ = __detect_mips_arch_revision();
#endif

#elif V8_HOST_ARCH_ARM64

  CPUInfo cpu_info;

  // Extract implementor from the "CPU implementer" field.
  char* implementer = cpu_info.ExtractField("CPU implementer");
  if (implementer != NULL) {
    char* end;
    implementer_ = static_cast<int>(strtol(implementer, &end, 0));
    if (end == implementer) {
      implementer_ = 0;
    }
    delete[] implementer;
  }

  char* variant = cpu_info.ExtractField("CPU variant");
  if (variant != NULL) {
    char* end;
    variant_ = static_cast<int>(strtol(variant, &end, 0));
    if (end == variant) {
      variant_ = -1;
    }
    delete[] variant;
  }

  // Extract part number from the "CPU part" field.
  char* part = cpu_info.ExtractField("CPU part");
  if (part != NULL) {
    char* end;
    part_ = static_cast<int>(strtol(part, &end, 0));
    if (end == part) {
      part_ = 0;
    }
    delete[] part;
  }

#elif V8_HOST_ARCH_PPC

#ifndef USE_SIMULATOR
#if V8_OS_LINUX
  // Read processor info from /proc/self/auxv.
  char* auxv_cpu_type = NULL;
  FILE* fp = fopen("/proc/self/auxv", "r");
  if (fp != NULL) {
#if V8_TARGET_ARCH_PPC64
    Elf64_auxv_t entry;
#else
    Elf32_auxv_t entry;
#endif
    for (;;) {
      size_t n = fread(&entry, sizeof(entry), 1, fp);
      if (n == 0 || entry.a_type == AT_NULL) {
        break;
      }
      switch (entry.a_type) {
        case AT_PLATFORM:
          auxv_cpu_type = reinterpret_cast<char*>(entry.a_un.a_val);
          break;
        case AT_ICACHEBSIZE:
          icache_line_size_ = entry.a_un.a_val;
          break;
        case AT_DCACHEBSIZE:
          dcache_line_size_ = entry.a_un.a_val;
          break;
      }
    }
    fclose(fp);
  }

  part_ = -1;
  if (auxv_cpu_type) {
    if (strcmp(auxv_cpu_type, "power9") == 0) {
      part_ = PPC_POWER9;
    } else if (strcmp(auxv_cpu_type, "power8") == 0) {
      part_ = PPC_POWER8;
    } else if (strcmp(auxv_cpu_type, "power7") == 0) {
      part_ = PPC_POWER7;
    } else if (strcmp(auxv_cpu_type, "power6") == 0) {
      part_ = PPC_POWER6;
    } else if (strcmp(auxv_cpu_type, "power5") == 0) {
      part_ = PPC_POWER5;
    } else if (strcmp(auxv_cpu_type, "ppc970") == 0) {
      part_ = PPC_G5;
    } else if (strcmp(auxv_cpu_type, "ppc7450") == 0) {
      part_ = PPC_G4;
    } else if (strcmp(auxv_cpu_type, "pa6t") == 0) {
      part_ = PPC_PA6T;
    }
  }

#elif V8_OS_AIX
  switch (_system_configuration.implementation) {
    case POWER_9:
      part_ = PPC_POWER9;
      break;
    case POWER_8:
      part_ = PPC_POWER8;
      break;
    case POWER_7:
      part_ = PPC_POWER7;
      break;
    case POWER_6:
      part_ = PPC_POWER6;
      break;
    case POWER_5:
      part_ = PPC_POWER5;
      break;
  }
#endif  // V8_OS_AIX
#endif  // !USE_SIMULATOR
#endif  // V8_HOST_ARCH_PPC
}

}  // namespace base
}  // namespace v8
