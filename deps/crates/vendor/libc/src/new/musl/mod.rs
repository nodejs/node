//! Musl libc.
//!
//! * Headers: <https://git.musl-libc.org/cgit/musl> (official)
//! * Headers: <https://github.com/kraj/musl> (mirror)

// The musl build system includes `arch/$(ARCH)` (preferred if it exists) and `arch/generic` (used
// as the fallback). We can't exactly mirror this with glob exports, so instead we selectively
// reexport in a new module.
mod arch;

pub(crate) mod bits {
    cfg_if! {
        if #[cfg(target_arch = "mips")] {
            pub(crate) use super::arch::mips::bits::socket;
        } else if #[cfg(target_arch = "mips64")] {
            pub(crate) use super::arch::mips64::bits::socket;
        } else {
            // Reexports from generic will live here once we need them.
        }
    }
}

pub(crate) mod pthread;

/// Directory: `sys/`
///
/// <https://github.com/kraj/musl/tree/kraj/master/include/sys>
pub(crate) mod sys {
    pub(crate) mod socket;
}

pub(crate) mod sched;
pub(crate) mod unistd;
