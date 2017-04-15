// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils.h"
#include "src/builtins/builtins.h"
#include "src/code-factory.h"
#include "src/code-stub-assembler.h"

namespace v8 {
namespace internal {

class NumberBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit NumberBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

 protected:
  template <Signedness signed_result = kSigned>
  void BitwiseOp(std::function<Node*(Node* lhs, Node* rhs)> body) {
    Node* left = Parameter(0);
    Node* right = Parameter(1);
    Node* context = Parameter(2);

    Node* lhs_value = TruncateTaggedToWord32(context, left);
    Node* rhs_value = TruncateTaggedToWord32(context, right);
    Node* value = body(lhs_value, rhs_value);
    Node* result = signed_result == kSigned ? ChangeInt32ToTagged(value)
                                            : ChangeUint32ToTagged(value);
    Return(result);
  }

  template <Signedness signed_result = kSigned>
  void BitwiseShiftOp(std::function<Node*(Node* lhs, Node* shift_count)> body) {
    BitwiseOp<signed_result>([this, body](Node* lhs, Node* rhs) {
      Node* shift_count = Word32And(rhs, Int32Constant(0x1f));
      return body(lhs, shift_count);
    });
  }

  void RelationalComparisonBuiltin(RelationalComparisonMode mode) {
    Node* lhs = Parameter(0);
    Node* rhs = Parameter(1);
    Node* context = Parameter(2);

    Return(RelationalComparison(mode, lhs, rhs, context));
  }
};

// -----------------------------------------------------------------------------
// ES6 section 20.1 Number Objects

// ES6 section 20.1.2.2 Number.isFinite ( number )
TF_BUILTIN(NumberIsFinite, CodeStubAssembler) {
  Node* number = Parameter(1);

  Label return_true(this), return_false(this);

  // Check if {number} is a Smi.
  GotoIf(TaggedIsSmi(number), &return_true);

  // Check if {number} is a HeapNumber.
  GotoUnless(IsHeapNumberMap(LoadMap(number)), &return_false);

  // Check if {number} contains a finite, non-NaN value.
  Node* number_value = LoadHeapNumberValue(number);
  BranchIfFloat64IsNaN(Float64Sub(number_value, number_value), &return_false,
                       &return_true);

  Bind(&return_true);
  Return(BooleanConstant(true));

  Bind(&return_false);
  Return(BooleanConstant(false));
}

// ES6 section 20.1.2.3 Number.isInteger ( number )
TF_BUILTIN(NumberIsInteger, CodeStubAssembler) {
  Node* number = Parameter(1);

  Label return_true(this), return_false(this);

  // Check if {number} is a Smi.
  GotoIf(TaggedIsSmi(number), &return_true);

  // Check if {number} is a HeapNumber.
  GotoUnless(IsHeapNumberMap(LoadMap(number)), &return_false);

  // Load the actual value of {number}.
  Node* number_value = LoadHeapNumberValue(number);

  // Truncate the value of {number} to an integer (or an infinity).
  Node* integer = Float64Trunc(number_value);

  // Check if {number}s value matches the integer (ruling out the infinities).
  Branch(Float64Equal(Float64Sub(number_value, integer), Float64Constant(0.0)),
         &return_true, &return_false);

  Bind(&return_true);
  Return(BooleanConstant(true));

  Bind(&return_false);
  Return(BooleanConstant(false));
}

// ES6 section 20.1.2.4 Number.isNaN ( number )
TF_BUILTIN(NumberIsNaN, CodeStubAssembler) {
  Node* number = Parameter(1);

  Label return_true(this), return_false(this);

  // Check if {number} is a Smi.
  GotoIf(TaggedIsSmi(number), &return_false);

  // Check if {number} is a HeapNumber.
  GotoUnless(IsHeapNumberMap(LoadMap(number)), &return_false);

  // Check if {number} contains a NaN value.
  Node* number_value = LoadHeapNumberValue(number);
  BranchIfFloat64IsNaN(number_value, &return_true, &return_false);

  Bind(&return_true);
  Return(BooleanConstant(true));

  Bind(&return_false);
  Return(BooleanConstant(false));
}

// ES6 section 20.1.2.5 Number.isSafeInteger ( number )
TF_BUILTIN(NumberIsSafeInteger, CodeStubAssembler) {
  Node* number = Parameter(1);

  Label return_true(this), return_false(this);

  // Check if {number} is a Smi.
  GotoIf(TaggedIsSmi(number), &return_true);

  // Check if {number} is a HeapNumber.
  GotoUnless(IsHeapNumberMap(LoadMap(number)), &return_false);

  // Load the actual value of {number}.
  Node* number_value = LoadHeapNumberValue(number);

  // Truncate the value of {number} to an integer (or an infinity).
  Node* integer = Float64Trunc(number_value);

  // Check if {number}s value matches the integer (ruling out the infinities).
  GotoUnless(
      Float64Equal(Float64Sub(number_value, integer), Float64Constant(0.0)),
      &return_false);

  // Check if the {integer} value is in safe integer range.
  Branch(Float64LessThanOrEqual(Float64Abs(integer),
                                Float64Constant(kMaxSafeInteger)),
         &return_true, &return_false);

  Bind(&return_true);
  Return(BooleanConstant(true));

  Bind(&return_false);
  Return(BooleanConstant(false));
}

// ES6 section 20.1.2.12 Number.parseFloat ( string )
TF_BUILTIN(NumberParseFloat, CodeStubAssembler) {
  Node* context = Parameter(4);

  // We might need to loop once for ToString conversion.
  Variable var_input(this, MachineRepresentation::kTagged);
  Label loop(this, &var_input);
  var_input.Bind(Parameter(1));
  Goto(&loop);
  Bind(&loop);
  {
    // Load the current {input} value.
    Node* input = var_input.value();

    // Check if the {input} is a HeapObject or a Smi.
    Label if_inputissmi(this), if_inputisnotsmi(this);
    Branch(TaggedIsSmi(input), &if_inputissmi, &if_inputisnotsmi);

    Bind(&if_inputissmi);
    {
      // The {input} is already a Number, no need to do anything.
      Return(input);
    }

    Bind(&if_inputisnotsmi);
    {
      // The {input} is a HeapObject, check if it's already a String.
      Label if_inputisstring(this), if_inputisnotstring(this);
      Node* input_map = LoadMap(input);
      Node* input_instance_type = LoadMapInstanceType(input_map);
      Branch(IsStringInstanceType(input_instance_type), &if_inputisstring,
             &if_inputisnotstring);

      Bind(&if_inputisstring);
      {
        // The {input} is already a String, check if {input} contains
        // a cached array index.
        Label if_inputcached(this), if_inputnotcached(this);
        Node* input_hash = LoadNameHashField(input);
        Node* input_bit = Word32And(
            input_hash, Int32Constant(String::kContainsCachedArrayIndexMask));
        Branch(Word32Equal(input_bit, Int32Constant(0)), &if_inputcached,
               &if_inputnotcached);

        Bind(&if_inputcached);
        {
          // Just return the {input}s cached array index.
          Node* input_array_index =
              DecodeWordFromWord32<String::ArrayIndexValueBits>(input_hash);
          Return(SmiTag(input_array_index));
        }

        Bind(&if_inputnotcached);
        {
          // Need to fall back to the runtime to convert {input} to double.
          Return(CallRuntime(Runtime::kStringParseFloat, context, input));
        }
      }

      Bind(&if_inputisnotstring);
      {
        // The {input} is neither a String nor a Smi, check for HeapNumber.
        Label if_inputisnumber(this),
            if_inputisnotnumber(this, Label::kDeferred);
        Branch(IsHeapNumberMap(input_map), &if_inputisnumber,
               &if_inputisnotnumber);

        Bind(&if_inputisnumber);
        {
          // The {input} is already a Number, take care of -0.
          Label if_inputiszero(this), if_inputisnotzero(this);
          Node* input_value = LoadHeapNumberValue(input);
          Branch(Float64Equal(input_value, Float64Constant(0.0)),
                 &if_inputiszero, &if_inputisnotzero);

          Bind(&if_inputiszero);
          Return(SmiConstant(0));

          Bind(&if_inputisnotzero);
          Return(input);
        }

        Bind(&if_inputisnotnumber);
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

// ES6 section 20.1.2.13 Number.parseInt ( string, radix )
TF_BUILTIN(NumberParseInt, CodeStubAssembler) {
  Node* input = Parameter(1);
  Node* radix = Parameter(2);
  Node* context = Parameter(5);

  // Check if {radix} is treated as 10 (i.e. undefined, 0 or 10).
  Label if_radix10(this), if_generic(this, Label::kDeferred);
  GotoIf(WordEqual(radix, UndefinedConstant()), &if_radix10);
  GotoIf(WordEqual(radix, SmiConstant(Smi::FromInt(10))), &if_radix10);
  GotoIf(WordEqual(radix, SmiConstant(Smi::FromInt(0))), &if_radix10);
  Goto(&if_generic);

  Bind(&if_radix10);
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

    Bind(&if_inputissmi);
    {
      // Just return the {input}.
      Return(input);
    }

    Bind(&if_inputisheapnumber);
    {
      // Check if the {input} value is in Signed32 range.
      Label if_inputissigned32(this);
      Node* input_value = LoadHeapNumberValue(input);
      Node* input_value32 = TruncateFloat64ToWord32(input_value);
      GotoIf(Float64Equal(input_value, ChangeInt32ToFloat64(input_value32)),
             &if_inputissigned32);

      // Check if the absolute {input} value is in the ]0.01,1e9[ range.
      Node* input_value_abs = Float64Abs(input_value);

      GotoUnless(Float64LessThan(input_value_abs, Float64Constant(1e9)),
                 &if_generic);
      Branch(Float64LessThan(Float64Constant(0.01), input_value_abs),
             &if_inputissigned32, &if_generic);

      // Return the truncated int32 value, and return the tagged result.
      Bind(&if_inputissigned32);
      Node* result = ChangeInt32ToTagged(input_value32);
      Return(result);
    }

    Bind(&if_inputisstring);
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

  Bind(&if_generic);
  {
    Node* result = CallRuntime(Runtime::kStringParseInt, context, input, radix);
    Return(result);
  }
}

// ES6 section 20.1.3.2 Number.prototype.toExponential ( fractionDigits )
BUILTIN(NumberPrototypeToExponential) {
  HandleScope scope(isolate);
  Handle<Object> value = args.at(0);
  Handle<Object> fraction_digits = args.atOrUndefined(isolate, 1);

  // Unwrap the receiver {value}.
  if (value->IsJSValue()) {
    value = handle(Handle<JSValue>::cast(value)->value(), isolate);
  }
  if (!value->IsNumber()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kNotGeneric,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "Number.prototype.toExponential")));
  }
  double const value_number = value->Number();

  // Convert the {fraction_digits} to an integer first.
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, fraction_digits, Object::ToInteger(isolate, fraction_digits));
  double const fraction_digits_number = fraction_digits->Number();

  if (std::isnan(value_number)) return isolate->heap()->nan_string();
  if (std::isinf(value_number)) {
    return (value_number < 0.0) ? isolate->heap()->minus_infinity_string()
                                : isolate->heap()->infinity_string();
  }
  if (fraction_digits_number < 0.0 || fraction_digits_number > 20.0) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kNumberFormatRange,
                               isolate->factory()->NewStringFromAsciiChecked(
                                   "toExponential()")));
  }
  int const f = args.atOrUndefined(isolate, 1)->IsUndefined(isolate)
                    ? -1
                    : static_cast<int>(fraction_digits_number);
  char* const str = DoubleToExponentialCString(value_number, f);
  Handle<String> result = isolate->factory()->NewStringFromAsciiChecked(str);
  DeleteArray(str);
  return *result;
}

