//! POSIX APIs that are used by a number of platforms
//!
//! These can be found at: <https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/contents.html>.

// FIXME(pthread): eventually all platforms should use this module
#[cfg(any(
    target_os = "android",
    target_os = "emscripten",
    target_os = "l4re",
    target_os = "linux",
    target_os = "qurt",
    target_vendor = "apple",
))]
pub(crate) mod pthread;
pub(crate) mod unistd;
