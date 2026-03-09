//! Interfaces that are common across multiple platforms
//!
//! We make these available everywhere but each platform must opt in to reexporting.
//!
//! There shouldn't be any repeated definitions or complex configuration in this module. On
//! platforms that don't use common APIs it is fine to use `#[cfg(not(...))]`, but if a platform
//! needs a custom definition then it should be defined in the platform-specific module.
//!
//! The goal is that platforms need to opt in to the definitions here, so that worst case we have
//! an unused warning on untested platforms (rather than exposing incorrect API).

#[cfg(any(
    target_vendor = "apple",
    target_os = "dragonfly",
    target_os = "freebsd",
    target_os = "netbsd",
    target_os = "openbsd",
))]
pub(crate) mod bsd;

#[cfg(any(
    target_os = "android",
    target_os = "emscripten",
    target_os = "l4re",
    target_os = "linux",
))]
pub(crate) mod linux_like;

#[cfg(any(target_os = "dragonfly", target_os = "freebsd"))]
pub(crate) mod freebsd_like;

#[cfg(any(target_os = "netbsd", target_os = "openbsd"))]
pub(crate) mod netbsd_like;

#[cfg(any(target_os = "illumos", target_os = "solaris"))]
pub(crate) mod solarish;

#[cfg(target_family = "unix")]
pub(crate) mod posix;
