//! Header: `pthread/qos.h`
//!
//! <https://github.com/apple-oss-distributions/libpthread/blob/main/include/pthread/qos.h>

use crate::prelude::*;
pub use crate::sys::qos::*;

extern "C" {
    pub fn pthread_attr_set_qos_class_np(
        attr: *mut crate::pthread_attr_t,
        class: qos_class_t,
        priority: c_int,
    ) -> c_int;
    pub fn pthread_attr_get_qos_class_np(
        attr: *mut crate::pthread_attr_t,
        class: *mut qos_class_t,
        priority: *mut c_int,
    ) -> c_int;
    pub fn pthread_set_qos_class_self_np(class: qos_class_t, priority: c_int) -> c_int;
    pub fn pthread_get_qos_class_np(
        thread: crate::pthread_t,
        class: *mut qos_class_t,
        priority: *mut c_int,
    ) -> c_int;
}
