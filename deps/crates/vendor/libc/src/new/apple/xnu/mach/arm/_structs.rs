//! Header: `arm/_structs.h`
//!
//! <https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/mach/arm/_structs.h>

#[cfg(target_arch = "arm")]
use crate::prelude::*;

s! {
    pub struct __darwin_arm_exception_state64 {
        pub __far: u64,
        pub __esr: u32,
        pub __exception: u32,
    }

    pub struct __darwin_arm_thread_state64 {
        pub __x: [u64; 29],
        pub __fp: u64,
        pub __lr: u64,
        pub __sp: u64,
        pub __pc: u64,
        pub __cpsr: u32,
        pub __pad: u32,
    }

    #[cfg(target_arch = "aarch64")]
    pub struct __darwin_arm_neon_state64 {
        pub __v: [crate::__uint128_t; 32],
        pub __fpsr: u32,
        pub __fpcr: u32,
    }

    #[cfg(target_arch = "arm")]
    #[repr(align(16))]
    pub struct __darwin_arm_neon_state64 {
        opaque: [c_char; (32 * 16) + (2 * size_of::<u32>())],
    }
}
