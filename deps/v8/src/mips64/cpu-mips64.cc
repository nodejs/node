// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// CPU specific code for arm independent of OS goes here.

#include <sys/syscall.h>
#include <unistd.h>

#ifdef __mips
#include <asm/cachectl.h>
#endif  // #ifdef __mips

#include "src/v8.h"

#if V8_TARGET_ARCH_MIPS64

#include "src/assembler.h"
#include "src/macro-assembler.h"

#include "src/simulator.h"  // For cache flushing.

namespace v8 {
namespace internal {


void CpuFeatures::FlushICache(void* start, size_t size) {
  // Nothing to do, flushing no instructions.
  if (size == 0) {
    return;
  }

#if !defined (USE_SIMULATOR)
#if defined(ANDROID) && !defined(__LP64__)
  // Bionic cacheflush can typically run in userland, avoiding kernel call.
  char *end = reinterpret_cast<char *>(start) + size;
  cacheflush(
    reinterpret_cast<intptr_t>(start), reinterpret_cast<intptr_t>(end), 0);
#else  // ANDROID
  int res;
  // See http://www.linux-mips.org/wiki/Cacheflush_Syscall.
  res = syscall(__NR_cacheflush, start, size, ICACHE);
  if (res) {
    V8_Fatal(__FILE__, __LINE__, "Failed to flush the instruction cache");
  }
#endif  // ANDROID
#else  // USE_SIMULATOR.
  // Not generating mips instructions for C-code. This means that we are
  // building a mips emulator based target.  We should notify the simulator
  // that the Icache was flushed.
  // None of this code ends up in the snapshot so there are no issues
  // around whether or not to generate the code when building snapshots.
  Simulator::FlushICache(Isolate::Current()->simulator_i_cache(), start, size);
#endif  // USE_SIMULATOR.
}

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_MIPS64
