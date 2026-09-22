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
}  // namespace compiler

class UnaryOpAssembler final : public CodeStubAssembler {
 public:
  explicit UnaryOpAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  FeedbackUpdater MakeEmbeddedFeedbackUpdater(
      TNode<BytecodeArray> bytecode_array, TNode<IntPtrT> feedback_offset) {
    return [=, this](TNode<Smi> feedback) {
      UpdateEmbeddedFeedback<BinaryOperationFeedback>(feedback, bytecode_array,
                                                      feedback_offset);
    };
  }

  TNode<Object> Generate_BitwiseNotWithFeedback(
      TNode<Context> context, TNode<Object> value,
      const FeedbackUpdater& feedback_updater);

  TNode<Object> Generate_DecrementWithFeedback(
      TNode<Context> context, TNode<Object> value,
      const FeedbackUpdater& feedback_updater);

  TNode<Object> Generate_IncrementWithFeedback(
      TNode<Context> context, TNode<Object> value,
      const FeedbackUpdater& feedback_updater);

  TNode<Object> Generate_NegateWithFeedback(
      TNode<Context> context, TNode<Object> value,
      const FeedbackUpdater& feedback_updater);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_IC_UNARY_OP_ASSEMBLER_H_
