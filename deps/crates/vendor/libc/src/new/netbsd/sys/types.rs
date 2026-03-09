//! Header: `sys/types.h`
//!
//! <https://github.com/NetBSD/src/blob/trunk/sys/sys/types.h>

use crate::prelude::*;

pub type dev_t = u64;

pub type lwpid_t = i32;

pub type mqd_t = c_int;
pub type cpuid_t = c_ulong;

pub type clock_t = c_uint;
pub type timer_t = c_int;
pub type suseconds_t = c_int;
