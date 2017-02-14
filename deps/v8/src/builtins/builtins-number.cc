// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils.h"
#include "src/builtins/builtins.h"
#include "src/code-factory.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// ES6 section 20.1 Number Objects

// ES6 section 20.1.2.2 Number.isFinite ( number )
void Builtins::Generate_NumberIsFinite(CodeStubAssembler* assembler) {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;

  Node* number = assembler->Parameter(1);

  Label return_true(assembler), return_false(assembler);

  // Check if {number} is a Smi.
  assembler->GotoIf(assembler->TaggedIsSmi(number), &return_true);

  // Check if {number} is a HeapNumber.
  assembler->GotoUnless(
      assembler->WordEqual(assembler->LoadMap(number),
                           assembler->HeapNumberMapConstant()),
      &return_false);

  // Check if {number} contains a finite, non-NaN value.
  Node* number_value = assembler->LoadHeapNumberValue(number);
  assembler->BranchIfFloat64IsNaN(
      assembler->Float64Sub(number_value, number_value), &return_false,
      &return_true);

  assembler->Bind(&return_true);
  assembler->Return(assembler->BooleanConstant(true));

  assembler->Bind(&return_false);
  assembler->Return(assembler->BooleanConstant(false));
}

// ES6 section 20.1.2.3 Number.isInteger ( number )
void Builtins::Generate_NumberIsInteger(CodeStubAssembler* assembler) {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;

  Node* number = assembler->Parameter(1);

  Label return_true(assembler), return_false(assembler);

  // Check if {number} is a Smi.
  assembler->GotoIf(assembler->TaggedIsSmi(number), &return_true);

  // Check if {number} is a HeapNumber.
  assembler->GotoUnless(
      assembler->WordEqual(assembler->LoadMap(number),
                           assembler->HeapNumberMapConstant()),
      &return_false);

  // Load the actual value of {number}.
  Node* number_value = assembler->LoadHeapNumberValue(number);

  // Truncate the value of {number} to an integer (or an infinity).
  Node* integer = assembler->Float64Trunc(number_value);

  // Check if {number}s value matches the integer (ruling out the infinities).
  assembler->Branch(
      assembler->Float64Equal(assembler->Float64Sub(number_value, integer),
                              assembler->Float64Constant(0.0)),
      &return_true, &return_false);

  assembler->Bind(&return_true);
  assembler->Return(assembler->BooleanConstant(true));

  assembler->Bind(&return_false);
  assembler->Return(assembler->BooleanConstant(false));
}

// ES6 section 20.1.2.4 Number.isNaN ( number )
void Builtins::Generate_NumberIsNaN(CodeStubAssembler* assembler) {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;

  Node* number = assembler->Parameter(1);

  Label return_true(assembler), return_false(assembler);

  // Check if {number} is a Smi.
  assembler->GotoIf(assembler->TaggedIsSmi(number), &return_false);

  // Check if {number} is a HeapNumber.
  assembler->GotoUnless(
      assembler->WordEqual(assembler->LoadMap(number),
                           assembler->HeapNumberMapConstant()),
      &return_false);

  // Check if {number} contains a NaN value.
  Node* number_value = assembler->LoadHeapNumberValue(number);
  assembler->BranchIfFloat64IsNaN(number_value, &return_true, &return_false);

  assembler->Bind(&return_true);
  assembler->Return(assembler->BooleanConstant(true));

  assembler->Bind(&return_false);
  assembler->Return(assembler->BooleanConstant(false));
}

// ES6 section 20.1.2.5 Number.isSafeInteger ( number )
void Builtins::Generate_NumberIsSafeInteger(CodeStubAssembler* assembler) {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;

  Node* number = assembler->Parameter(1);

  Label return_true(assembler), return_false(assembler);

  // Check if {number} is a Smi.
  assembler->GotoIf(assembler->TaggedIsSmi(number), &return_true);

  // Check if {number} is a HeapNumber.
  assembler->GotoUnless(
      assembler->WordEqual(assembler->LoadMap(number),
                           assembler->HeapNumberMapConstant()),
      &return_false);

  // Load the actual value of {number}.
  Node* number_value = assembler->LoadHeapNumberValue(number);

  // Truncate the value of {number} to an integer (or an infinity).
  Node* integer = assembler->Float64Trunc(number_value);

  // Check if {number}s value matches the integer (ruling out the infinities).
  assembler->GotoUnless(
      assembler->Float64Equal(assembler->Float64Sub(number_value, integer),
                              assembler->Float64Constant(0.0)),
      &return_false);

  // Check if the {integer} value is in safe integer range.
  assembler->Branch(assembler->Float64LessThanOrEqual(
                        assembler->Float64Abs(integer),
                        assembler->Float64Constant(kMaxSafeInteger)),
                    &return_true, &return_false);

  assembler->Bind(&return_true);
  assembler->Return(assembler->BooleanConstant(true));

  assembler->Bind(&return_false);
  assembler->Return(assembler->BooleanConstant(false));
}

// ES6 section 20.1.2.12 Number.parseFloat ( string )
void Builtins::Generate_NumberParseFloat(CodeStubAssembler* assembler) {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  typedef CodeStubAssembler::Variable Variable;

  Node* context = assembler->Parameter(4);

  // We might need to loop once for ToString conversion.
  Variable var_input(assembler, MachineRepresentation::kTagged);
  Label loop(assembler, &var_input);
  var_input.Bind(assembler->Parameter(1));
  assembler->Goto(&loop);
  assembler->Bind(&loop);
  {
    // Load the current {input} value.
    Node* input = var_input.value();

    // Check if the {input} is a HeapObject or a Smi.
    Label if_inputissmi(assembler), if_inputisnotsmi(assembler);
    assembler->Branch(assembler->TaggedIsSmi(input), &if_inputissmi,
                      &if_inputisnotsmi);

    assembler->Bind(&if_inputissmi);
    {
      // The {input} is already a Number, no need to do anything.
      assembler->Return(input);
    }

    assembler->Bind(&if_inputisnotsmi);
    {
      // The {input} is a HeapObject, check if it's already a String.
      Label if_inputisstring(assembler), if_inputisnotstring(assembler);
      Node* input_map = assembler->LoadMap(input);
      Node* input_instance_type = assembler->LoadMapInstanceType(input_map);
      assembler->Branch(assembler->IsStringInstanceType(input_instance_type),
                        &if_inputisstring, &if_inputisnotstring);

      assembler->Bind(&if_inputisstring);
      {
        // The {input} is already a String, check if {input} contains
        // a cached array index.
        Label if_inputcached(assembler), if_inputnotcached(assembler);
        Node* input_hash = assembler->LoadNameHashField(input);
        Node* input_bit = assembler->Word32And(
            input_hash,
            assembler->Int32Constant(String::kContainsCachedArrayIndexMask));
        assembler->Branch(
            assembler->Word32Equal(input_bit, assembler->Int32Constant(0)),
            &if_inputcached, &if_inputnotcached);

        assembler->Bind(&if_inputcached);
        {
          // Just return the {input}s cached array index.
          Node* input_array_index =
              assembler->DecodeWordFromWord32<String::ArrayIndexValueBits>(
                  input_hash);
          assembler->Return(assembler->SmiTag(input_array_index));
        }

        assembler->Bind(&if_inputnotcached);
        {
          // Need to fall back to the runtime to convert {input} to double.
          assembler->Return(assembler->CallRuntime(Runtime::kStringParseFloat,
                                                   context, input));
        }
      }

      assembler->Bind(&if_inputisnotstring);
      {
        // The {input} is neither a String nor a Smi, check for HeapNumber.
        Label if_inputisnumber(assembler),
            if_inputisnotnumber(assembler, Label::kDeferred);
        assembler->Branch(
            assembler->WordEqual(input_map, assembler->HeapNumberMapConstant()),
            &if_inputisnumber, &if_inputisnotnumber);

        assembler->Bind(&if_inputisnumber);
        {
          // The {input} is already a Number, take care of -0.
          Label if_inputiszero(assembler), if_inputisnotzero(assembler);
          Node* input_value = assembler->LoadHeapNumberValue(input);
          assembler->Branch(assembler->Float64Equal(
                                input_value, assembler->Float64Constant(0.0)),
                            &if_inputiszero, &if_inputisnotzero);

          assembler->Bind(&if_inputiszero);
          assembler->Return(assembler->SmiConstant(0));

          assembler->Bind(&if_inputisnotzero);
          assembler->Return(input);
        }

        assembler->Bind(&if_inputisnotnumber);
        {
          // Need to convert the {input} to String first.
          // TODO(bmeurer): This could be more efficient if necessary.
          Callable callable = CodeFactory::ToString(assembler->isolate());
          var_input.Bind(assembler->CallStub(callable, context, input));
          assembler->Goto(&loop);
        }
      }
    }
  }
}

