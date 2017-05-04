// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/code-stub-assembler.h"
#include "src/ic/binary-op-assembler.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// ES6 section 20.1 Number Objects

class NumberBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit NumberBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

 protected:
  template <typename Descriptor>
  void BitwiseOp(std::function<Node*(Node* lhs, Node* rhs)> body,
                 Signedness signed_result = kSigned) {
    Node* left = Parameter(Descriptor::kLeft);
    Node* right = Parameter(Descriptor::kRight);
    Node* context = Parameter(Descriptor::kContext);

    Node* lhs_value = TruncateTaggedToWord32(context, left);
    Node* rhs_value = TruncateTaggedToWord32(context, right);
    Node* value = body(lhs_value, rhs_value);
    Node* result = signed_result == kSigned ? ChangeInt32ToTagged(value)
                                            : ChangeUint32ToTagged(value);
    Return(result);
  }

  template <typename Descriptor>
  void BitwiseShiftOp(std::function<Node*(Node* lhs, Node* shift_count)> body,
                      Signedness signed_result = kSigned) {
    BitwiseOp<Descriptor>(
        [=](Node* lhs, Node* rhs) {
          Node* shift_count = Word32And(rhs, Int32Constant(0x1f));
          return body(lhs, shift_count);
        },
        signed_result);
  }

  template <typename Descriptor>
  void RelationalComparisonBuiltin(RelationalComparisonMode mode) {
    Node* lhs = Parameter(Descriptor::kLeft);
    Node* rhs = Parameter(Descriptor::kRight);
    Node* context = Parameter(Descriptor::kContext);

    Return(RelationalComparison(mode, lhs, rhs, context));
  }
};

// ES6 #sec-number.isfinite
TF_BUILTIN(NumberIsFinite, CodeStubAssembler) {
  Node* number = Parameter(Descriptor::kNumber);

  Label return_true(this), return_false(this);

  // Check if {number} is a Smi.
  GotoIf(TaggedIsSmi(number), &return_true);

  // Check if {number} is a HeapNumber.
  GotoIfNot(IsHeapNumberMap(LoadMap(number)), &return_false);

  // Check if {number} contains a finite, non-NaN value.
  Node* number_value = LoadHeapNumberValue(number);
  BranchIfFloat64IsNaN(Float64Sub(number_value, number_value), &return_false,
                       &return_true);

  BIND(&return_true);
  Return(BooleanConstant(true));

  BIND(&return_false);
  Return(BooleanConstant(false));
}

// ES6 #sec-number.isinteger
TF_BUILTIN(NumberIsInteger, CodeStubAssembler) {
  Node* number = Parameter(Descriptor::kNumber);

  Label return_true(this), return_false(this);

  // Check if {number} is a Smi.
  GotoIf(TaggedIsSmi(number), &return_true);

  // Check if {number} is a HeapNumber.
  GotoIfNot(IsHeapNumberMap(LoadMap(number)), &return_false);

  // Load the actual value of {number}.
  Node* number_value = LoadHeapNumberValue(number);

  // Truncate the value of {number} to an integer (or an infinity).
  Node* integer = Float64Trunc(number_value);

  // Check if {number}s value matches the integer (ruling out the infinities).
  Branch(Float64Equal(Float64Sub(number_value, integer), Float64Constant(0.0)),
         &return_true, &return_false);

  BIND(&return_true);
  Return(BooleanConstant(true));

  BIND(&return_false);
  Return(BooleanConstant(false));
}

// ES6 #sec-number.isnan
TF_BUILTIN(NumberIsNaN, CodeStubAssembler) {
  Node* number = Parameter(Descriptor::kNumber);

  Label return_true(this), return_false(this);

  // Check if {number} is a Smi.
  GotoIf(TaggedIsSmi(number), &return_false);

  // Check if {number} is a HeapNumber.
  GotoIfNot(IsHeapNumberMap(LoadMap(number)), &return_false);

  // Check if {number} contains a NaN value.
  Node* number_value = LoadHeapNumberValue(number);
  BranchIfFloat64IsNaN(number_value, &return_true, &return_false);

  BIND(&return_true);
  Return(BooleanConstant(true));

  BIND(&return_false);
  Return(BooleanConstant(false));
}

// ES6 #sec-number.issafeinteger
TF_BUILTIN(NumberIsSafeInteger, CodeStubAssembler) {
  Node* number = Parameter(Descriptor::kNumber);

  Label return_true(this), return_false(this);

  // Check if {number} is a Smi.
  GotoIf(TaggedIsSmi(number), &return_true);

  // Check if {number} is a HeapNumber.
  GotoIfNot(IsHeapNumberMap(LoadMap(number)), &return_false);

  // Load the actual value of {number}.
  Node* number_value = LoadHeapNumberValue(number);

  // Truncate the value of {number} to an integer (or an infinity).
  Node* integer = Float64Trunc(number_value);

  // Check if {number}s value matches the integer (ruling out the infinities).
  GotoIfNot(
      Float64Equal(Float64Sub(number_value, integer), Float64Constant(0.0)),
      &return_false);

  // Check if the {integer} value is in safe integer range.
  Branch(Float64LessThanOrEqual(Float64Abs(integer),
                                Float64Constant(kMaxSafeInteger)),
         &return_true, &return_false);

  BIND(&return_true);
  Return(BooleanConstant(true));

  BIND(&return_false);
  Return(BooleanConstant(false));
}

