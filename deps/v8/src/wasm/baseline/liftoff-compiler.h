// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASELINE_LIFTOFF_COMPILER_H_
#define V8_WASM_BASELINE_LIFTOFF_COMPILER_H_

#include "src/wasm/baseline/liftoff-bailout-reasons.h"
#include "src/wasm/function-compiler.h"

namespace v8 {
namespace internal {

class AccountingAllocator;
class Counters;

namespace wasm {

struct CompilationEnv;
class DebugSideTable;
struct FunctionBody;
class WasmDetectedFeatures;

// Further information about a location for a deopt: A call_ref can either be
// just an inline call (that didn't cause a deopt) with a deopt happening within
// the inlinee or it could be the deopt point itself. This changes whether the
// relevant stackstate is the one before the call or after the call.
enum class LocationKindForDeopt : uint8_t {
  kNone,
  kEagerDeopt,   // The location is the point of an eager deopt.
  kInlinedCall,  // The loation is an inlined call, not a deopt.
};

struct LiftoffOptions {
  // func_index is non-optional. Make it const with no default value to force
  // assigning a value to this field on aggregate initialization.
  const int func_index;

  ForDebugging for_debugging = kNotForDebugging;
  DelayedCounterUpdates* counter_updates = nullptr;
  WasmDetectedFeatures* detected_features = nullptr;
  base::Vector<const int> breakpoints = {};
  std::unique_ptr<DebugSideTable>* debug_sidetable = nullptr;
  int dead_breakpoint = 0;
  int32_t* max_steps = nullptr;
  uint32_t deopt_info_bytecode_offset = std::numeric_limits<uint32_t>::max();
  LocationKindForDeopt deopt_location_kind = LocationKindForDeopt::kNone;
};

V8_EXPORT_PRIVATE WasmCompilationResult ExecuteLiftoffCompilation(
    CompilationEnv*, const FunctionBody&, const LiftoffOptions&);

V8_EXPORT_PRIVATE std::unique_ptr<DebugSideTable> GenerateLiftoffDebugSideTable(
    const WasmCode*);

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_BASELINE_LIFTOFF_COMPILER_H_
