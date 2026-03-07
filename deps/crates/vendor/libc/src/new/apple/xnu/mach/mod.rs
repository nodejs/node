//! Directory: `mach/`
//!
//! <https://github.com/apple-oss-distributions/xnu/tree/main/osfmk/mach>

/// Directory: `mach/arm`
#[cfg(any(target_arch = "arm", target_arch = "aarch64"))]
pub(crate) mod arm {
    pub(crate) mod _structs;
}

/// Directory: `mach/i386`
#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
pub(crate) mod i386 {
    pub(crate) mod _structs;
}

/// Directory: `mach/machine`
pub(crate) mod machine {
    pub(crate) mod _structs;
}
