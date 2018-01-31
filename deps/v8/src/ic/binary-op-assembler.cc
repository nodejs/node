// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ic/binary-op-assembler.h"

#include "src/globals.h"

namespace v8 {
namespace internal {

using compiler::Node;

Node* BinaryOpAssembler::Generate_AddWithFeedback(Node* context, Node* lhs,
                                                  Node* rhs, Node* slot_id,
                                                  Node* feedback_vector,
                                                  bool rhs_is_smi) {
  // Shared entry for floating point addition.
  Label do_fadd(this), if_lhsisnotnumber(this, Label::kDeferred),
      check_rhsisoddball(this, Label::kDeferred),
      call_with_oddball_feedback(this), call_with_any_feedback(this),
      call_add_stub(this), end(this), bigint(this, Label::kDeferred);
  VARIABLE(var_fadd_lhs, MachineRepresentation::kFloat64);
  VARIABLE(var_fadd_rhs, MachineRepresentation::kFloat64);
  VARIABLE(var_type_feedback, MachineRepresentation::kTaggedSigned);
  VARIABLE(var_result, MachineRepresentation::kTagged);

  // Check if the {lhs} is a Smi or a HeapObject.
  Label if_lhsissmi(this);
  // If rhs is known to be an Smi we want to fast path Smi operation. This is
  // for AddSmi operation. For the normal Add operation, we want to fast path
  // both Smi and Number operations, so this path should not be marked as
  // Deferred.
  Label if_lhsisnotsmi(this,
                       rhs_is_smi ? Label::kDeferred : Label::kNonDeferred);
  Branch(TaggedIsNotSmi(lhs), &if_lhsisnotsmi, &if_lhsissmi);

  BIND(&if_lhsissmi);
  {
    Comment("lhs is Smi");
    if (!rhs_is_smi) {
      // Check if the {rhs} is also a Smi.
      Label if_rhsissmi(this), if_rhsisnotsmi(this);
      Branch(TaggedIsSmi(rhs), &if_rhsissmi, &if_rhsisnotsmi);

      BIND(&if_rhsisnotsmi);
      {
        // Check if the {rhs} is a HeapNumber.
        GotoIfNot(IsHeapNumber(rhs), &check_rhsisoddball);

        var_fadd_lhs.Bind(SmiToFloat64(lhs));
        var_fadd_rhs.Bind(LoadHeapNumberValue(rhs));
        Goto(&do_fadd);
      }

      BIND(&if_rhsissmi);
    }

    {
      Comment("perform smi operation");
      // Try fast Smi addition first.
      Node* pair = IntPtrAddWithOverflow(BitcastTaggedToWord(lhs),
                                         BitcastTaggedToWord(rhs));
      Node* overflow = Projection(1, pair);

      // Check if the Smi additon overflowed.
      // If rhs is known to be an Smi we want to fast path Smi operation. This
      // is for AddSmi operation. For the normal Add operation, we want to fast
      // path both Smi and Number operations, so this path should not be marked
      // as Deferred.
      Label if_overflow(this,
                        rhs_is_smi ? Label::kDeferred : Label::kNonDeferred),
          if_notoverflow(this);
      Branch(overflow, &if_overflow, &if_notoverflow);

      BIND(&if_overflow);
      {
        var_fadd_lhs.Bind(SmiToFloat64(lhs));
        var_fadd_rhs.Bind(SmiToFloat64(rhs));
        Goto(&do_fadd);
      }

      BIND(&if_notoverflow);
      {
        var_type_feedback.Bind(
            SmiConstant(BinaryOperationFeedback::kSignedSmall));
        var_result.Bind(BitcastWordToTaggedSigned(Projection(0, pair)));
        Goto(&end);
      }
    }
  }

  BIND(&if_lhsisnotsmi);
  {
    // Check if {lhs} is a HeapNumber.
    GotoIfNot(IsHeapNumber(lhs), &if_lhsisnotnumber);

    if (!rhs_is_smi) {
      // Check if the {rhs} is Smi.
      Label if_rhsissmi(this), if_rhsisnotsmi(this);
      Branch(TaggedIsSmi(rhs), &if_rhsissmi, &if_rhsisnotsmi);

      BIND(&if_rhsisnotsmi);
      {
        // Check if the {rhs} is a HeapNumber.
        GotoIfNot(IsHeapNumber(rhs), &check_rhsisoddball);

        var_fadd_lhs.Bind(LoadHeapNumberValue(lhs));
        var_fadd_rhs.Bind(LoadHeapNumberValue(rhs));
        Goto(&do_fadd);
      }

      BIND(&if_rhsissmi);
    }
    {
      var_fadd_lhs.Bind(LoadHeapNumberValue(lhs));
      var_fadd_rhs.Bind(SmiToFloat64(rhs));
      Goto(&do_fadd);
    }
  }

  BIND(&do_fadd);
  {
    var_type_feedback.Bind(SmiConstant(BinaryOperationFeedback::kNumber));
    Node* value = Float64Add(var_fadd_lhs.value(), var_fadd_rhs.value());
    Node* result = AllocateHeapNumberWithValue(value);
    var_result.Bind(result);
    Goto(&end);
  }

  BIND(&if_lhsisnotnumber);
  {
    // No checks on rhs are done yet. We just know lhs is not a number or Smi.
    Label if_lhsisoddball(this), if_lhsisnotoddball(this);
    Node* lhs_instance_type = LoadInstanceType(lhs);
    Node* lhs_is_oddball = InstanceTypeEqual(lhs_instance_type, ODDBALL_TYPE);
    Branch(lhs_is_oddball, &if_lhsisoddball, &if_lhsisnotoddball);

    BIND(&if_lhsisoddball);
    {
      GotoIf(TaggedIsSmi(rhs), &call_with_oddball_feedback);

      // Check if {rhs} is a HeapNumber.
      Branch(IsHeapNumber(rhs), &call_with_oddball_feedback,
             &check_rhsisoddball);
    }

    BIND(&if_lhsisnotoddball);
    {
      Label lhs_is_string(this), lhs_is_bigint(this);
      GotoIf(IsStringInstanceType(lhs_instance_type), &lhs_is_string);
      GotoIf(IsBigIntInstanceType(lhs_instance_type), &lhs_is_bigint);
      Goto(&call_with_any_feedback);

      BIND(&lhs_is_bigint);
      {
        GotoIf(TaggedIsSmi(rhs), &call_with_any_feedback);
        Branch(IsBigInt(rhs), &bigint, &call_with_any_feedback);
      }

      BIND(&lhs_is_string);
      // Check if the {rhs} is a smi, and exit the string check early if it is.
      GotoIf(TaggedIsSmi(rhs), &call_with_any_feedback);

      Node* rhs_instance_type = LoadInstanceType(rhs);

      // Exit unless {rhs} is a string. Since {lhs} is a string we no longer
      // need an Oddball check.
      GotoIfNot(IsStringInstanceType(rhs_instance_type),
                &call_with_any_feedback);

      var_type_feedback.Bind(SmiConstant(BinaryOperationFeedback::kString));
      Callable callable =
          CodeFactory::StringAdd(isolate(), STRING_ADD_CHECK_NONE, NOT_TENURED);
      var_result.Bind(CallStub(callable, context, lhs, rhs));

      Goto(&end);
    }
  }

  BIND(&check_rhsisoddball);
  {
    // Check if rhs is an oddball. At this point we know lhs is either a
    // Smi or number or oddball and rhs is not a number or Smi.
    Node* rhs_instance_type = LoadInstanceType(rhs);
    Node* rhs_is_oddball = InstanceTypeEqual(rhs_instance_type, ODDBALL_TYPE);
    GotoIf(rhs_is_oddball, &call_with_oddball_feedback);
    Branch(IsBigIntInstanceType(rhs_instance_type), &bigint,
           &call_with_any_feedback);
  }

  BIND(&bigint);
  {
    var_type_feedback.Bind(SmiConstant(BinaryOperationFeedback::kBigInt));
    var_result.Bind(CallRuntime(Runtime::kBigIntBinaryOp, context, lhs, rhs,
                                SmiConstant(Operation::kAdd)));
    Goto(&end);
  }

  BIND(&call_with_oddball_feedback);
  {
    var_type_feedback.Bind(
        SmiConstant(BinaryOperationFeedback::kNumberOrOddball));
    Goto(&call_add_stub);
  }

  BIND(&call_with_any_feedback);
  {
    var_type_feedback.Bind(SmiConstant(BinaryOperationFeedback::kAny));
    Goto(&call_add_stub);
  }

  BIND(&call_add_stub);
  {
    var_result.Bind(CallBuiltin(Builtins::kAdd, context, lhs, rhs));
    Goto(&end);
  }

  BIND(&end);
  UpdateFeedback(var_type_feedback.value(), feedback_vector, slot_id);
  return var_result.value();
}

Node* BinaryOpAssembler::Generate_BinaryOperationWithFeedback(
    Node* context, Node* lhs, Node* rhs, Node* slot_id, Node* feedback_vector,
    const SmiOperation& smiOperation, const FloatOperation& floatOperation,
    Operation op, bool rhs_is_smi) {
  Label do_float_operation(this), end(this), call_stub(this),
      check_rhsisoddball(this, Label::kDeferred), call_with_any_feedback(this),
      if_lhsisnotnumber(this, Label::kDeferred),
      if_bigint(this, Label::kDeferred);
  VARIABLE(var_float_lhs, MachineRepresentation::kFloat64);
  VARIABLE(var_float_rhs, MachineRepresentation::kFloat64);
  VARIABLE(var_type_feedback, MachineRepresentation::kTaggedSigned);
  VARIABLE(var_result, MachineRepresentation::kTagged);

  Label if_lhsissmi(this);
  // If rhs is known to be an Smi (in the SubSmi, MulSmi, DivSmi, ModSmi
  // bytecode handlers) we want to fast path Smi operation. For the normal
  // operation, we want to fast path both Smi and Number operations, so this
  // path should not be marked as Deferred.
  Label if_lhsisnotsmi(this,
                       rhs_is_smi ? Label::kDeferred : Label::kNonDeferred);
  Branch(TaggedIsNotSmi(lhs), &if_lhsisnotsmi, &if_lhsissmi);

  // Check if the {lhs} is a Smi or a HeapObject.
  BIND(&if_lhsissmi);
  {
    Comment("lhs is Smi");
    if (!rhs_is_smi) {
      // Check if the {rhs} is also a Smi.
      Label if_rhsissmi(this), if_rhsisnotsmi(this);
      Branch(TaggedIsSmi(rhs), &if_rhsissmi, &if_rhsisnotsmi);
      BIND(&if_rhsisnotsmi);
      {
        // Check if {rhs} is a HeapNumber.
        GotoIfNot(IsHeapNumber(rhs), &check_rhsisoddball);

        // Perform a floating point operation.
        var_float_lhs.Bind(SmiToFloat64(lhs));
        var_float_rhs.Bind(LoadHeapNumberValue(rhs));
        Goto(&do_float_operation);
      }

      BIND(&if_rhsissmi);
    }

    {
      Comment("perform smi operation");
      var_result.Bind(smiOperation(lhs, rhs, &var_type_feedback));
      Goto(&end);
    }
  }

  BIND(&if_lhsisnotsmi);
  {
    Comment("lhs is not Smi");
    // Check if the {lhs} is a HeapNumber.
    GotoIfNot(IsHeapNumber(lhs), &if_lhsisnotnumber);

    if (!rhs_is_smi) {
      // Check if the {rhs} is a Smi.
      Label if_rhsissmi(this), if_rhsisnotsmi(this);
      Branch(TaggedIsSmi(rhs), &if_rhsissmi, &if_rhsisnotsmi);

      BIND(&if_rhsisnotsmi);
      {
        // Check if the {rhs} is a HeapNumber.
        GotoIfNot(IsHeapNumber(rhs), &check_rhsisoddball);

        // Perform a floating point operation.
        var_float_lhs.Bind(LoadHeapNumberValue(lhs));
        var_float_rhs.Bind(LoadHeapNumberValue(rhs));
        Goto(&do_float_operation);
      }

      BIND(&if_rhsissmi);
    }

    {
      // Perform floating point operation.
      var_float_lhs.Bind(LoadHeapNumberValue(lhs));
      var_float_rhs.Bind(SmiToFloat64(rhs));
      Goto(&do_float_operation);
    }
  }

  BIND(&do_float_operation);
  {
    var_type_feedback.Bind(SmiConstant(BinaryOperationFeedback::kNumber));
    Node* lhs_value = var_float_lhs.value();
    Node* rhs_value = var_float_rhs.value();
    Node* value = floatOperation(lhs_value, rhs_value);
    var_result.Bind(AllocateHeapNumberWithValue(value));
    Goto(&end);
  }

  BIND(&if_lhsisnotnumber);
  {
    // No checks on rhs are done yet. We just know lhs is not a number or Smi.
    Label if_left_bigint(this), if_left_oddball(this);
    Node* lhs_instance_type = LoadInstanceType(lhs);
    GotoIf(IsBigIntInstanceType(lhs_instance_type), &if_left_bigint);
    Node* lhs_is_oddball = InstanceTypeEqual(lhs_instance_type, ODDBALL_TYPE);
    Branch(lhs_is_oddball, &if_left_oddball, &call_with_any_feedback);

    BIND(&if_left_oddball);
    {
      Label if_rhsissmi(this), if_rhsisnotsmi(this);
      Branch(TaggedIsSmi(rhs), &if_rhsissmi, &if_rhsisnotsmi);

      BIND(&if_rhsissmi);
      {
        var_type_feedback.Bind(
            SmiConstant(BinaryOperationFeedback::kNumberOrOddball));
        Goto(&call_stub);
      }

      BIND(&if_rhsisnotsmi);
      {
        // Check if {rhs} is a HeapNumber.
        GotoIfNot(IsHeapNumber(rhs), &check_rhsisoddball);

        var_type_feedback.Bind(
            SmiConstant(BinaryOperationFeedback::kNumberOrOddball));
        Goto(&call_stub);
      }
    }

    BIND(&if_left_bigint);
    {
      GotoIf(TaggedIsSmi(rhs), &call_with_any_feedback);
      Branch(IsBigInt(rhs), &if_bigint, &call_with_any_feedback);
    }
  }

  BIND(&check_rhsisoddball);
  {
    // Check if rhs is an oddball. At this point we know lhs is either a
    // Smi or number or oddball and rhs is not a number or Smi.
    Node* rhs_instance_type = LoadInstanceType(rhs);
    GotoIf(IsBigIntInstanceType(rhs_instance_type), &if_bigint);
    Node* rhs_is_oddball = InstanceTypeEqual(rhs_instance_type, ODDBALL_TYPE);
    GotoIfNot(rhs_is_oddball, &call_with_any_feedback);

    var_type_feedback.Bind(
        SmiConstant(BinaryOperationFeedback::kNumberOrOddball));
    Goto(&call_stub);
  }

  // This handles the case where at least one input is a BigInt.
  BIND(&if_bigint);
  {
    var_type_feedback.Bind(SmiConstant(BinaryOperationFeedback::kBigInt));
    var_result.Bind(CallRuntime(Runtime::kBigIntBinaryOp, context, lhs, rhs,
                                SmiConstant(op)));
    Goto(&end);
  }

  BIND(&call_with_any_feedback);
  {
    var_type_feedback.Bind(SmiConstant(BinaryOperationFeedback::kAny));
    Goto(&call_stub);
  }

  BIND(&call_stub);
  {
    Node* result;
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
    var_result.Bind(result);
    Goto(&end);
  }

  BIND(&end);
  UpdateFeedback(var_type_feedback.value(), feedback_vector, slot_id);
  return var_result.value();
}

Node* BinaryOpAssembler::Generate_SubtractWithFeedback(Node* context, Node* lhs,
                                                       Node* rhs, Node* slot_id,
                                                       Node* feedback_vector,
                                                       bool rhs_is_smi) {
  auto smiFunction = [=](Node* lhs, Node* rhs, Variable* var_type_feedback) {
    VARIABLE(var_result, MachineRepresentation::kTagged);
    // Try a fast Smi subtraction first.
    Node* pair = IntPtrSubWithOverflow(BitcastTaggedToWord(lhs),
                                       BitcastTaggedToWord(rhs));
    Node* overflow = Projection(1, pair);

    // Check if the Smi subtraction overflowed.
    Label if_notoverflow(this), end(this);
    // If rhs is known to be an Smi (for SubSmi) we want to fast path Smi
    // operation. For the normal Sub operation, we want to fast path both
    // Smi and Number operations, so this path should not be marked as Deferred.
    Label if_overflow(this,
                      rhs_is_smi ? Label::kDeferred : Label::kNonDeferred);
    Branch(overflow, &if_overflow, &if_notoverflow);

    BIND(&if_notoverflow);
    {
      var_type_feedback->Bind(
          SmiConstant(BinaryOperationFeedback::kSignedSmall));
      var_result.Bind(BitcastWordToTaggedSigned(Projection(0, pair)));
      Goto(&end);
    }

    BIND(&if_overflow);
    {
      var_type_feedback->Bind(SmiConstant(BinaryOperationFeedback::kNumber));
      Node* value = Float64Sub(SmiToFloat64(lhs), SmiToFloat64(rhs));
      var_result.Bind(AllocateHeapNumberWithValue(value));
      Goto(&end);
    }

    BIND(&end);
    return var_result.value();
  };
  auto floatFunction = [=](Node* lhs, Node* rhs) {
    return Float64Sub(lhs, rhs);
  };
  return Generate_BinaryOperationWithFeedback(
      context, lhs, rhs, slot_id, feedback_vector, smiFunction, floatFunction,
      Operation::kSubtract, rhs_is_smi);
}

Node* BinaryOpAssembler::Generate_MultiplyWithFeedback(Node* context, Node* lhs,
                                                       Node* rhs, Node* slot_id,
                                                       Node* feedback_vector,
                                                       bool rhs_is_smi) {
  auto smiFunction = [=](Node* lhs, Node* rhs, Variable* var_type_feedback) {
    Node* result = SmiMul(lhs, rhs);
    var_type_feedback->Bind(SelectSmiConstant(
        TaggedIsSmi(result), BinaryOperationFeedback::kSignedSmall,
        BinaryOperationFeedback::kNumber));
    return result;
  };
  auto floatFunction = [=](Node* lhs, Node* rhs) {
    return Float64Mul(lhs, rhs);
  };
  return Generate_BinaryOperationWithFeedback(
      context, lhs, rhs, slot_id, feedback_vector, smiFunction, floatFunction,
      Operation::kMultiply, rhs_is_smi);
}

Node* BinaryOpAssembler::Generate_DivideWithFeedback(
    Node* context, Node* dividend, Node* divisor, Node* slot_id,
    Node* feedback_vector, bool rhs_is_smi) {
  auto smiFunction = [=](Node* lhs, Node* rhs, Variable* var_type_feedback) {
    VARIABLE(var_result, MachineRepresentation::kTagged);
    // If rhs is known to be an Smi (for DivSmi) we want to fast path Smi
    // operation. For the normal Div operation, we want to fast path both
    // Smi and Number operations, so this path should not be marked as Deferred.
    Label bailout(this, rhs_is_smi ? Label::kDeferred : Label::kNonDeferred),
        end(this);
    var_result.Bind(TrySmiDiv(lhs, rhs, &bailout));
    var_type_feedback->Bind(SmiConstant(BinaryOperationFeedback::kSignedSmall));
    Goto(&end);

    BIND(&bailout);
    {
      var_type_feedback->Bind(
          SmiConstant(BinaryOperationFeedback::kSignedSmallInputs));
      Node* value = Float64Div(SmiToFloat64(lhs), SmiToFloat64(rhs));
      var_result.Bind(AllocateHeapNumberWithValue(value));
      Goto(&end);
    }

    BIND(&end);
    return var_result.value();
  };
  auto floatFunction = [=](Node* lhs, Node* rhs) {
    return Float64Div(lhs, rhs);
  };
  return Generate_BinaryOperationWithFeedback(
      context, dividend, divisor, slot_id, feedback_vector, smiFunction,
      floatFunction, Operation::kDivide, rhs_is_smi);
}

Node* BinaryOpAssembler::Generate_ModulusWithFeedback(
    Node* context, Node* dividend, Node* divisor, Node* slot_id,
    Node* feedback_vector, bool rhs_is_smi) {
  auto smiFunction = [=](Node* lhs, Node* rhs, Variable* var_type_feedback) {
    Node* result = SmiMod(lhs, rhs);
    var_type_feedback->Bind(SelectSmiConstant(
        TaggedIsSmi(result), BinaryOperationFeedback::kSignedSmall,
        BinaryOperationFeedback::kNumber));
    return result;
  };
  auto floatFunction = [=](Node* lhs, Node* rhs) {
    return Float64Mod(lhs, rhs);
  };
  return Generate_BinaryOperationWithFeedback(
      context, dividend, divisor, slot_id, feedback_vector, smiFunction,
      floatFunction, Operation::kModulus, rhs_is_smi);
}

Node* BinaryOpAssembler::Generate_ExponentiateWithFeedback(
    Node* context, Node* base, Node* exponent, Node* slot_id,
    Node* feedback_vector, bool rhs_is_smi) {
  // We currently don't optimize exponentiation based on feedback.
  Node* dummy_feedback = SmiConstant(BinaryOperationFeedback::kAny);
  UpdateFeedback(dummy_feedback, feedback_vector, slot_id);
  return CallBuiltin(Builtins::kExponentiate, context, base, exponent);
}

}  // namespace internal
}  // namespace v8