// ES6 section 20.1.2.13 Number.parseInt ( string, radix )
void Builtins::Generate_NumberParseInt(CodeStubAssembler* assembler) {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;

  Node* input = assembler->Parameter(1);
  Node* radix = assembler->Parameter(2);
  Node* context = assembler->Parameter(5);

  // Check if {radix} is treated as 10 (i.e. undefined, 0 or 10).
  Label if_radix10(assembler), if_generic(assembler, Label::kDeferred);
  assembler->GotoIf(assembler->WordEqual(radix, assembler->UndefinedConstant()),
                    &if_radix10);
  assembler->GotoIf(
      assembler->WordEqual(radix, assembler->SmiConstant(Smi::FromInt(10))),
      &if_radix10);
  assembler->GotoIf(
      assembler->WordEqual(radix, assembler->SmiConstant(Smi::FromInt(0))),
      &if_radix10);
  assembler->Goto(&if_generic);

  assembler->Bind(&if_radix10);
  {
    // Check if we can avoid the ToString conversion on {input}.
    Label if_inputissmi(assembler), if_inputisheapnumber(assembler),
        if_inputisstring(assembler);
    assembler->GotoIf(assembler->TaggedIsSmi(input), &if_inputissmi);
    Node* input_map = assembler->LoadMap(input);
    assembler->GotoIf(
        assembler->WordEqual(input_map, assembler->HeapNumberMapConstant()),
        &if_inputisheapnumber);
    Node* input_instance_type = assembler->LoadMapInstanceType(input_map);
    assembler->Branch(assembler->IsStringInstanceType(input_instance_type),
                      &if_inputisstring, &if_generic);

    assembler->Bind(&if_inputissmi);
    {
      // Just return the {input}.
      assembler->Return(input);
    }

    assembler->Bind(&if_inputisheapnumber);
    {
      // Check if the {input} value is in Signed32 range.
      Label if_inputissigned32(assembler);
      Node* input_value = assembler->LoadHeapNumberValue(input);
      Node* input_value32 = assembler->TruncateFloat64ToWord32(input_value);
      assembler->GotoIf(
          assembler->Float64Equal(
              input_value, assembler->ChangeInt32ToFloat64(input_value32)),
          &if_inputissigned32);

      // Check if the absolute {input} value is in the ]0.01,1e9[ range.
      Node* input_value_abs = assembler->Float64Abs(input_value);

      assembler->GotoUnless(
          assembler->Float64LessThan(input_value_abs,
                                     assembler->Float64Constant(1e9)),
          &if_generic);
      assembler->Branch(assembler->Float64LessThan(
                            assembler->Float64Constant(0.01), input_value_abs),
                        &if_inputissigned32, &if_generic);

      // Return the truncated int32 value, and return the tagged result.
      assembler->Bind(&if_inputissigned32);
      Node* result = assembler->ChangeInt32ToTagged(input_value32);
      assembler->Return(result);
    }

    assembler->Bind(&if_inputisstring);
    {
      // Check if the String {input} has a cached array index.
      Node* input_hash = assembler->LoadNameHashField(input);
      Node* input_bit = assembler->Word32And(
          input_hash,
          assembler->Int32Constant(String::kContainsCachedArrayIndexMask));
      assembler->GotoIf(
          assembler->Word32NotEqual(input_bit, assembler->Int32Constant(0)),
          &if_generic);

      // Return the cached array index as result.
      Node* input_index =
          assembler->DecodeWordFromWord32<String::ArrayIndexValueBits>(
              input_hash);
      Node* result = assembler->SmiTag(input_index);
      assembler->Return(result);
    }
  }

  assembler->Bind(&if_generic);
  {
    Node* result =
        assembler->CallRuntime(Runtime::kStringParseInt, context, input, radix);
    assembler->Return(result);
  }
}

