// Copyright 2006-2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_CPU_CPU_HELPER_H_
#define V8_BASE_CPU_CPU_HELPER_H_

#include "src/base/base-export.h"
#include "src/base/macros.h"

namespace v8::base {

#if V8_HOST_ARCH_ARM || V8_HOST_ARCH_ARM64 || V8_HOST_ARCH_MIPS64 || \
    V8_HOST_ARCH_RISCV64 || V8_HOST_ARCH_LOONG64
#if V8_OS_LINUX

// Extract the information exposed by the kernel via /proc/cpuinfo.
class CPUInfo final {
 public:
  CPUInfo();

  ~CPUInfo() { delete[] data_; }

  // Extract the content of the first occurrence of a given field in
  // the content of the cpuinfo file and return it as a heap-allocated
  // string that must be freed by the caller using delete[].
  // Return nullptr if not found.
  char* ExtractField(const char* field) const;

 private:
  char* data_ = nullptr;
  size_t datalen_ = 0;
};

// Checks that a space-separated list of items contains one given 'item'.
bool HasListItem(const char* list, const char* item);

#endif  // V8_OS_LINUX
#endif  // V8_HOST_ARCH_ARM || V8_HOST_ARCH_ARM64 || V8_HOST_ARCH_MIPS64 ||
        // V8_HOST_ARCH_RISCV64 || V8_HOST_ARCH_LOONG64

}  // namespace v8::base

#endif  // V8_BASE_CPU_CPU_HELPER_H_
