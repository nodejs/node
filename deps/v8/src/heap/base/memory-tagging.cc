// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/base/memory-tagging.h"

#include "src/base/cpu/cpu.h"
#include "src/base/logging.h"
#include "v8config.h"

#if defined(V8_OS_LINUX) && defined(V8_HOST_ARCH_ARM64)
#define V8_HAS_MTE_SUPPORT
#endif

namespace heap::base {

SuspendTagCheckingScope::SuspendTagCheckingScope() noexcept {
#ifdef V8_HAS_MTE_SUPPORT
  if (v8::base::CPU::GetInstance().has_mte()) [[unlikely]] {
    uint64_t val;
    // Do a test to see if anything else has interfered with TCO.
    // We expect TCO to be unset here.
    asm volatile(".arch_extension memtag \n mrs %0, tco" : "=r"(val));
    CHECK_EQ(val, 0);

    // Suspend tag checks via PSTATE.TCO.
    asm volatile(".arch_extension memtag \n msr tco, #1" ::: "memory");
  }
#endif
}

SuspendTagCheckingScope::~SuspendTagCheckingScope() {
#ifdef V8_HAS_MTE_SUPPORT
  if (v8::base::CPU::GetInstance().has_mte()) [[unlikely]] {
    uint64_t val;
    // Do a test to see if anything else has interfered with TCO.
    // We expect TCO to be set here.
    asm volatile(".arch_extension memtag \n mrs %0, tco" : "=r"(val));
    CHECK_EQ(val, 1u << 25);

    asm volatile(".arch_extension memtag \n msr tco, #0" ::: "memory");
  }
#endif
}

}  // namespace heap::base
