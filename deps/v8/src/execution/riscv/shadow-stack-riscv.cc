// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_RISCV_SHADOW_STACK_RISCV_CC_
#define V8_EXECUTION_RISCV_SHADOW_STACK_RISCV_CC_

#include "src/execution/riscv/shadow-stack-riscv.h"

#include "include/v8-internal.h"
#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/execution/isolate-inl.h"
#include "src/execution/isolate.h"
#include "src/execution/pointer-authentication.h"
#include "src/execution/simulator.h"
#include "src/flags/flags.h"

#ifndef V8_ENABLE_RISCV_SHADOW_STACK
#error "V8_ENABLE_RISCV_SHADOW_STACK should imply V8_TARGET_ARCH_RISCV"
#endif

namespace v8 {
namespace internal {
void RelaceShadowStack(Address* pc_address, Address new_pc,
                       int num_frames_above) {
#ifdef USE_SIMULATOR
  v8::internal::Simulator* simulator =
      reinterpret_cast<v8::internal::Isolate*>(Internals::GetCurrentIsolate())
          ->CurrentPerIsolateThreadData()
          ->simulator();
  if (v8_flags.trace_shadowstack) {
    PrintF("RelaceShadowStack: old=%016lx, new=%016lx\n", *pc_address, new_pc);
  }
  auto pop_value = simulator->SwapShadowStack(static_cast<uintptr_t>(new_pc),
                                              num_frames_above - 1);
  CHECK_EQ(pop_value, *pc_address);
#else
  UNREACHABLE();
#endif
  *pc_address = new_pc;
}
}  // namespace internal
}  // namespace v8
#endif  // V8_EXECUTION_RISCV_SHADOW_STACK_RISCV_CC_
