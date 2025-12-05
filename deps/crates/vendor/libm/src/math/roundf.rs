/// Round `x` to the nearest integer, breaking ties away from zero.
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn roundf(x: f32) -> f32 {
    super::generic::round(x)
}