// ES6 #sec-number.parsefloat
TF_BUILTIN(NumberParseFloat, CodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);

  // We might need to loop once for ToString conversion.
  VARIABLE(var_input, MachineRepresentation::kTagged,
           Parameter(Descriptor::kString));
  Label loop(this, &var_input);
  Goto(&loop);
  BIND(&loop);
  {
    // Load the current {input} value.
    Node* input = var_input.value();

    // Check if the {input} is a HeapObject or a Smi.
    Label if_inputissmi(this), if_inputisnotsmi(this);
    Branch(TaggedIsSmi(input), &if_inputissmi, &if_inputisnotsmi);

    BIND(&if_inputissmi);
    {
      // The {input} is already a Number, no need to do anything.
      Return(input);
    }

    BIND(&if_inputisnotsmi);
    {
      // The {input} is a HeapObject, check if it's already a String.
      Label if_inputisstring(this), if_inputisnotstring(this);
      Node* input_map = LoadMap(input);
      Node* input_instance_type = LoadMapInstanceType(input_map);
      Branch(IsStringInstanceType(input_instance_type), &if_inputisstring,
             &if_inputisnotstring);

      BIND(&if_inputisstring);
      {
        // The {input} is already a String, check if {input} contains
        // a cached array index.
        Label if_inputcached(this), if_inputnotcached(this);
        Node* input_hash = LoadNameHashField(input);
        Node* input_bit = Word32And(
            input_hash, Int32Constant(String::kContainsCachedArrayIndexMask));
        Branch(Word32Equal(input_bit, Int32Constant(0)), &if_inputcached,
               &if_inputnotcached);

        BIND(&if_inputcached);
        {
          // Just return the {input}s cached array index.
          Node* input_array_index =
              DecodeWordFromWord32<String::ArrayIndexValueBits>(input_hash);
          Return(SmiTag(input_array_index));
        }

        BIND(&if_inputnotcached);
        {
          // Need to fall back to the runtime to convert {input} to double.
          Return(CallRuntime(Runtime::kStringParseFloat, context, input));
        }
      }

      BIND(&if_inputisnotstring);
      {
        // The {input} is neither a String nor a Smi, check for HeapNumber.
        Label if_inputisnumber(this),
            if_inputisnotnumber(this, Label::kDeferred);
        Branch(IsHeapNumberMap(input_map), &if_inputisnumber,
               &if_inputisnotnumber);

        BIND(&if_inputisnumber);
        {
          // The {input} is already a Number, take care of -0.
          Label if_inputiszero(this), if_inputisnotzero(this);
          Node* input_value = LoadHeapNumberValue(input);
          Branch(Float64Equal(input_value, Float64Constant(0.0)),
                 &if_inputiszero, &if_inputisnotzero);

          BIND(&if_inputiszero);
          Return(SmiConstant(0));

          BIND(&if_inputisnotzero);
          Return(input);
        }

        BIND(&if_inputisnotnumber);
        {
          // Need to convert the {input} to String first.
          // TODO(bmeurer): This could be more efficient if necessary.
          Callable callable = CodeFactory::ToString(isolate());
          var_input.Bind(CallStub(callable, context, input));
          Goto(&loop);
        }
      }
    }
  }
}

// ES6 #sec-number.parseint
TF_BUILTIN(NumberParseInt, CodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* input = Parameter(Descriptor::kString);
  Node* radix = Parameter(Descriptor::kRadix);

  // Check if {radix} is treated as 10 (i.e. undefined, 0 or 10).
  Label if_radix10(this), if_generic(this, Label::kDeferred);
  GotoIf(WordEqual(radix, UndefinedConstant()), &if_radix10);
  GotoIf(WordEqual(radix, SmiConstant(Smi::FromInt(10))), &if_radix10);
  GotoIf(WordEqual(radix, SmiConstant(Smi::FromInt(0))), &if_radix10);
  Goto(&if_generic);

  BIND(&if_radix10);
  {
    // Check if we can avoid the ToString conversion on {input}.
    Label if_inputissmi(this), if_inputisheapnumber(this),
        if_inputisstring(this);
    GotoIf(TaggedIsSmi(input), &if_inputissmi);
    Node* input_map = LoadMap(input);
    GotoIf(IsHeapNumberMap(input_map), &if_inputisheapnumber);
    Node* input_instance_type = LoadMapInstanceType(input_map);
    Branch(IsStringInstanceType(input_instance_type), &if_inputisstring,
           &if_generic);

    BIND(&if_inputissmi);
    {
      // Just return the {input}.
      Return(input);
    }

    BIND(&if_inputisheapnumber);
    {
      // Check if the {input} value is in Signed32 range.
      Label if_inputissigned32(this);
      Node* input_value = LoadHeapNumberValue(input);
      Node* input_value32 = TruncateFloat64ToWord32(input_value);
      GotoIf(Float64Equal(input_value, ChangeInt32ToFloat64(input_value32)),
             &if_inputissigned32);

      // Check if the absolute {input} value is in the ]0.01,1e9[ range.
      Node* input_value_abs = Float64Abs(input_value);

      GotoIfNot(Float64LessThan(input_value_abs, Float64Constant(1e9)),
                &if_generic);
      Branch(Float64LessThan(Float64Constant(0.01), input_value_abs),
             &if_inputissigned32, &if_generic);

      // Return the truncated int32 value, and return the tagged result.
      BIND(&if_inputissigned32);
      Node* result = ChangeInt32ToTagged(input_value32);
      Return(result);
    }

    BIND(&if_inputisstring);
    {
      // Check if the String {input} has a cached array index.
      Node* input_hash = LoadNameHashField(input);
      Node* input_bit = Word32And(
          input_hash, Int32Constant(String::kContainsCachedArrayIndexMask));
      GotoIf(Word32NotEqual(input_bit, Int32Constant(0)), &if_generic);

      // Return the cached array index as result.
      Node* input_index =
          DecodeWordFromWord32<String::ArrayIndexValueBits>(input_hash);
      Node* result = SmiTag(input_index);
      Return(result);
    }
  }

  BIND(&if_generic);
  {
    Node* result = CallRuntime(Runtime::kStringParseInt, context, input, radix);
    Return(result);
  }
}

// ES6 #sec-number.prototype.valueof
TF_BUILTIN(NumberPrototypeValueOf, CodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);

  Node* result = ToThisValue(context, receiver, PrimitiveType::kNumber,
                             "Number.prototype.valueOf");
  Return(result);
}

