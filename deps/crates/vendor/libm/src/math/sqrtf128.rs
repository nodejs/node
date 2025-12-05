/// The square root of `x` (f128).
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn sqrtf128(x: f128) -> f128 {
    return super::generic::sqrt(x);
}
