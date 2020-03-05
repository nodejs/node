// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASELINE_LIFTOFF_COMPILER_H_
#define V8_WASM_BASELINE_LIFTOFF_COMPILER_H_

#include "src/wasm/function-compiler.h"

namespace v8 {
namespace internal {

class AccountingAllocator;
class Counters;

namespace wasm {

struct CompilationEnv;
class DebugSideTable;
struct FunctionBody;
class WasmFeatures;

// Note: If this list changes, also the histogram "V8.LiftoffBailoutReasons"
// on the chromium side needs to be updated.
// Deprecating entries is always fine. Repurposing works if you don't care about
// temporary mix-ups. Increasing the number of reasons {kNumBailoutReasons} is
// more tricky, and might require introducing a new (updated) histogram.
enum LiftoffBailoutReason : int8_t {
  // Nothing actually failed.
  kSuccess = 0,
  // Compilation failed, but not because of Liftoff.
  kDecodeError = 1,
  // Liftoff is not implemented on that architecture.
  kUnsupportedArchitecture = 2,
  // More complex code would be needed because a CPU feature is not present.
  kMissingCPUFeature = 3,
  // Liftoff does not implement a complex (and rare) instruction.
  kComplexOperation = 4,
  // Unimplemented proposals:
  kSimd = 5,
  kAnyRef = 6,
  kExceptionHandling = 7,
  kMultiValue = 8,
  kTailCall = 9,
  kAtomics = 10,
  kBulkMemory = 11,
  kNonTrappingFloatToInt = 12,
  // A little gap, for forward compatibility.
  // Any other reason (use rarely; introduce new reasons if this spikes).
  kOtherReason = 20,
  // Marker:
  kNumBailoutReasons
};

V8_EXPORT_PRIVATE WasmCompilationResult ExecuteLiftoffCompilation(
    AccountingAllocator*, CompilationEnv*, const FunctionBody&, int func_index,
    Counters*, WasmFeatures* detected_features, Vector<int> breakpoints = {});

V8_EXPORT_PRIVATE DebugSideTable GenerateLiftoffDebugSideTable(
    AccountingAllocator*, CompilationEnv*, const FunctionBody&);

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_BASELINE_LIFTOFF_COMPILER_H_
