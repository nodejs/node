//! Header: `sys/timex.h`
//!
//! <https://github.com/NetBSD/src/tree/trunk/sys/sys/timex.h>

use crate::prelude::*;

pub const MAXPHASE: c_long = 500000000;
pub const MAXFREQ: c_long = 500000;
pub const MINSEC: c_int = 256;
pub const MAXSEC: c_int = 2048;
pub const NANOSECOND: c_long = 1000000000;
pub const SCALE_PPM: c_int = 65;
pub const MAXTC: c_int = 10;

pub const MOD_OFFSET: c_uint = 0x0001;
pub const MOD_FREQUENCY: c_uint = 0x0002;
pub const MOD_MAXERROR: c_uint = 0x0004;
pub const MOD_ESTERROR: c_uint = 0x0008;
pub const MOD_STATUS: c_uint = 0x0010;
pub const MOD_TIMECONST: c_uint = 0x0020;
pub const MOD_PPSMAX: c_uint = 0x0040;
pub const MOD_TAI: c_uint = 0x0080;
pub const MOD_MICRO: c_uint = 0x1000;
pub const MOD_NANO: c_uint = 0x2000;
pub const MOD_CLKB: c_uint = 0x4000;
pub const MOD_CLKA: c_uint = 0x8000;

pub const STA_PLL: c_int = 0x0001;
pub const STA_PPSFREQ: c_int = 0x0002;
pub const STA_PPSTIME: c_int = 0x0004;
pub const STA_FLL: c_int = 0x0008;
pub const STA_INS: c_int = 0x0010;
pub const STA_DEL: c_int = 0x0020;
pub const STA_UNSYNC: c_int = 0x0040;
pub const STA_FREQHOLD: c_int = 0x0080;
pub const STA_PPSSIGNAL: c_int = 0x0100;
pub const STA_PPSJITTER: c_int = 0x0200;
pub const STA_PPSWANDER: c_int = 0x0400;
pub const STA_PPSERROR: c_int = 0x0800;
pub const STA_CLOCKERR: c_int = 0x1000;
pub const STA_NANO: c_int = 0x2000;
pub const STA_MODE: c_int = 0x4000;
pub const STA_CLK: c_int = 0x8000;

pub const STA_RONLY: c_int = STA_PPSSIGNAL
    | STA_PPSJITTER
    | STA_PPSWANDER
    | STA_PPSERROR
    | STA_CLOCKERR
    | STA_NANO
    | STA_MODE
    | STA_CLK;

pub const TIME_OK: c_int = 0;
pub const TIME_INS: c_int = 1;
pub const TIME_DEL: c_int = 2;
pub const TIME_OOP: c_int = 3;
pub const TIME_WAIT: c_int = 4;
pub const TIME_ERROR: c_int = 5;

s! {
    pub struct ntptimeval {
        pub time: crate::timespec,
        pub maxerror: c_long,
        pub esterror: c_long,
        pub tai: c_long,
        pub time_state: c_int,
    }

    pub struct timex {
        pub modes: c_uint,
        pub offset: c_long,
        pub freq: c_long,
        pub maxerror: c_long,
        pub esterror: c_long,
        pub status: c_int,
        pub constant: c_long,
        pub precision: c_long,
        pub tolerance: c_long,
        pub ppsfreq: c_long,
        pub jitter: c_long,
        pub shift: c_int,
        pub stabil: c_long,
        pub jitcnt: c_long,
        pub calcnt: c_long,
        pub errcnt: c_long,
        pub stbcnt: c_long,
    }
}

extern "C" {
    #[link_name = "__ntp_gettime50"]
    pub fn ntp_gettime(buf: *mut ntptimeval) -> c_int;
    pub fn ntp_adjtime(buf: *mut timex) -> c_int;
}
