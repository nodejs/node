/* SPDX-License-Identifier: MIT */
/* origin: musl src/math/sqrt.c. Ported to generic Rust algorithm in 2025, TG. */

//! Generic square root algorithm.
//!
//! This routine operates around `m_u2`, a U.2 (fixed point with two integral bits) mantissa
//! within the range [1, 4). A table lookup provides an initial estimate, then goldschmidt
//! iterations at various widths are used to approach the real values.
//!
//! For the iterations, `r` is a U0 number that approaches `1/sqrt(m_u2)`, and `s` is a U2 number
//! that approaches `sqrt(m_u2)`. Recall that m_u2 ∈ [1, 4).
//!
//! With Newton-Raphson iterations, this would be:
//!
//! - `w = r * r           w ~ 1 / m`
//! - `u = 3 - m * w       u ~ 3 - m * w = 3 - m / m = 2`
//! - `r = r * u / 2       r ~ r`
//!
//! (Note that the righthand column does not show anything analytically meaningful (i.e. r ~ r),
//! since the value of performing one iteration is in reducing the error representable by `~`).
//!
//! Instead of Newton-Raphson iterations, Goldschmidt iterations are used to calculate
//! `s = m * r`:
//!
//! - `s = m * r           s ~ m / sqrt(m)`
//! - `u = 3 - s * r       u ~ 3 - (m / sqrt(m)) * (1 / sqrt(m)) = 3 - m / m = 2`
//! - `r = r * u / 2       r ~ r`
//! - `s = s * u / 2       s ~ s`
//!
//! The above is precise because it uses the original value `m`. There is also a faster version
//! that performs fewer steps but does not use `m`:
//!
//! - `u = 3 - s * r       u ~ 3 - 1`
//! - `r = r * u / 2       r ~ r`
//! - `s = s * u / 2       s ~ s`
//!
//! Rounding errors accumulate faster with the second version, so it is only used for subsequent
//! iterations within the same width integer. The first version is always used for the first
//! iteration at a new width in order to avoid this accumulation.
//!
//! Goldschmidt has the advantage over Newton-Raphson that `sqrt(x)` and `1/sqrt(x)` are
//! computed at the same time, i.e. there is no need to calculate `1/sqrt(x)` and invert it.

use crate::support::{
    CastFrom, CastInto, DInt, Float, FpResult, HInt, Int, IntTy, MinInt, Round, Status, cold_path,
};

#[inline]
pub fn sqrt<F>(x: F) -> F
where
    F: Float + SqrtHelper,
    F::Int: HInt,
    F::Int: From<u8>,
    F::Int: From<F::ISet2>,
    F::Int: CastInto<F::ISet1>,
    F::Int: CastInto<F::ISet2>,
    u32: CastInto<F::Int>,
{
    sqrt_round(x, Round::Nearest).val
}

