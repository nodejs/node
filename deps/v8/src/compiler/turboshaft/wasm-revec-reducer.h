// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_COMPILER_TURBOSHAFT_WASM_REVEC_REDUCER_H_
#define V8_COMPILER_TURBOSHAFT_WASM_REVEC_REDUCER_H_

#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/phase.h"
#include "src/compiler/wasm-graph-assembler.h"

namespace v8::internal::compiler::turboshaft {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

class WasmRevecAnalyzer {
 public:
  WasmRevecAnalyzer(Zone* zone, Graph& graph)
      : graph_(graph),
        phase_zone_(zone),
        store_seeds_(zone),
        should_reduce_(false) {
    Run();
  }

  void Run();

  bool ShouldReduce() const { return should_reduce_; }

 private:
  void ProcessBlock(const Block& block);

  Graph& graph_;
  Zone* phase_zone_;
  ZoneVector<std::pair<const StoreOp*, const StoreOp*>> store_seeds_;
  const wasm::WasmModule* module_ = PipelineData::Get().wasm_module();
  const wasm::FunctionSig* signature_ = PipelineData::Get().wasm_sig();
  bool should_reduce_;
};

template <class Next>
class WasmRevecReducer : public Next {
 public:
  TURBOSHAFT_REDUCER_BOILERPLATE()

 private:
  const wasm::WasmModule* module_ = PipelineData::Get().wasm_module();
  WasmRevecAnalyzer analyzer_ = *PipelineData::Get().wasm_revec_analyzer();
};

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_WASM_REVEC_REDUCER_H_
