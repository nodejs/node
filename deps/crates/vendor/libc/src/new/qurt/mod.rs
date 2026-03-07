//! QuRT (Qualcomm Real-Time OS) bindings
//!
//! QuRT is Qualcomm's real-time operating system for Hexagon DSP architectures.
//! Headers available via the
//! Hexagon SDK: https://softwarecenter.qualcomm.com/catalog/item/Hexagon_SDK

use crate::prelude::*;

// Basic C types for QuRT
pub type intptr_t = isize;
pub type uintptr_t = usize;
pub type ptrdiff_t = isize;
pub type size_t = uintptr_t;
pub type ssize_t = c_int;

// Process and system types
pub type pid_t = c_int;
pub type uid_t = c_uint;
pub type gid_t = c_uint;

// Time types
pub type time_t = c_long;
pub type suseconds_t = c_long;
pub type useconds_t = c_ulong;
pub type clockid_t = c_int;
pub type timer_t = *mut c_void;

// File system types
pub type dev_t = c_ulonglong;
pub type ino_t = c_ulonglong;
pub type mode_t = c_uint;
pub type nlink_t = c_uint;
pub type off_t = c_long;
pub type blkcnt_t = c_long;
pub type blksize_t = c_long;

// Thread types based on QuRT pthread implementation
pub type pthread_t = c_uint;
pub type pthread_key_t = c_int;
pub type pthread_once_t = c_int;
pub type pthread_mutex_t = c_uint;
pub type pthread_mutexattr_t = c_uint;
pub type pthread_cond_t = c_uint;
pub type pthread_condattr_t = c_uint;
pub type pthread_attr_t = c_uint;
pub type pthread_rwlock_t = c_uint;
pub type pthread_rwlockattr_t = c_uint;
pub type pthread_spinlock_t = c_uint;
pub type pthread_barrier_t = c_uint;
pub type pthread_barrierattr_t = c_uint;

// Network types
pub type socklen_t = c_uint;
pub type sa_family_t = c_ushort;
pub type in_addr_t = c_uint;
pub type in_port_t = c_ushort;

// File descriptor types
pub type fd_set = c_ulong;

// Standard C library types
extern_ty! {
    pub enum FILE {}
}
pub type fpos_t = c_long;
pub type clock_t = c_long;

// POSIX semaphore types
pub type sem_t = c_uint;

// Message queue types
pub type mqd_t = c_int;

// Additional file system types
pub type nfds_t = c_ulong;

// Signal types
pub type sigset_t = c_ulong;

// Variadic argument list type
pub type va_list = *mut c_char;

// Additional missing types
pub type c_schar = i8;

// Wide character type (hexagon-specific)
pub type wchar_t = u32;

// Error type (compatible with std expectations)
pub type errno_t = c_int;

// Resource limit type (for compatibility, not fully supported on QuRT)
pub type rlim_t = c_ulong;

// Terminal types (for compatibility, not fully supported on QuRT)
pub type speed_t = c_uint;
pub type tcflag_t = c_uint;