#[inline]
pub fn sqrt_round<F>(x: F, _round: Round) -> FpResult<F>
where
    F: Float + SqrtHelper,
    F::Int: HInt,
    F::Int: From<u8>,
    F::Int: From<F::ISet2>,
    F::Int: CastInto<F::ISet1>,
    F::Int: CastInto<F::ISet2>,
    u32: CastInto<F::Int>,
{
    let zero = IntTy::<F>::ZERO;
    let one = IntTy::<F>::ONE;

    let mut ix = x.to_bits();

    // Top is the exponent and sign, which may or may not be shifted. If the float fits into a
    // `u32`, we can get by without paying shifting costs.
    let noshift = F::BITS <= u32::BITS;
    let (mut top, special_case) = if noshift {
        let exp_lsb = one << F::SIG_BITS;
        let special_case = ix.wrapping_sub(exp_lsb) >= F::EXP_MASK - exp_lsb;
        (Exp::NoShift(()), special_case)
    } else {
        let top = u32::cast_from(ix >> F::SIG_BITS);
        let special_case = top.wrapping_sub(1) >= F::EXP_SAT - 1;
        (Exp::Shifted(top), special_case)
    };

    // Handle NaN, zero, and out of domain (<= 0)
    if special_case {
        cold_path();

        // +/-0
        if ix << 1 == zero {
            return FpResult::ok(x);
        }

        // Positive infinity
        if ix == F::EXP_MASK {
            return FpResult::ok(x);
        }

        // NaN or negative
        if ix > F::EXP_MASK {
            return FpResult::new(F::NAN, Status::INVALID);
        }

        // Normalize subnormals by multiplying by 1.0 << SIG_BITS (e.g. 0x1p52 for doubles).
        let scaled = x * F::from_parts(false, F::SIG_BITS + F::EXP_BIAS, zero);
        ix = scaled.to_bits();
        match top {
            Exp::Shifted(ref mut v) => {
                *v = scaled.ex();
                *v = (*v).wrapping_sub(F::SIG_BITS);
            }
            Exp::NoShift(()) => {
                ix = ix.wrapping_sub((F::SIG_BITS << F::SIG_BITS).cast());
            }
        }
    }

    // Reduce arguments such that `x = 4^e * m`:
    //
    // - m_u2 ∈ [1, 4), a fixed point U2.BITS number
    // - 2^e is the exponent part of the result
    let (m_u2, exp) = match top {
        Exp::Shifted(top) => {
            // We now know `x` is positive, so `top` is just its (biased) exponent
            let mut e = top;
            // Construct a fixed point representation of the mantissa.
            let mut m_u2 = (ix | F::IMPLICIT_BIT) << F::EXP_BITS;
            let even = (e & 1) != 0;
            if even {
                m_u2 >>= 1;
            }
            e = (e.wrapping_add(F::EXP_SAT >> 1)) >> 1;
            (m_u2, Exp::Shifted(e))
        }
        Exp::NoShift(()) => {
            let even = ix & (one << F::SIG_BITS) != zero;

            // Exponent part of the return value
            let mut e_noshift = ix >> 1;
            // ey &= (F::EXP_MASK << 2) >> 2; // clear the top exponent bit (result = 1.0)
            e_noshift += (F::EXP_MASK ^ (F::SIGN_MASK >> 1)) >> 1;
            e_noshift &= F::EXP_MASK;

            let m1 = (ix << F::EXP_BITS) | F::SIGN_MASK;
            let m0 = (ix << (F::EXP_BITS - 1)) & !F::SIGN_MASK;
            let m_u2 = if even { m0 } else { m1 };

            (m_u2, Exp::NoShift(e_noshift))
        }
    };

    // Extract the top 6 bits of the significand with the lowest bit of the exponent.
    let i = usize::cast_from(ix >> (F::SIG_BITS - 6)) & 0b1111111;

    // Start with an initial guess for `r = 1 / sqrt(m)` from the table, and shift `m` as an
    // initial value for `s = sqrt(m)`. See the module documentation for details.
    let r1_u0: F::ISet1 = F::ISet1::cast_from(RSQRT_TAB[i]) << (F::ISet1::BITS - 16);
    let s1_u2: F::ISet1 = ((m_u2) >> (F::BITS - F::ISet1::BITS)).cast();

    // Perform iterations, if any, at quarter width (used for `f128`).
    let (r1_u0, _s1_u2) = goldschmidt::<F, F::ISet1>(r1_u0, s1_u2, F::SET1_ROUNDS, false);

    // Widen values and perform iterations at half width (used for `f64` and `f128`).
    let r2_u0: F::ISet2 = F::ISet2::from(r1_u0) << (F::ISet2::BITS - F::ISet1::BITS);
    let s2_u2: F::ISet2 = ((m_u2) >> (F::BITS - F::ISet2::BITS)).cast();
    let (r2_u0, _s2_u2) = goldschmidt::<F, F::ISet2>(r2_u0, s2_u2, F::SET2_ROUNDS, false);

    // Perform final iterations at full width (used for all float types).
    let r_u0: F::Int = F::Int::from(r2_u0) << (F::BITS - F::ISet2::BITS);
    let s_u2: F::Int = m_u2;
    let (_r_u0, s_u2) = goldschmidt::<F, F::Int>(r_u0, s_u2, F::FINAL_ROUNDS, true);

    // Shift back to mantissa position.
    let mut m = s_u2 >> (F::EXP_BITS - 2);

    // The musl source includes the following comment (with literals replaced):
    //
    // > s < sqrt(m) < s + 0x1.09p-SIG_BITS
    // > compute nearest rounded result: the nearest result to SIG_BITS bits is either s or
    // > s+0x1p-SIG_BITS, we can decide by comparing (2^SIG_BITS s + 0.5)^2 to 2^(2*SIG_BITS) m.
    //
    // Expanding this with , with `SIG_BITS = p` and adjusting based on the operations done to
    // `d0` and `d1`:
    //
    // - `2^(2p)m ≟ ((2^p)m + 0.5)^2`
    // - `2^(2p)m ≟ 2^(2p)m^2 + (2^p)m + 0.25`
    // - `2^(2p)m - m^2 ≟ (2^(2p) - 1)m^2 + (2^p)m + 0.25`
    // - `(1 - 2^(2p))m + m^2 ≟ (1 - 2^(2p))m^2 + (1 - 2^p)m + 0.25` (?)
    //
    // I do not follow how the rounding bit is extracted from this comparison with the below
    // operations. In any case, the algorithm is well tested.

    // The value needed to shift `m_u2` by to create `m*2^(2p)`. `2p = 2 * F::SIG_BITS`,
    // `F::BITS - 2` accounts for the offset that `m_u2` already has.
    let shift = 2 * F::SIG_BITS - (F::BITS - 2);

    // `2^(2p)m - m^2`
    let d0 = (m_u2 << shift).wrapping_sub(m.wrapping_mul(m));
    // `m - 2^(2p)m + m^2`
    let d1 = m.wrapping_sub(d0);
    m += d1 >> (F::BITS - 1);
    m &= F::SIG_MASK;

    match exp {
        Exp::Shifted(e) => m |= IntTy::<F>::cast_from(e) << F::SIG_BITS,
        Exp::NoShift(e) => m |= e,
    };

    let mut y = F::from_bits(m);

    // FIXME(f16): the fenv math does not work for `f16`
    if F::BITS > 16 {
        // Handle rounding and inexact. `(m + 1)^2 == 2^shift m` is exact; for all other cases, add
        // a tiny value to cause fenv effects.
        let d2 = d1.wrapping_add(m).wrapping_add(one);
        let mut tiny = if d2 == zero {
            cold_path();
            zero
        } else {
            F::IMPLICIT_BIT
        };

        tiny |= (d1 ^ d2) & F::SIGN_MASK;
        let t = F::from_bits(tiny);
        y = y + t;
    }

    FpResult::ok(y)
}

