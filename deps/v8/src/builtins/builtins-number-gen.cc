// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/codegen/code-stub-assembler-inl.h"
#include "src/ic/binary-op-assembler.h"
#include "src/ic/unary-op-assembler.h"

namespace v8 {
namespace internal {

#include "src/codegen/define-code-stub-assembler-macros.inc"

// -----------------------------------------------------------------------------
// ES6 section 20.1 Number Objects

#define DEF_BINOP(Name, Generator)                                           \
  TF_BUILTIN(Name, CodeStubAssembler) {                                      \
    auto lhs = Parameter<Object>(Descriptor::kLeft);                         \
    auto rhs = Parameter<Object>(Descriptor::kRight);                        \
    auto context = Parameter<Context>(Descriptor::kContext);                 \
    auto feedback_vector =                                                   \
        Parameter<FeedbackVector>(Descriptor::kFeedbackVector);              \
    auto slot = UncheckedParameter<UintPtrT>(Descriptor::kSlot);             \
                                                                             \
    BinaryOpAssembler binop_asm(state());                                    \
    TNode<Object> result =                                                   \
        binop_asm.Generator([&]() { return context; }, lhs, rhs, slot,       \
                            [&]() { return feedback_vector; },               \
                            UpdateFeedbackMode::kGuaranteedFeedback, false); \
                                                                             \
    Return(result);                                                          \
  }
DEF_BINOP(Add_LhsIsStringConstant_Internalize_WithFeedback,
          Generate_AddLhsIsStringConstantInternalizeWithFeedback)
DEF_BINOP(Add_RhsIsStringConstant_Internalize_WithFeedback,
          Generate_AddRhsIsStringConstantInternalizeWithFeedback)
#undef DEF_BINOP

#define DEF_BINOP(Name, Generator)                                   \
  TF_BUILTIN(Name, CodeStubAssembler) {                              \
    auto lhs = Parameter<Object>(Descriptor::kLeft);                 \
    auto rhs = Parameter<Object>(Descriptor::kRight);                \
    auto slot = UncheckedParameter<UintPtrT>(Descriptor::kSlot);     \
                                                                     \
    BinaryOpAssembler binop_asm(state());                            \
    TNode<Object> result = binop_asm.Generator(                      \
        [&]() { return LoadContextFromBaseline(); }, lhs, rhs, slot, \
        [&]() { return LoadFeedbackVectorFromBaseline(); },          \
        UpdateFeedbackMode::kGuaranteedFeedback, false);             \
                                                                     \
    Return(result);                                                  \
  }
DEF_BINOP(Add_LhsIsStringConstant_Internalize_Baseline,
          Generate_AddLhsIsStringConstantInternalizeWithFeedback)
DEF_BINOP(Add_RhsIsStringConstant_Internalize_Baseline,
          Generate_AddRhsIsStringConstantInternalizeWithFeedback)
#undef DEF_BINOP

#define DEF_BINOP(Name, Generator)                                 \
  TF_BUILTIN(Name, CodeStubAssembler) {                            \
    auto lhs = Parameter<Object>(Descriptor::kLeft);               \
    auto rhs = Parameter<Object>(Descriptor::kRight);              \
    auto context = Parameter<Context>(Descriptor::kContext);       \
    auto bytecode_array =                                          \
        Parameter<BytecodeArray>(Descriptor::kBytecodeArray);      \
    auto feedback_offset =                                         \
        UncheckedParameter<IntPtrT>(Descriptor::kFeedbackOffset);  \
                                                                   \
    BinaryOpAssembler binop_asm(state());                          \
    TNode<Object> result =                                         \
        binop_asm.Generator([&]() { return context; }, lhs, rhs,   \
                            binop_asm.MakeEmbeddedFeedbackUpdater( \
                                bytecode_array, feedback_offset),  \
                            false);                                \
                                                                   \
    Return(result);                                                \
  }
#ifndef V8_ENABLE_EXPERIMENTAL_TSA_BUILTINS
DEF_BINOP(Add_WithFeedback, Generate_AddWithFeedback)
#endif  // V8_ENABLE_EXPERIMENTAL_TSA_BUILTINS
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

#define DEF_BINOP(BuiltinPrefix, Op, RhsKnownSmi)                              \
  TF_BUILTIN(BuiltinPrefix##_Generic_Baseline, CodeStubAssembler) {            \
    auto lhs = Parameter<Object>(Descriptor::kLeft);                           \
    auto rhs = Parameter<Object>(Descriptor::kRight);                          \
    auto feedback_offset =                                                     \
        UncheckedParameter<IntPtrT>(Descriptor::kFeedbackOffset);              \
                                                                               \
    BinaryOpAssembler binop_asm(state());                                      \
    TNode<Object> result = binop_asm.Generate_##Op##WithFeedback(              \
        [&]() { return LoadContextFromBaseline(); }, lhs, rhs,                 \
        binop_asm.MakeEmbeddedFeedbackUpdater(LoadBytecodeArrayFromBaseline(), \
                                              feedback_offset),                \
        RhsKnownSmi);                                                          \
                                                                               \
    Return(result);                                                            \
  }
DEF_BINOP(Add, Add, false)
DEF_BINOP(Subtract, Subtract, false)
DEF_BINOP(Multiply, Multiply, false)
DEF_BINOP(Divide, Divide, false)
DEF_BINOP(Modulus, Modulus, false)
DEF_BINOP(Exponentiate, Exponentiate, false)
DEF_BINOP(BitwiseOr, BitwiseOr, false)
DEF_BINOP(BitwiseXor, BitwiseXor, false)
DEF_BINOP(BitwiseAnd, BitwiseAnd, false)
DEF_BINOP(ShiftLeft, ShiftLeft, false)
DEF_BINOP(ShiftRight, ShiftRight, false)
DEF_BINOP(ShiftRightLogical, ShiftRightLogical, false)
DEF_BINOP(AddSmi, Add, true)
DEF_BINOP(SubtractSmi, Subtract, true)
DEF_BINOP(MultiplySmi, Multiply, true)
DEF_BINOP(DivideSmi, Divide, true)
DEF_BINOP(ModulusSmi, Modulus, true)
DEF_BINOP(ExponentiateSmi, Exponentiate, true)
DEF_BINOP(BitwiseOrSmi, BitwiseOr, true)
DEF_BINOP(BitwiseXorSmi, BitwiseXor, true)
DEF_BINOP(BitwiseAndSmi, BitwiseAnd, true)
DEF_BINOP(ShiftLeftSmi, ShiftLeft, true)
DEF_BINOP(ShiftRightSmi, ShiftRight, true)
DEF_BINOP(ShiftRightLogicalSmi, ShiftRightLogical, true)
#undef DEF_BINOP

#define DEF_UNOP(Name, Generator)                                \
  TF_BUILTIN(Name, CodeStubAssembler) {                          \
    auto value = Parameter<Object>(Descriptor::kValue);          \
    auto context = Parameter<Context>(Descriptor::kContext);     \
    auto feedback_vector =                                       \
        Parameter<FeedbackVector>(Descriptor::kFeedbackVector);  \
    auto slot = UncheckedParameter<UintPtrT>(Descriptor::kSlot); \
                                                                 \
    UnaryOpAssembler a(state());                                 \
    TNode<Object> result =                                       \
        a.Generator(context, value, slot, feedback_vector,       \
                    UpdateFeedbackMode::kGuaranteedFeedback);    \
                                                                 \
    Return(result);                                              \
  }
#ifndef V8_ENABLE_EXPERIMENTAL_TSA_BUILTINS
DEF_UNOP(BitwiseNot_WithFeedback, Generate_BitwiseNotWithFeedback)
#endif
DEF_UNOP(Decrement_WithFeedback, Generate_DecrementWithFeedback)
DEF_UNOP(Increment_WithFeedback, Generate_IncrementWithFeedback)
DEF_UNOP(Negate_WithFeedback, Generate_NegateWithFeedback)
#undef DEF_UNOP

#define DEF_UNOP(Name, Generator)                                \
  TF_BUILTIN(Name, CodeStubAssembler) {                          \
    auto value = Parameter<Object>(Descriptor::kValue);          \
    auto context = LoadContextFromBaseline();                    \
    auto feedback_vector = LoadFeedbackVectorFromBaseline();     \
    auto slot = UncheckedParameter<UintPtrT>(Descriptor::kSlot); \
                                                                 \
    UnaryOpAssembler a(state());                                 \
    TNode<Object> result =                                       \
        a.Generator(context, value, slot, feedback_vector,       \
                    UpdateFeedbackMode::kGuaranteedFeedback);    \
                                                                 \
    Return(result);                                              \
  }
DEF_UNOP(BitwiseNot_Baseline, Generate_BitwiseNotWithFeedback)
DEF_UNOP(Decrement_Baseline, Generate_DecrementWithFeedback)
DEF_UNOP(Increment_Baseline, Generate_IncrementWithFeedback)
DEF_UNOP(Negate_Baseline, Generate_NegateWithFeedback)
#undef DEF_UNOP

#define DEF_RELATIONAL_COMPARE(Name)                                       \
  TF_BUILTIN(Name##_WithEmbeddedFeedback, CodeStubAssembler) {             \
    auto lhs = Parameter<Object>(Descriptor::kLeft);                       \
    auto rhs = Parameter<Object>(Descriptor::kRight);                      \
    auto context = Parameter<Context>(Descriptor::kContext);               \
    auto bytecode_array =                                                  \
        Parameter<BytecodeArray>(Descriptor::kBytecodeArray);              \
    auto feedback_offset =                                                 \
        UncheckedParameter<IntPtrT>(Descriptor::kFeedbackOffset);          \
                                                                           \
    TVARIABLE(Smi, var_type_feedback);                                     \
    TVARIABLE(Object, var_exception);                                      \
    Label if_exception(this, Label::kDeferred);                            \
    TNode<Boolean> result;                                                 \
    {                                                                      \
      ScopedExceptionHandler handler(this, &if_exception, &var_exception); \
      result = RelationalComparison(Operation::k##Name, lhs, rhs, context, \
                                    &var_type_feedback);                   \
    }                                                                      \
    UpdateEmbeddedFeedback<CompareOperationFeedback>(                      \
        var_type_feedback.value(), bytecode_array, feedback_offset);       \
                                                                           \
    Return(result);                                                        \
    BIND(&if_exception);                                                   \
    {                                                                      \
      UpdateEmbeddedFeedback<CompareOperationFeedback>(                    \
          var_type_feedback.value(), bytecode_array, feedback_offset);     \
      CallRuntime(Runtime::kReThrow, context, var_exception.value());      \
      Unreachable();                                                       \
    }                                                                      \
  }
DEF_RELATIONAL_COMPARE(LessThan)
DEF_RELATIONAL_COMPARE(LessThanOrEqual)
DEF_RELATIONAL_COMPARE(GreaterThan)
DEF_RELATIONAL_COMPARE(GreaterThanOrEqual)
#undef DEF_RELATIONAL_COMPARE

#define DEF_RELATIONAL_COMPARE(Name)                                        \
  TF_BUILTIN(Name##_Generic_Baseline, CodeStubAssembler) {                  \
    auto lhs = Parameter<Object>(Descriptor::kLeft);                        \
    auto rhs = Parameter<Object>(Descriptor::kRight);                       \
    auto feedback_offset =                                                  \
        UncheckedParameter<IntPtrT>(Descriptor::kFeedbackOffset);           \
                                                                            \
    TVARIABLE(Smi, var_type_feedback);                                      \
    TVARIABLE(Object, var_exception);                                       \
    Label if_exception(this, Label::kDeferred);                             \
    TNode<Boolean> result;                                                  \
    {                                                                       \
      ScopedExceptionHandler handler(this, &if_exception, &var_exception);  \
      result = RelationalComparison(                                        \
          Operation::k##Name, lhs, rhs,                                     \
          [&]() { return LoadContextFromBaseline(); }, &var_type_feedback); \
    }                                                                       \
    auto bytecode_array = LoadBytecodeArrayFromBaseline();                  \
    UpdateEmbeddedFeedback<CompareOperationFeedback>(                       \
        var_type_feedback.value(), bytecode_array, feedback_offset);        \
                                                                            \
    Return(result);                                                         \
    BIND(&if_exception);                                                    \
    {                                                                       \
      bytecode_array = LoadBytecodeArrayFromBaseline();                     \
      UpdateEmbeddedFeedback<CompareOperationFeedback>(                     \
          var_type_feedback.value(), bytecode_array, feedback_offset);      \
      CallRuntime(Runtime::kReThrow, LoadContextFromBaseline(),             \
                  var_exception.value());                                   \
      Unreachable();                                                        \
    }                                                                       \
  }
DEF_RELATIONAL_COMPARE(LessThan)
DEF_RELATIONAL_COMPARE(LessThanOrEqual)
DEF_RELATIONAL_COMPARE(GreaterThan)
DEF_RELATIONAL_COMPARE(GreaterThanOrEqual)
#undef DEF_RELATIONAL_COMPARE

TF_BUILTIN(AddLhsIsStringConstantInternalizeWithVector, CodeStubAssembler) {
  auto left = Parameter<String>(Descriptor::kLeft);
  auto right = Parameter<Object>(Descriptor::kRight);
  auto slot = Parameter<Smi>(Descriptor::kSlot);
  auto vector = Parameter<HeapObject>(Descriptor::kVector);
  TNode<Context> context = Parameter<Context>(Descriptor::kContext);
  BinaryOpAssembler binop_asm(state());
  TNode<Object> result =
      binop_asm.Generate_AddLhsIsStringConstantInternalizeWithFeedback(
          [&]() { return context; }, left, right, Unsigned(SmiUntag(slot)),
          [&]() { return vector; }, UpdateFeedbackMode::kGuaranteedFeedback,
          false);
  Return(result);
}

TF_BUILTIN(AddLhsIsStringConstantInternalizeTrampoline, CodeStubAssembler) {
  auto left = Parameter<String>(Descriptor::kLeft);
  auto right = Parameter<Object>(Descriptor::kRight);
  auto slot = Parameter<Smi>(Descriptor::kSlot);
  TNode<Context> context = Parameter<Context>(Descriptor::kContext);
  BinaryOpAssembler binop_asm(state());
  TNode<Object> result =
      binop_asm.Generate_AddLhsIsStringConstantInternalizeWithFeedback(
          [&]() { return context; }, left, right, Unsigned(SmiUntag(slot)),
          [&]() { return LoadFeedbackVectorForStub(); },
          UpdateFeedbackMode::kGuaranteedFeedback, false);
  Return(result);
}

TF_BUILTIN(AddRhsIsStringConstantInternalizeWithVector, CodeStubAssembler) {
  auto left = Parameter<Object>(Descriptor::kLeft);
  auto right = Parameter<String>(Descriptor::kRight);
  auto slot = Parameter<Smi>(Descriptor::kSlot);
  auto vector = Parameter<HeapObject>(Descriptor::kVector);
  TNode<Context> context = Parameter<Context>(Descriptor::kContext);
  BinaryOpAssembler binop_asm(state());
  TNode<Object> result =
      binop_asm.Generate_AddRhsIsStringConstantInternalizeWithFeedback(
          [&]() { return context; }, left, right, Unsigned(SmiUntag(slot)),
          [&]() { return vector; }, UpdateFeedbackMode::kGuaranteedFeedback,
          false);
  Return(result);
}

TF_BUILTIN(AddRhsIsStringConstantInternalizeTrampoline, CodeStubAssembler) {
  auto left = Parameter<Object>(Descriptor::kLeft);
  auto right = Parameter<String>(Descriptor::kRight);
  auto slot = Parameter<Smi>(Descriptor::kSlot);
  TNode<Context> context = Parameter<Context>(Descriptor::kContext);
  BinaryOpAssembler binop_asm(state());
  TNode<Object> result =
      binop_asm.Generate_AddRhsIsStringConstantInternalizeWithFeedback(
          [&]() { return context; }, left, right, Unsigned(SmiUntag(slot)),
          [&]() { return LoadFeedbackVectorForStub(); },
          UpdateFeedbackMode::kGuaranteedFeedback, false);
  Return(result);
}

TF_BUILTIN(Equal_WithEmbeddedFeedback, CodeStubAssembler) {
  auto lhs = Parameter<Object>(Descriptor::kLeft);
  auto rhs = Parameter<Object>(Descriptor::kRight);
  auto context = Parameter<Context>(Descriptor::kContext);
  auto bytecode_array = Parameter<BytecodeArray>(Descriptor::kBytecodeArray);
  auto feedback_offset =
      UncheckedParameter<IntPtrT>(Descriptor::kFeedbackOffset);

  TVARIABLE(Smi, var_type_feedback);
  TVARIABLE(Object, var_exception);
  Label if_exception(this, Label::kDeferred);
  TNode<Boolean> result;
  {
    ScopedExceptionHandler handler(this, &if_exception, &var_exception);
    result = Equal(lhs, rhs, [&]() { return context; }, &var_type_feedback);
  }
  UpdateEmbeddedFeedback<CompareOperationFeedback>(
      var_type_feedback.value(), bytecode_array, feedback_offset);
  Return(result);

  BIND(&if_exception);
  UpdateEmbeddedFeedback<CompareOperationFeedback>(
      var_type_feedback.value(), bytecode_array, feedback_offset);
  CallRuntime(Runtime::kReThrow, LoadContextFromBaseline(),
              var_exception.value());
  Unreachable();
}

TF_BUILTIN(StrictEqual_WithEmbeddedFeedback, CodeStubAssembler) {
  auto lhs = Parameter<Object>(Descriptor::kLeft);
  auto rhs = Parameter<Object>(Descriptor::kRight);
  auto bytecode_array = Parameter<BytecodeArray>(Descriptor::kBytecodeArray);
  auto feedback_offset =
      UncheckedParameter<IntPtrT>(Descriptor::kFeedbackOffset);

  TVARIABLE(Smi, var_type_feedback);
  TNode<Boolean> result = StrictEqual(lhs, rhs, &var_type_feedback);
  UpdateEmbeddedFeedback<CompareOperationFeedback>(
      var_type_feedback.value(), bytecode_array, feedback_offset);

  Return(result);
}

TF_BUILTIN(Equal_Generic_Baseline, CodeStubAssembler) {
  auto lhs = Parameter<Object>(Descriptor::kLeft);
  auto rhs = Parameter<Object>(Descriptor::kRight);
  auto feedback_offset =
      UncheckedParameter<IntPtrT>(Descriptor::kFeedbackOffset);

  TVARIABLE(Smi, var_type_feedback);
  TVARIABLE(Object, var_exception);
  Label if_exception(this, Label::kDeferred);
  TNode<Boolean> result;
  {
    ScopedExceptionHandler handler(this, &if_exception, &var_exception);
    result = Equal(
        lhs, rhs, [&]() { return LoadContextFromBaseline(); },
        &var_type_feedback);
  }
  auto bytecode_array = LoadBytecodeArrayFromBaseline();
  UpdateEmbeddedFeedback<CompareOperationFeedback>(
      var_type_feedback.value(), bytecode_array, feedback_offset);
  Return(result);

  BIND(&if_exception);
  {
    bytecode_array = LoadBytecodeArrayFromBaseline();
    UpdateEmbeddedFeedback<CompareOperationFeedback>(
        var_type_feedback.value(), bytecode_array, feedback_offset);
    CallRuntime(Runtime::kReThrow, LoadContextFromBaseline(),
                var_exception.value());
    Unreachable();
  }
}

TF_BUILTIN(StrictEqual_Generic_Baseline, CodeStubAssembler) {
  auto lhs = Parameter<Object>(Descriptor::kLeft);
  auto rhs = Parameter<Object>(Descriptor::kRight);
  auto feedback_offset =
      UncheckedParameter<IntPtrT>(Descriptor::kFeedbackOffset);

  TVARIABLE(Smi, var_type_feedback);
  TNode<Boolean> result = StrictEqual(lhs, rhs, &var_type_feedback);
  UpdateEmbeddedFeedback<CompareOperationFeedback>(
      var_type_feedback.value(), LoadBytecodeArrayFromBaseline(),
      feedback_offset);

  Return(result);
}

#ifdef V8_ENABLE_SPARKPLUG_PLUS
#define DEFINE_TYPED_EQUALITY_COMMON(Name)                         \
  TF_BUILTIN(Name##_None_Baseline, CodeStubAssembler) {            \
    auto lhs = Parameter<Object>(Descriptor::kLeft);               \
    auto rhs = Parameter<Object>(Descriptor::kRight);              \
    auto feedback_offset =                                         \
        UncheckedParameter<UintPtrT>(Descriptor::kFeedbackOffset); \
                                                                   \
    TailCallBuiltin(Builtin::k##Name##AndTryPatchCode,             \
                    LoadContextFromBaseline(), lhs, rhs,           \
                    Int32Constant(static_cast<int32_t>(            \
                        CompareOperationFeedback::Type::kNone)),   \
                    feedback_offset);                              \
  }                                                                \
                                                                   \
  TF_BUILTIN(Name##AndTryPatchCode, CodeStubAssembler) {           \
    auto lhs = Parameter<Object>(Descriptor::kLeft);               \
    auto rhs = Parameter<Object>(Descriptor::kRight);              \
    auto current_feedback =                                        \
        UncheckedParameter<Int32T>(Descriptor::kCurrentFeedback);  \
    auto feedback_offset =                                         \
        UncheckedParameter<UintPtrT>(Descriptor::kFeedbackOffset); \
                                                                   \
    Generate##Name##AndTryPatchCode(lhs, rhs, current_feedback,    \
                                    feedback_offset);              \
  }                                                                \
                                                                   \
  TF_BUILTIN(Name##_SignedSmall_Baseline, CodeStubAssembler) {     \
    auto lhs = Parameter<Object>(Descriptor::kLeft);               \
    auto rhs = Parameter<Object>(Descriptor::kRight);              \
    auto feedback_offset =                                         \
        UncheckedParameter<UintPtrT>(Descriptor::kFeedbackOffset); \
                                                                   \
    GenerateSmiEqual(lhs, rhs, feedback_offset,                    \
                     Builtin::k##Name##AndTryPatchCode);           \
  }                                                                \
                                                                   \
  TF_BUILTIN(Name##_Number_Baseline, CodeStubAssembler) {          \
    auto lhs = Parameter<Object>(Descriptor::kLeft);               \
    auto rhs = Parameter<Object>(Descriptor::kRight);              \
    auto feedback_offset =                                         \
        UncheckedParameter<UintPtrT>(Descriptor::kFeedbackOffset); \
                                                                   \
    GenerateNumberEqual(lhs, rhs, feedback_offset,                 \
                        Builtin::k##Name##AndTryPatchCode);        \
  }                                                                \
                                                                   \
  TF_BUILTIN(Name##_String_Baseline, CodeStubAssembler) {          \
    auto lhs = Parameter<Object>(Descriptor::kLeft);               \
    auto rhs = Parameter<Object>(Descriptor::kRight);              \
    auto feedback_offset =                                         \
        UncheckedParameter<UintPtrT>(Descriptor::kFeedbackOffset); \
                                                                   \
    GenerateStringEqual(lhs, rhs, feedback_offset,                 \
                        Builtin::k##Name##AndTryPatchCode);        \
  }

DEFINE_TYPED_EQUALITY_COMMON(StrictEqual)
DEFINE_TYPED_EQUALITY_COMMON(Equal)
#undef DEFINE_TYPED_EQUALITY_COMMON

#define DEF_TYPED_OBJECT_EQUALITY(EqualityType, StubType, TypeChecker)  \
  TF_BUILTIN(EqualityType##_##StubType##_Baseline, CodeStubAssembler) { \
    auto lhs = Parameter<Object>(Descriptor::kLeft);                    \
    auto rhs = Parameter<Object>(Descriptor::kRight);                   \
    auto feedback_offset =                                              \
        UncheckedParameter<UintPtrT>(Descriptor::kFeedbackOffset);      \
                                                                        \
    GenerateTypedTaggedEqual(                                           \
        lhs, rhs, feedback_offset,                                      \
        [this](TNode<HeapObject> o) { return TypeChecker(o); },         \
        Builtin::k##EqualityType##AndTryPatchCode,                      \
        Int32Constant(static_cast<int32_t>(                             \
            CompareOperationFeedback::TypeIndex::k##StubType)));        \
  }

DEF_TYPED_OBJECT_EQUALITY(StrictEqual, Symbol, IsSymbol)
DEF_TYPED_OBJECT_EQUALITY(StrictEqual, Receiver, IsJSReceiver)
DEF_TYPED_OBJECT_EQUALITY(StrictEqual, InternalizedString, IsInternalizedString)
DEF_TYPED_OBJECT_EQUALITY(Equal, Receiver, IsJSReceiver)
DEF_TYPED_OBJECT_EQUALITY(Equal, InternalizedString, IsInternalizedString)
#undef DEF_TYPED_OBJECT_EQUALITY

TF_BUILTIN(StrictEqual_Any_Baseline, CodeStubAssembler) {
  auto lhs = Parameter<Object>(Descriptor::kLeft);
  auto rhs = Parameter<Object>(Descriptor::kRight);

  TNode<Boolean> result = StrictEqual(lhs, rhs);
  Return(result);
}

TF_BUILTIN(Equal_Any_Baseline, CodeStubAssembler) {
  auto lhs = Parameter<Object>(Descriptor::kLeft);
  auto rhs = Parameter<Object>(Descriptor::kRight);

  TVARIABLE(Object, var_exception);
  Label if_exception(this, Label::kDeferred);
  TNode<Boolean> result;
  {
    ScopedExceptionHandler handler(this, &if_exception, &var_exception);
    result = Equal(lhs, rhs, [&]() { return LoadContextFromBaseline(); });
  }
  Return(result);

  BIND(&if_exception);
  {
    CallRuntime(Runtime::kReThrow, LoadContextFromBaseline(),
                var_exception.value());
    Unreachable();
  }
}

#define DEF_TYPED_RELATIONAL_COMPARE(Name)                              \
  TF_BUILTIN(Name##AndTryPatchCode, CodeStubAssembler) {                \
    auto lhs = Parameter<Object>(Descriptor::kLeft);                    \
    auto rhs = Parameter<Object>(Descriptor::kRight);                   \
    auto current_feedback =                                             \
        UncheckedParameter<Int32T>(Descriptor::kCurrentFeedback);       \
    auto feedback_offset =                                              \
        UncheckedParameter<UintPtrT>(Descriptor::kFeedbackOffset);      \
                                                                        \
    Generate##Name##AndTryPatchCode(lhs, rhs, current_feedback,         \
                                    feedback_offset);                   \
  }                                                                     \
                                                                        \
  TF_BUILTIN(Name##_None_Baseline, CodeStubAssembler) {                 \
    auto lhs = Parameter<Object>(Descriptor::kLeft);                    \
    auto rhs = Parameter<Object>(Descriptor::kRight);                   \
    auto feedback_offset =                                              \
        UncheckedParameter<IntPtrT>(Descriptor::kFeedbackOffset);       \
                                                                        \
    TailCallBuiltin(Builtin::k##Name##AndTryPatchCode,                  \
                    LoadContextFromBaseline(), lhs, rhs,                \
                    Int32Constant(static_cast<int32_t>(                 \
                        CompareOperationFeedback::TypeIndex::kNone)),   \
                    feedback_offset);                                   \
  }                                                                     \
                                                                        \
  TF_BUILTIN(Name##_SignedSmall_Baseline, CodeStubAssembler) {          \
    auto lhs = Parameter<Object>(Descriptor::kLeft);                    \
    auto rhs = Parameter<Object>(Descriptor::kRight);                   \
    auto feedback_offset =                                              \
        UncheckedParameter<UintPtrT>(Descriptor::kFeedbackOffset);      \
                                                                        \
    GenerateSmiRelationalCompare(Operation::k##Name, lhs, rhs,          \
                                 feedback_offset,                       \
                                 Builtin::k##Name##AndTryPatchCode);    \
  }                                                                     \
                                                                        \
  TF_BUILTIN(Name##_Number_Baseline, CodeStubAssembler) {               \
    auto lhs = Parameter<Object>(Descriptor::kLeft);                    \
    auto rhs = Parameter<Object>(Descriptor::kRight);                   \
    auto feedback_offset =                                              \
        UncheckedParameter<UintPtrT>(Descriptor::kFeedbackOffset);      \
                                                                        \
    GenerateNumberRelationalCompare(Operation::k##Name, lhs, rhs,       \
                                    feedback_offset,                    \
                                    Builtin::k##Name##AndTryPatchCode); \
  }

DEF_TYPED_RELATIONAL_COMPARE(LessThan)
DEF_TYPED_RELATIONAL_COMPARE(LessThanOrEqual)
DEF_TYPED_RELATIONAL_COMPARE(GreaterThan)
DEF_TYPED_RELATIONAL_COMPARE(GreaterThanOrEqual)
#undef DEF_TYPED_RELATIONAL_COMPARE

// TODO(yuheng): The *Smi version bytecodes reuse these typed stubs instead of a
// dedicated typed *Smi family for smaller code size, but this will introduce
// one unnecessary rhs Smi check. Adding a parallel typed *Smi family is a
// possible future experiment to remove that extra branch.
#define DEF_TYPED_BINOP(OpName)                                             \
  TF_BUILTIN(OpName##AndTryPatchCode, CodeStubAssembler) {                  \
    auto lhs = Parameter<Object>(Descriptor::kLeft);                        \
    auto rhs = Parameter<Object>(Descriptor::kRight);                       \
    auto current_feedback =                                                 \
        UncheckedParameter<Int32T>(Descriptor::kCurrentFeedback);           \
    auto feedback_offset =                                                  \
        UncheckedParameter<UintPtrT>(Descriptor::kFeedbackOffset);          \
    GenerateBinaryOpAndTryPatchCode(Operation::k##OpName, lhs, rhs,         \
                                    current_feedback, feedback_offset);     \
  }                                                                         \
                                                                            \
  TF_BUILTIN(OpName##_None_Baseline, CodeStubAssembler) {                   \
    auto lhs = Parameter<Object>(Descriptor::kLeft);                        \
    auto rhs = Parameter<Object>(Descriptor::kRight);                       \
    auto feedback_offset =                                                  \
        UncheckedParameter<UintPtrT>(Descriptor::kFeedbackOffset);          \
    TailCallBuiltin(Builtin::k##OpName##AndTryPatchCode,                    \
                    LoadContextFromBaseline(), lhs, rhs,                    \
                    Int32Constant(static_cast<int32_t>(                     \
                        BinaryOperationFeedback::TypeIndex::kNone)),        \
                    feedback_offset);                                       \
  }                                                                         \
                                                                            \
  TF_BUILTIN(OpName##_SignedSmall_Baseline, CodeStubAssembler) {            \
    auto lhs = Parameter<Object>(Descriptor::kLeft);                        \
    auto rhs = Parameter<Object>(Descriptor::kRight);                       \
    auto feedback_offset =                                                  \
        UncheckedParameter<UintPtrT>(Descriptor::kFeedbackOffset);          \
    GenerateSmiBinaryOp(Operation::k##OpName, lhs, rhs, feedback_offset,    \
                        Builtin::k##OpName##AndTryPatchCode);               \
  }                                                                         \
                                                                            \
  TF_BUILTIN(OpName##_Number_Baseline, CodeStubAssembler) {                 \
    auto lhs = Parameter<Object>(Descriptor::kLeft);                        \
    auto rhs = Parameter<Object>(Descriptor::kRight);                       \
    auto feedback_offset =                                                  \
        UncheckedParameter<UintPtrT>(Descriptor::kFeedbackOffset);          \
    GenerateNumberBinaryOp(Operation::k##OpName, lhs, rhs, feedback_offset, \
                           Builtin::k##OpName##AndTryPatchCode);            \
  }
DEF_TYPED_BINOP(Add)
DEF_TYPED_BINOP(Subtract)
DEF_TYPED_BINOP(Multiply)
DEF_TYPED_BINOP(Divide)
DEF_TYPED_BINOP(Modulus)
#undef DEF_TYPED_BINOP

TF_BUILTIN(Add_String_Baseline, CodeStubAssembler) {
  auto lhs = Parameter<Object>(Descriptor::kLeft);
  auto rhs = Parameter<Object>(Descriptor::kRight);
  auto feedback_offset =
      UncheckedParameter<UintPtrT>(Descriptor::kFeedbackOffset);
  GenerateStringAdd(lhs, rhs, feedback_offset, Builtin::kAddAndTryPatchCode);
}

TF_BUILTIN(ExponentiateAndTryPatchCode, CodeStubAssembler) {
  auto lhs = Parameter<Object>(Descriptor::kLeft);
  auto rhs = Parameter<Object>(Descriptor::kRight);
  auto current_feedback =
      UncheckedParameter<Int32T>(Descriptor::kCurrentFeedback);
  auto feedback_offset =
      UncheckedParameter<UintPtrT>(Descriptor::kFeedbackOffset);
  GenerateBinaryOpAndTryPatchCode(Operation::kExponentiate, lhs, rhs,
                                  current_feedback, feedback_offset);
}

TF_BUILTIN(Exponentiate_None_Baseline, CodeStubAssembler) {
  auto lhs = Parameter<Object>(Descriptor::kLeft);
  auto rhs = Parameter<Object>(Descriptor::kRight);
  auto feedback_offset =
      UncheckedParameter<UintPtrT>(Descriptor::kFeedbackOffset);
  TailCallBuiltin(Builtin::kExponentiateAndTryPatchCode,
                  LoadContextFromBaseline(), lhs, rhs,
                  Int32Constant(static_cast<int32_t>(
                      BinaryOperationFeedback::TypeIndex::kNone)),
                  feedback_offset);
}

TF_BUILTIN(Exponentiate_Number_Baseline, CodeStubAssembler) {
  auto lhs = Parameter<Object>(Descriptor::kLeft);
  auto rhs = Parameter<Object>(Descriptor::kRight);
  auto feedback_offset =
      UncheckedParameter<UintPtrT>(Descriptor::kFeedbackOffset);
  GenerateNumberBinaryOp(Operation::kExponentiate, lhs, rhs, feedback_offset,
                         Builtin::kExponentiateAndTryPatchCode);
}

#define DEF_TYPED_BITWISE_BINOP(OpName)                                  \
  TF_BUILTIN(OpName##AndTryPatchCode, CodeStubAssembler) {               \
    auto lhs = Parameter<Object>(Descriptor::kLeft);                     \
    auto rhs = Parameter<Object>(Descriptor::kRight);                    \
    auto current_feedback =                                              \
        UncheckedParameter<Int32T>(Descriptor::kCurrentFeedback);        \
    auto feedback_offset =                                               \
        UncheckedParameter<UintPtrT>(Descriptor::kFeedbackOffset);       \
    GenerateBinaryOpAndTryPatchCode(Operation::k##OpName, lhs, rhs,      \
                                    current_feedback, feedback_offset);  \
  }                                                                      \
                                                                         \
  TF_BUILTIN(OpName##_None_Baseline, CodeStubAssembler) {                \
    auto lhs = Parameter<Object>(Descriptor::kLeft);                     \
    auto rhs = Parameter<Object>(Descriptor::kRight);                    \
    auto feedback_offset =                                               \
        UncheckedParameter<UintPtrT>(Descriptor::kFeedbackOffset);       \
    TailCallBuiltin(Builtin::k##OpName##AndTryPatchCode,                 \
                    LoadContextFromBaseline(), lhs, rhs,                 \
                    Int32Constant(static_cast<int32_t>(                  \
                        BinaryOperationFeedback::TypeIndex::kNone)),     \
                    feedback_offset);                                    \
  }                                                                      \
                                                                         \
  TF_BUILTIN(OpName##_SignedSmall_Baseline, CodeStubAssembler) {         \
    auto lhs = Parameter<Object>(Descriptor::kLeft);                     \
    auto rhs = Parameter<Object>(Descriptor::kRight);                    \
    auto feedback_offset =                                               \
        UncheckedParameter<UintPtrT>(Descriptor::kFeedbackOffset);       \
    GenerateSmiBinaryOp(Operation::k##OpName, lhs, rhs, feedback_offset, \
                        Builtin::k##OpName##AndTryPatchCode);            \
  }
DEF_TYPED_BITWISE_BINOP(BitwiseOr)
DEF_TYPED_BITWISE_BINOP(BitwiseXor)
DEF_TYPED_BITWISE_BINOP(BitwiseAnd)
DEF_TYPED_BITWISE_BINOP(ShiftLeft)
DEF_TYPED_BITWISE_BINOP(ShiftRight)
DEF_TYPED_BITWISE_BINOP(ShiftRightLogical)
#undef DEF_TYPED_BITWISE_BINOP

#endif  // V8_ENABLE_SPARKPLUG_PLUS

#include "src/codegen/undef-code-stub-assembler-macros.inc"

}  // namespace internal
}  // namespace v8
