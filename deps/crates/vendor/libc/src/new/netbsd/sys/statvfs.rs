//! Header: `sys/statvfs.h`
//!
//! <https://github.com/NetBSD/src/blob/trunk/sys/sys/statvfs.h>

use crate::prelude::*;

const _VFS_NAMELEN: usize = 32;
const _VFS_MNAMELEN: usize = 1024;

s! {
    pub struct statvfs {
        pub f_flag: c_ulong,
        pub f_bsize: c_ulong,
        pub f_frsize: c_ulong,
        pub f_iosize: c_ulong,

        pub f_blocks: crate::fsblkcnt_t,
        pub f_bfree: crate::fsblkcnt_t,
        pub f_bavail: crate::fsblkcnt_t,
        pub f_bresvd: crate::fsblkcnt_t,

        pub f_files: crate::fsfilcnt_t,
        pub f_ffree: crate::fsfilcnt_t,
        pub f_favail: crate::fsfilcnt_t,
        pub f_fresvd: crate::fsfilcnt_t,

        pub f_syncreads: u64,
        pub f_syncwrites: u64,

        pub f_asyncreads: u64,
        pub f_asyncwrites: u64,

        pub f_fsidx: crate::fsid_t,
        pub f_fsid: c_ulong,
        pub f_namemax: c_ulong,
        pub f_owner: crate::uid_t,

        // This type is updated in a future version
        f_spare: [u32; 4],

        pub f_fstypename: [c_char; _VFS_NAMELEN],
        pub f_mntonname: [c_char; _VFS_MNAMELEN],
        pub f_mntfromname: [c_char; _VFS_MNAMELEN],
        // Added in NetBSD10
        // pub f_mntfromlabel: [c_char; _VFS_MNAMELEN],
    }
}
