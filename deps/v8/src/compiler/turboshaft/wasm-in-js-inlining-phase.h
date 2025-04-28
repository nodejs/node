// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_COMPILER_TURBOSHAFT_WASM_IN_JS_INLINING_PHASE_H_
#define V8_COMPILER_TURBOSHAFT_WASM_IN_JS_INLINING_PHASE_H_

#include "src/compiler/turboshaft/phase.h"

namespace v8::internal::compiler::turboshaft {

// This reducer is part of the JavaScript pipeline and inlines the code of
// sufficiently small/hot Wasm functions into the caller JS function.
struct WasmInJSInliningPhase {
  DECL_TURBOSHAFT_PHASE_CONSTANTS(WasmInJSInlining)

  void Run(PipelineData* data, Zone* temp_zone);
};

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_WASM_IN_JS_INLINING_PHASE_H_
