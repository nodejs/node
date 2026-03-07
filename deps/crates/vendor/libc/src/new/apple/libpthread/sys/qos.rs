//! Header: `sys/qos.h`
//!
//! <https://github.com/apple-oss-distributions/libpthread/blob/main/include/sys/qos.h>

use crate::prelude::*;

#[derive(Debug)]
#[repr(u32)]
pub enum qos_class_t {
    QOS_CLASS_USER_INTERACTIVE = 0x21,
    QOS_CLASS_USER_INITIATED = 0x19,
    QOS_CLASS_DEFAULT = 0x15,
    QOS_CLASS_UTILITY = 0x11,
    QOS_CLASS_BACKGROUND = 0x09,
    QOS_CLASS_UNSPECIFIED = 0x00,
}
impl Copy for qos_class_t {}
impl Clone for qos_class_t {
    fn clone(&self) -> qos_class_t {
        *self
    }
}
