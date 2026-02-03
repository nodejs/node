/// Positive difference (f16)
///
/// Determines the positive difference between arguments, returning:
/// * x - y if x > y, or
/// * +0    if x <= y, or
/// * NAN   if either argument is NAN.
///
/// A range error may occur.
#[cfg(f16_enabled)]
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn fdimf16(x: f16, y: f16) -> f16 {
    super::generic::fdim(x, y)
}

/// Positive difference (f32)
///
/// Determines the positive difference between arguments, returning:
/// * x - y if x > y, or
/// * +0    if x <= y, or
/// * NAN   if either argument is NAN.
///
/// A range error may occur.
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn fdimf(x: f32, y: f32) -> f32 {
    super::generic::fdim(x, y)
}

/// Positive difference (f64)
///
/// Determines the positive difference between arguments, returning:
/// * x - y if x > y, or
/// * +0    if x <= y, or
/// * NAN   if either argument is NAN.
///
/// A range error may occur.
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn fdim(x: f64, y: f64) -> f64 {
    super::generic::fdim(x, y)
}

/// Positive difference (f128)
///
/// Determines the positive difference between arguments, returning:
/// * x - y if x > y, or
/// * +0    if x <= y, or
/// * NAN   if either argument is NAN.
///
/// A range error may occur.
#[cfg(f128_enabled)]
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn fdimf128(x: f128, y: f128) -> f128 {
    super::generic::fdim(x, y)
}
