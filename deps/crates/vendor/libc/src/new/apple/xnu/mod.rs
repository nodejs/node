//! Source from XNU <https://github.com/apple-oss-distributions/xnu/tree/main>
//!
//! We omit nesting for the `bsd` module since most items of interest are in there.

/// Directory: `arm/`
///
/// https://github.com/apple-oss-distributions/xnu/tree/main/bsd/arm
#[cfg(any(target_arch = "arm", target_arch = "aarch64"))]
pub(crate) mod arm {
    pub(crate) mod _mcontext;
}

/// Directory: `i386/`
///
/// https://github.com/apple-oss-distributions/xnu/tree/main/bsd/i386
#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
pub(crate) mod i386 {
    pub(crate) mod _mcontext;
}

pub(crate) mod mach;

/// Directory: `machine/`
///
/// https://github.com/apple-oss-distributions/xnu/tree/main/bsd/machine
pub(crate) mod machine {
    pub(crate) mod _mcontext;
}

pub(crate) mod sys;
