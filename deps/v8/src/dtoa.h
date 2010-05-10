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

#ifndef V8_DTOA_H_
#define V8_DTOA_H_

namespace v8 {
namespace internal {

enum DtoaMode {
  // 0.9999999999999999 becomes 0.1
  DTOA_SHORTEST,
  // Fixed number of digits after the decimal point.
  // For instance fixed(0.1, 4) becomes 0.1000
  // If the input number is big, the output will be big.
  DTOA_FIXED,
  // Fixed number of digits (independent of the decimal point).
  DTOA_PRECISION
};

// The maximal length of digits a double can have in base 10.
// Note that DoubleToAscii null-terminates its input. So the given buffer should
// be at least kBase10MaximalLength + 1 characters long.
static const int kBase10MaximalLength = 17;

// Converts the given double 'v' to ascii.
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
// and a terminating null-character.
bool DoubleToAscii(double v, DtoaMode mode, int requested_digits,
                   Vector<char> buffer, int* sign, int* length, int* point);

} }  // namespace v8::internal

#endif  // V8_DTOA_H_
