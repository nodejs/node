/// Absolute value (magnitude) (f16)
///
/// Calculates the absolute value (magnitude) of the argument `x`,
/// by direct manipulation of the bit representation of `x`.
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn fabsf16(x: f16) -> f16 {
    super::generic::fabs(x)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn sanity_check() {
        assert_eq!(fabsf16(-1.0), 1.0);
        assert_eq!(fabsf16(2.8), 2.8);
    }

    /// The spec: https://en.cppreference.com/w/cpp/numeric/math/fabs
    #[test]
    fn spec_tests() {
        assert!(fabsf16(f16::NAN).is_nan());
        for f in [0.0, -0.0].iter().copied() {
            assert_eq!(fabsf16(f), 0.0);
        }
        for f in [f16::INFINITY, f16::NEG_INFINITY].iter().copied() {
            assert_eq!(fabsf16(f), f16::INFINITY);
        }
    }
}
