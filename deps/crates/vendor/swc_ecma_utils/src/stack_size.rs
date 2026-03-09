/// Calls `callback` with a larger stack size.
#[inline(always)]
#[cfg(any(
    target_arch = "wasm32",
    target_arch = "arm",
    not(feature = "stacker"),
    // miri does not work with stacker
    miri
))]
pub fn maybe_grow<R, F: FnOnce() -> R>(_red_zone: usize, _stack_size: usize, callback: F) -> R {
    callback()
}

/// Calls `callback` with a larger stack size.
#[inline(always)]
#[cfg(all(
    not(any(target_arch = "wasm32", target_arch = "arm", miri)),
    feature = "stacker"
))]
pub fn maybe_grow<R, F: FnOnce() -> R>(red_zone: usize, stack_size: usize, callback: F) -> R {
    stacker::maybe_grow(red_zone, stack_size, callback)
}

/// Calls `callback` with a larger stack size.
///
/// `maybe_grow` with default values.
pub fn maybe_grow_default<R, F: FnOnce() -> R>(callback: F) -> R {
    let (red_zone, stack_size) = (4 * 1024, 16 * 1024);

    maybe_grow(red_zone, stack_size, callback)
}
