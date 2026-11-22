// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/decompression-optimization-phase.h"

#include "src/compiler/linkage.h"
#include "src/compiler/turboshaft/decompression-optimization.h"

namespace v8::internal::compiler::turboshaft {

void DecompressionOptimizationPhase::Run(PipelineData* data, Zone* temp_zone) {
  if (!COMPRESS_POINTERS_BOOL) return;
#if V8_ENABLE_WEBASSEMBLY
  bool is_wasm =
      data->linkage() != nullptr
          ? (data->linkage()
                 ->GetIncomingDescriptor()
                 ->IsAnyWasmFunctionCall() ||
             data->linkage()->GetIncomingDescriptor()->IsWasmImportWrapper() ||
             data->linkage()->GetIncomingDescriptor()->IsWasmCapiFunction())
          : data->pipeline_kind() == TurboshaftPipelineKind::kWasm;
#else
  bool is_wasm = false;
#endif
  RunDecompressionOptimization(data->graph(), temp_zone, is_wasm);
}

}  // namespace v8::internal::compiler::turboshaft
