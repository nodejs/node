// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins.h"
#include "src/builtins/builtins-utils.h"

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
  assembler->GotoIf(assembler->WordIsSmi(number), &return_true);

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
  assembler->GotoIf(assembler->WordIsSmi(number), &return_true);

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
  assembler->BranchIfFloat64Equal(assembler->Float64Sub(number_value, integer),
                                  assembler->Float64Constant(0.0), &return_true,
                                  &return_false);

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
  assembler->GotoIf(assembler->WordIsSmi(number), &return_false);

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
  assembler->GotoIf(assembler->WordIsSmi(number), &return_true);

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
  assembler->BranchIfFloat64LessThanOrEqual(
      assembler->Float64Abs(integer),
      assembler->Float64Constant(kMaxSafeInteger), &return_true, &return_false);

  assembler->Bind(&return_true);
  assembler->Return(assembler->BooleanConstant(true));

  assembler->Bind(&return_false);
  assembler->Return(assembler->BooleanConstant(false));
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

}  // namespace internal
}  // namespace v8
