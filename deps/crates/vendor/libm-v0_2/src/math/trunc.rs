/// Rounds the number toward 0 to the closest integral value (f16).
///
/// This effectively removes the decimal part of the number, leaving the integral part.
#[cfg(f16_enabled)]
#[cfg_attr(assert_no_panic, no_panic::no_panic)]
pub fn truncf16(x: f16) -> f16 {
    super::generic::trunc(x)
}

/// Rounds the number toward 0 to the closest integral value (f32).
///
/// This effectively removes the decimal part of the number, leaving the integral part.
#[cfg_attr(assert_no_panic, no_panic::no_panic)]
pub fn truncf(x: f32) -> f32 {
    select_implementation! {
        name: truncf,
        use_arch: all(target_arch = "wasm32", intrinsics_enabled),
        args: x,
    }

    super::generic::trunc(x)
}

/// Rounds the number toward 0 to the closest integral value (f64).
///
/// This effectively removes the decimal part of the number, leaving the integral part.
#[cfg_attr(assert_no_panic, no_panic::no_panic)]
pub fn trunc(x: f64) -> f64 {
    select_implementation! {
        name: trunc,
        use_arch: all(target_arch = "wasm32", intrinsics_enabled),
        args: x,
    }

    super::generic::trunc(x)
}

/// Rounds the number toward 0 to the closest integral value (f128).
///
/// This effectively removes the decimal part of the number, leaving the integral part.
#[cfg(f128_enabled)]
#[cfg_attr(assert_no_panic, no_panic::no_panic)]
pub fn truncf128(x: f128) -> f128 {
    super::generic::trunc(x)
}

#[cfg(test)]
mod tests {
    #[test]
    fn sanity_check() {
        assert_eq!(super::truncf(1.1), 1.0);
    }
}
