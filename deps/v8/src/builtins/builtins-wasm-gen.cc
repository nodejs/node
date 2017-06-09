// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-gen.h"
#include "src/code-stub-assembler.h"
#include "src/objects-inl.h"
#include "src/wasm/wasm-opcodes.h"

namespace v8 {
namespace internal {

typedef compiler::Node Node;

TF_BUILTIN(WasmStackGuard, CodeStubAssembler) {
  Node* context = SmiConstant(Smi::kZero);
  TailCallRuntime(Runtime::kWasmStackGuard, context);
}

#define DECLARE_ENUM(name)                                                    \
  TF_BUILTIN(ThrowWasm##name, CodeStubAssembler) {                            \
    int message_id = wasm::WasmOpcodes::TrapReasonToMessageId(wasm::k##name); \
    TailCallRuntime(Runtime::kThrowWasmErrorFromTrapIf,                       \
                    SmiConstant(Smi::kZero), SmiConstant(message_id));        \
  }
FOREACH_WASM_TRAPREASON(DECLARE_ENUM)
#undef DECLARE_ENUM

}  // namespace internal
}  // namespace v8
