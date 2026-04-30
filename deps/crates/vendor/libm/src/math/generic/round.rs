use super::{copysign, trunc};
use crate::support::{Float, MinInt};

#[inline]
pub fn round<F: Float>(x: F) -> F {
    let f0p5 = F::from_parts(false, F::EXP_BIAS - 1, F::Int::ZERO); // 0.5
    let f0p25 = F::from_parts(false, F::EXP_BIAS - 2, F::Int::ZERO); // 0.25

    trunc(x + copysign(f0p5 - f0p25 * F::EPSILON, x))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    #[cfg(f16_enabled)]
    fn zeroes_f16() {
        assert_biteq!(round(0.0_f16), 0.0_f16);
        assert_biteq!(round(-0.0_f16), -0.0_f16);
    }

    #[test]
    #[cfg(f16_enabled)]
    fn sanity_check_f16() {
        assert_eq!(round(-1.0_f16), -1.0);
        assert_eq!(round(2.8_f16), 3.0);
        assert_eq!(round(-0.5_f16), -1.0);
        assert_eq!(round(0.5_f16), 1.0);
        assert_eq!(round(-1.5_f16), -2.0);
        assert_eq!(round(1.5_f16), 2.0);
    }

    #[test]
    fn zeroes_f32() {
        assert_biteq!(round(0.0_f32), 0.0_f32);
        assert_biteq!(round(-0.0_f32), -0.0_f32);
    }

    #[test]
    fn sanity_check_f32() {
        assert_eq!(round(-1.0_f32), -1.0);
        assert_eq!(round(2.8_f32), 3.0);
        assert_eq!(round(-0.5_f32), -1.0);
        assert_eq!(round(0.5_f32), 1.0);
        assert_eq!(round(-1.5_f32), -2.0);
        assert_eq!(round(1.5_f32), 2.0);
    }

    #[test]
    fn zeroes_f64() {
        assert_biteq!(round(0.0_f64), 0.0_f64);
        assert_biteq!(round(-0.0_f64), -0.0_f64);
    }

    #[test]
    fn sanity_check_f64() {
        assert_eq!(round(-1.0_f64), -1.0);
        assert_eq!(round(2.8_f64), 3.0);
        assert_eq!(round(-0.5_f64), -1.0);
        assert_eq!(round(0.5_f64), 1.0);
        assert_eq!(round(-1.5_f64), -2.0);
        assert_eq!(round(1.5_f64), 2.0);
    }

    #[test]
    #[cfg(f128_enabled)]
    fn zeroes_f128() {
        assert_biteq!(round(0.0_f128), 0.0_f128);
        assert_biteq!(round(-0.0_f128), -0.0_f128);
    }

    #[test]
    #[cfg(f128_enabled)]
    fn sanity_check_f128() {
        assert_eq!(round(-1.0_f128), -1.0);
        assert_eq!(round(2.8_f128), 3.0);
        assert_eq!(round(-0.5_f128), -1.0);
        assert_eq!(round(0.5_f128), 1.0);
        assert_eq!(round(-1.5_f128), -2.0);
        assert_eq!(round(1.5_f128), 2.0);
    }
}
