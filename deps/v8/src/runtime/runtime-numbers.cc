// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/arguments.h"
#include "src/base/bits.h"
#include "src/bootstrapper.h"
#include "src/codegen.h"
#include "src/runtime/runtime-utils.h"


#ifndef _STLP_VENDOR_CSTD
// STLPort doesn't import fpclassify and isless into the std namespace.
using std::fpclassify;
using std::isless;
#endif

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


static bool AreDigits(const uint8_t* s, int from, int to) {
  for (int i = from; i < to; i++) {
    if (s[i] < '0' || s[i] > '9') return false;
  }

  return true;
}


static int ParseDecimalInteger(const uint8_t* s, int from, int to) {
  DCHECK(to - from < 10);  // Overflow is not possible.
  DCHECK(from < to);
  int d = s[from] - '0';

  for (int i = from + 1; i < to; i++) {
    d = 10 * d + (s[i] - '0');
  }

  return d;
}


RUNTIME_FUNCTION(Runtime_StringToNumber) {
  HandleScope handle_scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(String, subject, 0);
  subject = String::Flatten(subject);

  // Fast case: short integer or some sorts of junk values.
  if (subject->IsSeqOneByteString()) {
    int len = subject->length();
    if (len == 0) return Smi::FromInt(0);

    DisallowHeapAllocation no_gc;
    uint8_t const* data = Handle<SeqOneByteString>::cast(subject)->GetChars();
    bool minus = (data[0] == '-');
    int start_pos = (minus ? 1 : 0);

    if (start_pos == len) {
      return isolate->heap()->nan_value();
    } else if (data[start_pos] > '9') {
      // Fast check for a junk value. A valid string may start from a
      // whitespace, a sign ('+' or '-'), the decimal point, a decimal digit
      // or the 'I' character ('Infinity'). All of that have codes not greater
      // than '9' except 'I' and &nbsp;.
      if (data[start_pos] != 'I' && data[start_pos] != 0xa0) {
        return isolate->heap()->nan_value();
      }
    } else if (len - start_pos < 10 && AreDigits(data, start_pos, len)) {
      // The maximal/minimal smi has 10 digits. If the string has less digits
      // we know it will fit into the smi-data type.
      int d = ParseDecimalInteger(data, start_pos, len);
      if (minus) {
        if (d == 0) return isolate->heap()->minus_zero_value();
        d = -d;
      } else if (!subject->HasHashCode() && len <= String::kMaxArrayIndexSize &&
                 (len == 1 || data[0] != '0')) {
        // String hash is not calculated yet but all the data are present.
        // Update the hash field to speed up sequential convertions.
        uint32_t hash = StringHasher::MakeArrayIndexHash(d, len);
#ifdef DEBUG
        subject->Hash();  // Force hash calculation.
        DCHECK_EQ(static_cast<int>(subject->hash_field()),
                  static_cast<int>(hash));
#endif
        subject->set_hash_field(hash);
      }
      return Smi::FromInt(d);
    }
  }

  // Slower case.
  int flags = ALLOW_HEX;
  if (FLAG_harmony_numeric_literals) {
    // The current spec draft has not updated "ToNumber Applied to the String
    // Type", https://bugs.ecmascript.org/show_bug.cgi?id=1584
    flags |= ALLOW_OCTAL | ALLOW_BINARY;
  }

  return *isolate->factory()->NewNumber(
      StringToDouble(isolate->unicode_cache(), subject, flags));
}


RUNTIME_FUNCTION(Runtime_StringParseInt) {
  HandleScope handle_scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(String, subject, 0);
  CONVERT_NUMBER_CHECKED(int, radix, Int32, args[1]);
  RUNTIME_ASSERT(radix == 0 || (2 <= radix && radix <= 36));

  subject = String::Flatten(subject);
  double value;

  {
    DisallowHeapAllocation no_gc;
    String::FlatContent flat = subject->GetFlatContent();

    // ECMA-262 section 15.1.2.3, empty string is NaN
    if (flat.IsOneByte()) {
      value =
          StringToInt(isolate->unicode_cache(), flat.ToOneByteVector(), radix);
    } else {
      value = StringToInt(isolate->unicode_cache(), flat.ToUC16Vector(), radix);
    }
  }

  return *isolate->factory()->NewNumber(value);
}


RUNTIME_FUNCTION(Runtime_StringParseFloat) {
  HandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(String, subject, 0);

  double value =
      StringToDouble(isolate->unicode_cache(), subject, ALLOW_TRAILING_JUNK,
                     std::numeric_limits<double>::quiet_NaN());

  return *isolate->factory()->NewNumber(value);
}


RUNTIME_FUNCTION(Runtime_NumberToStringRT) {
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


RUNTIME_FUNCTION(Runtime_NumberToInteger) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);

  CONVERT_DOUBLE_ARG_CHECKED(number, 0);
  return *isolate->factory()->NewNumber(DoubleToInteger(number));
}


RUNTIME_FUNCTION(Runtime_NumberToIntegerMapMinusZero) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);

  CONVERT_DOUBLE_ARG_CHECKED(number, 0);
  double double_value = DoubleToInteger(number);
  // Map both -0 and +0 to +0.
  if (double_value == 0) double_value = 0;

  return *isolate->factory()->NewNumber(double_value);
}


RUNTIME_FUNCTION(Runtime_NumberToJSUint32) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);

  CONVERT_NUMBER_CHECKED(int32_t, number, Uint32, args[0]);
  return *isolate->factory()->NewNumberFromUint(number);
}


