use super::lgammaf_r;

/// The natural logarithm of the
/// [Gamma function](https://en.wikipedia.org/wiki/Gamma_function) (f32).
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn lgammaf(x: f32) -> f32 {
    lgammaf_r(x).0
}
