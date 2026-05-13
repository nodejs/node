/// Calculate the remainder of `x / y`, the precise result of `x - trunc(x / y) * y`.
#[cfg(f16_enabled)]
#[cfg_attr(assert_no_panic, no_panic::no_panic)]
pub fn fmodf16(x: f16, y: f16) -> f16 {
    super::generic::fmod(x, y)
}

/// Calculate the remainder of `x / y`, the precise result of `x - trunc(x / y) * y`.
#[cfg_attr(assert_no_panic, no_panic::no_panic)]
pub fn fmodf(x: f32, y: f32) -> f32 {
    super::generic::fmod(x, y)
}

/// Calculate the remainder of `x / y`, the precise result of `x - trunc(x / y) * y`.
#[cfg_attr(assert_no_panic, no_panic::no_panic)]
pub fn fmod(x: f64, y: f64) -> f64 {
    super::generic::fmod(x, y)
}

/// Calculate the remainder of `x / y`, the precise result of `x - trunc(x / y) * y`.
#[cfg(f128_enabled)]
#[cfg_attr(assert_no_panic, no_panic::no_panic)]
pub fn fmodf128(x: f128, y: f128) -> f128 {
    super::generic::fmod(x, y)
}
