// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASELINE_LIFTOFF_BAILOUT_REASONS_H_
#define V8_WASM_BASELINE_LIFTOFF_BAILOUT_REASONS_H_

#include <cstdint>

namespace v8::internal::wasm {

// Note: If this list changes, also the histogram "V8.LiftoffBailoutReasons"
// on the chromium side needs to be updated.
// Deprecating entries is always fine. Repurposing works if you don't care about
// temporary mix-ups. Increasing the number of reasons {kNumBailoutReasons} is
// more tricky, and might require introducing a new (updated) histogram.
enum LiftoffBailoutReason : int8_t {
  // Nothing actually failed.
  kNoReason = 0,
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
  kRefTypes = 6,
  kExceptionHandling = 7,
  kMultiValue = 8,
  kTailCall = 9,
  kAtomics = 10,
  kBulkMemory = 11,
  kNonTrappingFloatToInt = 12,
  kGC = 13,
  kRelaxedSimd = 14,
  kWasmfx = 15,
  // A little gap, for forward compatibility.
  // Any other reason (use rarely; introduce new reasons if this spikes).
  kOtherReason = 20,
  // Marker:
  kNumBailoutReasons
};

inline const char* LiftoffBailoutReasonToString(LiftoffBailoutReason reason) {
  switch (reason) {
    case kNoReason:
      return "no bailout";
    case kDecodeError:
      return "decode error";
    case kUnsupportedArchitecture:
      return "unsupported architecture";
    case kMissingCPUFeature:
      return "missing CPU feature";
    case kComplexOperation:
      return "complex operation";
    case kSimd:
      return "simd unsupported";
    case kRefTypes:
      return "reftypes unsupported";
    case kExceptionHandling:
      return "exception handling unsupported";
    case kMultiValue:
      return "multi-value unsupported";
    case kTailCall:
      return "tail call unsupported";
    case kAtomics:
      return "atomics unsupported";
    case kBulkMemory:
      return "bulk memory unsupported";
    case kNonTrappingFloatToInt:
      return "non-trapping float-to-int unsupported";
    case kGC:
      return "gc unsupported";
    case kRelaxedSimd:
      return "relaxed simd unsupported";
    case kWasmfx:
      return "wasmfx unsupported";
    case kOtherReason:
      return "other reason";
    case kNumBailoutReasons:
      return "num bailout reasons";
  }
}

}  // namespace v8::internal::wasm

#endif  // V8_WASM_BASELINE_LIFTOFF_BAILOUT_REASONS_H_
