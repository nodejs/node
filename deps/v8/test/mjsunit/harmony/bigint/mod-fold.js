// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Coverage for the fold fast path in {CachedMod}.
//
// {CachedMod_MakeInverse} divides T = 2^(n * kDigitBits) by the n-digit
// divisor B and arms {cached_mod_fold_factor_} with the remainder C when
// T = q * B + C has a single-digit C and q <= kMaxFoldQuotient. A nonzero
// factor makes {CachedMod} run {CachedModFold} instead of multiplying by the
// cached inverse. Zero is the not-armed sentinel, so a power of two, which
// divides T exactly, can never arm the fold.
//
// Whether a divisor qualifies depends on kDigitBits, so the cases below are
// built from the running target's actual digit width rather than from both
// candidate widths.

const kDigitBits = %Is64Bit() ? 64 : 32;

// The cache arms after kCachingThreshold = 100 consecutive uses of one
// divisor, so every case runs past that to cover the pre- and post-arming
// paths.
const kIterationsUntilCached = 105;

// Reference modulo. Division and multiplication have no cached-divisor path of
// their own (the cache is only consulted from
// MutableBigInt_AbsoluteModAndCanonicalize), so this does not go through the
// code under test.
function refMod(a, b) {
  return a - (a / b) * b;
}

// The mod in this loop is loop-invariant, so once the helper tiers up most
// iterations are eliminated and never reach the runtime, leaving most cases
// short of the arming threshold.
%NeverOptimizeFunction(checkRepeated);
function checkRepeated(divisor, dividend) {
  const expected = refMod(dividend, divisor);
  for (let i = 0; i < kIterationsUntilCached; i++) {
    assertEquals(expected, dividend % divisor);
  }
}

// {CachedMod} is only reached for dividends of at most 2n digits; wider ones
// are rejected by the length check in the caller and fall through to
// ModuloLarge. Sweep both sides of the split at n digits.
function checkAllDividendSizes(divisor, n) {
  for (let size = n; size <= 2 * n; size++) {
    const top = 1n << BigInt(size * kDigitBits);
    for (const dividend of [top - 1n, top - 12345n, top >> 1n,
                            (top >> 1n) | 1n]) {
      checkRepeated(divisor, dividend);
    }
  }
}

// Pseudo-Mersenne divisors 2^k - c. A small c keeps C single-digit and q at 1
// or 2, which is what the fold path is for.
//
// The fold factor and the cached inverse are single slots belonging to
// whichever divisor is armed, so a stale one would show up right after a
// switch. Every case here switches divisors, and each gets the arming
// threshold to itself, so the sweeps below cover that on their own: 34
// switches between two armed divisors with different C, plus 7 transitions
// between the fold and the inverse.
for (const n of [2, 3, 4, 5, 6, 8]) {
  const bits = n * kDigitBits;
  for (const c of [1n, 19n, 977n, 4294968273n]) {
    checkAllDividendSizes((1n << BigInt(bits)) - c, n);
  }
  // B need not fill its top digit. T is taken at the next whole digit boundary
  // at or above B, so a B whose top bit is clear gets C scaled up accordingly:
  // 2^255 - 19 spans the same 4 digits as 2^256 - c at a 64-bit width, and
  // yields q = 2, C = 38.
  checkAllDividendSizes((1n << BigInt(bits - 1)) - 19n, n);
}

// The moduli this path exists for. The first three arm the fold; 2^521 - 1
// does not, because its 521 bits sit in a 9-digit span (at a 64-bit width)
// whose q is 2^55.
const ed25519P = (1n << 255n) - 19n;
const secp256k1P = (1n << 256n) - (1n << 32n) - 977n;
const mersenne127 = (1n << 127n) - 1n;
const mersenne521 = (1n << 521n) - 1n;
for (const p of [ed25519P, secp256k1P, mersenne127, mersenne521]) {
  checkRepeated(p, p - 1n);
  checkRepeated(p, p);
  checkRepeated(p, p + 1n);
  checkRepeated(p, p * 2n - 1n);
  checkRepeated(p, p * p - 1n);
  let x = 123456789n;
  for (let i = 0; i < 200; i++) {
    // Reduce the unreduced square rather than the previous result: x is
    // already below p by construction, so checking it against refMod(x, p)
    // would compare it against itself.
    const square = x * x;
    x = square % p;
    assertEquals(refMod(square, p), x);
  }
}

// The two halves of the gate in {CachedMod_MakeInverse} do different jobs, so
// cover each independently.
//
// q bounds the corrective loop at the end of {CachedModFold} and is capped at
// kMaxFoldQuotient = 4. Divisors built from T = q * B + C with single-digit C:
//   2^256 = 3 * ((2^256 - 4) / 3) + 4   q = 3, admitted
//   2^256 = 4 * (2^254 - 1) + 4         q = 4, the largest admitted
//   2^256 = 5 * ((2^256 - 1) / 5) + 1   q = 5, rejected, uses the inverse
const n256 = 256 / kDigitBits;
checkAllDividendSizes(((1n << 256n) - 4n) / 3n, n256);
checkAllDividendSizes((1n << 254n) - 1n, n256);
checkAllDividendSizes(((1n << 256n) - 1n) / 5n, n256);