TF_BUILTIN(Add, CodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* left = Parameter(Descriptor::kLeft);
  Node* right = Parameter(Descriptor::kRight);

  // Shared entry for floating point addition.
  Label do_fadd(this);
  VARIABLE(var_fadd_lhs, MachineRepresentation::kFloat64);
  VARIABLE(var_fadd_rhs, MachineRepresentation::kFloat64);

  // We might need to loop several times due to ToPrimitive, ToString and/or
  // ToNumber conversions.
  VARIABLE(var_lhs, MachineRepresentation::kTagged);
  VARIABLE(var_rhs, MachineRepresentation::kTagged);
  VARIABLE(var_result, MachineRepresentation::kTagged);
  Variable* loop_vars[2] = {&var_lhs, &var_rhs};
  Label loop(this, 2, loop_vars), end(this),
      string_add_convert_left(this, Label::kDeferred),
      string_add_convert_right(this, Label::kDeferred);
  var_lhs.Bind(left);
  var_rhs.Bind(right);
  Goto(&loop);
  BIND(&loop);
  {
    // Load the current {lhs} and {rhs} values.
    Node* lhs = var_lhs.value();
    Node* rhs = var_rhs.value();

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
        var_result.Bind(BitcastWordToTaggedSigned(Projection(0, pair)));
        Goto(&end);
      }

      BIND(&if_rhsisnotsmi);
      {
        // Load the map of {rhs}.
        Node* rhs_map = LoadMap(rhs);

        // Check if the {rhs} is a HeapNumber.
        Label if_rhsisnumber(this), if_rhsisnotnumber(this, Label::kDeferred);
        Branch(IsHeapNumberMap(rhs_map), &if_rhsisnumber, &if_rhsisnotnumber);

        BIND(&if_rhsisnumber);
        {
          var_fadd_lhs.Bind(SmiToFloat64(lhs));
          var_fadd_rhs.Bind(LoadHeapNumberValue(rhs));
          Goto(&do_fadd);
        }

        BIND(&if_rhsisnotnumber);
        {
          // Load the instance type of {rhs}.
          Node* rhs_instance_type = LoadMapInstanceType(rhs_map);

          // Check if the {rhs} is a String.
          Label if_rhsisstring(this, Label::kDeferred),
              if_rhsisnotstring(this, Label::kDeferred);
          Branch(IsStringInstanceType(rhs_instance_type), &if_rhsisstring,
                 &if_rhsisnotstring);

          BIND(&if_rhsisstring);
          {
            var_lhs.Bind(lhs);
            var_rhs.Bind(rhs);
            Goto(&string_add_convert_left);
          }

          BIND(&if_rhsisnotstring);
          {
            // Check if {rhs} is a JSReceiver.
            Label if_rhsisreceiver(this, Label::kDeferred),
                if_rhsisnotreceiver(this, Label::kDeferred);
            Branch(IsJSReceiverInstanceType(rhs_instance_type),
                   &if_rhsisreceiver, &if_rhsisnotreceiver);

            BIND(&if_rhsisreceiver);
            {
              // Convert {rhs} to a primitive first passing no hint.
              Callable callable =
                  CodeFactory::NonPrimitiveToPrimitive(isolate());
              var_rhs.Bind(CallStub(callable, context, rhs));
              Goto(&loop);
            }

            BIND(&if_rhsisnotreceiver);
            {
              // Convert {rhs} to a Number first.
              Callable callable = CodeFactory::NonNumberToNumber(isolate());
              var_rhs.Bind(CallStub(callable, context, rhs));
              Goto(&loop);
            }
          }
        }
      }
    }

    BIND(&if_lhsisnotsmi);
    {
      // Load the map and instance type of {lhs}.
      Node* lhs_instance_type = LoadInstanceType(lhs);

      // Check if {lhs} is a String.
      Label if_lhsisstring(this), if_lhsisnotstring(this);
      Branch(IsStringInstanceType(lhs_instance_type), &if_lhsisstring,
             &if_lhsisnotstring);

      BIND(&if_lhsisstring);
      {
        var_lhs.Bind(lhs);
        var_rhs.Bind(rhs);
        Goto(&string_add_convert_right);
      }

      BIND(&if_lhsisnotstring);
      {
        // Check if {rhs} is a Smi.
        Label if_rhsissmi(this), if_rhsisnotsmi(this);
        Branch(TaggedIsSmi(rhs), &if_rhsissmi, &if_rhsisnotsmi);

        BIND(&if_rhsissmi);
        {
          // Check if {lhs} is a Number.
          Label if_lhsisnumber(this), if_lhsisnotnumber(this, Label::kDeferred);
          Branch(
              Word32Equal(lhs_instance_type, Int32Constant(HEAP_NUMBER_TYPE)),
              &if_lhsisnumber, &if_lhsisnotnumber);

          BIND(&if_lhsisnumber);
          {
            // The {lhs} is a HeapNumber, the {rhs} is a Smi, just add them.
            var_fadd_lhs.Bind(LoadHeapNumberValue(lhs));
            var_fadd_rhs.Bind(SmiToFloat64(rhs));
            Goto(&do_fadd);
          }

          BIND(&if_lhsisnotnumber);
          {
            // The {lhs} is neither a Number nor a String, and the {rhs} is a
            // Smi.
            Label if_lhsisreceiver(this, Label::kDeferred),
                if_lhsisnotreceiver(this, Label::kDeferred);
            Branch(IsJSReceiverInstanceType(lhs_instance_type),
                   &if_lhsisreceiver, &if_lhsisnotreceiver);

            BIND(&if_lhsisreceiver);
            {
              // Convert {lhs} to a primitive first passing no hint.
              Callable callable =
                  CodeFactory::NonPrimitiveToPrimitive(isolate());
              var_lhs.Bind(CallStub(callable, context, lhs));
              Goto(&loop);
            }

            BIND(&if_lhsisnotreceiver);
            {
              // Convert {lhs} to a Number first.
              Callable callable = CodeFactory::NonNumberToNumber(isolate());
              var_lhs.Bind(CallStub(callable, context, lhs));
              Goto(&loop);
            }
          }
        }

        BIND(&if_rhsisnotsmi);
        {
          // Load the instance type of {rhs}.
          Node* rhs_instance_type = LoadInstanceType(rhs);

          // Check if {rhs} is a String.
          Label if_rhsisstring(this), if_rhsisnotstring(this);
          Branch(IsStringInstanceType(rhs_instance_type), &if_rhsisstring,
                 &if_rhsisnotstring);

          BIND(&if_rhsisstring);
          {
            var_lhs.Bind(lhs);
            var_rhs.Bind(rhs);
            Goto(&string_add_convert_left);
          }

          BIND(&if_rhsisnotstring);
          {
            // Check if {lhs} is a HeapNumber.
            Label if_lhsisnumber(this), if_lhsisnotnumber(this);
            Branch(
                Word32Equal(lhs_instance_type, Int32Constant(HEAP_NUMBER_TYPE)),
                &if_lhsisnumber, &if_lhsisnotnumber);

            BIND(&if_lhsisnumber);
            {
              // Check if {rhs} is also a HeapNumber.
              Label if_rhsisnumber(this),
                  if_rhsisnotnumber(this, Label::kDeferred);
              Branch(Word32Equal(rhs_instance_type,
                                 Int32Constant(HEAP_NUMBER_TYPE)),
                     &if_rhsisnumber, &if_rhsisnotnumber);

              BIND(&if_rhsisnumber);
              {
                // Perform a floating point addition.
                var_fadd_lhs.Bind(LoadHeapNumberValue(lhs));
                var_fadd_rhs.Bind(LoadHeapNumberValue(rhs));
                Goto(&do_fadd);
              }

              BIND(&if_rhsisnotnumber);
              {
                // Check if {rhs} is a JSReceiver.
                Label if_rhsisreceiver(this, Label::kDeferred),
                    if_rhsisnotreceiver(this, Label::kDeferred);
                Branch(IsJSReceiverInstanceType(rhs_instance_type),
                       &if_rhsisreceiver, &if_rhsisnotreceiver);

                BIND(&if_rhsisreceiver);
                {
                  // Convert {rhs} to a primitive first passing no hint.
                  Callable callable =
                      CodeFactory::NonPrimitiveToPrimitive(isolate());
                  var_rhs.Bind(CallStub(callable, context, rhs));
                  Goto(&loop);
                }

                BIND(&if_rhsisnotreceiver);
                {
                  // Convert {rhs} to a Number first.
                  Callable callable = CodeFactory::NonNumberToNumber(isolate());
                  var_rhs.Bind(CallStub(callable, context, rhs));
                  Goto(&loop);
                }
              }
            }

            BIND(&if_lhsisnotnumber);
            {
              // Check if {lhs} is a JSReceiver.
              Label if_lhsisreceiver(this, Label::kDeferred),
                  if_lhsisnotreceiver(this);
              Branch(IsJSReceiverInstanceType(lhs_instance_type),
                     &if_lhsisreceiver, &if_lhsisnotreceiver);

              BIND(&if_lhsisreceiver);
              {
                // Convert {lhs} to a primitive first passing no hint.
                Callable callable =
                    CodeFactory::NonPrimitiveToPrimitive(isolate());
                var_lhs.Bind(CallStub(callable, context, lhs));
                Goto(&loop);
              }

              BIND(&if_lhsisnotreceiver);
              {
                // Check if {rhs} is a JSReceiver.
                Label if_rhsisreceiver(this, Label::kDeferred),
                    if_rhsisnotreceiver(this, Label::kDeferred);
                Branch(IsJSReceiverInstanceType(rhs_instance_type),
                       &if_rhsisreceiver, &if_rhsisnotreceiver);

                BIND(&if_rhsisreceiver);
                {
                  // Convert {rhs} to a primitive first passing no hint.
                  Callable callable =
                      CodeFactory::NonPrimitiveToPrimitive(isolate());
                  var_rhs.Bind(CallStub(callable, context, rhs));
                  Goto(&loop);
                }

                BIND(&if_rhsisnotreceiver);
                {
                  // Convert {lhs} to a Number first.
                  Callable callable = CodeFactory::NonNumberToNumber(isolate());
                  var_lhs.Bind(CallStub(callable, context, lhs));
                  Goto(&loop);
                }
              }
            }
          }
        }
      }
    }
  }
  BIND(&string_add_convert_left);
  {
    // Convert {lhs}, which is a Smi, to a String and concatenate the
    // resulting string with the String {rhs}.
    Callable callable =
        CodeFactory::StringAdd(isolate(), STRING_ADD_CONVERT_LEFT, NOT_TENURED);
    var_result.Bind(
        CallStub(callable, context, var_lhs.value(), var_rhs.value()));
    Goto(&end);
  }

  BIND(&string_add_convert_right);
  {
    // Convert {lhs}, which is a Smi, to a String and concatenate the
    // resulting string with the String {rhs}.
    Callable callable = CodeFactory::StringAdd(
        isolate(), STRING_ADD_CONVERT_RIGHT, NOT_TENURED);
    var_result.Bind(
        CallStub(callable, context, var_lhs.value(), var_rhs.value()));
    Goto(&end);
  }

  BIND(&do_fadd);
  {
    Node* lhs_value = var_fadd_lhs.value();
    Node* rhs_value = var_fadd_rhs.value();
    Node* value = Float64Add(lhs_value, rhs_value);
    Node* result = AllocateHeapNumberWithValue(value);
    var_result.Bind(result);
    Goto(&end);
  }
  BIND(&end);
  Return(var_result.value());
}

