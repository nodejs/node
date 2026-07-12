/* SPDX-License-Identifier: MIT
 * origin: musl src/math/floor.c */

//! Generic `floor` algorithm.
//!
//! Note that this uses the algorithm from musl's `floorf` rather than `floor` or `floorl` because
//! performance seems to be better (based on icount) and it does not seem to experience rounding
//! errors on i386.

use crate::support::{Float, FpResult, Int, IntTy, MinInt, Status};

#[inline]
pub fn floor<F: Float>(x: F) -> F {
    floor_status(x).val
}

#[inline]
pub fn floor_status<F: Float>(x: F) -> FpResult<F> {
    let zero = IntTy::<F>::ZERO;

    let mut ix = x.to_bits();
    let e = x.exp_unbiased();

    // If the represented value has no fractional part, no truncation is needed.
    if e >= F::SIG_BITS as i32 {
        return FpResult::ok(x);
    }

    let status;
    let res = if e >= 0 {
        // |x| >= 1.0
        let m = F::SIG_MASK >> e.unsigned();
        if ix & m == zero {
            // Portion to be masked is already zero; no adjustment needed.
            return FpResult::ok(x);
        }

        // Otherwise, raise an inexact exception.
        status = Status::INEXACT;

        if x.is_sign_negative() {
            ix += m;
        }

        ix &= !m;
        F::from_bits(ix)
    } else {
        // |x| < 1.0, raise an inexact exception since truncation will happen.
        if ix & F::SIG_MASK == F::Int::ZERO {
            status = Status::OK;
        } else {
            status = Status::INEXACT;
        }

        if x.is_sign_positive() {
            // 0.0 <= x < 1.0; rounding down goes toward +0.0.
            F::ZERO
        } else if ix << 1 != zero {
            // -1.0 < x < 0.0; rounding down goes toward -1.0.
            F::NEG_ONE
        } else {
            // -0.0 remains unchanged
            x
        }
    };

    FpResult::new(res, status)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::support::Hexf;

    /// Test against https://en.cppreference.com/w/cpp/numeric/math/floor
    fn spec_test<F: Float>(cases: &[(F, F, Status)]) {
        let roundtrip = [
            F::ZERO,
            F::ONE,
            F::NEG_ONE,
            F::NEG_ZERO,
            F::INFINITY,
            F::NEG_INFINITY,
        ];

        for x in roundtrip {
            let FpResult { val, status } = floor_status(x);
            assert_biteq!(val, x, "{}", Hexf(x));
            assert_eq!(status, Status::OK, "{}", Hexf(x));
        }

        for &(x, res, res_stat) in cases {
            let FpResult { val, status } = floor_status(x);
            assert_biteq!(val, res, "{}", Hexf(x));
            assert_eq!(status, res_stat, "{}", Hexf(x));
        }
    }

    /* Skipping f16 / f128 "sanity_check"s and spec cases due to rejected literal lexing at MSRV */

    #[test]
    #[cfg(f16_enabled)]
    fn spec_tests_f16() {
        let cases = [];
        spec_test::<f16>(&cases);
    }

    #[test]
    fn sanity_check_f32() {
        assert_eq!(floor(0.5f32), 0.0);
        assert_eq!(floor(1.1f32), 1.0);
        assert_eq!(floor(2.9f32), 2.0);
    }

    #[test]
    fn spec_tests_f32() {
        let cases = [
            (0.1, 0.0, Status::INEXACT),
            (-0.1, -1.0, Status::INEXACT),
            (0.9, 0.0, Status::INEXACT),
            (-0.9, -1.0, Status::INEXACT),
            (1.1, 1.0, Status::INEXACT),
            (-1.1, -2.0, Status::INEXACT),
            (1.9, 1.0, Status::INEXACT),
            (-1.9, -2.0, Status::INEXACT),
        ];
        spec_test::<f32>(&cases);
    }

    #[test]
    fn sanity_check_f64() {
        assert_eq!(floor(1.1f64), 1.0);
        assert_eq!(floor(2.9f64), 2.0);
    }

    #[test]
    fn spec_tests_f64() {
        let cases = [
            (0.1, 0.0, Status::INEXACT),
            (-0.1, -1.0, Status::INEXACT),
            (0.9, 0.0, Status::INEXACT),
            (-0.9, -1.0, Status::INEXACT),
            (1.1, 1.0, Status::INEXACT),
            (-1.1, -2.0, Status::INEXACT),
            (1.9, 1.0, Status::INEXACT),
            (-1.9, -2.0, Status::INEXACT),
        ];
        spec_test::<f64>(&cases);
    }

    #[test]
    #[cfg(f128_enabled)]
    fn spec_tests_f128() {
        let cases = [];
        spec_test::<f128>(&cases);
    }
}
