// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// CPU specific code for arm independent of OS goes here.

#include <sys/syscall.h>
#include <unistd.h>

#include "src/codegen/cpu-features.h"

namespace v8 {
namespace internal {

void CpuFeatures::FlushICache(void* start, size_t size) {
#if !defined(USE_SIMULATOR)
  char* end = reinterpret_cast<char*>(start) + size;
  // The definition of this syscall is
  // SYSCALL_DEFINE3(riscv_flush_icache, uintptr_t, start,
  //                 uintptr_t, end, uintptr_t, flags)
  // The flag here is set to be SYS_RISCV_FLUSH_ICACHE_LOCAL, which is
  // defined as 1 in the Linux kernel.
  syscall(SYS_riscv_flush_icache, start, end, 1);
#endif  // !USE_SIMULATOR.
}

}  // namespace internal
}  // namespace v8
