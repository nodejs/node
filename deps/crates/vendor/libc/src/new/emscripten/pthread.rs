//! Header: `pthread.h`

pub use crate::new::common::linux_like::pthread::pthread_getattr_np;
pub use crate::new::common::posix::pthread::{
    pthread_attr_getstack,
    pthread_attr_setstack,
    pthread_condattr_getclock,
    pthread_condattr_setclock,
    pthread_condattr_setpshared,
    pthread_create,
    pthread_mutexattr_setpshared,
    pthread_rwlockattr_getpshared,
    pthread_rwlockattr_setpshared,
};
