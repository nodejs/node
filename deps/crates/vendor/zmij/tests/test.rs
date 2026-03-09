#![allow(clippy::float_cmp, clippy::unreadable_literal)]

fn dtoa(value: f64) -> String {
    zmij::Buffer::new().format(value).to_owned()
}

fn ftoa(value: f32) -> String {
    zmij::Buffer::new().format(value).to_owned()
}

mod dtoa_test {
    use super::dtoa;

    #[test]
    fn normal() {
        assert_eq!(dtoa(6.62607015e-34), "6.62607015e-34");

        // Exact half-ulp tie when rounding to nearest integer.
        assert_eq!(dtoa(5.444310685350916e+14), "544431068535091.6");
    }

    #[test]
    fn subnormal() {
        assert_eq!(dtoa(0.0f64.next_up()), "5e-324");
        assert_eq!(dtoa(1e-323), "1e-323");
        assert_eq!(dtoa(1.2e-322), "1.2e-322");
        assert_eq!(dtoa(1.24e-322), "1.24e-322");
        assert_eq!(dtoa(1.234e-320), "1.234e-320");
    }

    #[test]
    fn all_irregular() {
        for exp in 1..0x3ff {
            let bits = exp << 52;
            let value = f64::from_bits(bits);

            assert_eq!(dtoa(value), ryu::Buffer::new().format(value));
        }
    }

    #[test]
    fn all_exponents() {
        for exp in 0..=0x3ff {
            let bits = (exp << 52) | 1;
            let value = f64::from_bits(bits);

            assert_eq!(dtoa(value), ryu::Buffer::new().format(value));
        }
    }

    #[test]
    fn small_int() {
        assert_eq!(dtoa(1.0), "1.0");
    }

    #[test]
    fn zero() {
        assert_eq!(dtoa(0.0), "0.0");
        assert_eq!(dtoa(-0.0), "-0.0");
    }

    #[test]
    fn inf() {
        assert_eq!(dtoa(f64::INFINITY), "inf");
    }

    #[test]
    fn nan() {
        assert_eq!(dtoa(f64::NAN.copysign(-1.0)), "NaN");
    }

    #[test]
    fn shorter() {
        // A possibly shorter underestimate is picked (u' in Schubfach).
        assert_eq!(dtoa(-4.932096661796888e-226), "-4.932096661796888e-226");

        // A possibly shorter overestimate is picked (w' in Schubfach).
        assert_eq!(dtoa(3.439070283483335e+35), "3.439070283483335e+35");
    }

    #[test]
    fn single_candidate() {
        // Only an underestimate is in the rounding region (u in Schubfach).
        assert_eq!(dtoa(6.606854224493745e-17), "6.606854224493745e-17");

        // Only an overestimate is in the rounding region (w in Schubfach).
        assert_eq!(dtoa(6.079537928711555e+61), "6.079537928711555e+61");
    }

    #[test]
    fn null_terminated() {
        assert_eq!(dtoa(9.061488e15), "9061488000000000.0");
        assert_eq!(dtoa(f64::NAN.copysign(1.0)), "NaN");
    }

    #[test]
    fn no_buffer() {
        assert_eq!(dtoa(6.62607015e-34), "6.62607015e-34");
    }
}

mod ftoa_test {
    use super::ftoa;

    #[test]
    fn normal() {
        assert_eq!(ftoa(6.62607e-34), "6.62607e-34");
        assert_eq!(ftoa(1.342178e+08), "134217800.0");
        assert_eq!(ftoa(1.3421781e+08), "134217810.0");
    }

    #[test]
    fn subnormal() {
        assert_eq!(ftoa(0.0f32.next_up()), "1e-45");
    }

    #[test]
    fn no_buffer() {
        assert_eq!(ftoa(6.62607e-34), "6.62607e-34");
    }
}
