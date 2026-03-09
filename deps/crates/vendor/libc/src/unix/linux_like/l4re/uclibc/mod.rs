use crate::off64_t;
use crate::prelude::*;

pub type shmatt_t = c_ulong;
pub type regoff_t = c_int;
pub type rlim_t = c_ulong;
pub type __rlimit_resource_t = c_int;
pub type __priority_which_t = c_uint;

pub type _pthread_descr = *mut c_void;
pub type __pthread_cond_align_t = c_long;

cfg_if! {
    if #[cfg(doc)] {
        // Used in `linux::arch` to define ioctl constants.
        pub(crate) type Ioctl = c_ulong;
    } else {
        #[doc(hidden)]
        pub type Ioctl = c_ulong;
    }
}

s! {
    pub struct cmsghdr {
        pub cmsg_len: crate::size_t,
        pub cmsg_level: c_int,
        pub cmsg_type: c_int,
    }

    pub struct msghdr {
        pub msg_name: *mut c_void,
        pub msg_namelen: crate::socklen_t,
        pub msg_iov: *mut crate::iovec,
        #[cfg(target_pointer_width = "32")]
        pub msg_iovlen: c_int,
        #[cfg(target_pointer_width = "64")]
        pub msg_iovlen: crate::size_t,
        pub msg_control: *mut c_void,
        #[cfg(target_pointer_width = "32")]
        pub msg_controllen: crate::socklen_t,
        #[cfg(target_pointer_width = "64")]
        pub msg_controllen: crate::size_t,
        pub msg_flags: c_int,
    }

    pub struct statfs {
        pub f_type: fsword_t,
        pub f_bsize: fsword_t,
        pub f_blocks: crate::fsblkcnt_t,
        pub f_bfree: crate::fsblkcnt_t,
        pub f_bavail: crate::fsblkcnt_t,
        pub f_files: crate::fsfilcnt_t,
        pub f_ffree: crate::fsfilcnt_t,
        pub f_fsid: crate::fsid_t,
        pub f_namelen: fsword_t,
        pub f_frsize: fsword_t,
        pub f_flags: fsword_t,
        pub f_spare: [fsword_t; 4],
    }

    pub struct statfs64 {
        pub f_type: fsword_t,
        pub f_bsize: fsword_t,
        pub f_blocks: crate::fsblkcnt64_t,
        pub f_bfree: crate::fsblkcnt64_t,
        pub f_bavail: crate::fsblkcnt64_t,
        pub f_files: crate::fsfilcnt64_t,
        pub f_ffree: crate::fsfilcnt64_t,
        pub f_fsid: crate::fsid_t,
        pub f_namelen: fsword_t,
        pub f_frsize: fsword_t,
        pub f_flags: fsword_t,
        pub f_spare: [fsword_t; 4],
    }

    pub struct statvfs64 {
        pub f_bsize: c_ulong,
        pub f_frsize: c_ulong,
        pub f_blocks: crate::fsfilcnt64_t,
        pub f_bfree: crate::fsfilcnt64_t,
        pub f_bavail: crate::fsfilcnt64_t,
        pub f_files: crate::fsfilcnt64_t,
        pub f_ffree: crate::fsfilcnt64_t,
        pub f_favail: crate::fsfilcnt64_t,
        pub f_fsid: c_ulong,
        pub f_flag: c_ulong,
        pub f_namemax: c_ulong,
        pub __f_spare: [c_int; 6],
    }

    pub struct ipc_perm {
        pub __key: crate::key_t,
        pub uid: crate::uid_t,
        pub gid: crate::gid_t,
        pub cuid: crate::uid_t,
        pub cgid: crate::gid_t,
        #[cfg(target_pointer_width = "32")]
        pub mode: c_ushort,
        #[cfg(target_pointer_width = "64")]
        pub mode: c_uint,
        #[cfg(target_pointer_width = "32")]
        __pad1: c_ushort,
        pub __seq: c_ushort,
        __pad2: c_ushort,
        __unused1: c_ulong,
        __unused2: c_ulong,
    }

    pub struct statvfs {
        // Different than GNU!
        pub f_bsize: c_ulong,
        pub f_frsize: c_ulong,
        pub f_blocks: crate::fsblkcnt_t,
        pub f_bfree: crate::fsblkcnt_t,
        pub f_bavail: crate::fsblkcnt_t,
        pub f_files: crate::fsfilcnt_t,
        pub f_ffree: crate::fsfilcnt_t,
        pub f_favail: crate::fsfilcnt_t,
        #[cfg(target_endian = "little")]
        pub f_fsid: c_ulong,
        #[cfg(target_pointer_width = "32")]
        __f_unused: c_int,
        #[cfg(target_endian = "big")]
        pub f_fsid: c_ulong,
        pub f_flag: c_ulong,
        pub f_namemax: c_ulong,
        __f_spare: [c_int; 6],
    }

    pub struct sysinfo {
        pub uptime: c_long,
        pub loads: [c_ulong; 3],
        pub totalram: c_ulong,
        pub freeram: c_ulong,
        pub sharedram: c_ulong,
        pub bufferram: c_ulong,
        pub totalswap: c_ulong,
        pub freeswap: c_ulong,
        pub procs: c_ushort,
        pub pad: c_ushort,
        pub totalhigh: c_ulong,
        pub freehigh: c_ulong,
        pub mem_unit: c_uint,
        #[cfg(target_pointer_width = "32")]
        pub _f: [c_char; 8],
        #[cfg(target_pointer_width = "64")]
        pub _f: [c_char; 0],
    }

    pub struct regex_t {
        __buffer: *mut c_void,
        __allocated: size_t,
        __used: size_t,
        __syntax: c_ulong,
        __fastmap: *mut c_char,
        __translate: *mut c_char,
        __re_nsub: size_t,
        __bitfield: u8,
    }

    pub struct rtentry {
        pub rt_pad1: c_ulong,
        pub rt_dst: crate::sockaddr,
        pub rt_gateway: crate::sockaddr,
        pub rt_genmask: crate::sockaddr,
        pub rt_flags: c_ushort,
        pub rt_pad2: c_short,
        pub rt_pad3: c_ulong,
        pub rt_tos: c_uchar,
        pub rt_class: c_uchar,
        #[cfg(target_pointer_width = "64")]
        pub rt_pad4: [c_short; 3usize],
        #[cfg(not(target_pointer_width = "64"))]
        pub rt_pad4: c_short,
        pub rt_metric: c_short,
        pub rt_dev: *mut c_char,
        pub rt_mtu: c_ulong,
        pub rt_window: c_ulong,
        pub rt_irtt: c_ushort,
    }

    pub struct __exit_status {
        pub e_termination: c_short,
        pub e_exit: c_short,
    }

    pub struct tcp_info {
        pub tcpi_state: u8,
        pub tcpi_ca_state: u8,
        pub tcpi_retransmits: u8,
        pub tcpi_probes: u8,
        pub tcpi_backoff: u8,
        pub tcpi_options: u8,
        /// This contains the bitfields `tcpi_snd_wscale` and `tcpi_rcv_wscale`.
        /// Each is 4 bits.
        pub tcpi_snd_rcv_wscale: u8,
        pub tcpi_rto: u32,
        pub tcpi_ato: u32,
        pub tcpi_snd_mss: u32,
        pub tcpi_rcv_mss: u32,
        pub tcpi_unacked: u32,
        pub tcpi_sacked: u32,
        pub tcpi_lost: u32,
        pub tcpi_retrans: u32,
        pub tcpi_fackets: u32,
        pub tcpi_last_data_sent: u32,
        pub tcpi_last_ack_sent: u32,
        pub tcpi_last_data_recv: u32,
        pub tcpi_last_ack_recv: u32,
        pub tcpi_pmtu: u32,
        pub tcpi_rcv_ssthresh: u32,
        pub tcpi_rtt: u32,
        pub tcpi_rttvar: u32,
        pub tcpi_snd_ssthresh: u32,
        pub tcpi_snd_cwnd: u32,
        pub tcpi_advmss: u32,
        pub tcpi_reordering: u32,
        pub tcpi_rcv_rtt: u32,
        pub tcpi_rcv_space: u32,
        pub tcpi_total_retrans: u32,
    }

    pub struct __sched_param {
        __sched_priority: c_int,
    }

    pub struct _pthread_fastlock {
        __status: c_long,
        __spinlock: c_int,
    }

    pub struct pthread_cond_t {
        __c_lock: _pthread_fastlock,
        __c_waiting: _pthread_descr,
        __padding: [u8; PTHREAD_COND_PADDING_SIZE],
        __align: __pthread_cond_align_t,
    }

    pub struct pthread_condattr_t {
        __dummy: c_int,
    }

    pub struct pthread_mutex_t {
        __m_reserved: c_int,
        __m_count: c_int,
        __m_owner: _pthread_descr,
        __m_kind: c_int,
        __m_lock: _pthread_fastlock,
    }

    pub struct pthread_mutexattr_t {
        __mutexkind: c_int,
    }

    pub struct pthread_rwlock_t {
        __rw_lock: _pthread_fastlock,
        __rw_readers: c_int,
        __rw_writer: _pthread_descr,
        __rw_read_waiting: _pthread_descr,
        __rw_write_waiting: _pthread_descr,
        __rw_kind: c_int,
        __rw_pshared: c_int,
    }

    pub struct pthread_rwlockattr_t {
        __lockkind: c_int,
        __pshared: c_int,
    }

    pub struct pthread_barrier_t {
        __ba_lock: _pthread_fastlock,
        __ba_required: c_int,
        __ba_present: c_int,
        __ba_waiting: _pthread_descr,
    }

    pub struct pthread_barrierattr_t {
        __pshared: c_int,
    }
}

