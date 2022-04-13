// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Karatsuba multiplication. This is loosely based on Go's implementation
// found at https://golang.org/src/math/big/nat.go, licensed as follows:
//
// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file [1].
//
// [1] https://golang.org/LICENSE

#include <algorithm>
#include <utility>

#include "src/bigint/bigint-internal.h"
#include "src/bigint/digit-arithmetic.h"
#include "src/bigint/util.h"
#include "src/bigint/vector-arithmetic.h"

namespace v8 {
namespace bigint {

// If Karatsuba is the best supported algorithm, then it must check for
// termination requests. If there are more advanced algorithms available
// for larger inputs, then Karatsuba will only be used for sufficiently
// small chunks that checking for termination requests is not necessary.
#if V8_ADVANCED_BIGINT_ALGORITHMS
#define MAYBE_TERMINATE
#else
#define MAYBE_TERMINATE \
  if (should_terminate()) return;
#endif

namespace {

// The Karatsuba algorithm sometimes finishes more quickly when the
// input length is rounded up a bit. This method encodes some heuristics
// to accomplish this. The details have been determined experimentally.
int RoundUpLen(int len) {
  if (len <= 36) return RoundUp(len, 2);
  // Keep the 4 or 5 most significant non-zero bits.
  int shift = BitLength(len) - 5;
  if ((len >> shift) >= 0x18) {
    shift++;
  }
  // Round up, unless we're only just above the threshold. This smoothes
  // the steps by which time goes up as input size increases.
  int additive = ((1 << shift) - 1);
  if (shift >= 2 && (len & additive) < (1 << (shift - 2))) {
    return len;
  }
  return ((len + additive) >> shift) << shift;
}

// This method makes the final decision how much to bump up the input size.
int KaratsubaLength(int n) {
  n = RoundUpLen(n);
  int i = 0;
  while (n > kKaratsubaThreshold) {
    n >>= 1;
    i++;
  }
  return n << i;
}

// Performs the specific subtraction required by {KaratsubaMain} below.
void KaratsubaSubtractionHelper(RWDigits result, Digits X, Digits Y,
                                int* sign) {
  X.Normalize();
  Y.Normalize();
  digit_t borrow = 0;
  int i = 0;
  if (!GreaterThanOrEqual(X, Y)) {
    *sign = -(*sign);
    std::swap(X, Y);
  }
  for (; i < Y.len(); i++) {
    result[i] = digit_sub2(X[i], Y[i], borrow, &borrow);
  }
  for (; i < X.len(); i++) {
    result[i] = digit_sub(X[i], borrow, &borrow);
  }
  DCHECK(borrow == 0);
  for (; i < result.len(); i++) result[i] = 0;
}

}  // namespace

void ProcessorImpl::MultiplyKaratsuba(RWDigits Z, Digits X, Digits Y) {
  DCHECK(X.len() >= Y.len());
  DCHECK(Y.len() >= kKaratsubaThreshold);
  DCHECK(Z.len() >= X.len() + Y.len());
  int k = KaratsubaLength(Y.len());
  int scratch_len = 4 * k;
  ScratchDigits scratch(scratch_len);
  KaratsubaStart(Z, X, Y, scratch, k);
}

// Entry point for Karatsuba-based multiplication, takes care of inputs
// with unequal lengths by chopping the larger into chunks.
void ProcessorImpl::KaratsubaStart(RWDigits Z, Digits X, Digits Y,
                                   RWDigits scratch, int k) {
  KaratsubaMain(Z, X, Y, scratch, k);
  MAYBE_TERMINATE
  for (int i = 2 * k; i < Z.len(); i++) Z[i] = 0;
  if (k < Y.len() || X.len() != Y.len()) {
    ScratchDigits T(2 * k);
    // Add X0 * Y1 * b.
    Digits X0(X, 0, k);
    Digits Y1 = Y + std::min(k, Y.len());
    if (Y1.len() > 0) {
      KaratsubaChunk(T, X0, Y1, scratch);
      MAYBE_TERMINATE
      AddAndReturnOverflow(Z + k, T);  // Can't overflow.
    }

    // Add Xi * Y0 << i and Xi * Y1 * b << (i + k).
    Digits Y0(Y, 0, k);
    for (int i = k; i < X.len(); i += k) {
      Digits Xi(X, i, k);
      KaratsubaChunk(T, Xi, Y0, scratch);
      MAYBE_TERMINATE
      AddAndReturnOverflow(Z + i, T);  // Can't overflow.
      if (Y1.len() > 0) {
        KaratsubaChunk(T, Xi, Y1, scratch);
        MAYBE_TERMINATE
        AddAndReturnOverflow(Z + (i + k), T);  // Can't overflow.
      }
    }
  }
}

// Entry point for chunk-wise multiplications, selects an appropriate
// algorithm for the inputs based on their sizes.
void ProcessorImpl::KaratsubaChunk(RWDigits Z, Digits X, Digits Y,
                                   RWDigits scratch) {
  X.Normalize();
  Y.Normalize();
  if (X.len() == 0 || Y.len() == 0) return Z.Clear();
  if (X.len() < Y.len()) std::swap(X, Y);
  if (Y.len() == 1) return MultiplySingle(Z, X, Y[0]);
  if (Y.len() < kKaratsubaThreshold) return MultiplySchoolbook(Z, X, Y);
  int k = KaratsubaLength(Y.len());
  DCHECK(scratch.len() >= 4 * k);
  return KaratsubaStart(Z, X, Y, scratch, k);
}

// The main recursive Karatsuba method.
void ProcessorImpl::KaratsubaMain(RWDigits Z, Digits X, Digits Y,
                                  RWDigits scratch, int n) {
  if (n < kKaratsubaThreshold) {
    X.Normalize();
    Y.Normalize();
    if (X.len() >= Y.len()) {
      return MultiplySchoolbook(RWDigits(Z, 0, 2 * n), X, Y);
    } else {
      return MultiplySchoolbook(RWDigits(Z, 0, 2 * n), Y, X);
    }
  }
  DCHECK(scratch.len() >= 4 * n);
  DCHECK((n & 1) == 0);
  int n2 = n >> 1;
  Digits X0(X, 0, n2);
  Digits X1(X, n2, n2);
  Digits Y0(Y, 0, n2);
  Digits Y1(Y, n2, n2);
  RWDigits scratch_for_recursion(scratch, 2 * n, 2 * n);
  RWDigits P0(scratch, 0, n);
  KaratsubaMain(P0, X0, Y0, scratch_for_recursion, n2);
  MAYBE_TERMINATE
  for (int i = 0; i < n; i++) Z[i] = P0[i];
  RWDigits P2(scratch, n, n);
  KaratsubaMain(P2, X1, Y1, scratch_for_recursion, n2);
  MAYBE_TERMINATE
  RWDigits Z2 = Z + n;
  int end = std::min(Z2.len(), P2.len());
  for (int i = 0; i < end; i++) Z2[i] = P2[i];
  for (int i = end; i < n; i++) {
    DCHECK(P2[i] == 0);
  }
  // The intermediate result can be one digit too large; the subtraction
  // below will fix this.
  digit_t overflow = AddAndReturnOverflow(Z + n2, P0);
  overflow += AddAndReturnOverflow(Z + n2, P2);
  RWDigits X_diff(scratch, 0, n2);
  RWDigits Y_diff(scratch, n2, n2);
  int sign = 1;
  KaratsubaSubtractionHelper(X_diff, X1, X0, &sign);
  KaratsubaSubtractionHelper(Y_diff, Y0, Y1, &sign);
  RWDigits P1(scratch, n, n);
  KaratsubaMain(P1, X_diff, Y_diff, scratch_for_recursion, n2);
  if (sign > 0) {
    overflow += AddAndReturnOverflow(Z + n2, P1);
  } else {
    overflow -= SubAndReturnBorrow(Z + n2, P1);
  }
  // The intermediate result may have been bigger, but the final result fits.
  DCHECK(overflow == 0);
  USE(overflow);
}

#undef MAYBE_TERMINATE

}  // namespace bigint
}  // namespace v8
