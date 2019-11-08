// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ic/binary-op-assembler.h"

#include "src/common/globals.h"

namespace v8 {
namespace internal {

TNode<Object> BinaryOpAssembler::Generate_AddWithFeedback(
    TNode<Context> context, TNode<Object> lhs, TNode<Object> rhs,
    TNode<UintPtrT> slot_id, TNode<HeapObject> maybe_feedback_vector,
    bool rhs_known_smi) {
  // Shared entry for floating point addition.
  Label do_fadd(this), if_lhsisnotnumber(this, Label::kDeferred),
      check_rhsisoddball(this, Label::kDeferred),
      call_with_oddball_feedback(this), call_with_any_feedback(this),
      call_add_stub(this), end(this), bigint(this, Label::kDeferred);
  TVARIABLE(Float64T, var_fadd_lhs);
  TVARIABLE(Float64T, var_fadd_rhs);
  TVARIABLE(Smi, var_type_feedback);
  TVARIABLE(Object, var_result);

  // Check if the {lhs} is a Smi or a HeapObject.
  Label if_lhsissmi(this);
  // If rhs is known to be an Smi we want to fast path Smi operation. This is
  // for AddSmi operation. For the normal Add operation, we want to fast path
  // both Smi and Number operations, so this path should not be marked as
  // Deferred.
  Label if_lhsisnotsmi(this,
                       rhs_known_smi ? Label::kDeferred : Label::kNonDeferred);
  Branch(TaggedIsNotSmi(lhs), &if_lhsisnotsmi, &if_lhsissmi);

  BIND(&if_lhsissmi);
  {
    Comment("lhs is Smi");
    TNode<Smi> lhs_smi = CAST(lhs);
    if (!rhs_known_smi) {
      // Check if the {rhs} is also a Smi.
      Label if_rhsissmi(this), if_rhsisnotsmi(this);
      Branch(TaggedIsSmi(rhs), &if_rhsissmi, &if_rhsisnotsmi);

      BIND(&if_rhsisnotsmi);
      {
        // Check if the {rhs} is a HeapNumber.
        TNode<HeapObject> rhs_heap_object = CAST(rhs);
        GotoIfNot(IsHeapNumber(rhs_heap_object), &check_rhsisoddball);

        var_fadd_lhs = SmiToFloat64(lhs_smi);
        var_fadd_rhs = LoadHeapNumberValue(rhs_heap_object);
        Goto(&do_fadd);
      }

      BIND(&if_rhsissmi);
    }

    {
      Comment("perform smi operation");
      // If rhs is known to be an Smi we want to fast path Smi operation. This
      // is for AddSmi operation. For the normal Add operation, we want to fast
      // path both Smi and Number operations, so this path should not be marked
      // as Deferred.
      TNode<Smi> rhs_smi = CAST(rhs);
      Label if_overflow(this,
                        rhs_known_smi ? Label::kDeferred : Label::kNonDeferred);
      TNode<Smi> smi_result = TrySmiAdd(lhs_smi, rhs_smi, &if_overflow);
      // Not overflowed.
      {
        var_type_feedback = SmiConstant(BinaryOperationFeedback::kSignedSmall);
        var_result = smi_result;
        Goto(&end);
      }

      BIND(&if_overflow);
      {
        var_fadd_lhs = SmiToFloat64(lhs_smi);
        var_fadd_rhs = SmiToFloat64(rhs_smi);
        Goto(&do_fadd);
      }
    }
  }

  BIND(&if_lhsisnotsmi);
  {
    // Check if {lhs} is a HeapNumber.
    TNode<HeapObject> lhs_heap_object = CAST(lhs);
    GotoIfNot(IsHeapNumber(lhs_heap_object), &if_lhsisnotnumber);

    if (!rhs_known_smi) {
      // Check if the {rhs} is Smi.
      Label if_rhsissmi(this), if_rhsisnotsmi(this);
      Branch(TaggedIsSmi(rhs), &if_rhsissmi, &if_rhsisnotsmi);

      BIND(&if_rhsisnotsmi);
      {
        // Check if the {rhs} is a HeapNumber.
        TNode<HeapObject> rhs_heap_object = CAST(rhs);
        GotoIfNot(IsHeapNumber(rhs_heap_object), &check_rhsisoddball);

        var_fadd_lhs = LoadHeapNumberValue(lhs_heap_object);
        var_fadd_rhs = LoadHeapNumberValue(rhs_heap_object);
        Goto(&do_fadd);
      }

      BIND(&if_rhsissmi);
    }
    {
      var_fadd_lhs = LoadHeapNumberValue(lhs_heap_object);
      var_fadd_rhs = SmiToFloat64(CAST(rhs));
      Goto(&do_fadd);
    }
  }

  BIND(&do_fadd);
  {
    var_type_feedback = SmiConstant(BinaryOperationFeedback::kNumber);
    TNode<Float64T> value =
        Float64Add(var_fadd_lhs.value(), var_fadd_rhs.value());
    TNode<HeapNumber> result = AllocateHeapNumberWithValue(value);
    var_result = result;
    Goto(&end);
  }

  BIND(&if_lhsisnotnumber);
  {
    // No checks on rhs are done yet. We just know lhs is not a number or Smi.
    Label if_lhsisoddball(this), if_lhsisnotoddball(this);
    TNode<Uint16T> lhs_instance_type = LoadInstanceType(CAST(lhs));
    TNode<BoolT> lhs_is_oddball =
        InstanceTypeEqual(lhs_instance_type, ODDBALL_TYPE);
    Branch(lhs_is_oddball, &if_lhsisoddball, &if_lhsisnotoddball);

    BIND(&if_lhsisoddball);
    {
      GotoIf(TaggedIsSmi(rhs), &call_with_oddball_feedback);

      // Check if {rhs} is a HeapNumber.
      Branch(IsHeapNumber(CAST(rhs)), &call_with_oddball_feedback,
             &check_rhsisoddball);
    }

    BIND(&if_lhsisnotoddball);
    {
      // Check if the {rhs} is a smi, and exit the string and bigint check early
      // if it is.
      GotoIf(TaggedIsSmi(rhs), &call_with_any_feedback);
      TNode<HeapObject> rhs_heap_object = CAST(rhs);

      Label lhs_is_string(this), lhs_is_bigint(this);
      GotoIf(IsStringInstanceType(lhs_instance_type), &lhs_is_string);
      GotoIf(IsBigIntInstanceType(lhs_instance_type), &lhs_is_bigint);
      Goto(&call_with_any_feedback);

      BIND(&lhs_is_bigint);
      Branch(IsBigInt(rhs_heap_object), &bigint, &call_with_any_feedback);

      BIND(&lhs_is_string);
      {
        TNode<Uint16T> rhs_instance_type = LoadInstanceType(rhs_heap_object);

        // Exit unless {rhs} is a string. Since {lhs} is a string we no longer
        // need an Oddball check.
        GotoIfNot(IsStringInstanceType(rhs_instance_type),
                  &call_with_any_feedback);

        var_type_feedback = SmiConstant(BinaryOperationFeedback::kString);
        var_result =
            CallBuiltin(Builtins::kStringAdd_CheckNone, context, lhs, rhs);

        Goto(&end);
      }
    }
  }

  BIND(&check_rhsisoddball);
  {
    // Check if rhs is an oddball. At this point we know lhs is either a
    // Smi or number or oddball and rhs is not a number or Smi.
    TNode<Uint16T> rhs_instance_type = LoadInstanceType(CAST(rhs));
    TNode<BoolT> rhs_is_oddball =
        InstanceTypeEqual(rhs_instance_type, ODDBALL_TYPE);
    GotoIf(rhs_is_oddball, &call_with_oddball_feedback);
    Goto(&call_with_any_feedback);
  }

  BIND(&bigint);
  {
    // Both {lhs} and {rhs} are of BigInt type.
    Label bigint_too_big(this);
    var_result = CallBuiltin(Builtins::kBigIntAddNoThrow, context, lhs, rhs);
    // Check for sentinel that signals BigIntTooBig exception.
    GotoIf(TaggedIsSmi(var_result.value()), &bigint_too_big);

    var_type_feedback = SmiConstant(BinaryOperationFeedback::kBigInt);
    Goto(&end);

    BIND(&bigint_too_big);
    {
      // Update feedback to prevent deopt loop.
      UpdateFeedback(SmiConstant(BinaryOperationFeedback::kAny),
                     maybe_feedback_vector, slot_id);
      ThrowRangeError(context, MessageTemplate::kBigIntTooBig);
    }
  }

  BIND(&call_with_oddball_feedback);
  {
    var_type_feedback = SmiConstant(BinaryOperationFeedback::kNumberOrOddball);
    Goto(&call_add_stub);
  }

  BIND(&call_with_any_feedback);
  {
    var_type_feedback = SmiConstant(BinaryOperationFeedback::kAny);
    Goto(&call_add_stub);
  }

  BIND(&call_add_stub);
  {
    var_result = CallBuiltin(Builtins::kAdd, context, lhs, rhs);
    Goto(&end);
  }

  BIND(&end);
  UpdateFeedback(var_type_feedback.value(), maybe_feedback_vector, slot_id);
  return var_result.value();
}

TNode<Object> BinaryOpAssembler::Generate_BinaryOperationWithFeedback(
    TNode<Context> context, TNode<Object> lhs, TNode<Object> rhs,
    TNode<UintPtrT> slot_id, TNode<HeapObject> maybe_feedback_vector,
    const SmiOperation& smiOperation, const FloatOperation& floatOperation,
    Operation op, bool rhs_known_smi) {
  Label do_float_operation(this), end(this), call_stub(this),
      check_rhsisoddball(this, Label::kDeferred), call_with_any_feedback(this),
      if_lhsisnotnumber(this, Label::kDeferred),
      if_bigint(this, Label::kDeferred);
  TVARIABLE(Float64T, var_float_lhs);
  TVARIABLE(Float64T, var_float_rhs);
  TVARIABLE(Smi, var_type_feedback);
  TVARIABLE(Object, var_result);

  Label if_lhsissmi(this);
  // If rhs is known to be an Smi (in the SubSmi, MulSmi, DivSmi, ModSmi
  // bytecode handlers) we want to fast path Smi operation. For the normal
  // operation, we want to fast path both Smi and Number operations, so this
  // path should not be marked as Deferred.
  Label if_lhsisnotsmi(this,
                       rhs_known_smi ? Label::kDeferred : Label::kNonDeferred);
  Branch(TaggedIsNotSmi(lhs), &if_lhsisnotsmi, &if_lhsissmi);

  // Check if the {lhs} is a Smi or a HeapObject.
  BIND(&if_lhsissmi);
  {
    Comment("lhs is Smi");
    TNode<Smi> lhs_smi = CAST(lhs);
    if (!rhs_known_smi) {
      // Check if the {rhs} is also a Smi.
      Label if_rhsissmi(this), if_rhsisnotsmi(this);
      Branch(TaggedIsSmi(rhs), &if_rhsissmi, &if_rhsisnotsmi);

      BIND(&if_rhsisnotsmi);
      {
        // Check if {rhs} is a HeapNumber.
        TNode<HeapObject> rhs_heap_object = CAST(rhs);
        GotoIfNot(IsHeapNumber(rhs_heap_object), &check_rhsisoddball);

        // Perform a floating point operation.
        var_float_lhs = SmiToFloat64(lhs_smi);
        var_float_rhs = LoadHeapNumberValue(rhs_heap_object);
        Goto(&do_float_operation);
      }

      BIND(&if_rhsissmi);
    }

    {
      Comment("perform smi operation");
      var_result = smiOperation(lhs_smi, CAST(rhs), &var_type_feedback);
      Goto(&end);
    }
  }

  BIND(&if_lhsisnotsmi);
  {
    Comment("lhs is not Smi");
    // Check if the {lhs} is a HeapNumber.
    TNode<HeapObject> lhs_heap_object = CAST(lhs);
    GotoIfNot(IsHeapNumber(lhs_heap_object), &if_lhsisnotnumber);

    if (!rhs_known_smi) {
      // Check if the {rhs} is a Smi.
      Label if_rhsissmi(this), if_rhsisnotsmi(this);
      Branch(TaggedIsSmi(rhs), &if_rhsissmi, &if_rhsisnotsmi);

      BIND(&if_rhsisnotsmi);
      {
        // Check if the {rhs} is a HeapNumber.
        TNode<HeapObject> rhs_heap_object = CAST(rhs);
        GotoIfNot(IsHeapNumber(rhs_heap_object), &check_rhsisoddball);

        // Perform a floating point operation.
        var_float_lhs = LoadHeapNumberValue(lhs_heap_object);
        var_float_rhs = LoadHeapNumberValue(rhs_heap_object);
        Goto(&do_float_operation);
      }

      BIND(&if_rhsissmi);
    }

    {
      // Perform floating point operation.
      var_float_lhs = LoadHeapNumberValue(lhs_heap_object);
      var_float_rhs = SmiToFloat64(CAST(rhs));
      Goto(&do_float_operation);
    }
  }

  BIND(&do_float_operation);
  {
    var_type_feedback = SmiConstant(BinaryOperationFeedback::kNumber);
    TNode<Float64T> lhs_value = var_float_lhs.value();
    TNode<Float64T> rhs_value = var_float_rhs.value();
    TNode<Float64T> value = floatOperation(lhs_value, rhs_value);
    var_result = AllocateHeapNumberWithValue(value);
    Goto(&end);
  }

  BIND(&if_lhsisnotnumber);
  {
    // No checks on rhs are done yet. We just know lhs is not a number or Smi.
    Label if_left_bigint(this), if_left_oddball(this);
    TNode<Uint16T> lhs_instance_type = LoadInstanceType(CAST(lhs));
    GotoIf(IsBigIntInstanceType(lhs_instance_type), &if_left_bigint);
    TNode<BoolT> lhs_is_oddball =
        InstanceTypeEqual(lhs_instance_type, ODDBALL_TYPE);
    Branch(lhs_is_oddball, &if_left_oddball, &call_with_any_feedback);

    BIND(&if_left_oddball);
    {
      Label if_rhsissmi(this), if_rhsisnotsmi(this);
      Branch(TaggedIsSmi(rhs), &if_rhsissmi, &if_rhsisnotsmi);

      BIND(&if_rhsissmi);
      {
        var_type_feedback =
            SmiConstant(BinaryOperationFeedback::kNumberOrOddball);
        Goto(&call_stub);
      }

      BIND(&if_rhsisnotsmi);
      {
        // Check if {rhs} is a HeapNumber.
        GotoIfNot(IsHeapNumber(CAST(rhs)), &check_rhsisoddball);

        var_type_feedback =
            SmiConstant(BinaryOperationFeedback::kNumberOrOddball);
        Goto(&call_stub);
      }
    }

    BIND(&if_left_bigint);
    {
      GotoIf(TaggedIsSmi(rhs), &call_with_any_feedback);
      Branch(IsBigInt(CAST(rhs)), &if_bigint, &call_with_any_feedback);
    }
  }

  BIND(&check_rhsisoddball);
  {
    // Check if rhs is an oddball. At this point we know lhs is either a
    // Smi or number or oddball and rhs is not a number or Smi.
    TNode<Uint16T> rhs_instance_type = LoadInstanceType(CAST(rhs));
    GotoIf(IsBigIntInstanceType(rhs_instance_type), &if_bigint);
    TNode<BoolT> rhs_is_oddball =
        InstanceTypeEqual(rhs_instance_type, ODDBALL_TYPE);
    GotoIfNot(rhs_is_oddball, &call_with_any_feedback);

    var_type_feedback = SmiConstant(BinaryOperationFeedback::kNumberOrOddball);
    Goto(&call_stub);
  }

  // This handles the case where at least one input is a BigInt.
  BIND(&if_bigint);
  {
    var_type_feedback = SmiConstant(BinaryOperationFeedback::kBigInt);
    if (op == Operation::kAdd) {
      var_result = CallBuiltin(Builtins::kBigIntAdd, context, lhs, rhs);
    } else {
      var_result = CallRuntime(Runtime::kBigIntBinaryOp, context, lhs, rhs,
                               SmiConstant(op));
    }
    Goto(&end);
  }

  BIND(&call_with_any_feedback);
  {
    var_type_feedback = SmiConstant(BinaryOperationFeedback::kAny);
    Goto(&call_stub);
  }

  BIND(&call_stub);
  {
    TNode<Object> result;
    switch (op) {
      case Operation::kSubtract:
        result = CallBuiltin(Builtins::kSubtract, context, lhs, rhs);
        break;
      case Operation::kMultiply:
        result = CallBuiltin(Builtins::kMultiply, context, lhs, rhs);
        break;
      case Operation::kDivide:
        result = CallBuiltin(Builtins::kDivide, context, lhs, rhs);
        break;
      case Operation::kModulus:
        result = CallBuiltin(Builtins::kModulus, context, lhs, rhs);
        break;
      default:
        UNREACHABLE();
    }
    var_result = result;
    Goto(&end);
  }

  BIND(&end);
  UpdateFeedback(var_type_feedback.value(), maybe_feedback_vector, slot_id);
  return var_result.value();
}

TNode<Object> BinaryOpAssembler::Generate_SubtractWithFeedback(
    TNode<Context> context, TNode<Object> lhs, TNode<Object> rhs,
    TNode<UintPtrT> slot_id, TNode<HeapObject> maybe_feedback_vector,
    bool rhs_known_smi) {
  auto smiFunction = [=](TNode<Smi> lhs, TNode<Smi> rhs,
                         TVariable<Smi>* var_type_feedback) {
    Label end(this);
    TVARIABLE(Number, var_result);
    // If rhs is known to be an Smi (for SubSmi) we want to fast path Smi
    // operation. For the normal Sub operation, we want to fast path both
    // Smi and Number operations, so this path should not be marked as Deferred.
    Label if_overflow(this,
                      rhs_known_smi ? Label::kDeferred : Label::kNonDeferred);
    var_result = TrySmiSub(lhs, rhs, &if_overflow);
    *var_type_feedback = SmiConstant(BinaryOperationFeedback::kSignedSmall);
    Goto(&end);

    BIND(&if_overflow);
    {
      *var_type_feedback = SmiConstant(BinaryOperationFeedback::kNumber);
      TNode<Float64T> value = Float64Sub(SmiToFloat64(lhs), SmiToFloat64(rhs));
      var_result = AllocateHeapNumberWithValue(value);
      Goto(&end);
    }

    BIND(&end);
    return var_result.value();
  };
  auto floatFunction = [=](TNode<Float64T> lhs, TNode<Float64T> rhs) {
    return Float64Sub(lhs, rhs);
  };
  return Generate_BinaryOperationWithFeedback(
      context, lhs, rhs, slot_id, maybe_feedback_vector, smiFunction,
      floatFunction, Operation::kSubtract, rhs_known_smi);
}

TNode<Object> BinaryOpAssembler::Generate_MultiplyWithFeedback(
    TNode<Context> context, TNode<Object> lhs, TNode<Object> rhs,
    TNode<UintPtrT> slot_id, TNode<HeapObject> maybe_feedback_vector,
    bool rhs_known_smi) {
  auto smiFunction = [=](TNode<Smi> lhs, TNode<Smi> rhs,
                         TVariable<Smi>* var_type_feedback) {
    TNode<Number> result = SmiMul(lhs, rhs);
    *var_type_feedback = SelectSmiConstant(
        TaggedIsSmi(result), BinaryOperationFeedback::kSignedSmall,
        BinaryOperationFeedback::kNumber);
    return result;
  };
  auto floatFunction = [=](TNode<Float64T> lhs, TNode<Float64T> rhs) {
    return Float64Mul(lhs, rhs);
  };
  return Generate_BinaryOperationWithFeedback(
      context, lhs, rhs, slot_id, maybe_feedback_vector, smiFunction,
      floatFunction, Operation::kMultiply, rhs_known_smi);
}

TNode<Object> BinaryOpAssembler::Generate_DivideWithFeedback(
    TNode<Context> context, TNode<Object> dividend, TNode<Object> divisor,
    TNode<UintPtrT> slot_id, TNode<HeapObject> maybe_feedback_vector,
    bool rhs_known_smi) {
  auto smiFunction = [=](TNode<Smi> lhs, TNode<Smi> rhs,
                         TVariable<Smi>* var_type_feedback) {
    TVARIABLE(Object, var_result);
    // If rhs is known to be an Smi (for DivSmi) we want to fast path Smi
    // operation. For the normal Div operation, we want to fast path both
    // Smi and Number operations, so this path should not be marked as Deferred.
    Label bailout(this, rhs_known_smi ? Label::kDeferred : Label::kNonDeferred),
        end(this);
    var_result = TrySmiDiv(lhs, rhs, &bailout);
    *var_type_feedback = SmiConstant(BinaryOperationFeedback::kSignedSmall);
    Goto(&end);

    BIND(&bailout);
    {
      *var_type_feedback =
          SmiConstant(BinaryOperationFeedback::kSignedSmallInputs);
      TNode<Float64T> value = Float64Div(SmiToFloat64(lhs), SmiToFloat64(rhs));
      var_result = AllocateHeapNumberWithValue(value);
      Goto(&end);
    }

    BIND(&end);
    return var_result.value();
  };
  auto floatFunction = [=](TNode<Float64T> lhs, TNode<Float64T> rhs) {
    return Float64Div(lhs, rhs);
  };
  return Generate_BinaryOperationWithFeedback(
      context, dividend, divisor, slot_id, maybe_feedback_vector, smiFunction,
      floatFunction, Operation::kDivide, rhs_known_smi);
}

TNode<Object> BinaryOpAssembler::Generate_ModulusWithFeedback(
    TNode<Context> context, TNode<Object> dividend, TNode<Object> divisor,
    TNode<UintPtrT> slot_id, TNode<HeapObject> maybe_feedback_vector,
    bool rhs_known_smi) {
  auto smiFunction = [=](TNode<Smi> lhs, TNode<Smi> rhs,
                         TVariable<Smi>* var_type_feedback) {
    TNode<Number> result = SmiMod(lhs, rhs);
    *var_type_feedback = SelectSmiConstant(
        TaggedIsSmi(result), BinaryOperationFeedback::kSignedSmall,
        BinaryOperationFeedback::kNumber);
    return result;
  };
  auto floatFunction = [=](TNode<Float64T> lhs, TNode<Float64T> rhs) {
    return Float64Mod(lhs, rhs);
  };
  return Generate_BinaryOperationWithFeedback(
      context, dividend, divisor, slot_id, maybe_feedback_vector, smiFunction,
      floatFunction, Operation::kModulus, rhs_known_smi);
}

TNode<Object> BinaryOpAssembler::Generate_ExponentiateWithFeedback(
    TNode<Context> context, TNode<Object> base, TNode<Object> exponent,
    TNode<UintPtrT> slot_id, TNode<HeapObject> maybe_feedback_vector,
    bool rhs_known_smi) {
  // We currently don't optimize exponentiation based on feedback.
  TNode<Smi> dummy_feedback = SmiConstant(BinaryOperationFeedback::kAny);
  UpdateFeedback(dummy_feedback, maybe_feedback_vector, slot_id);
  return CallBuiltin(Builtins::kExponentiate, context, base, exponent);
}

}  // namespace internal
}  // namespace v8
