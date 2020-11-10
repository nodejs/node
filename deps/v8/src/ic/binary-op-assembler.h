// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IC_BINARY_OP_ASSEMBLER_H_
#define V8_IC_BINARY_OP_ASSEMBLER_H_

#include <functional>
#include "src/codegen/code-stub-assembler.h"

namespace v8 {
namespace internal {

namespace compiler {
class CodeAssemblerState;
}

class BinaryOpAssembler : public CodeStubAssembler {
 public:
  explicit BinaryOpAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  TNode<Object> Generate_AddWithFeedback(
      TNode<Context> context, TNode<Object> left, TNode<Object> right,
      TNode<UintPtrT> slot, TNode<HeapObject> maybe_feedback_vector,
      bool rhs_known_smi);

  TNode<Object> Generate_SubtractWithFeedback(
      TNode<Context> context, TNode<Object> left, TNode<Object> right,
      TNode<UintPtrT> slot, TNode<HeapObject> maybe_feedback_vector,
      bool rhs_known_smi);

  TNode<Object> Generate_MultiplyWithFeedback(
      TNode<Context> context, TNode<Object> left, TNode<Object> right,
      TNode<UintPtrT> slot, TNode<HeapObject> maybe_feedback_vector,
      bool rhs_known_smi);

  TNode<Object> Generate_DivideWithFeedback(
      TNode<Context> context, TNode<Object> dividend, TNode<Object> divisor,
      TNode<UintPtrT> slot, TNode<HeapObject> maybe_feedback_vector,
      bool rhs_known_smi);

  TNode<Object> Generate_ModulusWithFeedback(
      TNode<Context> context, TNode<Object> dividend, TNode<Object> divisor,
      TNode<UintPtrT> slot, TNode<HeapObject> maybe_feedback_vector,
      bool rhs_known_smi);

  TNode<Object> Generate_ExponentiateWithFeedback(
      TNode<Context> context, TNode<Object> base, TNode<Object> exponent,
      TNode<UintPtrT> slot, TNode<HeapObject> maybe_feedback_vector,
      bool rhs_known_smi);

  TNode<Object> Generate_BitwiseBinaryOpWithFeedback(Operation bitwise_op,
                                                     TNode<Object> left,
                                                     TNode<Object> right,
                                                     TNode<Context> context,
                                                     TVariable<Smi>* feedback) {
    return Generate_BitwiseBinaryOpWithOptionalFeedback(bitwise_op, left, right,
                                                        context, feedback);
  }

  TNode<Object> Generate_BitwiseBinaryOp(Operation bitwise_op,
                                         TNode<Object> left,
                                         TNode<Object> right,
                                         TNode<Context> context) {
    return Generate_BitwiseBinaryOpWithOptionalFeedback(bitwise_op, left, right,
                                                        context, nullptr);
  }

 private:
  using SmiOperation =
      std::function<TNode<Object>(TNode<Smi>, TNode<Smi>, TVariable<Smi>*)>;
  using FloatOperation =
      std::function<TNode<Float64T>(TNode<Float64T>, TNode<Float64T>)>;

  TNode<Object> Generate_BinaryOperationWithFeedback(
      TNode<Context> context, TNode<Object> left, TNode<Object> right,
      TNode<UintPtrT> slot, TNode<HeapObject> maybe_feedback_vector,
      const SmiOperation& smiOperation, const FloatOperation& floatOperation,
      Operation op, bool rhs_known_smi);

  TNode<Object> Generate_BitwiseBinaryOpWithOptionalFeedback(
      Operation bitwise_op, TNode<Object> left, TNode<Object> right,
      TNode<Context> context, TVariable<Smi>* feedback);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_IC_BINARY_OP_ASSEMBLER_H_
