//! Header: `i386/_structs.h`
//!
//! <https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/mach/i386/_structs.h>

use crate::prelude::*;

s! {
    pub struct __darwin_mmst_reg {
        pub __mmst_reg: [c_char; 10],
        pub __mmst_rsrv: [c_char; 6],
    }

    pub struct __darwin_xmm_reg {
        pub __xmm_reg: [c_char; 16],
    }

    pub struct __darwin_x86_thread_state64 {
        pub __rax: u64,
        pub __rbx: u64,
        pub __rcx: u64,
        pub __rdx: u64,
        pub __rdi: u64,
        pub __rsi: u64,
        pub __rbp: u64,
        pub __rsp: u64,
        pub __r8: u64,
        pub __r9: u64,
        pub __r10: u64,
        pub __r11: u64,
        pub __r12: u64,
        pub __r13: u64,
        pub __r14: u64,
        pub __r15: u64,
        pub __rip: u64,
        pub __rflags: u64,
        pub __cs: u64,
        pub __fs: u64,
        pub __gs: u64,
    }

    pub struct __darwin_x86_exception_state64 {
        pub __trapno: u16,
        pub __cpu: u16,
        pub __err: u32,
        pub __faultvaddr: u64,
    }

    pub struct __darwin_x86_float_state64 {
        pub __fpu_reserved: [c_int; 2],
        __fpu_fcw: c_short,
        __fpu_fsw: c_short,
        pub __fpu_ftw: u8,
        pub __fpu_rsrv1: u8,
        pub __fpu_fop: u16,
        pub __fpu_ip: u32,
        pub __fpu_cs: u16,
        pub __fpu_rsrv2: u16,
        pub __fpu_dp: u32,
        pub __fpu_ds: u16,
        pub __fpu_rsrv3: u16,
        pub __fpu_mxcsr: u32,
        pub __fpu_mxcsrmask: u32,
        pub __fpu_stmm0: __darwin_mmst_reg,
        pub __fpu_stmm1: __darwin_mmst_reg,
        pub __fpu_stmm2: __darwin_mmst_reg,
        pub __fpu_stmm3: __darwin_mmst_reg,
        pub __fpu_stmm4: __darwin_mmst_reg,
        pub __fpu_stmm5: __darwin_mmst_reg,
        pub __fpu_stmm6: __darwin_mmst_reg,
        pub __fpu_stmm7: __darwin_mmst_reg,
        pub __fpu_xmm0: __darwin_xmm_reg,
        pub __fpu_xmm1: __darwin_xmm_reg,
        pub __fpu_xmm2: __darwin_xmm_reg,
        pub __fpu_xmm3: __darwin_xmm_reg,
        pub __fpu_xmm4: __darwin_xmm_reg,
        pub __fpu_xmm5: __darwin_xmm_reg,
        pub __fpu_xmm6: __darwin_xmm_reg,
        pub __fpu_xmm7: __darwin_xmm_reg,
        pub __fpu_xmm8: __darwin_xmm_reg,
        pub __fpu_xmm9: __darwin_xmm_reg,
        pub __fpu_xmm10: __darwin_xmm_reg,
        pub __fpu_xmm11: __darwin_xmm_reg,
        pub __fpu_xmm12: __darwin_xmm_reg,
        pub __fpu_xmm13: __darwin_xmm_reg,
        pub __fpu_xmm14: __darwin_xmm_reg,
        pub __fpu_xmm15: __darwin_xmm_reg,
        // FIXME(apple): this field is actually [u8; 96], but defining it with a bigger type allows
        // us to auto-implement traits for it since the length of the array is less than 32
        __fpu_rsrv4: [u32; 24],
        pub __fpu_reserved1: c_int,
    }
}
