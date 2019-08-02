// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_NUMBERS_DTOA_H_
#define V8_NUMBERS_DTOA_H_

#include "src/utils/vector.h"

namespace v8 {
namespace internal {

enum DtoaMode {
  // Return the shortest correct representation.
  // For example the output of 0.299999999999999988897 is (the less accurate but
  // correct) 0.3.
  DTOA_SHORTEST,
  // Return a fixed number of digits after the decimal point.
  // For instance fixed(0.1, 4) becomes 0.1000
  // If the input number is big, the output will be big.
  DTOA_FIXED,
  // Return a fixed number of digits, no matter what the exponent is.
  DTOA_PRECISION
};

// The maximal length of digits a double can have in base 10.
// Note that DoubleToAscii null-terminates its input. So the given buffer should
// be at least kBase10MaximalLength + 1 characters long.
const int kBase10MaximalLength = 17;

// Converts the given double 'v' to ASCII.
// The result should be interpreted as buffer * 10^(point-length).
//
// The output depends on the given mode:
//  - SHORTEST: produce the least amount of digits for which the internal
//   identity requirement is still satisfied. If the digits are printed
//   (together with the correct exponent) then reading this number will give
//   'v' again. The buffer will choose the representation that is closest to
//   'v'. If there are two at the same distance, than the one farther away
//   from 0 is chosen (halfway cases - ending with 5 - are rounded up).
//   In this mode the 'requested_digits' parameter is ignored.
//  - FIXED: produces digits necessary to print a given number with
//   'requested_digits' digits after the decimal point. The produced digits
//   might be too short in which case the caller has to fill the gaps with '0's.
//   Example: toFixed(0.001, 5) is allowed to return buffer="1", point=-2.
//   Halfway cases are rounded towards +/-Infinity (away from 0). The call
//   toFixed(0.15, 2) thus returns buffer="2", point=0.
//   The returned buffer may contain digits that would be truncated from the
//   shortest representation of the input.
//  - PRECISION: produces 'requested_digits' where the first digit is not '0'.
//   Even though the length of produced digits usually equals
//   'requested_digits', the function is allowed to return fewer digits, in
//   which case the caller has to fill the missing digits with '0's.
//   Halfway cases are again rounded away from 0.
// 'DoubleToAscii' expects the given buffer to be big enough to hold all digits
// and a terminating null-character. In SHORTEST-mode it expects a buffer of
// at least kBase10MaximalLength + 1. Otherwise, the size of the output is
// limited to requested_digits digits plus the null terminator.
V8_EXPORT_PRIVATE void DoubleToAscii(double v, DtoaMode mode,
                                     int requested_digits, Vector<char> buffer,
                                     int* sign, int* length, int* point);

}  // namespace internal
}  // namespace v8

#endif  // V8_NUMBERS_DTOA_H_
