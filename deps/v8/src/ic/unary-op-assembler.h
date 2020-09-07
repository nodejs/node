// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IC_UNARY_OP_ASSEMBLER_H_
#define V8_IC_UNARY_OP_ASSEMBLER_H_

#include "src/codegen/code-stub-assembler.h"

namespace v8 {
namespace internal {

namespace compiler {
class CodeAssemblerState;
}

class UnaryOpAssembler final {
 public:
  explicit UnaryOpAssembler(compiler::CodeAssemblerState* state)
      : state_(state) {}

  TNode<Object> Generate_BitwiseNotWithFeedback(
      TNode<Context> context, TNode<Object> value, TNode<UintPtrT> slot,
      TNode<HeapObject> maybe_feedback_vector);

  TNode<Object> Generate_DecrementWithFeedback(
      TNode<Context> context, TNode<Object> value, TNode<UintPtrT> slot,
      TNode<HeapObject> maybe_feedback_vector);

  TNode<Object> Generate_IncrementWithFeedback(
      TNode<Context> context, TNode<Object> value, TNode<UintPtrT> slot,
      TNode<HeapObject> maybe_feedback_vector);

  TNode<Object> Generate_NegateWithFeedback(
      TNode<Context> context, TNode<Object> value, TNode<UintPtrT> slot,
      TNode<HeapObject> maybe_feedback_vector);

 private:
  compiler::CodeAssemblerState* const state_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_IC_UNARY_OP_ASSEMBLER_H_
