/* SPDX-License-Identifier: MIT OR Apache-2.0 */
use crate::support::{CastInto, DInt, HInt, Int, MinInt, u256};

/// Trait for unsigned division of a double-wide integer
/// when the quotient doesn't overflow.
///
/// This is the inverse of widening multiplication:
///  - for any `x` and nonzero `y`: `x.widen_mul(y).checked_narrowing_div_rem(y) == Some((x, 0))`,
///  - and for any `r in 0..y`: `x.carrying_mul(y, r).checked_narrowing_div_rem(y) == Some((x, r))`,
pub trait NarrowingDiv: DInt + MinInt<Unsigned = Self> {
    /// Computes `(self / n, self % n))`
    ///
    /// # Safety
    /// The caller must ensure that `self.hi() < n`, or equivalently,
    /// that the quotient does not overflow.
    unsafe fn unchecked_narrowing_div_rem(self, n: Self::H) -> (Self::H, Self::H);

    /// Returns `Some((self / n, self % n))` when `self.hi() < n`.
    fn checked_narrowing_div_rem(self, n: Self::H) -> Option<(Self::H, Self::H)> {
        if self.hi() < n {
            Some(unsafe { self.unchecked_narrowing_div_rem(n) })
        } else {
            None
        }
    }
}

// For primitive types we can just use the standard
// division operators in the double-wide type.
macro_rules! impl_narrowing_div_primitive {
    ($D:ident) => {
        impl NarrowingDiv for $D {
            unsafe fn unchecked_narrowing_div_rem(self, n: Self::H) -> (Self::H, Self::H) {
                if self.hi() >= n {
                    unsafe { core::hint::unreachable_unchecked() }
                }
                ((self / n.widen()).cast(), (self % n.widen()).cast())
            }
        }
    };
}

// Extend division from `u2N / uN` to `u4N / u2N`
// This is not the most efficient algorithm, but it is
// relatively simple.
macro_rules! impl_narrowing_div_recurse {
    ($D:ident) => {
        impl NarrowingDiv for $D {
            unsafe fn unchecked_narrowing_div_rem(self, n: Self::H) -> (Self::H, Self::H) {
                if self.hi() >= n {
                    unsafe { core::hint::unreachable_unchecked() }
                }

                // Normalize the divisor by shifting the most significant one
                // to the leading position. `n != 0` is implied by `self.hi() < n`
                let lz = n.leading_zeros();
                let a = self << lz;
                let b = n << lz;

                let ah = a.hi();
                let (a0, a1) = a.lo().lo_hi();
                // SAFETY: For both calls, `b.leading_zeros() == 0` by the above shift.
                // SAFETY: `ah < b` follows from `self.hi() < n`
                let (q1, r) = unsafe { div_three_digits_by_two(a1, ah, b) };
                // SAFETY: `r < b` is given as the postcondition of the previous call
                let (q0, r) = unsafe { div_three_digits_by_two(a0, r, b) };

                // Undo the earlier normalization for the remainder
                (Self::H::from_lo_hi(q0, q1), r >> lz)
            }
        }
    };
}

impl_narrowing_div_primitive!(u16);
impl_narrowing_div_primitive!(u32);
impl_narrowing_div_primitive!(u64);
impl_narrowing_div_primitive!(u128);
impl_narrowing_div_recurse!(u256);

/// Implement `u3N / u2N`-division on top of `u2N / uN`-division.
///
/// Returns the quotient and remainder of `(a * R + a0) / n`,
/// where `R = (1 << U::BITS)` is the digit size.
///
/// # Safety
/// Requires that `n.leading_zeros() == 0` and `a < n`.
unsafe fn div_three_digits_by_two<U>(a0: U, a: U::D, n: U::D) -> (U, U::D)
where
    U: HInt,
    U::D: Int + NarrowingDiv,
{
    if n.leading_zeros() > 0 || a >= n {
        unsafe { core::hint::unreachable_unchecked() }
    }

    // n = n1R + n0
    let (n0, n1) = n.lo_hi();
    // a = a2R + a1
    let (a1, a2) = a.lo_hi();

    let mut q;
    let mut r;
    let mut wrap;
    // `a < n` is guaranteed by the caller, but `a2 == n1 && a1 < n0` is possible
    if let Some((q0, r1)) = a.checked_narrowing_div_rem(n1) {
        q = q0;
        // a = qn1 + r1, where 0 <= r1 < n1

        // Include the remainder with the low bits:
        // r = a0 + r1R
        r = U::D::from_lo_hi(a0, r1);

        // Subtract the contribution of the divisor low bits with the estimated quotient
        let d = q.widen_mul(n0);
        (r, wrap) = r.overflowing_sub(d);

        // Since `q` is the quotient of dividing with a slightly smaller divisor,
        // it may be an overapproximation, but is never too small, and similarly,
        // `r` is now either the correct remainder ...
        if !wrap {
            return (q, r);
        }
        // ... or the remainder went "negative" (by as much as `d = qn0 < RR`)
        // and we have to adjust.
        q -= U::ONE;
    } else {
        debug_assert!(a2 == n1 && a1 < n0);
        // Otherwise, `a2 == n1`, and the estimated quotient would be
        // `R + (a1 % n1)`, but the correct quotient can't overflow.
        // We'll start from `q = R = (1 << U::BITS)`,
        // so `r = aR + a0 - qn = (a - n)R + a0`
        r = U::D::from_lo_hi(a0, a1.wrapping_sub(n0));
        // Since `a < n`, the first decrement is always needed:
        q = U::MAX; /* R - 1 */
    }

    (r, wrap) = r.overflowing_add(n);
    if wrap {
        return (q, r);
    }

    // If the remainder still didn't wrap, we need another step.
    q -= U::ONE;
    (r, wrap) = r.overflowing_add(n);
    // Since `n >= RR/2`, at least one of the two `r += n` must have wrapped.
    debug_assert!(wrap, "estimated quotient should be off by at most two");
    (q, r)
}

#[cfg(test)]
mod test {
    use super::{HInt, NarrowingDiv};

    #[test]
    fn inverse_mul() {
        for x in 0..=u8::MAX {
            for y in 1..=u8::MAX {
                let xy = x.widen_mul(y);
                assert_eq!(xy.checked_narrowing_div_rem(y), Some((x, 0)));
                assert_eq!(
                    (xy + (y - 1) as u16).checked_narrowing_div_rem(y),
                    Some((x, y - 1))
                );
                if y > 1 {
                    assert_eq!((xy + 1).checked_narrowing_div_rem(y), Some((x, 1)));
                    assert_eq!(
                        (xy + (y - 2) as u16).checked_narrowing_div_rem(y),
                        Some((x, y - 2))
                    );
                }
            }
        }
    }
}
