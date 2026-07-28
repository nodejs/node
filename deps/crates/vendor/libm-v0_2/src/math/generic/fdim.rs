use crate::support::Float;

#[inline]
pub fn fdim<F: Float>(x: F, y: F) -> F {
    if x <= y { F::ZERO } else { x - y }
}