TF_BUILTIN(Subtract, CodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* left = Parameter(Descriptor::kLeft);
  Node* right = Parameter(Descriptor::kRight);

  // Shared entry for floating point subtraction.
  Label do_fsub(this), end(this);
  VARIABLE(var_fsub_lhs, MachineRepresentation::kFloat64);
  VARIABLE(var_fsub_rhs, MachineRepresentation::kFloat64);

  // We might need to loop several times due to ToPrimitive and/or ToNumber
  // conversions.
  VARIABLE(var_lhs, MachineRepresentation::kTagged);
  VARIABLE(var_rhs, MachineRepresentation::kTagged);
  VARIABLE(var_result, MachineRepresentation::kTagged);
  Variable* loop_vars[2] = {&var_lhs, &var_rhs};
  Label loop(this, 2, loop_vars);
  var_lhs.Bind(left);
  var_rhs.Bind(right);
  Goto(&loop);
  BIND(&loop);
  {
    // Load the current {lhs} and {rhs} values.
    Node* lhs = var_lhs.value();
    Node* rhs = var_rhs.value();

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
          // The result doesn't fit into Smi range.
          var_fsub_lhs.Bind(SmiToFloat64(lhs));
          var_fsub_rhs.Bind(SmiToFloat64(rhs));
          Goto(&do_fsub);
        }

        BIND(&if_notoverflow);
        var_result.Bind(BitcastWordToTaggedSigned(Projection(0, pair)));
        Goto(&end);
      }

      BIND(&if_rhsisnotsmi);
      {
        // Load the map of the {rhs}.
        Node* rhs_map = LoadMap(rhs);

        // Check if {rhs} is a HeapNumber.
        Label if_rhsisnumber(this), if_rhsisnotnumber(this, Label::kDeferred);
        Branch(IsHeapNumberMap(rhs_map), &if_rhsisnumber, &if_rhsisnotnumber);

        BIND(&if_rhsisnumber);
        {
          // Perform a floating point subtraction.
          var_fsub_lhs.Bind(SmiToFloat64(lhs));
          var_fsub_rhs.Bind(LoadHeapNumberValue(rhs));
          Goto(&do_fsub);
        }

        BIND(&if_rhsisnotnumber);
        {
          // Convert the {rhs} to a Number first.
          Callable callable = CodeFactory::NonNumberToNumber(isolate());
          var_rhs.Bind(CallStub(callable, context, rhs));
          Goto(&loop);
        }
      }
    }

    BIND(&if_lhsisnotsmi);
    {
      // Load the map of the {lhs}.
      Node* lhs_map = LoadMap(lhs);

      // Check if the {lhs} is a HeapNumber.
      Label if_lhsisnumber(this), if_lhsisnotnumber(this, Label::kDeferred);
      Branch(IsHeapNumberMap(lhs_map), &if_lhsisnumber, &if_lhsisnotnumber);

      BIND(&if_lhsisnumber);
      {
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
          Label if_rhsisnumber(this), if_rhsisnotnumber(this, Label::kDeferred);
          Branch(IsHeapNumberMap(rhs_map), &if_rhsisnumber, &if_rhsisnotnumber);

          BIND(&if_rhsisnumber);
          {
            // Perform a floating point subtraction.
            var_fsub_lhs.Bind(LoadHeapNumberValue(lhs));
            var_fsub_rhs.Bind(LoadHeapNumberValue(rhs));
            Goto(&do_fsub);
          }

          BIND(&if_rhsisnotnumber);
          {
            // Convert the {rhs} to a Number first.
            Callable callable = CodeFactory::NonNumberToNumber(isolate());
            var_rhs.Bind(CallStub(callable, context, rhs));
            Goto(&loop);
          }
        }
      }

      BIND(&if_lhsisnotnumber);
      {
        // Convert the {lhs} to a Number first.
        Callable callable = CodeFactory::NonNumberToNumber(isolate());
        var_lhs.Bind(CallStub(callable, context, lhs));
        Goto(&loop);
      }
    }
  }

  BIND(&do_fsub);
  {
    Node* lhs_value = var_fsub_lhs.value();
    Node* rhs_value = var_fsub_rhs.value();
    Node* value = Float64Sub(lhs_value, rhs_value);
    var_result.Bind(AllocateHeapNumberWithValue(value));
    Goto(&end);
  }
  BIND(&end);
  Return(var_result.value());
}

