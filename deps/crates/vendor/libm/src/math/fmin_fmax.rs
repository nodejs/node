/// Return the lesser of two arguments or, if either argument is NaN, the other argument.
///
/// This coincides with IEEE 754-2011 `minNum`. The result disregards signed zero (meaning if
/// the inputs are -0.0 and +0.0, either may be returned).
#[cfg(f16_enabled)]
#[cfg_attr(assert_no_panic, no_panic::no_panic)]
pub fn fminf16(x: f16, y: f16) -> f16 {
    super::generic::fmin(x, y)
}

/// Return the lesser of two arguments or, if either argument is NaN, the other argument.
///
/// This coincides with IEEE 754-2011 `minNum`. The result disregards signed zero (meaning if
/// the inputs are -0.0 and +0.0, either may be returned).
#[cfg_attr(assert_no_panic, no_panic::no_panic)]
pub fn fminf(x: f32, y: f32) -> f32 {
    super::generic::fmin(x, y)
}

/// Return the lesser of two arguments or, if either argument is NaN, the other argument.
///
/// This coincides with IEEE 754-2011 `minNum`. The result disregards signed zero (meaning if
/// the inputs are -0.0 and +0.0, either may be returned).
#[cfg_attr(assert_no_panic, no_panic::no_panic)]
pub fn fmin(x: f64, y: f64) -> f64 {
    super::generic::fmin(x, y)
}

/// Return the lesser of two arguments or, if either argument is NaN, the other argument.
///
/// This coincides with IEEE 754-2011 `minNum`. The result disregards signed zero (meaning if
/// the inputs are -0.0 and +0.0, either may be returned).
#[cfg(f128_enabled)]
#[cfg_attr(assert_no_panic, no_panic::no_panic)]
pub fn fminf128(x: f128, y: f128) -> f128 {
    super::generic::fmin(x, y)
}

/// Return the greater of two arguments or, if either argument is NaN, the other argument.
///
/// This coincides with IEEE 754-2011 `maxNum`. The result disregards signed zero (meaning if
/// the inputs are -0.0 and +0.0, either may be returned).
#[cfg(f16_enabled)]
#[cfg_attr(assert_no_panic, no_panic::no_panic)]
pub fn fmaxf16(x: f16, y: f16) -> f16 {
    super::generic::fmax(x, y)
}

/// Return the greater of two arguments or, if either argument is NaN, the other argument.
///
/// This coincides with IEEE 754-2011 `maxNum`. The result disregards signed zero (meaning if
/// the inputs are -0.0 and +0.0, either may be returned).
#[cfg_attr(assert_no_panic, no_panic::no_panic)]
pub fn fmaxf(x: f32, y: f32) -> f32 {
    super::generic::fmax(x, y)
}

/// Return the greater of two arguments or, if either argument is NaN, the other argument.
///
/// This coincides with IEEE 754-2011 `maxNum`. The result disregards signed zero (meaning if
/// the inputs are -0.0 and +0.0, either may be returned).
#[cfg_attr(assert_no_panic, no_panic::no_panic)]
pub fn fmax(x: f64, y: f64) -> f64 {
    super::generic::fmax(x, y)
}

