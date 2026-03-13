// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_WASM_DEBUG_MEMORY_LOWERING_PHASE_H_
#define V8_COMPILER_TURBOSHAFT_WASM_DEBUG_MEMORY_LOWERING_PHASE_H_

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifdef DEBUG
#include "src/compiler/turboshaft/phase.h"

namespace v8::internal::compiler::turboshaft {

// Extra phase to allow running the wasm Turboshaft pipeline without
// optimizations (--no-wasm-opt) which is currently the
// MemoryOptimizationReducer. Normally this reducer runs as part of the
// WasmOptimizePhase.
struct WasmDebugMemoryLoweringPhase {
  DECL_TURBOSHAFT_PHASE_CONSTANTS(WasmDebugMemoryLowering)

  void Run(PipelineData* data, Zone* temp_zone);
};

}  // namespace v8::internal::compiler::turboshaft

#endif  // DEBUG
#endif  // V8_COMPILER_TURBOSHAFT_WASM_DEBUG_MEMORY_LOWERING_PHASE_H_
