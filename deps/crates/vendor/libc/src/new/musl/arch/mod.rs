//! Source directory: `arch/`
//!
//! <https://github.com/kraj/musl/tree/master/arch>

pub(crate) mod generic;

#[cfg(target_arch = "mips")]
pub(crate) mod mips;
#[cfg(target_arch = "mips64")]
pub(crate) mod mips64;
