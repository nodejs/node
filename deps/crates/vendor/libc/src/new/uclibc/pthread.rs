//! Header: `pthread.h`
//!
//! Note that The l4re port of uclibc doesn't yet support all `pthread_*` API that is
//! available upstream.

pub use crate::new::common::linux_like::pthread::{
    pthread_getaffinity_np,
    pthread_getattr_np,
    pthread_setaffinity_np,
};
#[cfg(not(target_os = "l4re"))]
pub use crate::new::common::linux_like::pthread::{
    pthread_getname_np,
    pthread_setname_np,
};
#[cfg(not(target_os = "l4re"))]
pub use crate::new::common::posix::pthread::{
    pthread_atfork,
    pthread_barrierattr_getpshared,
    pthread_condattr_getclock,
    pthread_condattr_setclock,
    pthread_getcpuclockid,
    pthread_mutex_consistent,
    pthread_mutexattr_getprotocol,
    pthread_mutexattr_getrobust,
    pthread_mutexattr_setprotocol,
    pthread_mutexattr_setrobust,
    pthread_setschedprio,
};
pub use crate::new::common::posix::pthread::{
    pthread_attr_getguardsize,
    pthread_attr_getinheritsched,
    pthread_attr_getschedparam,
    pthread_attr_getschedpolicy,
    pthread_attr_getstack,
    pthread_attr_setguardsize,
    pthread_attr_setinheritsched,
    pthread_attr_setschedparam,
    pthread_attr_setschedpolicy,
    pthread_attr_setstack,
    pthread_barrier_destroy,
    pthread_barrier_init,
    pthread_barrier_wait,
    pthread_barrierattr_destroy,
    pthread_barrierattr_init,
    pthread_barrierattr_setpshared,
    pthread_cancel,
    pthread_condattr_getpshared,
    pthread_condattr_setpshared,
    pthread_create,
    pthread_getschedparam,
    pthread_kill,
    pthread_mutex_timedlock,
    pthread_mutexattr_getpshared,
    pthread_mutexattr_setpshared,
    pthread_once,
    pthread_rwlockattr_getpshared,
    pthread_rwlockattr_setpshared,
    pthread_setschedparam,
    pthread_sigmask,
    pthread_spin_destroy,
    pthread_spin_init,
    pthread_spin_lock,
    pthread_spin_trylock,
    pthread_spin_unlock,
};
