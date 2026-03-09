use crate::prelude::*;

pub type clock_t = c_ulong;
pub type wchar_t = i32;
pub type time_t = i32;
pub type suseconds_t = i32;
pub type register_t = i32;

s! {
    #[repr(align(16))]
    pub struct mcontext_t {
        pub mc_onstack: register_t,
        pub mc_gs: register_t,
        pub mc_fs: register_t,
        pub mc_es: register_t,
        pub mc_ds: register_t,
        pub mc_edi: register_t,
        pub mc_esi: register_t,
        pub mc_ebp: register_t,
        pub mc_isp: register_t,
        pub mc_ebx: register_t,
        pub mc_edx: register_t,
        pub mc_ecx: register_t,
        pub mc_eax: register_t,
        pub mc_trapno: register_t,
        pub mc_err: register_t,
        pub mc_eip: register_t,
        pub mc_cs: register_t,
        pub mc_eflags: register_t,
        pub mc_esp: register_t,
        pub mc_ss: register_t,
        pub mc_len: c_int,
        pub mc_fpformat: c_int,
        pub mc_ownedfp: c_int,
        pub mc_flags: register_t,
        pub mc_fpstate: [c_int; 128],
        pub mc_fsbase: register_t,
        pub mc_gsbase: register_t,
        pub mc_xfpustate: register_t,
        pub mc_xfpustate_len: register_t,
        pub mc_spare2: [c_int; 4],
    }
}

pub(crate) const _ALIGNBYTES: usize = size_of::<c_long>() - 1;

pub const MINSIGSTKSZ: size_t = 2048; // 512 * 4

pub const BIOCSRTIMEOUT: c_ulong = 0x8008426d;
pub const BIOCGRTIMEOUT: c_ulong = 0x4008426e;
pub const KINFO_FILE_SIZE: c_int = 1392;
pub const TIOCTIMESTAMP: c_ulong = 0x40087459;