TF_BUILTIN(Multiply, CodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* left = Parameter(Descriptor::kLeft);
  Node* right = Parameter(Descriptor::kRight);

  // Shared entry point for floating point multiplication.
  Label do_fmul(this), return_result(this);
  VARIABLE(var_lhs_float64, MachineRepresentation::kFloat64);
  VARIABLE(var_rhs_float64, MachineRepresentation::kFloat64);

  // We might need to loop one or two times due to ToNumber conversions.
  VARIABLE(var_lhs, MachineRepresentation::kTagged);
  VARIABLE(var_rhs, MachineRepresentation::kTagged);
  VARIABLE(var_result, MachineRepresentation::kTagged);
  Variable* loop_variables[] = {&var_lhs, &var_rhs};
  Label loop(this, 2, loop_variables);
  var_lhs.Bind(left);
  var_rhs.Bind(right);
  Goto(&loop);
  BIND(&loop);
  {
    Node* lhs = var_lhs.value();
    Node* rhs = var_rhs.value();

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
        Goto(&return_result);
      }

      BIND(&rhs_is_not_smi);
      {
        Node* rhs_map = LoadMap(rhs);

        // Check if {rhs} is a HeapNumber.
        Label rhs_is_number(this), rhs_is_not_number(this, Label::kDeferred);
        Branch(IsHeapNumberMap(rhs_map), &rhs_is_number, &rhs_is_not_number);

        BIND(&rhs_is_number);
        {
          // Convert {lhs} to a double and multiply it with the value of {rhs}.
          var_lhs_float64.Bind(SmiToFloat64(lhs));
          var_rhs_float64.Bind(LoadHeapNumberValue(rhs));
          Goto(&do_fmul);
        }

        BIND(&rhs_is_not_number);
        {
          // Multiplication is commutative, swap {lhs} with {rhs} and loop.
          var_lhs.Bind(rhs);
          var_rhs.Bind(lhs);
          Goto(&loop);
        }
      }
    }

    BIND(&lhs_is_not_smi);
    {
      Node* lhs_map = LoadMap(lhs);

      // Check if {lhs} is a HeapNumber.
      Label lhs_is_number(this), lhs_is_not_number(this, Label::kDeferred);
      Branch(IsHeapNumberMap(lhs_map), &lhs_is_number, &lhs_is_not_number);

      BIND(&lhs_is_number);
      {
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
          Label rhs_is_number(this), rhs_is_not_number(this, Label::kDeferred);
          Branch(IsHeapNumberMap(rhs_map), &rhs_is_number, &rhs_is_not_number);

          BIND(&rhs_is_number);
          {
            // Both {lhs} and {rhs} are HeapNumbers. Load their values and
            // multiply them.
            var_lhs_float64.Bind(LoadHeapNumberValue(lhs));
            var_rhs_float64.Bind(LoadHeapNumberValue(rhs));
            Goto(&do_fmul);
          }

          BIND(&rhs_is_not_number);
          {
            // Multiplication is commutative, swap {lhs} with {rhs} and loop.
            var_lhs.Bind(rhs);
            var_rhs.Bind(lhs);
            Goto(&loop);
          }
        }
      }

      BIND(&lhs_is_not_number);
      {
        // Convert {lhs} to a Number and loop.
        Callable callable = CodeFactory::NonNumberToNumber(isolate());
        var_lhs.Bind(CallStub(callable, context, lhs));
        Goto(&loop);
      }
    }
  }

  BIND(&do_fmul);
  {
    Node* value = Float64Mul(var_lhs_float64.value(), var_rhs_float64.value());
    Node* result = AllocateHeapNumberWithValue(value);
    var_result.Bind(result);
    Goto(&return_result);
  }

  BIND(&return_result);
  Return(var_result.value());
}

