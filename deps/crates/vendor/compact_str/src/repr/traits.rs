use super::Repr;

const FALSE: Repr = Repr::new_inline("false");
const TRUE: Repr = Repr::new_inline("true");

/// Defines how to _efficiently_ create a [`Repr`] from `self`
pub trait IntoRepr {
    fn into_repr(self) -> Repr;
}

impl IntoRepr for f32 {
    fn into_repr(self) -> Repr {
        let mut buf = ryu::Buffer::new();
        let s = buf.format(self);
        Repr::new(s)
    }
}

impl IntoRepr for f64 {
    fn into_repr(self) -> Repr {
        let mut buf = ryu::Buffer::new();
        let s = buf.format(self);
        Repr::new(s)
    }
}

impl IntoRepr for bool {
    fn into_repr(self) -> Repr {
        if self {
            TRUE
        } else {
            FALSE
        }
    }
}

impl IntoRepr for char {
    fn into_repr(self) -> Repr {
        let mut buf = [0_u8; 4];
        Repr::new_inline(self.encode_utf8(&mut buf))
    }
}

#[cfg(test)]
mod tests {
    use quickcheck_macros::quickcheck;

    use super::IntoRepr;

    #[test]
    fn test_into_repr_bool() {
        let t = true;
        let repr = t.into_repr();
        assert_eq!(repr.as_str(), t.to_string());

        let f = false;
        let repr = f.into_repr();
        assert_eq!(repr.as_str(), f.to_string());
    }

    #[quickcheck]
    #[cfg_attr(miri, ignore)]
    fn quickcheck_into_repr_char(val: char) {
        let repr = char::into_repr(val);
        assert_eq!(repr.as_str(), val.to_string());
    }

    #[test]
    fn test_into_repr_f64_sanity() {
        let vals = [
            f64::MIN,
            f64::MIN_POSITIVE,
            f64::MAX,
            f64::NEG_INFINITY,
            f64::INFINITY,
        ];

        for x in &vals {
            let repr = f64::into_repr(*x);
            let roundtrip = repr.as_str().parse::<f64>().unwrap();

            assert_eq!(*x, roundtrip);
        }
    }

    #[test]
    fn test_into_repr_f64_nan() {
        let repr = f64::into_repr(f64::NAN);
        let roundtrip = repr.as_str().parse::<f64>().unwrap();
        assert!(roundtrip.is_nan());
    }

    #[quickcheck]
    #[cfg_attr(miri, ignore)]
    fn quickcheck_into_repr_f64(val: f64) {
        let repr = f64::into_repr(val);
        let roundtrip = repr.as_str().parse::<f64>().unwrap();

        // Note: The formatting of floats by `ryu` sometimes differs from that of `std`, so instead
        // of asserting equality with `std` we just make sure the value roundtrips

        if val.is_nan() != roundtrip.is_nan() {
            assert_eq!(val, roundtrip);
        }
    }

    // `f32` formatting is broken on powerpc64le, not only in `ryu` but also `std`
    //
    // See: https://github.com/rust-lang/rust/issues/96306
    #[test]
    #[cfg_attr(all(target_arch = "powerpc64", target_pointer_width = "64"), ignore)]
    fn test_into_repr_f32_sanity() {
        let vals = [
            f32::MIN,
            f32::MIN_POSITIVE,
            f32::MAX,
            f32::NEG_INFINITY,
            f32::INFINITY,
        ];

        for x in &vals {
            let repr = f32::into_repr(*x);
            let roundtrip = repr.as_str().parse::<f32>().unwrap();

            assert_eq!(*x, roundtrip);
        }
    }

    #[test]
    #[cfg_attr(all(target_arch = "powerpc64", target_pointer_width = "64"), ignore)]
    fn test_into_repr_f32_nan() {
        let repr = f32::into_repr(f32::NAN);
        let roundtrip = repr.as_str().parse::<f32>().unwrap();
        assert!(roundtrip.is_nan());
    }

    #[quickcheck]
    #[cfg_attr(all(target_arch = "powerpc64", target_pointer_width = "64"), ignore)]
    fn proptest_into_repr_f32(val: f32) {
        let repr = f32::into_repr(val);
        let roundtrip = repr.as_str().parse::<f32>().unwrap();

        // Note: The formatting of floats by `ryu` sometimes differs from that of `std`, so instead
        // of asserting equality with `std` we just make sure the value roundtrips

        if val.is_nan() != roundtrip.is_nan() {
            assert_eq!(val, roundtrip);
        }
    }
}
