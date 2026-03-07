//! Header: `pthread/pthread_spis.h`
//!
//! <https://github.com/apple-oss-distributions/libpthread/blob/main/include/pthread/pthread_spis.h>

use crate::prelude::*;

extern "C" {
    pub fn pthread_create_from_mach_thread(
        thread: *mut crate::pthread_t,
        attr: *const crate::pthread_attr_t,
        f: extern "C" fn(*mut c_void) -> *mut c_void,
        value: *mut c_void,
    ) -> c_int;
}