TF_BUILTIN(Divide, CodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* left = Parameter(Descriptor::kLeft);
  Node* right = Parameter(Descriptor::kRight);

  // Shared entry point for floating point division.
  Label do_fdiv(this), end(this);
  VARIABLE(var_dividend_float64, MachineRepresentation::kFloat64);
  VARIABLE(var_divisor_float64, MachineRepresentation::kFloat64);

  // We might need to loop one or two times due to ToNumber conversions.
  VARIABLE(var_dividend, MachineRepresentation::kTagged);
  VARIABLE(var_divisor, MachineRepresentation::kTagged);
  VARIABLE(var_result, MachineRepresentation::kTagged);
  Variable* loop_variables[] = {&var_dividend, &var_divisor};
  Label loop(this, 2, loop_variables);
  var_dividend.Bind(left);
  var_divisor.Bind(right);
  Goto(&loop);
  BIND(&loop);
  {
    Node* dividend = var_dividend.value();
    Node* divisor = var_divisor.value();

    Label dividend_is_smi(this), dividend_is_not_smi(this);
    Branch(TaggedIsSmi(dividend), &dividend_is_smi, &dividend_is_not_smi);

    BIND(&dividend_is_smi);
    {
      Label divisor_is_smi(this), divisor_is_not_smi(this);
      Branch(TaggedIsSmi(divisor), &divisor_is_smi, &divisor_is_not_smi);

      BIND(&divisor_is_smi);
      {
        Label bailout(this);

        // Do floating point division if {divisor} is zero.
        GotoIf(SmiEqual(divisor, SmiConstant(0)), &bailout);

        // Do floating point division {dividend} is zero and {divisor} is
        // negative.
        Label dividend_is_zero(this), dividend_is_not_zero(this);
        Branch(SmiEqual(dividend, SmiConstant(0)), &dividend_is_zero,
               &dividend_is_not_zero);

        BIND(&dividend_is_zero);
        {
          GotoIf(SmiLessThan(divisor, SmiConstant(0)), &bailout);
          Goto(&dividend_is_not_zero);
        }
        BIND(&dividend_is_not_zero);

        Node* untagged_divisor = SmiToWord32(divisor);
        Node* untagged_dividend = SmiToWord32(dividend);

        // Do floating point division if {dividend} is kMinInt (or kMinInt - 1
        // if the Smi size is 31) and {divisor} is -1.
        Label divisor_is_minus_one(this), divisor_is_not_minus_one(this);
        Branch(Word32Equal(untagged_divisor, Int32Constant(-1)),
               &divisor_is_minus_one, &divisor_is_not_minus_one);

        BIND(&divisor_is_minus_one);
        {
          GotoIf(
              Word32Equal(untagged_dividend,
                          Int32Constant(kSmiValueSize == 32 ? kMinInt
                                                            : (kMinInt >> 1))),
              &bailout);
          Goto(&divisor_is_not_minus_one);
        }
        BIND(&divisor_is_not_minus_one);

        // TODO(epertoso): consider adding a machine instruction that returns
        // both the result and the remainder.
        Node* untagged_result = Int32Div(untagged_dividend, untagged_divisor);
        Node* truncated = Int32Mul(untagged_result, untagged_divisor);
        // Do floating point division if the remainder is not 0.
        GotoIf(Word32NotEqual(untagged_dividend, truncated), &bailout);
        var_result.Bind(SmiFromWord32(untagged_result));
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
        Label divisor_is_number(this),
            divisor_is_not_number(this, Label::kDeferred);
        Branch(IsHeapNumberMap(divisor_map), &divisor_is_number,
               &divisor_is_not_number);

        BIND(&divisor_is_number);
        {
          // Convert {dividend} to a double and divide it with the value of
          // {divisor}.
          var_dividend_float64.Bind(SmiToFloat64(dividend));
          var_divisor_float64.Bind(LoadHeapNumberValue(divisor));
          Goto(&do_fdiv);
        }

        BIND(&divisor_is_not_number);
        {
          // Convert {divisor} to a number and loop.
          Callable callable = CodeFactory::NonNumberToNumber(isolate());
          var_divisor.Bind(CallStub(callable, context, divisor));
          Goto(&loop);
        }
      }
    }

    BIND(&dividend_is_not_smi);
    {
      Node* dividend_map = LoadMap(dividend);

      // Check if {dividend} is a HeapNumber.
      Label dividend_is_number(this),
          dividend_is_not_number(this, Label::kDeferred);
      Branch(IsHeapNumberMap(dividend_map), &dividend_is_number,
             &dividend_is_not_number);

      BIND(&dividend_is_number);
      {
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
          Label divisor_is_number(this),
              divisor_is_not_number(this, Label::kDeferred);
          Branch(IsHeapNumberMap(divisor_map), &divisor_is_number,
                 &divisor_is_not_number);

          BIND(&divisor_is_number);
          {
            // Both {dividend} and {divisor} are HeapNumbers. Load their values
            // and divide them.
            var_dividend_float64.Bind(LoadHeapNumberValue(dividend));
            var_divisor_float64.Bind(LoadHeapNumberValue(divisor));
            Goto(&do_fdiv);
          }

          BIND(&divisor_is_not_number);
          {
            // Convert {divisor} to a number and loop.
            Callable callable = CodeFactory::NonNumberToNumber(isolate());
            var_divisor.Bind(CallStub(callable, context, divisor));
            Goto(&loop);
          }
        }
      }

      BIND(&dividend_is_not_number);
      {
        // Convert {dividend} to a Number and loop.
        Callable callable = CodeFactory::NonNumberToNumber(isolate());
        var_dividend.Bind(CallStub(callable, context, dividend));
        Goto(&loop);
      }
    }
  }

  BIND(&do_fdiv);
  {
    Node* value =
        Float64Div(var_dividend_float64.value(), var_divisor_float64.value());
    var_result.Bind(AllocateHeapNumberWithValue(value));
    Goto(&end);
  }
  BIND(&end);
  Return(var_result.value());
}

