/// Rounds the number toward 0 to the closest integral value (f128).
///
/// This effectively removes the decimal part of the number, leaving the integral part.
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn truncf128(x: f128) -> f128 {
    super::generic::trunc(x)
}
