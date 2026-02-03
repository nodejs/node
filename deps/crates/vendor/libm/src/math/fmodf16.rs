/// Calculate the remainder of `x / y`, the precise result of `x - trunc(x / y) * y`.
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn fmodf16(x: f16, y: f16) -> f16 {
    super::generic::fmod(x, y)
}
