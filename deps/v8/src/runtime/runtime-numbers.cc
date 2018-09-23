// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/arguments-inl.h"
#include "src/base/bits.h"
#include "src/bootstrapper.h"
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

  double value = StringToDouble(isolate, isolate->unicode_cache(), subject,
                                ALLOW_TRAILING_JUNK,
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
RUNTIME_FUNCTION(Runtime_SmiLexicographicCompare) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_SMI_ARG_CHECKED(x_value, 0);
  CONVERT_SMI_ARG_CHECKED(y_value, 1);

  // If the integers are equal so are the string representations.
  if (x_value == y_value) return Smi::FromInt(0);

  // If one of the integers is zero the normal integer order is the
  // same as the lexicographic order of the string representations.
  if (x_value == 0 || y_value == 0)
    return Smi::FromInt(x_value < y_value ? -1 : 1);

  // If only one of the integers is negative the negative number is
  // smallest because the char code of '-' is less than the char code
  // of any digit.  Otherwise, we make both values positive.

  // Use unsigned values otherwise the logic is incorrect for -MIN_INT on
  // architectures using 32-bit Smis.
  uint32_t x_scaled = x_value;
  uint32_t y_scaled = y_value;
  if (x_value < 0 || y_value < 0) {
    if (y_value >= 0) return Smi::FromInt(-1);
    if (x_value >= 0) return Smi::FromInt(1);
    x_scaled = -x_value;
    y_scaled = -y_value;
  }

  static const uint32_t kPowersOf10[] = {
      1,                 10,                100,         1000,
      10 * 1000,         100 * 1000,        1000 * 1000, 10 * 1000 * 1000,
      100 * 1000 * 1000, 1000 * 1000 * 1000};

  // If the integers have the same number of decimal digits they can be
  // compared directly as the numeric order is the same as the
  // lexicographic order.  If one integer has fewer digits, it is scaled
  // by some power of 10 to have the same number of digits as the longer
  // integer.  If the scaled integers are equal it means the shorter
  // integer comes first in the lexicographic order.

  // From http://graphics.stanford.edu/~seander/bithacks.html#IntegerLog10
  int x_log2 = 31 - base::bits::CountLeadingZeros(x_scaled);
  int x_log10 = ((x_log2 + 1) * 1233) >> 12;
  x_log10 -= x_scaled < kPowersOf10[x_log10];

  int y_log2 = 31 - base::bits::CountLeadingZeros(y_scaled);
  int y_log10 = ((y_log2 + 1) * 1233) >> 12;
  y_log10 -= y_scaled < kPowersOf10[y_log10];

  int tie = 0;

  if (x_log10 < y_log10) {
    // X has fewer digits.  We would like to simply scale up X but that
    // might overflow, e.g when comparing 9 with 1_000_000_000, 9 would
    // be scaled up to 9_000_000_000. So we scale up by the next
    // smallest power and scale down Y to drop one digit. It is OK to
    // drop one digit from the longer integer since the final digit is
    // past the length of the shorter integer.
    x_scaled *= kPowersOf10[y_log10 - x_log10 - 1];
    y_scaled /= 10;
    tie = -1;
  } else if (y_log10 < x_log10) {
    y_scaled *= kPowersOf10[x_log10 - y_log10 - 1];
    x_scaled /= 10;
    tie = 1;
  }

  if (x_scaled < y_scaled) return Smi::FromInt(-1);
  if (x_scaled > y_scaled) return Smi::FromInt(1);
  return Smi::FromInt(tie);
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
