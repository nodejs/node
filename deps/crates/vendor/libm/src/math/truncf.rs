/// Rounds the number toward 0 to the closest integral value (f32).
///
/// This effectively removes the decimal part of the number, leaving the integral part.
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn truncf(x: f32) -> f32 {
    select_implementation! {
        name: truncf,
        use_arch: all(target_arch = "wasm32", intrinsics_enabled),
        args: x,
    }

    super::generic::trunc(x)
}

// PowerPC tests are failing on LLVM 13: https://github.com/rust-lang/rust/issues/88520
#[cfg(not(target_arch = "powerpc64"))]
#[cfg(test)]
mod tests {
    #[test]
    fn sanity_check() {
        assert_eq!(super::truncf(1.1), 1.0);
    }
}