// Division result types and structures
s! {
    pub struct div_t {
        pub quot: c_int,
        pub rem: c_int,
    }

    pub struct ldiv_t {
        pub quot: c_long,
        pub rem: c_long,
    }

    pub struct lldiv_t {
        pub quot: c_longlong,
        pub rem: c_longlong,
    }

    pub struct stat {
        pub st_dev: dev_t,
        pub st_ino: ino_t,
        pub st_mode: mode_t,
        pub st_nlink: nlink_t,
        pub st_rdev: dev_t,
        pub st_size: off_t,
        pub st_atime: time_t,
        pub st_mtime: time_t,
        pub st_ctime: time_t,
    }

    pub struct tm {
        pub tm_sec: c_int,
        pub tm_min: c_int,
        pub tm_hour: c_int,
        pub tm_mday: c_int,
        pub tm_mon: c_int,
        pub tm_year: c_int,
        pub tm_wday: c_int,
        pub tm_yday: c_int,
        pub tm_isdst: c_int,
    }

    pub struct timespec {
        pub tv_sec: time_t,
        pub tv_nsec: c_long,
    }

    pub struct timeval {
        pub tv_sec: time_t,
        pub tv_usec: suseconds_t,
    }

    pub struct itimerspec {
        pub it_interval: timespec,
        pub it_value: timespec,
    }

    pub struct dirent {
        pub d_ino: c_long,
        pub d_name: [c_char; 255],
    }

    pub struct DIR {
        pub index: c_int,
        pub entry: dirent,
    }

    // Terminal I/O structure (for compatibility, limited support on QuRT)
    pub struct termios {
        pub c_iflag: tcflag_t,
        pub c_oflag: tcflag_t,
        pub c_cflag: tcflag_t,
        pub c_lflag: tcflag_t,
        pub c_cc: [c_uchar; 32],
        pub c_ispeed: speed_t,
        pub c_ospeed: speed_t,
    }

    // Resource limit structures (for compatibility, limited support on QuRT)
    pub struct rlimit {
        pub rlim_cur: rlim_t,
        pub rlim_max: rlim_t,
    }

    pub struct rusage {
        pub ru_utime: timeval,
        pub ru_stime: timeval,
        pub ru_maxrss: c_long,
        pub ru_ixrss: c_long,
        pub ru_idrss: c_long,
        pub ru_isrss: c_long,
        pub ru_minflt: c_long,
        pub ru_majflt: c_long,
        pub ru_nswap: c_long,
        pub ru_inblock: c_long,
        pub ru_oublock: c_long,
        pub ru_msgsnd: c_long,
        pub ru_msgrcv: c_long,
        pub ru_nsignals: c_long,
        pub ru_nvcsw: c_long,
        pub ru_nivcsw: c_long,
    }

    // File lock structure (for compatibility)
    pub struct flock {
        pub l_type: c_short,
        pub l_whence: c_short,
        pub l_start: off_t,
        pub l_len: off_t,
        pub l_pid: pid_t,
    }
}

// Additional pthread constants
pub const PTHREAD_NAME_LEN: c_int = 16;
pub const PTHREAD_MAX_THREADS: c_uint = 512;
pub const PTHREAD_MIN_STACKSIZE: c_int = 512;
pub const PTHREAD_MAX_STACKSIZE: c_int = 1048576;
pub const PTHREAD_DEFAULT_STACKSIZE: c_int = 16384;
pub const PTHREAD_DEFAULT_PRIORITY: c_int = 1;
pub const PTHREAD_SPINLOCK_UNLOCKED: c_int = 0;
pub const PTHREAD_SPINLOCK_LOCKED: c_int = 1;

// Additional time constants
pub const TIME_CONV_SCLK_FREQ: c_int = 19200000;
pub const CLOCK_MONOTONIC_RAW: clockid_t = 4;
pub const CLOCK_REALTIME_COARSE: clockid_t = 5;
pub const CLOCK_MONOTONIC_COARSE: clockid_t = 6;
pub const CLOCK_BOOTTIME: clockid_t = 7;

// Stdio constants
pub const L_tmpnam: c_uint = 260;
pub const TMP_MAX: c_uint = 25;
pub const FOPEN_MAX: c_uint = 20;

// Error constants
pub const EOK: c_int = 0;

// Semaphore constants
pub const SEM_FAILED: *mut sem_t = 0 as *mut sem_t;

// Page size constants (hexagon-specific)
pub const PAGESIZE: size_t = 4096;
pub const PAGE_SIZE: size_t = 4096;

// Directory entry types (hexagon-specific)
pub const DT_UNKNOWN: c_uchar = 0;
pub const DT_FIFO: c_uchar = 1;
pub const DT_CHR: c_uchar = 2;
pub const DT_DIR: c_uchar = 4;
pub const DT_BLK: c_uchar = 6;
pub const DT_REG: c_uchar = 8;
pub const DT_LNK: c_uchar = 10;
pub const DT_SOCK: c_uchar = 12;

// Directory functions (dirent.h)
extern "C" {
    pub fn opendir(name: *const c_char) -> *mut DIR;
    pub fn readdir(dirp: *mut DIR) -> *mut dirent;
    pub fn closedir(dirp: *const DIR) -> c_int;
    pub fn mkdir(path: *const c_char, mode: mode_t) -> c_int;
}

// Additional pthread functions
extern "C" {
    pub fn pthread_attr_getstack(
        attr: *const pthread_attr_t,
        stackaddr: *mut *mut c_void,
        stacksize: *mut size_t,
    ) -> c_int;
    pub fn pthread_attr_setstack(
        attr: *mut pthread_attr_t,
        stackaddr: *mut c_void,
        stacksize: size_t,
    ) -> c_int;
}

// Additional time functions
extern "C" {
    pub fn clock_getcpuclockid(pid: pid_t, clock_id: *mut clockid_t) -> c_int;
}