// C fitting one digit is the other half of the gate, and it is what keeps each
// fold column a single multiply. 2^128 = 4 * (2^126 - 2^62 + 1) + (2^64 - 4)
// has q = 4 and a C just under a 64-bit digit, so it is admitted at a 64-bit
// width and rejected at a 32-bit one, where the same C spans two digits.
checkAllDividendSizes((1n << 126n) - (1n << 62n) + 1n, 128 / kDigitBits);

// C at the very top of a digit: 2^128 = 1 * (2^128 - 2^64 + 1) + (2^64 - 1)
// arms with C == ~digit_t{0}, the largest factor the fold can see. Each column
// then sums to exactly D^2 - 1, the tight end of the bound {CachedModFold}
// relies on for its carry to stay single-digit.
checkAllDividendSizes((1n << 128n) - (1n << 64n) + 1n, 128 / kDigitBits);

// A C spanning exactly two digits with q = 1, which pins the C half of the
// gate on its own: every other rejected divisor here is rejected on q too, and
// admitting this one would fold with a factor truncated to C's low digit.
// 2^256 = 1 * B + C with B = 2^256 - 2^64 - 1 gives C = 2^64 + 1.
//
// Not checkAllDividendSizes: its dividends below the divisor return early
// without reaching the cached path, breaking the run the arming counter needs.
{
  const twoDigitFactorP = (1n << 256n) - (1n << 64n) - 1n;
  checkRepeated(twoDigitFactorP, twoDigitFactorP * 3n + 12345n);
  checkRepeated(twoDigitFactorP, twoDigitFactorP * twoDigitFactorP - 1n);
}

// Dividends that leave the fold with a nonzero carry out while the low n
// digits are already below B, so comparing R against B alone would stop the
// corrective loop one subtraction early. Each is solved for rather than
// sampled: it puts the first fold at exactly R = D^n - 1 with a carry of 1,
// which random dividends essentially never produce.
{
  // 2^255 - 19 at n = 4 (64-bit digits) or n = 8 (32-bit).
  const forEd25519P =
      352837050787963081567737499952785424407351732120852457308514774834783263949486697425160875523482044389225082438360059208781376754060895394392327283802089n;
  checkRepeated(ed25519P, forEd25519P);
  // 2^127 - 1 at n = 2 or n = 4.
  const forMersenne127 =
      57896044618658097711785492504343953926975274699741220483192166611388333031423n;
  checkRepeated(mersenne127, forMersenne127);
}

// Negative divisors and dividends reach the same paths; only the result's sign
// differs. The fold itself is sign-agnostic, so one case each is enough.
checkRepeated(-ed25519P, ed25519P * ed25519P - 1n);
checkRepeated(ed25519P, -(ed25519P * 2n + 12345n));

// Divisors that must keep using the inverse: standard curve moduli whose C
// spans more than one digit at either width.
const kNonQualifying = [
  // ed25519 group order.
  (1n << 252n) + 27742317777372353535851937790883648493n,
  // secp256k1 group order.
  0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141n,
  // NIST P-256 and P-384.
  (1n << 256n) - (1n << 224n) + (1n << 192n) + (1n << 96n) - 1n,
  (1n << 384n) - (1n << 128n) - (1n << 96n) + (1n << 32n) - 1n,
  // bls12-381 base field.
  0x1a0111ea397fe69a4b1ba7b6434bacd764774b84f38512bf6730d2a0f6b0f6241eabfffeb153ffffb9feffffffffaaabn,
];
for (const m of kNonQualifying) {
  checkRepeated(m, m - 1n);
  checkRepeated(m, m * m - 1n);
  checkRepeated(m, (m << 1n) + 12345n);
}

// Powers of two divide T exactly, so C is 0, which is also the not-armed
// sentinel. They must stay on the inverse path and stay correct.
for (const bits of [128n, 192n, 256n, 320n]) {
  const divisor = 1n << bits;
  checkRepeated(divisor, divisor - 1n);
  checkRepeated(divisor, divisor * divisor - 1n);
  checkRepeated(divisor, (divisor << 1n) | 1n);
}

// An exact multiple folds to all-zero digits, the one fold result that
// {Canonicalize} has to trim all the way down to 0n.
for (const p of [ed25519P, secp256k1P, (1n << 640n) - 19n]) {
  checkRepeated(p, p * 4n);
  checkRepeated(p, -(p * 4n));
}

// kMaxCachedModDivisorSize = 32 digits caps what the cache accepts at all,
// which is 2048 bits on a 64-bit digit and 1024 on a 32-bit one. Straddle both
// so the larger divisor is uncacheable at either width, while the smaller
// still folds.
for (const bits of [1024n, 2048n, 2112n]) {
  const divisor = (1n << bits) - 19n;
  checkRepeated(divisor, divisor - 1n);
  checkRepeated(divisor, divisor * 3n);
  checkRepeated(divisor, divisor * divisor - 1n);
}

// Dividends wider than 2n digits skip the cached path entirely via the length
// check in the caller, whether or not a factor is armed.
{
  const p = ed25519P;
  checkRepeated(p, p * 3n);
  const wide = (1n << 1200n) - 1n;
  assertEquals(refMod(wide, p), wide % p);
}
