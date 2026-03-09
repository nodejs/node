//! Header: `utmp.h`
//!
//! <https://github.com/NetBSD/src/blob/master/include/utmp.h>

use crate::prelude::*;

pub const UT_NAMESIZE: usize = 8;
pub const UT_LINESIZE: usize = 8;
pub const UT_HOSTSIZE: usize = 16;

s! {
    pub struct lastlog {
        pub ll_time: crate::time_t,
        pub ll_line: [c_char; UT_LINESIZE],
        pub ll_host: [c_char; UT_HOSTSIZE],
    }

    pub struct utmp {
        pub ut_line: [c_char; UT_LINESIZE],
        pub ut_name: [c_char; UT_NAMESIZE],
        pub ut_host: [c_char; UT_HOSTSIZE],
        pub ut_time: crate::time_t,
    }
}

#[link(name = "util")]
extern "C" {
    pub fn utmpname(file: *const c_char) -> c_int;
    pub fn setutent();
    #[link_name = "__getutent50"]
    pub fn getutent() -> *mut utmp;
    pub fn endutent();
}
