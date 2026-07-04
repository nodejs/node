// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/cpu/cpu.h"
#include "src/base/cpu/cpu-helper.h"
#if defined(V8_OS_STARBOARD)
#include "starboard/cpu_features.h"
#endif

#if V8_LIBC_MSVCRT
#include <intrin.h>  // __cpuid()
#endif
#if V8_OS_LINUX
#include <linux/auxvec.h>  // AT_HWCAP
#endif
#if V8_OS_LINUX
#include <sys/auxv.h>  // getauxval()
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

#include "src/base/logging.h"
#include "src/base/platform/wrappers.h"
#if V8_OS_WIN
#include <windows.h>
#endif

namespace v8::base {

#if V8_HOST_ARCH_ARM || V8_HOST_ARCH_ARM64 || V8_HOST_ARCH_MIPS64 || \
    V8_HOST_ARCH_RISCV64 || V8_HOST_ARCH_LOONG64

#if V8_OS_LINUX
CPUInfo::CPUInfo() : datalen_(0) {
  // Get the size of the cpuinfo file by reading it until the end. This is
  // required because files under /proc do not always return a valid size
  // when using fseek(0, SEEK_END) + ftell(). Nor can the be mmap()-ed.
  static const char PATHNAME[] = "/proc/cpuinfo";
  FILE* fp = base::Fopen(PATHNAME, "r");
  if (fp != nullptr) {
    for (;;) {
      char buffer[256];
      size_t n = fread(buffer, 1, sizeof(buffer), fp);
      if (n == 0) {
        break;
      }
      datalen_ += n;
    }
    base::Fclose(fp);
  }

  // Read the contents of the cpuinfo file.
  data_ = new char[datalen_ + 1];
  fp = base::Fopen(PATHNAME, "r");
  if (fp != nullptr) {
    for (size_t offset = 0; offset < datalen_;) {
      size_t n = fread(data_ + offset, 1, datalen_ - offset, fp);
      if (n == 0) {
        break;
      }
      offset += n;
    }
    base::Fclose(fp);
  }

  // Zero-terminate the data.
  data_[datalen_] = '\0';
}

char* CPUInfo::ExtractField(const char* field) const {
  DCHECK_NOT_NULL(field);

  // Look for first field occurrence, and ensure it starts the line.
  size_t fieldlen = strlen(field);
  char* p = data_;
  for (;;) {
    p = strstr(p, field);
    if (p == nullptr) {
      return nullptr;
    }
    if (p == data_ || p[-1] == '\n') {
      break;
    }
    p += fieldlen;
  }

  // Skip to the first colon followed by a space.
  p = strchr(p + fieldlen, ':');
  if (p == nullptr || !isspace(p[1])) {
    return nullptr;
  }
  p += 2;

  // Find the end of the line.
  char* q = strchr(p, '\n');
  if (q == nullptr) {
    q = data_ + datalen_;
  }

  // Copy the line into a heap-allocated buffer.
  size_t len = q - p;
  char* result = new char[len + 1];
  if (result != nullptr) {
    memcpy(result, p, len);
    result[len] = '\0';
  }
  return result;
}

// Checks whether the given item appears in a list of items separated by
// characters for which isseparator returns true.
template <typename Predicate>
bool HasListItemImpl(const char* list, const char* item,
                     Predicate isseparator) {
  ssize_t item_len = strlen(item);
  const char* p = list;
  if (p != nullptr) {
    // Skip whitespace.
    while (isspace(*p)) ++p;

    while (*p != '\0') {
      // Skip separator.
      while (isseparator(*p)) ++p;

      // Find end of current list item.
      const char* q = p;
      while (*q != '\0' && !isseparator(*q)) ++q;

      if (item_len == q - p && memcmp(p, item, item_len) == 0) {
        return true;
      }

      // Skip to next item.
      p = q;
    }
  }
  return false;
}

bool HasListItem(const char* list, const char* item) {
  return HasListItemImpl(list, item, isspace);
}

#endif  // V8_OS_LINUX

#endif  // V8_HOST_ARCH_ARM || V8_HOST_ARCH_ARM64 ||
        // V8_HOST_ARCH_MIPS64 || V8_HOST_ARCH_RISCV64 ||
        // V8_HOST_ARCH_LOONG64

#if defined(V8_OS_STARBOARD)

bool CPU::StarboardDetectCPU() {
  SbCPUFeatures features;
  if (!SbCPUFeaturesGet(&features)) {
    return false;
  }
  architecture_ = features.arm.architecture_generation;
  switch (features.architecture) {
    case kSbCPUFeaturesArchitectureArm:
    case kSbCPUFeaturesArchitectureArm64:
      has_neon_ = features.arm.has_neon;
      has_thumb2_ = features.arm.has_thumb2;
      has_vfp_ = features.arm.has_vfp;
      has_vfp3_ = features.arm.has_vfp3;
      has_vfp3_d32_ = features.arm.has_vfp3_d32;
      has_idiva_ = features.arm.has_idiva;
      break;
    case kSbCPUFeaturesArchitectureX86:
    case kSbCPUFeaturesArchitectureX86_64:
      // Following flags are mandatory for V8
      has_cmov_ = features.x86.has_cmov;
      has_sse2_ = features.x86.has_sse2;
      // These flags are optional
      has_sse3_ = features.x86.has_sse3;
      has_ssse3_ = features.x86.has_ssse3;
      has_sse41_ = features.x86.has_sse41;
      has_sahf_ = features.x86.has_sahf;
      has_avx_ = features.x86.has_avx;
      has_avx2_ = features.x86.has_avx2;
      // TODO: Support AVX-VNNI on Starboard
      has_fma3_ = features.x86.has_fma3;
      has_bmi1_ = features.x86.has_bmi1;
      has_bmi2_ = features.x86.has_bmi2;
      has_lzcnt_ = features.x86.has_lzcnt;
      has_popcnt_ = features.x86.has_popcnt;
      has_f16c_ = features.x86.has_f16c;
      // TODO(jiepan): Support APX_F on STARBOARD
      break;
    default:
      return false;
  }

  return true;
}

#endif

CPU::CPU() {
  memcpy(vendor_, "Unknown", 8);

#if defined(V8_OS_STARBOARD)
  if (StarboardDetectCPU()) {
    return;
  }
#endif

  DetectFeatures();
}

}  // namespace v8::base
