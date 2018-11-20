// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_FAST_DTOA_H_
#define V8_FAST_DTOA_H_

namespace v8 {
namespace internal {

enum FastDtoaMode {
  // Computes the shortest representation of the given input. The returned
  // result will be the most accurate number of this length. Longer
  // representations might be more accurate.
  FAST_DTOA_SHORTEST,
  // Computes a representation where the precision (number of digits) is
  // given as input. The precision is independent of the decimal point.
  FAST_DTOA_PRECISION
};

// FastDtoa will produce at most kFastDtoaMaximalLength digits. This does not
// include the terminating '\0' character.
const int kFastDtoaMaximalLength = 17;

// Provides a decimal representation of v.
// The result should be interpreted as buffer * 10^(point - length).
//
// Precondition:
//   * v must be a strictly positive finite double.
//
// Returns true if it succeeds, otherwise the result can not be trusted.
// There will be *length digits inside the buffer followed by a null terminator.
// If the function returns true and mode equals
//   - FAST_DTOA_SHORTEST, then
//     the parameter requested_digits is ignored.
//     The result satisfies
//         v == (double) (buffer * 10^(point - length)).
//     The digits in the buffer are the shortest representation possible. E.g.
//     if 0.099999999999 and 0.1 represent the same double then "1" is returned
//     with point = 0.
//     The last digit will be closest to the actual v. That is, even if several
//     digits might correctly yield 'v' when read again, the buffer will contain
//     the one closest to v.
//   - FAST_DTOA_PRECISION, then
//     the buffer contains requested_digits digits.
//     the difference v - (buffer * 10^(point-length)) is closest to zero for
//     all possible representations of requested_digits digits.
//     If there are two values that are equally close, then FastDtoa returns
//     false.
// For both modes the buffer must be large enough to hold the result.
bool FastDtoa(double d,
              FastDtoaMode mode,
              int requested_digits,
              Vector<char> buffer,
              int* length,
              int* decimal_point);

} }  // namespace v8::internal

#endif  // V8_FAST_DTOA_H_
