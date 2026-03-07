//! Header: `pthread/spawn.h`
//!
//! <https://github.com/apple-oss-distributions/libpthread/blob/main/include/pthread/spawn.h>

use crate::prelude::*;

extern "C" {
    pub fn posix_spawnattr_set_qos_class_np(
        attr: *mut crate::posix_spawnattr_t,
        qos_class: crate::qos_class_t,
    ) -> c_int;
    pub fn posix_spawnattr_get_qos_class_np(
        attr: *const crate::posix_spawnattr_t,
        qos_class: *mut crate::qos_class_t,
    ) -> c_int;
}
