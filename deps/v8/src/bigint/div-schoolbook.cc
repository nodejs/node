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
#include <optional>

#include "src/bigint/bigint-internal.h"
#include "src/bigint/div-helpers-inl.h"

namespace v8 {
namespace bigint {

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

// {DivideSchoolbook} below performs 2-by-1 digit divisions. On arm/arm64, there
// is no matching machine instruction for that, and the fallback is slow.
// Fortunately, the divisor is always the same, so a faster alternative is to
// precompute its modular multiplicative inverse, and then replace the division
// with a multiplication-based instruction sequence.
// On x64 this is approximately as fast as using {digit_div} directly.
// Note: this relies on {divisor} being left-shifted such that its most
// significant bit is set, making this technique difficult to apply to
// {DivideSingle}.
class MultiplicativeDigitDiv {
 public:
  explicit MultiplicativeDigitDiv(digit_t divisor)
      : divisor_(divisor), inverse_(ComputeInverse(divisor)) {
    DCHECK((divisor >> (kDigitBits - 1)) == 1);
  }

  // Reference:
  // https://gmplib.org/~tege/division-paper.pdf, "Algorithm 4".
  digit_t div(digit_t high, digit_t low, digit_t* remainder) {
    // Paper: u1 = high, u0 = low, d = divisor, v = inverse,
    // "mod β" means "truncate to digit_t width".
    // 1. <q1, q0> ← v*u1
    digit_t q1;
    digit_t q0 = digit_mul(high, inverse_, &q1);
    // 2. <q1, q0> ← <q1, q0> + <u1, u0>
    digit_t carry;
    q0 = digit_add2(q0, low, &carry);
    q1 = digit_add3(q1, high, carry, &carry);
    // 3. q1 ← (q1 + 1) mod β
    q1++;
    // 4. r ← (u0 − q1*d) mod β
    digit_t r = low - q1 * divisor_;
    // 5. if r > q0:  // Unpredictable condition
    if (r > q0) {
      // 6. q1 ← (q1 − 1) mod β
      q1--;
      // 7. r ← (r + d) mod β
      r += divisor_;
    }
    // 8 if r >= d:  // Unlikely condition
    if (r >= divisor_) [[unlikely]] {
      // 9. q1 ← q1 + 1
      q1++;
      // 10. r ← r − d
      r -= divisor_;
    }
    // 11. return q1, r
    *remainder = r;
    return q1;
  }

 private:
  static digit_t ComputeInverse(digit_t divisor) {
    digit_t high = ~divisor;
    digit_t low = ~digit_t{0};
#if (__x86_64__ || __i386__) && (__GNUC__ || __clang__)
    // Single {div} machine instruction via inline assembly.
    digit_t remainder;
    return digit_div(high, low, divisor, &remainder);
#elif HAVE_TWODIGIT_T
    // Clang-provided system library call.
    return ((twodigit_t{high} << kDigitBits) | low) / divisor;
#else
    // Slow fallback (see {digit_div} implementation).
    digit_t remainder;
    return digit_div(high, low, divisor, &remainder);
#endif
  }

  const digit_t divisor_;
  const digit_t inverse_;
};

// Computes Q(uotient) and R(emainder) for A/B, such that
// Q = (A - R) / B, with 0 <= R < B.
// Both Q and R are optional: callers that are only interested in one of them
// can pass the other with len == 0.
// If Q is present, its length must be at least A.len - B.len + 1.
// If R is present, its length must be at least B.len.
// See Knuth, Volume 2, section 4.3.1, Algorithm D.
void ProcessorImpl::DivideSchoolbook(RWDigits& Q, RWDigits& R, Digits& A,
                                     Digits& B) {
  DCHECK(B.len() >= 2);        // Use DivideSingle otherwise.
  DCHECK(A.len() >= B.len());  // No-op otherwise.
  DCHECK(Q.len() == 0 || QLengthOK(Q, A, B));
  DCHECK(R.len() == 0 || R.len() >= B.len());
  // The unusual variable names inside this function are consistent with
  // Knuth's book, as well as with Go's implementation of this algorithm.
  // Maintaining this consistency is probably more useful than trying to
  // come up with more descriptive names for them.
  const uint32_t n = B.len();
  const uint32_t m = A.len() - n;

  // Try to avoid allocations by caching some scratch memory on the processor.
  int qhatv_len = n + 1;
  int b_normalized_storage_len = n;
  int U_len = A.len() + 1;
  int needed_scratch_space = qhatv_len + b_normalized_storage_len + U_len;
  std::optional<ScratchDigits> allocated_scratch;
  RWDigits scratch(nullptr, 0);
  if (needed_scratch_space <= kSmallScratchSize) {
    scratch = GetSmallScratch();
  } else {
    allocated_scratch = ScratchDigits(needed_scratch_space);
    scratch = allocated_scratch.value();
  }

  // In each iteration, {qhatv} holds {divisor} * {current quotient digit}.
  // "v" is the book's name for {divisor}, "qhat" the current quotient digit.
  RWDigits qhatv(scratch, 0, qhatv_len);

  // D1.
  // Left-shift inputs so that the divisor's MSB is set. This is necessary
  // to prevent the digit-wise divisions (see digit_div call below) from
  // overflowing (they take a two digits wide input, and return a one digit
  // result).
  RWDigits b_normalized_storage(scratch, qhatv_len, b_normalized_storage_len);
  ShiftedDigits b_normalized(B, b_normalized_storage);
  B = b_normalized;
  // U holds the (continuously updated) remaining part of the dividend, which
  // eventually becomes the remainder.
  RWDigits U(scratch, qhatv_len + b_normalized_storage_len, U_len);
  LeftShift(U, A, b_normalized.shift());

  // D2.
  // Iterate over the dividend's digits (like the "grad school" algorithm).
  // {vn1} is the divisor's most significant digit.
  digit_t vn1 = B[n - 1];
  MultiplicativeDigitDiv vn1_divisor(vn1);
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
      qhat = vn1_divisor.div(ujn, U[j + n - 1], &rhat);

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
    if (qhat != 0) {
      MultiplySingle(qhatv, B, qhat);
      AddWorkEstimate(n);
      digit_t c = InplaceSubAndReturnBorrow(U + j, qhatv);
      if (c != 0) {
        c = InplaceAddAndReturnCarry(U + j, B);
        U[j + n] = U[j + n] + c;
        qhat--;
      }
    }

    if (Q.len() != 0) {
      if (static_cast<uint32_t>(j) >= Q.len()) {
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
  for (uint32_t i = m + 1; i < Q.len(); i++) Q[i] = 0;
}

}  // namespace bigint
}  // namespace v8
