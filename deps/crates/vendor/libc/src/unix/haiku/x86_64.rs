use crate::prelude::*;

s! {
    pub struct fpu_state {
        pub control: c_ushort,
        pub status: c_ushort,
        pub tag: c_ushort,
        pub opcode: c_ushort,
        pub rip: c_ulong,
        pub rdp: c_ulong,
        pub mxcsr: c_uint,
        pub mscsr_mask: c_uint,
        pub _fpreg: [[c_uchar; 8]; 16],
        pub _xmm: [[c_uchar; 16]; 16],
        _reserved_416_511: Padding<[c_uchar; 96]>,
    }

    pub struct xstate_hdr {
        pub bv: c_ulong,
        pub xcomp_bv: c_ulong,
        _reserved: Padding<[c_uchar; 48]>,
    }

    pub struct savefpu {
        pub fp_fxsave: fpu_state,
        pub fp_xstate: xstate_hdr,
        pub _fp_ymm: [[c_uchar; 16]; 16],
    }

    pub struct mcontext_t {
        pub rax: c_ulong,
        pub rbx: c_ulong,
        pub rcx: c_ulong,
        pub rdx: c_ulong,
        pub rdi: c_ulong,
        pub rsi: c_ulong,
        pub rbp: c_ulong,
        pub r8: c_ulong,
        pub r9: c_ulong,
        pub r10: c_ulong,
        pub r11: c_ulong,
        pub r12: c_ulong,
        pub r13: c_ulong,
        pub r14: c_ulong,
        pub r15: c_ulong,
        pub rsp: c_ulong,
        pub rip: c_ulong,
        pub rflags: c_ulong,
        pub fpu: savefpu,
    }

    pub struct ucontext_t {
        pub uc_link: *mut ucontext_t,
        pub uc_sigmask: crate::sigset_t,
        pub uc_stack: crate::stack_t,
        pub uc_mcontext: mcontext_t,
    }
}