impl siginfo_t {
    pub unsafe fn si_addr(&self) -> *mut c_void {
        #[repr(C)]
        struct siginfo_sigfault {
            _si_signo: c_int,
            _si_errno: c_int,
            _si_code: c_int,
            si_addr: *mut c_void,
        }
        (*core::ptr::from_ref(self).cast::<siginfo_sigfault>()).si_addr
    }

    pub unsafe fn si_value(&self) -> crate::sigval {
        #[repr(C)]
        struct siginfo_si_value {
            _si_signo: c_int,
            _si_errno: c_int,
            _si_code: c_int,
            _si_timerid: c_int,
            _si_overrun: c_int,
            si_value: crate::sigval,
        }
        (*core::ptr::from_ref(self).cast::<siginfo_si_value>()).si_value
    }
}

s_no_extra_traits! {
    // Internal, for casts to access union fields
    struct sifields_sigchld {
        si_pid: crate::pid_t,
        si_uid: crate::uid_t,
        si_status: c_int,
        si_utime: c_long,
        si_stime: c_long,
    }

    // Internal, for casts to access union fields
    union sifields {
        _align_pointer: *mut c_void,
        sigchld: sifields_sigchld,
    }

    // Internal, for casts to access union fields. Note that some variants
    // of sifields start with a pointer, which makes the alignment of
    // sifields vary on 32-bit and 64-bit architectures.
    struct siginfo_f {
        _siginfo_base: [c_int; 3],
        sifields: sifields,
    }
}

