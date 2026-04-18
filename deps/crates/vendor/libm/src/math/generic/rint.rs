/* SPDX-License-Identifier: MIT */
/* origin: musl src/math/rint.c */

use crate::support::{Float, FpResult, Round};

/// IEEE 754-2019 `roundToIntegralExact`, which respects rounding mode and raises inexact if
/// applicable.
#[inline]
pub fn rint_round<F: Float>(x: F, _round: Round) -> FpResult<F> {
    let toint = F::ONE / F::EPSILON;
    let e = x.ex();
    let positive = x.is_sign_positive();

    // On i386 `force_eval!` must be used to force rounding via storage to memory. Otherwise,
    // the excess precission from x87 would cause an incorrect final result.
    let force = |x| {
        if cfg!(x86_no_sse) && (F::BITS == 32 || F::BITS == 64) {
            force_eval!(x)
        } else {
            x
        }
    };

    let res = if e >= F::EXP_BIAS + F::SIG_BITS {
        // No fractional part; exact result can be returned.
        x
    } else {
        // Apply a net-zero adjustment that nudges `y` in the direction of the rounding mode. For
        // Rust this is always nearest, but ideally it would take `round` into account.
        let y = if positive {
            force(force(x) + toint) - toint
        } else {
            force(force(x) - toint) + toint
        };

        if y == F::ZERO {
            // A zero result takes the sign of the input.
            if positive { F::ZERO } else { F::NEG_ZERO }
        } else {
            y
        }
    };

    FpResult::ok(res)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::support::{Hexf, Status};

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
            let FpResult { val, status } = rint_round(x, Round::Nearest);
            assert_biteq!(val, x, "rint_round({})", Hexf(x));
            assert_eq!(status, Status::OK, "{}", Hexf(x));
        }

        for &(x, res, res_stat) in cases {
            let FpResult { val, status } = rint_round(x, Round::Nearest);
            assert_biteq!(val, res, "rint_round({})", Hexf(x));
            assert_eq!(status, res_stat, "{}", Hexf(x));
        }
    }

    #[test]
    #[cfg(f16_enabled)]
    fn spec_tests_f16() {
        let cases = [];
        spec_test::<f16>(&cases);
    }

    #[test]
    fn spec_tests_f32() {
        let cases = [
            (0.1, 0.0, Status::OK),
            (-0.1, -0.0, Status::OK),
            (0.5, 0.0, Status::OK),
            (-0.5, -0.0, Status::OK),
            (0.9, 1.0, Status::OK),
            (-0.9, -1.0, Status::OK),
            (1.1, 1.0, Status::OK),
            (-1.1, -1.0, Status::OK),
            (1.5, 2.0, Status::OK),
            (-1.5, -2.0, Status::OK),
            (1.9, 2.0, Status::OK),
            (-1.9, -2.0, Status::OK),
            (2.8, 3.0, Status::OK),
            (-2.8, -3.0, Status::OK),
        ];
        spec_test::<f32>(&cases);
    }

    #[test]
    fn spec_tests_f64() {
        let cases = [
            (0.1, 0.0, Status::OK),
            (-0.1, -0.0, Status::OK),
            (0.5, 0.0, Status::OK),
            (-0.5, -0.0, Status::OK),
            (0.9, 1.0, Status::OK),
            (-0.9, -1.0, Status::OK),
            (1.1, 1.0, Status::OK),
            (-1.1, -1.0, Status::OK),
            (1.5, 2.0, Status::OK),
            (-1.5, -2.0, Status::OK),
            (1.9, 2.0, Status::OK),
            (-1.9, -2.0, Status::OK),
            (2.8, 3.0, Status::OK),
            (-2.8, -3.0, Status::OK),
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