/// Multiply at the wider integer size, returning the high half.
fn wmulh<I: HInt>(a: I, b: I) -> I {
    a.widen_mul(b).hi()
}

/// Perform `count` goldschmidt iterations, returning `(r_u0, s_u?)`.
///
/// - `r_u0` is the reciprocal `r ~ 1 / sqrt(m)`, as U0.
/// - `s_u2` is the square root, `s ~ sqrt(m)`, as U2.
/// - `count` is the number of iterations to perform.
/// - `final_set` should be true if this is the last round (same-sized integer). If so, the
///   returned `s` will be U3, for later shifting. Otherwise, the returned `s` is U2.
///
/// Note that performance relies on the optimizer being able to unroll these loops (reasonably
/// trivial, `count` is a constant when called).
#[inline]
fn goldschmidt<F, I>(mut r_u0: I, mut s_u2: I, count: u32, final_set: bool) -> (I, I)
where
    F: SqrtHelper,
    I: HInt + From<u8>,
{
    let three_u2 = I::from(0b11u8) << (I::BITS - 2);
    let mut u_u0 = r_u0;

    for i in 0..count {
        // First iteration: `s = m*r` (`u_u0 = r_u0` set above)
        // Subsequent iterations: `s=s*u/2`
        s_u2 = wmulh(s_u2, u_u0);

        // Perform `s /= 2` if:
        //
        // 1. This is not the first iteration (the first iteration is `s = m*r`)...
        // 2. ... and this is not the last set of iterations
        // 3. ... or, if this is the last set, it is not the last iteration
        //
        // This step is not performed for the final iteration because the shift is combined with
        // a later shift (moving `s` into the mantissa).
        if i > 0 && (!final_set || i + 1 < count) {
            s_u2 <<= 1;
        }

        // u = 3 - s*r
        let d_u2 = wmulh(s_u2, r_u0);
        u_u0 = three_u2.wrapping_sub(d_u2);

        // r = r*u/2
        r_u0 = wmulh(r_u0, u_u0) << 1;
    }

    (r_u0, s_u2)
}

/// Representation of whether we shift the exponent into a `u32`, or modify it in place to save
/// the shift operations.
enum Exp<T> {
    /// The exponent has been shifted to a `u32` and is LSB-aligned.
    Shifted(u32),
    /// The exponent is in its natural position in integer repr.
    NoShift(T),
}

/// Size-specific constants related to the square root routine.
pub trait SqrtHelper: Float {
    /// Integer for the first set of rounds. If unused, set to the same type as the next set.
    type ISet1: HInt + Into<Self::ISet2> + CastFrom<Self::Int> + From<u8>;
    /// Integer for the second set of rounds. If unused, set to the same type as the next set.
    type ISet2: HInt + From<Self::ISet1> + From<u8>;

