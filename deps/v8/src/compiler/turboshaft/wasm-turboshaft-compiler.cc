// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/wasm-turboshaft-compiler.h"

#include "src/codegen/optimized-compilation-info.h"
#include "src/compiler/pipeline.h"
// TODO(14108): Remove.
#include "src/compiler/wasm-compiler.h"

namespace v8::internal::compiler::turboshaft {

wasm::WasmCompilationResult ExecuteTurboshaftWasmCompilation(
    wasm::CompilationEnv* env, compiler::WasmCompilationData& data,
    wasm::WasmDetectedFeatures* detected,
    DelayedCounterUpdates* counter_updates) {
  wasm::WasmCompilationResult result =
      Pipeline::GenerateWasmCode(env, data, detected, counter_updates);
  DCHECK(result.succeeded());
  DCHECK_EQ(wasm::ExecutionTier::kTurbofan, result.result_tier);
  DCHECK_NULL(result.assumptions);
  result.assumptions = std::move(data.assumptions);
  DCHECK_IMPLIES(result.assumptions, !result.assumptions->empty());
  return result;
}

}  // namespace v8::internal::compiler::turboshaft
