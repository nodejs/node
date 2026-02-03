use crate::support::Float;

/// Copy the sign of `y` to `x`.
#[inline]
pub fn copysign<F: Float>(x: F, y: F) -> F {
    let mut ux = x.to_bits();
    let uy = y.to_bits();
    ux &= !F::SIGN_MASK;
    ux |= uy & F::SIGN_MASK;
    F::from_bits(ux)
}
