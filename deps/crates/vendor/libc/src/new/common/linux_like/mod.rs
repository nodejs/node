//! API that primarily comes from Linux but is also used other platforms (e.g. Android).

#[cfg(any(
    target_os = "android",
    target_os = "emscripten",
    target_os = "l4re",
    target_os = "linux"
))]
pub(crate) mod pthread;
