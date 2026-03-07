//! Header: `pthread/introspection.h`
//!
//! <https://github.com/apple-oss-distributions/libpthread/blob/main/include/pthread/introspection.h>

use crate::prelude::*;
pub use crate::pthread_::pthread::*;

c_enum! {
    #[repr(c_uint)]
    pub enum #anon {
        pub PTHREAD_INTROSPECTION_THREAD_CREATE = 1,
        pub PTHREAD_INTROSPECTION_THREAD_START,
        pub PTHREAD_INTROSPECTION_THREAD_TERMINATE,
        pub PTHREAD_INTROSPECTION_THREAD_DESTROY,
    }
}

pub type pthread_introspection_hook_t =
    extern "C" fn(event: c_uint, thread: pthread_t, addr: *mut c_void, size: size_t);

extern "C" {
    // Available from Big Sur
    pub fn pthread_introspection_hook_install(
        hook: pthread_introspection_hook_t,
    ) -> pthread_introspection_hook_t;
    pub fn pthread_introspection_setspecific_np(
        thread: pthread_t,
        key: pthread_key_t,
        value: *const c_void,
    ) -> c_int;

    pub fn pthread_introspection_getspecific_np(
        thread: pthread_t,
        key: pthread_key_t,
    ) -> *mut c_void;
}
