// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Adapted from xsum.c and xsum.h by Radford M. Neal.
// https://gitlab.com/radfordneal/xsum
//
// Original license header:
//
// Copyright 2015, 2018, 2021 Radford M. Neal
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "src/builtins/builtins-math-xsum.h"

#include "src/base/bit-field.h"

namespace v8 {
namespace internal {

Xsum::Xsum() {
  chunk_.fill(0);
}

void Xsum::AddInfNan(int64_t ivalue) {
  int64_t mantissa = ivalue & kMantissaMask;

  if (mantissa == 0) {  // Inf
    if (inf_ == 0) {
      // no previous Inf
      inf_ = ivalue;
    } else if (inf_ != ivalue) {
      // previous Inf was opposite sign
      double fltv = base::bit_cast<double>(ivalue);
      fltv = fltv - fltv;  // result will be a NaN
      inf_ = base::bit_cast<int64_t>(fltv);
      inf_sign_change_ = true;
    }
  } else {  // NaN
    // Choose the NaN with the bigger payload and clear its sign.  Using <=
    // ensures that we will choose the first NaN over the previous zero.
    if ((nan_ & kMantissaMask) <= mantissa) {
      nan_ = ivalue & ~kSignMask;
    }
  }
}

int Xsum::CarryPropagate() {
  int u = kSchunks - 1;

  // Search for the uppermost non-zero chunk.
  // Unrolling the check for the first few chunks (66, 65, 64)
  // so that the loop can handle groups of 4 starting from 63.
  static_assert(kSchunks == 67, "Logic assumes kSchunks is 67");

  do {
    if (chunk_[u] != 0) break;
    u--;
    if (chunk_[u] != 0) break;
    u--;
    if (chunk_[u] != 0) break;
    u--;
    // Now u is 63.

    // Search downwards in groups of 4.
    while (u >= 0) {
      // Here, u should be a multiple of 4 minus one, and at least 3.
      if (chunk_[u] | chunk_[u - 1] | chunk_[u - 2] | chunk_[u - 3]) {
        break;
      }
      u -= 4;
    }

    // Number is zero.
    if (u < 0) {
      adds_until_propagate_ = kSmallCarryTerms - 1;
      return 0;
    }

    DCHECK_EQ(u % 4, 3);
    DCHECK_GE(u, 3);
    if (chunk_[u] != 0) break;
    u--;
    if (chunk_[u] != 0) break;
    u--;
    if (chunk_[u] != 0) break;
    u--;
  } while (false);

  DCHECK_NE(chunk_[u], 0);

  // Propagate carries.
  // i is the index of the next non-zero chunk from the bottom.
  int i = 0;

  // Quickly skip over unused low-order chunks.
  int limit = u - 3;
  while (i <= limit) {
    if (chunk_[i] | chunk_[i + 1] | chunk_[i + 2] | chunk_[i + 3]) {
      break;
    }
    i += 4;
  }

  int uix = -1;  // Index of uppermost non-zero chunk found so far.

  do {
    int64_t c = chunk_[i];
    if (c == 0) {
      i++;
      // Find the next non-zero chunk.
      // We know u is the upper bound, but it might have changed to 0.
      while (i <= u && chunk_[i] == 0) {
        i++;
      }
      if (i > u) break;
      c = chunk_[i];
    }

    int64_t chigh = c >> kLowMantissaBits;
    if (chigh == 0) {
      uix = i;
      i++;
      continue;
    }

    if (u == i) {
      if (chigh == -1) {
        uix = i;
        break;  // Don't propagate -1 into the region of all zeros above.
      }
      u = i + 1;
    }

    int64_t clow = c & kLowMantissaMask;
    if (clow != 0) {
      uix = i;
    }

    chunk_[i] = clow;
    if (i + 1 >= kSchunks) {
      AddInfNan((int64_t{kExpMask} << kMantissaBits) | kMantissaMask);
      u = i;
    } else {
      chunk_[i + 1] += chigh;
    }

    i++;
  } while (i <= u);

  if (uix < 0) {
    adds_until_propagate_ = kSmallCarryTerms - 1;
    return 0;
  }

  // While the uppermost chunk is negative, with value -1, combine it with
  // the chunk below.
  while (chunk_[uix] == -1 && uix > 0) {
    chunk_[uix - 1] += (int64_t{-1}) * (int64_t{1} << kLowMantissaBits);
    chunk_[uix] = 0;
    uix--;
  }

  adds_until_propagate_ = kSmallCarryTerms - 1;
  return uix;
}

double Xsum::Round() {
  if (nan_ != 0) {
    return base::bit_cast<double>(nan_);
  }
  if (inf_ != 0) {
    return base::bit_cast<double>(inf_);
  }

  int i = CarryPropagate();
  int64_t ivalue = chunk_[i];

  // Handle a possible denormalized number, including zero.
  if (i <= 1) {
    if (ivalue == 0) return 0.0;

    int64_t intv;
    if (i == 0) {
      intv = ivalue >= 0 ? ivalue : -ivalue;
      intv >>= 1;
      if (ivalue < 0) intv |= kSignMask;
      return base::bit_cast<double>(intv);
    } else {
      intv = ivalue * (int64_t{1} << (kLowMantissaBits - 1)) + (chunk_[0] >> 1);
      if (intv < 0) {
        if (intv > -(int64_t{1} << kMantissaBits)) {
          intv = (-intv) | kSignMask;
          return base::bit_cast<double>(intv);
        }
      } else {
        if (static_cast<uint64_t>(intv) < (uint64_t{1} << kMantissaBits)) {
          return base::bit_cast<double>(intv);
        }
      }
    }
  }

  double fltv = static_cast<double>(ivalue);
  int64_t intv = base::bit_cast<int64_t>(fltv);
  int e = static_cast<int>((intv >> kMantissaBits) & kExpMask);
  int more = 2 + kMantissaBits + static_cast<int>(kExpBias) - e;

  ivalue *= (int64_t{1} << more);
  int j = i - 1;
  int64_t lower = chunk_[j];
  if (more >= kLowMantissaBits) {
    more -= kLowMantissaBits;
    ivalue += lower << more;
    j--;
    lower = j < 0 ? 0 : chunk_[j];
  }
  ivalue += lower >> (kLowMantissaBits - more);
  lower &= (int64_t{1} << (kLowMantissaBits - more)) - 1;

  bool round_away = false;
  if (ivalue >= 0) {
    intv = 0;
    if ((ivalue & 2) != 0) {
      if ((ivalue & 1) != 0) {
        round_away = true;
      } else if ((ivalue & 4) != 0) {
        round_away = true;
      } else {
        if (lower == 0) {
          while (j > 0) {
            j--;
            if (chunk_[j] != 0) {
              lower = 1;
              break;
            }
          }
        }
        if (lower != 0) round_away = true;
      }
    }
  } else {
    if (((-ivalue) & (int64_t{1} << (kMantissaBits + 2))) == 0) {
      int64_t pos = int64_t{1} << (kLowMantissaBits - 1 - more);
      ivalue *= 2;
      if (lower & pos) {
        ivalue += 1;
        lower &= ~pos;
      }
      e -= 1;
    }

    intv = kSignMask;
    ivalue = -ivalue;

    if ((ivalue & 3) == 3) {
      round_away = true;
    } else if ((ivalue & 3) > 1 && (ivalue & 4) != 0) {
      if (lower == 0) {
        while (j > 0) {
          j--;
          if (chunk_[j] != 0) {
            lower = 1;
            break;
          }
        }
      }
      if (lower == 0) round_away = true;
    }
  }

  if (round_away) {
    ivalue += 4;
    if (ivalue & (int64_t{1} << (kMantissaBits + 3))) {
      ivalue >>= 1;
      e += 1;
    }
  }

  ivalue >>= 2;
  e += (i << kLowExpBits) - static_cast<int>(kExpBias) - kMantissaBits;

  if (e >= kExpMask) {
    intv |= int64_t{kExpMask} << kMantissaBits;
    return base::bit_cast<double>(intv);
  }

  intv += static_cast<int64_t>(e) << kMantissaBits;
  intv += ivalue & kMantissaMask;

  return base::bit_cast<double>(intv);
}

}  // namespace internal
}  // namespace v8
