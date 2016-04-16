// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/arguments.h"
#include "src/base/bits.h"
#include "src/bootstrapper.h"
#include "src/codegen.h"
#include "src/isolate-inl.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_NumberToRadixString) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_SMI_ARG_CHECKED(radix, 1);
  RUNTIME_ASSERT(2 <= radix && radix <= 36);

  // Fast case where the result is a one character string.
  if (args[0]->IsSmi()) {
    int value = args.smi_at(0);
    if (value >= 0 && value < radix) {
      // Character array used for conversion.
      static const char kCharTable[] = "0123456789abcdefghijklmnopqrstuvwxyz";
      return *isolate->factory()->LookupSingleCharacterStringFromCode(
          kCharTable[value]);
    }
  }

  // Slow case.
  CONVERT_DOUBLE_ARG_CHECKED(value, 0);
  if (std::isnan(value)) {
    return isolate->heap()->nan_string();
  }
  if (std::isinf(value)) {
    if (value < 0) {
      return isolate->heap()->minus_infinity_string();
    }
    return isolate->heap()->infinity_string();
  }
  char* str = DoubleToRadixCString(value, radix);
  Handle<String> result = isolate->factory()->NewStringFromAsciiChecked(str);
  DeleteArray(str);
  return *result;
}


RUNTIME_FUNCTION(Runtime_NumberToFixed) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);

  CONVERT_DOUBLE_ARG_CHECKED(value, 0);
  CONVERT_DOUBLE_ARG_CHECKED(f_number, 1);
  int f = FastD2IChecked(f_number);
  // See DoubleToFixedCString for these constants:
  RUNTIME_ASSERT(f >= 0 && f <= 20);
  RUNTIME_ASSERT(!Double(value).IsSpecial());
  char* str = DoubleToFixedCString(value, f);
  Handle<String> result = isolate->factory()->NewStringFromAsciiChecked(str);
  DeleteArray(str);
  return *result;
}


RUNTIME_FUNCTION(Runtime_NumberToExponential) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);

  CONVERT_DOUBLE_ARG_CHECKED(value, 0);
  CONVERT_DOUBLE_ARG_CHECKED(f_number, 1);
  int f = FastD2IChecked(f_number);
  RUNTIME_ASSERT(f >= -1 && f <= 20);
  RUNTIME_ASSERT(!Double(value).IsSpecial());
  char* str = DoubleToExponentialCString(value, f);
  Handle<String> result = isolate->factory()->NewStringFromAsciiChecked(str);
  DeleteArray(str);
  return *result;
}


RUNTIME_FUNCTION(Runtime_NumberToPrecision) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);

  CONVERT_DOUBLE_ARG_CHECKED(value, 0);
  CONVERT_DOUBLE_ARG_CHECKED(f_number, 1);
  int f = FastD2IChecked(f_number);
  RUNTIME_ASSERT(f >= 1 && f <= 21);
  RUNTIME_ASSERT(!Double(value).IsSpecial());
  char* str = DoubleToPrecisionCString(value, f);
  Handle<String> result = isolate->factory()->NewStringFromAsciiChecked(str);
  DeleteArray(str);
  return *result;
}


RUNTIME_FUNCTION(Runtime_IsValidSmi) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);

  CONVERT_NUMBER_CHECKED(int32_t, number, Int32, args[0]);
  return isolate->heap()->ToBoolean(Smi::IsValid(number));
}


RUNTIME_FUNCTION(Runtime_StringToNumber) {
  HandleScope handle_scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, subject, 0);
  return *String::ToNumber(subject);
}


// ES6 18.2.5 parseInt(string, radix) slow path
RUNTIME_FUNCTION(Runtime_StringParseInt) {
  HandleScope handle_scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(String, subject, 0);
  CONVERT_NUMBER_CHECKED(int, radix, Int32, args[1]);
  // Step 8.a. is already handled in the JS function.
  RUNTIME_ASSERT(radix == 0 || (2 <= radix && radix <= 36));

  subject = String::Flatten(subject);
  double value;

  {
    DisallowHeapAllocation no_gc;
    String::FlatContent flat = subject->GetFlatContent();

    if (flat.IsOneByte()) {
      value =
          StringToInt(isolate->unicode_cache(), flat.ToOneByteVector(), radix);
    } else {
      value = StringToInt(isolate->unicode_cache(), flat.ToUC16Vector(), radix);
    }
  }

  return *isolate->factory()->NewNumber(value);
}


// ES6 18.2.4 parseFloat(string)
RUNTIME_FUNCTION(Runtime_StringParseFloat) {
  HandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(String, subject, 0);

  double value =
      StringToDouble(isolate->unicode_cache(), subject, ALLOW_TRAILING_JUNK,
                     std::numeric_limits<double>::quiet_NaN());

  return *isolate->factory()->NewNumber(value);
}


RUNTIME_FUNCTION(Runtime_NumberToString) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_NUMBER_ARG_HANDLE_CHECKED(number, 0);

  return *isolate->factory()->NumberToString(number);
}


