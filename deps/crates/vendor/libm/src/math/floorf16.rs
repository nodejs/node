/// Floor (f16)
///
/// Finds the nearest integer less than or equal to `x`.
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn floorf16(x: f16) -> f16 {
    return super::generic::floor(x);
}