// POSIX semaphore functions
extern "C" {
    pub fn sem_open(name: *const c_char, oflag: c_int, ...) -> *mut sem_t;
    pub fn sem_close(sem: *mut sem_t) -> c_int;
    pub fn sem_unlink(name: *const c_char) -> c_int;
}

// Additional stdlib functions
extern "C" {
    pub fn aligned_alloc(alignment: size_t, size: size_t) -> *mut c_void;
}

// String functions (string.h)
extern "C" {
    pub fn strlen(s: *const c_char) -> size_t;
    pub fn strcpy(dest: *mut c_char, src: *const c_char) -> *mut c_char;
    pub fn strncpy(dest: *mut c_char, src: *const c_char, n: size_t) -> *mut c_char;
    pub fn strcat(dest: *mut c_char, src: *const c_char) -> *mut c_char;
    pub fn strncat(dest: *mut c_char, src: *const c_char, n: size_t) -> *mut c_char;
    pub fn strcmp(s1: *const c_char, s2: *const c_char) -> c_int;
    pub fn strncmp(s1: *const c_char, s2: *const c_char, n: size_t) -> c_int;
    pub fn strcoll(s1: *const c_char, s2: *const c_char) -> c_int;
    pub fn strxfrm(dest: *mut c_char, src: *const c_char, n: size_t) -> size_t;
    pub fn strchr(s: *const c_char, c: c_int) -> *mut c_char;
    pub fn strrchr(s: *const c_char, c: c_int) -> *mut c_char;
    pub fn strspn(s: *const c_char, accept: *const c_char) -> size_t;
    pub fn strcspn(s: *const c_char, reject: *const c_char) -> size_t;
    pub fn strpbrk(s: *const c_char, accept: *const c_char) -> *mut c_char;
    pub fn strstr(haystack: *const c_char, needle: *const c_char) -> *mut c_char;
    pub fn strtok(s: *mut c_char, delim: *const c_char) -> *mut c_char;
    pub fn strerror(errnum: c_int) -> *mut c_char;
    pub fn memchr(s: *const c_void, c: c_int, n: size_t) -> *mut c_void;
    pub fn memcmp(s1: *const c_void, s2: *const c_void, n: size_t) -> c_int;
    pub fn memcpy(dest: *mut c_void, src: *const c_void, n: size_t) -> *mut c_void;
    pub fn memmove(dest: *mut c_void, src: *const c_void, n: size_t) -> *mut c_void;
    pub fn memset(s: *mut c_void, c: c_int, n: size_t) -> *mut c_void;
}

// Additional unistd functions
extern "C" {
    pub fn fork() -> pid_t;
    pub fn execve(
        filename: *const c_char,
        argv: *const *const c_char,
        envp: *const *const c_char,
    ) -> c_int;
}

// Character classification functions (ctype.h)
extern "C" {
    pub fn isalnum(c: c_int) -> c_int;
    pub fn isalpha(c: c_int) -> c_int;
    pub fn iscntrl(c: c_int) -> c_int;
    pub fn isdigit(c: c_int) -> c_int;
    pub fn isgraph(c: c_int) -> c_int;
    pub fn islower(c: c_int) -> c_int;
    pub fn isprint(c: c_int) -> c_int;
    pub fn ispunct(c: c_int) -> c_int;
    pub fn isspace(c: c_int) -> c_int;
    pub fn isupper(c: c_int) -> c_int;
    pub fn isxdigit(c: c_int) -> c_int;
    pub fn tolower(c: c_int) -> c_int;
    pub fn toupper(c: c_int) -> c_int;
}

pub(crate) mod dlfcn;
pub(crate) mod errno;
pub(crate) mod fcntl;
pub(crate) mod limits;
pub(crate) mod pthread;
pub(crate) mod semaphore;
pub(crate) mod signal;
pub(crate) mod stdio;
pub(crate) mod stdlib;
pub(crate) mod sys;
pub(crate) mod time;
pub(crate) mod unistd;

// Re-export public items from submodules
pub use dlfcn::*;
pub use errno::*;
pub use fcntl::*;
pub use limits::*;
pub use pthread::*;
pub use semaphore::*;
pub use signal::*;
pub use stdio::*;
pub use stdlib::*;
pub use sys::mman::*;
pub use sys::sched::*;
pub use sys::stat::*;
pub use sys::types::*;
pub use time::*;
pub use unistd::*;
