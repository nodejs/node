///! L4Re specifics
///! This module contains definitions required by various L4Re libc backends.
///! Some of them are formally not part of the libc, but are a dependency of the
///! libc and hence we should provide them here.
use crate::prelude::*;

pub type l4_umword_t = c_ulong; // Unsigned machine word.
pub type pthread_t = *mut c_void;

pub type dev_t = u64;
pub type socklen_t = u32;
pub type mode_t = u32;
pub type ino64_t = u64;
pub type off64_t = i64;
pub type blkcnt64_t = i64;
pub type rlim64_t = u64;
pub type nfds_t = c_ulong;
pub type nl_item = c_int;
pub type idtype_t = c_uint;
pub type loff_t = c_longlong;
pub type pthread_key_t = c_uint;
pub type pthread_once_t = c_int;
pub type pthread_spinlock_t = c_int;

s! {
    /// CPU sets.
    pub struct l4_sched_cpu_set_t {
        // from the L4Re docs
        /// Combination of granularity and offset.
        ///
        /// The granularity defines how many CPUs each bit in map describes.
        /// The offset is the number of the first CPU described by the first
        /// bit in the bitmap.
        /// offset must be a multiple of 2^graularity.
        ///
        /// | MSB              |                 LSB |
        /// | ---------------- | ------------------- |
        /// | 8bit granularity | 24bit offset ..     |
        gran_offset: l4_umword_t,
        /// Bitmap of CPUs.
        map: l4_umword_t,
    }

    pub struct pthread_attr_t {
        __detachstate: c_int,
        __schedpolicy: c_int,
        __schedparam: super::__sched_param,
        __inheritsched: c_int,
        __scope: c_int,
        __guardsize: size_t,
        __stackaddr_set: c_int,
        __stackaddr: *mut c_void, // better don't use it
        __stacksize: size_t,
        // L4Re specifics
        pub affinity: l4_sched_cpu_set_t,
        pub create_flags: c_uint,
    }
}

// L4Re requires a min stack size of 64k; that isn't defined in uClibc, but
// somewhere in the core libraries. uClibc wants 16k, but that's not enough.
pub const PTHREAD_STACK_MIN: usize = 65536;

pub const BOTHER: crate::speed_t = 0o010000;

pub const RLIMIT_CPU: crate::__rlimit_resource_t = 0;
pub const RLIMIT_FSIZE: crate::__rlimit_resource_t = 1;
pub const RLIMIT_DATA: crate::__rlimit_resource_t = 2;
pub const RLIMIT_STACK: crate::__rlimit_resource_t = 3;
pub const RLIMIT_CORE: crate::__rlimit_resource_t = 4;
pub const RLIMIT_RSS: crate::__rlimit_resource_t = 5;
pub const RLIMIT_NPROC: crate::__rlimit_resource_t = 6;
pub const RLIMIT_NOFILE: crate::__rlimit_resource_t = 7;
pub const RLIMIT_MEMLOCK: crate::__rlimit_resource_t = 8;
pub const RLIMIT_AS: crate::__rlimit_resource_t = 9;
pub const RLIMIT_LOCKS: crate::__rlimit_resource_t = 10;
pub const RLIMIT_SIGPENDING: crate::__rlimit_resource_t = 11;
pub const RLIMIT_MSGQUEUE: crate::__rlimit_resource_t = 12;
pub const RLIMIT_NICE: crate::__rlimit_resource_t = 13;
pub const RLIMIT_RTPRIO: crate::__rlimit_resource_t = 14;
pub const RLIMIT_RTTIME: crate::__rlimit_resource_t = 15;
pub const RLIMIT_NLIMITS: crate::__rlimit_resource_t = RLIM_NLIMITS;
pub const RLIM_NLIMITS: crate::__rlimit_resource_t = 16;

pub const SOL_SOCKET: c_int = 1;

// pub const SO_DEBUG: c_int = 1;
pub const SO_REUSEADDR: c_int = 2;
pub const SO_TYPE: c_int = 3;
pub const SO_ERROR: c_int = 4;
pub const SO_DONTROUTE: c_int = 5;
pub const SO_BROADCAST: c_int = 6;
pub const SO_SNDBUF: c_int = 7;
pub const SO_RCVBUF: c_int = 8;
pub const SO_KEEPALIVE: c_int = 9;
pub const SO_OOBINLINE: c_int = 10;
pub const SO_NO_CHECK: c_int = 11;
pub const SO_PRIORITY: c_int = 12;
pub const SO_LINGER: c_int = 13;
pub const SO_BSDCOMPAT: c_int = 14;
pub const SO_PASSCRED: c_int = 16;
pub const SO_PEERCRED: c_int = 17;
pub const SO_RCVLOWAT: c_int = 18;
pub const SO_SNDLOWAT: c_int = 19;
pub const SO_RCVTIMEO: c_int = 20;
pub const SO_SNDTIMEO: c_int = 21;
pub const SO_SECURITY_AUTHENTICATION: c_int = 22;
pub const SO_SECURITY_ENCRYPTION_TRANSPORT: c_int = 23;
pub const SO_SECURITY_ENCRYPTION_NETWORK: c_int = 24;
pub const SO_BINDTODEVICE: c_int = 25;
pub const SO_ATTACH_FILTER: c_int = 26;
pub const SO_DETACH_FILTER: c_int = 27;
pub const SO_PEERNAME: c_int = 28;

