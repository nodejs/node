// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/numbers/dtoa.h"

#include <cmath>

#include "src/base/logging.h"
#include "src/base/numbers/bignum-dtoa.h"
#include "src/base/numbers/double.h"
#include "src/base/numbers/fast-dtoa.h"
#include "src/base/numbers/fixed-dtoa.h"

namespace v8 {
namespace base {

static BignumDtoaMode DtoaToBignumDtoaMode(DtoaMode dtoa_mode) {
  switch (dtoa_mode) {
    case DTOA_SHORTEST:
      return BIGNUM_DTOA_SHORTEST;
    case DTOA_FIXED:
      return BIGNUM_DTOA_FIXED;
    case DTOA_PRECISION:
      return BIGNUM_DTOA_PRECISION;
    default:
      UNREACHABLE();
  }
}

void DoubleToAscii(double v, DtoaMode mode, int requested_digits,
                   Vector<char> buffer, int* sign, int* length, int* point) {
  DCHECK(!Double(v).IsSpecial());
  DCHECK(mode == DTOA_SHORTEST || requested_digits >= 0);

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
      fast_worked = FastDtoa(v, FAST_DTOA_PRECISION, requested_digits, buffer,
                             length, point);
      break;
    default:
      UNREACHABLE();
  }
  if (fast_worked) return;

  // If the fast dtoa didn't succeed use the slower bignum version.
  BignumDtoaMode bignum_mode = DtoaToBignumDtoaMode(mode);
  BignumDtoa(v, bignum_mode, requested_digits, buffer, length, point);
  buffer[*length] = '\0';
}

}  // namespace base
}  // namespace v8
