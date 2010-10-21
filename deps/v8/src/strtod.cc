// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <stdarg.h>
#include <limits.h>

#include "v8.h"

#include "strtod.h"
// #include "cached-powers.h"

namespace v8 {
namespace internal {

// 2^53 = 9007199254740992.
// Any integer with at most 15 decimal digits will hence fit into a double
// (which has a 53bit significand) without loss of precision.
static const int kMaxExactDoubleIntegerDecimalDigits = 15;
// 2^64 = 18446744073709551616
// Any integer with at most 19 digits will hence fit into a 64bit datatype.
static const int kMaxUint64DecimalDigits = 19;
// Max double: 1.7976931348623157 x 10^308
// Min non-zero double: 4.9406564584124654 x 10^-324
// Any x >= 10^309 is interpreted as +infinity.
// Any x <= 10^-324 is interpreted as 0.
// Note that 2.5e-324 (despite being smaller than the min double) will be read
// as non-zero (equal to the min non-zero double).
static const int kMaxDecimalPower = 309;
static const int kMinDecimalPower = -324;

static const double exact_powers_of_ten[] = {
  1.0,  // 10^0
  10.0,
  100.0,
  1000.0,
  10000.0,
  100000.0,
  1000000.0,
  10000000.0,
  100000000.0,
  1000000000.0,
  10000000000.0,  // 10^10
  100000000000.0,
  1000000000000.0,
  10000000000000.0,
  100000000000000.0,
  1000000000000000.0,
  10000000000000000.0,
  100000000000000000.0,
  1000000000000000000.0,
  10000000000000000000.0,
  100000000000000000000.0,  // 10^20
  1000000000000000000000.0,
  // 10^22 = 0x21e19e0c9bab2400000 = 0x878678326eac9 * 2^22
  10000000000000000000000.0
};

static const int kExactPowersOfTenSize = ARRAY_SIZE(exact_powers_of_ten);


extern "C" double gay_strtod(const char* s00, const char** se);

static double old_strtod(Vector<const char> buffer, int exponent) {
  // gay_strtod is broken on Linux,x86. For numbers with few decimal digits
  // the computation is done using floating-point operations which (on Linux)
  // are prone to double-rounding errors.
  // By adding several zeroes to the buffer gay_strtod falls back to a slower
  // (but correct) algorithm.
  const int kInsertedZeroesCount = 20;
  char gay_buffer[1024];
  Vector<char> gay_buffer_vector(gay_buffer, sizeof(gay_buffer));
  int pos = 0;
  for (int i = 0; i < buffer.length(); ++i) {
    gay_buffer_vector[pos++] = buffer[i];
  }
  for (int i = 0; i < kInsertedZeroesCount; ++i) {
    gay_buffer_vector[pos++] = '0';
  }
  exponent -= kInsertedZeroesCount;
  gay_buffer_vector[pos++] = 'e';
  if (exponent < 0) {
    gay_buffer_vector[pos++] = '-';
    exponent = -exponent;
  }
  const int kNumberOfExponentDigits = 5;
  for (int i = kNumberOfExponentDigits - 1; i >= 0; i--) {
    gay_buffer_vector[pos + i] = exponent % 10 + '0';
    exponent /= 10;
  }
  pos += kNumberOfExponentDigits;
  gay_buffer_vector[pos] = '\0';
  return gay_strtod(gay_buffer, NULL);
}


static Vector<const char> TrimLeadingZeros(Vector<const char> buffer) {
  for (int i = 0; i < buffer.length(); i++) {
    if (buffer[i] != '0') {
      return Vector<const char>(buffer.start() + i, buffer.length() - i);
    }
  }
  return Vector<const char>(buffer.start(), 0);
}


static Vector<const char> TrimTrailingZeros(Vector<const char> buffer) {
  for (int i = buffer.length() - 1; i >= 0; --i) {
    if (buffer[i] != '0') {
      return Vector<const char>(buffer.start(), i + 1);
    }
  }
  return Vector<const char>(buffer.start(), 0);
}


uint64_t ReadUint64(Vector<const char> buffer) {
  ASSERT(buffer.length() <= kMaxUint64DecimalDigits);
  uint64_t result = 0;
  for (int i = 0; i < buffer.length(); ++i) {
    int digit = buffer[i] - '0';
    ASSERT(0 <= digit && digit <= 9);
    result = 10 * result + digit;
  }
  return result;
}


static bool DoubleStrtod(Vector<const char> trimmed,
                         int exponent,
                         double* result) {
#if (defined(V8_TARGET_ARCH_IA32) || defined(USE_SIMULATOR)) && !defined(WIN32)
  // On x86 the floating-point stack can be 64 or 80 bits wide. If it is
  // 80 bits wide (as is the case on Linux) then double-rounding occurs and the
  // result is not accurate.
  // We know that Windows32 uses 64 bits and is therefore accurate.
  // Note that the ARM simulator is compiled for 32bits. It therefore exhibits
  // the same problem.
  return false;
#endif
  if (trimmed.length() <= kMaxExactDoubleIntegerDecimalDigits) {
    // The trimmed input fits into a double.
    // If the 10^exponent (resp. 10^-exponent) fits into a double too then we
    // can compute the result-double simply by multiplying (resp. dividing) the
    // two numbers.
    // This is possible because IEEE guarantees that floating-point operations
    // return the best possible approximation.
    if (exponent < 0 && -exponent < kExactPowersOfTenSize) {
      // 10^-exponent fits into a double.
      *result = static_cast<double>(ReadUint64(trimmed));
      *result /= exact_powers_of_ten[-exponent];
      return true;
    }
    if (0 <= exponent && exponent < kExactPowersOfTenSize) {
      // 10^exponent fits into a double.
      *result = static_cast<double>(ReadUint64(trimmed));
      *result *= exact_powers_of_ten[exponent];
      return true;
    }
    int remaining_digits =
        kMaxExactDoubleIntegerDecimalDigits - trimmed.length();
    if ((0 <= exponent) &&
        (exponent - remaining_digits < kExactPowersOfTenSize)) {
      // The trimmed string was short and we can multiply it with
      // 10^remaining_digits. As a result the remaining exponent now fits
      // into a double too.
      *result = static_cast<double>(ReadUint64(trimmed));
      *result *= exact_powers_of_ten[remaining_digits];
      *result *= exact_powers_of_ten[exponent - remaining_digits];
      return true;
    }
  }
  return false;
}


double Strtod(Vector<const char> buffer, int exponent) {
  Vector<const char> left_trimmed = TrimLeadingZeros(buffer);
  Vector<const char> trimmed = TrimTrailingZeros(left_trimmed);
  exponent += left_trimmed.length() - trimmed.length();
  if (trimmed.length() == 0) return 0.0;
  if (exponent + trimmed.length() - 1 >= kMaxDecimalPower) return V8_INFINITY;
  if (exponent + trimmed.length() <= kMinDecimalPower) return 0.0;
  double result;
  if (DoubleStrtod(trimmed, exponent, &result)) {
    return result;
  }
  return old_strtod(trimmed, exponent);
}

} }  // namespace v8::internal
