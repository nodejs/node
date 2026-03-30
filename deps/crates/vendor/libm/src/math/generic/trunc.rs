/* SPDX-License-Identifier: MIT
 * origin: musl src/math/trunc.c */

use crate::support::{Float, FpResult, Int, IntTy, MinInt, Status};

#[inline]
pub fn trunc<F: Float>(x: F) -> F {
    trunc_status(x).val
}

#[inline]
pub fn trunc_status<F: Float>(x: F) -> FpResult<F> {
    let mut xi: F::Int = x.to_bits();
    let e: i32 = x.exp_unbiased();

    // C1: The represented value has no fractional part, so no truncation is needed
    if e >= F::SIG_BITS as i32 {
        return FpResult::ok(x);
    }

    let mask = if e < 0 {
        // C2: If the exponent is negative, the result will be zero so we mask out everything
        // except the sign.
        F::SIGN_MASK
    } else {
        // C3: Otherwise, we mask out the last `e` bits of the significand.
        !(F::SIG_MASK >> e.unsigned())
    };

    // C4: If the to-be-masked-out portion is already zero, we have an exact result
    if (xi & !mask) == IntTy::<F>::ZERO {
        return FpResult::ok(x);
    }

    // C5: Otherwise the result is inexact and we will truncate. Raise `FE_INEXACT`, mask the
    // result, and return.

    let status = if xi & F::SIG_MASK == F::Int::ZERO {
        Status::OK
    } else {
        Status::INEXACT
    };
    xi &= mask;
    FpResult::new(F::from_bits(xi), status)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::support::Hexf;

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
            let FpResult { val, status } = trunc_status(x);
            assert_biteq!(val, x, "{}", Hexf(x));
            assert_eq!(status, Status::OK, "{}", Hexf(x));
        }

        for &(x, res, res_stat) in cases {
            let FpResult { val, status } = trunc_status(x);
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
        assert_eq!(trunc(0.5f32), 0.0);
        assert_eq!(trunc(1.1f32), 1.0);
        assert_eq!(trunc(2.9f32), 2.0);
    }

    #[test]
    fn spec_tests_f32() {
        let cases = [
            (0.1, 0.0, Status::INEXACT),
            (-0.1, -0.0, Status::INEXACT),
            (0.9, 0.0, Status::INEXACT),
            (-0.9, -0.0, Status::INEXACT),
            (1.1, 1.0, Status::INEXACT),
            (-1.1, -1.0, Status::INEXACT),
            (1.9, 1.0, Status::INEXACT),
            (-1.9, -1.0, Status::INEXACT),
        ];
        spec_test::<f32>(&cases);

        assert_biteq!(trunc(1.1f32), 1.0);
        assert_biteq!(trunc(1.1f64), 1.0);

        // C1
        assert_biteq!(trunc(hf32!("0x1p23")), hf32!("0x1p23"));
        assert_biteq!(trunc(hf64!("0x1p52")), hf64!("0x1p52"));
        assert_biteq!(trunc(hf32!("-0x1p23")), hf32!("-0x1p23"));
        assert_biteq!(trunc(hf64!("-0x1p52")), hf64!("-0x1p52"));

        // C2
        assert_biteq!(trunc(hf32!("0x1p-1")), 0.0);
        assert_biteq!(trunc(hf64!("0x1p-1")), 0.0);
        assert_biteq!(trunc(hf32!("-0x1p-1")), -0.0);
        assert_biteq!(trunc(hf64!("-0x1p-1")), -0.0);
    }

    #[test]
    fn sanity_check_f64() {
        assert_eq!(trunc(1.1f64), 1.0);
        assert_eq!(trunc(2.9f64), 2.0);
    }

    #[test]
    fn spec_tests_f64() {
        let cases = [
            (0.1, 0.0, Status::INEXACT),
            (-0.1, -0.0, Status::INEXACT),
            (0.9, 0.0, Status::INEXACT),
            (-0.9, -0.0, Status::INEXACT),
            (1.1, 1.0, Status::INEXACT),
            (-1.1, -1.0, Status::INEXACT),
            (1.9, 1.0, Status::INEXACT),
            (-1.9, -1.0, Status::INEXACT),
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
