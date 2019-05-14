// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/arguments-inl.h"
#include "src/base/bits.h"
#include "src/bootstrapper.h"
#include "src/counters.h"
#include "src/heap/heap-inl.h"  // For ToBoolean. TODO(jkummerow): Drop.
#include "src/isolate-inl.h"
#include "src/runtime/runtime-utils.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_IsValidSmi) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());

  CONVERT_NUMBER_CHECKED(int32_t, number, Int32, args[0]);
  return isolate->heap()->ToBoolean(Smi::IsValid(number));
}


RUNTIME_FUNCTION(Runtime_StringToNumber) {
  HandleScope handle_scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, subject, 0);
  return *String::ToNumber(isolate, subject);
}


// ES6 18.2.5 parseInt(string, radix) slow path
RUNTIME_FUNCTION(Runtime_StringParseInt) {
  HandleScope handle_scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, string, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, radix, 1);

  // Convert {string} to a String first, and flatten it.
  Handle<String> subject;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, subject,
                                     Object::ToString(isolate, string));
  subject = String::Flatten(isolate, subject);

  // Convert {radix} to Int32.
  if (!radix->IsNumber()) {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, radix,
                                       Object::ToNumber(isolate, radix));
  }
  int radix32 = DoubleToInt32(radix->Number());
  if (radix32 != 0 && (radix32 < 2 || radix32 > 36)) {
    return ReadOnlyRoots(isolate).nan_value();
  }

  double result = StringToInt(isolate, subject, radix32);
  return *isolate->factory()->NewNumber(result);
}


// ES6 18.2.4 parseFloat(string)
RUNTIME_FUNCTION(Runtime_StringParseFloat) {
  HandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, subject, 0);

  double value = StringToDouble(isolate, subject, ALLOW_TRAILING_JUNK,
                                std::numeric_limits<double>::quiet_NaN());

  return *isolate->factory()->NewNumber(value);
}

RUNTIME_FUNCTION(Runtime_NumberToString) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_NUMBER_ARG_HANDLE_CHECKED(number, 0);

  return *isolate->factory()->NumberToString(number);
}

// Compare two Smis x, y as if they were converted to strings and then
// compared lexicographically. Returns:
// -1 if x < y
//  0 if x == y
//  1 if x > y
// TODO(szuend): Remove once the call-site in src/js/array.js is gone.
RUNTIME_FUNCTION(Runtime_SmiLexicographicCompare) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_CHECKED(Smi, x_value, 0);
  CONVERT_ARG_CHECKED(Smi, y_value, 1);

  return Object(Smi::LexicographicCompare(isolate, x_value, y_value));
}

RUNTIME_FUNCTION(Runtime_MaxSmi) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  return Smi::FromInt(Smi::kMaxValue);
}


RUNTIME_FUNCTION(Runtime_IsSmi) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(Object, obj, 0);
  return isolate->heap()->ToBoolean(obj->IsSmi());
}


RUNTIME_FUNCTION(Runtime_GetHoleNaNUpper) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  return *isolate->factory()->NewNumberFromUint(kHoleNanUpper32);
}


RUNTIME_FUNCTION(Runtime_GetHoleNaNLower) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  return *isolate->factory()->NewNumberFromUint(kHoleNanLower32);
}

}  // namespace internal
}  // namespace v8
