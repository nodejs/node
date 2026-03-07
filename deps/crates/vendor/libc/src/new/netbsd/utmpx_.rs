//! Header: `utmpx.h`
//!
//! <https://github.com/NetBSD/src/blob/master/include/utmp.h>

use crate::prelude::*;

// pub const _PATH_UTMPX: &[c_char; 14] = b"/var/run/utmpx";
// pub const _PATH_WTMPX: &[c_char; 14] = b"/var/log/wtmpx";
// pub const _PATH_LASTLOGX: &[c_char; 17] = b"/var/log/lastlogx";
// pub const _PATH_UTMP_UPDATE: &[c_char; 24] = b"/usr/libexec/utmp_update";

pub const _UTX_USERSIZE: usize = 32;
pub const _UTX_LINESIZE: usize = 32;
pub const _UTX_IDSIZE: usize = 4;
pub const _UTX_HOSTSIZE: usize = 256;

pub const EMPTY: u16 = 0;
pub const RUN_LVL: u16 = 1;
pub const BOOT_TIME: u16 = 2;
pub const OLD_TIME: u16 = 3;
pub const NEW_TIME: u16 = 4;
pub const INIT_PROCESS: u16 = 5;
pub const LOGIN_PROCESS: u16 = 6;
pub const USER_PROCESS: u16 = 7;
pub const DEAD_PROCESS: u16 = 8;
pub const ACCOUNTING: u16 = 9;
pub const SIGNATURE: u16 = 10;
pub const DOWN_TIME: u16 = 11;

// Expression based on the comment in NetBSD source.
pub const _UTX_PADSIZE: usize = if cfg!(target_pointer_width = "64") {
    36
} else {
    40
};

s! {
    pub struct utmpx {
        pub ut_name: [c_char; _UTX_USERSIZE],
        pub ut_id: [c_char; _UTX_IDSIZE],
        pub ut_line: [c_char; _UTX_LINESIZE],
        pub ut_host: [c_char; _UTX_HOSTSIZE],
        pub ut_session: u16,
        pub ut_type: u16,
        pub ut_pid: crate::pid_t,
        pub ut_exit: __exit_status, // FIXME(netbsd): when anonymous struct are supported
        pub ut_ss: crate::sockaddr_storage,
        pub ut_tv: crate::timeval,
        ut_pad: Padding<[u8; _UTX_PADSIZE]>,
    }

    pub struct __exit_status {
        pub e_termination: u16,
        pub e_exit: u16,
    }

    pub struct lastlogx {
        pub ll_tv: crate::timeval,
        pub ll_line: [c_char; _UTX_LINESIZE],
        pub ll_host: [c_char; _UTX_HOSTSIZE],
        pub ll_ss: crate::sockaddr_storage,
    }
}

#[link(name = "util")]
extern "C" {
    pub fn setutxent();
    pub fn endutxent();

    #[link_name = "__getutxent50"]
    pub fn getutxent() -> *mut utmpx;
    #[link_name = "__getutxid50"]
    pub fn getutxid(ut: *const utmpx) -> *mut utmpx;
    #[link_name = "__getutxline50"]
    pub fn getutxline(ut: *const utmpx) -> *mut utmpx;
    #[link_name = "__pututxline50"]
    pub fn pututxline(ut: *const utmpx) -> *mut utmpx;

    #[link_name = "__updwtmpx50"]
    pub fn updwtmpx(file: *const c_char, ut: *const utmpx) -> c_int;
    #[link_name = "__getlastlogx50"]
    pub fn getlastlogx(fname: *const c_char, uid: crate::uid_t, ll: *mut lastlogx)
        -> *mut lastlogx;

    #[link_name = "__updlastlogx50"]
    pub fn updlastlogx(fname: *const c_char, uid: crate::uid_t, ll: *mut lastlogx) -> c_int;
    #[link_name = "__getutmp50"]
    pub fn getutmp(ux: *const utmpx, u: *mut crate::utmp);
    #[link_name = "__getutmpx50"]
    pub fn getutmpx(u: *const crate::utmp, ux: *mut utmpx);
    pub fn utmpxname(file: *const c_char) -> c_int;
}
