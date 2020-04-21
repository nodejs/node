// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_TIER_H_
#define V8_WASM_WASM_TIER_H_

#include <cstdint>

namespace v8 {
namespace internal {
namespace wasm {

// All the tiers of Wasm execution.
enum class ExecutionTier : int8_t {
  kNone,
  kInterpreter,
  kLiftoff,
  kTurbofan,
};

inline const char* ExecutionTierToString(ExecutionTier tier) {
  switch (tier) {
    case ExecutionTier::kTurbofan:
      return "turbofan";
    case ExecutionTier::kLiftoff:
      return "liftoff";
    case ExecutionTier::kInterpreter:
      return "interpreter";
    case ExecutionTier::kNone:
      return "none";
  }
}

// {kForDebugging} is used for default tiered-down code (potentially with
// breakpoints), {kForStepping} is code that is flooded with breakpoints.
enum ForDebugging : int8_t { kNoDebugging = 0, kForDebugging, kForStepping };

enum TieringState : int8_t { kTieredUp, kTieredDown };

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_TIER_H_
