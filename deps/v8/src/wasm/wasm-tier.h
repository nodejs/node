// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_WASM_WASM_TIER_H_
#define V8_WASM_WASM_TIER_H_

#include <cstdint>

namespace v8 {
namespace internal {
namespace wasm {

// All the tiers of Wasm execution.
enum class ExecutionTier : int8_t {
  kNone,
  kLiftoff,
  kTurbofan,
};

inline const char* ExecutionTierToString(ExecutionTier tier) {
  switch (tier) {
    case ExecutionTier::kTurbofan:
      return "turbofan";
    case ExecutionTier::kLiftoff:
      return "liftoff";
    case ExecutionTier::kNone:
      return "none";
  }
}

// {kForDebugging} is used for default tiered-down code, {kWithBreakpoints} if
// the code also contains breakpoints, and {kForStepping} for code that is
// flooded with breakpoints.
enum ForDebugging : int8_t {
  kNoDebugging = 0,
  kForDebugging,
  kWithBreakpoints,
  kForStepping
};

enum TieringState : int8_t { kTieredUp, kTieredDown };

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_TIER_H_
