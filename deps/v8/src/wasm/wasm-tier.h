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

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_TIER_H_
