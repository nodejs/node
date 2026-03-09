//! Header: `sys/mman.h`
//!
//! Memory mapping functions and constants from Hexagon toolchain.

use super::super::*;
use crate::prelude::*;

// Memory protection constants
pub const PROT_NONE: c_int = 0x00;
pub const PROT_READ: c_int = 0x01;
pub const PROT_WRITE: c_int = 0x02;
pub const PROT_EXEC: c_int = 0x04;

// Memory mapping constants
pub const MAP_SHARED: c_int = 0x0001;
pub const MAP_PRIVATE: c_int = 0x0002;
pub const MAP_FIXED: c_int = 0x0010;
pub const MAP_ANON: c_int = 0x1000;
pub const MAP_ANONYMOUS: c_int = MAP_ANON;
pub const MAP_FILE: c_int = 0x0000;
pub const MAP_RENAME: c_int = 0x0020;
pub const MAP_NORESERVE: c_int = 0x0040;
pub const MAP_INHERIT: c_int = 0x0080;
pub const MAP_HASSEMAPHORE: c_int = 0x0200;
pub const MAP_TRYFIXED: c_int = 0x0400;
pub const MAP_WIRED: c_int = 0x0800;

pub const MAP_FAILED: *mut c_void = !0 as *mut c_void;

// Memory sync constants
pub const MS_ASYNC: c_int = 0x01;
pub const MS_INVALIDATE: c_int = 0x02;
pub const MS_SYNC: c_int = 0x04;

// Memory lock constants
pub const MCL_CURRENT: c_int = 0x01;
pub const MCL_FUTURE: c_int = 0x02;

extern "C" {
    pub fn mmap(
        addr: *mut c_void,
        len: size_t,
        prot: c_int,
        flags: c_int,
        fd: c_int,
        offset: off_t,
    ) -> *mut c_void;
    pub fn munmap(addr: *mut c_void, len: size_t) -> c_int;
    pub fn mprotect(addr: *mut c_void, len: size_t, prot: c_int) -> c_int;
    pub fn mlock(addr: *const c_void, len: size_t) -> c_int;
    pub fn munlock(addr: *const c_void, len: size_t) -> c_int;
    pub fn mlockall(flags: c_int) -> c_int;
    pub fn munlockall() -> c_int;
    pub fn msync(addr: *mut c_void, len: size_t, flags: c_int) -> c_int;
}