// ES6 section 20.1.3.3 Number.prototype.toFixed ( fractionDigits )
BUILTIN(NumberPrototypeToFixed) {
  HandleScope scope(isolate);
  Handle<Object> value = args.at(0);
  Handle<Object> fraction_digits = args.atOrUndefined(isolate, 1);

  // Unwrap the receiver {value}.
  if (value->IsJSValue()) {
    value = handle(Handle<JSValue>::cast(value)->value(), isolate);
  }
  if (!value->IsNumber()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kNotGeneric,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "Number.prototype.toFixed")));
  }
  double const value_number = value->Number();

  // Convert the {fraction_digits} to an integer first.
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, fraction_digits, Object::ToInteger(isolate, fraction_digits));
  double const fraction_digits_number = fraction_digits->Number();

  // Check if the {fraction_digits} are in the supported range.
  if (fraction_digits_number < 0.0 || fraction_digits_number > 20.0) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kNumberFormatRange,
                               isolate->factory()->NewStringFromAsciiChecked(
                                   "toFixed() digits")));
  }

  if (std::isnan(value_number)) return isolate->heap()->nan_string();
  if (std::isinf(value_number)) {
    return (value_number < 0.0) ? isolate->heap()->minus_infinity_string()
                                : isolate->heap()->infinity_string();
  }
  char* const str = DoubleToFixedCString(
      value_number, static_cast<int>(fraction_digits_number));
  Handle<String> result = isolate->factory()->NewStringFromAsciiChecked(str);
  DeleteArray(str);
  return *result;
}

// ES6 section 20.1.3.4 Number.prototype.toLocaleString ( [ r1 [ , r2 ] ] )
BUILTIN(NumberPrototypeToLocaleString) {
  HandleScope scope(isolate);
  Handle<Object> value = args.at(0);

  // Unwrap the receiver {value}.
  if (value->IsJSValue()) {
    value = handle(Handle<JSValue>::cast(value)->value(), isolate);
  }
  if (!value->IsNumber()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kNotGeneric,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "Number.prototype.toLocaleString")));
  }

  // Turn the {value} into a String.
  return *isolate->factory()->NumberToString(value);
}

