/// Return the lesser of two arguments or, if either argument is NaN, NaN.
///
/// This coincides with IEEE 754-2019 `minimumNumber`. The result orders -0.0 < 0.0.
#[cfg(f16_enabled)]
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn fminimum_numf16(x: f16, y: f16) -> f16 {
    super::generic::fminimum_num(x, y)
}

/// Return the lesser of two arguments or, if either argument is NaN, NaN.
///
/// This coincides with IEEE 754-2019 `minimumNumber`. The result orders -0.0 < 0.0.
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn fminimum_numf(x: f32, y: f32) -> f32 {
    super::generic::fminimum_num(x, y)
}

/// Return the lesser of two arguments or, if either argument is NaN, NaN.
///
/// This coincides with IEEE 754-2019 `minimumNumber`. The result orders -0.0 < 0.0.
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn fminimum_num(x: f64, y: f64) -> f64 {
    super::generic::fminimum_num(x, y)
}

/// Return the lesser of two arguments or, if either argument is NaN, NaN.
///
/// This coincides with IEEE 754-2019 `minimumNumber`. The result orders -0.0 < 0.0.
#[cfg(f128_enabled)]
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn fminimum_numf128(x: f128, y: f128) -> f128 {
    super::generic::fminimum_num(x, y)
}

/// Return the greater of two arguments or, if either argument is NaN, NaN.
///
/// This coincides with IEEE 754-2019 `maximumNumber`. The result orders -0.0 < 0.0.
#[cfg(f16_enabled)]
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn fmaximum_numf16(x: f16, y: f16) -> f16 {
    super::generic::fmaximum_num(x, y)
}

/// Return the greater of two arguments or, if either argument is NaN, NaN.
///
/// This coincides with IEEE 754-2019 `maximumNumber`. The result orders -0.0 < 0.0.
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn fmaximum_numf(x: f32, y: f32) -> f32 {
    super::generic::fmaximum_num(x, y)
}

/// Return the greater of two arguments or, if either argument is NaN, NaN.
///
/// This coincides with IEEE 754-2019 `maximumNumber`. The result orders -0.0 < 0.0.
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn fmaximum_num(x: f64, y: f64) -> f64 {
    super::generic::fmaximum_num(x, y)
}

/// Return the greater of two arguments or, if either argument is NaN, NaN.
///
/// This coincides with IEEE 754-2019 `maximumNumber`. The result orders -0.0 < 0.0.
#[cfg(f128_enabled)]
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn fmaximum_numf128(x: f128, y: f128) -> f128 {
    super::generic::fmaximum_num(x, y)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::support::{Float, Hexf};

    fn fminimum_num_spec_test<F: Float>(f: impl Fn(F, F) -> F) {
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
            (F::ZERO, F::NEG_ZERO, F::NEG_ZERO),
            (F::NEG_ZERO, F::ZERO, F::NEG_ZERO),
        ];

        for (x, y, res) in cases {
            let val = f(x, y);
            assert_biteq!(val, res, "fminimum_num({}, {})", Hexf(x), Hexf(y));
        }
    }

    #[test]
    #[cfg(f16_enabled)]
    fn fminimum_num_spec_tests_f16() {
        fminimum_num_spec_test::<f16>(fminimum_numf16);
    }

    #[test]
    fn fminimum_num_spec_tests_f32() {
        fminimum_num_spec_test::<f32>(fminimum_numf);
    }

    #[test]
    fn fminimum_num_spec_tests_f64() {
        fminimum_num_spec_test::<f64>(fminimum_num);
    }

    #[test]
    #[cfg(f128_enabled)]
    fn fminimum_num_spec_tests_f128() {
        fminimum_num_spec_test::<f128>(fminimum_numf128);
    }

    fn fmaximum_num_spec_test<F: Float>(f: impl Fn(F, F) -> F) {
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
            (F::ZERO, F::NEG_ZERO, F::ZERO),
            (F::NEG_ZERO, F::ZERO, F::ZERO),
        ];

        for (x, y, res) in cases {
            let val = f(x, y);
            assert_biteq!(val, res, "fmaximum_num({}, {})", Hexf(x), Hexf(y));
        }
    }

    #[test]
    #[cfg(f16_enabled)]
    fn fmaximum_num_spec_tests_f16() {
        fmaximum_num_spec_test::<f16>(fmaximum_numf16);
    }

    #[test]
    fn fmaximum_num_spec_tests_f32() {
        fmaximum_num_spec_test::<f32>(fmaximum_numf);
    }

    #[test]
    fn fmaximum_num_spec_tests_f64() {
        fmaximum_num_spec_test::<f64>(fmaximum_num);
    }

    #[test]
    #[cfg(f128_enabled)]
    fn fmaximum_num_spec_tests_f128() {
        fmaximum_num_spec_test::<f128>(fmaximum_numf128);
    }
}
