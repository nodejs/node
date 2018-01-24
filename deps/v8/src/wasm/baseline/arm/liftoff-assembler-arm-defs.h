// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASELINE_LIFTOFF_ASSEMBLER_ARM_DEFS_H_
#define V8_WASM_BASELINE_LIFTOFF_ASSEMBLER_ARM_DEFS_H_

#include "src/reglist.h"

namespace v8 {
namespace internal {
namespace wasm {

// TODO(clemensh): Implement the LiftoffAssembler on this platform.
static constexpr bool kLiftoffAssemblerImplementedOnThisPlatform = false;

static constexpr RegList kLiftoffAssemblerGpCacheRegs = 0xff;

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_BASELINE_LIFTOFF_ASSEMBLER_ARM_DEFS_H_
