// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// "Schoolbook" division. This is loosely based on Go's implementation
// found at https://golang.org/src/math/big/nat.go, licensed as follows:
//
// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file [1].
//
// [1] https://golang.org/LICENSE

#include <limits>

#include "src/bigint/bigint-internal.h"
#include "src/bigint/digit-arithmetic.h"
#include "src/bigint/div-helpers.h"
#include "src/bigint/util.h"
#include "src/bigint/vector-arithmetic.h"

namespace v8 {
namespace bigint {

// Computes Q(uotient) and remainder for A/b, such that
// Q = (A - remainder) / b, with 0 <= remainder < b.
// If Q.len == 0, only the remainder will be returned.
// Q may be the same as A for an in-place division.
void ProcessorImpl::DivideSingle(RWDigits Q, digit_t* remainder, Digits A,
                                 digit_t b) {
  DCHECK(b != 0);
  DCHECK(A.len() > 0);
  *remainder = 0;
  int length = A.len();
  if (Q.len() != 0) {
    if (A[length - 1] >= b) {
      DCHECK(Q.len() >= A.len());
      for (int i = length - 1; i >= 0; i--) {
        Q[i] = digit_div(*remainder, A[i], b, remainder);
      }
      for (int i = length; i < Q.len(); i++) Q[i] = 0;
    } else {
      DCHECK(Q.len() >= A.len() - 1);
      *remainder = A[length - 1];
      for (int i = length - 2; i >= 0; i--) {
        Q[i] = digit_div(*remainder, A[i], b, remainder);
      }
      for (int i = length - 1; i < Q.len(); i++) Q[i] = 0;
    }
  } else {
    for (int i = length - 1; i >= 0; i--) {
      digit_div(*remainder, A[i], b, remainder);
    }
  }
}

// Z += X. Returns the "carry" (0 or 1) after adding all of X's digits.
inline digit_t InplaceAdd(RWDigits Z, Digits X) {
  return AddAndReturnCarry(Z, Z, X);
}

// Z -= X. Returns the "borrow" (0 or 1) after subtracting all of X's digits.
inline digit_t InplaceSub(RWDigits Z, Digits X) {
  return SubtractAndReturnBorrow(Z, Z, X);
}

// Returns whether (factor1 * factor2) > (high << kDigitBits) + low.
bool ProductGreaterThan(digit_t factor1, digit_t factor2, digit_t high,
                        digit_t low) {
  digit_t result_high;
  digit_t result_low = digit_mul(factor1, factor2, &result_high);
  return result_high > high || (result_high == high && result_low > low);
}

#if DEBUG
bool QLengthOK(Digits Q, Digits A, Digits B) {
  // If A's top B.len digits are greater than or equal to B, then the division
  // result will be greater than A.len - B.len, otherwise it will be that
  // difference. Intuitively: 100/10 has 2 digits, 100/11 has 1.
  if (GreaterThanOrEqual(Digits(A, A.len() - B.len(), B.len()), B)) {
    return Q.len() >= A.len() - B.len() + 1;
  }
  return Q.len() >= A.len() - B.len();
}
#endif

// Computes Q(uotient) and R(emainder) for A/B, such that
// Q = (A - R) / B, with 0 <= R < B.
// Both Q and R are optional: callers that are only interested in one of them
// can pass the other with len == 0.
// If Q is present, its length must be at least A.len - B.len + 1.
// If R is present, its length must be at least B.len.
// See Knuth, Volume 2, section 4.3.1, Algorithm D.
void ProcessorImpl::DivideSchoolbook(RWDigits Q, RWDigits R, Digits A,
                                     Digits B) {
  DCHECK(B.len() >= 2);        // Use DivideSingle otherwise.
  DCHECK(A.len() >= B.len());  // No-op otherwise.
  DCHECK(Q.len() == 0 || QLengthOK(Q, A, B));
  DCHECK(R.len() == 0 || R.len() >= B.len());
  // The unusual variable names inside this function are consistent with
  // Knuth's book, as well as with Go's implementation of this algorithm.
  // Maintaining this consistency is probably more useful than trying to
  // come up with more descriptive names for them.
  const int n = B.len();
  const int m = A.len() - n;

  // In each iteration, {qhatv} holds {divisor} * {current quotient digit}.
  // "v" is the book's name for {divisor}, "qhat" the current quotient digit.
  ScratchDigits qhatv(n + 1);

  // D1.
  // Left-shift inputs so that the divisor's MSB is set. This is necessary
  // to prevent the digit-wise divisions (see digit_div call below) from
  // overflowing (they take a two digits wide input, and return a one digit
  // result).
  ShiftedDigits b_normalized(B);
  B = b_normalized;
  // U holds the (continuously updated) remaining part of the dividend, which
  // eventually becomes the remainder.
  ScratchDigits U(A.len() + 1);
  LeftShift(U, A, b_normalized.shift());

  // D2.
  // Iterate over the dividend's digits (like the "grad school" algorithm).
  // {vn1} is the divisor's most significant digit.
  digit_t vn1 = B[n - 1];
  for (int j = m; j >= 0; j--) {
    // D3.
    // Estimate the current iteration's quotient digit (see Knuth for details).
    // {qhat} is the current quotient digit.
    digit_t qhat = std::numeric_limits<digit_t>::max();
    // {ujn} is the dividend's most significant remaining digit.
    digit_t ujn = U[j + n];
    if (ujn != vn1) {
      // {rhat} is the current iteration's remainder.
      digit_t rhat = 0;
      // Estimate the current quotient digit by dividing the most significant
      // digits of dividend and divisor. The result will not be too small,
      // but could be a bit too large.
      qhat = digit_div(ujn, U[j + n - 1], vn1, &rhat);

      // Decrement the quotient estimate as needed by looking at the next
      // digit, i.e. by testing whether
      // qhat * v_{n-2} > (rhat << kDigitBits) + u_{j+n-2}.
      digit_t vn2 = B[n - 2];
      digit_t ujn2 = U[j + n - 2];
      while (ProductGreaterThan(qhat, vn2, rhat, ujn2)) {
        qhat--;
        digit_t prev_rhat = rhat;
        rhat += vn1;
        // v[n-1] >= 0, so this tests for overflow.
        if (rhat < prev_rhat) break;
      }
    }

    // D4.
    // Multiply the divisor with the current quotient digit, and subtract
    // it from the dividend. If there was "borrow", then the quotient digit
    // was one too high, so we must correct it and undo one subtraction of
    // the (shifted) divisor.
    if (qhat == 0) {
      qhatv.Clear();
    } else {
      MultiplySingle(qhatv, B, qhat);
    }
    digit_t c = InplaceSub(U + j, qhatv);
    if (c != 0) {
      c = InplaceAdd(U + j, B);
      U[j + n] = U[j + n] + c;
      qhat--;
    }

    if (Q.len() != 0) {
      if (j >= Q.len()) {
        DCHECK(qhat == 0);
      } else {
        Q[j] = qhat;
      }
    }
  }
  if (R.len() != 0) {
    RightShift(R, U, b_normalized.shift());
  }
  // If Q has extra storage, clear it.
  for (int i = m + 1; i < Q.len(); i++) Q[i] = 0;
}

}  // namespace bigint
}  // namespace v8
