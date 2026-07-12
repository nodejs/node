// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_JS_TRAMPOLINE_ASSEMBLER_H_
#define V8_BUILTINS_JS_TRAMPOLINE_ASSEMBLER_H_

#include "src/codegen/code-stub-assembler.h"

namespace v8::internal {

// Provides helpers which assume parameters according to the JavaScript calling
// conventions (`JSTrampolineDescriptor`).
class JSTrampolineAssembler : public CodeStubAssembler {
 public:
  using Descriptor = JSTrampolineDescriptor;

  explicit JSTrampolineAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  void TailCallJSFunction(TNode<JSFunction> function);

  template <typename Function>
  void TieringBuiltinImpl(const Function& Impl);


  void CompileLazy(TNode<JSFunction> function, TNode<Context> context);
};

}  // namespace v8::internal

#endif  // V8_BUILTINS_JS_TRAMPOLINE_ASSEMBLER_H_
