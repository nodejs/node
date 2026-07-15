/// Absolute value (magnitude) (f16)
///
/// Calculates the absolute value (magnitude) of the argument `x`,
/// by direct manipulation of the bit representation of `x`.
#[cfg(f16_enabled)]
#[cfg_attr(assert_no_panic, no_panic::no_panic)]
pub fn fabsf16(x: f16) -> f16 {
    super::generic::fabs(x)
}

/// Absolute value (magnitude) (f32)
///
/// Calculates the absolute value (magnitude) of the argument `x`,
/// by direct manipulation of the bit representation of `x`.
#[cfg_attr(assert_no_panic, no_panic::no_panic)]
pub fn fabsf(x: f32) -> f32 {
    select_implementation! {
        name: fabsf,
        use_arch: all(target_arch = "wasm32", intrinsics_enabled),
        args: x,
    }

    super::generic::fabs(x)
}

/// Absolute value (magnitude) (f64)
///
/// Calculates the absolute value (magnitude) of the argument `x`,
/// by direct manipulation of the bit representation of `x`.
#[cfg_attr(assert_no_panic, no_panic::no_panic)]
pub fn fabs(x: f64) -> f64 {
    select_implementation! {
        name: fabs,
        use_arch: all(target_arch = "wasm32", intrinsics_enabled),
        args: x,
    }

    super::generic::fabs(x)
}

/// Absolute value (magnitude) (f128)
///
/// Calculates the absolute value (magnitude) of the argument `x`,
/// by direct manipulation of the bit representation of `x`.
#[cfg(f128_enabled)]
#[cfg_attr(assert_no_panic, no_panic::no_panic)]
pub fn fabsf128(x: f128) -> f128 {
    super::generic::fabs(x)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::support::Float;

    /// Based on https://en.cppreference.com/w/cpp/numeric/math/fabs
    fn spec_test<F: Float>(f: impl Fn(F) -> F) {
        assert_biteq!(f(F::ZERO), F::ZERO);
        assert_biteq!(f(F::NEG_ZERO), F::ZERO);
        assert_biteq!(f(F::INFINITY), F::INFINITY);
        assert_biteq!(f(F::NEG_INFINITY), F::INFINITY);
        assert!(f(F::NAN).is_nan());

        // Not spec rewquired but we expect it
        assert!(f(F::NAN).is_sign_positive());
        assert!(f(F::from_bits(F::NAN.to_bits() | F::SIGN_MASK)).is_sign_positive());
    }

    #[test]
    #[cfg(f16_enabled)]
    fn sanity_check_f16() {
        assert_eq!(fabsf16(-1.0f16), 1.0);
        assert_eq!(fabsf16(2.8f16), 2.8);
    }

    #[test]
    #[cfg(f16_enabled)]
    fn spec_tests_f16() {
        spec_test::<f16>(fabsf16);
    }

    #[test]
    fn sanity_check_f32() {
        assert_eq!(fabsf(-1.0f32), 1.0);
        assert_eq!(fabsf(2.8f32), 2.8);
    }

    #[test]
    fn spec_tests_f32() {
        spec_test::<f32>(fabsf);
    }

    #[test]
    fn sanity_check_f64() {
        assert_eq!(fabs(-1.0f64), 1.0);
        assert_eq!(fabs(2.8f64), 2.8);
    }

    #[test]
    fn spec_tests_f64() {
        spec_test::<f64>(fabs);
    }

    #[test]
    #[cfg(f128_enabled)]
    fn sanity_check_f128() {
        assert_eq!(fabsf128(-1.0f128), 1.0);
        assert_eq!(fabsf128(2.8f128), 2.8);
    }

    #[test]
    #[cfg(f128_enabled)]
    fn spec_tests_f128() {
        spec_test::<f128>(fabsf128);
    }
}