    /// Number of rounds at `ISet1`.
    const SET1_ROUNDS: u32 = 0;
    /// Number of rounds at `ISet2`.
    const SET2_ROUNDS: u32 = 0;
    /// Number of rounds at `Self::Int`.
    const FINAL_ROUNDS: u32;
}

#[cfg(f16_enabled)]
impl SqrtHelper for f16 {
    type ISet1 = u16; // unused
    type ISet2 = u16; // unused

    const FINAL_ROUNDS: u32 = 2;
}

impl SqrtHelper for f32 {
    type ISet1 = u32; // unused
    type ISet2 = u32; // unused

    const FINAL_ROUNDS: u32 = 3;
}

impl SqrtHelper for f64 {
    type ISet1 = u32; // unused
    type ISet2 = u32;

    const SET2_ROUNDS: u32 = 2;
    const FINAL_ROUNDS: u32 = 2;
}

#[cfg(f128_enabled)]
impl SqrtHelper for f128 {
    type ISet1 = u32;
    type ISet2 = u64;

    const SET1_ROUNDS: u32 = 1;
    const SET2_ROUNDS: u32 = 2;
    const FINAL_ROUNDS: u32 = 2;
}

/// A U0.16 representation of `1/sqrt(x)`.
///
/// The index is a 7-bit number consisting of a single exponent bit and 6 bits of significand.
#[rustfmt::skip]
static RSQRT_TAB: [u16; 128] = [
    0xb451, 0xb2f0, 0xb196, 0xb044, 0xaef9, 0xadb6, 0xac79, 0xab43,
    0xaa14, 0xa8eb, 0xa7c8, 0xa6aa, 0xa592, 0xa480, 0xa373, 0xa26b,
    0xa168, 0xa06a, 0x9f70, 0x9e7b, 0x9d8a, 0x9c9d, 0x9bb5, 0x9ad1,
    0x99f0, 0x9913, 0x983a, 0x9765, 0x9693, 0x95c4, 0x94f8, 0x9430,
    0x936b, 0x92a9, 0x91ea, 0x912e, 0x9075, 0x8fbe, 0x8f0a, 0x8e59,
    0x8daa, 0x8cfe, 0x8c54, 0x8bac, 0x8b07, 0x8a64, 0x89c4, 0x8925,
    0x8889, 0x87ee, 0x8756, 0x86c0, 0x862b, 0x8599, 0x8508, 0x8479,
    0x83ec, 0x8361, 0x82d8, 0x8250, 0x81c9, 0x8145, 0x80c2, 0x8040,
    0xff02, 0xfd0e, 0xfb25, 0xf947, 0xf773, 0xf5aa, 0xf3ea, 0xf234,
    0xf087, 0xeee3, 0xed47, 0xebb3, 0xea27, 0xe8a3, 0xe727, 0xe5b2,
    0xe443, 0xe2dc, 0xe17a, 0xe020, 0xdecb, 0xdd7d, 0xdc34, 0xdaf1,
    0xd9b3, 0xd87b, 0xd748, 0xd61a, 0xd4f1, 0xd3cd, 0xd2ad, 0xd192,
    0xd07b, 0xcf69, 0xce5b, 0xcd51, 0xcc4a, 0xcb48, 0xca4a, 0xc94f,
    0xc858, 0xc764, 0xc674, 0xc587, 0xc49d, 0xc3b7, 0xc2d4, 0xc1f4,
    0xc116, 0xc03c, 0xbf65, 0xbe90, 0xbdbe, 0xbcef, 0xbc23, 0xbb59,
    0xba91, 0xb9cc, 0xb90a, 0xb84a, 0xb78c, 0xb6d0, 0xb617, 0xb560,
];

#[cfg(test)]
mod tests {
    use super::*;

    /// Test behavior specified in IEEE 754 `squareRoot`.
    fn spec_test<F>()
    where
        F: Float + SqrtHelper,
        F::Int: HInt,
        F::Int: From<u8>,
        F::Int: From<F::ISet2>,
        F::Int: CastInto<F::ISet1>,
        F::Int: CastInto<F::ISet2>,
        u32: CastInto<F::Int>,
    {
        // Values that should return a NaN and raise invalid
        let nan = [F::NEG_INFINITY, F::NEG_ONE, F::NAN, F::MIN];

        // Values that return unaltered
        let roundtrip = [F::ZERO, F::NEG_ZERO, F::INFINITY];

        for x in nan {
            let FpResult { val, status } = sqrt_round(x, Round::Nearest);
            assert!(val.is_nan());
            assert!(status == Status::INVALID);
        }

        for x in roundtrip {
            let FpResult { val, status } = sqrt_round(x, Round::Nearest);
            assert_biteq!(val, x);
            assert!(status == Status::OK);
        }
    }

