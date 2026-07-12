/* SPDX-License-Identifier: MIT OR Apache-2.0 */

//! This module provides accelerated modular multiplication by large powers
//! of two, which is needed for computing floating point remainders in `fmod`
//! and similar functions.
//!
//! To keep the equations somewhat concise, the following conventions are used:
//!  - all integer operations are in the mathematical sense, without overflow
//!  - concatenation means multiplication: `2xq = 2 * x * q`
//!  - `R = (1 << U::BITS)` is the modulus of wrapping arithmetic in `U`

use crate::support::int_traits::NarrowingDiv;
use crate::support::{DInt, HInt, Int};

/// Compute the remainder `(x << e) % y` with unbounded integers.
/// Requires `x < 2y` and `y.leading_zeros() >= 2`
pub fn linear_mul_reduction<U>(x: U, mut e: u32, mut y: U) -> U
where
    U: HInt + Int<Unsigned = U>,
    U::D: NarrowingDiv,
{
    assert!(y <= U::MAX >> 2);
    assert!(x < (y << 1));
    let _0 = U::ZERO;
    let _1 = U::ONE;

    // power of two divisors
    if (y & (y - _1)).is_zero() {
        if e < U::BITS {
            // shift and only keep low bits
            return (x << e) & (y - _1);
        } else {
            // would shift out all the bits
            return _0;
        }
    }

    // Use the identity `(x << e) % y == ((x << (e + s)) % (y << s)) >> s`
    // to shift the divisor so it has exactly two leading zeros to satisfy
    // the precondition of `Reducer::new`
    let s = y.leading_zeros() - 2;
    e += s;
    y <<= s;

    // `m: Reducer` keeps track of the remainder `x` in a form that makes it
    //  very efficient to do `x <<= k` modulo `y` for integers `k < U::BITS`
    let mut m = Reducer::new(x, y);

    // Use the faster special case with constant `k == U::BITS - 1` while we can
    while e >= U::BITS - 1 {
        m.word_reduce();
        e -= U::BITS - 1;
    }
    // Finish with the variable shift operation
    m.shift_reduce(e);

    // The partial remainder is in `[0, 2y)` ...
    let r = m.partial_remainder();
    // ... so check and correct, and compensate for the earlier shift.
    r.checked_sub(y).unwrap_or(r) >> s
}

/// Helper type for computing the reductions. The implementation has a number
/// of seemingly weird choices, but everything is aimed at streamlining
/// `Reducer::word_reduce` into its current form.
///
/// Implicitly contains:
///  n in (R/8, R/4)
///  x in [0, 2n)
/// The value of `n` is fixed for a given `Reducer`,
/// but the value of `x` is modified by the methods.
#[derive(Debug, Clone, PartialEq, Eq)]
struct Reducer<U: HInt> {
    // m = 2n
    m: U,
    // q = (RR/2) / m
    // r = (RR/2) % m
    // Then RR/2 = qm + r, where `0 <= r < m`
    // The value `q` is only needed during construction, so isn't saved.
    r: U,
    // The value `x` is implicitly stored as `2 * q * x`:
    _2xq: U::D,
}

