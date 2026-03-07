//! Header: `sys/_pthread/_pthread_types.h`
//!
//! <https://github.com/apple-oss-distributions/libpthread/blob/main/include/sys/_pthread/_pthread_types.h>
//!
//! Note that the actual header defines `_opaque_pthread_*` structs which are typedefed to
//! `__darwin_pthread*` structs, and typedefed again in separate `_pthread_*.h` files to their final
//! `pthread_` name. This isn't useful for us so we simplify a bit and just define everything here.

use crate::prelude::*;

cfg_if! {
    if #[cfg(target_pointer_width = "64")] {
        pub const __PTHREAD_SIZE__: usize = 8176;
        pub const __PTHREAD_ATTR_SIZE__: usize = 56;
        pub const __PTHREAD_MUTEXATTR_SIZE__: usize = 8;
        pub const __PTHREAD_MUTEX_SIZE__: usize = 56;
        pub const __PTHREAD_CONDATTR_SIZE__: usize = 8;
        pub const __PTHREAD_COND_SIZE__: usize = 40;
        pub const __PTHREAD_ONCE_SIZE__: usize = 8;
        pub const __PTHREAD_RWLOCK_SIZE__: usize = 192;
        pub const __PTHREAD_RWLOCKATTR_SIZE__: usize = 16;
    } else {
        pub const __PTHREAD_SIZE__: usize = 4088;
        pub const __PTHREAD_ATTR_SIZE__: usize = 36;
        pub const __PTHREAD_MUTEXATTR_SIZE__: usize = 8;
        pub const __PTHREAD_MUTEX_SIZE__: usize = 40;
        pub const __PTHREAD_CONDATTR_SIZE__: usize = 4;
        pub const __PTHREAD_COND_SIZE__: usize = 24;
        pub const __PTHREAD_ONCE_SIZE__: usize = 4;
        pub const __PTHREAD_RWLOCK_SIZE__: usize = 124;
        pub const __PTHREAD_RWLOCKATTR_SIZE__: usize = 12;
    }
}

s! {
    pub struct pthread_attr_t {
        pub(crate) __sig: c_long,
        pub(crate) __opaque: [c_char; __PTHREAD_ATTR_SIZE__],
    }

    pub struct pthread_cond_t {
        pub(crate) __sig: c_long,
        pub(crate) __opaque: [u8; __PTHREAD_COND_SIZE__],
    }

    pub struct pthread_condattr_t {
        pub(crate) __sig: c_long,
        pub(crate) __opaque: [u8; __PTHREAD_CONDATTR_SIZE__],
    }

    pub struct pthread_mutex_t {
        pub(crate) __sig: c_long,
        pub(crate) __opaque: [u8; __PTHREAD_MUTEX_SIZE__],
    }

    pub struct pthread_mutexattr_t {
        pub(crate) __sig: c_long,
        pub(crate) __opaque: [u8; __PTHREAD_MUTEXATTR_SIZE__],
    }

    pub struct pthread_once_t {
        pub(crate) __sig: c_long,
        pub(crate) __opaque: [c_char; __PTHREAD_ONCE_SIZE__],
    }

    pub struct pthread_rwlock_t {
        pub(crate) __sig: c_long,
        pub(crate) __opaque: [u8; __PTHREAD_RWLOCK_SIZE__],
    }

    pub struct pthread_rwlockattr_t {
        pub(crate) __sig: c_long,
        pub(crate) __opaque: [u8; __PTHREAD_RWLOCKATTR_SIZE__],
    }
}

pub type pthread_key_t = c_ulong;

pub use crate::pthread_t;