// ES6 section 20.1.3.2 Number.prototype.toExponential ( fractionDigits )
BUILTIN(NumberPrototypeToExponential) {
  HandleScope scope(isolate);
  Handle<Object> value = args.at<Object>(0);
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
  Handle<Object> value = args.at<Object>(0);
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
  Handle<Object> value = args.at<Object>(0);

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
  Handle<Object> value = args.at<Object>(0);
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
  Handle<Object> value = args.at<Object>(0);
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
  if (IsUint32Double(value_number) && value_number < radix_number) {
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
void Builtins::Generate_NumberPrototypeValueOf(CodeStubAssembler* assembler) {
  typedef compiler::Node Node;

  Node* receiver = assembler->Parameter(0);
  Node* context = assembler->Parameter(3);

  Node* result = assembler->ToThisValue(
      context, receiver, PrimitiveType::kNumber, "Number.prototype.valueOf");
  assembler->Return(result);
}

// static
void Builtins::Generate_Add(CodeStubAssembler* assembler) {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  typedef CodeStubAssembler::Variable Variable;

  Node* left = assembler->Parameter(0);
  Node* right = assembler->Parameter(1);
  Node* context = assembler->Parameter(2);

  // Shared entry for floating point addition.
  Label do_fadd(assembler);
  Variable var_fadd_lhs(assembler, MachineRepresentation::kFloat64),
      var_fadd_rhs(assembler, MachineRepresentation::kFloat64);

  // We might need to loop several times due to ToPrimitive, ToString and/or
  // ToNumber conversions.
  Variable var_lhs(assembler, MachineRepresentation::kTagged),
      var_rhs(assembler, MachineRepresentation::kTagged),
      var_result(assembler, MachineRepresentation::kTagged);
  Variable* loop_vars[2] = {&var_lhs, &var_rhs};
  Label loop(assembler, 2, loop_vars), end(assembler),
      string_add_convert_left(assembler, Label::kDeferred),
      string_add_convert_right(assembler, Label::kDeferred);
  var_lhs.Bind(left);
  var_rhs.Bind(right);
  assembler->Goto(&loop);
  assembler->Bind(&loop);
  {
    // Load the current {lhs} and {rhs} values.
    Node* lhs = var_lhs.value();
    Node* rhs = var_rhs.value();

    // Check if the {lhs} is a Smi or a HeapObject.
    Label if_lhsissmi(assembler), if_lhsisnotsmi(assembler);
    assembler->Branch(assembler->TaggedIsSmi(lhs), &if_lhsissmi,
                      &if_lhsisnotsmi);

    assembler->Bind(&if_lhsissmi);
    {
      // Check if the {rhs} is also a Smi.
      Label if_rhsissmi(assembler), if_rhsisnotsmi(assembler);
      assembler->Branch(assembler->TaggedIsSmi(rhs), &if_rhsissmi,
                        &if_rhsisnotsmi);

      assembler->Bind(&if_rhsissmi);
      {
        // Try fast Smi addition first.
        Node* pair = assembler->IntPtrAddWithOverflow(
            assembler->BitcastTaggedToWord(lhs),
            assembler->BitcastTaggedToWord(rhs));
        Node* overflow = assembler->Projection(1, pair);

        // Check if the Smi additon overflowed.
        Label if_overflow(assembler), if_notoverflow(assembler);
        assembler->Branch(overflow, &if_overflow, &if_notoverflow);

        assembler->Bind(&if_overflow);
        {
          var_fadd_lhs.Bind(assembler->SmiToFloat64(lhs));
          var_fadd_rhs.Bind(assembler->SmiToFloat64(rhs));
          assembler->Goto(&do_fadd);
        }

        assembler->Bind(&if_notoverflow);
        var_result.Bind(assembler->BitcastWordToTaggedSigned(
            assembler->Projection(0, pair)));
        assembler->Goto(&end);
      }

      assembler->Bind(&if_rhsisnotsmi);
      {
        // Load the map of {rhs}.
        Node* rhs_map = assembler->LoadMap(rhs);

        // Check if the {rhs} is a HeapNumber.
        Label if_rhsisnumber(assembler),
            if_rhsisnotnumber(assembler, Label::kDeferred);
        assembler->Branch(assembler->IsHeapNumberMap(rhs_map), &if_rhsisnumber,
                          &if_rhsisnotnumber);

        assembler->Bind(&if_rhsisnumber);
        {
          var_fadd_lhs.Bind(assembler->SmiToFloat64(lhs));
          var_fadd_rhs.Bind(assembler->LoadHeapNumberValue(rhs));
          assembler->Goto(&do_fadd);
        }

        assembler->Bind(&if_rhsisnotnumber);
        {
          // Load the instance type of {rhs}.
          Node* rhs_instance_type = assembler->LoadMapInstanceType(rhs_map);

          // Check if the {rhs} is a String.
          Label if_rhsisstring(assembler, Label::kDeferred),
              if_rhsisnotstring(assembler, Label::kDeferred);
          assembler->Branch(assembler->IsStringInstanceType(rhs_instance_type),
                            &if_rhsisstring, &if_rhsisnotstring);

          assembler->Bind(&if_rhsisstring);
          {
            var_lhs.Bind(lhs);
            var_rhs.Bind(rhs);
            assembler->Goto(&string_add_convert_left);
          }

          assembler->Bind(&if_rhsisnotstring);
          {
            // Check if {rhs} is a JSReceiver.
            Label if_rhsisreceiver(assembler, Label::kDeferred),
                if_rhsisnotreceiver(assembler, Label::kDeferred);
            assembler->Branch(
                assembler->IsJSReceiverInstanceType(rhs_instance_type),
                &if_rhsisreceiver, &if_rhsisnotreceiver);

            assembler->Bind(&if_rhsisreceiver);
            {
              // Convert {rhs} to a primitive first passing no hint.
              Callable callable =
                  CodeFactory::NonPrimitiveToPrimitive(assembler->isolate());
              var_rhs.Bind(assembler->CallStub(callable, context, rhs));
              assembler->Goto(&loop);
            }

            assembler->Bind(&if_rhsisnotreceiver);
            {
              // Convert {rhs} to a Number first.
              Callable callable =
                  CodeFactory::NonNumberToNumber(assembler->isolate());
              var_rhs.Bind(assembler->CallStub(callable, context, rhs));
              assembler->Goto(&loop);
            }
          }
        }
      }
    }

    assembler->Bind(&if_lhsisnotsmi);
    {
      // Load the map and instance type of {lhs}.
      Node* lhs_instance_type = assembler->LoadInstanceType(lhs);

      // Check if {lhs} is a String.
      Label if_lhsisstring(assembler), if_lhsisnotstring(assembler);
      assembler->Branch(assembler->IsStringInstanceType(lhs_instance_type),
                        &if_lhsisstring, &if_lhsisnotstring);

      assembler->Bind(&if_lhsisstring);
      {
        var_lhs.Bind(lhs);
        var_rhs.Bind(rhs);
        assembler->Goto(&string_add_convert_right);
      }

      assembler->Bind(&if_lhsisnotstring);
      {
        // Check if {rhs} is a Smi.
        Label if_rhsissmi(assembler), if_rhsisnotsmi(assembler);
        assembler->Branch(assembler->TaggedIsSmi(rhs), &if_rhsissmi,
                          &if_rhsisnotsmi);

        assembler->Bind(&if_rhsissmi);
        {
          // Check if {lhs} is a Number.
          Label if_lhsisnumber(assembler),
              if_lhsisnotnumber(assembler, Label::kDeferred);
          assembler->Branch(assembler->Word32Equal(
                                lhs_instance_type,
                                assembler->Int32Constant(HEAP_NUMBER_TYPE)),
                            &if_lhsisnumber, &if_lhsisnotnumber);

          assembler->Bind(&if_lhsisnumber);
          {
            // The {lhs} is a HeapNumber, the {rhs} is a Smi, just add them.
            var_fadd_lhs.Bind(assembler->LoadHeapNumberValue(lhs));
            var_fadd_rhs.Bind(assembler->SmiToFloat64(rhs));
            assembler->Goto(&do_fadd);
          }

          assembler->Bind(&if_lhsisnotnumber);
          {
            // The {lhs} is neither a Number nor a String, and the {rhs} is a
            // Smi.
            Label if_lhsisreceiver(assembler, Label::kDeferred),
                if_lhsisnotreceiver(assembler, Label::kDeferred);
            assembler->Branch(
                assembler->IsJSReceiverInstanceType(lhs_instance_type),
                &if_lhsisreceiver, &if_lhsisnotreceiver);

            assembler->Bind(&if_lhsisreceiver);
            {
              // Convert {lhs} to a primitive first passing no hint.
              Callable callable =
                  CodeFactory::NonPrimitiveToPrimitive(assembler->isolate());
              var_lhs.Bind(assembler->CallStub(callable, context, lhs));
              assembler->Goto(&loop);
            }

            assembler->Bind(&if_lhsisnotreceiver);
            {
              // Convert {lhs} to a Number first.
              Callable callable =
                  CodeFactory::NonNumberToNumber(assembler->isolate());
              var_lhs.Bind(assembler->CallStub(callable, context, lhs));
              assembler->Goto(&loop);
            }
          }
        }

        assembler->Bind(&if_rhsisnotsmi);
        {
          // Load the instance type of {rhs}.
          Node* rhs_instance_type = assembler->LoadInstanceType(rhs);

          // Check if {rhs} is a String.
          Label if_rhsisstring(assembler), if_rhsisnotstring(assembler);
          assembler->Branch(assembler->IsStringInstanceType(rhs_instance_type),
                            &if_rhsisstring, &if_rhsisnotstring);

          assembler->Bind(&if_rhsisstring);
          {
            var_lhs.Bind(lhs);
            var_rhs.Bind(rhs);
            assembler->Goto(&string_add_convert_left);
          }

          assembler->Bind(&if_rhsisnotstring);
          {
            // Check if {lhs} is a HeapNumber.
            Label if_lhsisnumber(assembler), if_lhsisnotnumber(assembler);
            assembler->Branch(assembler->Word32Equal(
                                  lhs_instance_type,
                                  assembler->Int32Constant(HEAP_NUMBER_TYPE)),
                              &if_lhsisnumber, &if_lhsisnotnumber);

            assembler->Bind(&if_lhsisnumber);
            {
              // Check if {rhs} is also a HeapNumber.
              Label if_rhsisnumber(assembler),
                  if_rhsisnotnumber(assembler, Label::kDeferred);
              assembler->Branch(assembler->Word32Equal(
                                    rhs_instance_type,
                                    assembler->Int32Constant(HEAP_NUMBER_TYPE)),
                                &if_rhsisnumber, &if_rhsisnotnumber);

              assembler->Bind(&if_rhsisnumber);
              {
                // Perform a floating point addition.
                var_fadd_lhs.Bind(assembler->LoadHeapNumberValue(lhs));
                var_fadd_rhs.Bind(assembler->LoadHeapNumberValue(rhs));
                assembler->Goto(&do_fadd);
              }

              assembler->Bind(&if_rhsisnotnumber);
              {
                // Check if {rhs} is a JSReceiver.
                Label if_rhsisreceiver(assembler, Label::kDeferred),
                    if_rhsisnotreceiver(assembler, Label::kDeferred);
                assembler->Branch(
                    assembler->IsJSReceiverInstanceType(rhs_instance_type),
                    &if_rhsisreceiver, &if_rhsisnotreceiver);

                assembler->Bind(&if_rhsisreceiver);
                {
                  // Convert {rhs} to a primitive first passing no hint.
                  Callable callable = CodeFactory::NonPrimitiveToPrimitive(
                      assembler->isolate());
                  var_rhs.Bind(assembler->CallStub(callable, context, rhs));
                  assembler->Goto(&loop);
                }

                assembler->Bind(&if_rhsisnotreceiver);
                {
                  // Convert {rhs} to a Number first.
                  Callable callable =
                      CodeFactory::NonNumberToNumber(assembler->isolate());
                  var_rhs.Bind(assembler->CallStub(callable, context, rhs));
                  assembler->Goto(&loop);
                }
              }
            }

            assembler->Bind(&if_lhsisnotnumber);
            {
              // Check if {lhs} is a JSReceiver.
              Label if_lhsisreceiver(assembler, Label::kDeferred),
                  if_lhsisnotreceiver(assembler);
              assembler->Branch(
                  assembler->IsJSReceiverInstanceType(lhs_instance_type),
                  &if_lhsisreceiver, &if_lhsisnotreceiver);

              assembler->Bind(&if_lhsisreceiver);
              {
                // Convert {lhs} to a primitive first passing no hint.
                Callable callable =
                    CodeFactory::NonPrimitiveToPrimitive(assembler->isolate());
                var_lhs.Bind(assembler->CallStub(callable, context, lhs));
                assembler->Goto(&loop);
              }

              assembler->Bind(&if_lhsisnotreceiver);
              {
                // Check if {rhs} is a JSReceiver.
                Label if_rhsisreceiver(assembler, Label::kDeferred),
                    if_rhsisnotreceiver(assembler, Label::kDeferred);
                assembler->Branch(
                    assembler->IsJSReceiverInstanceType(rhs_instance_type),
                    &if_rhsisreceiver, &if_rhsisnotreceiver);

                assembler->Bind(&if_rhsisreceiver);
                {
                  // Convert {rhs} to a primitive first passing no hint.
                  Callable callable = CodeFactory::NonPrimitiveToPrimitive(
                      assembler->isolate());
                  var_rhs.Bind(assembler->CallStub(callable, context, rhs));
                  assembler->Goto(&loop);
                }

                assembler->Bind(&if_rhsisnotreceiver);
                {
                  // Convert {lhs} to a Number first.
                  Callable callable =
                      CodeFactory::NonNumberToNumber(assembler->isolate());
                  var_lhs.Bind(assembler->CallStub(callable, context, lhs));
                  assembler->Goto(&loop);
                }
              }
            }
          }
        }
      }
    }
  }
  assembler->Bind(&string_add_convert_left);
  {
    // Convert {lhs}, which is a Smi, to a String and concatenate the
    // resulting string with the String {rhs}.
    Callable callable = CodeFactory::StringAdd(
        assembler->isolate(), STRING_ADD_CONVERT_LEFT, NOT_TENURED);
    var_result.Bind(assembler->CallStub(callable, context, var_lhs.value(),
                                        var_rhs.value()));
    assembler->Goto(&end);
  }

  assembler->Bind(&string_add_convert_right);
  {
    // Convert {lhs}, which is a Smi, to a String and concatenate the
    // resulting string with the String {rhs}.
    Callable callable = CodeFactory::StringAdd(
        assembler->isolate(), STRING_ADD_CONVERT_RIGHT, NOT_TENURED);
    var_result.Bind(assembler->CallStub(callable, context, var_lhs.value(),
                                        var_rhs.value()));
    assembler->Goto(&end);
  }

  assembler->Bind(&do_fadd);
  {
    Node* lhs_value = var_fadd_lhs.value();
    Node* rhs_value = var_fadd_rhs.value();
    Node* value = assembler->Float64Add(lhs_value, rhs_value);
    Node* result = assembler->AllocateHeapNumberWithValue(value);
    var_result.Bind(result);
    assembler->Goto(&end);
  }
  assembler->Bind(&end);
  assembler->Return(var_result.value());
}

void Builtins::Generate_Subtract(CodeStubAssembler* assembler) {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  typedef CodeStubAssembler::Variable Variable;

  Node* left = assembler->Parameter(0);
  Node* right = assembler->Parameter(1);
  Node* context = assembler->Parameter(2);

  // Shared entry for floating point subtraction.
  Label do_fsub(assembler), end(assembler);
  Variable var_fsub_lhs(assembler, MachineRepresentation::kFloat64),
      var_fsub_rhs(assembler, MachineRepresentation::kFloat64);

  // We might need to loop several times due to ToPrimitive and/or ToNumber
  // conversions.
  Variable var_lhs(assembler, MachineRepresentation::kTagged),
      var_rhs(assembler, MachineRepresentation::kTagged),
      var_result(assembler, MachineRepresentation::kTagged);
  Variable* loop_vars[2] = {&var_lhs, &var_rhs};
  Label loop(assembler, 2, loop_vars);
  var_lhs.Bind(left);
  var_rhs.Bind(right);
  assembler->Goto(&loop);
  assembler->Bind(&loop);
  {
    // Load the current {lhs} and {rhs} values.
    Node* lhs = var_lhs.value();
    Node* rhs = var_rhs.value();

    // Check if the {lhs} is a Smi or a HeapObject.
    Label if_lhsissmi(assembler), if_lhsisnotsmi(assembler);
    assembler->Branch(assembler->TaggedIsSmi(lhs), &if_lhsissmi,
                      &if_lhsisnotsmi);

    assembler->Bind(&if_lhsissmi);
    {
      // Check if the {rhs} is also a Smi.
      Label if_rhsissmi(assembler), if_rhsisnotsmi(assembler);
      assembler->Branch(assembler->TaggedIsSmi(rhs), &if_rhsissmi,
                        &if_rhsisnotsmi);

      assembler->Bind(&if_rhsissmi);
      {
        // Try a fast Smi subtraction first.
        Node* pair = assembler->IntPtrSubWithOverflow(
            assembler->BitcastTaggedToWord(lhs),
            assembler->BitcastTaggedToWord(rhs));
        Node* overflow = assembler->Projection(1, pair);

        // Check if the Smi subtraction overflowed.
        Label if_overflow(assembler), if_notoverflow(assembler);
        assembler->Branch(overflow, &if_overflow, &if_notoverflow);

        assembler->Bind(&if_overflow);
        {
          // The result doesn't fit into Smi range.
          var_fsub_lhs.Bind(assembler->SmiToFloat64(lhs));
          var_fsub_rhs.Bind(assembler->SmiToFloat64(rhs));
          assembler->Goto(&do_fsub);
        }

        assembler->Bind(&if_notoverflow);
        var_result.Bind(assembler->BitcastWordToTaggedSigned(
            assembler->Projection(0, pair)));
        assembler->Goto(&end);
      }

      assembler->Bind(&if_rhsisnotsmi);
      {
        // Load the map of the {rhs}.
        Node* rhs_map = assembler->LoadMap(rhs);

        // Check if {rhs} is a HeapNumber.
        Label if_rhsisnumber(assembler),
            if_rhsisnotnumber(assembler, Label::kDeferred);
        assembler->Branch(assembler->IsHeapNumberMap(rhs_map), &if_rhsisnumber,
                          &if_rhsisnotnumber);

        assembler->Bind(&if_rhsisnumber);
        {
          // Perform a floating point subtraction.
          var_fsub_lhs.Bind(assembler->SmiToFloat64(lhs));
          var_fsub_rhs.Bind(assembler->LoadHeapNumberValue(rhs));
          assembler->Goto(&do_fsub);
        }

        assembler->Bind(&if_rhsisnotnumber);
        {
          // Convert the {rhs} to a Number first.
          Callable callable =
              CodeFactory::NonNumberToNumber(assembler->isolate());
          var_rhs.Bind(assembler->CallStub(callable, context, rhs));
          assembler->Goto(&loop);
        }
      }
    }

    assembler->Bind(&if_lhsisnotsmi);
    {
      // Load the map of the {lhs}.
      Node* lhs_map = assembler->LoadMap(lhs);

      // Check if the {lhs} is a HeapNumber.
      Label if_lhsisnumber(assembler),
          if_lhsisnotnumber(assembler, Label::kDeferred);
      Node* number_map = assembler->HeapNumberMapConstant();
      assembler->Branch(assembler->WordEqual(lhs_map, number_map),
                        &if_lhsisnumber, &if_lhsisnotnumber);

      assembler->Bind(&if_lhsisnumber);
      {
        // Check if the {rhs} is a Smi.
        Label if_rhsissmi(assembler), if_rhsisnotsmi(assembler);
        assembler->Branch(assembler->TaggedIsSmi(rhs), &if_rhsissmi,
                          &if_rhsisnotsmi);

        assembler->Bind(&if_rhsissmi);
        {
          // Perform a floating point subtraction.
          var_fsub_lhs.Bind(assembler->LoadHeapNumberValue(lhs));
          var_fsub_rhs.Bind(assembler->SmiToFloat64(rhs));
          assembler->Goto(&do_fsub);
        }

        assembler->Bind(&if_rhsisnotsmi);
        {
          // Load the map of the {rhs}.
          Node* rhs_map = assembler->LoadMap(rhs);

          // Check if the {rhs} is a HeapNumber.
          Label if_rhsisnumber(assembler),
              if_rhsisnotnumber(assembler, Label::kDeferred);
          assembler->Branch(assembler->WordEqual(rhs_map, number_map),
                            &if_rhsisnumber, &if_rhsisnotnumber);

          assembler->Bind(&if_rhsisnumber);
          {
            // Perform a floating point subtraction.
            var_fsub_lhs.Bind(assembler->LoadHeapNumberValue(lhs));
            var_fsub_rhs.Bind(assembler->LoadHeapNumberValue(rhs));
            assembler->Goto(&do_fsub);
          }

          assembler->Bind(&if_rhsisnotnumber);
          {
            // Convert the {rhs} to a Number first.
            Callable callable =
                CodeFactory::NonNumberToNumber(assembler->isolate());
            var_rhs.Bind(assembler->CallStub(callable, context, rhs));
            assembler->Goto(&loop);
          }
        }
      }

      assembler->Bind(&if_lhsisnotnumber);
      {
        // Convert the {lhs} to a Number first.
        Callable callable =
            CodeFactory::NonNumberToNumber(assembler->isolate());
        var_lhs.Bind(assembler->CallStub(callable, context, lhs));
        assembler->Goto(&loop);
      }
    }
  }

  assembler->Bind(&do_fsub);
  {
    Node* lhs_value = var_fsub_lhs.value();
    Node* rhs_value = var_fsub_rhs.value();
    Node* value = assembler->Float64Sub(lhs_value, rhs_value);
    var_result.Bind(assembler->AllocateHeapNumberWithValue(value));
    assembler->Goto(&end);
  }
  assembler->Bind(&end);
  assembler->Return(var_result.value());
}

void Builtins::Generate_Multiply(CodeStubAssembler* assembler) {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  typedef CodeStubAssembler::Variable Variable;

  Node* left = assembler->Parameter(0);
  Node* right = assembler->Parameter(1);
  Node* context = assembler->Parameter(2);

  // Shared entry point for floating point multiplication.
  Label do_fmul(assembler), return_result(assembler);
  Variable var_lhs_float64(assembler, MachineRepresentation::kFloat64),
      var_rhs_float64(assembler, MachineRepresentation::kFloat64);

  Node* number_map = assembler->HeapNumberMapConstant();

  // We might need to loop one or two times due to ToNumber conversions.
  Variable var_lhs(assembler, MachineRepresentation::kTagged),
      var_rhs(assembler, MachineRepresentation::kTagged),
      var_result(assembler, MachineRepresentation::kTagged);
  Variable* loop_variables[] = {&var_lhs, &var_rhs};
  Label loop(assembler, 2, loop_variables);
  var_lhs.Bind(left);
  var_rhs.Bind(right);
  assembler->Goto(&loop);
  assembler->Bind(&loop);
  {
    Node* lhs = var_lhs.value();
    Node* rhs = var_rhs.value();

    Label lhs_is_smi(assembler), lhs_is_not_smi(assembler);
    assembler->Branch(assembler->TaggedIsSmi(lhs), &lhs_is_smi,
                      &lhs_is_not_smi);

    assembler->Bind(&lhs_is_smi);
    {
      Label rhs_is_smi(assembler), rhs_is_not_smi(assembler);
      assembler->Branch(assembler->TaggedIsSmi(rhs), &rhs_is_smi,
                        &rhs_is_not_smi);

      assembler->Bind(&rhs_is_smi);
      {
        // Both {lhs} and {rhs} are Smis. The result is not necessarily a smi,
        // in case of overflow.
        var_result.Bind(assembler->SmiMul(lhs, rhs));
        assembler->Goto(&return_result);
      }

      assembler->Bind(&rhs_is_not_smi);
      {
        Node* rhs_map = assembler->LoadMap(rhs);

        // Check if {rhs} is a HeapNumber.
        Label rhs_is_number(assembler),
            rhs_is_not_number(assembler, Label::kDeferred);
        assembler->Branch(assembler->WordEqual(rhs_map, number_map),
                          &rhs_is_number, &rhs_is_not_number);

        assembler->Bind(&rhs_is_number);
        {
          // Convert {lhs} to a double and multiply it with the value of {rhs}.
          var_lhs_float64.Bind(assembler->SmiToFloat64(lhs));
          var_rhs_float64.Bind(assembler->LoadHeapNumberValue(rhs));
          assembler->Goto(&do_fmul);
        }

        assembler->Bind(&rhs_is_not_number);
        {
          // Multiplication is commutative, swap {lhs} with {rhs} and loop.
          var_lhs.Bind(rhs);
          var_rhs.Bind(lhs);
          assembler->Goto(&loop);
        }
      }
    }

    assembler->Bind(&lhs_is_not_smi);
    {
      Node* lhs_map = assembler->LoadMap(lhs);

      // Check if {lhs} is a HeapNumber.
      Label lhs_is_number(assembler),
          lhs_is_not_number(assembler, Label::kDeferred);
      assembler->Branch(assembler->WordEqual(lhs_map, number_map),
                        &lhs_is_number, &lhs_is_not_number);

      assembler->Bind(&lhs_is_number);
      {
        // Check if {rhs} is a Smi.
        Label rhs_is_smi(assembler), rhs_is_not_smi(assembler);
        assembler->Branch(assembler->TaggedIsSmi(rhs), &rhs_is_smi,
                          &rhs_is_not_smi);

        assembler->Bind(&rhs_is_smi);
        {
          // Convert {rhs} to a double and multiply it with the value of {lhs}.
          var_lhs_float64.Bind(assembler->LoadHeapNumberValue(lhs));
          var_rhs_float64.Bind(assembler->SmiToFloat64(rhs));
          assembler->Goto(&do_fmul);
        }

        assembler->Bind(&rhs_is_not_smi);
        {
          Node* rhs_map = assembler->LoadMap(rhs);

          // Check if {rhs} is a HeapNumber.
          Label rhs_is_number(assembler),
              rhs_is_not_number(assembler, Label::kDeferred);
          assembler->Branch(assembler->WordEqual(rhs_map, number_map),
                            &rhs_is_number, &rhs_is_not_number);

          assembler->Bind(&rhs_is_number);
          {
            // Both {lhs} and {rhs} are HeapNumbers. Load their values and
            // multiply them.
            var_lhs_float64.Bind(assembler->LoadHeapNumberValue(lhs));
            var_rhs_float64.Bind(assembler->LoadHeapNumberValue(rhs));
            assembler->Goto(&do_fmul);
          }

          assembler->Bind(&rhs_is_not_number);
          {
            // Multiplication is commutative, swap {lhs} with {rhs} and loop.
            var_lhs.Bind(rhs);
            var_rhs.Bind(lhs);
            assembler->Goto(&loop);
          }
        }
      }

      assembler->Bind(&lhs_is_not_number);
      {
        // Convert {lhs} to a Number and loop.
        Callable callable =
            CodeFactory::NonNumberToNumber(assembler->isolate());
        var_lhs.Bind(assembler->CallStub(callable, context, lhs));
        assembler->Goto(&loop);
      }
    }
  }

  assembler->Bind(&do_fmul);
  {
    Node* value =
        assembler->Float64Mul(var_lhs_float64.value(), var_rhs_float64.value());
    Node* result = assembler->AllocateHeapNumberWithValue(value);
    var_result.Bind(result);
    assembler->Goto(&return_result);
  }

  assembler->Bind(&return_result);
  assembler->Return(var_result.value());
}

void Builtins::Generate_Divide(CodeStubAssembler* assembler) {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  typedef CodeStubAssembler::Variable Variable;

  Node* left = assembler->Parameter(0);
  Node* right = assembler->Parameter(1);
  Node* context = assembler->Parameter(2);

  // Shared entry point for floating point division.
  Label do_fdiv(assembler), end(assembler);
  Variable var_dividend_float64(assembler, MachineRepresentation::kFloat64),
      var_divisor_float64(assembler, MachineRepresentation::kFloat64);

  Node* number_map = assembler->HeapNumberMapConstant();

  // We might need to loop one or two times due to ToNumber conversions.
  Variable var_dividend(assembler, MachineRepresentation::kTagged),
      var_divisor(assembler, MachineRepresentation::kTagged),
      var_result(assembler, MachineRepresentation::kTagged);
  Variable* loop_variables[] = {&var_dividend, &var_divisor};
  Label loop(assembler, 2, loop_variables);
  var_dividend.Bind(left);
  var_divisor.Bind(right);
  assembler->Goto(&loop);
  assembler->Bind(&loop);
  {
    Node* dividend = var_dividend.value();
    Node* divisor = var_divisor.value();

    Label dividend_is_smi(assembler), dividend_is_not_smi(assembler);
    assembler->Branch(assembler->TaggedIsSmi(dividend), &dividend_is_smi,
                      &dividend_is_not_smi);

    assembler->Bind(&dividend_is_smi);
    {
      Label divisor_is_smi(assembler), divisor_is_not_smi(assembler);
      assembler->Branch(assembler->TaggedIsSmi(divisor), &divisor_is_smi,
                        &divisor_is_not_smi);

      assembler->Bind(&divisor_is_smi);
      {
        Label bailout(assembler);

        // Do floating point division if {divisor} is zero.
        assembler->GotoIf(
            assembler->WordEqual(divisor, assembler->IntPtrConstant(0)),
            &bailout);

        // Do floating point division {dividend} is zero and {divisor} is
        // negative.
        Label dividend_is_zero(assembler), dividend_is_not_zero(assembler);
        assembler->Branch(
            assembler->WordEqual(dividend, assembler->IntPtrConstant(0)),
            &dividend_is_zero, &dividend_is_not_zero);

        assembler->Bind(&dividend_is_zero);
        {
          assembler->GotoIf(
              assembler->IntPtrLessThan(divisor, assembler->IntPtrConstant(0)),
              &bailout);
          assembler->Goto(&dividend_is_not_zero);
        }
        assembler->Bind(&dividend_is_not_zero);

        Node* untagged_divisor = assembler->SmiUntag(divisor);
        Node* untagged_dividend = assembler->SmiUntag(dividend);

        // Do floating point division if {dividend} is kMinInt (or kMinInt - 1
        // if the Smi size is 31) and {divisor} is -1.
        Label divisor_is_minus_one(assembler),
            divisor_is_not_minus_one(assembler);
        assembler->Branch(assembler->Word32Equal(untagged_divisor,
                                                 assembler->Int32Constant(-1)),
                          &divisor_is_minus_one, &divisor_is_not_minus_one);

        assembler->Bind(&divisor_is_minus_one);
        {
          assembler->GotoIf(
              assembler->Word32Equal(
                  untagged_dividend,
                  assembler->Int32Constant(
                      kSmiValueSize == 32 ? kMinInt : (kMinInt >> 1))),
              &bailout);
          assembler->Goto(&divisor_is_not_minus_one);
        }
        assembler->Bind(&divisor_is_not_minus_one);

        // TODO(epertoso): consider adding a machine instruction that returns
        // both the result and the remainder.
        Node* untagged_result =
            assembler->Int32Div(untagged_dividend, untagged_divisor);
        Node* truncated =
            assembler->Int32Mul(untagged_result, untagged_divisor);
        // Do floating point division if the remainder is not 0.
        assembler->GotoIf(
            assembler->Word32NotEqual(untagged_dividend, truncated), &bailout);
        var_result.Bind(assembler->SmiTag(untagged_result));
        assembler->Goto(&end);

        // Bailout: convert {dividend} and {divisor} to double and do double
        // division.
        assembler->Bind(&bailout);
        {
          var_dividend_float64.Bind(assembler->SmiToFloat64(dividend));
          var_divisor_float64.Bind(assembler->SmiToFloat64(divisor));
          assembler->Goto(&do_fdiv);
        }
      }

      assembler->Bind(&divisor_is_not_smi);
      {
        Node* divisor_map = assembler->LoadMap(divisor);

        // Check if {divisor} is a HeapNumber.
        Label divisor_is_number(assembler),
            divisor_is_not_number(assembler, Label::kDeferred);
        assembler->Branch(assembler->WordEqual(divisor_map, number_map),
                          &divisor_is_number, &divisor_is_not_number);

        assembler->Bind(&divisor_is_number);
        {
          // Convert {dividend} to a double and divide it with the value of
          // {divisor}.
          var_dividend_float64.Bind(assembler->SmiToFloat64(dividend));
          var_divisor_float64.Bind(assembler->LoadHeapNumberValue(divisor));
          assembler->Goto(&do_fdiv);
        }

        assembler->Bind(&divisor_is_not_number);
        {
          // Convert {divisor} to a number and loop.
          Callable callable =
              CodeFactory::NonNumberToNumber(assembler->isolate());
          var_divisor.Bind(assembler->CallStub(callable, context, divisor));
          assembler->Goto(&loop);
        }
      }
    }

    assembler->Bind(&dividend_is_not_smi);
    {
      Node* dividend_map = assembler->LoadMap(dividend);

      // Check if {dividend} is a HeapNumber.
      Label dividend_is_number(assembler),
          dividend_is_not_number(assembler, Label::kDeferred);
      assembler->Branch(assembler->WordEqual(dividend_map, number_map),
                        &dividend_is_number, &dividend_is_not_number);

      assembler->Bind(&dividend_is_number);
      {
        // Check if {divisor} is a Smi.
        Label divisor_is_smi(assembler), divisor_is_not_smi(assembler);
        assembler->Branch(assembler->TaggedIsSmi(divisor), &divisor_is_smi,
                          &divisor_is_not_smi);

        assembler->Bind(&divisor_is_smi);
        {
          // Convert {divisor} to a double and use it for a floating point
          // division.
          var_dividend_float64.Bind(assembler->LoadHeapNumberValue(dividend));
          var_divisor_float64.Bind(assembler->SmiToFloat64(divisor));
          assembler->Goto(&do_fdiv);
        }

        assembler->Bind(&divisor_is_not_smi);
        {
          Node* divisor_map = assembler->LoadMap(divisor);

          // Check if {divisor} is a HeapNumber.
          Label divisor_is_number(assembler),
              divisor_is_not_number(assembler, Label::kDeferred);
          assembler->Branch(assembler->WordEqual(divisor_map, number_map),
                            &divisor_is_number, &divisor_is_not_number);

          assembler->Bind(&divisor_is_number);
          {
            // Both {dividend} and {divisor} are HeapNumbers. Load their values
            // and divide them.
            var_dividend_float64.Bind(assembler->LoadHeapNumberValue(dividend));
            var_divisor_float64.Bind(assembler->LoadHeapNumberValue(divisor));
            assembler->Goto(&do_fdiv);
          }

          assembler->Bind(&divisor_is_not_number);
          {
            // Convert {divisor} to a number and loop.
            Callable callable =
                CodeFactory::NonNumberToNumber(assembler->isolate());
            var_divisor.Bind(assembler->CallStub(callable, context, divisor));
            assembler->Goto(&loop);
          }
        }
      }

      assembler->Bind(&dividend_is_not_number);
      {
        // Convert {dividend} to a Number and loop.
        Callable callable =
            CodeFactory::NonNumberToNumber(assembler->isolate());
        var_dividend.Bind(assembler->CallStub(callable, context, dividend));
        assembler->Goto(&loop);
      }
    }
  }

  assembler->Bind(&do_fdiv);
  {
    Node* value = assembler->Float64Div(var_dividend_float64.value(),
                                        var_divisor_float64.value());
    var_result.Bind(assembler->AllocateHeapNumberWithValue(value));
    assembler->Goto(&end);
  }
  assembler->Bind(&end);
  assembler->Return(var_result.value());
}

void Builtins::Generate_Modulus(CodeStubAssembler* assembler) {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  typedef CodeStubAssembler::Variable Variable;

  Node* left = assembler->Parameter(0);
  Node* right = assembler->Parameter(1);
  Node* context = assembler->Parameter(2);

  Variable var_result(assembler, MachineRepresentation::kTagged);
  Label return_result(assembler, &var_result);

  // Shared entry point for floating point modulus.
  Label do_fmod(assembler);
  Variable var_dividend_float64(assembler, MachineRepresentation::kFloat64),
      var_divisor_float64(assembler, MachineRepresentation::kFloat64);

  Node* number_map = assembler->HeapNumberMapConstant();

  // We might need to loop one or two times due to ToNumber conversions.
  Variable var_dividend(assembler, MachineRepresentation::kTagged),
      var_divisor(assembler, MachineRepresentation::kTagged);
  Variable* loop_variables[] = {&var_dividend, &var_divisor};
  Label loop(assembler, 2, loop_variables);
  var_dividend.Bind(left);
  var_divisor.Bind(right);
  assembler->Goto(&loop);
  assembler->Bind(&loop);
  {
    Node* dividend = var_dividend.value();
    Node* divisor = var_divisor.value();

    Label dividend_is_smi(assembler), dividend_is_not_smi(assembler);
    assembler->Branch(assembler->TaggedIsSmi(dividend), &dividend_is_smi,
                      &dividend_is_not_smi);

    assembler->Bind(&dividend_is_smi);
    {
      Label dividend_is_not_zero(assembler);
      Label divisor_is_smi(assembler), divisor_is_not_smi(assembler);
      assembler->Branch(assembler->TaggedIsSmi(divisor), &divisor_is_smi,
                        &divisor_is_not_smi);

      assembler->Bind(&divisor_is_smi);
      {
        // Compute the modulus of two Smis.
        var_result.Bind(assembler->SmiMod(dividend, divisor));
        assembler->Goto(&return_result);
      }

      assembler->Bind(&divisor_is_not_smi);
      {
        Node* divisor_map = assembler->LoadMap(divisor);

        // Check if {divisor} is a HeapNumber.
        Label divisor_is_number(assembler),
            divisor_is_not_number(assembler, Label::kDeferred);
        assembler->Branch(assembler->WordEqual(divisor_map, number_map),
                          &divisor_is_number, &divisor_is_not_number);

        assembler->Bind(&divisor_is_number);
        {
          // Convert {dividend} to a double and compute its modulus with the
          // value of {dividend}.
          var_dividend_float64.Bind(assembler->SmiToFloat64(dividend));
          var_divisor_float64.Bind(assembler->LoadHeapNumberValue(divisor));
          assembler->Goto(&do_fmod);
        }

        assembler->Bind(&divisor_is_not_number);
        {
          // Convert {divisor} to a number and loop.
          Callable callable =
              CodeFactory::NonNumberToNumber(assembler->isolate());
          var_divisor.Bind(assembler->CallStub(callable, context, divisor));
          assembler->Goto(&loop);
        }
      }
    }

    assembler->Bind(&dividend_is_not_smi);
    {
      Node* dividend_map = assembler->LoadMap(dividend);

      // Check if {dividend} is a HeapNumber.
      Label dividend_is_number(assembler),
          dividend_is_not_number(assembler, Label::kDeferred);
      assembler->Branch(assembler->WordEqual(dividend_map, number_map),
                        &dividend_is_number, &dividend_is_not_number);

      assembler->Bind(&dividend_is_number);
      {
        // Check if {divisor} is a Smi.
        Label divisor_is_smi(assembler), divisor_is_not_smi(assembler);
        assembler->Branch(assembler->TaggedIsSmi(divisor), &divisor_is_smi,
                          &divisor_is_not_smi);

        assembler->Bind(&divisor_is_smi);
        {
          // Convert {divisor} to a double and compute {dividend}'s modulus with
          // it.
          var_dividend_float64.Bind(assembler->LoadHeapNumberValue(dividend));
          var_divisor_float64.Bind(assembler->SmiToFloat64(divisor));
          assembler->Goto(&do_fmod);
        }

        assembler->Bind(&divisor_is_not_smi);
        {
          Node* divisor_map = assembler->LoadMap(divisor);

          // Check if {divisor} is a HeapNumber.
          Label divisor_is_number(assembler),
              divisor_is_not_number(assembler, Label::kDeferred);
          assembler->Branch(assembler->WordEqual(divisor_map, number_map),
                            &divisor_is_number, &divisor_is_not_number);

          assembler->Bind(&divisor_is_number);
          {
            // Both {dividend} and {divisor} are HeapNumbers. Load their values
            // and compute their modulus.
            var_dividend_float64.Bind(assembler->LoadHeapNumberValue(dividend));
            var_divisor_float64.Bind(assembler->LoadHeapNumberValue(divisor));
            assembler->Goto(&do_fmod);
          }

          assembler->Bind(&divisor_is_not_number);
          {
            // Convert {divisor} to a number and loop.
            Callable callable =
                CodeFactory::NonNumberToNumber(assembler->isolate());
            var_divisor.Bind(assembler->CallStub(callable, context, divisor));
            assembler->Goto(&loop);
          }
        }
      }

      assembler->Bind(&dividend_is_not_number);
      {
        // Convert {dividend} to a Number and loop.
        Callable callable =
            CodeFactory::NonNumberToNumber(assembler->isolate());
        var_dividend.Bind(assembler->CallStub(callable, context, dividend));
        assembler->Goto(&loop);
      }
    }
  }

  assembler->Bind(&do_fmod);
  {
    Node* value = assembler->Float64Mod(var_dividend_float64.value(),
                                        var_divisor_float64.value());
    var_result.Bind(assembler->AllocateHeapNumberWithValue(value));
    assembler->Goto(&return_result);
  }

  assembler->Bind(&return_result);
  assembler->Return(var_result.value());
}

void Builtins::Generate_ShiftLeft(CodeStubAssembler* assembler) {
  compiler::Node* left = assembler->Parameter(0);
  compiler::Node* right = assembler->Parameter(1);
  compiler::Node* context = assembler->Parameter(2);

  using compiler::Node;

  Node* lhs_value = assembler->TruncateTaggedToWord32(context, left);
  Node* rhs_value = assembler->TruncateTaggedToWord32(context, right);
  Node* shift_count =
      assembler->Word32And(rhs_value, assembler->Int32Constant(0x1f));
  Node* value = assembler->Word32Shl(lhs_value, shift_count);
  Node* result = assembler->ChangeInt32ToTagged(value);
  assembler->Return(result);
}

void Builtins::Generate_ShiftRight(CodeStubAssembler* assembler) {
  compiler::Node* left = assembler->Parameter(0);
  compiler::Node* right = assembler->Parameter(1);
  compiler::Node* context = assembler->Parameter(2);

  using compiler::Node;

  Node* lhs_value = assembler->TruncateTaggedToWord32(context, left);
  Node* rhs_value = assembler->TruncateTaggedToWord32(context, right);
  Node* shift_count =
      assembler->Word32And(rhs_value, assembler->Int32Constant(0x1f));
  Node* value = assembler->Word32Sar(lhs_value, shift_count);
  Node* result = assembler->ChangeInt32ToTagged(value);
  assembler->Return(result);
}

void Builtins::Generate_ShiftRightLogical(CodeStubAssembler* assembler) {
  compiler::Node* left = assembler->Parameter(0);
  compiler::Node* right = assembler->Parameter(1);
  compiler::Node* context = assembler->Parameter(2);

  using compiler::Node;

  Node* lhs_value = assembler->TruncateTaggedToWord32(context, left);
  Node* rhs_value = assembler->TruncateTaggedToWord32(context, right);
  Node* shift_count =
      assembler->Word32And(rhs_value, assembler->Int32Constant(0x1f));
  Node* value = assembler->Word32Shr(lhs_value, shift_count);
  Node* result = assembler->ChangeUint32ToTagged(value);
  assembler->Return(result);
}

void Builtins::Generate_BitwiseAnd(CodeStubAssembler* assembler) {
  compiler::Node* left = assembler->Parameter(0);
  compiler::Node* right = assembler->Parameter(1);
  compiler::Node* context = assembler->Parameter(2);

  using compiler::Node;

  Node* lhs_value = assembler->TruncateTaggedToWord32(context, left);
  Node* rhs_value = assembler->TruncateTaggedToWord32(context, right);
  Node* value = assembler->Word32And(lhs_value, rhs_value);
  Node* result = assembler->ChangeInt32ToTagged(value);
  assembler->Return(result);
}

void Builtins::Generate_BitwiseOr(CodeStubAssembler* assembler) {
  compiler::Node* left = assembler->Parameter(0);
  compiler::Node* right = assembler->Parameter(1);
  compiler::Node* context = assembler->Parameter(2);

  using compiler::Node;

  Node* lhs_value = assembler->TruncateTaggedToWord32(context, left);
  Node* rhs_value = assembler->TruncateTaggedToWord32(context, right);
  Node* value = assembler->Word32Or(lhs_value, rhs_value);
  Node* result = assembler->ChangeInt32ToTagged(value);
  assembler->Return(result);
}

void Builtins::Generate_BitwiseXor(CodeStubAssembler* assembler) {
  compiler::Node* left = assembler->Parameter(0);
  compiler::Node* right = assembler->Parameter(1);
  compiler::Node* context = assembler->Parameter(2);

  using compiler::Node;

  Node* lhs_value = assembler->TruncateTaggedToWord32(context, left);
  Node* rhs_value = assembler->TruncateTaggedToWord32(context, right);
  Node* value = assembler->Word32Xor(lhs_value, rhs_value);
  Node* result = assembler->ChangeInt32ToTagged(value);
  assembler->Return(result);
}

void Builtins::Generate_LessThan(CodeStubAssembler* assembler) {
  compiler::Node* lhs = assembler->Parameter(0);
  compiler::Node* rhs = assembler->Parameter(1);
  compiler::Node* context = assembler->Parameter(2);

  assembler->Return(assembler->RelationalComparison(
      CodeStubAssembler::kLessThan, lhs, rhs, context));
}

void Builtins::Generate_LessThanOrEqual(CodeStubAssembler* assembler) {
  compiler::Node* lhs = assembler->Parameter(0);
  compiler::Node* rhs = assembler->Parameter(1);
  compiler::Node* context = assembler->Parameter(2);

  assembler->Return(assembler->RelationalComparison(
      CodeStubAssembler::kLessThanOrEqual, lhs, rhs, context));
}

void Builtins::Generate_GreaterThan(CodeStubAssembler* assembler) {
  compiler::Node* lhs = assembler->Parameter(0);
  compiler::Node* rhs = assembler->Parameter(1);
  compiler::Node* context = assembler->Parameter(2);

  assembler->Return(assembler->RelationalComparison(
      CodeStubAssembler::kGreaterThan, lhs, rhs, context));
}

void Builtins::Generate_GreaterThanOrEqual(CodeStubAssembler* assembler) {
  compiler::Node* lhs = assembler->Parameter(0);
  compiler::Node* rhs = assembler->Parameter(1);
  compiler::Node* context = assembler->Parameter(2);

  assembler->Return(assembler->RelationalComparison(
      CodeStubAssembler::kGreaterThanOrEqual, lhs, rhs, context));
}

void Builtins::Generate_Equal(CodeStubAssembler* assembler) {
  compiler::Node* lhs = assembler->Parameter(0);
  compiler::Node* rhs = assembler->Parameter(1);
  compiler::Node* context = assembler->Parameter(2);

  assembler->Return(assembler->Equal(CodeStubAssembler::kDontNegateResult, lhs,
                                     rhs, context));
}

void Builtins::Generate_NotEqual(CodeStubAssembler* assembler) {
  compiler::Node* lhs = assembler->Parameter(0);
  compiler::Node* rhs = assembler->Parameter(1);
  compiler::Node* context = assembler->Parameter(2);

  assembler->Return(
      assembler->Equal(CodeStubAssembler::kNegateResult, lhs, rhs, context));
}

void Builtins::Generate_StrictEqual(CodeStubAssembler* assembler) {
  compiler::Node* lhs = assembler->Parameter(0);
  compiler::Node* rhs = assembler->Parameter(1);
  compiler::Node* context = assembler->Parameter(2);

  assembler->Return(assembler->StrictEqual(CodeStubAssembler::kDontNegateResult,
                                           lhs, rhs, context));
}

void Builtins::Generate_StrictNotEqual(CodeStubAssembler* assembler) {
  compiler::Node* lhs = assembler->Parameter(0);
  compiler::Node* rhs = assembler->Parameter(1);
  compiler::Node* context = assembler->Parameter(2);

  assembler->Return(assembler->StrictEqual(CodeStubAssembler::kNegateResult,
                                           lhs, rhs, context));
}

}  // namespace internal
}  // namespace v8
