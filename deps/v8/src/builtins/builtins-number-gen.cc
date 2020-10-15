// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/codegen/code-stub-assembler.h"
#include "src/ic/binary-op-assembler.h"
#include "src/ic/unary-op-assembler.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// ES6 section 20.1 Number Objects

#define DEF_BINOP(Name, Generator)                                            \
  TF_BUILTIN(Name, CodeStubAssembler) {                                       \
    TNode<Object> lhs = CAST(Parameter(Descriptor::kLeft));                   \
    TNode<Object> rhs = CAST(Parameter(Descriptor::kRight));                  \
    TNode<Context> context = CAST(Parameter(Descriptor::kContext));           \
    TNode<HeapObject> maybe_feedback_vector =                                 \
        CAST(Parameter(Descriptor::kMaybeFeedbackVector));                    \
    TNode<UintPtrT> slot =                                                    \
        UncheckedCast<UintPtrT>(Parameter(Descriptor::kSlot));                \
                                                                              \
    BinaryOpAssembler binop_asm(state());                                     \
    TNode<Object> result = binop_asm.Generator(context, lhs, rhs, slot,       \
                                               maybe_feedback_vector, false); \
                                                                              \
    Return(result);                                                           \
  }
DEF_BINOP(Add_WithFeedback, Generate_AddWithFeedback)
DEF_BINOP(Subtract_WithFeedback, Generate_SubtractWithFeedback)
DEF_BINOP(Multiply_WithFeedback, Generate_MultiplyWithFeedback)
DEF_BINOP(Divide_WithFeedback, Generate_DivideWithFeedback)
DEF_BINOP(Modulus_WithFeedback, Generate_ModulusWithFeedback)
DEF_BINOP(Exponentiate_WithFeedback, Generate_ExponentiateWithFeedback)
DEF_BINOP(BitwiseOr_WithFeedback, Generate_BitwiseOrWithFeedback)
DEF_BINOP(BitwiseXor_WithFeedback, Generate_BitwiseXorWithFeedback)
DEF_BINOP(BitwiseAnd_WithFeedback, Generate_BitwiseAndWithFeedback)
DEF_BINOP(ShiftLeft_WithFeedback, Generate_ShiftLeftWithFeedback)
DEF_BINOP(ShiftRight_WithFeedback, Generate_ShiftRightWithFeedback)
DEF_BINOP(ShiftRightLogical_WithFeedback,
          Generate_ShiftRightLogicalWithFeedback)
#undef DEF_BINOP

#define DEF_UNOP(Name, Generator)                                   \
  TF_BUILTIN(Name, CodeStubAssembler) {                             \
    TNode<Object> value = CAST(Parameter(Descriptor::kValue));      \
    TNode<Context> context = CAST(Parameter(Descriptor::kContext)); \
    TNode<HeapObject> maybe_feedback_vector =                       \
        CAST(Parameter(Descriptor::kMaybeFeedbackVector));          \
    TNode<UintPtrT> slot =                                          \
        UncheckedCast<UintPtrT>(Parameter(Descriptor::kSlot));      \
                                                                    \
    UnaryOpAssembler a(state());                                    \
    TNode<Object> result =                                          \
        a.Generator(context, value, slot, maybe_feedback_vector);   \
                                                                    \
    Return(result);                                                 \
  }
DEF_UNOP(BitwiseNot_WithFeedback, Generate_BitwiseNotWithFeedback)
DEF_UNOP(Decrement_WithFeedback, Generate_DecrementWithFeedback)
DEF_UNOP(Increment_WithFeedback, Generate_IncrementWithFeedback)
DEF_UNOP(Negate_WithFeedback, Generate_NegateWithFeedback)
#undef DEF_UNOP

#define DEF_COMPARE(Name)                                                      \
  TF_BUILTIN(Name##_WithFeedback, CodeStubAssembler) {                         \
    TNode<Object> lhs = CAST(Parameter(Descriptor::kLeft));                    \
    TNode<Object> rhs = CAST(Parameter(Descriptor::kRight));                   \
    TNode<Context> context = CAST(Parameter(Descriptor::kContext));            \
    TNode<HeapObject> maybe_feedback_vector =                                  \
        CAST(Parameter(Descriptor::kMaybeFeedbackVector));                     \
    TNode<UintPtrT> slot =                                                     \
        UncheckedCast<UintPtrT>(Parameter(Descriptor::kSlot));                 \
                                                                               \
    TVARIABLE(Smi, var_type_feedback);                                         \
    TNode<Oddball> result = RelationalComparison(Operation::k##Name, lhs, rhs, \
                                                 context, &var_type_feedback); \
    UpdateFeedback(var_type_feedback.value(), maybe_feedback_vector, slot);    \
                                                                               \
    Return(result);                                                            \
  }
DEF_COMPARE(LessThan)
DEF_COMPARE(LessThanOrEqual)
DEF_COMPARE(GreaterThan)
DEF_COMPARE(GreaterThanOrEqual)
#undef DEF_COMPARE

TF_BUILTIN(Equal_WithFeedback, CodeStubAssembler) {
  TNode<Object> lhs = CAST(Parameter(Descriptor::kLeft));
  TNode<Object> rhs = CAST(Parameter(Descriptor::kRight));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<HeapObject> maybe_feedback_vector =
      CAST(Parameter(Descriptor::kMaybeFeedbackVector));
  TNode<UintPtrT> slot = UncheckedCast<UintPtrT>(Parameter(Descriptor::kSlot));

  TVARIABLE(Smi, var_type_feedback);
  TNode<Oddball> result = Equal(lhs, rhs, context, &var_type_feedback);
  UpdateFeedback(var_type_feedback.value(), maybe_feedback_vector, slot);

  Return(result);
}

TF_BUILTIN(StrictEqual_WithFeedback, CodeStubAssembler) {
  TNode<Object> lhs = CAST(Parameter(Descriptor::kLeft));
  TNode<Object> rhs = CAST(Parameter(Descriptor::kRight));
  TNode<HeapObject> maybe_feedback_vector =
      CAST(Parameter(Descriptor::kMaybeFeedbackVector));
  TNode<UintPtrT> slot = UncheckedCast<UintPtrT>(Parameter(Descriptor::kSlot));

  TVARIABLE(Smi, var_type_feedback);
  TNode<Oddball> result = StrictEqual(lhs, rhs, &var_type_feedback);
  UpdateFeedback(var_type_feedback.value(), maybe_feedback_vector, slot);

  Return(result);
}

}  // namespace internal
}  // namespace v8