impl siginfo_t {
    unsafe fn sifields(&self) -> &sifields {
        &(*core::ptr::from_ref(self).cast::<siginfo_f>()).sifields
    }

    pub unsafe fn si_pid(&self) -> crate::pid_t {
        self.sifields().sigchld.si_pid
    }

    pub unsafe fn si_uid(&self) -> crate::uid_t {
        self.sifields().sigchld.si_uid
    }

    pub unsafe fn si_status(&self) -> c_int {
        self.sifields().sigchld.si_status
    }

    pub unsafe fn si_utime(&self) -> c_long {
        self.sifields().sigchld.si_utime
    }

    pub unsafe fn si_stime(&self) -> c_long {
        self.sifields().sigchld.si_stime
    }
}

pub const MCL_CURRENT: c_int = 0x0001;
pub const MCL_FUTURE: c_int = 0x0002;
pub const MCL_ONFAULT: c_int = 0x0004;

pub const SIGEV_THREAD_ID: c_int = 4;

pub const AF_VSOCK: c_int = 40;

pub const POSIX_FADV_DONTNEED: c_int = 4;
pub const POSIX_FADV_NOREUSE: c_int = 5;

// These are different than GNU!
pub const LC_CTYPE: c_int = 0;
pub const LC_NUMERIC: c_int = 1;
pub const LC_TIME: c_int = 3;
pub const LC_COLLATE: c_int = 4;
pub const LC_MONETARY: c_int = 2;
pub const LC_MESSAGES: c_int = 5;
pub const LC_ALL: c_int = 6;
// end different section

