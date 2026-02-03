/// Floor (f32)
///
/// Finds the nearest integer less than or equal to `x`.
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn floorf(x: f32) -> f32 {
    select_implementation! {
        name: floorf,
        use_arch: all(target_arch = "wasm32", intrinsics_enabled),
        args: x,
    }

    return super::generic::floor(x);
}
