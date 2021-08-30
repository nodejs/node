// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_NUMBERS_FIXED_DTOA_H_
#define V8_BASE_NUMBERS_FIXED_DTOA_H_

#include "src/base/vector.h"

namespace v8 {
namespace base {

// Produces digits necessary to print a given number with
// 'fractional_count' digits after the decimal point.
// The buffer must be big enough to hold the result plus one terminating null
// character.
//
// The produced digits might be too short in which case the caller has to fill
// the gaps with '0's.
// Example: FastFixedDtoa(0.001, 5, ...) is allowed to return buffer = "1", and
// decimal_point = -2.
// Halfway cases are rounded towards +/-Infinity (away from 0). The call
// FastFixedDtoa(0.15, 2, ...) thus returns buffer = "2", decimal_point = 0.
// The returned buffer may contain digits that would be truncated from the
// shortest representation of the input.
//
// This method only works for some parameters. If it can't handle the input it
// returns false. The output is null-terminated when the function succeeds.
V8_BASE_EXPORT bool FastFixedDtoa(double v, int fractional_count,
                                  Vector<char> buffer, int* length,
                                  int* decimal_point);

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_NUMBERS_FIXED_DTOA_H_
