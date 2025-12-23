/// Absolute value (magnitude) (f32)
///
/// Calculates the absolute value (magnitude) of the argument `x`,
/// by direct manipulation of the bit representation of `x`.
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn fabsf(x: f32) -> f32 {
    select_implementation! {
        name: fabsf,
        use_arch: all(target_arch = "wasm32", intrinsics_enabled),
        args: x,
    }

    super::generic::fabs(x)
}

// PowerPC tests are failing on LLVM 13: https://github.com/rust-lang/rust/issues/88520
#[cfg(not(target_arch = "powerpc64"))]
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn sanity_check() {
        assert_eq!(fabsf(-1.0), 1.0);
        assert_eq!(fabsf(2.8), 2.8);
    }

    /// The spec: https://en.cppreference.com/w/cpp/numeric/math/fabs
    #[test]
    fn spec_tests() {
        assert!(fabsf(f32::NAN).is_nan());
        for f in [0.0, -0.0].iter().copied() {
            assert_eq!(fabsf(f), 0.0);
        }
        for f in [f32::INFINITY, f32::NEG_INFINITY].iter().copied() {
            assert_eq!(fabsf(f), f32::INFINITY);
        }
    }
}
