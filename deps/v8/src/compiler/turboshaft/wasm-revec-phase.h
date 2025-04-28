// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_WASM_REVEC_PHASE_H_
#define V8_COMPILER_TURBOSHAFT_WASM_REVEC_PHASE_H_

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#include "src/compiler/turboshaft/phase.h"

namespace v8::internal::compiler::turboshaft {

class WasmRevecVerifier {
 public:
  explicit WasmRevecVerifier(std::function<void(const Graph&)> handler)
      : handler_(handler) {}

  void Verify(const Graph& graph) {
    if (handler_) handler_(graph);
  }

 private:
  std::function<void(const Graph&)> handler_ = nullptr;
};

struct WasmRevecPhase {
  DECL_TURBOSHAFT_PHASE_CONSTANTS(WasmRevec)

  void Run(PipelineData* data, Zone* temp_zone);
};

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_WASM_REVEC_PHASE_H_
