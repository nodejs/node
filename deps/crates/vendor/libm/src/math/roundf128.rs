/// Round `x` to the nearest integer, breaking ties away from zero.
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn roundf128(x: f128) -> f128 {
    super::generic::round(x)
}
