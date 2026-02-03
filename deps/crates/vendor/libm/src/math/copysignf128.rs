/// Sign of Y, magnitude of X (f128)
///
/// Constructs a number with the magnitude (absolute value) of its
/// first argument, `x`, and the sign of its second argument, `y`.
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn copysignf128(x: f128, y: f128) -> f128 {
    super::generic::copysign(x, y)
}