TF_BUILTIN(Modulus, CodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* left = Parameter(Descriptor::kLeft);
  Node* right = Parameter(Descriptor::kRight);

  VARIABLE(var_result, MachineRepresentation::kTagged);
  Label return_result(this, &var_result);

  // Shared entry point for floating point modulus.
  Label do_fmod(this);
  VARIABLE(var_dividend_float64, MachineRepresentation::kFloat64);
  VARIABLE(var_divisor_float64, MachineRepresentation::kFloat64);

  // We might need to loop one or two times due to ToNumber conversions.
  VARIABLE(var_dividend, MachineRepresentation::kTagged);
  VARIABLE(var_divisor, MachineRepresentation::kTagged);
  Variable* loop_variables[] = {&var_dividend, &var_divisor};
  Label loop(this, 2, loop_variables);
  var_dividend.Bind(left);
  var_divisor.Bind(right);
  Goto(&loop);
  BIND(&loop);
  {
    Node* dividend = var_dividend.value();
    Node* divisor = var_divisor.value();

    Label dividend_is_smi(this), dividend_is_not_smi(this);
    Branch(TaggedIsSmi(dividend), &dividend_is_smi, &dividend_is_not_smi);

    BIND(&dividend_is_smi);
    {
      Label dividend_is_not_zero(this);
      Label divisor_is_smi(this), divisor_is_not_smi(this);
      Branch(TaggedIsSmi(divisor), &divisor_is_smi, &divisor_is_not_smi);

      BIND(&divisor_is_smi);
      {
        // Compute the modulus of two Smis.
        var_result.Bind(SmiMod(dividend, divisor));
        Goto(&return_result);
      }

      BIND(&divisor_is_not_smi);
      {
        Node* divisor_map = LoadMap(divisor);

        // Check if {divisor} is a HeapNumber.
        Label divisor_is_number(this),
            divisor_is_not_number(this, Label::kDeferred);
        Branch(IsHeapNumberMap(divisor_map), &divisor_is_number,
               &divisor_is_not_number);

        BIND(&divisor_is_number);
        {
          // Convert {dividend} to a double and compute its modulus with the
          // value of {dividend}.
          var_dividend_float64.Bind(SmiToFloat64(dividend));
          var_divisor_float64.Bind(LoadHeapNumberValue(divisor));
          Goto(&do_fmod);
        }

        BIND(&divisor_is_not_number);
        {
          // Convert {divisor} to a number and loop.
          Callable callable = CodeFactory::NonNumberToNumber(isolate());
          var_divisor.Bind(CallStub(callable, context, divisor));
          Goto(&loop);
        }
      }
    }

    BIND(&dividend_is_not_smi);
    {
      Node* dividend_map = LoadMap(dividend);

      // Check if {dividend} is a HeapNumber.
      Label dividend_is_number(this),
          dividend_is_not_number(this, Label::kDeferred);
      Branch(IsHeapNumberMap(dividend_map), &dividend_is_number,
             &dividend_is_not_number);

      BIND(&dividend_is_number);
      {
        // Check if {divisor} is a Smi.
        Label divisor_is_smi(this), divisor_is_not_smi(this);
        Branch(TaggedIsSmi(divisor), &divisor_is_smi, &divisor_is_not_smi);

        BIND(&divisor_is_smi);
        {
          // Convert {divisor} to a double and compute {dividend}'s modulus with
          // it.
          var_dividend_float64.Bind(LoadHeapNumberValue(dividend));
          var_divisor_float64.Bind(SmiToFloat64(divisor));
          Goto(&do_fmod);
        }

        BIND(&divisor_is_not_smi);
        {
          Node* divisor_map = LoadMap(divisor);

          // Check if {divisor} is a HeapNumber.
          Label divisor_is_number(this),
              divisor_is_not_number(this, Label::kDeferred);
          Branch(IsHeapNumberMap(divisor_map), &divisor_is_number,
                 &divisor_is_not_number);

          BIND(&divisor_is_number);
          {
            // Both {dividend} and {divisor} are HeapNumbers. Load their values
            // and compute their modulus.
            var_dividend_float64.Bind(LoadHeapNumberValue(dividend));
            var_divisor_float64.Bind(LoadHeapNumberValue(divisor));
            Goto(&do_fmod);
          }

          BIND(&divisor_is_not_number);
          {
            // Convert {divisor} to a number and loop.
            Callable callable = CodeFactory::NonNumberToNumber(isolate());
            var_divisor.Bind(CallStub(callable, context, divisor));
            Goto(&loop);
          }
        }
      }

      BIND(&dividend_is_not_number);
      {
        // Convert {dividend} to a Number and loop.
        Callable callable = CodeFactory::NonNumberToNumber(isolate());
        var_dividend.Bind(CallStub(callable, context, dividend));
        Goto(&loop);
      }
    }
  }

  BIND(&do_fmod);
  {
    Node* value =
        Float64Mod(var_dividend_float64.value(), var_divisor_float64.value());
    var_result.Bind(AllocateHeapNumberWithValue(value));
    Goto(&return_result);
  }

  BIND(&return_result);
  Return(var_result.value());
}

