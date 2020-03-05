// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_PROMISE_GEN_H_
#define V8_BUILTINS_BUILTINS_PROMISE_GEN_H_

#include "src/codegen/code-stub-assembler.h"
#include "src/objects/promise.h"

namespace v8 {
namespace internal {

using CodeAssemblerState = compiler::CodeAssemblerState;

class V8_EXPORT_PRIVATE PromiseBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit PromiseBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}
  void ZeroOutEmbedderOffsets(TNode<JSPromise> promise);

  TNode<HeapObject> AllocateJSPromise(TNode<Context> context);

  TNode<HeapObject> AllocatePromiseReactionJobTask(TNode<Context> context);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_PROMISE_GEN_H_
