//! Header: `fcntl.h`

use super::*;
use crate::prelude::*;

// File access modes
pub const O_RDONLY: c_int = 0x0000;
pub const O_WRONLY: c_int = 0x0001;
pub const O_RDWR: c_int = 0x0002;
pub const O_ACCMODE: c_int = 0x0003;

// File creation flags
pub const O_CREAT: c_int = 0x0040;
pub const O_EXCL: c_int = 0x0080;
pub const O_NOCTTY: c_int = 0x0100;
pub const O_TRUNC: c_int = 0x0200;

// File status flags
pub const O_APPEND: c_int = 0x0400;
pub const O_NONBLOCK: c_int = 0x0800;
pub const O_SYNC: c_int = 0x1000;
pub const O_FSYNC: c_int = O_SYNC;
pub const O_DSYNC: c_int = 0x1000;
pub const O_DIRECTORY: c_int = 0x10000;
pub const O_NOFOLLOW: c_int = 0x20000;
pub const O_CLOEXEC: c_int = 0x80000;

// fcntl() commands
pub const F_DUPFD: c_int = 0;
pub const F_GETFD: c_int = 1;
pub const F_SETFD: c_int = 2;
pub const F_GETFL: c_int = 3;
pub const F_SETFL: c_int = 4;
pub const F_GETLK: c_int = 5;
pub const F_SETLK: c_int = 6;
pub const F_SETLKW: c_int = 7;
pub const F_DUPFD_CLOEXEC: c_int = 1030;

// File descriptor flags
pub const FD_CLOEXEC: c_int = 1;

// Lock types for fcntl()
pub const F_RDLCK: c_int = 0;
pub const F_WRLCK: c_int = 1;
pub const F_UNLCK: c_int = 2;

// Functions
extern "C" {
    pub fn open(pathname: *const c_char, flags: c_int, ...) -> c_int;
    pub fn creat(pathname: *const c_char, mode: mode_t) -> c_int;
    pub fn fcntl(fd: c_int, cmd: c_int, ...) -> c_int;
}