TF_BUILTIN(ShiftLeft, NumberBuiltinsAssembler) {
  BitwiseShiftOp<Descriptor>([=](Node* lhs, Node* shift_count) {
    return Word32Shl(lhs, shift_count);
  });
}

TF_BUILTIN(ShiftRight, NumberBuiltinsAssembler) {
  BitwiseShiftOp<Descriptor>([=](Node* lhs, Node* shift_count) {
    return Word32Sar(lhs, shift_count);
  });
}

TF_BUILTIN(ShiftRightLogical, NumberBuiltinsAssembler) {
  BitwiseShiftOp<Descriptor>(
      [=](Node* lhs, Node* shift_count) { return Word32Shr(lhs, shift_count); },
      kUnsigned);
}

TF_BUILTIN(BitwiseAnd, NumberBuiltinsAssembler) {
  BitwiseOp<Descriptor>(
      [=](Node* lhs, Node* rhs) { return Word32And(lhs, rhs); });
}

TF_BUILTIN(BitwiseOr, NumberBuiltinsAssembler) {
  BitwiseOp<Descriptor>(
      [=](Node* lhs, Node* rhs) { return Word32Or(lhs, rhs); });
}

TF_BUILTIN(BitwiseXor, NumberBuiltinsAssembler) {
  BitwiseOp<Descriptor>(
      [=](Node* lhs, Node* rhs) { return Word32Xor(lhs, rhs); });
}

TF_BUILTIN(LessThan, NumberBuiltinsAssembler) {
  RelationalComparisonBuiltin<Descriptor>(kLessThan);
}

TF_BUILTIN(LessThanOrEqual, NumberBuiltinsAssembler) {
  RelationalComparisonBuiltin<Descriptor>(kLessThanOrEqual);
}

TF_BUILTIN(GreaterThan, NumberBuiltinsAssembler) {
  RelationalComparisonBuiltin<Descriptor>(kGreaterThan);
}

TF_BUILTIN(GreaterThanOrEqual, NumberBuiltinsAssembler) {
  RelationalComparisonBuiltin<Descriptor>(kGreaterThanOrEqual);
}

TF_BUILTIN(Equal, CodeStubAssembler) {
  Node* lhs = Parameter(Descriptor::kLeft);
  Node* rhs = Parameter(Descriptor::kRight);
  Node* context = Parameter(Descriptor::kContext);

  Return(Equal(lhs, rhs, context));
}

TF_BUILTIN(StrictEqual, CodeStubAssembler) {
  Node* lhs = Parameter(Descriptor::kLeft);
  Node* rhs = Parameter(Descriptor::kRight);

  Return(StrictEqual(lhs, rhs));
}

TF_BUILTIN(AddWithFeedback, BinaryOpAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* left = Parameter(Descriptor::kLeft);
  Node* right = Parameter(Descriptor::kRight);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);

  Return(Generate_AddWithFeedback(context, left, right,
                                  ChangeUint32ToWord(slot), vector));
}

TF_BUILTIN(SubtractWithFeedback, BinaryOpAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* left = Parameter(Descriptor::kLeft);
  Node* right = Parameter(Descriptor::kRight);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);

  Return(Generate_SubtractWithFeedback(context, left, right,
                                       ChangeUint32ToWord(slot), vector));
}

TF_BUILTIN(MultiplyWithFeedback, BinaryOpAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* left = Parameter(Descriptor::kLeft);
  Node* right = Parameter(Descriptor::kRight);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);

  Return(Generate_MultiplyWithFeedback(context, left, right,
                                       ChangeUint32ToWord(slot), vector));
}

TF_BUILTIN(DivideWithFeedback, BinaryOpAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* left = Parameter(Descriptor::kLeft);
  Node* right = Parameter(Descriptor::kRight);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);

  Return(Generate_DivideWithFeedback(context, left, right,
                                     ChangeUint32ToWord(slot), vector));
}

TF_BUILTIN(ModulusWithFeedback, BinaryOpAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* left = Parameter(Descriptor::kLeft);
  Node* right = Parameter(Descriptor::kRight);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);

  Return(Generate_ModulusWithFeedback(context, left, right,
                                      ChangeUint32ToWord(slot), vector));
}

}  // namespace internal
}  // namespace v8
