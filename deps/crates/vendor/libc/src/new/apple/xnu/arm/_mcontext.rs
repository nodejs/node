//! Header: `arm/_mcontext.h`
//!
//! <https://github.com/apple-oss-distributions/xnu/blob/main/bsd/arm/_mcontext.h>

pub use crate::mach::machine::_structs::*;

s! {
    pub struct __darwin_mcontext64 {
        pub __es: __darwin_arm_exception_state64,
        pub __ss: __darwin_arm_thread_state64,
        pub __ns: __darwin_arm_neon_state64,
    }
}

pub type mcontext_t = *mut __darwin_mcontext64;