// ES6 section 20.1.3.5 Number.prototype.toPrecision ( precision )
BUILTIN(NumberPrototypeToPrecision) {
  HandleScope scope(isolate);
  Handle<Object> value = args.at(0);
  Handle<Object> precision = args.atOrUndefined(isolate, 1);

  // Unwrap the receiver {value}.
  if (value->IsJSValue()) {
    value = handle(Handle<JSValue>::cast(value)->value(), isolate);
  }
  if (!value->IsNumber()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kNotGeneric,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "Number.prototype.toPrecision")));
  }
  double const value_number = value->Number();

  // If no {precision} was specified, just return ToString of {value}.
  if (precision->IsUndefined(isolate)) {
    return *isolate->factory()->NumberToString(value);
  }

  // Convert the {precision} to an integer first.
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, precision,
                                     Object::ToInteger(isolate, precision));
  double const precision_number = precision->Number();

  if (std::isnan(value_number)) return isolate->heap()->nan_string();
  if (std::isinf(value_number)) {
    return (value_number < 0.0) ? isolate->heap()->minus_infinity_string()
                                : isolate->heap()->infinity_string();
  }
  if (precision_number < 1.0 || precision_number > 21.0) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kToPrecisionFormatRange));
  }
  char* const str = DoubleToPrecisionCString(
      value_number, static_cast<int>(precision_number));
  Handle<String> result = isolate->factory()->NewStringFromAsciiChecked(str);
  DeleteArray(str);
  return *result;
}

// ES6 section 20.1.3.6 Number.prototype.toString ( [ radix ] )
BUILTIN(NumberPrototypeToString) {
  HandleScope scope(isolate);
  Handle<Object> value = args.at(0);
  Handle<Object> radix = args.atOrUndefined(isolate, 1);

  // Unwrap the receiver {value}.
  if (value->IsJSValue()) {
    value = handle(Handle<JSValue>::cast(value)->value(), isolate);
  }
  if (!value->IsNumber()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kNotGeneric,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "Number.prototype.toString")));
  }
  double const value_number = value->Number();

  // If no {radix} was specified, just return ToString of {value}.
  if (radix->IsUndefined(isolate)) {
    return *isolate->factory()->NumberToString(value);
  }

  // Convert the {radix} to an integer first.
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, radix,
                                     Object::ToInteger(isolate, radix));
  double const radix_number = radix->Number();

  // If {radix} is 10, just return ToString of {value}.
  if (radix_number == 10.0) return *isolate->factory()->NumberToString(value);

  // Make sure the {radix} is within the valid range.
  if (radix_number < 2.0 || radix_number > 36.0) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kToRadixFormatRange));
  }

  // Fast case where the result is a one character string.
  if ((IsUint32Double(value_number) && value_number < radix_number) ||
      value_number == -0.0) {
    // Character array used for conversion.
    static const char kCharTable[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    return *isolate->factory()->LookupSingleCharacterStringFromCode(
        kCharTable[static_cast<uint32_t>(value_number)]);
  }

  // Slow case.
  if (std::isnan(value_number)) return isolate->heap()->nan_string();
  if (std::isinf(value_number)) {
    return (value_number < 0.0) ? isolate->heap()->minus_infinity_string()
                                : isolate->heap()->infinity_string();
  }
  char* const str =
      DoubleToRadixCString(value_number, static_cast<int>(radix_number));
  Handle<String> result = isolate->factory()->NewStringFromAsciiChecked(str);
  DeleteArray(str);
  return *result;
}

// ES6 section 20.1.3.7 Number.prototype.valueOf ( )
TF_BUILTIN(NumberPrototypeValueOf, CodeStubAssembler) {
  Node* receiver = Parameter(0);
  Node* context = Parameter(3);

  Node* result = ToThisValue(context, receiver, PrimitiveType::kNumber,
                             "Number.prototype.valueOf");
  Return(result);
}

