/* SPDX-License-Identifier: MIT OR Apache-2.0 */
use crate::support::{CastFrom, CastInto, Float, HInt, Int, MinInt, NarrowingDiv};

#[inline]
pub fn fmod<F: Float>(x: F, y: F) -> F
where
    F::Int: HInt,
    <F::Int as HInt>::D: NarrowingDiv,
{
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
    let rem = reduction::<F>(num, ex - ey, div);
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
fn reduction<F>(mut x: F::Int, e: u32, y: F::Int) -> F::Int
where
    F: Float,
    F::Int: HInt,
    <<F as Float>::Int as HInt>::D: NarrowingDiv,
{
    // `f16` only has 5 exponent bits, so even `f16::MAX = 65504.0` is only
    // a 40-bit integer multiple of the smallest subnormal.
    if F::BITS == 16 {
        debug_assert!(F::EXP_MAX - F::EXP_MIN == 29);
        debug_assert!(e <= 29);
        let u: u16 = x.cast();
        let v: u16 = y.cast();
        let u = (u as u64) << e;
        let v = v as u64;
        return F::Int::cast_from((u % v) as u16);
    }

    // Ensure `x < 2y` for later steps
    if x >= (y << 1) {
        // This case is only reached with subnormal divisors,
        // but it might be better to just normalize all significands
        // to make this unnecessary. The further calls could potentially
        // benefit from assuming a specific fixed leading bit position.
        x %= y;
    }

    // The simple implementation seems to be fastest for a short reduction
    // at this size. The limit here was chosen empirically on an Intel Nehalem.
    // Less old CPUs that have faster `u64 * u64 -> u128` might not benefit,
    // and 32-bit systems or architectures without hardware multipliers might
    // want to do this in more cases.
    if F::BITS == 64 && e < 32 {
        // Assumes `x < 2y`
        for _ in 0..e {
            x = x.checked_sub(y).unwrap_or(x);
            x <<= 1;
        }
        return x.checked_sub(y).unwrap_or(x);
    }

    // Fast path for short reductions
    if e < F::BITS {
        let w = x.widen() << e;
        if let Some((_, r)) = w.checked_narrowing_div_rem(y) {
            return r;
        }
    }

    // Assumes `x < 2y`
    crate::support::linear_mul_reduction(x, e, y)
}