// MS_ flags for mount(2)
pub const MS_RMT_MASK: c_ulong =
    crate::MS_RDONLY | crate::MS_SYNCHRONOUS | crate::MS_MANDLOCK | crate::MS_I_VERSION;

pub const ENOTSUP: c_int = EOPNOTSUPP;

pub const IPV6_JOIN_GROUP: c_int = 20;
pub const IPV6_LEAVE_GROUP: c_int = 21;

// Different than Gnu.
pub const FILENAME_MAX: c_uint = 4095;

pub const PRIO_PROCESS: c_int = 0;
pub const PRIO_PGRP: c_int = 1;
pub const PRIO_USER: c_int = 2;

pub const SOMAXCONN: c_int = 128;

pub const ST_RELATIME: c_ulong = 4096;

pub const SO_TIMESTAMP: c_int = 29;

pub const RLIM_INFINITY: crate::rlim_t = !0;

pub const AF_NFC: c_int = PF_NFC;
pub const BUFSIZ: c_int = 256;
pub const EDEADLK: c_int = 0x23;
pub const EDEADLOCK: c_int = EDEADLK;
pub const EXTA: c_uint = B19200;
pub const EXTB: c_uint = B38400;
pub const EXTPROC: crate::tcflag_t = 0o200000;
pub const FOPEN_MAX: c_int = 16;
pub const F_GETOWN: c_int = 9;
pub const F_OFD_GETLK: c_int = 36;
pub const F_OFD_SETLK: c_int = 37;
pub const F_OFD_SETLKW: c_int = 38;
pub const F_RDLCK: c_int = 0;
pub const F_SETOWN: c_int = 8;
pub const F_UNLCK: c_int = 2;
pub const F_WRLCK: c_int = 1;
pub const IPV6_MULTICAST_ALL: c_int = 29;
pub const IPV6_ROUTER_ALERT_ISOLATE: c_int = 30;
pub const MAP_HUGE_SHIFT: c_int = 26;
pub const MAP_HUGE_MASK: c_int = 0x3f;
pub const MSG_COPY: c_int = 0o40000;
pub const NI_MAXHOST: crate::socklen_t = 1025;
pub const O_TMPFILE: c_int = 0o20000000 | O_DIRECTORY;
pub const PACKET_MR_UNICAST: c_int = 3;
pub const PF_NFC: c_int = 39;
pub const PF_VSOCK: c_int = 40;
pub const RTLD_NOLOAD: c_int = 0x00004;
pub const RUSAGE_THREAD: c_int = 1;
pub const SHM_EXEC: c_int = 0o100000;
pub const SOCK_DCCP: c_int = 6;
pub const SOCK_PACKET: c_int = 10;
pub const TCP_COOKIE_TRANSACTIONS: c_int = 15;
pub const UDP_GRO: c_int = 104;
pub const UDP_SEGMENT: c_int = 103;

pub const PTHREAD_RWLOCK_PREFER_READER_NP: c_int = 0;
pub const PTHREAD_RWLOCK_PREFER_WRITER_NP: c_int = 1;
pub const PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP: c_int = 2;
pub const PTHREAD_RWLOCK_DEFAULT_NP: c_int = PTHREAD_RWLOCK_PREFER_WRITER_NP;

