//! Header: `sys/ipc.h`
//!
//! <https://github.com/openbsd/src/blob/master/sys/sys/ipc.h>

use crate::prelude::*;

s! {
    pub struct ipc_perm {
        pub cuid: crate::uid_t,
        pub cgid: crate::gid_t,
        pub uid: crate::uid_t,
        pub gid: crate::gid_t,
        pub mode: crate::mode_t,
        pub seq: c_ushort,
        pub key: crate::key_t,
    }
}