TF_BUILTIN(Add, CodeStubAssembler) {
  Node* left = Parameter(0);
  Node* right = Parameter(1);
  Node* context = Parameter(2);

  // Shared entry for floating point addition.
  Label do_fadd(this);
  Variable var_fadd_lhs(this, MachineRepresentation::kFloat64),
      var_fadd_rhs(this, MachineRepresentation::kFloat64);

  // We might need to loop several times due to ToPrimitive, ToString and/or
  // ToNumber conversions.
  Variable var_lhs(this, MachineRepresentation::kTagged),
      var_rhs(this, MachineRepresentation::kTagged),
      var_result(this, MachineRepresentation::kTagged);
  Variable* loop_vars[2] = {&var_lhs, &var_rhs};
  Label loop(this, 2, loop_vars), end(this),
      string_add_convert_left(this, Label::kDeferred),
      string_add_convert_right(this, Label::kDeferred);
  var_lhs.Bind(left);
  var_rhs.Bind(right);
  Goto(&loop);
  Bind(&loop);
  {
    // Load the current {lhs} and {rhs} values.
    Node* lhs = var_lhs.value();
    Node* rhs = var_rhs.value();

    // Check if the {lhs} is a Smi or a HeapObject.
    Label if_lhsissmi(this), if_lhsisnotsmi(this);
    Branch(TaggedIsSmi(lhs), &if_lhsissmi, &if_lhsisnotsmi);

    Bind(&if_lhsissmi);
    {
      // Check if the {rhs} is also a Smi.
      Label if_rhsissmi(this), if_rhsisnotsmi(this);
      Branch(TaggedIsSmi(rhs), &if_rhsissmi, &if_rhsisnotsmi);

      Bind(&if_rhsissmi);
      {
        // Try fast Smi addition first.
        Node* pair = IntPtrAddWithOverflow(BitcastTaggedToWord(lhs),
                                           BitcastTaggedToWord(rhs));
        Node* overflow = Projection(1, pair);

        // Check if the Smi additon overflowed.
        Label if_overflow(this), if_notoverflow(this);
        Branch(overflow, &if_overflow, &if_notoverflow);

        Bind(&if_overflow);
        {
          var_fadd_lhs.Bind(SmiToFloat64(lhs));
          var_fadd_rhs.Bind(SmiToFloat64(rhs));
          Goto(&do_fadd);
        }

        Bind(&if_notoverflow);
        var_result.Bind(BitcastWordToTaggedSigned(Projection(0, pair)));
        Goto(&end);
      }

      Bind(&if_rhsisnotsmi);
      {
        // Load the map of {rhs}.
        Node* rhs_map = LoadMap(rhs);

        // Check if the {rhs} is a HeapNumber.
        Label if_rhsisnumber(this), if_rhsisnotnumber(this, Label::kDeferred);
        Branch(IsHeapNumberMap(rhs_map), &if_rhsisnumber, &if_rhsisnotnumber);

        Bind(&if_rhsisnumber);
        {
          var_fadd_lhs.Bind(SmiToFloat64(lhs));
          var_fadd_rhs.Bind(LoadHeapNumberValue(rhs));
          Goto(&do_fadd);
        }

        Bind(&if_rhsisnotnumber);
        {
          // Load the instance type of {rhs}.
          Node* rhs_instance_type = LoadMapInstanceType(rhs_map);

          // Check if the {rhs} is a String.
          Label if_rhsisstring(this, Label::kDeferred),
              if_rhsisnotstring(this, Label::kDeferred);
          Branch(IsStringInstanceType(rhs_instance_type), &if_rhsisstring,
                 &if_rhsisnotstring);

          Bind(&if_rhsisstring);
          {
            var_lhs.Bind(lhs);
            var_rhs.Bind(rhs);
            Goto(&string_add_convert_left);
          }

          Bind(&if_rhsisnotstring);
          {
            // Check if {rhs} is a JSReceiver.
            Label if_rhsisreceiver(this, Label::kDeferred),
                if_rhsisnotreceiver(this, Label::kDeferred);
            Branch(IsJSReceiverInstanceType(rhs_instance_type),
                   &if_rhsisreceiver, &if_rhsisnotreceiver);

            Bind(&if_rhsisreceiver);
            {
              // Convert {rhs} to a primitive first passing no hint.
              Callable callable =
                  CodeFactory::NonPrimitiveToPrimitive(isolate());
              var_rhs.Bind(CallStub(callable, context, rhs));
              Goto(&loop);
            }

            Bind(&if_rhsisnotreceiver);
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

    Bind(&if_lhsisnotsmi);
    {
      // Load the map and instance type of {lhs}.
      Node* lhs_instance_type = LoadInstanceType(lhs);

      // Check if {lhs} is a String.
      Label if_lhsisstring(this), if_lhsisnotstring(this);
      Branch(IsStringInstanceType(lhs_instance_type), &if_lhsisstring,
             &if_lhsisnotstring);

      Bind(&if_lhsisstring);
      {
        var_lhs.Bind(lhs);
        var_rhs.Bind(rhs);
        Goto(&string_add_convert_right);
      }

      Bind(&if_lhsisnotstring);
      {
        // Check if {rhs} is a Smi.
        Label if_rhsissmi(this), if_rhsisnotsmi(this);
        Branch(TaggedIsSmi(rhs), &if_rhsissmi, &if_rhsisnotsmi);

        Bind(&if_rhsissmi);
        {
          // Check if {lhs} is a Number.
          Label if_lhsisnumber(this), if_lhsisnotnumber(this, Label::kDeferred);
          Branch(
              Word32Equal(lhs_instance_type, Int32Constant(HEAP_NUMBER_TYPE)),
              &if_lhsisnumber, &if_lhsisnotnumber);

          Bind(&if_lhsisnumber);
          {
            // The {lhs} is a HeapNumber, the {rhs} is a Smi, just add them.
            var_fadd_lhs.Bind(LoadHeapNumberValue(lhs));
            var_fadd_rhs.Bind(SmiToFloat64(rhs));
            Goto(&do_fadd);
          }

          Bind(&if_lhsisnotnumber);
          {
            // The {lhs} is neither a Number nor a String, and the {rhs} is a
            // Smi.
            Label if_lhsisreceiver(this, Label::kDeferred),
                if_lhsisnotreceiver(this, Label::kDeferred);
            Branch(IsJSReceiverInstanceType(lhs_instance_type),
                   &if_lhsisreceiver, &if_lhsisnotreceiver);

            Bind(&if_lhsisreceiver);
            {
              // Convert {lhs} to a primitive first passing no hint.
              Callable callable =
                  CodeFactory::NonPrimitiveToPrimitive(isolate());
              var_lhs.Bind(CallStub(callable, context, lhs));
              Goto(&loop);
            }

            Bind(&if_lhsisnotreceiver);
            {
              // Convert {lhs} to a Number first.
              Callable callable = CodeFactory::NonNumberToNumber(isolate());
              var_lhs.Bind(CallStub(callable, context, lhs));
              Goto(&loop);
            }
          }
        }

        Bind(&if_rhsisnotsmi);
        {
          // Load the instance type of {rhs}.
          Node* rhs_instance_type = LoadInstanceType(rhs);

          // Check if {rhs} is a String.
          Label if_rhsisstring(this), if_rhsisnotstring(this);
          Branch(IsStringInstanceType(rhs_instance_type), &if_rhsisstring,
                 &if_rhsisnotstring);

          Bind(&if_rhsisstring);
          {
            var_lhs.Bind(lhs);
            var_rhs.Bind(rhs);
            Goto(&string_add_convert_left);
          }

          Bind(&if_rhsisnotstring);
          {
            // Check if {lhs} is a HeapNumber.
            Label if_lhsisnumber(this), if_lhsisnotnumber(this);
            Branch(
                Word32Equal(lhs_instance_type, Int32Constant(HEAP_NUMBER_TYPE)),
                &if_lhsisnumber, &if_lhsisnotnumber);

            Bind(&if_lhsisnumber);
            {
              // Check if {rhs} is also a HeapNumber.
              Label if_rhsisnumber(this),
                  if_rhsisnotnumber(this, Label::kDeferred);
              Branch(Word32Equal(rhs_instance_type,
                                 Int32Constant(HEAP_NUMBER_TYPE)),
                     &if_rhsisnumber, &if_rhsisnotnumber);

              Bind(&if_rhsisnumber);
              {
                // Perform a floating point addition.
                var_fadd_lhs.Bind(LoadHeapNumberValue(lhs));
                var_fadd_rhs.Bind(LoadHeapNumberValue(rhs));
                Goto(&do_fadd);
              }

              Bind(&if_rhsisnotnumber);
              {
                // Check if {rhs} is a JSReceiver.
                Label if_rhsisreceiver(this, Label::kDeferred),
                    if_rhsisnotreceiver(this, Label::kDeferred);
                Branch(IsJSReceiverInstanceType(rhs_instance_type),
                       &if_rhsisreceiver, &if_rhsisnotreceiver);

                Bind(&if_rhsisreceiver);
                {
                  // Convert {rhs} to a primitive first passing no hint.
                  Callable callable =
                      CodeFactory::NonPrimitiveToPrimitive(isolate());
                  var_rhs.Bind(CallStub(callable, context, rhs));
                  Goto(&loop);
                }

                Bind(&if_rhsisnotreceiver);
                {
                  // Convert {rhs} to a Number first.
                  Callable callable = CodeFactory::NonNumberToNumber(isolate());
                  var_rhs.Bind(CallStub(callable, context, rhs));
                  Goto(&loop);
                }
              }
            }

            Bind(&if_lhsisnotnumber);
            {
              // Check if {lhs} is a JSReceiver.
              Label if_lhsisreceiver(this, Label::kDeferred),
                  if_lhsisnotreceiver(this);
              Branch(IsJSReceiverInstanceType(lhs_instance_type),
                     &if_lhsisreceiver, &if_lhsisnotreceiver);

              Bind(&if_lhsisreceiver);
              {
                // Convert {lhs} to a primitive first passing no hint.
                Callable callable =
                    CodeFactory::NonPrimitiveToPrimitive(isolate());
                var_lhs.Bind(CallStub(callable, context, lhs));
                Goto(&loop);
              }

              Bind(&if_lhsisnotreceiver);
              {
                // Check if {rhs} is a JSReceiver.
                Label if_rhsisreceiver(this, Label::kDeferred),
                    if_rhsisnotreceiver(this, Label::kDeferred);
                Branch(IsJSReceiverInstanceType(rhs_instance_type),
                       &if_rhsisreceiver, &if_rhsisnotreceiver);

                Bind(&if_rhsisreceiver);
                {
                  // Convert {rhs} to a primitive first passing no hint.
                  Callable callable =
                      CodeFactory::NonPrimitiveToPrimitive(isolate());
                  var_rhs.Bind(CallStub(callable, context, rhs));
                  Goto(&loop);
                }

                Bind(&if_rhsisnotreceiver);
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
  Bind(&string_add_convert_left);
  {
    // Convert {lhs}, which is a Smi, to a String and concatenate the
    // resulting string with the String {rhs}.
    Callable callable =
        CodeFactory::StringAdd(isolate(), STRING_ADD_CONVERT_LEFT, NOT_TENURED);
    var_result.Bind(
        CallStub(callable, context, var_lhs.value(), var_rhs.value()));
    Goto(&end);
  }

  Bind(&string_add_convert_right);
  {
    // Convert {lhs}, which is a Smi, to a String and concatenate the
    // resulting string with the String {rhs}.
    Callable callable = CodeFactory::StringAdd(
        isolate(), STRING_ADD_CONVERT_RIGHT, NOT_TENURED);
    var_result.Bind(
        CallStub(callable, context, var_lhs.value(), var_rhs.value()));
    Goto(&end);
  }

  Bind(&do_fadd);
  {
    Node* lhs_value = var_fadd_lhs.value();
    Node* rhs_value = var_fadd_rhs.value();
    Node* value = Float64Add(lhs_value, rhs_value);
    Node* result = AllocateHeapNumberWithValue(value);
    var_result.Bind(result);
    Goto(&end);
  }
  Bind(&end);
  Return(var_result.value());
}

TF_BUILTIN(Subtract, CodeStubAssembler) {
  Node* left = Parameter(0);
  Node* right = Parameter(1);
  Node* context = Parameter(2);

  // Shared entry for floating point subtraction.
  Label do_fsub(this), end(this);
  Variable var_fsub_lhs(this, MachineRepresentation::kFloat64),
      var_fsub_rhs(this, MachineRepresentation::kFloat64);

  // We might need to loop several times due to ToPrimitive and/or ToNumber
  // conversions.
  Variable var_lhs(this, MachineRepresentation::kTagged),
      var_rhs(this, MachineRepresentation::kTagged),
      var_result(this, MachineRepresentation::kTagged);
  Variable* loop_vars[2] = {&var_lhs, &var_rhs};
  Label loop(this, 2, loop_vars);
  var_lhs.Bind(left);
  var_rhs.Bind(right);
  Goto(&loop);
  Bind(&loop);
  {
    // Load the current {lhs} and {rhs} values.
    Node* lhs = var_lhs.value();
    Node* rhs = var_rhs.value();

    // Check if the {lhs} is a Smi or a HeapObject.
    Label if_lhsissmi(this), if_lhsisnotsmi(this);
    Branch(TaggedIsSmi(lhs), &if_lhsissmi, &if_lhsisnotsmi);

    Bind(&if_lhsissmi);
    {
      // Check if the {rhs} is also a Smi.
      Label if_rhsissmi(this), if_rhsisnotsmi(this);
      Branch(TaggedIsSmi(rhs), &if_rhsissmi, &if_rhsisnotsmi);

      Bind(&if_rhsissmi);
      {
        // Try a fast Smi subtraction first.
        Node* pair = IntPtrSubWithOverflow(BitcastTaggedToWord(lhs),
                                           BitcastTaggedToWord(rhs));
        Node* overflow = Projection(1, pair);

        // Check if the Smi subtraction overflowed.
        Label if_overflow(this), if_notoverflow(this);
        Branch(overflow, &if_overflow, &if_notoverflow);

        Bind(&if_overflow);
        {
          // The result doesn't fit into Smi range.
          var_fsub_lhs.Bind(SmiToFloat64(lhs));
          var_fsub_rhs.Bind(SmiToFloat64(rhs));
          Goto(&do_fsub);
        }

        Bind(&if_notoverflow);
        var_result.Bind(BitcastWordToTaggedSigned(Projection(0, pair)));
        Goto(&end);
      }

      Bind(&if_rhsisnotsmi);
      {
        // Load the map of the {rhs}.
        Node* rhs_map = LoadMap(rhs);

        // Check if {rhs} is a HeapNumber.
        Label if_rhsisnumber(this), if_rhsisnotnumber(this, Label::kDeferred);
        Branch(IsHeapNumberMap(rhs_map), &if_rhsisnumber, &if_rhsisnotnumber);

        Bind(&if_rhsisnumber);
        {
          // Perform a floating point subtraction.
          var_fsub_lhs.Bind(SmiToFloat64(lhs));
          var_fsub_rhs.Bind(LoadHeapNumberValue(rhs));
          Goto(&do_fsub);
        }

        Bind(&if_rhsisnotnumber);
        {
          // Convert the {rhs} to a Number first.
          Callable callable = CodeFactory::NonNumberToNumber(isolate());
          var_rhs.Bind(CallStub(callable, context, rhs));
          Goto(&loop);
        }
      }
    }

    Bind(&if_lhsisnotsmi);
    {
      // Load the map of the {lhs}.
      Node* lhs_map = LoadMap(lhs);

      // Check if the {lhs} is a HeapNumber.
      Label if_lhsisnumber(this), if_lhsisnotnumber(this, Label::kDeferred);
      Branch(IsHeapNumberMap(lhs_map), &if_lhsisnumber, &if_lhsisnotnumber);

      Bind(&if_lhsisnumber);
      {
        // Check if the {rhs} is a Smi.
        Label if_rhsissmi(this), if_rhsisnotsmi(this);
        Branch(TaggedIsSmi(rhs), &if_rhsissmi, &if_rhsisnotsmi);

        Bind(&if_rhsissmi);
        {
          // Perform a floating point subtraction.
          var_fsub_lhs.Bind(LoadHeapNumberValue(lhs));
          var_fsub_rhs.Bind(SmiToFloat64(rhs));
          Goto(&do_fsub);
        }

        Bind(&if_rhsisnotsmi);
        {
          // Load the map of the {rhs}.
          Node* rhs_map = LoadMap(rhs);

          // Check if the {rhs} is a HeapNumber.
          Label if_rhsisnumber(this), if_rhsisnotnumber(this, Label::kDeferred);
          Branch(IsHeapNumberMap(rhs_map), &if_rhsisnumber, &if_rhsisnotnumber);

          Bind(&if_rhsisnumber);
          {
            // Perform a floating point subtraction.
            var_fsub_lhs.Bind(LoadHeapNumberValue(lhs));
            var_fsub_rhs.Bind(LoadHeapNumberValue(rhs));
            Goto(&do_fsub);
          }

          Bind(&if_rhsisnotnumber);
          {
            // Convert the {rhs} to a Number first.
            Callable callable = CodeFactory::NonNumberToNumber(isolate());
            var_rhs.Bind(CallStub(callable, context, rhs));
            Goto(&loop);
          }
        }
      }

      Bind(&if_lhsisnotnumber);
      {
        // Convert the {lhs} to a Number first.
        Callable callable = CodeFactory::NonNumberToNumber(isolate());
        var_lhs.Bind(CallStub(callable, context, lhs));
        Goto(&loop);
      }
    }
  }

  Bind(&do_fsub);
  {
    Node* lhs_value = var_fsub_lhs.value();
    Node* rhs_value = var_fsub_rhs.value();
    Node* value = Float64Sub(lhs_value, rhs_value);
    var_result.Bind(AllocateHeapNumberWithValue(value));
    Goto(&end);
  }
  Bind(&end);
  Return(var_result.value());
}

TF_BUILTIN(Multiply, CodeStubAssembler) {
  Node* left = Parameter(0);
  Node* right = Parameter(1);
  Node* context = Parameter(2);

  // Shared entry point for floating point multiplication.
  Label do_fmul(this), return_result(this);
  Variable var_lhs_float64(this, MachineRepresentation::kFloat64),
      var_rhs_float64(this, MachineRepresentation::kFloat64);

  // We might need to loop one or two times due to ToNumber conversions.
  Variable var_lhs(this, MachineRepresentation::kTagged),
      var_rhs(this, MachineRepresentation::kTagged),
      var_result(this, MachineRepresentation::kTagged);
  Variable* loop_variables[] = {&var_lhs, &var_rhs};
  Label loop(this, 2, loop_variables);
  var_lhs.Bind(left);
  var_rhs.Bind(right);
  Goto(&loop);
  Bind(&loop);
  {
    Node* lhs = var_lhs.value();
    Node* rhs = var_rhs.value();

    Label lhs_is_smi(this), lhs_is_not_smi(this);
    Branch(TaggedIsSmi(lhs), &lhs_is_smi, &lhs_is_not_smi);

    Bind(&lhs_is_smi);
    {
      Label rhs_is_smi(this), rhs_is_not_smi(this);
      Branch(TaggedIsSmi(rhs), &rhs_is_smi, &rhs_is_not_smi);

      Bind(&rhs_is_smi);
      {
        // Both {lhs} and {rhs} are Smis. The result is not necessarily a smi,
        // in case of overflow.
        var_result.Bind(SmiMul(lhs, rhs));
        Goto(&return_result);
      }

      Bind(&rhs_is_not_smi);
      {
        Node* rhs_map = LoadMap(rhs);

        // Check if {rhs} is a HeapNumber.
        Label rhs_is_number(this), rhs_is_not_number(this, Label::kDeferred);
        Branch(IsHeapNumberMap(rhs_map), &rhs_is_number, &rhs_is_not_number);

        Bind(&rhs_is_number);
        {
          // Convert {lhs} to a double and multiply it with the value of {rhs}.
          var_lhs_float64.Bind(SmiToFloat64(lhs));
          var_rhs_float64.Bind(LoadHeapNumberValue(rhs));
          Goto(&do_fmul);
        }

        Bind(&rhs_is_not_number);
        {
          // Multiplication is commutative, swap {lhs} with {rhs} and loop.
          var_lhs.Bind(rhs);
          var_rhs.Bind(lhs);
          Goto(&loop);
        }
      }
    }

    Bind(&lhs_is_not_smi);
    {
      Node* lhs_map = LoadMap(lhs);

      // Check if {lhs} is a HeapNumber.
      Label lhs_is_number(this), lhs_is_not_number(this, Label::kDeferred);
      Branch(IsHeapNumberMap(lhs_map), &lhs_is_number, &lhs_is_not_number);

      Bind(&lhs_is_number);
      {
        // Check if {rhs} is a Smi.
        Label rhs_is_smi(this), rhs_is_not_smi(this);
        Branch(TaggedIsSmi(rhs), &rhs_is_smi, &rhs_is_not_smi);

        Bind(&rhs_is_smi);
        {
          // Convert {rhs} to a double and multiply it with the value of {lhs}.
          var_lhs_float64.Bind(LoadHeapNumberValue(lhs));
          var_rhs_float64.Bind(SmiToFloat64(rhs));
          Goto(&do_fmul);
        }

        Bind(&rhs_is_not_smi);
        {
          Node* rhs_map = LoadMap(rhs);

          // Check if {rhs} is a HeapNumber.
          Label rhs_is_number(this), rhs_is_not_number(this, Label::kDeferred);
          Branch(IsHeapNumberMap(rhs_map), &rhs_is_number, &rhs_is_not_number);

          Bind(&rhs_is_number);
          {
            // Both {lhs} and {rhs} are HeapNumbers. Load their values and
            // multiply them.
            var_lhs_float64.Bind(LoadHeapNumberValue(lhs));
            var_rhs_float64.Bind(LoadHeapNumberValue(rhs));
            Goto(&do_fmul);
          }

          Bind(&rhs_is_not_number);
          {
            // Multiplication is commutative, swap {lhs} with {rhs} and loop.
            var_lhs.Bind(rhs);
            var_rhs.Bind(lhs);
            Goto(&loop);
          }
        }
      }

      Bind(&lhs_is_not_number);
      {
        // Convert {lhs} to a Number and loop.
        Callable callable = CodeFactory::NonNumberToNumber(isolate());
        var_lhs.Bind(CallStub(callable, context, lhs));
        Goto(&loop);
      }
    }
  }

  Bind(&do_fmul);
  {
    Node* value = Float64Mul(var_lhs_float64.value(), var_rhs_float64.value());
    Node* result = AllocateHeapNumberWithValue(value);
    var_result.Bind(result);
    Goto(&return_result);
  }

  Bind(&return_result);
  Return(var_result.value());
}

TF_BUILTIN(Divide, CodeStubAssembler) {
  Node* left = Parameter(0);
  Node* right = Parameter(1);
  Node* context = Parameter(2);

  // Shared entry point for floating point division.
  Label do_fdiv(this), end(this);
  Variable var_dividend_float64(this, MachineRepresentation::kFloat64),
      var_divisor_float64(this, MachineRepresentation::kFloat64);

  // We might need to loop one or two times due to ToNumber conversions.
  Variable var_dividend(this, MachineRepresentation::kTagged),
      var_divisor(this, MachineRepresentation::kTagged),
      var_result(this, MachineRepresentation::kTagged);
  Variable* loop_variables[] = {&var_dividend, &var_divisor};
  Label loop(this, 2, loop_variables);
  var_dividend.Bind(left);
  var_divisor.Bind(right);
  Goto(&loop);
  Bind(&loop);
  {
    Node* dividend = var_dividend.value();
    Node* divisor = var_divisor.value();

    Label dividend_is_smi(this), dividend_is_not_smi(this);
    Branch(TaggedIsSmi(dividend), &dividend_is_smi, &dividend_is_not_smi);

    Bind(&dividend_is_smi);
    {
      Label divisor_is_smi(this), divisor_is_not_smi(this);
      Branch(TaggedIsSmi(divisor), &divisor_is_smi, &divisor_is_not_smi);

      Bind(&divisor_is_smi);
      {
        Label bailout(this);

        // Do floating point division if {divisor} is zero.
        GotoIf(SmiEqual(divisor, SmiConstant(0)), &bailout);

        // Do floating point division {dividend} is zero and {divisor} is
        // negative.
        Label dividend_is_zero(this), dividend_is_not_zero(this);
        Branch(SmiEqual(dividend, SmiConstant(0)), &dividend_is_zero,
               &dividend_is_not_zero);

        Bind(&dividend_is_zero);
        {
          GotoIf(SmiLessThan(divisor, SmiConstant(0)), &bailout);
          Goto(&dividend_is_not_zero);
        }
        Bind(&dividend_is_not_zero);

        Node* untagged_divisor = SmiToWord32(divisor);
        Node* untagged_dividend = SmiToWord32(dividend);

        // Do floating point division if {dividend} is kMinInt (or kMinInt - 1
        // if the Smi size is 31) and {divisor} is -1.
        Label divisor_is_minus_one(this), divisor_is_not_minus_one(this);
        Branch(Word32Equal(untagged_divisor, Int32Constant(-1)),
               &divisor_is_minus_one, &divisor_is_not_minus_one);

        Bind(&divisor_is_minus_one);
        {
          GotoIf(
              Word32Equal(untagged_dividend,
                          Int32Constant(kSmiValueSize == 32 ? kMinInt
                                                            : (kMinInt >> 1))),
              &bailout);
          Goto(&divisor_is_not_minus_one);
        }
        Bind(&divisor_is_not_minus_one);

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
        Bind(&bailout);
        {
          var_dividend_float64.Bind(SmiToFloat64(dividend));
          var_divisor_float64.Bind(SmiToFloat64(divisor));
          Goto(&do_fdiv);
        }
      }

      Bind(&divisor_is_not_smi);
      {
        Node* divisor_map = LoadMap(divisor);

        // Check if {divisor} is a HeapNumber.
        Label divisor_is_number(this),
            divisor_is_not_number(this, Label::kDeferred);
        Branch(IsHeapNumberMap(divisor_map), &divisor_is_number,
               &divisor_is_not_number);

        Bind(&divisor_is_number);
        {
          // Convert {dividend} to a double and divide it with the value of
          // {divisor}.
          var_dividend_float64.Bind(SmiToFloat64(dividend));
          var_divisor_float64.Bind(LoadHeapNumberValue(divisor));
          Goto(&do_fdiv);
        }

        Bind(&divisor_is_not_number);
        {
          // Convert {divisor} to a number and loop.
          Callable callable = CodeFactory::NonNumberToNumber(isolate());
          var_divisor.Bind(CallStub(callable, context, divisor));
          Goto(&loop);
        }
      }
    }

    Bind(&dividend_is_not_smi);
    {
      Node* dividend_map = LoadMap(dividend);

      // Check if {dividend} is a HeapNumber.
      Label dividend_is_number(this),
          dividend_is_not_number(this, Label::kDeferred);
      Branch(IsHeapNumberMap(dividend_map), &dividend_is_number,
             &dividend_is_not_number);

      Bind(&dividend_is_number);
      {
        // Check if {divisor} is a Smi.
        Label divisor_is_smi(this), divisor_is_not_smi(this);
        Branch(TaggedIsSmi(divisor), &divisor_is_smi, &divisor_is_not_smi);

        Bind(&divisor_is_smi);
        {
          // Convert {divisor} to a double and use it for a floating point
          // division.
          var_dividend_float64.Bind(LoadHeapNumberValue(dividend));
          var_divisor_float64.Bind(SmiToFloat64(divisor));
          Goto(&do_fdiv);
        }

        Bind(&divisor_is_not_smi);
        {
          Node* divisor_map = LoadMap(divisor);

          // Check if {divisor} is a HeapNumber.
          Label divisor_is_number(this),
              divisor_is_not_number(this, Label::kDeferred);
          Branch(IsHeapNumberMap(divisor_map), &divisor_is_number,
                 &divisor_is_not_number);

          Bind(&divisor_is_number);
          {
            // Both {dividend} and {divisor} are HeapNumbers. Load their values
            // and divide them.
            var_dividend_float64.Bind(LoadHeapNumberValue(dividend));
            var_divisor_float64.Bind(LoadHeapNumberValue(divisor));
            Goto(&do_fdiv);
          }

          Bind(&divisor_is_not_number);
          {
            // Convert {divisor} to a number and loop.
            Callable callable = CodeFactory::NonNumberToNumber(isolate());
            var_divisor.Bind(CallStub(callable, context, divisor));
            Goto(&loop);
          }
        }
      }

      Bind(&dividend_is_not_number);
      {
        // Convert {dividend} to a Number and loop.
        Callable callable = CodeFactory::NonNumberToNumber(isolate());
        var_dividend.Bind(CallStub(callable, context, dividend));
        Goto(&loop);
      }
    }
  }

  Bind(&do_fdiv);
  {
    Node* value =
        Float64Div(var_dividend_float64.value(), var_divisor_float64.value());
    var_result.Bind(AllocateHeapNumberWithValue(value));
    Goto(&end);
  }
  Bind(&end);
  Return(var_result.value());
}

TF_BUILTIN(Modulus, CodeStubAssembler) {
  Node* left = Parameter(0);
  Node* right = Parameter(1);
  Node* context = Parameter(2);

  Variable var_result(this, MachineRepresentation::kTagged);
  Label return_result(this, &var_result);

  // Shared entry point for floating point modulus.
  Label do_fmod(this);
  Variable var_dividend_float64(this, MachineRepresentation::kFloat64),
      var_divisor_float64(this, MachineRepresentation::kFloat64);

  // We might need to loop one or two times due to ToNumber conversions.
  Variable var_dividend(this, MachineRepresentation::kTagged),
      var_divisor(this, MachineRepresentation::kTagged);
  Variable* loop_variables[] = {&var_dividend, &var_divisor};
  Label loop(this, 2, loop_variables);
  var_dividend.Bind(left);
  var_divisor.Bind(right);
  Goto(&loop);
  Bind(&loop);
  {
    Node* dividend = var_dividend.value();
    Node* divisor = var_divisor.value();

    Label dividend_is_smi(this), dividend_is_not_smi(this);
    Branch(TaggedIsSmi(dividend), &dividend_is_smi, &dividend_is_not_smi);

    Bind(&dividend_is_smi);
    {
      Label dividend_is_not_zero(this);
      Label divisor_is_smi(this), divisor_is_not_smi(this);
      Branch(TaggedIsSmi(divisor), &divisor_is_smi, &divisor_is_not_smi);

      Bind(&divisor_is_smi);
      {
        // Compute the modulus of two Smis.
        var_result.Bind(SmiMod(dividend, divisor));
        Goto(&return_result);
      }

      Bind(&divisor_is_not_smi);
      {
        Node* divisor_map = LoadMap(divisor);

        // Check if {divisor} is a HeapNumber.
        Label divisor_is_number(this),
            divisor_is_not_number(this, Label::kDeferred);
        Branch(IsHeapNumberMap(divisor_map), &divisor_is_number,
               &divisor_is_not_number);

        Bind(&divisor_is_number);
        {
          // Convert {dividend} to a double and compute its modulus with the
          // value of {dividend}.
          var_dividend_float64.Bind(SmiToFloat64(dividend));
          var_divisor_float64.Bind(LoadHeapNumberValue(divisor));
          Goto(&do_fmod);
        }

        Bind(&divisor_is_not_number);
        {
          // Convert {divisor} to a number and loop.
          Callable callable = CodeFactory::NonNumberToNumber(isolate());
          var_divisor.Bind(CallStub(callable, context, divisor));
          Goto(&loop);
        }
      }
    }

    Bind(&dividend_is_not_smi);
    {
      Node* dividend_map = LoadMap(dividend);

      // Check if {dividend} is a HeapNumber.
      Label dividend_is_number(this),
          dividend_is_not_number(this, Label::kDeferred);
      Branch(IsHeapNumberMap(dividend_map), &dividend_is_number,
             &dividend_is_not_number);

      Bind(&dividend_is_number);
      {
        // Check if {divisor} is a Smi.
        Label divisor_is_smi(this), divisor_is_not_smi(this);
        Branch(TaggedIsSmi(divisor), &divisor_is_smi, &divisor_is_not_smi);

        Bind(&divisor_is_smi);
        {
          // Convert {divisor} to a double and compute {dividend}'s modulus with
          // it.
          var_dividend_float64.Bind(LoadHeapNumberValue(dividend));
          var_divisor_float64.Bind(SmiToFloat64(divisor));
          Goto(&do_fmod);
        }

        Bind(&divisor_is_not_smi);
        {
          Node* divisor_map = LoadMap(divisor);

          // Check if {divisor} is a HeapNumber.
          Label divisor_is_number(this),
              divisor_is_not_number(this, Label::kDeferred);
          Branch(IsHeapNumberMap(divisor_map), &divisor_is_number,
                 &divisor_is_not_number);

          Bind(&divisor_is_number);
          {
            // Both {dividend} and {divisor} are HeapNumbers. Load their values
            // and compute their modulus.
            var_dividend_float64.Bind(LoadHeapNumberValue(dividend));
            var_divisor_float64.Bind(LoadHeapNumberValue(divisor));
            Goto(&do_fmod);
          }

          Bind(&divisor_is_not_number);
          {
            // Convert {divisor} to a number and loop.
            Callable callable = CodeFactory::NonNumberToNumber(isolate());
            var_divisor.Bind(CallStub(callable, context, divisor));
            Goto(&loop);
          }
        }
      }

      Bind(&dividend_is_not_number);
      {
        // Convert {dividend} to a Number and loop.
        Callable callable = CodeFactory::NonNumberToNumber(isolate());
        var_dividend.Bind(CallStub(callable, context, dividend));
        Goto(&loop);
      }
    }
  }

  Bind(&do_fmod);
  {
    Node* value =
        Float64Mod(var_dividend_float64.value(), var_divisor_float64.value());
    var_result.Bind(AllocateHeapNumberWithValue(value));
    Goto(&return_result);
  }

  Bind(&return_result);
  Return(var_result.value());
}

TF_BUILTIN(ShiftLeft, NumberBuiltinsAssembler) {
  BitwiseShiftOp([this](Node* lhs, Node* shift_count) {
    return Word32Shl(lhs, shift_count);
  });
}

TF_BUILTIN(ShiftRight, NumberBuiltinsAssembler) {
  BitwiseShiftOp([this](Node* lhs, Node* shift_count) {
    return Word32Sar(lhs, shift_count);
  });
}

TF_BUILTIN(ShiftRightLogical, NumberBuiltinsAssembler) {
  BitwiseShiftOp<kUnsigned>([this](Node* lhs, Node* shift_count) {
    return Word32Shr(lhs, shift_count);
  });
}

TF_BUILTIN(BitwiseAnd, NumberBuiltinsAssembler) {
  BitwiseOp([this](Node* lhs, Node* rhs) { return Word32And(lhs, rhs); });
}

TF_BUILTIN(BitwiseOr, NumberBuiltinsAssembler) {
  BitwiseOp([this](Node* lhs, Node* rhs) { return Word32Or(lhs, rhs); });
}

TF_BUILTIN(BitwiseXor, NumberBuiltinsAssembler) {
  BitwiseOp([this](Node* lhs, Node* rhs) { return Word32Xor(lhs, rhs); });
}

TF_BUILTIN(LessThan, NumberBuiltinsAssembler) {
  RelationalComparisonBuiltin(kLessThan);
}

TF_BUILTIN(LessThanOrEqual, NumberBuiltinsAssembler) {
  RelationalComparisonBuiltin(kLessThanOrEqual);
}

TF_BUILTIN(GreaterThan, NumberBuiltinsAssembler) {
  RelationalComparisonBuiltin(kGreaterThan);
}

TF_BUILTIN(GreaterThanOrEqual, NumberBuiltinsAssembler) {
  RelationalComparisonBuiltin(kGreaterThanOrEqual);
}

TF_BUILTIN(Equal, CodeStubAssembler) {
  Node* lhs = Parameter(0);
  Node* rhs = Parameter(1);
  Node* context = Parameter(2);

  Return(Equal(kDontNegateResult, lhs, rhs, context));
}

TF_BUILTIN(NotEqual, CodeStubAssembler) {
  Node* lhs = Parameter(0);
  Node* rhs = Parameter(1);
  Node* context = Parameter(2);

  Return(Equal(kNegateResult, lhs, rhs, context));
}

TF_BUILTIN(StrictEqual, CodeStubAssembler) {
  Node* lhs = Parameter(0);
  Node* rhs = Parameter(1);
  Node* context = Parameter(2);

  Return(StrictEqual(kDontNegateResult, lhs, rhs, context));
}

TF_BUILTIN(StrictNotEqual, CodeStubAssembler) {
  Node* lhs = Parameter(0);
  Node* rhs = Parameter(1);
  Node* context = Parameter(2);

  Return(StrictEqual(kNegateResult, lhs, rhs, context));
}

}  // namespace internal
}  // namespace v8
