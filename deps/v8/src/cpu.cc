// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cpu.h"

#if V8_LIBC_MSVCRT
#include <intrin.h>  // __cpuid()
#endif
#if V8_OS_POSIX
#include <unistd.h>  // sysconf()
#endif
#if V8_OS_QNX
#include <sys/syspage.h>  // cpuinfo
#endif

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>

#include "checks.h"
#if V8_OS_WIN
#include "win32-headers.h"
#endif

namespace v8 {
namespace internal {

#if V8_HOST_ARCH_IA32 || V8_HOST_ARCH_X64

// Define __cpuid() for non-MSVC libraries.
#if !V8_LIBC_MSVCRT

static V8_INLINE void __cpuid(int cpu_info[4], int info_type) {
#if defined(__i386__) && defined(__pic__)
  // Make sure to preserve ebx, which contains the pointer
  // to the GOT in case we're generating PIC.
  __asm__ volatile (
    "mov %%ebx, %%edi\n\t"
    "cpuid\n\t"
    "xchg %%edi, %%ebx\n\t"
    : "=a"(cpu_info[0]), "=D"(cpu_info[1]), "=c"(cpu_info[2]), "=d"(cpu_info[3])
    : "a"(info_type)
  );
#else
  __asm__ volatile (
    "cpuid \n\t"
    : "=a"(cpu_info[0]), "=b"(cpu_info[1]), "=c"(cpu_info[2]), "=d"(cpu_info[3])
    : "a"(info_type)
  );
#endif  // defined(__i386__) && defined(__pic__)
}

#endif  // !V8_LIBC_MSVCRT

#elif V8_HOST_ARCH_ARM || V8_HOST_ARCH_MIPS

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

#define AT_HWCAP 16

// Read the ELF HWCAP flags by parsing /proc/self/auxv.
static uint32_t ReadELFHWCaps() {
  uint32_t result = 0;
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
  return result;
}

#endif  // V8_HOST_ARCH_ARM

// Extract the information exposed by the kernel via /proc/cpuinfo.
class CPUInfo V8_FINAL BASE_EMBEDDED {
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
    ASSERT(field != NULL);

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

#endif  // V8_OS_LINUX

#endif  // V8_HOST_ARCH_IA32 || V8_HOST_ARCH_X64

CPU::CPU() : stepping_(0),
             model_(0),
             ext_model_(0),
             family_(0),
             ext_family_(0),
             type_(0),
             implementer_(0),
             architecture_(0),
             part_(0),
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
             has_idiva_(false),
             has_neon_(false),
             has_thumbee_(false),
             has_vfp_(false),
             has_vfp3_(false),
             has_vfp3_d32_(false) {
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
  }

  // Query extended IDs.
  __cpuid(cpu_info, 0x80000000);
  unsigned num_ext_ids = cpu_info[0];

  // Interpret extended CPU feature information.
  if (num_ext_ids > 0x80000000) {
    __cpuid(cpu_info, 0x80000001);
    // SAHF is always available in compat/legacy mode,
    // but must be probed in long mode.
#if V8_HOST_ARCH_IA32
    has_sahf_ = true;
#else
    has_sahf_ = (cpu_info[2] & 0x00000001) != 0;
#endif
  }

#elif V8_HOST_ARCH_ARM

#if V8_OS_LINUX

  CPUInfo cpu_info;

  // Extract implementor from the "CPU implementer" field.
  char* implementer = cpu_info.ExtractField("CPU implementer");
  if (implementer != NULL) {
    char* end ;
    implementer_ = strtol(implementer, &end, 0);
    if (end == implementer) {
      implementer_ = 0;
    }
    delete[] implementer;
  }

  // Extract part number from the "CPU part" field.
  char* part = cpu_info.ExtractField("CPU part");
  if (part != NULL) {
    char* end ;
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
      architecture_ = 0;
    }
    delete[] architecture;

    // Unfortunately, it seems that certain ARMv6-based CPUs
    // report an incorrect architecture number of 7!
    //
    // See http://code.google.com/p/android/issues/detail?id=10812
    //
    // We try to correct this by looking at the 'elf_format'
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
  }

  // Try to extract the list of CPU features from ELF hwcaps.
  uint32_t hwcaps = ReadELFHWCaps();
  if (hwcaps != 0) {
    has_idiva_ = (hwcaps & HWCAP_IDIVA) != 0;
    has_neon_ = (hwcaps & HWCAP_NEON) != 0;
    has_thumbee_ = (hwcaps & HWCAP_THUMBEE) != 0;
    has_vfp_ = (hwcaps & HWCAP_VFP) != 0;
    has_vfp3_ = (hwcaps & (HWCAP_VFPv3 | HWCAP_VFPv3D16 | HWCAP_VFPv4)) != 0;
    has_vfp3_d32_ = (has_vfp3_ && ((hwcaps & HWCAP_VFPv3D16) == 0 ||
                                   (hwcaps & HWCAP_VFPD32) != 0));
  } else {
    // Try to fallback to "Features" CPUInfo field.
    char* features = cpu_info.ExtractField("Features");
    has_idiva_ = HasListItem(features, "idiva");
    has_neon_ = HasListItem(features, "neon");
    has_thumbee_ = HasListItem(features, "thumbee");
    has_vfp_ = HasListItem(features, "vfp");
    if (HasListItem(features, "vfpv3")) {
      has_vfp3_ = true;
      has_vfp3_d32_ = true;
    } else if (HasListItem(features, "vfpv3d16")) {
      has_vfp3_ = true;
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

  // ARMv7 implies ThumbEE.
  if (architecture_ >= 7) {
    has_thumbee_ = true;
  }

  // The earliest architecture with ThumbEE is ARMv6T2.
  if (has_thumbee_ && architecture_ < 6) {
    architecture_ = 6;
  }

  // We don't support any FPUs other than VFP.
  has_fpu_ = has_vfp_;

#elif V8_OS_QNX

  uint32_t cpu_flags = SYSPAGE_ENTRY(cpuinfo)->flags;
  if (cpu_flags & ARM_CPU_FLAG_V7) {
    architecture_ = 7;
    has_thumbee_ = true;
  } else if (cpu_flags & ARM_CPU_FLAG_V6) {
    architecture_ = 6;
    // QNX doesn't say if ThumbEE is available.
    // Assume false for the architectures older than ARMv7.
  }
  ASSERT(architecture_ >= 6);
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

#elif V8_HOST_ARCH_MIPS

  // Simple detection of FPU at runtime for Linux.
  // It is based on /proc/cpuinfo, which reveals hardware configuration
  // to user-space applications.  According to MIPS (early 2010), no similar
  // facility is universally available on the MIPS architectures,
  // so it's up to individual OSes to provide such.
  CPUInfo cpu_info;
  char* cpu_model = cpu_info.ExtractField("cpu model");
  has_fpu_ = HasListItem(cpu_model, "FPU");
  delete[] cpu_model;

#endif
}


// static
int CPU::NumberOfProcessorsOnline() {
#if V8_OS_WIN
  SYSTEM_INFO info;
  GetSystemInfo(&info);
  return info.dwNumberOfProcessors;
#else
  return static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
#endif
}

} }  // namespace v8::internal
