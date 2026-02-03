/// Floor (f128)
///
/// Finds the nearest integer less than or equal to `x`.
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn floorf128(x: f128) -> f128 {
    return super::generic::floor(x);
}