pub const SO_ACCEPTCONN: c_int = 30;
pub const SO_PEERSEC: c_int = 31;

pub const TCGETS: Ioctl = 0x5401;
pub const TCSETS: Ioctl = 0x5402;
pub const TCSETSW: Ioctl = 0x5403;
pub const TCSETSF: Ioctl = 0x5404;
pub const TCGETA: Ioctl = 0x5405;
pub const TCSETA: Ioctl = 0x5406;
pub const TCSETAW: Ioctl = 0x5407;
pub const TCSETAF: Ioctl = 0x5408;
pub const TCSBRK: Ioctl = 0x5409;
pub const TCXONC: Ioctl = 0x540A;
pub const TCFLSH: Ioctl = 0x540B;
pub const TIOCM_LE: c_int = 0x001;
pub const TIOCM_DTR: c_int = 0x002;
pub const TIOCM_RTS: c_int = 0x004;
pub const TIOCM_ST: c_int = 0x008;
pub const TIOCM_SR: c_int = 0x010;
pub const TIOCM_CTS: c_int = 0x020;
pub const TIOCM_CAR: c_int = 0x040;
pub const TIOCM_CD: c_int = TIOCM_CAR;
pub const TIOCM_RNG: c_int = 0x080;
pub const TIOCM_RI: c_int = TIOCM_RNG;
pub const TIOCM_DSR: c_int = 0x100;
pub const TIOCEXCL: Ioctl = 0x540C;
pub const TIOCNXCL: Ioctl = 0x540D;
pub const TIOCSCTTY: Ioctl = 0x540E;
pub const TIOCGPGRP: Ioctl = 0x540F;
pub const TIOCSPGRP: Ioctl = 0x5410;
pub const TIOCOUTQ: Ioctl = 0x5411;
pub const TIOCSTI: Ioctl = 0x5412;
pub const TIOCGWINSZ: Ioctl = 0x5413;
pub const TIOCSWINSZ: Ioctl = 0x5414;
pub const TIOCMGET: Ioctl = 0x5415;
pub const TIOCMBIS: Ioctl = 0x5416;
pub const TIOCMBIC: Ioctl = 0x5417;
pub const TIOCMSET: Ioctl = 0x5418;
pub const TIOCGSOFTCAR: Ioctl = 0x5419;
pub const TIOCSSOFTCAR: Ioctl = 0x541A;
pub const FIONREAD: Ioctl = 0x541B;
pub const TIOCINQ: Ioctl = FIONREAD;
pub const TIOCLINUX: Ioctl = 0x541C;
pub const TIOCCONS: Ioctl = 0x541D;
pub const TIOCGSERIAL: Ioctl = 0x541E;
pub const TIOCSSERIAL: Ioctl = 0x541F;
pub const TIOCPKT: Ioctl = 0x5420;
pub const FIONBIO: Ioctl = 0x5421;
pub const TIOCNOTTY: Ioctl = 0x5422;
pub const TIOCSETD: Ioctl = 0x5423;
pub const TIOCGETD: Ioctl = 0x5424;
pub const TCSBRKP: Ioctl = 0x5425;
pub const TIOCSBRK: Ioctl = 0x5427;
pub const TIOCCBRK: Ioctl = 0x5428;
pub const TIOCGSID: Ioctl = 0x5429;
pub const TIOCGPTN: Ioctl = 0x80045430;
pub const TIOCSPTLCK: Ioctl = 0x40045431;
pub const FIONCLEX: Ioctl = 0x5450;
pub const FIOCLEX: Ioctl = 0x5451;
pub const FIOASYNC: Ioctl = 0x5452;
pub const TIOCSERCONFIG: Ioctl = 0x5453;
pub const TIOCSERGWILD: Ioctl = 0x5454;
pub const TIOCSERSWILD: Ioctl = 0x5455;
pub const TIOCGLCKTRMIOS: Ioctl = 0x5456;
pub const TIOCSLCKTRMIOS: Ioctl = 0x5457;
pub const TIOCSERGSTRUCT: Ioctl = 0x5458;
pub const TIOCSERGETLSR: Ioctl = 0x5459;
pub const TIOCSERGETMULTI: Ioctl = 0x545A;
pub const TIOCSERSETMULTI: Ioctl = 0x545B;
pub const TIOCMIWAIT: Ioctl = 0x545C;
pub const TIOCGICOUNT: Ioctl = 0x545D;

pub const BLKSSZGET: Ioctl = 0x1268;

cfg_if! {
    if #[cfg(not(target_env = "uclibc"))] {
        pub const NLMSG_NOOP: c_int = 0x1;
        pub const NLMSG_ERROR: c_int = 0x2;
        pub const NLMSG_DONE: c_int = 0x3;
        pub const NLMSG_OVERRUN: c_int = 0x4;
        pub const NLMSG_MIN_TYPE: c_int = 0x10;
    }
}

cfg_if! {
    if #[cfg(target_env = "uclibc")] {
        mod uclibc;
        pub use self::uclibc::*;
    }
}
