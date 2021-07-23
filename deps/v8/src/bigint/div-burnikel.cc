// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Burnikel-Ziegler division.
// Reference: "Fast Recursive Division" by Christoph Burnikel and Joachim
// Ziegler, found at http://cr.yp.to/bib/1998/burnikel.ps

#include <string.h>

#include "src/bigint/bigint-internal.h"
#include "src/bigint/digit-arithmetic.h"
#include "src/bigint/div-helpers.h"
#include "src/bigint/util.h"
#include "src/bigint/vector-arithmetic.h"

namespace v8 {
namespace bigint {

namespace {

// Compares [a_high, A] with B.
// Returns:
// - a value < 0 if [a_high, A] < B
// - 0           if [a_high, A] == B
// - a value > 0 if [a_high, A] > B.
int SpecialCompare(digit_t a_high, Digits A, Digits B) {
  B.Normalize();
  int a_len;
  if (a_high == 0) {
    A.Normalize();
    a_len = A.len();
  } else {
    a_len = A.len() + 1;
  }
  int diff = a_len - B.len();
  if (diff != 0) return diff;
  int i = a_len - 1;
  if (a_high != 0) {
    if (a_high > B[i]) return 1;
    if (a_high < B[i]) return -1;
    i--;
  }
  while (i >= 0 && A[i] == B[i]) i--;
  if (i < 0) return 0;
  return A[i] > B[i] ? 1 : -1;
}

void SetOnes(RWDigits X) {
  memset(X.digits(), 0xFF, X.len() * sizeof(digit_t));
}

// Since the Burnikel-Ziegler method is inherently recursive, we put
// non-changing data into a container object.
class BZ {
 public:
  BZ(ProcessorImpl* proc, int scratch_space)
      : proc_(proc),
        scratch_mem_(scratch_space >= kBurnikelThreshold ? scratch_space : 0) {}

  void DivideBasecase(RWDigits Q, RWDigits R, Digits A, Digits B);
  void D3n2n(RWDigits Q, RWDigits R, Digits A1A2, Digits A3, Digits B);
  void D2n1n(RWDigits Q, RWDigits R, Digits A, Digits B);