impl<U> Reducer<U>
where
    U: HInt,
    U: Int<Unsigned = U>,
{
    /// Construct a reducer for `(x << _) mod n`.
    ///
    /// Requires `R/8 < n < R/4` and `x < 2n`.
    fn new(x: U, n: U) -> Self
    where
        U::D: NarrowingDiv,
    {
        let _1 = U::ONE;
        assert!(n > (_1 << (U::BITS - 3)));
        assert!(n < (_1 << (U::BITS - 2)));
        let m = n << 1;
        assert!(x < m);

        // We need to compute the parameters
        // `q = (RR/2) / m`
        // `r = (RR/2) % m`

        // Since `m` is in `(R/4, R/2)`, the quotient `q` is in `[R, 2R)`, and
        // it would overflow in `U` if computed directly. Instead, we compute
        // `f = q - R`, which is in `[0, R)`. To do so, we simply subtract `Rm`
        // from the dividend, which doesn't change the remainder:
        // `f = R(R/2 - m) / m`
        // `r = R(R/2 - m) % m`
        let dividend = ((_1 << (U::BITS - 1)) - m).widen_hi();
        let (f, r) = dividend.checked_narrowing_div_rem(m).unwrap();

        // As `x < m`, `xq < qm <= RR/2`
        // Thus `2xq = 2xR + 2xf` does not overflow in `U::D`.
        let _2x = x + x;
        let _2xq = _2x.widen_hi() + _2x.widen_mul(f);
        Self { m, r, _2xq }
    }

    /// Extract the current remainder `x` in the range `[0, 2n)`
    fn partial_remainder(&self) -> U {
        // `RR/2 = qm + r`, where `0 <= r < m`
        // `2xq = uR + v`,  where `0 <= v < R`

        // The goal is to extract the current value of `x` from the value `2xq`
        // that we actually have. A bit simplified, we could multiply it by `m`
        // to obtain `2xqm == 2x(RR/2 - r) == xRR - 2xr`, where `2xr < RR`.
        // We could just round that up to the next multiple of `RR` to get `x`,
        // but we can avoid having to multiply the full double-wide `2xq` by
        // making a couple of adjustments:

        // First, let's only use the high half `u` for the product, and
        // include an additional error term due to the truncation:
        //  `mu = xR - (2xr + mv)/R`

        // Next, show bounds for the error term
        //  `0 <= mv < mR` follows from `0 <= v < R`
        //  `0 <= 2xr < mR` follows from `0 <= x < m < R/2` and `0 <= r < m`
        // Adding those together, we have:
        //  `0 <= (mv + 2xr)/R < 2m`
        // Which also implies:
        //  `0 < 2m - (mv + 2xr)/R <= 2m < R`

        // For that reason, we can use `u + 2` as the factor to obtain
        //  `m(u + 2) = xR + (2m - (mv + 2xr)/R)`
        // By the previous inequality, the second term fits neatly in the lower
        // half, so we get exactly `x` as the high half.
        let u = self._2xq.hi();
        let _2 = U::ONE + U::ONE;
        self.m.widen_mul(u + _2).hi()

        // Additionally, we should ensure that `u + 2` cannot overflow:
        // Since `x < m` and `2qm <= RR`,
        //  `2xq <= 2q(m-1) <= RR - 2q`
        // As we also have `q > R`,
        //  `2xq < RR - 2R`
        // which is sufficient.
    }

    /// Replace the remainder `x` with `(x << k) - un`,
    /// for a suitable quotient `u`, which is returned.
    ///
    /// Requires that `k < U::BITS`.
    fn shift_reduce(&mut self, k: u32) -> U {
        assert!(k < U::BITS);

        // First, split the shifted value:
        // `2xq << k = aRR/2 + b`, where `0 <= b < RR/2`
        let a = self._2xq.hi() >> (U::BITS - 1 - k);
        let (low, high) = (self._2xq << k).lo_hi();
        let b = U::D::from_lo_hi(low, high & (U::MAX >> 1));

        // Then, subtract `2anq = aqm`:
        // ```
        // (2xq << k) - aqm
        // = aRR/2 + b - aqm
        // = a(RR/2 - qm) + b
        // = ar + b
        // ```
        self._2xq = a.widen_mul(self.r) + b;
        a

        // Since `a` is at most the high half of `2xq`, we have
        //  `a + 2 < R` (shown above, in `partial_remainder`)
        // Using that together with `b < RR/2` and `r < m < R/2`,
        // we get `(a + 2)r + b < RR`, so
        //  `ar + b < RR - 2r = 2mq`
        // which shows that the new remainder still satisfies `x < m`.
    }

    // NB: `word_reduce()` is just the special case `shift_reduce(U::BITS - 1)`
    // that optimizes especially well. The correspondence is that `a == u` and
    //  `b == (v >> 1).widen_hi()`
    //
    /// Replace the remainder `x` with `x(R/2) - un`,
    /// for a suitable quotient `u`, which is returned.
    fn word_reduce(&mut self) -> U {
        // To do so, we replace `2xq = uR + v` with
        // ```
        // 2 * (x(R/2) - un) * q
        // = xqR - 2unq
        // = xqR - uqm
        // = uRR/2 + vR/2 - uRR/2 + ur
        // = ur + (v/2)R
        // ```
        let (v, u) = self._2xq.lo_hi();
        self._2xq = u.widen_mul(self.r) + U::widen_hi(v >> 1);
        u

        // Additional notes:
        //  1. As `v` is the low bits of `2xq`, it is even and can be halved.
        //  2. The new remainder is `(xr + mv/2) / R` (see below)
        //      and since `v < R`, `r < m`, `x < m < R/2`,
        //      that is also strictly less than `m`.
        // ```
        // (x(R/2) - un)R
        //      = xRR/2 - (m/2)uR
        //      = x(qm + r) - (m/2)(2xq - v)
        //      = xqm + xr - xqm + mv/2
        //      = xr + mv/2
        // ```
    }
}

#[cfg(test)]
mod test {
    use crate::support::linear_mul_reduction;
    use crate::support::modular::Reducer;

    #[test]
    fn reducer_ops() {
        for n in 33..=63_u8 {
            for x in 0..2 * n {
                let temp = Reducer::new(x, n);
                let n = n as u32;
                let x0 = temp.partial_remainder() as u32;
                assert_eq!(x as u32, x0);
                for k in 0..=7 {
                    let mut red = temp.clone();
                    let u = red.shift_reduce(k) as u32;
                    let x1 = red.partial_remainder() as u32;
                    assert_eq!(x1, (x0 << k) - u * n);
                    assert!(x1 < 2 * n);
                    assert!((red._2xq as u32).is_multiple_of(2 * x1));

                    // `word_reduce` is equivalent to
                    // `shift_reduce(U::BITS - 1)`
                    if k == 7 {
                        let mut alt = temp.clone();
                        let w = alt.word_reduce();
                        assert_eq!(u, w as u32);
                        assert_eq!(alt, red);
                    }
                }
            }
        }
    }
    #[test]
    fn reduction_u8() {
        for y in 1..64u8 {
            for x in 0..2 * y {
                let mut r = x % y;
                for e in 0..100 {
                    assert_eq!(r, linear_mul_reduction(x, e, y));
                    // maintain the correct expected remainder
                    r <<= 1;
                    if r >= y {
                        r -= y;
                    }
                }
            }
        }
    }
    #[test]
    fn reduction_u128() {
        assert_eq!(
            linear_mul_reduction::<u128>(17, 100, 123456789),
            (17 << 100) % 123456789
        );

        // power-of-two divisor
        assert_eq!(
            linear_mul_reduction(0xdead_beef, 100, 1_u128 << 116),
            0xbeef << 100
        );

        let x = 10_u128.pow(37);
        let y = 11_u128.pow(36);
        assert!(x < y);
        let mut r = x;
        for e in 0..1000 {
            assert_eq!(r, linear_mul_reduction(x, e, y));
            // maintain the correct expected remainder
            r <<= 1;
            if r >= y {
                r -= y;
            }
            assert!(r != 0);
        }
    }
}
