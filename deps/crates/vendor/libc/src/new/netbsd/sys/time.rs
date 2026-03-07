//! Header: `sys/time.h`
//!
//! <https://github.com/NetBSD/src/blob/trunk/sys/sys/time.h>

s! {
    pub struct itimerspec {
        pub it_interval: crate::timespec,
        pub it_value: crate::timespec,
    }
}

pub const CLOCK_THREAD_CPUTIME_ID: crate::clockid_t = 0x20000000;
pub const CLOCK_PROCESS_CPUTIME_ID: crate::clockid_t = 0x40000000;
