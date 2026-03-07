//! Header: `signal.h`

use super::*;
use crate::prelude::*;

// Standard signal numbers
pub const SIGHUP: c_int = 1;
pub const SIGINT: c_int = 2;
pub const SIGQUIT: c_int = 3;
pub const SIGILL: c_int = 4;
pub const SIGTRAP: c_int = 5;
pub const SIGABRT: c_int = 6;
pub const SIGBUS: c_int = 7;
pub const SIGFPE: c_int = 8;
pub const SIGKILL: c_int = 9;
pub const SIGUSR1: c_int = 10;
pub const SIGSEGV: c_int = 11;
pub const SIGUSR2: c_int = 12;
pub const SIGPIPE: c_int = 13;
pub const SIGALRM: c_int = 14;
pub const SIGTERM: c_int = 15;
pub const SIGSTKFLT: c_int = 16;
pub const SIGCHLD: c_int = 17;
pub const SIGCONT: c_int = 18;
pub const SIGSTOP: c_int = 19;
pub const SIGTSTP: c_int = 20;
pub const SIGTTIN: c_int = 21;
pub const SIGTTOU: c_int = 22;
pub const SIGURG: c_int = 23;
pub const SIGXCPU: c_int = 24;
pub const SIGXFSZ: c_int = 25;
pub const SIGVTALRM: c_int = 26;
pub const SIGPROF: c_int = 27;
pub const SIGWINCH: c_int = 28;
pub const SIGIO: c_int = 29;
pub const SIGPWR: c_int = 30;
pub const SIGSYS: c_int = 31;

// Signal handling constants
pub const SIG_DFL: sighandler_t = 0 as sighandler_t;
pub const SIG_IGN: sighandler_t = 1 as sighandler_t;
pub const SIG_ERR: sighandler_t = !0 as sighandler_t;

// Signal mask operations (QuRT uses different values than Linux)
pub const SIG_BLOCK: c_int = 1;
pub const SIG_UNBLOCK: c_int = 2;
pub const SIG_SETMASK: c_int = 3;

// QuRT-specific signal constants
pub const POSIX_MSG: c_int = 7;
pub const POSIX_NOTIF: c_int = 8;
pub const SIGRTMIN: c_int = 10;
pub const SIGRTMAX: c_int = 32;

// Notification types (from QuRT signal.h)
pub const SIGEV_NONE: c_int = 0;
pub const SIGEV_SIGNAL: c_int = 1;
pub const SIGEV_THREAD: c_int = 2;
pub const SA_SIGINFO: c_int = 1;

pub type sighandler_t = size_t;

// Signal structures based on QuRT SDK headers
s! {
    pub struct sigval {
        pub sival_int: c_int,
        pub sival_ptr: *mut c_void,
    }

    pub struct sigevent {
        pub sigev_notify: c_int,
        pub sigev_signo: c_int,
        pub sigev_value: sigval,
        pub sigev_notify_function: Option<extern "C" fn(sigval)>,
        pub sigev_notify_attributes: *mut pthread_attr_t,
    }

    pub struct siginfo_t {
        pub si_signo: c_int,
        pub si_code: c_int,
        pub si_value: sigval,
    }

    pub struct sigaction {
        pub sa_handler: Option<extern "C" fn(c_int)>,
        pub sa_mask: sigset_t,
        pub sa_flags: c_int,
        pub sa_sigaction: Option<extern "C" fn(c_int, *mut siginfo_t, *mut c_void)>,
    }
}

extern "C" {
    pub fn signal(sig: c_int, handler: sighandler_t) -> sighandler_t;
    pub fn kill(pid: pid_t, sig: c_int) -> c_int;
    pub fn raise(sig: c_int) -> c_int;
    pub fn alarm(seconds: c_uint) -> c_uint;
    pub fn pause() -> c_int;

    // Signal mask functions
    pub fn sigemptyset(set: *mut sigset_t) -> c_int;
    pub fn sigfillset(set: *mut sigset_t) -> c_int;
    pub fn sigaddset(set: *mut sigset_t, signum: c_int) -> c_int;
    pub fn sigdelset(set: *mut sigset_t, signum: c_int) -> c_int;
    pub fn sigismember(set: *const sigset_t, signum: c_int) -> c_int;
    pub fn sigprocmask(how: c_int, set: *const sigset_t, oldset: *mut sigset_t) -> c_int;
    pub fn sigpending(set: *mut sigset_t) -> c_int;
    pub fn sigsuspend(mask: *const sigset_t) -> c_int;

    // QuRT-specific signal functions
    pub fn sigwait(set: *const sigset_t, sig: *mut c_int) -> c_int;
    pub fn _sigaction(sig: c_int, act: *const sigaction, oact: *mut sigaction) -> c_int;
    pub fn sigtimedwait(
        set: *const sigset_t,
        info: *mut siginfo_t,
        timeout: *const timespec,
    ) -> c_int;
}
