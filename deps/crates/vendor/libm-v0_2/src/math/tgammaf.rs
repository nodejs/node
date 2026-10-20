use super::tgamma;

/// The [Gamma function](https://en.wikipedia.org/wiki/Gamma_function) (f32).
#[cfg_attr(assert_no_panic, no_panic::no_panic)]
pub fn tgammaf(x: f32) -> f32 {
    tgamma(x as f64) as f32
}
