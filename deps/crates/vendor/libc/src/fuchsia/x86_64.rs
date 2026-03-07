use crate::off_t;
use crate::prelude::*;

pub type wchar_t = i32;
pub type nlink_t = u64;
pub type blksize_t = c_long;
pub type __u64 = c_ulonglong;

s! {
    pub struct stat {
        pub st_dev: crate::dev_t,
        pub st_ino: crate::ino_t,
        pub st_nlink: crate::nlink_t,
        pub st_mode: crate::mode_t,
        pub st_uid: crate::uid_t,
        pub st_gid: crate::gid_t,
        __pad0: Padding<c_int>,
        pub st_rdev: crate::dev_t,
        pub st_size: off_t,
        pub st_blksize: crate::blksize_t,
        pub st_blocks: crate::blkcnt_t,
        pub st_atime: crate::time_t,
        pub st_atime_nsec: c_long,
        pub st_mtime: crate::time_t,
        pub st_mtime_nsec: c_long,
        pub st_ctime: crate::time_t,
        pub st_ctime_nsec: c_long,
        __unused: Padding<[c_long; 3]>,
    }

    pub struct stat64 {
        pub st_dev: crate::dev_t,
        pub st_ino: crate::ino64_t,
        pub st_nlink: crate::nlink_t,
        pub st_mode: crate::mode_t,
        pub st_uid: crate::uid_t,
        pub st_gid: crate::gid_t,
        __pad0: Padding<c_int>,
        pub st_rdev: crate::dev_t,
        pub st_size: off_t,
        pub st_blksize: crate::blksize_t,
        pub st_blocks: crate::blkcnt64_t,
        pub st_atime: crate::time_t,
        pub st_atime_nsec: c_long,
        pub st_mtime: crate::time_t,
        pub st_mtime_nsec: c_long,
        pub st_ctime: crate::time_t,
        pub st_ctime_nsec: c_long,
        __reserved: Padding<[c_long; 3]>,
    }

    pub struct mcontext_t {
        __private: [u64; 32],
    }

    pub struct ipc_perm {
        pub __ipc_perm_key: crate::key_t,
        pub uid: crate::uid_t,
        pub gid: crate::gid_t,
        pub cuid: crate::uid_t,
        pub cgid: crate::gid_t,
        pub mode: crate::mode_t,
        pub __seq: c_int,
        __unused1: Padding<c_long>,
        __unused2: Padding<c_long>,
    }

    pub struct ucontext_t {
        pub uc_flags: c_ulong,
        pub uc_link: *mut ucontext_t,
        pub uc_stack: crate::stack_t,
        pub uc_mcontext: mcontext_t,
        pub uc_sigmask: crate::sigset_t,
        __private: [u8; 512],
    }
}

// offsets in user_regs_structs, from sys/reg.h
pub const R15: c_int = 0;
pub const R14: c_int = 1;
pub const R13: c_int = 2;
pub const R12: c_int = 3;
pub const RBP: c_int = 4;
pub const RBX: c_int = 5;
pub const R11: c_int = 6;
pub const R10: c_int = 7;
pub const R9: c_int = 8;
pub const R8: c_int = 9;
pub const RAX: c_int = 10;
pub const RCX: c_int = 11;
pub const RDX: c_int = 12;
pub const RSI: c_int = 13;
pub const RDI: c_int = 14;
pub const ORIG_RAX: c_int = 15;
pub const RIP: c_int = 16;
pub const CS: c_int = 17;
pub const EFLAGS: c_int = 18;
pub const RSP: c_int = 19;
pub const SS: c_int = 20;
pub const FS_BASE: c_int = 21;
pub const GS_BASE: c_int = 22;
pub const DS: c_int = 23;
pub const ES: c_int = 24;
pub const FS: c_int = 25;
pub const GS: c_int = 26;

pub const MAP_32BIT: c_int = 0x0040;

pub const SIGSTKSZ: size_t = 8192;
pub const MINSIGSTKSZ: size_t = 2048;
