use crate::prelude::*;

// FIXME(apple): these should all be `pub(crate)`
pub const _PTHREAD_MUTEX_SIG_init: c_long = 0x32AAABA7;

pub const _PTHREAD_COND_SIG_init: c_long = 0x3CB0B1BB;
pub(crate) const _PTHREAD_ONCE_SIG_INIT: c_long = 0x30B1BCBA;
pub const _PTHREAD_RWLOCK_SIG_init: c_long = 0x2DA8B3B4;

pub const SCHED_OTHER: c_int = 1;
pub const SCHED_FIFO: c_int = 4;
pub const SCHED_RR: c_int = 2;
