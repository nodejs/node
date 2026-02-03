use super::support::Round;

/// Round `x` to the nearest integer, breaking ties toward even.
#[cfg(f16_enabled)]
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn rintf16(x: f16) -> f16 {
    select_implementation! {
        name: rintf16,
        use_arch: all(target_arch = "aarch64", target_feature = "fp16"),
        args: x,
    }

    super::generic::rint_round(x, Round::Nearest).val
}

/// Round `x` to the nearest integer, breaking ties toward even.
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn rintf(x: f32) -> f32 {
    select_implementation! {
        name: rintf,
        use_arch: any(
            all(target_arch = "aarch64", target_feature = "neon"),
            all(target_arch = "wasm32", intrinsics_enabled),
        ),
        args: x,
    }

    super::generic::rint_round(x, Round::Nearest).val
}

/// Round `x` to the nearest integer, breaking ties toward even.
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn rint(x: f64) -> f64 {
    select_implementation! {
        name: rint,
        use_arch: any(
            all(target_arch = "aarch64", target_feature = "neon"),
            all(target_arch = "wasm32", intrinsics_enabled),
        ),
        args: x,
    }

    super::generic::rint_round(x, Round::Nearest).val
}

/// Round `x` to the nearest integer, breaking ties toward even.
#[cfg(f128_enabled)]
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn rintf128(x: f128) -> f128 {
    super::generic::rint_round(x, Round::Nearest).val
}