    #[test]
    #[cfg(f16_enabled)]
    fn sanity_check_f16() {
        assert_biteq!(sqrt(100.0f16), 10.0);
        assert_biteq!(sqrt(4.0f16), 2.0);
    }

    #[test]
    #[cfg(f16_enabled)]
    fn spec_tests_f16() {
        spec_test::<f16>();
    }

    #[test]
    #[cfg(f16_enabled)]
    #[allow(clippy::approx_constant)]
    fn conformance_tests_f16() {
        let cases = [
            (f16::PI, 0x3f17_u16),
            // 10_000.0, using a hex literal for MSRV hack (Rust < 1.67 checks literal widths as
            // part of the AST, so the `cfg` is irrelevant here).
            (f16::from_bits(0x70e2), 0x5640_u16),
            (f16::from_bits(0x0000000f), 0x13bf_u16),
            (f16::INFINITY, f16::INFINITY.to_bits()),
        ];

        for (input, output) in cases {
            assert_biteq!(
                sqrt(input),
                f16::from_bits(output),
                "input: {input:?} ({:#018x})",
                input.to_bits()
            );
        }
    }

    #[test]
    fn sanity_check_f32() {
        assert_biteq!(sqrt(100.0f32), 10.0);
        assert_biteq!(sqrt(4.0f32), 2.0);
    }

    #[test]
    fn spec_tests_f32() {
        spec_test::<f32>();
    }

    #[test]
    #[allow(clippy::approx_constant)]
    fn conformance_tests_f32() {
        let cases = [
            (f32::PI, 0x3fe2dfc5_u32),
            (10000.0f32, 0x42c80000_u32),
            (f32::from_bits(0x0000000f), 0x1b2f456f_u32),
            (f32::INFINITY, f32::INFINITY.to_bits()),
        ];

        for (input, output) in cases {
            assert_biteq!(
                sqrt(input),
                f32::from_bits(output),
                "input: {input:?} ({:#018x})",
                input.to_bits()
            );
        }
    }

    #[test]
    fn sanity_check_f64() {
        assert_biteq!(sqrt(100.0f64), 10.0);
        assert_biteq!(sqrt(4.0f64), 2.0);
    }

    #[test]
    fn spec_tests_f64() {
        spec_test::<f64>();
    }

    #[test]
    #[allow(clippy::approx_constant)]
    fn conformance_tests_f64() {
        let cases = [
            (f64::PI, 0x3ffc5bf891b4ef6a_u64),
            (10000.0, 0x4059000000000000_u64),
            (f64::from_bits(0x0000000f), 0x1e7efbdeb14f4eda_u64),
            (f64::INFINITY, f64::INFINITY.to_bits()),
        ];

        for (input, output) in cases {
            assert_biteq!(
                sqrt(input),
                f64::from_bits(output),
                "input: {input:?} ({:#018x})",
                input.to_bits()
            );
        }
    }

    #[test]
    #[cfg(f128_enabled)]
    fn sanity_check_f128() {
        assert_biteq!(sqrt(100.0f128), 10.0);
        assert_biteq!(sqrt(4.0f128), 2.0);
    }

    #[test]
    #[cfg(f128_enabled)]
    fn spec_tests_f128() {
        spec_test::<f128>();
    }

    #[test]
    #[cfg(f128_enabled)]
    #[allow(clippy::approx_constant)]
    fn conformance_tests_f128() {
        let cases = [
            (f128::PI, 0x3fffc5bf891b4ef6aa79c3b0520d5db9_u128),
            // 10_000.0, see `f16` for reasoning.
            (
                f128::from_bits(0x400c3880000000000000000000000000),
                0x40059000000000000000000000000000_u128,
            ),
            (
                f128::from_bits(0x0000000f),
                0x1fc9efbdeb14f4ed9b17ae807907e1e9_u128,
            ),
            (f128::INFINITY, f128::INFINITY.to_bits()),
        ];

        for (input, output) in cases {
            assert_biteq!(
                sqrt(input),
                f128::from_bits(output),
                "input: {input:?} ({:#018x})",
                input.to_bits()
            );
        }
    }
}
