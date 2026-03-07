//! Header: `i386/_mcontext.h`
//!
//! <https://github.com/apple-oss-distributions/xnu/blob/main/bsd/i386/_mcontext.h>

pub use crate::mach::machine::_structs::*;

s! {
    pub struct __darwin_mcontext64 {
        pub __es: __darwin_x86_exception_state64,
        pub __ss: __darwin_x86_thread_state64,
        pub __fs: __darwin_x86_float_state64,
    }
}

pub type mcontext_t = *mut __darwin_mcontext64;
