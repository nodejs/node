//! Header: `mach/machine/_structs.h`
//!
//! <https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/mach/machine/_structs.h>

cfg_if! {
    if #[cfg(any(target_arch = "x86", target_arch = "x86_64"))] {
        pub use crate::mach::i386::_structs::*;
    } else if #[cfg(any(target_arch = "arm", target_arch = "aarch64"))] {
        pub use crate::mach::arm::_structs::*;
    } else {
        // Unsupported arch
    }
}
