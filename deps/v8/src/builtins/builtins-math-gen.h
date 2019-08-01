// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_MATH_GEN_H_
#define V8_BUILTINS_BUILTINS_MATH_GEN_H_

#include "src/codegen/code-stub-assembler.h"

namespace v8 {
namespace internal {

class MathBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit MathBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  Node* MathPow(Node* context, Node* base, Node* exponent);

 protected:
  void MathRoundingOperation(
      Node* context, Node* x,
      TNode<Float64T> (CodeStubAssembler::*float64op)(SloppyTNode<Float64T>));
  void MathMaxMin(Node* context, Node* argc,
                  TNode<Float64T> (CodeStubAssembler::*float64op)(
                      SloppyTNode<Float64T>, SloppyTNode<Float64T>),
                  double default_val);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_MATH_GEN_H_
