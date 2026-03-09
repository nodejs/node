use crate::prelude::*;

pub type clock_t = u32;
pub type wchar_t = i32;
pub type time_t = i64;
pub type suseconds_t = i64;
pub type register_t = i64;

s! {
    #[repr(align(16))]
    pub struct mcontext_t {
        pub mc_vers: c_int,
        pub mc_flags: c_int,
        pub mc_onstack: c_int,
        pub mc_len: c_int,
        pub mc_avec: [u64; 64],
        pub mc_av: [u32; 2],
        pub mc_frame: [crate::register_t; 42],
        pub mc_fpreg: [u64; 33],
        pub mc_vsxfpreg: [u64; 32],
    }
}

pub(crate) const _ALIGNBYTES: usize = size_of::<c_long>() - 1;

pub const BIOCSRTIMEOUT: c_ulong = 0x8010426d;
pub const BIOCGRTIMEOUT: c_ulong = 0x4010426e;

pub const MAP_32BIT: c_int = 0x00080000;
pub const MINSIGSTKSZ: size_t = 2048; // 512 * 4
pub const TIOCTIMESTAMP: c_ulong = 0x40107459;
