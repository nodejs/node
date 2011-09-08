// Copyright 2011 the V8 project authors. All rights reserved.
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

#include "../include/v8stdint.h"
#include "checks.h"
#include "utils.h"

#include "dtoa.h"

#include "bignum-dtoa.h"
#include "double.h"
#include "fast-dtoa.h"
#include "fixed-dtoa.h"

namespace v8 {
namespace internal {

static BignumDtoaMode DtoaToBignumDtoaMode(DtoaMode dtoa_mode) {
  switch (dtoa_mode) {
    case DTOA_SHORTEST:  return BIGNUM_DTOA_SHORTEST;
    case DTOA_FIXED:     return BIGNUM_DTOA_FIXED;
    case DTOA_PRECISION: return BIGNUM_DTOA_PRECISION;
    default:
      UNREACHABLE();
      return BIGNUM_DTOA_SHORTEST;  // To silence compiler.
  }
}


void DoubleToAscii(double v, DtoaMode mode, int requested_digits,
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
    return;
  }

  if (mode == DTOA_PRECISION && requested_digits == 0) {
    buffer[0] = '\0';
    *length = 0;
    return;
  }

  bool fast_worked;
  switch (mode) {
    case DTOA_SHORTEST:
      fast_worked = FastDtoa(v, FAST_DTOA_SHORTEST, 0, buffer, length, point);
      break;
    case DTOA_FIXED:
      fast_worked = FastFixedDtoa(v, requested_digits, buffer, length, point);
      break;
    case DTOA_PRECISION:
      fast_worked = FastDtoa(v, FAST_DTOA_PRECISION, requested_digits,
                             buffer, length, point);
      break;
    default:
      UNREACHABLE();
      fast_worked = false;
  }
  if (fast_worked) return;

  // If the fast dtoa didn't succeed use the slower bignum version.
  BignumDtoaMode bignum_mode = DtoaToBignumDtoaMode(mode);
  BignumDtoa(v, bignum_mode, requested_digits, buffer, length, point);
  buffer[*length] = '\0';
}

} }  // namespace v8::internal