pub const PTHREAD_MUTEX_TIMED_NP: c_int = 0;
pub const PTHREAD_MUTEX_RECURSIVE_NP: c_int = 1;
pub const PTHREAD_MUTEX_ERRORCHECK_NP: c_int = 2;
pub const PTHREAD_MUTEX_ADAPTIVE_NP: c_int = 3;

pub const __LT_SPINLOCK_INIT: c_int = 0;

pub const __LOCK_INITIALIZER: _pthread_fastlock = _pthread_fastlock {
    __status: 0,
    __spinlock: __LT_SPINLOCK_INIT,
};

pub const PTHREAD_MUTEX_INITIALIZER: pthread_mutex_t = pthread_mutex_t {
    __m_reserved: 0,
    __m_count: 0,
    __m_owner: core::ptr::null_mut(),
    __m_kind: PTHREAD_MUTEX_TIMED_NP,
    __m_lock: __LOCK_INITIALIZER,
};

const PTHREAD_COND_PADDING_SIZE: usize = 48
    - size_of::<_pthread_fastlock>()
    - size_of::<_pthread_descr>()
    - size_of::<__pthread_cond_align_t>();

pub const PTHREAD_COND_INITIALIZER: pthread_cond_t = pthread_cond_t {
    __c_lock: __LOCK_INITIALIZER,
    __c_waiting: core::ptr::null_mut(),
    __padding: [0; PTHREAD_COND_PADDING_SIZE],
    __align: 0,
};

pub const PTHREAD_RWLOCK_INITIALIZER: pthread_rwlock_t = pthread_rwlock_t {
    __rw_lock: __LOCK_INITIALIZER,
    __rw_readers: 0,
    __rw_writer: core::ptr::null_mut(),
    __rw_read_waiting: core::ptr::null_mut(),
    __rw_write_waiting: core::ptr::null_mut(),
    __rw_kind: PTHREAD_RWLOCK_DEFAULT_NP,
    __rw_pshared: crate::PTHREAD_PROCESS_PRIVATE,
};

extern "C" {
    pub fn gettimeofday(tp: *mut crate::timeval, tz: *mut crate::timezone) -> c_int;

    pub fn pthread_rwlockattr_getkind_np(
        attr: *const crate::pthread_rwlockattr_t,
        val: *mut c_int,
    ) -> c_int;
    pub fn pthread_rwlockattr_setkind_np(
        attr: *mut crate::pthread_rwlockattr_t,
        val: c_int,
    ) -> c_int;

    pub fn openpty(
        amaster: *mut c_int,
        aslave: *mut c_int,
        name: *mut c_char,
        termp: *mut termios,
        winp: *mut crate::winsize,
    ) -> c_int;

    pub fn getnameinfo(
        sa: *const crate::sockaddr,
        salen: crate::socklen_t,
        host: *mut c_char,
        hostlen: crate::socklen_t,
        serv: *mut c_char,
        servlen: crate::socklen_t,
        flags: c_int,
    ) -> c_int;

    pub fn pwritev(fd: c_int, iov: *const crate::iovec, iovcnt: c_int, offset: off64_t) -> ssize_t;
    pub fn preadv(fd: c_int, iov: *const crate::iovec, iovcnt: c_int, offset: off64_t) -> ssize_t;

    pub fn getrlimit64(resource: crate::__rlimit_resource_t, rlim: *mut crate::rlimit64) -> c_int;
    pub fn setrlimit64(resource: crate::__rlimit_resource_t, rlim: *const crate::rlimit64)
        -> c_int;
    pub fn getrlimit(resource: crate::__rlimit_resource_t, rlim: *mut crate::rlimit) -> c_int;
    pub fn setrlimit(resource: crate::__rlimit_resource_t, rlim: *const crate::rlimit) -> c_int;
    pub fn getauxval(type_: c_ulong) -> c_ulong;

}

cfg_if! {
    if #[cfg(target_arch = "x86_64")] {
        mod x86_64;
        pub use self::x86_64::*;
    } else if #[cfg(target_arch = "aarch64")] {
        mod aarch64;
        pub use self::aarch64::*;
    } else {
        // unsupported target
    }
}
