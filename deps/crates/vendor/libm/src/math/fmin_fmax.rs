/// Return the lesser of two arguments or, if either argument is NaN, the other argument.
///
/// This coincides with IEEE 754-2011 `minNum`. The result disregards signed zero (meaning if
/// the inputs are -0.0 and +0.0, either may be returned).
#[cfg(f16_enabled)]
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn fminf16(x: f16, y: f16) -> f16 {
    super::generic::fmin(x, y)
}

/// Return the lesser of two arguments or, if either argument is NaN, the other argument.
///
/// This coincides with IEEE 754-2011 `minNum`. The result disregards signed zero (meaning if
/// the inputs are -0.0 and +0.0, either may be returned).
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn fminf(x: f32, y: f32) -> f32 {
    super::generic::fmin(x, y)
}

/// Return the lesser of two arguments or, if either argument is NaN, the other argument.
///
/// This coincides with IEEE 754-2011 `minNum`. The result disregards signed zero (meaning if
/// the inputs are -0.0 and +0.0, either may be returned).
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn fmin(x: f64, y: f64) -> f64 {
    super::generic::fmin(x, y)
}

/// Return the lesser of two arguments or, if either argument is NaN, the other argument.
///
/// This coincides with IEEE 754-2011 `minNum`. The result disregards signed zero (meaning if
/// the inputs are -0.0 and +0.0, either may be returned).
#[cfg(f128_enabled)]
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn fminf128(x: f128, y: f128) -> f128 {
    super::generic::fmin(x, y)
}

/// Return the greater of two arguments or, if either argument is NaN, the other argument.
///
/// This coincides with IEEE 754-2011 `maxNum`. The result disregards signed zero (meaning if
/// the inputs are -0.0 and +0.0, either may be returned).
#[cfg(f16_enabled)]
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn fmaxf16(x: f16, y: f16) -> f16 {
    super::generic::fmax(x, y)
}

/// Return the greater of two arguments or, if either argument is NaN, the other argument.
///
/// This coincides with IEEE 754-2011 `maxNum`. The result disregards signed zero (meaning if
/// the inputs are -0.0 and +0.0, either may be returned).
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn fmaxf(x: f32, y: f32) -> f32 {
    super::generic::fmax(x, y)
}

/// Return the greater of two arguments or, if either argument is NaN, the other argument.
///
/// This coincides with IEEE 754-2011 `maxNum`. The result disregards signed zero (meaning if
/// the inputs are -0.0 and +0.0, either may be returned).
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn fmax(x: f64, y: f64) -> f64 {
    super::generic::fmax(x, y)
}

/// Return the greater of two arguments or, if either argument is NaN, the other argument.
///
/// This coincides with IEEE 754-2011 `maxNum`. The result disregards signed zero (meaning if
/// the inputs are -0.0 and +0.0, either may be returned).
#[cfg(f128_enabled)]
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn fmaxf128(x: f128, y: f128) -> f128 {
    super::generic::fmax(x, y)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::support::{Float, Hexf};

    fn fmin_spec_test<F: Float>(f: impl Fn(F, F) -> F) {
        let cases = [
            (F::ZERO, F::ZERO, F::ZERO),
            (F::ONE, F::ONE, F::ONE),
            (F::ZERO, F::ONE, F::ZERO),
            (F::ONE, F::ZERO, F::ZERO),
            (F::ZERO, F::NEG_ONE, F::NEG_ONE),
            (F::NEG_ONE, F::ZERO, F::NEG_ONE),
            (F::INFINITY, F::ZERO, F::ZERO),
            (F::NEG_INFINITY, F::ZERO, F::NEG_INFINITY),
            (F::NAN, F::ZERO, F::ZERO),
            (F::ZERO, F::NAN, F::ZERO),
            (F::NAN, F::NAN, F::NAN),
        ];

        for (x, y, res) in cases {
            let val = f(x, y);
            assert_biteq!(val, res, "fmin({}, {})", Hexf(x), Hexf(y));
        }
    }

    #[test]
    #[cfg(f16_enabled)]
    fn fmin_spec_tests_f16() {
        fmin_spec_test::<f16>(fminf16);
    }

    #[test]
    fn fmin_spec_tests_f32() {
        fmin_spec_test::<f32>(fminf);
    }

    #[test]
    fn fmin_spec_tests_f64() {
        fmin_spec_test::<f64>(fmin);
    }

    #[test]
    #[cfg(f128_enabled)]
    fn fmin_spec_tests_f128() {
        fmin_spec_test::<f128>(fminf128);
    }

    fn fmax_spec_test<F: Float>(f: impl Fn(F, F) -> F) {
        let cases = [
            (F::ZERO, F::ZERO, F::ZERO),
            (F::ONE, F::ONE, F::ONE),
            (F::ZERO, F::ONE, F::ONE),
            (F::ONE, F::ZERO, F::ONE),
            (F::ZERO, F::NEG_ONE, F::ZERO),
            (F::NEG_ONE, F::ZERO, F::ZERO),
            (F::INFINITY, F::ZERO, F::INFINITY),
            (F::NEG_INFINITY, F::ZERO, F::ZERO),
            (F::NAN, F::ZERO, F::ZERO),
            (F::ZERO, F::NAN, F::ZERO),
            (F::NAN, F::NAN, F::NAN),
        ];

        for (x, y, res) in cases {
            let val = f(x, y);
            assert_biteq!(val, res, "fmax({}, {})", Hexf(x), Hexf(y));
        }
    }

    #[test]
    #[cfg(f16_enabled)]
    fn fmax_spec_tests_f16() {
        fmax_spec_test::<f16>(fmaxf16);
    }

    #[test]
    fn fmax_spec_tests_f32() {
        fmax_spec_test::<f32>(fmaxf);
    }

    #[test]
    fn fmax_spec_tests_f64() {
        fmax_spec_test::<f64>(fmax);
    }

    #[test]
    #[cfg(f128_enabled)]
    fn fmax_spec_tests_f128() {
        fmax_spec_test::<f128>(fmaxf128);
    }
}
