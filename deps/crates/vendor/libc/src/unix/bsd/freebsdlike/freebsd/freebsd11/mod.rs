use crate::prelude::*;

// APIs that were changed after FreeBSD 11

// The type of `nlink_t` changed from `u16` to `u64` in FreeBSD 12:
pub type nlink_t = u16;
// Type of `dev_t` changed from `u32` to `u64` in FreeBSD 12:
pub type dev_t = u32;
// Type of `ino_t` changed from `__uint32_t` to `__uint64_t` in FreeBSD 12:
pub type ino_t = u32;

s! {
    pub struct kevent {
        pub ident: crate::uintptr_t,
        pub filter: c_short,
        pub flags: c_ushort,
        pub fflags: c_uint,
        pub data: intptr_t,
        pub udata: *mut c_void,
    }

    pub struct shmid_ds {
        pub shm_perm: crate::ipc_perm,
        pub shm_segsz: size_t,
        pub shm_lpid: crate::pid_t,
        pub shm_cpid: crate::pid_t,
        // Type of shm_nattc changed from `int` to `shmatt_t` (aka `unsigned
        // int`) in FreeBSD 12:
        pub shm_nattch: c_int,
        pub shm_atime: crate::time_t,
        pub shm_dtime: crate::time_t,
        pub shm_ctime: crate::time_t,
    }

    pub struct kinfo_proc {
        /// Size of this structure.
        pub ki_structsize: c_int,
        /// Reserved: layout identifier.
        pub ki_layout: c_int,
        /// Address of command arguments.
        pub ki_args: *mut crate::pargs,
        // This is normally "struct proc".
        /// Address of proc.
        pub ki_paddr: *mut c_void,
        // This is normally "struct user".
        /// Kernel virtual address of u-area.
        pub ki_addr: *mut c_void,
        // This is normally "struct vnode".
        /// Pointer to trace file.
        pub ki_tracep: *mut c_void,
        // This is normally "struct vnode".
        /// Pointer to executable file.
        pub ki_textvp: *mut c_void,
        /// Pointer to open file info.
        pub ki_fd: *mut crate::filedesc,
        // This is normally "struct vmspace".
        /// Pointer to kernel vmspace struct.
        pub ki_vmspace: *mut c_void,
        /// Sleep address.
        pub ki_wchan: *mut c_void,
        /// Process identifier.
        pub ki_pid: crate::pid_t,
        /// Parent process ID.
        pub ki_ppid: crate::pid_t,
        /// Process group ID.
        pub ki_pgid: crate::pid_t,
        /// tty process group ID.
        pub ki_tpgid: crate::pid_t,
        /// Process session ID.
        pub ki_sid: crate::pid_t,
        /// Terminal session ID.
        pub ki_tsid: crate::pid_t,
        /// Job control counter.
        pub ki_jobc: c_short,
        /// Unused (just here for alignment).
        pub ki_spare_short1: c_short,
        /// Controlling tty dev.
        pub ki_tdev: crate::dev_t,
        /// Signals arrived but not delivered.
        pub ki_siglist: crate::sigset_t,
        /// Current signal mask.
        pub ki_sigmask: crate::sigset_t,
        /// Signals being ignored.
        pub ki_sigignore: crate::sigset_t,
        /// Signals being caught by user.
        pub ki_sigcatch: crate::sigset_t,
        /// Effective user ID.
        pub ki_uid: crate::uid_t,
        /// Real user ID.
        pub ki_ruid: crate::uid_t,
        /// Saved effective user ID.
        pub ki_svuid: crate::uid_t,
        /// Real group ID.
        pub ki_rgid: crate::gid_t,
        /// Saved effective group ID.
        pub ki_svgid: crate::gid_t,
        /// Number of groups.
        pub ki_ngroups: c_short,
        /// Unused (just here for alignment).
        pub ki_spare_short2: c_short,
        /// Groups.
        pub ki_groups: [crate::gid_t; crate::KI_NGROUPS],
        /// Virtual size.
        pub ki_size: crate::vm_size_t,
        /// Current resident set size in pages.
        pub ki_rssize: crate::segsz_t,
        /// Resident set size before last swap.
        pub ki_swrss: crate::segsz_t,
        /// Text size (pages) XXX.
        pub ki_tsize: crate::segsz_t,
        /// Data size (pages) XXX.
        pub ki_dsize: crate::segsz_t,
        /// Stack size (pages).
        pub ki_ssize: crate::segsz_t,
        /// Exit status for wait & stop signal.
        pub ki_xstat: crate::u_short,
        /// Accounting flags.
        pub ki_acflag: crate::u_short,
        /// %cpu for process during `ki_swtime`.
        pub ki_pctcpu: crate::fixpt_t,
        /// Time averaged value of `ki_cpticks`.
        pub ki_estcpu: crate::u_int,
        /// Time since last blocked.
        pub ki_slptime: crate::u_int,
        /// Time swapped in or out.
        pub ki_swtime: crate::u_int,
        /// Number of copy-on-write faults.
        pub ki_cow: crate::u_int,
        /// Real time in microsec.
        pub ki_runtime: u64,
        /// Starting time.
        pub ki_start: crate::timeval,
        /// Time used by process children.
        pub ki_childtime: crate::timeval,
        /// P_* flags.
        pub ki_flag: c_long,
        /// KI_* flags (below).
        pub ki_kiflag: c_long,
        /// Kernel trace points.
        pub ki_traceflag: c_int,
        /// S* process status.
        pub ki_stat: c_char,
        /// Process "nice" value.
        pub ki_nice: i8, // signed char
        /// Process lock (prevent swap) count.
        pub ki_lock: c_char,
        /// Run queue index.
        pub ki_rqindex: c_char,
        /// Which cpu we are on.
        pub ki_oncpu_old: c_uchar,
        /// Last cpu we were on.
        pub ki_lastcpu_old: c_uchar,
        /// Thread name.
        pub ki_tdname: [c_char; crate::TDNAMLEN + 1],
        /// Wchan message.
        pub ki_wmesg: [c_char; crate::WMESGLEN + 1],
        /// Setlogin name.
        pub ki_login: [c_char; crate::LOGNAMELEN + 1],
        /// Lock name.
        pub ki_lockname: [c_char; crate::LOCKNAMELEN + 1],
        /// Command name.
        pub ki_comm: [c_char; crate::COMMLEN + 1],
        /// Emulation name.
        pub ki_emul: [c_char; crate::KI_EMULNAMELEN + 1],
        /// Login class.
        pub ki_loginclass: [c_char; crate::LOGINCLASSLEN + 1],
        /// More thread name.
        pub ki_moretdname: [c_char; crate::MAXCOMLEN - crate::TDNAMLEN + 1],
        /// Spare string space.
        pub ki_sparestrings: [[c_char; 23]; 2], // little hack to allow PartialEq
        /// Spare room for growth.
        pub ki_spareints: [c_int; crate::KI_NSPARE_INT],
        /// Which cpu we are on.
        pub ki_oncpu: c_int,
        /// Last cpu we were on.
        pub ki_lastcpu: c_int,
        /// PID of tracing process.
        pub ki_tracer: c_int,
        /// P2_* flags.
        pub ki_flag2: c_int,
        /// Default FIB number.
        pub ki_fibnum: c_int,
        /// Credential flags.
        pub ki_cr_flags: crate::u_int,
        /// Process jail ID.
        pub ki_jid: c_int,
        /// Number of threads in total.
        pub ki_numthreads: c_int,
        /// Thread ID.
        pub ki_tid: crate::lwpid_t,
        /// Process priority.
        pub ki_pri: crate::priority,
        /// Process rusage statistics.
        pub ki_rusage: crate::rusage,
        /// rusage of children processes.
        pub ki_rusage_ch: crate::rusage,
        // This is normally "struct pcb".
        /// Kernel virtual addr of pcb.
        pub ki_pcb: *mut c_void,
        /// Kernel virtual addr of stack.
        pub ki_kstack: *mut c_void,
        /// User convenience pointer.
        pub ki_udata: *mut c_void,
        // This is normally "struct thread".
        pub ki_tdaddr: *mut c_void,
        pub ki_spareptrs: [*mut c_void; crate::KI_NSPARE_PTR],
        pub ki_sparelongs: [c_long; crate::KI_NSPARE_LONG],
        /// PS_* flags.
        pub ki_sflag: c_long,
        /// kthread flag.
        pub ki_tdflags: c_long,
    }
    pub struct dirent {
        pub d_fileno: crate::ino_t,
        pub d_reclen: u16,
        pub d_type: u8,
        // Type of `d_namlen` changed from `char` to `u16` in FreeBSD 12:
        pub d_namlen: u8,
        pub d_name: [c_char; 256],
    }

    pub struct statfs {
        pub f_version: u32,
        pub f_type: u32,
        pub f_flags: u64,
        pub f_bsize: u64,
        pub f_iosize: u64,
        pub f_blocks: u64,
        pub f_bfree: u64,
        pub f_bavail: i64,
        pub f_files: u64,
        pub f_ffree: i64,
        pub f_syncwrites: u64,
        pub f_asyncwrites: u64,
        pub f_syncreads: u64,
        pub f_asyncreads: u64,
        f_spare: [u64; 10],
        pub f_namemax: u32,
        pub f_owner: crate::uid_t,
        pub f_fsid: crate::fsid_t,
        f_charspare: [c_char; 80],
        pub f_fstypename: [c_char; 16],
        // Array length changed from 88 to 1024 in FreeBSD 12:
        pub f_mntfromname: [c_char; 88],
        // Array length changed from 88 to 1024 in FreeBSD 12:
        pub f_mntonname: [c_char; 88],
    }

    pub struct vnstat {
        pub vn_fileid: u64,
        pub vn_size: u64,
        pub vn_mntdir: *mut c_char,
        pub vn_dev: u32,
        pub vn_fsid: u32,
        pub vn_type: c_int,
        pub vn_mode: u16,
        pub vn_devname: [c_char; crate::SPECNAMELEN as usize + 1],
    }
}

