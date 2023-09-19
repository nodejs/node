// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_WASM_GEN_H_
#define V8_BUILTINS_BUILTINS_WASM_GEN_H_

#include "src/codegen/code-stub-assembler.h"

namespace v8 {
namespace internal {

class WasmBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit WasmBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  TNode<WasmInstanceObject> LoadInstanceFromFrame();

  TNode<NativeContext> LoadContextFromWasmOrJsFrame();

  TNode<NativeContext> LoadContextFromInstance(
      TNode<WasmInstanceObject> instance);

  TNode<FixedArray> LoadTablesFromInstance(TNode<WasmInstanceObject> instance);

  TNode<FixedArray> LoadInternalFunctionsFromInstance(
      TNode<WasmInstanceObject> instance);

  TNode<FixedArray> LoadManagedObjectMapsFromInstance(
      TNode<WasmInstanceObject> instance);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_WASM_GEN_H_
