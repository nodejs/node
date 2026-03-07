use crate::prelude::*;

pub type clock_t = i32;
pub type wchar_t = i32;
pub type time_t = i64;
pub type suseconds_t = i64;
pub type register_t = i64;

s! {
    pub struct reg32 {
        pub r_fs: u32,
        pub r_es: u32,
        pub r_ds: u32,
        pub r_edi: u32,
        pub r_esi: u32,
        pub r_ebp: u32,
        pub r_isp: u32,
        pub r_ebx: u32,
        pub r_edx: u32,
        pub r_ecx: u32,
        pub r_eax: u32,
        pub r_trapno: u32,
        pub r_err: u32,
        pub r_eip: u32,
        pub r_cs: u32,
        pub r_eflags: u32,
        pub r_esp: u32,
        pub r_ss: u32,
        pub r_gs: u32,
    }

    pub struct reg {
        pub r_r15: i64,
        pub r_r14: i64,
        pub r_r13: i64,
        pub r_r12: i64,
        pub r_r11: i64,
        pub r_r10: i64,
        pub r_r9: i64,
        pub r_r8: i64,
        pub r_rdi: i64,
        pub r_rsi: i64,
        pub r_rbp: i64,
        pub r_rbx: i64,
        pub r_rdx: i64,
        pub r_rcx: i64,
        pub r_rax: i64,
        pub r_trapno: u32,
        pub r_fs: u16,
        pub r_gs: u16,
        pub r_err: u32,
        pub r_es: u16,
        pub r_ds: u16,
        pub r_rip: i64,
        pub r_cs: i64,
        pub r_rflags: i64,
        pub r_rsp: i64,
        pub r_ss: i64,
    }

    pub struct fpreg32 {
        pub fpr_env: [u32; 7],
        pub fpr_acc: [[u8; 10]; 8],
        pub fpr_ex_sw: u32,
        pub fpr_pad: [u8; 64],
    }

    pub struct fpreg {
        pub fpr_env: [u64; 4],
        pub fpr_acc: [[u8; 16]; 8],
        pub fpr_xacc: [[u8; 16]; 16],
        pub fpr_spare: [u64; 12],
    }

    pub struct xmmreg {
        pub xmm_env: [u32; 8],
        pub xmm_acc: [[u8; 16]; 8],
        pub xmm_reg: [[u8; 16]; 8],
        pub xmm_pad: [u8; 224],
    }
    #[repr(align(16))]
    #[cfg_attr(not(any(freebsd11, freebsd12, freebsd13, freebsd14)), non_exhaustive)]
    pub struct mcontext_t {
        pub mc_onstack: register_t,
        pub mc_rdi: register_t,
        pub mc_rsi: register_t,
        pub mc_rdx: register_t,
        pub mc_rcx: register_t,
        pub mc_r8: register_t,
        pub mc_r9: register_t,
        pub mc_rax: register_t,
        pub mc_rbx: register_t,
        pub mc_rbp: register_t,
        pub mc_r10: register_t,
        pub mc_r11: register_t,
        pub mc_r12: register_t,
        pub mc_r13: register_t,
        pub mc_r14: register_t,
        pub mc_r15: register_t,
        pub mc_trapno: u32,
        pub mc_fs: u16,
        pub mc_gs: u16,
        pub mc_addr: register_t,
        pub mc_flags: u32,
        pub mc_es: u16,
        pub mc_ds: u16,
        pub mc_err: register_t,
        pub mc_rip: register_t,
        pub mc_cs: register_t,
        pub mc_rflags: register_t,
        pub mc_rsp: register_t,
        pub mc_ss: register_t,
        pub mc_len: c_long,
        pub mc_fpformat: c_long,
        pub mc_ownedfp: c_long,
        pub mc_fpstate: [c_long; 64],
        pub mc_fsbase: register_t,
        pub mc_gsbase: register_t,
        pub mc_xfpustate: register_t,
        pub mc_xfpustate_len: register_t,
        // freebsd < 15
        #[cfg(any(freebsd11, freebsd12, freebsd13, freebsd14))]
        pub mc_spare: [c_long; 4],
        // freebsd >= 15
        #[cfg(not(any(freebsd11, freebsd12, freebsd13, freebsd14)))]
        pub mc_tlsbase: register_t,
        #[cfg(not(any(freebsd11, freebsd12, freebsd13, freebsd14)))]
        pub mc_spare: [c_long; 3],
    }
}

s_no_extra_traits! {
    pub union __c_anonymous_elf64_auxv_union {
        pub a_val: c_long,
        pub a_ptr: *mut c_void,
        pub a_fcn: extern "C" fn(),
    }

    pub struct Elf64_Auxinfo {
        pub a_type: c_long,
        pub a_un: __c_anonymous_elf64_auxv_union,
    }

    #[repr(align(16))]
    pub struct max_align_t {
        priv_: [f64; 4],
    }
}

cfg_if! {
    if #[cfg(feature = "extra_traits")] {
        // FIXME(msrv): suggested method was added in 1.85
        #[allow(unpredictable_function_pointer_comparisons)]
        impl PartialEq for __c_anonymous_elf64_auxv_union {
            fn eq(&self, other: &__c_anonymous_elf64_auxv_union) -> bool {
                unsafe {
                    self.a_val == other.a_val
                        || self.a_ptr == other.a_ptr
                        || self.a_fcn == other.a_fcn
                }
            }
        }
        impl Eq for __c_anonymous_elf64_auxv_union {}
    }
}

pub(crate) const _ALIGNBYTES: usize = size_of::<c_long>() - 1;

pub const BIOCSRTIMEOUT: c_ulong = 0x8010426d;
pub const BIOCGRTIMEOUT: c_ulong = 0x4010426e;

pub const MAP_32BIT: c_int = 0x00080000;
pub const MINSIGSTKSZ: size_t = 2048; // 512 * 4

pub const _MC_HASSEGS: u32 = 0x1;
pub const _MC_HASBASES: u32 = 0x2;
pub const _MC_HASFPXSTATE: u32 = 0x4;

pub const _MC_FPFMT_NODEV: c_long = 0x10000;
pub const _MC_FPFMT_XMM: c_long = 0x10002;
pub const _MC_FPOWNED_NONE: c_long = 0x20000;
pub const _MC_FPOWNED_FPU: c_long = 0x20001;
pub const _MC_FPOWNED_PCB: c_long = 0x20002;

pub const KINFO_FILE_SIZE: c_int = 1392;

pub const TIOCTIMESTAMP: c_ulong = 0x40107459;
