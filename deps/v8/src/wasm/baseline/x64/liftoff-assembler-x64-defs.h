// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASELINE_LIFTOFF_ASSEMBLER_X64_DEFS_H_
#define V8_WASM_BASELINE_LIFTOFF_ASSEMBLER_X64_DEFS_H_

#include "src/reglist.h"

namespace v8 {
namespace internal {
namespace wasm {

static constexpr bool kLiftoffAssemblerImplementedOnThisPlatform = true;

static constexpr RegList kLiftoffAssemblerGpCacheRegs =
    Register::ListOf<rax, rcx, rdx, rbx, rsi, rdi>();

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_BASELINE_LIFTOFF_ASSEMBLER_X64_DEFS_H_
