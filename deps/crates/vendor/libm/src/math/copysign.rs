/// Sign of Y, magnitude of X (f16)
///
/// Constructs a number with the magnitude (absolute value) of its
/// first argument, `x`, and the sign of its second argument, `y`.
#[cfg(f16_enabled)]
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn copysignf16(x: f16, y: f16) -> f16 {
    super::generic::copysign(x, y)
}

/// Sign of Y, magnitude of X (f32)
///
/// Constructs a number with the magnitude (absolute value) of its
/// first argument, `x`, and the sign of its second argument, `y`.
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn copysignf(x: f32, y: f32) -> f32 {
    super::generic::copysign(x, y)
}

/// Sign of Y, magnitude of X (f64)
///
/// Constructs a number with the magnitude (absolute value) of its
/// first argument, `x`, and the sign of its second argument, `y`.
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn copysign(x: f64, y: f64) -> f64 {
    super::generic::copysign(x, y)
}

/// Sign of Y, magnitude of X (f128)
///
/// Constructs a number with the magnitude (absolute value) of its
/// first argument, `x`, and the sign of its second argument, `y`.
#[cfg(f128_enabled)]
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn copysignf128(x: f128, y: f128) -> f128 {
    super::generic::copysign(x, y)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::support::Float;

    fn spec_test<F: Float>(f: impl Fn(F, F) -> F) {
        assert_biteq!(f(F::ZERO, F::ZERO), F::ZERO);
        assert_biteq!(f(F::NEG_ZERO, F::ZERO), F::ZERO);
        assert_biteq!(f(F::ZERO, F::NEG_ZERO), F::NEG_ZERO);
        assert_biteq!(f(F::NEG_ZERO, F::NEG_ZERO), F::NEG_ZERO);

        assert_biteq!(f(F::ONE, F::ONE), F::ONE);
        assert_biteq!(f(F::NEG_ONE, F::ONE), F::ONE);
        assert_biteq!(f(F::ONE, F::NEG_ONE), F::NEG_ONE);
        assert_biteq!(f(F::NEG_ONE, F::NEG_ONE), F::NEG_ONE);

        assert_biteq!(f(F::INFINITY, F::INFINITY), F::INFINITY);
        assert_biteq!(f(F::NEG_INFINITY, F::INFINITY), F::INFINITY);
        assert_biteq!(f(F::INFINITY, F::NEG_INFINITY), F::NEG_INFINITY);
        assert_biteq!(f(F::NEG_INFINITY, F::NEG_INFINITY), F::NEG_INFINITY);

        // Not required but we expect it
        assert_biteq!(f(F::NAN, F::NAN), F::NAN);
        assert_biteq!(f(F::NEG_NAN, F::NAN), F::NAN);
        assert_biteq!(f(F::NAN, F::NEG_NAN), F::NEG_NAN);
        assert_biteq!(f(F::NEG_NAN, F::NEG_NAN), F::NEG_NAN);
    }

    #[test]
    #[cfg(f16_enabled)]
    fn spec_tests_f16() {
        spec_test::<f16>(copysignf16);
    }

    #[test]
    fn spec_tests_f32() {
        spec_test::<f32>(copysignf);
    }

    #[test]
    fn spec_tests_f64() {
        spec_test::<f64>(copysign);
    }

    #[test]
    #[cfg(f128_enabled)]
    fn spec_tests_f128() {
        spec_test::<f128>(copysignf128);
    }
}
