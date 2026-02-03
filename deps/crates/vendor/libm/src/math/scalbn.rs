#[cfg(f16_enabled)]
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn scalbnf16(x: f16, n: i32) -> f16 {
    super::generic::scalbn(x, n)
}

#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn scalbnf(x: f32, n: i32) -> f32 {
    super::generic::scalbn(x, n)
}

#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn scalbn(x: f64, n: i32) -> f64 {
    super::generic::scalbn(x, n)
}

#[cfg(f128_enabled)]
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn scalbnf128(x: f128, n: i32) -> f128 {
    super::generic::scalbn(x, n)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::support::{CastFrom, CastInto, Float};

    // Tests against N3220
    fn spec_test<F: Float>(f: impl Fn(F, i32) -> F)
    where
        u32: CastInto<F::Int>,
        F::Int: CastFrom<i32>,
        F::Int: CastFrom<u32>,
    {
        // `scalbn(±0, n)` returns `±0`.
        assert_biteq!(f(F::NEG_ZERO, 10), F::NEG_ZERO);
        assert_biteq!(f(F::NEG_ZERO, 0), F::NEG_ZERO);
        assert_biteq!(f(F::NEG_ZERO, -10), F::NEG_ZERO);
        assert_biteq!(f(F::ZERO, 10), F::ZERO);
        assert_biteq!(f(F::ZERO, 0), F::ZERO);
        assert_biteq!(f(F::ZERO, -10), F::ZERO);

        // `scalbn(x, 0)` returns `x`.
        assert_biteq!(f(F::MIN, 0), F::MIN);
        assert_biteq!(f(F::MAX, 0), F::MAX);
        assert_biteq!(f(F::INFINITY, 0), F::INFINITY);
        assert_biteq!(f(F::NEG_INFINITY, 0), F::NEG_INFINITY);
        assert_biteq!(f(F::ZERO, 0), F::ZERO);
        assert_biteq!(f(F::NEG_ZERO, 0), F::NEG_ZERO);

        // `scalbn(±∞, n)` returns `±∞`.
        assert_biteq!(f(F::INFINITY, 10), F::INFINITY);
        assert_biteq!(f(F::INFINITY, -10), F::INFINITY);
        assert_biteq!(f(F::NEG_INFINITY, 10), F::NEG_INFINITY);
        assert_biteq!(f(F::NEG_INFINITY, -10), F::NEG_INFINITY);

        // NaN should remain NaNs.
        assert!(f(F::NAN, 10).is_nan());
        assert!(f(F::NAN, 0).is_nan());
        assert!(f(F::NAN, -10).is_nan());
        assert!(f(-F::NAN, 10).is_nan());
        assert!(f(-F::NAN, 0).is_nan());
        assert!(f(-F::NAN, -10).is_nan());
    }

    #[test]
    #[cfg(f16_enabled)]
    fn spec_test_f16() {
        spec_test::<f16>(scalbnf16);
    }

    #[test]
    fn spec_test_f32() {
        spec_test::<f32>(scalbnf);
    }

    #[test]
    fn spec_test_f64() {
        spec_test::<f64>(scalbn);
    }

    #[test]
    #[cfg(f128_enabled)]
    fn spec_test_f128() {
        spec_test::<f128>(scalbnf128);
    }
}
