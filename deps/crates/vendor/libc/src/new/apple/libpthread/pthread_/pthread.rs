//! Header: `pthread.h` or `pthread/pthread.h`
//!
//! <https://github.com/apple-oss-distributions/libpthread/blob/main/include/pthread/pthread.h>

use crate::prelude::*;
pub use crate::pthread_::qos::*;
pub use crate::pthread_::sched::*;
// No need to import from the `_pthread_attr_t` and similar modules since `_pthread_types` has
// everything we need.
pub use crate::sys::_pthread::_pthread_types::*;

pub const PTHREAD_CREATE_JOINABLE: c_int = 1;
pub const PTHREAD_CREATE_DETACHED: c_int = 2;

pub const PTHREAD_INHERIT_SCHED: c_int = 1;
pub const PTHREAD_EXPLICIT_SCHED: c_int = 2;

pub const PTHREAD_CANCEL_ENABLE: c_int = 0x01;
pub const PTHREAD_CANCEL_DISABLE: c_int = 0x00;
pub const PTHREAD_CANCEL_DEFERRED: c_int = 0x02;
pub const PTHREAD_CANCEL_ASYNCHRONOUS: c_int = 0x00;

pub const PTHREAD_CANCELED: *mut c_void = 1 as *mut c_void;

pub const PTHREAD_SCOPE_SYSTEM: c_int = 1;
pub const PTHREAD_SCOPE_PROCESS: c_int = 2;

pub const PTHREAD_PROCESS_SHARED: c_int = 1;
pub const PTHREAD_PROCESS_PRIVATE: c_int = 2;

pub const PTHREAD_PRIO_NONE: c_int = 0;
pub const PTHREAD_PRIO_INHERIT: c_int = 1;
pub const PTHREAD_PRIO_PROTECT: c_int = 2;

pub const PTHREAD_MUTEX_NORMAL: c_int = 0;
pub const PTHREAD_MUTEX_ERRORCHECK: c_int = 1;
pub const PTHREAD_MUTEX_RECURSIVE: c_int = 2;
pub const PTHREAD_MUTEX_DEFAULT: c_int = PTHREAD_MUTEX_NORMAL;

pub const PTHREAD_RWLOCK_INITIALIZER: pthread_rwlock_t = pthread_rwlock_t {
    __sig: _PTHREAD_RWLOCK_SIG_init,
    __opaque: [0; __PTHREAD_RWLOCK_SIZE__],
};

pub const PTHREAD_MUTEX_INITIALIZER: pthread_mutex_t = pthread_mutex_t {
    __sig: _PTHREAD_MUTEX_SIG_init,
    __opaque: [0; __PTHREAD_MUTEX_SIZE__],
};

pub const PTHREAD_COND_INITIALIZER: pthread_cond_t = pthread_cond_t {
    __sig: _PTHREAD_COND_SIG_init,
    __opaque: [0; __PTHREAD_COND_SIZE__],
};

pub const PTHREAD_ONCE_INIT: crate::pthread_once_t = crate::pthread_once_t {
    __sig: _PTHREAD_ONCE_SIG_INIT,
    __opaque: [0; __PTHREAD_ONCE_SIZE__],
};

pub use crate::new::common::posix::pthread::{
    pthread_attr_getinheritsched,
    pthread_attr_getschedparam,
    pthread_attr_getschedpolicy,
    pthread_attr_setinheritsched,
    pthread_attr_setschedparam,
    pthread_attr_setschedpolicy,
    pthread_condattr_getpshared,
    pthread_condattr_setpshared,
    pthread_getschedparam,
    pthread_mutexattr_getpshared,
    pthread_mutexattr_setpshared,
    pthread_once,
    pthread_rwlockattr_getpshared,
    pthread_rwlockattr_setpshared,
    pthread_setschedparam,
};
