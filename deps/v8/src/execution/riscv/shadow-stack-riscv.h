// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_RISCV_SHADOW_STACK_RISCV_H_
#define V8_EXECUTION_RISCV_SHADOW_STACK_RISCV_H_

#include "include/v8-internal.h"
#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/execution/isolate.h"
#include "src/execution/pointer-authentication.h"
#include "src/execution/simulator.h"
#include "src/flags/flags.h"

#ifndef V8_ENABLE_RISCV_SHADOW_STACK
#error "V8_ENABLE_RISCV_SHADOW_STACK should imply V8_TARGET_ARCH_RISCV"
#endif

namespace v8 {
namespace internal {

// Dummy implementation of the PointerAuthentication class methods, to be used
// when CFI is not enabled.

// Load return address from {pc_address} and return it.
V8_INLINE Address PointerAuthentication::AuthenticatePC(Address* pc_address,
                                                        unsigned) {
  return *pc_address;
}

// Return {pc} unmodified.
V8_INLINE Address PointerAuthentication::StripPAC(Address pc) { return pc; }

void RelaceShadowStack(Address* pc_address, Address new_pc, int nest);

V8_INLINE void PointerAuthentication::ReplacePC(Address* pc_address,
                                                Address new_pc, int,
                                                int num_frames_above) {
  RelaceShadowStack(pc_address, new_pc, num_frames_above);
}

// Return {pc} unmodified.
V8_INLINE Address PointerAuthentication::SignAndCheckPC(Isolate*, Address pc,
                                                        Address) {
  return pc;
}

V8_INLINE Address PointerAuthentication::MoveSignedPC(Isolate*, Address pc,
                                                      Address, Address) {
#if V8_ENABLE_WEBASSEMBLY
  // Only used by wasm deoptimizations and growable stacks.
  CHECK(v8_flags.wasm_deopt || v8_flags.experimental_wasm_growable_stacks);
  return pc;
#else
  UNREACHABLE();
#endif
}

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_RISCV_SHADOW_STACK_RISCV_H_
