//! Header: `machine/_mcontext.h`

cfg_if! {
    if #[cfg(any(target_arch = "x86", target_arch = "x86_64"))] {
        pub use crate::i386::_mcontext::*;
    } else if #[cfg(any(target_arch = "arm", target_arch = "aarch64"))] {
        pub use crate::arm::_mcontext::*;
    } else {
        // Unsupported arch
    }
}
