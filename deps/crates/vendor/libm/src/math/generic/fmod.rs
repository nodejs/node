/* SPDX-License-Identifier: MIT OR Apache-2.0 */
use crate::support::{CastFrom, Float, Int, MinInt};

#[inline]
pub fn fmod<F: Float>(x: F, y: F) -> F {
    let _1 = F::Int::ONE;
    let sx = x.to_bits() & F::SIGN_MASK;
    let ux = x.to_bits() & !F::SIGN_MASK;
    let uy = y.to_bits() & !F::SIGN_MASK;

    // Cases that return NaN:
    //   NaN % _
    //   Inf % _
    //     _ % NaN
    //     _ % 0
    let x_nan_or_inf = ux & F::EXP_MASK == F::EXP_MASK;
    let y_nan_or_zero = uy.wrapping_sub(_1) & F::EXP_MASK == F::EXP_MASK;
    if x_nan_or_inf | y_nan_or_zero {
        return (x * y) / (x * y);
    }

    if ux < uy {
        // |x| < |y|
        return x;
    }

    let (num, ex) = into_sig_exp::<F>(ux);
    let (div, ey) = into_sig_exp::<F>(uy);

    // To compute `(num << ex) % (div << ey)`, first
    // evaluate `rem = (num << (ex - ey)) % div` ...
    let rem = reduction(num, ex - ey, div);
    // ... so the result will be `rem << ey`

    if rem.is_zero() {
        // Return zero with the sign of `x`
        return F::from_bits(sx);
    };

    // We would shift `rem` up by `ey`, but have to stop at `F::SIG_BITS`
    let shift = ey.min(F::SIG_BITS - rem.ilog2());
    // Anything past that is added to the exponent field
    let bits = (rem << shift) + (F::Int::cast_from(ey - shift) << F::SIG_BITS);
    F::from_bits(sx + bits)
}

/// Given the bits of a finite float, return a tuple of
///  - the mantissa with the implicit bit (0 if subnormal, 1 otherwise)
///  - the additional exponent past 1, (0 for subnormal, 0 or more otherwise)
fn into_sig_exp<F: Float>(mut bits: F::Int) -> (F::Int, u32) {
    bits &= !F::SIGN_MASK;
    // Subtract 1 from the exponent, clamping at 0
    let sat = bits.checked_sub(F::IMPLICIT_BIT).unwrap_or(F::Int::ZERO);
    (
        bits - (sat & F::EXP_MASK),
        u32::cast_from(sat >> F::SIG_BITS),
    )
}

/// Compute the remainder `(x * 2.pow(e)) % y` without overflow.
fn reduction<I: Int>(mut x: I, e: u32, y: I) -> I {
    x %= y;
    for _ in 0..e {
        x <<= 1;
        x = x.checked_sub(y).unwrap_or(x);
    }
    x
}