RUNTIME_FUNCTION(Runtime_NumberToJSInt32) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);

  CONVERT_DOUBLE_ARG_CHECKED(number, 0);
  return *isolate->factory()->NewNumberFromInt(DoubleToInt32(number));
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


RUNTIME_FUNCTION(Runtime_NumberAdd) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  CONVERT_DOUBLE_ARG_CHECKED(y, 1);
  return *isolate->factory()->NewNumber(x + y);
}


RUNTIME_FUNCTION(Runtime_NumberSub) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  CONVERT_DOUBLE_ARG_CHECKED(y, 1);
  return *isolate->factory()->NewNumber(x - y);
}


RUNTIME_FUNCTION(Runtime_NumberMul) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  CONVERT_DOUBLE_ARG_CHECKED(y, 1);
  return *isolate->factory()->NewNumber(x * y);
}


RUNTIME_FUNCTION(Runtime_NumberUnaryMinus) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  return *isolate->factory()->NewNumber(-x);
}


RUNTIME_FUNCTION(Runtime_NumberDiv) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  CONVERT_DOUBLE_ARG_CHECKED(y, 1);
  return *isolate->factory()->NewNumber(x / y);
}


RUNTIME_FUNCTION(Runtime_NumberMod) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  CONVERT_DOUBLE_ARG_CHECKED(y, 1);
  return *isolate->factory()->NewNumber(modulo(x, y));
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


RUNTIME_FUNCTION(Runtime_NumberOr) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);

  CONVERT_NUMBER_CHECKED(int32_t, x, Int32, args[0]);
  CONVERT_NUMBER_CHECKED(int32_t, y, Int32, args[1]);
  return *isolate->factory()->NewNumberFromInt(x | y);
}


RUNTIME_FUNCTION(Runtime_NumberAnd) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);

  CONVERT_NUMBER_CHECKED(int32_t, x, Int32, args[0]);
  CONVERT_NUMBER_CHECKED(int32_t, y, Int32, args[1]);
  return *isolate->factory()->NewNumberFromInt(x & y);
}


RUNTIME_FUNCTION(Runtime_NumberXor) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);

  CONVERT_NUMBER_CHECKED(int32_t, x, Int32, args[0]);
  CONVERT_NUMBER_CHECKED(int32_t, y, Int32, args[1]);
  return *isolate->factory()->NewNumberFromInt(x ^ y);
}


RUNTIME_FUNCTION(Runtime_NumberShl) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);

  CONVERT_NUMBER_CHECKED(int32_t, x, Int32, args[0]);
  CONVERT_NUMBER_CHECKED(int32_t, y, Int32, args[1]);
  return *isolate->factory()->NewNumberFromInt(x << (y & 0x1f));
}


RUNTIME_FUNCTION(Runtime_NumberShr) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);

  CONVERT_NUMBER_CHECKED(uint32_t, x, Uint32, args[0]);
  CONVERT_NUMBER_CHECKED(int32_t, y, Int32, args[1]);
  return *isolate->factory()->NewNumberFromUint(x >> (y & 0x1f));
}


RUNTIME_FUNCTION(Runtime_NumberSar) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);

  CONVERT_NUMBER_CHECKED(int32_t, x, Int32, args[0]);
  CONVERT_NUMBER_CHECKED(int32_t, y, Int32, args[1]);
  return *isolate->factory()->NewNumberFromInt(
      ArithmeticShiftRight(x, y & 0x1f));
}


RUNTIME_FUNCTION(Runtime_NumberEquals) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 2);

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  CONVERT_DOUBLE_ARG_CHECKED(y, 1);
  if (std::isnan(x)) return Smi::FromInt(NOT_EQUAL);
  if (std::isnan(y)) return Smi::FromInt(NOT_EQUAL);
  if (x == y) return Smi::FromInt(EQUAL);
  Object* result;
  if ((fpclassify(x) == FP_ZERO) && (fpclassify(y) == FP_ZERO)) {
    result = Smi::FromInt(EQUAL);
  } else {
    result = Smi::FromInt(NOT_EQUAL);
  }
  return result;
}


RUNTIME_FUNCTION(Runtime_NumberCompare) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 3);

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  CONVERT_DOUBLE_ARG_CHECKED(y, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, uncomparable_result, 2)
  if (std::isnan(x) || std::isnan(y)) return *uncomparable_result;
  if (x == y) return Smi::FromInt(EQUAL);
  if (isless(x, y)) return Smi::FromInt(LESS);
  return Smi::FromInt(GREATER);
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


RUNTIME_FUNCTION(Runtime_GetRootNaN) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 0);
  RUNTIME_ASSERT(isolate->bootstrapper()->IsActive());
  return isolate->heap()->nan_value();
}


RUNTIME_FUNCTION(Runtime_MaxSmi) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 0);
  return Smi::FromInt(Smi::kMaxValue);
}


RUNTIME_FUNCTION(RuntimeReference_NumberToString) {
  SealHandleScope shs(isolate);
  return __RT_impl_Runtime_NumberToStringRT(args, isolate);
}


RUNTIME_FUNCTION(RuntimeReference_IsSmi) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  return isolate->heap()->ToBoolean(obj->IsSmi());
}


RUNTIME_FUNCTION(RuntimeReference_IsNonNegativeSmi) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  return isolate->heap()->ToBoolean(obj->IsSmi() &&
                                    Smi::cast(obj)->value() >= 0);
}
}
}  // namespace v8::internal
