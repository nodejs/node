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

#include <math.h>

#include "v8.h"
#include "dtoa.h"

#include "double.h"
#include "fast-dtoa.h"
#include "fixed-dtoa.h"

namespace v8 {
namespace internal {

bool DoubleToAscii(double v, DtoaMode mode, int requested_digits,
                   Vector<char> buffer, int* sign, int* length, int* point) {
  ASSERT(!Double(v).IsSpecial());
  ASSERT(mode == DTOA_SHORTEST || requested_digits >= 0);

  if (Double(v).Sign() < 0) {
    *sign = 1;
    v = -v;
  } else {
    *sign = 0;
  }

  if (v == 0) {
    buffer[0] = '0';
    buffer[1] = '\0';
    *length = 1;
    *point = 1;
    return true;
  }

  if (mode == DTOA_PRECISION && requested_digits == 0) {
    buffer[0] = '\0';
    *length = 0;
    return true;
  }

  switch (mode) {
    case DTOA_SHORTEST:
      return FastDtoa(v, buffer, length, point);
    case DTOA_FIXED:
      return FastFixedDtoa(v, requested_digits, buffer, length, point);
    default:
      break;
  }
  return false;
}

} }  // namespace v8::internal
