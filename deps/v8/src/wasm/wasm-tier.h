// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_TIER_H_
#define V8_WASM_WASM_TIER_H_

#include <cstdint>

namespace v8 {
namespace internal {
namespace wasm {

// All the tiers of WASM execution.
enum class ExecutionTier : int8_t {
  kInterpreter,  // interpreter (used to provide debugging services).
  kBaseline,     // Liftoff.
  kOptimized     // TurboFan.
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_TIER_H_
