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

  template <typename Descriptor>
  void BinaryOp(Label* smis, Variable* var_left, Variable* var_right,
                Label* doubles, Variable* var_left_double,
                Variable* var_right_double);
};

// ES6 #sec-number.isfinite
TF_BUILTIN(NumberIsFinite, CodeStubAssembler) {
  Node* number = Parameter(Descriptor::kNumber);

  Label return_true(this), return_false(this);

  // Check if {number} is a Smi.
  GotoIf(TaggedIsSmi(number), &return_true);

  // Check if {number} is a HeapNumber.
  GotoIfNot(IsHeapNumber(number), &return_false);

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
  GotoIfNot(IsHeapNumber(number), &return_false);

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
  GotoIfNot(IsHeapNumber(number), &return_false);

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
  GotoIfNot(IsHeapNumber(number), &return_false);

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
        Branch(IsClearWord32(input_hash,
                             Name::kDoesNotContainCachedArrayIndexMask),
               &if_inputcached, &if_inputnotcached);

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
          var_input.Bind(CallBuiltin(Builtins::kToString, context, input));
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
  GotoIf(WordEqual(radix, SmiConstant(10)), &if_radix10);
  GotoIf(WordEqual(radix, SmiConstant(0)), &if_radix10);
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
      GotoIf(IsSetWord32(input_hash, Name::kDoesNotContainCachedArrayIndexMask),
             &if_generic);

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

class AddStubAssembler : public CodeStubAssembler {
 public:
  explicit AddStubAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

 protected:
  void ConvertReceiverAndLoop(Variable* var_value, Label* loop, Node* context) {
    // Call ToPrimitive explicitly without hint (whereas ToNumber
    // would pass a "number" hint).
    Callable callable = CodeFactory::NonPrimitiveToPrimitive(isolate());
    var_value->Bind(CallStub(callable, context, var_value->value()));
    Goto(loop);
  }

  void ConvertNonReceiverAndLoop(Variable* var_value, Label* loop,
                                 Node* context) {
    var_value->Bind(
        CallBuiltin(Builtins::kNonNumberToNumber, context, var_value->value()));
    Goto(loop);
  }

  void ConvertAndLoop(Variable* var_value, Node* instance_type, Label* loop,
                      Node* context) {
    Label is_not_receiver(this, Label::kDeferred);
    GotoIfNot(IsJSReceiverInstanceType(instance_type), &is_not_receiver);

    ConvertReceiverAndLoop(var_value, loop, context);

    BIND(&is_not_receiver);
    ConvertNonReceiverAndLoop(var_value, loop, context);
  }
};

TF_BUILTIN(Add, AddStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  VARIABLE(var_left, MachineRepresentation::kTagged,
           Parameter(Descriptor::kLeft));
  VARIABLE(var_right, MachineRepresentation::kTagged,
           Parameter(Descriptor::kRight));

  // Shared entry for floating point addition.
  Label do_double_add(this);
  VARIABLE(var_left_double, MachineRepresentation::kFloat64);
  VARIABLE(var_right_double, MachineRepresentation::kFloat64);

  // We might need to loop several times due to ToPrimitive, ToString and/or
  // ToNumber conversions.
  VARIABLE(var_result, MachineRepresentation::kTagged);
  Variable* loop_vars[2] = {&var_left, &var_right};
  Label loop(this, 2, loop_vars),
      string_add_convert_left(this, Label::kDeferred),
      string_add_convert_right(this, Label::kDeferred);
  Goto(&loop);
  BIND(&loop);
  {
    Node* left = var_left.value();
    Node* right = var_right.value();

    Label if_left_smi(this), if_left_heapobject(this);
    Branch(TaggedIsSmi(left), &if_left_smi, &if_left_heapobject);

    BIND(&if_left_smi);
    {
      Label if_right_smi(this), if_right_heapobject(this);
      Branch(TaggedIsSmi(right), &if_right_smi, &if_right_heapobject);

      BIND(&if_right_smi);
      {
        // Try fast Smi addition first, bail out if it overflows.
        Node* pair = IntPtrAddWithOverflow(BitcastTaggedToWord(left),
                                           BitcastTaggedToWord(right));
        Node* overflow = Projection(1, pair);
        Label if_overflow(this);
        GotoIf(overflow, &if_overflow);
        Return(BitcastWordToTaggedSigned(Projection(0, pair)));

        BIND(&if_overflow);
        {
          var_left_double.Bind(SmiToFloat64(left));
          var_right_double.Bind(SmiToFloat64(right));
          Goto(&do_double_add);
        }
      }  // if_right_smi

      BIND(&if_right_heapobject);
      {
        Node* right_map = LoadMap(right);

        Label if_right_not_number(this, Label::kDeferred);
        GotoIfNot(IsHeapNumberMap(right_map), &if_right_not_number);

        // {right} is a HeapNumber.
        var_left_double.Bind(SmiToFloat64(left));
        var_right_double.Bind(LoadHeapNumberValue(right));
        Goto(&do_double_add);

        BIND(&if_right_not_number);
        {
          Node* right_instance_type = LoadMapInstanceType(right_map);
          GotoIf(IsStringInstanceType(right_instance_type),
                 &string_add_convert_left);
          ConvertAndLoop(&var_right, right_instance_type, &loop, context);
        }
      }  // if_right_heapobject
    }    // if_left_smi

    BIND(&if_left_heapobject);
    {
      Node* left_map = LoadMap(left);
      Label if_right_smi(this), if_right_heapobject(this);
      Branch(TaggedIsSmi(right), &if_right_smi, &if_right_heapobject);

      BIND(&if_right_smi);
      {
        Label if_left_not_number(this, Label::kDeferred);
        GotoIfNot(IsHeapNumberMap(left_map), &if_left_not_number);

        // {left} is a HeapNumber, {right} is a Smi.
        var_left_double.Bind(LoadHeapNumberValue(left));
        var_right_double.Bind(SmiToFloat64(right));
        Goto(&do_double_add);

        BIND(&if_left_not_number);
        {
          Node* left_instance_type = LoadMapInstanceType(left_map);
          GotoIf(IsStringInstanceType(left_instance_type),
                 &string_add_convert_right);
          // {left} is neither a Number nor a String, and {right} is a Smi.
          ConvertAndLoop(&var_left, left_instance_type, &loop, context);
        }
      }  // if_right_smi

      BIND(&if_right_heapobject);
      {
        Node* right_map = LoadMap(right);

        Label if_left_number(this), if_left_not_number(this, Label::kDeferred);
        Branch(IsHeapNumberMap(left_map), &if_left_number, &if_left_not_number);

        BIND(&if_left_number);
        {
          Label if_right_not_number(this, Label::kDeferred);
          GotoIfNot(IsHeapNumberMap(right_map), &if_right_not_number);

          // Both {left} and {right} are HeapNumbers.
          var_left_double.Bind(LoadHeapNumberValue(left));
          var_right_double.Bind(LoadHeapNumberValue(right));
          Goto(&do_double_add);

          BIND(&if_right_not_number);
          {
            Node* right_instance_type = LoadMapInstanceType(right_map);
            GotoIf(IsStringInstanceType(right_instance_type),
                   &string_add_convert_left);
            // {left} is a HeapNumber, {right} is neither Number nor String.
            ConvertAndLoop(&var_right, right_instance_type, &loop, context);
          }
        }  // if_left_number

        BIND(&if_left_not_number);
        {
          Node* left_instance_type = LoadMapInstanceType(left_map);
          GotoIf(IsStringInstanceType(left_instance_type),
                 &string_add_convert_right);
          Node* right_instance_type = LoadMapInstanceType(right_map);
          GotoIf(IsStringInstanceType(right_instance_type),
                 &string_add_convert_left);
          Label if_left_not_receiver(this, Label::kDeferred);
          Label if_right_not_receiver(this, Label::kDeferred);
          GotoIfNot(IsJSReceiverInstanceType(left_instance_type),
                    &if_left_not_receiver);
          // {left} is a JSReceiver, convert it first.
          ConvertReceiverAndLoop(&var_left, &loop, context);

          BIND(&if_left_not_receiver);
          GotoIfNot(IsJSReceiverInstanceType(right_instance_type),
                    &if_right_not_receiver);
          // {left} is a Primitive, but {right} is a JSReceiver, so convert
          // {right} with priority.
          ConvertReceiverAndLoop(&var_right, &loop, context);

          BIND(&if_right_not_receiver);
          // Neither {left} nor {right} are JSReceivers.
          ConvertNonReceiverAndLoop(&var_left, &loop, context);
        }
      }  // if_right_heapobject
    }    // if_left_heapobject
  }
  BIND(&string_add_convert_left);
  {
    // Convert {left} to a String and concatenate it with the String {right}.
    Callable callable =
        CodeFactory::StringAdd(isolate(), STRING_ADD_CONVERT_LEFT, NOT_TENURED);
    Return(CallStub(callable, context, var_left.value(), var_right.value()));
  }

  BIND(&string_add_convert_right);
  {
    // Convert {right} to a String and concatenate it with the String {left}.
    Callable callable = CodeFactory::StringAdd(
        isolate(), STRING_ADD_CONVERT_RIGHT, NOT_TENURED);
    Return(CallStub(callable, context, var_left.value(), var_right.value()));
  }

  BIND(&do_double_add);
  {
    Node* value = Float64Add(var_left_double.value(), var_right_double.value());
    Return(AllocateHeapNumberWithValue(value));
  }
}

template <typename Descriptor>
void NumberBuiltinsAssembler::BinaryOp(Label* smis, Variable* var_left,
                                       Variable* var_right, Label* doubles,
                                       Variable* var_left_double,
                                       Variable* var_right_double) {
  DCHECK(var_left->rep() == MachineRepresentation::kTagged);
  DCHECK(var_right->rep() == MachineRepresentation::kTagged);

  Node* context = Parameter(Descriptor::kContext);
  var_left->Bind(Parameter(Descriptor::kLeft));
  var_right->Bind(Parameter(Descriptor::kRight));

  // We might need to loop for ToNumber conversions.
  Label loop(this, {var_left, var_right});
  Goto(&loop);
  BIND(&loop);

  Label left_not_smi(this), right_not_smi(this);
  Label left_not_number(this), right_not_number(this);
  GotoIfNot(TaggedIsSmi(var_left->value()), &left_not_smi);
  GotoIf(TaggedIsSmi(var_right->value()), smis);

  // At this point, var_left is a Smi but var_right is not.
  GotoIfNot(IsHeapNumber(var_right->value()), &right_not_number);
  var_left_double->Bind(SmiToFloat64(var_left->value()));
  var_right_double->Bind(LoadHeapNumberValue(var_right->value()));
  Goto(doubles);

  BIND(&left_not_smi);
  {
    GotoIfNot(IsHeapNumber(var_left->value()), &left_not_number);
    GotoIfNot(TaggedIsSmi(var_right->value()), &right_not_smi);

    // At this point, var_left is a HeapNumber and var_right is a Smi.
    var_left_double->Bind(LoadHeapNumberValue(var_left->value()));
    var_right_double->Bind(SmiToFloat64(var_right->value()));
    Goto(doubles);
  }

  BIND(&right_not_smi);
  {
    GotoIfNot(IsHeapNumber(var_right->value()), &right_not_number);
    var_left_double->Bind(LoadHeapNumberValue(var_left->value()));
    var_right_double->Bind(LoadHeapNumberValue(var_right->value()));
    Goto(doubles);
  }

  BIND(&left_not_number);
  {
    var_left->Bind(
        CallBuiltin(Builtins::kNonNumberToNumber, context, var_left->value()));
    Goto(&loop);
  }

  BIND(&right_not_number);
  {
    var_right->Bind(
        CallBuiltin(Builtins::kNonNumberToNumber, context, var_right->value()));
    Goto(&loop);
  }
}

TF_BUILTIN(Subtract, NumberBuiltinsAssembler) {
  VARIABLE(var_left, MachineRepresentation::kTagged);
  VARIABLE(var_right, MachineRepresentation::kTagged);
  VARIABLE(var_left_double, MachineRepresentation::kFloat64);
  VARIABLE(var_right_double, MachineRepresentation::kFloat64);
  Label do_smi_sub(this), do_double_sub(this);

  BinaryOp<Descriptor>(&do_smi_sub, &var_left, &var_right, &do_double_sub,
                       &var_left_double, &var_right_double);

  BIND(&do_smi_sub);
  {
    // Try a fast Smi subtraction first, bail out if it overflows.
    Node* pair = IntPtrSubWithOverflow(BitcastTaggedToWord(var_left.value()),
                                       BitcastTaggedToWord(var_right.value()));
    Node* overflow = Projection(1, pair);
    Label if_overflow(this), if_notoverflow(this);
    Branch(overflow, &if_overflow, &if_notoverflow);

    BIND(&if_overflow);
    {
      var_left_double.Bind(SmiToFloat64(var_left.value()));
      var_right_double.Bind(SmiToFloat64(var_right.value()));
      Goto(&do_double_sub);
    }

    BIND(&if_notoverflow);
    Return(BitcastWordToTaggedSigned(Projection(0, pair)));
  }

  BIND(&do_double_sub);
  {
    Node* value = Float64Sub(var_left_double.value(), var_right_double.value());
    Return(AllocateHeapNumberWithValue(value));
  }
}

TF_BUILTIN(Multiply, NumberBuiltinsAssembler) {
  VARIABLE(var_left, MachineRepresentation::kTagged);
  VARIABLE(var_right, MachineRepresentation::kTagged);
  VARIABLE(var_left_double, MachineRepresentation::kFloat64);
  VARIABLE(var_right_double, MachineRepresentation::kFloat64);
  Label do_smi_mul(this), do_double_mul(this);

  BinaryOp<Descriptor>(&do_smi_mul, &var_left, &var_right, &do_double_mul,
                       &var_left_double, &var_right_double);

  BIND(&do_smi_mul);
  // The result is not necessarily a smi, in case of overflow.
  Return(SmiMul(var_left.value(), var_right.value()));

  BIND(&do_double_mul);
  Node* value = Float64Mul(var_left_double.value(), var_right_double.value());
  Return(AllocateHeapNumberWithValue(value));
}

TF_BUILTIN(Divide, NumberBuiltinsAssembler) {
  VARIABLE(var_left, MachineRepresentation::kTagged);
  VARIABLE(var_right, MachineRepresentation::kTagged);
  VARIABLE(var_left_double, MachineRepresentation::kFloat64);
  VARIABLE(var_right_double, MachineRepresentation::kFloat64);
  Label do_smi_div(this), do_double_div(this);

  BinaryOp<Descriptor>(&do_smi_div, &var_left, &var_right, &do_double_div,
                       &var_left_double, &var_right_double);

  BIND(&do_smi_div);
  {
    // TODO(jkummerow): Consider just always doing a double division.
    Label bailout(this);
    Node* dividend = var_left.value();
    Node* divisor = var_right.value();

    // Do floating point division if {divisor} is zero.
    GotoIf(SmiEqual(divisor, SmiConstant(0)), &bailout);

    // Do floating point division if {dividend} is zero and {divisor} is
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
      GotoIf(Word32Equal(
                 untagged_dividend,
                 Int32Constant(kSmiValueSize == 32 ? kMinInt : (kMinInt >> 1))),
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
    Return(SmiFromWord32(untagged_result));

    // Bailout: convert {dividend} and {divisor} to double and do double
    // division.
    BIND(&bailout);
    {
      var_left_double.Bind(SmiToFloat64(dividend));
      var_right_double.Bind(SmiToFloat64(divisor));
      Goto(&do_double_div);
    }
  }

  BIND(&do_double_div);
  {
    Node* value = Float64Div(var_left_double.value(), var_right_double.value());
    Return(AllocateHeapNumberWithValue(value));
  }
}

TF_BUILTIN(Modulus, NumberBuiltinsAssembler) {
  VARIABLE(var_left, MachineRepresentation::kTagged);
  VARIABLE(var_right, MachineRepresentation::kTagged);
  VARIABLE(var_left_double, MachineRepresentation::kFloat64);
  VARIABLE(var_right_double, MachineRepresentation::kFloat64);
  Label do_smi_mod(this), do_double_mod(this);

  BinaryOp<Descriptor>(&do_smi_mod, &var_left, &var_right, &do_double_mod,
                       &var_left_double, &var_right_double);

  BIND(&do_smi_mod);
  Return(SmiMod(var_left.value(), var_right.value()));

  BIND(&do_double_mod);
  Node* value = Float64Mod(var_left_double.value(), var_right_double.value());
  Return(AllocateHeapNumberWithValue(value));
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

}  // namespace internal
}  // namespace v8