/// Return the greater of two arguments or, if either argument is NaN, the other argument.
///
/// This coincides with IEEE 754-2011 `maxNum`. The result disregards signed zero (meaning if
/// the inputs are -0.0 and +0.0, either may be returned).
#[cfg(f128_enabled)]
#[cfg_attr(assert_no_panic, no_panic::no_panic)]
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
            (F::ZERO, F::ONE, F::ZERO),
            (F::ZERO, F::NEG_ONE, F::NEG_ONE),
            (F::ZERO, F::INFINITY, F::ZERO),
            (F::ZERO, F::NEG_INFINITY, F::NEG_INFINITY),
            (F::ZERO, F::NAN, F::ZERO),
            (F::ZERO, F::NEG_NAN, F::ZERO),
            (F::NEG_ZERO, F::NEG_ZERO, F::NEG_ZERO),
            (F::NEG_ZERO, F::ONE, F::NEG_ZERO),
            (F::NEG_ZERO, F::NEG_ONE, F::NEG_ONE),
            (F::NEG_ZERO, F::INFINITY, F::NEG_ZERO),
            (F::NEG_ZERO, F::NEG_INFINITY, F::NEG_INFINITY),
            (F::NEG_ZERO, F::NAN, F::NEG_ZERO),
            (F::NEG_ZERO, F::NEG_NAN, F::NEG_ZERO),
            (F::ONE, F::ZERO, F::ZERO),
            (F::ONE, F::NEG_ZERO, F::NEG_ZERO),
            (F::ONE, F::ONE, F::ONE),
            (F::ONE, F::NEG_ONE, F::NEG_ONE),
            (F::ONE, F::INFINITY, F::ONE),
            (F::ONE, F::NEG_INFINITY, F::NEG_INFINITY),
            (F::ONE, F::NAN, F::ONE),
            (F::ONE, F::NEG_NAN, F::ONE),
            (F::NEG_ONE, F::ZERO, F::NEG_ONE),
            (F::NEG_ONE, F::NEG_ZERO, F::NEG_ONE),
            (F::NEG_ONE, F::ONE, F::NEG_ONE),
            (F::NEG_ONE, F::NEG_ONE, F::NEG_ONE),
            (F::NEG_ONE, F::INFINITY, F::NEG_ONE),
            (F::NEG_ONE, F::NEG_INFINITY, F::NEG_INFINITY),
            (F::NEG_ONE, F::NAN, F::NEG_ONE),
            (F::NEG_ONE, F::NEG_NAN, F::NEG_ONE),
            (F::INFINITY, F::ZERO, F::ZERO),
            (F::INFINITY, F::NEG_ZERO, F::NEG_ZERO),
            (F::INFINITY, F::ONE, F::ONE),
            (F::INFINITY, F::NEG_ONE, F::NEG_ONE),
            (F::INFINITY, F::INFINITY, F::INFINITY),
            (F::INFINITY, F::NEG_INFINITY, F::NEG_INFINITY),
            (F::INFINITY, F::NAN, F::INFINITY),
            (F::INFINITY, F::NEG_NAN, F::INFINITY),
            (F::NEG_INFINITY, F::ZERO, F::NEG_INFINITY),
            (F::NEG_INFINITY, F::NEG_ZERO, F::NEG_INFINITY),
            (F::NEG_INFINITY, F::ONE, F::NEG_INFINITY),
            (F::NEG_INFINITY, F::NEG_ONE, F::NEG_INFINITY),
            (F::NEG_INFINITY, F::INFINITY, F::NEG_INFINITY),
            (F::NEG_INFINITY, F::NEG_INFINITY, F::NEG_INFINITY),
            (F::NEG_INFINITY, F::NAN, F::NEG_INFINITY),
            (F::NEG_INFINITY, F::NEG_NAN, F::NEG_INFINITY),
            (F::NAN, F::ZERO, F::ZERO),
            (F::NAN, F::NEG_ZERO, F::NEG_ZERO),
            (F::NAN, F::ONE, F::ONE),
            (F::NAN, F::NEG_ONE, F::NEG_ONE),
            (F::NAN, F::INFINITY, F::INFINITY),
            (F::NAN, F::NEG_INFINITY, F::NEG_INFINITY),
            (F::NAN, F::NAN, F::NAN),
            (F::NEG_NAN, F::ZERO, F::ZERO),
            (F::NEG_NAN, F::NEG_ZERO, F::NEG_ZERO),
            (F::NEG_NAN, F::ONE, F::ONE),
            (F::NEG_NAN, F::NEG_ONE, F::NEG_ONE),
            (F::NEG_NAN, F::INFINITY, F::INFINITY),
            (F::NEG_NAN, F::NEG_INFINITY, F::NEG_INFINITY),
        ];

        for (x, y, res) in cases {
            let val = f(x, y);
            assert_biteq!(val, res, "fmin({}, {})", Hexf(x), Hexf(y));
        }

        // Ordering between zeros and NaNs does not matter
        assert_eq!(f(F::ZERO, F::NEG_ZERO), F::ZERO);
        assert_eq!(f(F::NEG_ZERO, F::ZERO), F::ZERO);
        assert!(f(F::NAN, F::NEG_NAN).is_nan());
        assert!(f(F::NEG_NAN, F::NAN).is_nan());
        assert!(f(F::NEG_NAN, F::NEG_NAN).is_nan());
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
            (F::ZERO, F::ONE, F::ONE),
            (F::ZERO, F::NEG_ONE, F::ZERO),
            (F::ZERO, F::INFINITY, F::INFINITY),
            (F::ZERO, F::NEG_INFINITY, F::ZERO),
            (F::ZERO, F::NAN, F::ZERO),
            (F::ZERO, F::NEG_NAN, F::ZERO),
            (F::NEG_ZERO, F::NEG_ZERO, F::NEG_ZERO),
            (F::NEG_ZERO, F::ONE, F::ONE),
            (F::NEG_ZERO, F::NEG_ONE, F::NEG_ZERO),
            (F::NEG_ZERO, F::INFINITY, F::INFINITY),
            (F::NEG_ZERO, F::NEG_INFINITY, F::NEG_ZERO),
            (F::NEG_ZERO, F::NAN, F::NEG_ZERO),
            (F::NEG_ZERO, F::NEG_NAN, F::NEG_ZERO),
            (F::ONE, F::ZERO, F::ONE),
            (F::ONE, F::NEG_ZERO, F::ONE),
            (F::ONE, F::ONE, F::ONE),
            (F::ONE, F::NEG_ONE, F::ONE),
            (F::ONE, F::INFINITY, F::INFINITY),
            (F::ONE, F::NEG_INFINITY, F::ONE),
            (F::ONE, F::NAN, F::ONE),
            (F::ONE, F::NEG_NAN, F::ONE),
            (F::NEG_ONE, F::ZERO, F::ZERO),
            (F::NEG_ONE, F::NEG_ZERO, F::NEG_ZERO),
            (F::NEG_ONE, F::ONE, F::ONE),
            (F::NEG_ONE, F::NEG_ONE, F::NEG_ONE),
            (F::NEG_ONE, F::INFINITY, F::INFINITY),
            (F::NEG_ONE, F::NEG_INFINITY, F::NEG_ONE),
            (F::NEG_ONE, F::NAN, F::NEG_ONE),
            (F::NEG_ONE, F::NEG_NAN, F::NEG_ONE),
            (F::INFINITY, F::ZERO, F::INFINITY),
            (F::INFINITY, F::NEG_ZERO, F::INFINITY),
            (F::INFINITY, F::ONE, F::INFINITY),
            (F::INFINITY, F::NEG_ONE, F::INFINITY),
            (F::INFINITY, F::INFINITY, F::INFINITY),
            (F::INFINITY, F::NEG_INFINITY, F::INFINITY),
            (F::INFINITY, F::NAN, F::INFINITY),
            (F::INFINITY, F::NEG_NAN, F::INFINITY),
            (F::NEG_INFINITY, F::ZERO, F::ZERO),
            (F::NEG_INFINITY, F::NEG_ZERO, F::NEG_ZERO),
            (F::NEG_INFINITY, F::ONE, F::ONE),
            (F::NEG_INFINITY, F::NEG_ONE, F::NEG_ONE),
            (F::NEG_INFINITY, F::INFINITY, F::INFINITY),
            (F::NEG_INFINITY, F::NEG_INFINITY, F::NEG_INFINITY),
            (F::NEG_INFINITY, F::NAN, F::NEG_INFINITY),
            (F::NEG_INFINITY, F::NEG_NAN, F::NEG_INFINITY),
            (F::NAN, F::ZERO, F::ZERO),
            (F::NAN, F::NEG_ZERO, F::NEG_ZERO),
            (F::NAN, F::ONE, F::ONE),
            (F::NAN, F::NEG_ONE, F::NEG_ONE),
            (F::NAN, F::INFINITY, F::INFINITY),
            (F::NAN, F::NEG_INFINITY, F::NEG_INFINITY),
            (F::NAN, F::NAN, F::NAN),
            (F::NEG_NAN, F::ZERO, F::ZERO),
            (F::NEG_NAN, F::NEG_ZERO, F::NEG_ZERO),
            (F::NEG_NAN, F::ONE, F::ONE),
            (F::NEG_NAN, F::NEG_ONE, F::NEG_ONE),
            (F::NEG_NAN, F::INFINITY, F::INFINITY),
            (F::NEG_NAN, F::NEG_INFINITY, F::NEG_INFINITY),
        ];

        for (x, y, res) in cases {
            let val = f(x, y);
            assert_biteq!(val, res, "fmax({}, {})", Hexf(x), Hexf(y));
        }

        // Ordering between zeros and NaNs does not matter
        assert_eq!(f(F::ZERO, F::NEG_ZERO), F::ZERO);
        assert_eq!(f(F::NEG_ZERO, F::ZERO), F::ZERO);
        assert!(f(F::NAN, F::NEG_NAN).is_nan());
        assert!(f(F::NEG_NAN, F::NAN).is_nan());
        assert!(f(F::NEG_NAN, F::NEG_NAN).is_nan());
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