 private:
  ProcessorImpl* proc_;
  Storage scratch_mem_;
};

void BZ::DivideBasecase(RWDigits Q, RWDigits R, Digits A, Digits B) {
  A.Normalize();
  B.Normalize();
  DCHECK(B.len() > 0);  // NOLINT(readability/check)
  int cmp = Compare(A, B);
  if (cmp <= 0) {
    Q.Clear();
    if (cmp == 0) {
      // If A == B, then Q=1, R=0.
      R.Clear();
      Q[0] = 1;
    } else {
      // If A < B, then Q=0, R=A.
      PutAt(R, A, R.len());
    }
    return;
  }
  if (B.len() == 1) {
    return proc_->DivideSingle(Q, R.digits(), A, B[0]);
  }
  return proc_->DivideSchoolbook(Q, R, A, B);
}

// Algorithm 2 from the paper. Variable names same as there.
// Returns Q(uotient) and R(emainder) for A/B, with B having two thirds
// the size of A = [A1, A2, A3].
void BZ::D3n2n(RWDigits Q, RWDigits R, Digits A1A2, Digits A3, Digits B) {
  DCHECK((B.len() & 1) == 0);  // NOLINT(readability/check)
  int n = B.len() / 2;
  DCHECK(A1A2.len() == 2 * n);
  // Actual condition is stricter than length: A < B * 2^(kDigitBits * n)
  DCHECK(Compare(A1A2, B) < 0);  // NOLINT(readability/check)
  DCHECK(A3.len() == n);
  DCHECK(Q.len() == n);
  DCHECK(R.len() == 2 * n);
  // 1. Split A into three parts A = [A1, A2, A3] with Ai < 2^(kDigitBits * n).
  Digits A1(A1A2, n, n);
  // 2. Split B into two parts B = [B1, B2] with Bi < 2^(kDigitBits * n).
  Digits B1(B, n, n);
  Digits B2(B, 0, n);
  // 3. Distinguish the cases A1 < B1 or A1 >= B1.
  RWDigits Qhat = Q;
  RWDigits R1(R, n, n);
  digit_t r1_high = 0;
  if (Compare(A1, B1) < 0) {
    // 3a. If A1 < B1, compute Qhat = floor([A1, A2] / B1) with remainder R1
    //     using algorithm D2n1n.
    D2n1n(Qhat, R1, A1A2, B1);
    if (proc_->should_terminate()) return;
  } else {
    // 3b. If A1 >= B1, set Qhat = 2^(kDigitBits * n) - 1 and set
    //     R1 = [A1, A2] - [B1, 0] + [0, B1]
    SetOnes(Qhat);
    // Step 1: compute A1 - B1, which can't underflow because of the comparison
    // guarding this else-branch, and always has a one-digit result because
    // of this function's preconditions.
    RWDigits temp = R1;
    Subtract(temp, A1, B1);
    temp.Normalize();
    DCHECK(temp.len() <= 1);  // NOLINT(readability/check)
    if (temp.len() > 0) r1_high = temp[0];
    // Step 2: compute A2 + B1.
    Digits A2(A1A2, 0, n);
    r1_high += AddAndReturnCarry(R1, A2, B1);
  }
  // 4. Compute D = Qhat * B2 using (Karatsuba) multiplication.
  RWDigits D(scratch_mem_.get(), 2 * n);
  proc_->Multiply(D, Qhat, B2);
  if (proc_->should_terminate()) return;

  // 5. Compute Rhat = R1*2^(kDigitBits * n) + A3 - D = [R1, A3] - D.
  PutAt(R, A3, n);
  // 6. As long as Rhat < 0, repeat:
  while (SpecialCompare(r1_high, R, D) < 0) {
    // 6a. Rhat = Rhat + B
    r1_high += AddAndReturnCarry(R, R, B);
    // 6b. Qhat = Qhat - 1
    Subtract(Qhat, 1);
  }
  // 5. Compute Rhat = R1*2^(kDigitBits * n) + A3 - D = [R1, A3] - D.
  digit_t borrow = SubtractAndReturnBorrow(R, R, D);
  DCHECK(borrow == r1_high);
  DCHECK(Compare(R, B) < 0);  // NOLINT(readability/check)
  (void)borrow;
  // 7. Return R = Rhat, Q = Qhat.
}

// Algorithm 1 from the paper. Variable names same as there.
// Returns Q(uotient) and (R)emainder for A/B, with A twice the size of B.
void BZ::D2n1n(RWDigits Q, RWDigits R, Digits A, Digits B) {
  int n = B.len();
  DCHECK(A.len() <= 2 * n);
  // A < B * 2^(kDigitsBits * n)
  DCHECK(Compare(Digits(A, n, n), B) < 0);  // NOLINT(readability/check)
  DCHECK(Q.len() == n);
  DCHECK(R.len() == n);
  // 1. If n is odd or smaller than some convenient constant, compute Q and R
  //    by school division and return.
  if ((n & 1) == 1 || n < kBurnikelThreshold) {
    return DivideBasecase(Q, R, A, B);
  }
  // 2. Split A into four parts A = [A1, ..., A4] with
  //    Ai < 2^(kDigitBits * n / 2). Split B into two parts [B2, B1] with
  //    Bi < 2^(kDigitBits * n / 2).
  Digits A1A2(A, n, n);
  Digits A3(A, n / 2, n / 2);
  Digits A4(A, 0, n / 2);
  // 3. Compute the high part Q1 of floor(A/B) as
  //    Q1 = floor([A1, A2, A3] / [B1, B2]) with remainder R1 = [R11, R12],
  //    using algorithm D3n2n.
  RWDigits Q1(Q, n / 2, n / 2);
  ScratchDigits R1(n);
  D3n2n(Q1, R1, A1A2, A3, B);
  if (proc_->should_terminate()) return;
  // 4. Compute the low part Q2 of floor(A/B) as
  //    Q2 = floor([R11, R12, A4] / [B1, B2]) with remainder R, using
  //    algorithm D3n2n.
  RWDigits Q2(Q, 0, n / 2);
  D3n2n(Q2, R, R1, A4, B);
  // 5. Return Q = [Q1, Q2] and R.
}

}  // namespace

// Algorithm 3 from the paper. Variables names same as there.
// Returns Q(uotient) and R(emainder) for A/B (no size restrictions).
// R is optional, Q is not.
void ProcessorImpl::DivideBurnikelZiegler(RWDigits Q, RWDigits R, Digits A,
                                          Digits B) {
  DCHECK(A.len() >= B.len());
  DCHECK(R.len() == 0 || R.len() >= B.len());
  DCHECK(Q.len() > A.len() - B.len());
  int r = A.len();
  int s = B.len();
  // The requirements are:
  // - n >= s, n as small as possible.
  // - m must be a power of two.
  // 1. Set m = min {2^k | 2^k * kBurnikelThreshold > s}.
  int m = 1 << BitLength(s / kBurnikelThreshold);
  // 2. Set j = roundup(s/m) and n = j * m.
  int j = DIV_CEIL(s, m);
  int n = j * m;
  // 3. Set sigma = max{tao | 2^tao * B < 2^(kDigitBits * n)}.
  int sigma = CountLeadingZeros(B[s - 1]);
  int digit_shift = n - s;
  // 4. Set B = B * 2^sigma to normalize B. Shift A by the same amount.
  ScratchDigits B_shifted(n);
  LeftShift(B_shifted + digit_shift, B, sigma);
  for (int i = 0; i < digit_shift; i++) B_shifted[i] = 0;
  B = B_shifted;
  // We need an extra digit if A's top digit does not have enough space for
  // the left-shift by {sigma}. Additionally, the top bit of A must be 0
  // (see "-1" in step 5 below), which combined with B being normalized (i.e.
  // B's top bit is 1) ensures the preconditions of the helper functions.
  int extra_digit = CountLeadingZeros(A[r - 1]) < (sigma + 1) ? 1 : 0;
  r = A.len() + digit_shift + extra_digit;
  ScratchDigits A_shifted(r);
  LeftShift(A_shifted + digit_shift, A, sigma);
  for (int i = 0; i < digit_shift; i++) A_shifted[i] = 0;
  A = A_shifted;
  // 5. Set t = min{t >= 2 | A < 2^(kDigitBits * t * n - 1)}.
  int t = std::max(DIV_CEIL(r, n), 2);
  // 6. Split A conceptually into t blocks.
  // 7. Set Z_(t-2) = [A_(t-1), A_(t-2)].
  int z_len = n * 2;
  ScratchDigits Z(z_len);
  PutAt(Z, A + n * (t - 2), z_len);
  // 8. For i from t-2 downto 0 do:
  BZ bz(this, n);
  ScratchDigits Ri(n);
  {
    // First iteration unrolled and specialized.
    // We might not have n digits at the top of Q, so use temporary storage
    // for Qi...
    ScratchDigits Qi(n);
    bz.D2n1n(Qi, Ri, Z, B);
    if (should_terminate()) return;
    // ...but there *will* be enough space for any non-zero result digits!
    Qi.Normalize();
    RWDigits target = Q + n * (t - 2);
    DCHECK(Qi.len() <= target.len());
    PutAt(target, Qi, target.len());
  }
  // Now loop over any remaining iterations.
  for (int i = t - 3; i >= 0; i--) {
    // 8b. If i > 0, set Z_(i-1) = [Ri, A_(i-1)].
    // (De-duped with unrolled first iteration, hence reading A_(i).)
    PutAt(Z + n, Ri, n);
    PutAt(Z, A + n * i, n);
    // 8a. Using algorithm D2n1n compute Qi, Ri such that Zi = B*Qi + Ri.
    RWDigits Qi(Q, i * n, n);
    bz.D2n1n(Qi, Ri, Z, B);
    if (should_terminate()) return;
  }
  // 9. Return Q = [Q_(t-2), ..., Q_0] and R = R_0 * 2^(-sigma).
#if DEBUG
  for (int i = 0; i < digit_shift; i++) {
    DCHECK(Ri[i] == 0);  // NOLINT(readability/check)
  }
#endif
  if (R.len() != 0) {
    Digits Ri_part(Ri, digit_shift, Ri.len());
    Ri_part.Normalize();
    DCHECK(Ri_part.len() <= R.len());
    RightShift(R, Ri_part, sigma);
  }
}

}  // namespace bigint
}  // namespace v8