RUNTIME_FUNCTION(Runtime_NumberToStringSkipCache) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_NUMBER_ARG_HANDLE_CHECKED(number, 0);

  return *isolate->factory()->NumberToString(number, false);
}


// TODO(bmeurer): Kill this runtime entry. Uses in date.js are wrong anyway.
RUNTIME_FUNCTION(Runtime_NumberToIntegerMapMinusZero) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, input, 0);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, input, Object::ToNumber(input));
  double double_value = DoubleToInteger(input->Number());
  // Map both -0 and +0 to +0.
  if (double_value == 0) double_value = 0;

  return *isolate->factory()->NewNumber(double_value);
}


// Converts a Number to a Smi, if possible. Returns NaN if the number is not
// a small integer.
RUNTIME_FUNCTION(Runtime_NumberToSmi) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  if (obj->IsSmi()) {
    return obj;
  }
  if (obj->IsHeapNumber()) {
    double value = HeapNumber::cast(obj)->value();
    int int_value = FastD2I(value);
    if (value == FastI2D(int_value) && Smi::IsValid(int_value)) {
      return Smi::FromInt(int_value);
    }
  }
  return isolate->heap()->nan_value();
}


RUNTIME_FUNCTION(Runtime_NumberImul) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);

  // We rely on implementation-defined behavior below, but at least not on
  // undefined behavior.
  CONVERT_NUMBER_CHECKED(uint32_t, x, Int32, args[0]);
  CONVERT_NUMBER_CHECKED(uint32_t, y, Int32, args[1]);
  int32_t product = static_cast<int32_t>(x * y);
  return *isolate->factory()->NewNumberFromInt(product);
}


// Compare two Smis as if they were converted to strings and then
// compared lexicographically.
RUNTIME_FUNCTION(Runtime_SmiLexicographicCompare) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 2);
  CONVERT_SMI_ARG_CHECKED(x_value, 0);
  CONVERT_SMI_ARG_CHECKED(y_value, 1);

  // If the integers are equal so are the string representations.
  if (x_value == y_value) return Smi::FromInt(EQUAL);

  // If one of the integers is zero the normal integer order is the
  // same as the lexicographic order of the string representations.
  if (x_value == 0 || y_value == 0)
    return Smi::FromInt(x_value < y_value ? LESS : GREATER);

  // If only one of the integers is negative the negative number is
  // smallest because the char code of '-' is less than the char code
  // of any digit.  Otherwise, we make both values positive.

  // Use unsigned values otherwise the logic is incorrect for -MIN_INT on
  // architectures using 32-bit Smis.
  uint32_t x_scaled = x_value;
  uint32_t y_scaled = y_value;
  if (x_value < 0 || y_value < 0) {
    if (y_value >= 0) return Smi::FromInt(LESS);
    if (x_value >= 0) return Smi::FromInt(GREATER);
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
  int x_log2 = 31 - base::bits::CountLeadingZeros32(x_scaled);
  int x_log10 = ((x_log2 + 1) * 1233) >> 12;
  x_log10 -= x_scaled < kPowersOf10[x_log10];

  int y_log2 = 31 - base::bits::CountLeadingZeros32(y_scaled);
  int y_log10 = ((y_log2 + 1) * 1233) >> 12;
  y_log10 -= y_scaled < kPowersOf10[y_log10];

  int tie = EQUAL;

  if (x_log10 < y_log10) {
    // X has fewer digits.  We would like to simply scale up X but that
    // might overflow, e.g when comparing 9 with 1_000_000_000, 9 would
    // be scaled up to 9_000_000_000. So we scale up by the next
    // smallest power and scale down Y to drop one digit. It is OK to
    // drop one digit from the longer integer since the final digit is
    // past the length of the shorter integer.
    x_scaled *= kPowersOf10[y_log10 - x_log10 - 1];
    y_scaled /= 10;
    tie = LESS;
  } else if (y_log10 < x_log10) {
    y_scaled *= kPowersOf10[x_log10 - y_log10 - 1];
    x_scaled /= 10;
    tie = GREATER;
  }

  if (x_scaled < y_scaled) return Smi::FromInt(LESS);
  if (x_scaled > y_scaled) return Smi::FromInt(GREATER);
  return Smi::FromInt(tie);
}


RUNTIME_FUNCTION(Runtime_MaxSmi) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 0);
  return Smi::FromInt(Smi::kMaxValue);
}


RUNTIME_FUNCTION(Runtime_IsSmi) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  return isolate->heap()->ToBoolean(obj->IsSmi());
}


RUNTIME_FUNCTION(Runtime_GetRootNaN) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 0);
  return isolate->heap()->nan_value();
}


RUNTIME_FUNCTION(Runtime_GetHoleNaNUpper) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 0);
  return *isolate->factory()->NewNumberFromUint(kHoleNanUpper32);
}


RUNTIME_FUNCTION(Runtime_GetHoleNaNLower) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 0);
  return *isolate->factory()->NewNumberFromUint(kHoleNanLower32);
}


}  // namespace internal
}  // namespace v8
