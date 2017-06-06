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
                                                  Node* feedback_vector) {
  // Shared entry for floating point addition.
  Label do_fadd(this), if_lhsisnotnumber(this, Label::kDeferred),
      check_rhsisoddball(this, Label::kDeferred),
      call_with_oddball_feedback(this), call_with_any_feedback(this),
      call_add_stub(this), end(this);
  VARIABLE(var_fadd_lhs, MachineRepresentation::kFloat64);
  VARIABLE(var_fadd_rhs, MachineRepresentation::kFloat64);
  VARIABLE(var_type_feedback, MachineRepresentation::kTaggedSigned);
  VARIABLE(var_result, MachineRepresentation::kTagged);

  // Check if the {lhs} is a Smi or a HeapObject.
  Label if_lhsissmi(this), if_lhsisnotsmi(this);
  Branch(TaggedIsSmi(lhs), &if_lhsissmi, &if_lhsisnotsmi);

  BIND(&if_lhsissmi);
  {
    // Check if the {rhs} is also a Smi.
    Label if_rhsissmi(this), if_rhsisnotsmi(this);
    Branch(TaggedIsSmi(rhs), &if_rhsissmi, &if_rhsisnotsmi);

    BIND(&if_rhsissmi);
    {
      // Try fast Smi addition first.
      Node* pair = IntPtrAddWithOverflow(BitcastTaggedToWord(lhs),
                                         BitcastTaggedToWord(rhs));
      Node* overflow = Projection(1, pair);

      // Check if the Smi additon overflowed.
      Label if_overflow(this), if_notoverflow(this);
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

    BIND(&if_rhsisnotsmi);
    {
      // Load the map of {rhs}.
      Node* rhs_map = LoadMap(rhs);

      // Check if the {rhs} is a HeapNumber.
      GotoIfNot(IsHeapNumberMap(rhs_map), &check_rhsisoddball);

      var_fadd_lhs.Bind(SmiToFloat64(lhs));
      var_fadd_rhs.Bind(LoadHeapNumberValue(rhs));
      Goto(&do_fadd);
    }
  }

  BIND(&if_lhsisnotsmi);
  {
    // Load the map of {lhs}.
    Node* lhs_map = LoadMap(lhs);

    // Check if {lhs} is a HeapNumber.
    GotoIfNot(IsHeapNumberMap(lhs_map), &if_lhsisnotnumber);

    // Check if the {rhs} is Smi.
    Label if_rhsissmi(this), if_rhsisnotsmi(this);
    Branch(TaggedIsSmi(rhs), &if_rhsissmi, &if_rhsisnotsmi);

    BIND(&if_rhsissmi);
    {
      var_fadd_lhs.Bind(LoadHeapNumberValue(lhs));
      var_fadd_rhs.Bind(SmiToFloat64(rhs));
      Goto(&do_fadd);
    }

    BIND(&if_rhsisnotsmi);
    {
      // Load the map of {rhs}.
      Node* rhs_map = LoadMap(rhs);

      // Check if the {rhs} is a HeapNumber.
      GotoIfNot(IsHeapNumberMap(rhs_map), &check_rhsisoddball);

      var_fadd_lhs.Bind(LoadHeapNumberValue(lhs));
      var_fadd_rhs.Bind(LoadHeapNumberValue(rhs));
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
    Node* lhs_is_oddball =
        Word32Equal(lhs_instance_type, Int32Constant(ODDBALL_TYPE));
    Branch(lhs_is_oddball, &if_lhsisoddball, &if_lhsisnotoddball);

    BIND(&if_lhsisoddball);
    {
      GotoIf(TaggedIsSmi(rhs), &call_with_oddball_feedback);

      // Load the map of the {rhs}.
      Node* rhs_map = LoadMap(rhs);

      // Check if {rhs} is a HeapNumber.
      Branch(IsHeapNumberMap(rhs_map), &call_with_oddball_feedback,
             &check_rhsisoddball);
    }

    BIND(&if_lhsisnotoddball);
    {
      // Exit unless {lhs} is a string
      GotoIfNot(IsStringInstanceType(lhs_instance_type),
                &call_with_any_feedback);

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
    Node* rhs_is_oddball =
        Word32Equal(rhs_instance_type, Int32Constant(ODDBALL_TYPE));
    Branch(rhs_is_oddball, &call_with_oddball_feedback,
           &call_with_any_feedback);
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
    Callable callable = CodeFactory::Add(isolate());
    var_result.Bind(CallStub(callable, context, lhs, rhs));
    Goto(&end);
  }

  BIND(&end);
  UpdateFeedback(var_type_feedback.value(), feedback_vector, slot_id);
  return var_result.value();
}

Node* BinaryOpAssembler::Generate_SubtractWithFeedback(Node* context, Node* lhs,
                                                       Node* rhs, Node* slot_id,
                                                       Node* feedback_vector) {
  // Shared entry for floating point subtraction.
  Label do_fsub(this), end(this), call_subtract_stub(this),
      if_lhsisnotnumber(this), check_rhsisoddball(this),
      call_with_any_feedback(this);
  VARIABLE(var_fsub_lhs, MachineRepresentation::kFloat64);
  VARIABLE(var_fsub_rhs, MachineRepresentation::kFloat64);
  VARIABLE(var_type_feedback, MachineRepresentation::kTaggedSigned);
  VARIABLE(var_result, MachineRepresentation::kTagged);

  // Check if the {lhs} is a Smi or a HeapObject.
  Label if_lhsissmi(this), if_lhsisnotsmi(this);
  Branch(TaggedIsSmi(lhs), &if_lhsissmi, &if_lhsisnotsmi);

  BIND(&if_lhsissmi);
  {
    // Check if the {rhs} is also a Smi.
    Label if_rhsissmi(this), if_rhsisnotsmi(this);
    Branch(TaggedIsSmi(rhs), &if_rhsissmi, &if_rhsisnotsmi);

    BIND(&if_rhsissmi);
    {
      // Try a fast Smi subtraction first.
      Node* pair = IntPtrSubWithOverflow(BitcastTaggedToWord(lhs),
                                         BitcastTaggedToWord(rhs));
      Node* overflow = Projection(1, pair);

      // Check if the Smi subtraction overflowed.
      Label if_overflow(this), if_notoverflow(this);
      Branch(overflow, &if_overflow, &if_notoverflow);

      BIND(&if_overflow);
      {
        // lhs, rhs - smi and result - number. combined - number.
        // The result doesn't fit into Smi range.
        var_fsub_lhs.Bind(SmiToFloat64(lhs));
        var_fsub_rhs.Bind(SmiToFloat64(rhs));
        Goto(&do_fsub);
      }

      BIND(&if_notoverflow);
      // lhs, rhs, result smi. combined - smi.
      var_type_feedback.Bind(
          SmiConstant(BinaryOperationFeedback::kSignedSmall));
      var_result.Bind(BitcastWordToTaggedSigned(Projection(0, pair)));
      Goto(&end);
    }

    BIND(&if_rhsisnotsmi);
    {
      // Load the map of the {rhs}.
      Node* rhs_map = LoadMap(rhs);

      // Check if {rhs} is a HeapNumber.
      GotoIfNot(IsHeapNumberMap(rhs_map), &check_rhsisoddball);

      // Perform a floating point subtraction.
      var_fsub_lhs.Bind(SmiToFloat64(lhs));
      var_fsub_rhs.Bind(LoadHeapNumberValue(rhs));
      Goto(&do_fsub);
    }
  }

  BIND(&if_lhsisnotsmi);
  {
    // Load the map of the {lhs}.
    Node* lhs_map = LoadMap(lhs);

    // Check if the {lhs} is a HeapNumber.
    GotoIfNot(IsHeapNumberMap(lhs_map), &if_lhsisnotnumber);

    // Check if the {rhs} is a Smi.
    Label if_rhsissmi(this), if_rhsisnotsmi(this);
    Branch(TaggedIsSmi(rhs), &if_rhsissmi, &if_rhsisnotsmi);

    BIND(&if_rhsissmi);
    {
      // Perform a floating point subtraction.
      var_fsub_lhs.Bind(LoadHeapNumberValue(lhs));
      var_fsub_rhs.Bind(SmiToFloat64(rhs));
      Goto(&do_fsub);
    }

    BIND(&if_rhsisnotsmi);
    {
      // Load the map of the {rhs}.
      Node* rhs_map = LoadMap(rhs);

      // Check if the {rhs} is a HeapNumber.
      GotoIfNot(IsHeapNumberMap(rhs_map), &check_rhsisoddball);

      // Perform a floating point subtraction.
      var_fsub_lhs.Bind(LoadHeapNumberValue(lhs));
      var_fsub_rhs.Bind(LoadHeapNumberValue(rhs));
      Goto(&do_fsub);
    }
  }

  BIND(&do_fsub);
  {
    var_type_feedback.Bind(SmiConstant(BinaryOperationFeedback::kNumber));
    Node* lhs_value = var_fsub_lhs.value();
    Node* rhs_value = var_fsub_rhs.value();
    Node* value = Float64Sub(lhs_value, rhs_value);
    var_result.Bind(AllocateHeapNumberWithValue(value));
    Goto(&end);
  }

  BIND(&if_lhsisnotnumber);
  {
    // No checks on rhs are done yet. We just know lhs is not a number or Smi.
    // Check if lhs is an oddball.
    Node* lhs_instance_type = LoadInstanceType(lhs);
    Node* lhs_is_oddball =
        Word32Equal(lhs_instance_type, Int32Constant(ODDBALL_TYPE));
    GotoIfNot(lhs_is_oddball, &call_with_any_feedback);

    Label if_rhsissmi(this), if_rhsisnotsmi(this);
    Branch(TaggedIsSmi(rhs), &if_rhsissmi, &if_rhsisnotsmi);

    BIND(&if_rhsissmi);
    {
      var_type_feedback.Bind(
          SmiConstant(BinaryOperationFeedback::kNumberOrOddball));
      Goto(&call_subtract_stub);
    }

    BIND(&if_rhsisnotsmi);
    {
      // Load the map of the {rhs}.
      Node* rhs_map = LoadMap(rhs);

      // Check if {rhs} is a HeapNumber.
      GotoIfNot(IsHeapNumberMap(rhs_map), &check_rhsisoddball);

      var_type_feedback.Bind(
          SmiConstant(BinaryOperationFeedback::kNumberOrOddball));
      Goto(&call_subtract_stub);
    }
  }

  BIND(&check_rhsisoddball);
  {
    // Check if rhs is an oddball. At this point we know lhs is either a
    // Smi or number or oddball and rhs is not a number or Smi.
    Node* rhs_instance_type = LoadInstanceType(rhs);
    Node* rhs_is_oddball =
        Word32Equal(rhs_instance_type, Int32Constant(ODDBALL_TYPE));
    GotoIfNot(rhs_is_oddball, &call_with_any_feedback);

    var_type_feedback.Bind(
        SmiConstant(BinaryOperationFeedback::kNumberOrOddball));
    Goto(&call_subtract_stub);
  }

  BIND(&call_with_any_feedback);
  {
    var_type_feedback.Bind(SmiConstant(BinaryOperationFeedback::kAny));
    Goto(&call_subtract_stub);
  }

  BIND(&call_subtract_stub);
  {
    Callable callable = CodeFactory::Subtract(isolate());
    var_result.Bind(CallStub(callable, context, lhs, rhs));
    Goto(&end);
  }

  BIND(&end);
  UpdateFeedback(var_type_feedback.value(), feedback_vector, slot_id);
  return var_result.value();
}

Node* BinaryOpAssembler::Generate_MultiplyWithFeedback(Node* context, Node* lhs,
                                                       Node* rhs, Node* slot_id,
                                                       Node* feedback_vector) {
  // Shared entry point for floating point multiplication.
  Label do_fmul(this), if_lhsisnotnumber(this, Label::kDeferred),
      check_rhsisoddball(this, Label::kDeferred),
      call_with_oddball_feedback(this), call_with_any_feedback(this),
      call_multiply_stub(this), end(this);
  VARIABLE(var_lhs_float64, MachineRepresentation::kFloat64);
  VARIABLE(var_rhs_float64, MachineRepresentation::kFloat64);
  VARIABLE(var_result, MachineRepresentation::kTagged);
  VARIABLE(var_type_feedback, MachineRepresentation::kTaggedSigned);

  Label lhs_is_smi(this), lhs_is_not_smi(this);
  Branch(TaggedIsSmi(lhs), &lhs_is_smi, &lhs_is_not_smi);

  BIND(&lhs_is_smi);
  {
    Label rhs_is_smi(this), rhs_is_not_smi(this);
    Branch(TaggedIsSmi(rhs), &rhs_is_smi, &rhs_is_not_smi);

    BIND(&rhs_is_smi);
    {
      // Both {lhs} and {rhs} are Smis. The result is not necessarily a smi,
      // in case of overflow.
      var_result.Bind(SmiMul(lhs, rhs));
      var_type_feedback.Bind(
          SelectSmiConstant(TaggedIsSmi(var_result.value()),
                            BinaryOperationFeedback::kSignedSmall,
                            BinaryOperationFeedback::kNumber));
      Goto(&end);
    }

    BIND(&rhs_is_not_smi);
    {
      Node* rhs_map = LoadMap(rhs);

      // Check if {rhs} is a HeapNumber.
      GotoIfNot(IsHeapNumberMap(rhs_map), &check_rhsisoddball);

      // Convert {lhs} to a double and multiply it with the value of {rhs}.
      var_lhs_float64.Bind(SmiToFloat64(lhs));
      var_rhs_float64.Bind(LoadHeapNumberValue(rhs));
      Goto(&do_fmul);
    }
  }

  BIND(&lhs_is_not_smi);
  {
    Node* lhs_map = LoadMap(lhs);

    // Check if {lhs} is a HeapNumber.
    GotoIfNot(IsHeapNumberMap(lhs_map), &if_lhsisnotnumber);

    // Check if {rhs} is a Smi.
    Label rhs_is_smi(this), rhs_is_not_smi(this);
    Branch(TaggedIsSmi(rhs), &rhs_is_smi, &rhs_is_not_smi);

    BIND(&rhs_is_smi);
    {
      // Convert {rhs} to a double and multiply it with the value of {lhs}.
      var_lhs_float64.Bind(LoadHeapNumberValue(lhs));
      var_rhs_float64.Bind(SmiToFloat64(rhs));
      Goto(&do_fmul);
    }

    BIND(&rhs_is_not_smi);
    {
      Node* rhs_map = LoadMap(rhs);

      // Check if {rhs} is a HeapNumber.
      GotoIfNot(IsHeapNumberMap(rhs_map), &check_rhsisoddball);

      // Both {lhs} and {rhs} are HeapNumbers. Load their values and
      // multiply them.
      var_lhs_float64.Bind(LoadHeapNumberValue(lhs));
      var_rhs_float64.Bind(LoadHeapNumberValue(rhs));
      Goto(&do_fmul);
    }
  }

  BIND(&do_fmul);
  {
    var_type_feedback.Bind(SmiConstant(BinaryOperationFeedback::kNumber));
    Node* value = Float64Mul(var_lhs_float64.value(), var_rhs_float64.value());
    Node* result = AllocateHeapNumberWithValue(value);
    var_result.Bind(result);
    Goto(&end);
  }

  BIND(&if_lhsisnotnumber);
  {
    // No checks on rhs are done yet. We just know lhs is not a number or Smi.
    // Check if lhs is an oddball.
    Node* lhs_instance_type = LoadInstanceType(lhs);
    Node* lhs_is_oddball =
        Word32Equal(lhs_instance_type, Int32Constant(ODDBALL_TYPE));
    GotoIfNot(lhs_is_oddball, &call_with_any_feedback);

    GotoIf(TaggedIsSmi(rhs), &call_with_oddball_feedback);

    // Load the map of the {rhs}.
    Node* rhs_map = LoadMap(rhs);

    // Check if {rhs} is a HeapNumber.
    Branch(IsHeapNumberMap(rhs_map), &call_with_oddball_feedback,
           &check_rhsisoddball);
  }

  BIND(&check_rhsisoddball);
  {
    // Check if rhs is an oddball. At this point we know lhs is either a
    // Smi or number or oddball and rhs is not a number or Smi.
    Node* rhs_instance_type = LoadInstanceType(rhs);
    Node* rhs_is_oddball =
        Word32Equal(rhs_instance_type, Int32Constant(ODDBALL_TYPE));
    Branch(rhs_is_oddball, &call_with_oddball_feedback,
           &call_with_any_feedback);
  }

  BIND(&call_with_oddball_feedback);
  {
    var_type_feedback.Bind(
        SmiConstant(BinaryOperationFeedback::kNumberOrOddball));
    Goto(&call_multiply_stub);
  }

  BIND(&call_with_any_feedback);
  {
    var_type_feedback.Bind(SmiConstant(BinaryOperationFeedback::kAny));
    Goto(&call_multiply_stub);
  }

  BIND(&call_multiply_stub);
  {
    Callable callable = CodeFactory::Multiply(isolate());
    var_result.Bind(CallStub(callable, context, lhs, rhs));
    Goto(&end);
  }

  BIND(&end);
  UpdateFeedback(var_type_feedback.value(), feedback_vector, slot_id);
  return var_result.value();
}

Node* BinaryOpAssembler::Generate_DivideWithFeedback(Node* context,
                                                     Node* dividend,
                                                     Node* divisor,
                                                     Node* slot_id,
                                                     Node* feedback_vector) {
  // Shared entry point for floating point division.
  Label do_fdiv(this), dividend_is_not_number(this, Label::kDeferred),
      check_divisor_for_oddball(this, Label::kDeferred),
      call_with_oddball_feedback(this), call_with_any_feedback(this),
      call_divide_stub(this), end(this);
  VARIABLE(var_dividend_float64, MachineRepresentation::kFloat64);
  VARIABLE(var_divisor_float64, MachineRepresentation::kFloat64);
  VARIABLE(var_result, MachineRepresentation::kTagged);
  VARIABLE(var_type_feedback, MachineRepresentation::kTaggedSigned);

  Label dividend_is_smi(this), dividend_is_not_smi(this);
  Branch(TaggedIsSmi(dividend), &dividend_is_smi, &dividend_is_not_smi);

  BIND(&dividend_is_smi);
  {
    Label divisor_is_smi(this), divisor_is_not_smi(this);
    Branch(TaggedIsSmi(divisor), &divisor_is_smi, &divisor_is_not_smi);

    BIND(&divisor_is_smi);
    {
      Label bailout(this);

      // Try to perform Smi division if possible.
      var_result.Bind(TrySmiDiv(dividend, divisor, &bailout));
      var_type_feedback.Bind(
          SmiConstant(BinaryOperationFeedback::kSignedSmall));
      Goto(&end);

      // Bailout: convert {dividend} and {divisor} to double and do double
      // division.
      BIND(&bailout);
      {
        var_dividend_float64.Bind(SmiToFloat64(dividend));
        var_divisor_float64.Bind(SmiToFloat64(divisor));
        Goto(&do_fdiv);
      }
    }

    BIND(&divisor_is_not_smi);
    {
      Node* divisor_map = LoadMap(divisor);

      // Check if {divisor} is a HeapNumber.
      GotoIfNot(IsHeapNumberMap(divisor_map), &check_divisor_for_oddball);

      // Convert {dividend} to a double and divide it with the value of
      // {divisor}.
      var_dividend_float64.Bind(SmiToFloat64(dividend));
      var_divisor_float64.Bind(LoadHeapNumberValue(divisor));
      Goto(&do_fdiv);
    }

    BIND(&dividend_is_not_smi);
    {
      Node* dividend_map = LoadMap(dividend);

      // Check if {dividend} is a HeapNumber.
      GotoIfNot(IsHeapNumberMap(dividend_map), &dividend_is_not_number);

      // Check if {divisor} is a Smi.
      Label divisor_is_smi(this), divisor_is_not_smi(this);
      Branch(TaggedIsSmi(divisor), &divisor_is_smi, &divisor_is_not_smi);

      BIND(&divisor_is_smi);
      {
        // Convert {divisor} to a double and use it for a floating point
        // division.
        var_dividend_float64.Bind(LoadHeapNumberValue(dividend));
        var_divisor_float64.Bind(SmiToFloat64(divisor));
        Goto(&do_fdiv);
      }

      BIND(&divisor_is_not_smi);
      {
        Node* divisor_map = LoadMap(divisor);

        // Check if {divisor} is a HeapNumber.
        GotoIfNot(IsHeapNumberMap(divisor_map), &check_divisor_for_oddball);

        // Both {dividend} and {divisor} are HeapNumbers. Load their values
        // and divide them.
        var_dividend_float64.Bind(LoadHeapNumberValue(dividend));
        var_divisor_float64.Bind(LoadHeapNumberValue(divisor));
        Goto(&do_fdiv);
      }
    }
  }

  BIND(&do_fdiv);
  {
    var_type_feedback.Bind(SmiConstant(BinaryOperationFeedback::kNumber));
    Node* value =
        Float64Div(var_dividend_float64.value(), var_divisor_float64.value());
    var_result.Bind(AllocateHeapNumberWithValue(value));
    Goto(&end);
  }

  BIND(&dividend_is_not_number);
  {
    // We just know dividend is not a number or Smi. No checks on divisor yet.
    // Check if dividend is an oddball.
    Node* dividend_instance_type = LoadInstanceType(dividend);
    Node* dividend_is_oddball =
        Word32Equal(dividend_instance_type, Int32Constant(ODDBALL_TYPE));
    GotoIfNot(dividend_is_oddball, &call_with_any_feedback);

    GotoIf(TaggedIsSmi(divisor), &call_with_oddball_feedback);

    // Load the map of the {divisor}.
    Node* divisor_map = LoadMap(divisor);

    // Check if {divisor} is a HeapNumber.
    Branch(IsHeapNumberMap(divisor_map), &call_with_oddball_feedback,
           &check_divisor_for_oddball);
  }

  BIND(&check_divisor_for_oddball);
  {
    // Check if divisor is an oddball. At this point we know dividend is either
    // a Smi or number or oddball and divisor is not a number or Smi.
    Node* divisor_instance_type = LoadInstanceType(divisor);
    Node* divisor_is_oddball =
        Word32Equal(divisor_instance_type, Int32Constant(ODDBALL_TYPE));
    Branch(divisor_is_oddball, &call_with_oddball_feedback,
           &call_with_any_feedback);
  }

  BIND(&call_with_oddball_feedback);
  {
    var_type_feedback.Bind(
        SmiConstant(BinaryOperationFeedback::kNumberOrOddball));
    Goto(&call_divide_stub);
  }

  BIND(&call_with_any_feedback);
  {
    var_type_feedback.Bind(SmiConstant(BinaryOperationFeedback::kAny));
    Goto(&call_divide_stub);
  }

  BIND(&call_divide_stub);
  {
    Callable callable = CodeFactory::Divide(isolate());
    var_result.Bind(CallStub(callable, context, dividend, divisor));
    Goto(&end);
  }

  BIND(&end);
  UpdateFeedback(var_type_feedback.value(), feedback_vector, slot_id);
  return var_result.value();
}

Node* BinaryOpAssembler::Generate_ModulusWithFeedback(Node* context,
                                                      Node* dividend,
                                                      Node* divisor,
                                                      Node* slot_id,
                                                      Node* feedback_vector) {
  // Shared entry point for floating point division.
  Label do_fmod(this), dividend_is_not_number(this, Label::kDeferred),
      check_divisor_for_oddball(this, Label::kDeferred),
      call_with_oddball_feedback(this), call_with_any_feedback(this),
      call_modulus_stub(this), end(this);
  VARIABLE(var_dividend_float64, MachineRepresentation::kFloat64);
  VARIABLE(var_divisor_float64, MachineRepresentation::kFloat64);
  VARIABLE(var_result, MachineRepresentation::kTagged);
  VARIABLE(var_type_feedback, MachineRepresentation::kTaggedSigned);

  Label dividend_is_smi(this), dividend_is_not_smi(this);
  Branch(TaggedIsSmi(dividend), &dividend_is_smi, &dividend_is_not_smi);

  BIND(&dividend_is_smi);
  {
    Label divisor_is_smi(this), divisor_is_not_smi(this);
    Branch(TaggedIsSmi(divisor), &divisor_is_smi, &divisor_is_not_smi);

    BIND(&divisor_is_smi);
    {
      var_result.Bind(SmiMod(dividend, divisor));
      var_type_feedback.Bind(
          SelectSmiConstant(TaggedIsSmi(var_result.value()),
                            BinaryOperationFeedback::kSignedSmall,
                            BinaryOperationFeedback::kNumber));
      Goto(&end);
    }

    BIND(&divisor_is_not_smi);
    {
      Node* divisor_map = LoadMap(divisor);

      // Check if {divisor} is a HeapNumber.
      GotoIfNot(IsHeapNumberMap(divisor_map), &check_divisor_for_oddball);

      // Convert {dividend} to a double and divide it with the value of
      // {divisor}.
      var_dividend_float64.Bind(SmiToFloat64(dividend));
      var_divisor_float64.Bind(LoadHeapNumberValue(divisor));
      Goto(&do_fmod);
    }
  }

  BIND(&dividend_is_not_smi);
  {
    Node* dividend_map = LoadMap(dividend);

    // Check if {dividend} is a HeapNumber.
    GotoIfNot(IsHeapNumberMap(dividend_map), &dividend_is_not_number);

    // Check if {divisor} is a Smi.
    Label divisor_is_smi(this), divisor_is_not_smi(this);
    Branch(TaggedIsSmi(divisor), &divisor_is_smi, &divisor_is_not_smi);

    BIND(&divisor_is_smi);
    {
      // Convert {divisor} to a double and use it for a floating point
      // division.
      var_dividend_float64.Bind(LoadHeapNumberValue(dividend));
      var_divisor_float64.Bind(SmiToFloat64(divisor));
      Goto(&do_fmod);
    }

    BIND(&divisor_is_not_smi);
    {
      Node* divisor_map = LoadMap(divisor);

      // Check if {divisor} is a HeapNumber.
      GotoIfNot(IsHeapNumberMap(divisor_map), &check_divisor_for_oddball);

      // Both {dividend} and {divisor} are HeapNumbers. Load their values
      // and divide them.
      var_dividend_float64.Bind(LoadHeapNumberValue(dividend));
      var_divisor_float64.Bind(LoadHeapNumberValue(divisor));
      Goto(&do_fmod);
    }
  }

  BIND(&do_fmod);
  {
    var_type_feedback.Bind(SmiConstant(BinaryOperationFeedback::kNumber));
    Node* value =
        Float64Mod(var_dividend_float64.value(), var_divisor_float64.value());
    var_result.Bind(AllocateHeapNumberWithValue(value));
    Goto(&end);
  }

  BIND(&dividend_is_not_number);
  {
    // No checks on divisor yet. We just know dividend is not a number or Smi.
    // Check if dividend is an oddball.
    Node* dividend_instance_type = LoadInstanceType(dividend);
    Node* dividend_is_oddball =
        Word32Equal(dividend_instance_type, Int32Constant(ODDBALL_TYPE));
    GotoIfNot(dividend_is_oddball, &call_with_any_feedback);

    GotoIf(TaggedIsSmi(divisor), &call_with_oddball_feedback);

    // Load the map of the {divisor}.
    Node* divisor_map = LoadMap(divisor);

    // Check if {divisor} is a HeapNumber.
    Branch(IsHeapNumberMap(divisor_map), &call_with_oddball_feedback,
           &check_divisor_for_oddball);
  }

  BIND(&check_divisor_for_oddball);
  {
    // Check if divisor is an oddball. At this point we know dividend is either
    // a Smi or number or oddball and divisor is not a number or Smi.
    Node* divisor_instance_type = LoadInstanceType(divisor);
    Node* divisor_is_oddball =
        Word32Equal(divisor_instance_type, Int32Constant(ODDBALL_TYPE));
    Branch(divisor_is_oddball, &call_with_oddball_feedback,
           &call_with_any_feedback);
  }

  BIND(&call_with_oddball_feedback);
  {
    var_type_feedback.Bind(
        SmiConstant(BinaryOperationFeedback::kNumberOrOddball));
    Goto(&call_modulus_stub);
  }

  BIND(&call_with_any_feedback);
  {
    var_type_feedback.Bind(SmiConstant(BinaryOperationFeedback::kAny));
    Goto(&call_modulus_stub);
  }

  BIND(&call_modulus_stub);
  {
    Callable callable = CodeFactory::Modulus(isolate());
    var_result.Bind(CallStub(callable, context, dividend, divisor));
    Goto(&end);
  }

  BIND(&end);
  UpdateFeedback(var_type_feedback.value(), feedback_vector, slot_id);
  return var_result.value();
}

}  // namespace internal
}  // namespace v8
