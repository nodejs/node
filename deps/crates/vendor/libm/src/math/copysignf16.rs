/// Sign of Y, magnitude of X (f16)
///
/// Constructs a number with the magnitude (absolute value) of its
/// first argument, `x`, and the sign of its second argument, `y`.
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn copysignf16(x: f16, y: f16) -> f16 {
    super::generic::copysign(x, y)
}