pub const ELAST: c_int = 96;
pub const RAND_MAX: c_int = 0x7fff_fffd;
pub const KI_NSPARE_PTR: usize = 6;
pub const MINCORE_SUPER: c_int = 0x20;
/// max length of devicename
pub const SPECNAMELEN: c_int = 63;

safe_f! {
    pub const fn makedev(major: c_uint, minor: c_uint) -> crate::dev_t {
        let major = major as crate::dev_t;
        let minor = minor as crate::dev_t;
        (major << 8) | minor
    }

    pub const fn major(dev: crate::dev_t) -> c_int {
        ((dev >> 8) & 0xff) as c_int
    }

    pub const fn minor(dev: crate::dev_t) -> c_int {
        (dev & 0xffff00ff) as c_int
    }
}

extern "C" {
    // Return type c_int was removed in FreeBSD 12
    pub fn setgrent() -> c_int;

    // Type of `addr` argument changed from `const void*` to `void*`
    // in FreeBSD 12
    pub fn mprotect(addr: *const c_void, len: size_t, prot: c_int) -> c_int;

    // Return type c_int was removed in FreeBSD 12
    pub fn freelocale(loc: crate::locale_t) -> c_int;

    // Return type c_int changed to ssize_t in FreeBSD 12:
    pub fn msgrcv(
        msqid: c_int,
        msgp: *mut c_void,
        msgsz: size_t,
        msgtyp: c_long,
        msgflg: c_int,
    ) -> c_int;

    // Type of `path` argument changed from `const void*` to `void*`
    // in FreeBSD 12
    pub fn dirname(path: *const c_char) -> *mut c_char;
    pub fn basename(path: *const c_char) -> *mut c_char;

    // Argument order of the function pointer changed in FreeBSD 14. From 14 onwards the signature
    // matches the POSIX specification by having the third argument be a mutable pointer, on
    // earlier versions the first argument is the mutable pointer.
    #[link_name = "qsort_r@FBSD_1.0"]
    pub fn qsort_r(
        base: *mut c_void,
        num: size_t,
        size: size_t,
        arg: *mut c_void,
        compar: Option<unsafe extern "C" fn(*mut c_void, *const c_void, *const c_void) -> c_int>,
    );
}

cfg_if! {
    if #[cfg(target_pointer_width = "64")] {
        mod b64;
        pub use self::b64::*;
    } else {
        mod b32;
        pub use self::b32::*;
    }
}
