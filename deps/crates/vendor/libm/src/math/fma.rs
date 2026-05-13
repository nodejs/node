/* SPDX-License-Identifier: MIT */
/* origin: musl src/math/fma.c, fmaf.c Ported to generic Rust algorithm in 2025, TG. */

use super::generic;
use crate::support::Round;

// Placeholder so we can have `fmaf16` in the `Float` trait.
#[allow(unused)]
#[cfg(f16_enabled)]
#[cfg_attr(assert_no_panic, no_panic::no_panic)]
pub(crate) fn fmaf16(_x: f16, _y: f16, _z: f16) -> f16 {
    unimplemented!()
}

/// Floating multiply add (f32)
///
/// Computes `(x*y)+z`, rounded as one ternary operation (i.e. calculated with infinite precision).
#[cfg_attr(assert_no_panic, no_panic::no_panic)]
pub fn fmaf(x: f32, y: f32, z: f32) -> f32 {
    select_implementation! {
        name: fmaf,
        use_arch: any(
            all(target_arch = "aarch64", target_feature = "neon"),
            target_feature = "sse2",
        ),
        args: x, y, z,
    }

    generic::fma_wide_round(x, y, z, Round::Nearest).val
}

/// Fused multiply add (f64)
///
/// Computes `(x*y)+z`, rounded as one ternary operation (i.e. calculated with infinite precision).
#[cfg_attr(assert_no_panic, no_panic::no_panic)]
pub fn fma(x: f64, y: f64, z: f64) -> f64 {
    select_implementation! {
        name: fma,
        use_arch: any(
            all(target_arch = "aarch64", target_feature = "neon"),
            target_feature = "sse2",
        ),
        args: x, y, z,
    }

    generic::fma_round(x, y, z, Round::Nearest).val
}

/// Fused multiply add (f128)
///
/// Computes `(x*y)+z`, rounded as one ternary operation (i.e. calculated with infinite precision).
#[cfg(f128_enabled)]
#[cfg_attr(assert_no_panic, no_panic::no_panic)]
pub fn fmaf128(x: f128, y: f128, z: f128) -> f128 {
    generic::fma_round(x, y, z, Round::Nearest).val
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::support::{CastFrom, CastInto, Float, FpResult, HInt, MinInt, Round, Status};

    /// Test the generic `fma_round` algorithm for a given float.
    fn spec_test<F>(f: impl Fn(F, F, F) -> F)
    where
        F: Float,
        F: CastFrom<F::SignedInt>,
        F: CastFrom<i8>,
        F::Int: HInt,
        u32: CastInto<F::Int>,
    {
        let x = F::from_bits(F::Int::ONE);
        let y = F::from_bits(F::Int::ONE);
        let z = F::ZERO;

        // 754-2020 says "When the exact result of (a × b) + c is non-zero yet the result of
        // fusedMultiplyAdd is zero because of rounding, the zero result takes the sign of the
        // exact result"
        assert_biteq!(f(x, y, z), F::ZERO);
        assert_biteq!(f(x, -y, z), F::NEG_ZERO);
        assert_biteq!(f(-x, y, z), F::NEG_ZERO);
        assert_biteq!(f(-x, -y, z), F::ZERO);
    }

    #[test]
    fn spec_test_f32() {
        spec_test::<f32>(fmaf);

        // Also do a small check that the non-widening version works for f32 (this should ideally
        // get tested some more).
        spec_test::<f32>(|x, y, z| generic::fma_round(x, y, z, Round::Nearest).val);
    }

    #[test]
    fn spec_test_f64() {
        spec_test::<f64>(fma);

        let expect_underflow = [
            (
                hf64!("0x1.0p-1070"),
                hf64!("0x1.0p-1070"),
                hf64!("0x1.ffffffffffffp-1023"),
                hf64!("0x0.ffffffffffff8p-1022"),
            ),
            (
                // FIXME: we raise underflow but this should only be inexact (based on C and
                // `rustc_apfloat`).
                hf64!("0x1.0p-1070"),
                hf64!("0x1.0p-1070"),
                hf64!("-0x1.0p-1022"),
                hf64!("-0x1.0p-1022"),
            ),
        ];

        for (x, y, z, res) in expect_underflow {
            let FpResult { val, status } = generic::fma_round(x, y, z, Round::Nearest);
            assert_biteq!(val, res);
            assert_eq!(status, Status::UNDERFLOW);
        }
    }

    #[test]
    #[cfg(f128_enabled)]
    fn spec_test_f128() {
        spec_test::<f128>(fmaf128);
    }

    #[test]
    fn issue_263() {
        let a = f32::from_bits(1266679807);
        let b = f32::from_bits(1300234242);
        let c = f32::from_bits(1115553792);
        let expected = f32::from_bits(1501560833);
        assert_eq!(fmaf(a, b, c), expected);
    }

    #[test]
    fn fma_segfault() {
        // These two inputs cause fma to segfault on release due to overflow:
        assert_eq!(
            fma(
                -0.0000000000000002220446049250313,
                -0.0000000000000002220446049250313,
                -0.0000000000000002220446049250313
            ),
            -0.00000000000000022204460492503126,
        );

        let result = fma(-0.992, -0.992, -0.992);
        //force rounding to storage format on x87 to prevent superious errors.
        #[cfg(all(target_arch = "x86", not(target_feature = "sse2")))]
        let result = force_eval!(result);
        assert_eq!(result, -0.007936000000000007,);
    }

    #[test]
    fn fma_sbb() {
        assert_eq!(
            fma(-(1.0 - f64::EPSILON), f64::MIN, f64::MIN),
            -3991680619069439e277
        );
    }

    #[test]
    fn fma_underflow() {
        assert_eq!(
            fma(1.1102230246251565e-16, -9.812526705433188e-305, 1.0894e-320),
            0.0,
        );
    }
}
