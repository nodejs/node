/// Calculate the remainder of `x / y`, the precise result of `x - trunc(x / y) * y`.
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn fmodf128(x: f128, y: f128) -> f128 {
    super::generic::fmod(x, y)
}
