use crate::prelude::*;

pub type Elf32_Half = u16;
pub type Elf32_Word = u32;
pub type Elf32_Off = u32;
pub type Elf32_Addr = u32;
pub type Elf32_Xword = u64;
pub type Elf32_Sword = i32;

pub type Elf64_Half = u16;
pub type Elf64_Word = u32;
pub type Elf64_Off = u64;
pub type Elf64_Addr = u64;
pub type Elf64_Xword = u64;
pub type Elf64_Sxword = i64;
pub type Elf64_Sword = i32;

pub type Elf32_Section = u16;
pub type Elf64_Section = u16;

pub type Elf32_Relr = Elf32_Word;
pub type Elf64_Relr = Elf32_Xword;
pub type Elf32_Rel = __c_anonymous_elf32_rel;
pub type Elf64_Rel = __c_anonymous_elf64_rel;

cfg_if! {
    if #[cfg(not(target_arch = "sparc64"))] {
        pub type Elf32_Rela = __c_anonymous_elf32_rela;
        pub type Elf64_Rela = __c_anonymous_elf64_rela;
    }
}

pub type iconv_t = *mut c_void;

cfg_if! {
    if #[cfg(not(target_env = "gnu"))] {
        extern_ty! {
            pub enum fpos64_t {} // FIXME(linux): fill this out with a struct
        }
    }
}

s! {
    pub struct glob_t {
        pub gl_pathc: size_t,
        pub gl_pathv: *mut *mut c_char,
        pub gl_offs: size_t,
        pub gl_flags: c_int,

        __unused1: Padding<*mut c_void>,
        __unused2: Padding<*mut c_void>,
        __unused3: Padding<*mut c_void>,
        __unused4: Padding<*mut c_void>,
        __unused5: Padding<*mut c_void>,
    }

    pub struct passwd {
        pub pw_name: *mut c_char,
        pub pw_passwd: *mut c_char,
        pub pw_uid: crate::uid_t,
        pub pw_gid: crate::gid_t,
        pub pw_gecos: *mut c_char,
        pub pw_dir: *mut c_char,
        pub pw_shell: *mut c_char,
    }

    pub struct spwd {
        pub sp_namp: *mut c_char,
        pub sp_pwdp: *mut c_char,
        pub sp_lstchg: c_long,
        pub sp_min: c_long,
        pub sp_max: c_long,
        pub sp_warn: c_long,
        pub sp_inact: c_long,
        pub sp_expire: c_long,
        pub sp_flag: c_ulong,
    }

    pub struct itimerspec {
        pub it_interval: crate::timespec,
        pub it_value: crate::timespec,
    }

    pub struct fsid_t {
        __val: [c_int; 2],
    }

    pub struct packet_mreq {
        pub mr_ifindex: c_int,
        pub mr_type: c_ushort,
        pub mr_alen: c_ushort,
        pub mr_address: [c_uchar; 8],
    }

    pub struct cpu_set_t {
        #[cfg(all(target_pointer_width = "32", not(target_arch = "x86_64")))]
        bits: [u32; 32],
        #[cfg(not(all(target_pointer_width = "32", not(target_arch = "x86_64"))))]
        bits: [u64; 16],
    }

    pub struct sembuf {
        pub sem_num: c_ushort,
        pub sem_op: c_short,
        pub sem_flg: c_short,
    }

    pub struct dl_phdr_info {
        #[cfg(target_pointer_width = "64")]
        pub dlpi_addr: Elf64_Addr,
        #[cfg(target_pointer_width = "32")]
        pub dlpi_addr: Elf32_Addr,

        pub dlpi_name: *const c_char,

        #[cfg(target_pointer_width = "64")]
        pub dlpi_phdr: *const Elf64_Phdr,
        #[cfg(target_pointer_width = "32")]
        pub dlpi_phdr: *const Elf32_Phdr,

        #[cfg(target_pointer_width = "64")]
        pub dlpi_phnum: Elf64_Half,
        #[cfg(target_pointer_width = "32")]
        pub dlpi_phnum: Elf32_Half,

        // As of uClibc 1.0.36, the following fields are
        // gated behind a "#if 0" block which always evaluates
        // to false. So I'm just removing these, and if uClibc changes
        // the #if block in the future to include the following fields, these
        // will probably need including here. tsidea, skrap
        // QNX (NTO) platform does not define these fields
        #[cfg(not(any(target_env = "uclibc", target_os = "nto")))]
        pub dlpi_adds: c_ulonglong,
        #[cfg(not(any(target_env = "uclibc", target_os = "nto")))]
        pub dlpi_subs: c_ulonglong,
        #[cfg(not(any(target_env = "uclibc", target_os = "nto")))]
        pub dlpi_tls_modid: size_t,
        #[cfg(not(any(target_env = "uclibc", target_os = "nto")))]
        pub dlpi_tls_data: *mut c_void,
    }

    pub struct Elf32_Ehdr {
        pub e_ident: [c_uchar; 16],
        pub e_type: Elf32_Half,
        pub e_machine: Elf32_Half,
        pub e_version: Elf32_Word,
        pub e_entry: Elf32_Addr,
        pub e_phoff: Elf32_Off,
        pub e_shoff: Elf32_Off,
        pub e_flags: Elf32_Word,
        pub e_ehsize: Elf32_Half,
        pub e_phentsize: Elf32_Half,
        pub e_phnum: Elf32_Half,
        pub e_shentsize: Elf32_Half,
        pub e_shnum: Elf32_Half,
        pub e_shstrndx: Elf32_Half,
    }

    pub struct Elf64_Ehdr {
        pub e_ident: [c_uchar; 16],
        pub e_type: Elf64_Half,
        pub e_machine: Elf64_Half,
        pub e_version: Elf64_Word,
        pub e_entry: Elf64_Addr,
        pub e_phoff: Elf64_Off,
        pub e_shoff: Elf64_Off,
        pub e_flags: Elf64_Word,
        pub e_ehsize: Elf64_Half,
        pub e_phentsize: Elf64_Half,
        pub e_phnum: Elf64_Half,
        pub e_shentsize: Elf64_Half,
        pub e_shnum: Elf64_Half,
        pub e_shstrndx: Elf64_Half,
    }

    pub struct Elf32_Sym {
        pub st_name: Elf32_Word,
        pub st_value: Elf32_Addr,
        pub st_size: Elf32_Word,
        pub st_info: c_uchar,
        pub st_other: c_uchar,
        pub st_shndx: Elf32_Section,
    }

    pub struct Elf64_Sym {
        pub st_name: Elf64_Word,
        pub st_info: c_uchar,
        pub st_other: c_uchar,
        pub st_shndx: Elf64_Section,
        pub st_value: Elf64_Addr,
        pub st_size: Elf64_Xword,
    }

    pub struct Elf32_Phdr {
        pub p_type: Elf32_Word,
        pub p_offset: Elf32_Off,
        pub p_vaddr: Elf32_Addr,
        pub p_paddr: Elf32_Addr,
        pub p_filesz: Elf32_Word,
        pub p_memsz: Elf32_Word,
        pub p_flags: Elf32_Word,
        pub p_align: Elf32_Word,
    }

    pub struct Elf64_Phdr {
        pub p_type: Elf64_Word,
        pub p_flags: Elf64_Word,
        pub p_offset: Elf64_Off,
        pub p_vaddr: Elf64_Addr,
        pub p_paddr: Elf64_Addr,
        pub p_filesz: Elf64_Xword,
        pub p_memsz: Elf64_Xword,
        pub p_align: Elf64_Xword,
    }

    pub struct Elf32_Shdr {
        pub sh_name: Elf32_Word,
        pub sh_type: Elf32_Word,
        pub sh_flags: Elf32_Word,
        pub sh_addr: Elf32_Addr,
        pub sh_offset: Elf32_Off,
        pub sh_size: Elf32_Word,
        pub sh_link: Elf32_Word,
        pub sh_info: Elf32_Word,
        pub sh_addralign: Elf32_Word,
        pub sh_entsize: Elf32_Word,
    }

    pub struct Elf64_Shdr {
        pub sh_name: Elf64_Word,
        pub sh_type: Elf64_Word,
        pub sh_flags: Elf64_Xword,
        pub sh_addr: Elf64_Addr,
        pub sh_offset: Elf64_Off,
        pub sh_size: Elf64_Xword,
        pub sh_link: Elf64_Word,
        pub sh_info: Elf64_Word,
        pub sh_addralign: Elf64_Xword,
        pub sh_entsize: Elf64_Xword,
    }

    pub struct __c_anonymous_elf32_rel {
        pub r_offset: Elf32_Addr,
        pub r_info: Elf32_Word,
    }

    pub struct __c_anonymous_elf64_rel {
        pub r_offset: Elf64_Addr,
        pub r_info: Elf64_Xword,
    }

    pub struct ucred {
        pub pid: crate::pid_t,
        pub uid: crate::uid_t,
        pub gid: crate::gid_t,
    }

    pub struct mntent {
        pub mnt_fsname: *mut c_char,
        pub mnt_dir: *mut c_char,
        pub mnt_type: *mut c_char,
        pub mnt_opts: *mut c_char,
        pub mnt_freq: c_int,
        pub mnt_passno: c_int,
    }

    pub struct in6_pktinfo {
        pub ipi6_addr: crate::in6_addr,
        pub ipi6_ifindex: c_uint,
    }

    pub struct arpd_request {
        pub req: c_ushort,
        pub ip: u32,
        pub dev: c_ulong,
        pub stamp: c_ulong,
        pub updated: c_ulong,
        pub ha: [c_uchar; crate::MAX_ADDR_LEN],
    }

    pub struct regmatch_t {
        pub rm_so: crate::regoff_t,
        pub rm_eo: crate::regoff_t,
    }

    pub struct option {
        pub name: *const c_char,
        pub has_arg: c_int,
        pub flag: *mut c_int,
        pub val: c_int,
    }

    pub struct rlimit64 {
        pub rlim_cur: crate::rlim64_t,
        pub rlim_max: crate::rlim64_t,
    }

    pub struct __c_anonymous_ifru_map {
        pub mem_start: c_ulong,
        pub mem_end: c_ulong,
        pub base_addr: c_ushort,
        pub irq: c_uchar,
        pub dma: c_uchar,
        pub port: c_uchar,
    }

    pub struct dirent {
        pub d_ino: crate::ino_t,
        pub d_off: crate::off_t,
        pub d_reclen: c_ushort,
        pub d_type: c_uchar,
        pub d_name: [c_char; 256],
    }

    pub struct dirent64 {
        pub d_ino: crate::ino64_t,
        pub d_off: crate::off64_t,
        pub d_reclen: c_ushort,
        pub d_type: c_uchar,
        pub d_name: [c_char; 256],
    }
}

cfg_if! {
    if #[cfg(not(target_arch = "sparc64"))] {
        s! {
            pub struct __c_anonymous_elf32_rela {
                pub r_offset: crate::Elf32_Addr,
                pub r_info: crate::Elf32_Word,
                pub r_addend: crate::Elf32_Sword,
            }

            pub struct __c_anonymous_elf64_rela {
                pub r_offset: crate::Elf64_Addr,
                pub r_info: crate::Elf64_Xword,
                pub r_addend: crate::Elf64_Sxword,
            }
        }
    }
}

s_no_extra_traits! {
    pub union __c_anonymous_ifr_ifru {
        pub ifru_addr: crate::sockaddr,
        pub ifru_dstaddr: crate::sockaddr,
        pub ifru_broadaddr: crate::sockaddr,
        pub ifru_netmask: crate::sockaddr,
        pub ifru_hwaddr: crate::sockaddr,
        pub ifru_flags: c_short,
        pub ifru_ifindex: c_int,
        pub ifru_metric: c_int,
        pub ifru_mtu: c_int,
        pub ifru_map: __c_anonymous_ifru_map,
        pub ifru_slave: [c_char; crate::IFNAMSIZ],
        pub ifru_newname: [c_char; crate::IFNAMSIZ],
        pub ifru_data: *mut c_char,
    }

    pub struct ifreq {
        /// interface name, e.g. "en0"
        pub ifr_name: [c_char; crate::IFNAMSIZ],
        pub ifr_ifru: __c_anonymous_ifr_ifru,
    }

    pub union __c_anonymous_ifc_ifcu {
        pub ifcu_buf: *mut c_char,
        pub ifcu_req: *mut crate::ifreq,
    }

    /// Structure used in SIOCGIFCONF request.  Used to retrieve interface configuration for
    /// machine (useful for programs which must know all networks accessible).
    pub struct ifconf {
        /// Size of buffer
        pub ifc_len: c_int,
        pub ifc_ifcu: __c_anonymous_ifc_ifcu,
    }
}

cfg_if! {
    if #[cfg(not(target_env = "uclibc"))] {
        const DATE_BASE: crate::nl_item = 0x20000;
    } else {
        const DATE_BASE: crate::nl_item = 768;
    }
}

pub const ABDAY_1: crate::nl_item = DATE_BASE;
pub const ABDAY_2: crate::nl_item = DATE_BASE + 0x1;
pub const ABDAY_3: crate::nl_item = DATE_BASE + 0x2;
pub const ABDAY_4: crate::nl_item = DATE_BASE + 0x3;
pub const ABDAY_5: crate::nl_item = DATE_BASE + 0x4;
pub const ABDAY_6: crate::nl_item = DATE_BASE + 0x5;
pub const ABDAY_7: crate::nl_item = DATE_BASE + 0x6;

pub const DAY_1: crate::nl_item = DATE_BASE + 0x7;
pub const DAY_2: crate::nl_item = DATE_BASE + 0x8;
pub const DAY_3: crate::nl_item = DATE_BASE + 0x9;
pub const DAY_4: crate::nl_item = DATE_BASE + 0xA;
pub const DAY_5: crate::nl_item = DATE_BASE + 0xB;
pub const DAY_6: crate::nl_item = DATE_BASE + 0xC;
pub const DAY_7: crate::nl_item = DATE_BASE + 0xD;

pub const ABMON_1: crate::nl_item = DATE_BASE + 0xE;
pub const ABMON_2: crate::nl_item = DATE_BASE + 0xF;
pub const ABMON_3: crate::nl_item = DATE_BASE + 0x10;
pub const ABMON_4: crate::nl_item = DATE_BASE + 0x11;
pub const ABMON_5: crate::nl_item = DATE_BASE + 0x12;
pub const ABMON_6: crate::nl_item = DATE_BASE + 0x13;
pub const ABMON_7: crate::nl_item = DATE_BASE + 0x14;
pub const ABMON_8: crate::nl_item = DATE_BASE + 0x15;
pub const ABMON_9: crate::nl_item = DATE_BASE + 0x16;
pub const ABMON_10: crate::nl_item = DATE_BASE + 0x17;
pub const ABMON_11: crate::nl_item = DATE_BASE + 0x18;
pub const ABMON_12: crate::nl_item = DATE_BASE + 0x19;

pub const MON_1: crate::nl_item = DATE_BASE + 0x1A;
pub const MON_2: crate::nl_item = DATE_BASE + 0x1B;
pub const MON_3: crate::nl_item = DATE_BASE + 0x1C;
pub const MON_4: crate::nl_item = DATE_BASE + 0x1D;
pub const MON_5: crate::nl_item = DATE_BASE + 0x1E;
pub const MON_6: crate::nl_item = DATE_BASE + 0x1F;
pub const MON_7: crate::nl_item = DATE_BASE + 0x20;
pub const MON_8: crate::nl_item = DATE_BASE + 0x21;
pub const MON_9: crate::nl_item = DATE_BASE + 0x22;
pub const MON_10: crate::nl_item = DATE_BASE + 0x23;
pub const MON_11: crate::nl_item = DATE_BASE + 0x24;
pub const MON_12: crate::nl_item = DATE_BASE + 0x25;

pub const AM_STR: crate::nl_item = DATE_BASE + 0x26;
pub const PM_STR: crate::nl_item = DATE_BASE + 0x27;

pub const D_T_FMT: crate::nl_item = DATE_BASE + 0x28;
pub const D_FMT: crate::nl_item = DATE_BASE + 0x29;
pub const T_FMT: crate::nl_item = DATE_BASE + 0x2A;
pub const T_FMT_AMPM: crate::nl_item = DATE_BASE + 0x2B;

pub const ERA: crate::nl_item = DATE_BASE + 0x2C;
pub const ERA_D_FMT: crate::nl_item = DATE_BASE + 0x2E;
pub const ALT_DIGITS: crate::nl_item = DATE_BASE + 0x2F;
pub const ERA_D_T_FMT: crate::nl_item = DATE_BASE + 0x30;
pub const ERA_T_FMT: crate::nl_item = DATE_BASE + 0x31;

cfg_if! {
    if #[cfg(any(
        target_env = "gnu",
        target_env = "musl",
        target_env = "ohos"
    ))] {
        pub const CODESET: crate::nl_item = 14;
        pub const CRNCYSTR: crate::nl_item = 0x4000F;
        pub const RADIXCHAR: crate::nl_item = 0x10000;
        pub const THOUSEP: crate::nl_item = 0x10001;
        pub const YESEXPR: crate::nl_item = 0x50000;
        pub const NOEXPR: crate::nl_item = 0x50001;
        pub const YESSTR: crate::nl_item = 0x50002;
        pub const NOSTR: crate::nl_item = 0x50003;
    } else if #[cfg(target_env = "uclibc")] {
        pub const CODESET: crate::nl_item = 10;
        pub const CRNCYSTR: crate::nl_item = 0x215;
        pub const RADIXCHAR: crate::nl_item = 0x100;
        pub const THOUSEP: crate::nl_item = 0x101;
        pub const YESEXPR: crate::nl_item = 0x500;
        pub const NOEXPR: crate::nl_item = 0x501;
        pub const YESSTR: crate::nl_item = 0x502;
        pub const NOSTR: crate::nl_item = 0x503;
    }
}

pub const RUSAGE_CHILDREN: c_int = -1;

pub const L_tmpnam: c_uint = 20;
pub const _PC_LINK_MAX: c_int = 0;
pub const _PC_MAX_CANON: c_int = 1;
pub const _PC_MAX_INPUT: c_int = 2;
pub const _PC_NAME_MAX: c_int = 3;
pub const _PC_PATH_MAX: c_int = 4;
pub const _PC_PIPE_BUF: c_int = 5;
pub const _PC_CHOWN_RESTRICTED: c_int = 6;
pub const _PC_NO_TRUNC: c_int = 7;
pub const _PC_VDISABLE: c_int = 8;
pub const _PC_SYNC_IO: c_int = 9;
pub const _PC_ASYNC_IO: c_int = 10;
pub const _PC_PRIO_IO: c_int = 11;
pub const _PC_SOCK_MAXBUF: c_int = 12;
pub const _PC_FILESIZEBITS: c_int = 13;
pub const _PC_REC_INCR_XFER_SIZE: c_int = 14;
pub const _PC_REC_MAX_XFER_SIZE: c_int = 15;
pub const _PC_REC_MIN_XFER_SIZE: c_int = 16;
pub const _PC_REC_XFER_ALIGN: c_int = 17;
pub const _PC_ALLOC_SIZE_MIN: c_int = 18;
pub const _PC_SYMLINK_MAX: c_int = 19;
pub const _PC_2_SYMLINKS: c_int = 20;

pub const _SC_ARG_MAX: c_int = 0;
pub const _SC_CHILD_MAX: c_int = 1;
pub const _SC_CLK_TCK: c_int = 2;
pub const _SC_NGROUPS_MAX: c_int = 3;
pub const _SC_OPEN_MAX: c_int = 4;
pub const _SC_STREAM_MAX: c_int = 5;
pub const _SC_TZNAME_MAX: c_int = 6;
pub const _SC_JOB_CONTROL: c_int = 7;
pub const _SC_SAVED_IDS: c_int = 8;
pub const _SC_REALTIME_SIGNALS: c_int = 9;
pub const _SC_PRIORITY_SCHEDULING: c_int = 10;
pub const _SC_TIMERS: c_int = 11;
pub const _SC_ASYNCHRONOUS_IO: c_int = 12;
pub const _SC_PRIORITIZED_IO: c_int = 13;
pub const _SC_SYNCHRONIZED_IO: c_int = 14;
pub const _SC_FSYNC: c_int = 15;
pub const _SC_MAPPED_FILES: c_int = 16;
pub const _SC_MEMLOCK: c_int = 17;
pub const _SC_MEMLOCK_RANGE: c_int = 18;
pub const _SC_MEMORY_PROTECTION: c_int = 19;
pub const _SC_MESSAGE_PASSING: c_int = 20;
pub const _SC_SEMAPHORES: c_int = 21;
pub const _SC_SHARED_MEMORY_OBJECTS: c_int = 22;
pub const _SC_AIO_LISTIO_MAX: c_int = 23;
pub const _SC_AIO_MAX: c_int = 24;
pub const _SC_AIO_PRIO_DELTA_MAX: c_int = 25;
pub const _SC_DELAYTIMER_MAX: c_int = 26;
pub const _SC_MQ_OPEN_MAX: c_int = 27;
pub const _SC_MQ_PRIO_MAX: c_int = 28;
pub const _SC_VERSION: c_int = 29;
pub const _SC_PAGESIZE: c_int = 30;
pub const _SC_PAGE_SIZE: c_int = _SC_PAGESIZE;
pub const _SC_RTSIG_MAX: c_int = 31;
pub const _SC_SEM_NSEMS_MAX: c_int = 32;
pub const _SC_SEM_VALUE_MAX: c_int = 33;
pub const _SC_SIGQUEUE_MAX: c_int = 34;
pub const _SC_TIMER_MAX: c_int = 35;
pub const _SC_BC_BASE_MAX: c_int = 36;
pub const _SC_BC_DIM_MAX: c_int = 37;
pub const _SC_BC_SCALE_MAX: c_int = 38;
pub const _SC_BC_STRING_MAX: c_int = 39;
pub const _SC_COLL_WEIGHTS_MAX: c_int = 40;
pub const _SC_EXPR_NEST_MAX: c_int = 42;
pub const _SC_LINE_MAX: c_int = 43;
pub const _SC_RE_DUP_MAX: c_int = 44;
pub const _SC_2_VERSION: c_int = 46;
pub const _SC_2_C_BIND: c_int = 47;
pub const _SC_2_C_DEV: c_int = 48;
pub const _SC_2_FORT_DEV: c_int = 49;
pub const _SC_2_FORT_RUN: c_int = 50;
pub const _SC_2_SW_DEV: c_int = 51;
pub const _SC_2_LOCALEDEF: c_int = 52;
pub const _SC_UIO_MAXIOV: c_int = 60;
pub const _SC_IOV_MAX: c_int = 60;
pub const _SC_THREADS: c_int = 67;
pub const _SC_THREAD_SAFE_FUNCTIONS: c_int = 68;
pub const _SC_GETGR_R_SIZE_MAX: c_int = 69;
pub const _SC_GETPW_R_SIZE_MAX: c_int = 70;
pub const _SC_LOGIN_NAME_MAX: c_int = 71;
pub const _SC_TTY_NAME_MAX: c_int = 72;
pub const _SC_THREAD_DESTRUCTOR_ITERATIONS: c_int = 73;
pub const _SC_THREAD_KEYS_MAX: c_int = 74;
pub const _SC_THREAD_STACK_MIN: c_int = 75;
pub const _SC_THREAD_THREADS_MAX: c_int = 76;
pub const _SC_THREAD_ATTR_STACKADDR: c_int = 77;
pub const _SC_THREAD_ATTR_STACKSIZE: c_int = 78;
pub const _SC_THREAD_PRIORITY_SCHEDULING: c_int = 79;
pub const _SC_THREAD_PRIO_INHERIT: c_int = 80;
pub const _SC_THREAD_PRIO_PROTECT: c_int = 81;
pub const _SC_THREAD_PROCESS_SHARED: c_int = 82;
pub const _SC_NPROCESSORS_CONF: c_int = 83;
pub const _SC_NPROCESSORS_ONLN: c_int = 84;
pub const _SC_PHYS_PAGES: c_int = 85;
pub const _SC_AVPHYS_PAGES: c_int = 86;
pub const _SC_ATEXIT_MAX: c_int = 87;
pub const _SC_PASS_MAX: c_int = 88;
pub const _SC_XOPEN_VERSION: c_int = 89;
pub const _SC_XOPEN_XCU_VERSION: c_int = 90;
pub const _SC_XOPEN_UNIX: c_int = 91;
pub const _SC_XOPEN_CRYPT: c_int = 92;
pub const _SC_XOPEN_ENH_I18N: c_int = 93;
pub const _SC_XOPEN_SHM: c_int = 94;
pub const _SC_2_CHAR_TERM: c_int = 95;
pub const _SC_2_UPE: c_int = 97;
pub const _SC_XOPEN_XPG2: c_int = 98;
pub const _SC_XOPEN_XPG3: c_int = 99;
pub const _SC_XOPEN_XPG4: c_int = 100;
pub const _SC_NZERO: c_int = 109;
pub const _SC_XBS5_ILP32_OFF32: c_int = 125;
pub const _SC_XBS5_ILP32_OFFBIG: c_int = 126;
pub const _SC_XBS5_LP64_OFF64: c_int = 127;
pub const _SC_XBS5_LPBIG_OFFBIG: c_int = 128;
pub const _SC_XOPEN_LEGACY: c_int = 129;
pub const _SC_XOPEN_REALTIME: c_int = 130;
pub const _SC_XOPEN_REALTIME_THREADS: c_int = 131;
pub const _SC_ADVISORY_INFO: c_int = 132;
pub const _SC_BARRIERS: c_int = 133;
pub const _SC_CLOCK_SELECTION: c_int = 137;
pub const _SC_CPUTIME: c_int = 138;
pub const _SC_THREAD_CPUTIME: c_int = 139;
pub const _SC_MONOTONIC_CLOCK: c_int = 149;
pub const _SC_READER_WRITER_LOCKS: c_int = 153;
pub const _SC_SPIN_LOCKS: c_int = 154;
pub const _SC_REGEXP: c_int = 155;
pub const _SC_SHELL: c_int = 157;
pub const _SC_SPAWN: c_int = 159;
pub const _SC_SPORADIC_SERVER: c_int = 160;
pub const _SC_THREAD_SPORADIC_SERVER: c_int = 161;
pub const _SC_TIMEOUTS: c_int = 164;
pub const _SC_TYPED_MEMORY_OBJECTS: c_int = 165;
pub const _SC_2_PBS: c_int = 168;
pub const _SC_2_PBS_ACCOUNTING: c_int = 169;
pub const _SC_2_PBS_LOCATE: c_int = 170;
pub const _SC_2_PBS_MESSAGE: c_int = 171;
pub const _SC_2_PBS_TRACK: c_int = 172;
pub const _SC_SYMLOOP_MAX: c_int = 173;
pub const _SC_STREAMS: c_int = 174;
pub const _SC_2_PBS_CHECKPOINT: c_int = 175;
pub const _SC_V6_ILP32_OFF32: c_int = 176;
pub const _SC_V6_ILP32_OFFBIG: c_int = 177;
pub const _SC_V6_LP64_OFF64: c_int = 178;
pub const _SC_V6_LPBIG_OFFBIG: c_int = 179;
pub const _SC_HOST_NAME_MAX: c_int = 180;
pub const _SC_TRACE: c_int = 181;
pub const _SC_TRACE_EVENT_FILTER: c_int = 182;
pub const _SC_TRACE_INHERIT: c_int = 183;
pub const _SC_TRACE_LOG: c_int = 184;
pub const _SC_IPV6: c_int = 235;
pub const _SC_RAW_SOCKETS: c_int = 236;
pub const _SC_V7_ILP32_OFF32: c_int = 237;
pub const _SC_V7_ILP32_OFFBIG: c_int = 238;
pub const _SC_V7_LP64_OFF64: c_int = 239;
pub const _SC_V7_LPBIG_OFFBIG: c_int = 240;
pub const _SC_SS_REPL_MAX: c_int = 241;
pub const _SC_TRACE_EVENT_NAME_MAX: c_int = 242;
pub const _SC_TRACE_NAME_MAX: c_int = 243;
pub const _SC_TRACE_SYS_MAX: c_int = 244;
pub const _SC_TRACE_USER_EVENT_MAX: c_int = 245;
pub const _SC_XOPEN_STREAMS: c_int = 246;
pub const _SC_THREAD_ROBUST_PRIO_INHERIT: c_int = 247;
pub const _SC_THREAD_ROBUST_PRIO_PROTECT: c_int = 248;

pub const _CS_PATH: c_int = 0;
pub const _CS_POSIX_V6_WIDTH_RESTRICTED_ENVS: c_int = 1;
pub const _CS_POSIX_V5_WIDTH_RESTRICTED_ENVS: c_int = 4;
pub const _CS_POSIX_V7_WIDTH_RESTRICTED_ENVS: c_int = 5;
pub const _CS_POSIX_V6_ILP32_OFF32_CFLAGS: c_int = 1116;
pub const _CS_POSIX_V6_ILP32_OFF32_LDFLAGS: c_int = 1117;
pub const _CS_POSIX_V6_ILP32_OFF32_LIBS: c_int = 1118;
pub const _CS_POSIX_V6_ILP32_OFF32_LINTFLAGS: c_int = 1119;
pub const _CS_POSIX_V6_ILP32_OFFBIG_CFLAGS: c_int = 1120;
pub const _CS_POSIX_V6_ILP32_OFFBIG_LDFLAGS: c_int = 1121;
pub const _CS_POSIX_V6_ILP32_OFFBIG_LIBS: c_int = 1122;
pub const _CS_POSIX_V6_ILP32_OFFBIG_LINTFLAGS: c_int = 1123;
pub const _CS_POSIX_V6_LP64_OFF64_CFLAGS: c_int = 1124;
pub const _CS_POSIX_V6_LP64_OFF64_LDFLAGS: c_int = 1125;
pub const _CS_POSIX_V6_LP64_OFF64_LIBS: c_int = 1126;
pub const _CS_POSIX_V6_LP64_OFF64_LINTFLAGS: c_int = 1127;
pub const _CS_POSIX_V6_LPBIG_OFFBIG_CFLAGS: c_int = 1128;
pub const _CS_POSIX_V6_LPBIG_OFFBIG_LDFLAGS: c_int = 1129;
pub const _CS_POSIX_V6_LPBIG_OFFBIG_LIBS: c_int = 1130;
pub const _CS_POSIX_V6_LPBIG_OFFBIG_LINTFLAGS: c_int = 1131;
pub const _CS_POSIX_V7_ILP32_OFF32_CFLAGS: c_int = 1132;
pub const _CS_POSIX_V7_ILP32_OFF32_LDFLAGS: c_int = 1133;
pub const _CS_POSIX_V7_ILP32_OFF32_LIBS: c_int = 1134;
pub const _CS_POSIX_V7_ILP32_OFF32_LINTFLAGS: c_int = 1135;
pub const _CS_POSIX_V7_ILP32_OFFBIG_CFLAGS: c_int = 1136;
pub const _CS_POSIX_V7_ILP32_OFFBIG_LDFLAGS: c_int = 1137;
pub const _CS_POSIX_V7_ILP32_OFFBIG_LIBS: c_int = 1138;
pub const _CS_POSIX_V7_ILP32_OFFBIG_LINTFLAGS: c_int = 1139;
pub const _CS_POSIX_V7_LP64_OFF64_CFLAGS: c_int = 1140;
pub const _CS_POSIX_V7_LP64_OFF64_LDFLAGS: c_int = 1141;
pub const _CS_POSIX_V7_LP64_OFF64_LIBS: c_int = 1142;
pub const _CS_POSIX_V7_LP64_OFF64_LINTFLAGS: c_int = 1143;
pub const _CS_POSIX_V7_LPBIG_OFFBIG_CFLAGS: c_int = 1144;
pub const _CS_POSIX_V7_LPBIG_OFFBIG_LDFLAGS: c_int = 1145;
pub const _CS_POSIX_V7_LPBIG_OFFBIG_LIBS: c_int = 1146;
pub const _CS_POSIX_V7_LPBIG_OFFBIG_LINTFLAGS: c_int = 1147;

pub const RLIM_SAVED_MAX: crate::rlim_t = crate::RLIM_INFINITY;
pub const RLIM_SAVED_CUR: crate::rlim_t = crate::RLIM_INFINITY;

// elf.h - Fields in the e_ident array.
pub const EI_NIDENT: usize = 16;

pub const EI_MAG0: usize = 0;
pub const ELFMAG0: u8 = 0x7f;
pub const EI_MAG1: usize = 1;
pub const ELFMAG1: u8 = b'E';
pub const EI_MAG2: usize = 2;
pub const ELFMAG2: u8 = b'L';
pub const EI_MAG3: usize = 3;
pub const ELFMAG3: u8 = b'F';
pub const SELFMAG: usize = 4;

pub const EI_CLASS: usize = 4;
pub const ELFCLASSNONE: u8 = 0;
pub const ELFCLASS32: u8 = 1;
pub const ELFCLASS64: u8 = 2;
pub const ELFCLASSNUM: usize = 3;

pub const EI_DATA: usize = 5;
pub const ELFDATANONE: u8 = 0;
pub const ELFDATA2LSB: u8 = 1;
pub const ELFDATA2MSB: u8 = 2;
pub const ELFDATANUM: usize = 3;

pub const EI_VERSION: usize = 6;

pub const EI_OSABI: usize = 7;
pub const ELFOSABI_NONE: u8 = 0;
pub const ELFOSABI_SYSV: u8 = 0;
pub const ELFOSABI_HPUX: u8 = 1;
pub const ELFOSABI_NETBSD: u8 = 2;
pub const ELFOSABI_GNU: u8 = 3;
pub const ELFOSABI_LINUX: u8 = ELFOSABI_GNU;
pub const ELFOSABI_SOLARIS: u8 = 6;
pub const ELFOSABI_AIX: u8 = 7;
pub const ELFOSABI_IRIX: u8 = 8;
pub const ELFOSABI_FREEBSD: u8 = 9;
pub const ELFOSABI_TRU64: u8 = 10;
pub const ELFOSABI_MODESTO: u8 = 11;
pub const ELFOSABI_OPENBSD: u8 = 12;
pub const ELFOSABI_ARM: u8 = 97;
pub const ELFOSABI_STANDALONE: u8 = 255;

pub const EI_ABIVERSION: usize = 8;

pub const EI_PAD: usize = 9;

// elf.h - Legal values for e_type (object file type).
pub const ET_NONE: u16 = 0;
pub const ET_REL: u16 = 1;
pub const ET_EXEC: u16 = 2;
pub const ET_DYN: u16 = 3;
pub const ET_CORE: u16 = 4;
pub const ET_NUM: u16 = 5;
pub const ET_LOOS: u16 = 0xfe00;
pub const ET_HIOS: u16 = 0xfeff;
pub const ET_LOPROC: u16 = 0xff00;
pub const ET_HIPROC: u16 = 0xffff;

// elf.h - Legal values for e_machine (architecture).
pub const EM_NONE: u16 = 0;
pub const EM_M32: u16 = 1;
pub const EM_SPARC: u16 = 2;
pub const EM_386: u16 = 3;
pub const EM_68K: u16 = 4;
pub const EM_88K: u16 = 5;
pub const EM_860: u16 = 7;
pub const EM_MIPS: u16 = 8;
pub const EM_S370: u16 = 9;
pub const EM_MIPS_RS3_LE: u16 = 10;
pub const EM_PARISC: u16 = 15;
pub const EM_VPP500: u16 = 17;
pub const EM_SPARC32PLUS: u16 = 18;
pub const EM_960: u16 = 19;
pub const EM_PPC: u16 = 20;
pub const EM_PPC64: u16 = 21;
pub const EM_S390: u16 = 22;
pub const EM_V800: u16 = 36;
pub const EM_FR20: u16 = 37;
pub const EM_RH32: u16 = 38;
pub const EM_RCE: u16 = 39;
pub const EM_ARM: u16 = 40;
pub const EM_FAKE_ALPHA: u16 = 41;
pub const EM_SH: u16 = 42;
pub const EM_SPARCV9: u16 = 43;
pub const EM_TRICORE: u16 = 44;
pub const EM_ARC: u16 = 45;
pub const EM_H8_300: u16 = 46;
pub const EM_H8_300H: u16 = 47;
pub const EM_H8S: u16 = 48;
pub const EM_H8_500: u16 = 49;
pub const EM_IA_64: u16 = 50;
pub const EM_MIPS_X: u16 = 51;
pub const EM_COLDFIRE: u16 = 52;
pub const EM_68HC12: u16 = 53;
pub const EM_MMA: u16 = 54;
pub const EM_PCP: u16 = 55;
pub const EM_NCPU: u16 = 56;
pub const EM_NDR1: u16 = 57;
pub const EM_STARCORE: u16 = 58;
pub const EM_ME16: u16 = 59;
pub const EM_ST100: u16 = 60;
pub const EM_TINYJ: u16 = 61;
pub const EM_X86_64: u16 = 62;
pub const EM_PDSP: u16 = 63;
pub const EM_FX66: u16 = 66;
pub const EM_ST9PLUS: u16 = 67;
pub const EM_ST7: u16 = 68;
pub const EM_68HC16: u16 = 69;
pub const EM_68HC11: u16 = 70;
pub const EM_68HC08: u16 = 71;
pub const EM_68HC05: u16 = 72;
pub const EM_SVX: u16 = 73;
pub const EM_ST19: u16 = 74;
pub const EM_VAX: u16 = 75;
pub const EM_CRIS: u16 = 76;
pub const EM_JAVELIN: u16 = 77;
pub const EM_FIREPATH: u16 = 78;
pub const EM_ZSP: u16 = 79;
pub const EM_MMIX: u16 = 80;
pub const EM_HUANY: u16 = 81;
pub const EM_PRISM: u16 = 82;
pub const EM_AVR: u16 = 83;
pub const EM_FR30: u16 = 84;
pub const EM_D10V: u16 = 85;
pub const EM_D30V: u16 = 86;
pub const EM_V850: u16 = 87;
pub const EM_M32R: u16 = 88;
pub const EM_MN10300: u16 = 89;
pub const EM_MN10200: u16 = 90;
pub const EM_PJ: u16 = 91;
#[cfg(not(target_env = "uclibc"))]
pub const EM_OPENRISC: u16 = 92;
#[cfg(target_env = "uclibc")]
pub const EM_OR1K: u16 = 92;
#[cfg(not(target_env = "uclibc"))]
pub const EM_ARC_A5: u16 = 93;
pub const EM_XTENSA: u16 = 94;
pub const EM_AARCH64: u16 = 183;
pub const EM_TILEPRO: u16 = 188;
pub const EM_TILEGX: u16 = 191;
pub const EM_RISCV: u16 = 243;
pub const EM_ALPHA: u16 = 0x9026;

// elf.h - Legal values for e_version (version).
pub const EV_NONE: u32 = 0;
pub const EV_CURRENT: u32 = 1;
pub const EV_NUM: u32 = 2;

// elf.h - Legal values for p_type (segment type).
pub const PT_NULL: u32 = 0;
pub const PT_LOAD: u32 = 1;
pub const PT_DYNAMIC: u32 = 2;
pub const PT_INTERP: u32 = 3;
pub const PT_NOTE: u32 = 4;
pub const PT_SHLIB: u32 = 5;
pub const PT_PHDR: u32 = 6;
pub const PT_TLS: u32 = 7;
pub const PT_NUM: u32 = 8;
pub const PT_LOOS: u32 = 0x60000000;
pub const PT_GNU_EH_FRAME: u32 = 0x6474e550;
pub const PT_GNU_STACK: u32 = 0x6474e551;
pub const PT_GNU_RELRO: u32 = 0x6474e552;
pub const PT_LOSUNW: u32 = 0x6ffffffa;
pub const PT_SUNWBSS: u32 = 0x6ffffffa;
pub const PT_SUNWSTACK: u32 = 0x6ffffffb;
pub const PT_HISUNW: u32 = 0x6fffffff;
pub const PT_HIOS: u32 = 0x6fffffff;
pub const PT_LOPROC: u32 = 0x70000000;
pub const PT_HIPROC: u32 = 0x7fffffff;

// Legal values for p_flags (segment flags).
pub const PF_X: u32 = 1 << 0;
pub const PF_W: u32 = 1 << 1;
pub const PF_R: u32 = 1 << 2;
pub const PF_MASKOS: u32 = 0x0ff00000;
pub const PF_MASKPROC: u32 = 0xf0000000;

// elf.h - Legal values for a_type (entry type).
pub const AT_NULL: c_ulong = 0;
pub const AT_IGNORE: c_ulong = 1;
pub const AT_EXECFD: c_ulong = 2;
pub const AT_PHDR: c_ulong = 3;
pub const AT_PHENT: c_ulong = 4;
pub const AT_PHNUM: c_ulong = 5;
pub const AT_PAGESZ: c_ulong = 6;
pub const AT_BASE: c_ulong = 7;
pub const AT_FLAGS: c_ulong = 8;
pub const AT_ENTRY: c_ulong = 9;
pub const AT_NOTELF: c_ulong = 10;
pub const AT_UID: c_ulong = 11;
pub const AT_EUID: c_ulong = 12;
pub const AT_GID: c_ulong = 13;
pub const AT_EGID: c_ulong = 14;
pub const AT_PLATFORM: c_ulong = 15;
pub const AT_HWCAP: c_ulong = 16;
pub const AT_CLKTCK: c_ulong = 17;

pub const AT_SECURE: c_ulong = 23;
pub const AT_BASE_PLATFORM: c_ulong = 24;
pub const AT_RANDOM: c_ulong = 25;
pub const AT_HWCAP2: c_ulong = 26;

pub const AT_HWCAP3: c_ulong = 29;
pub const AT_HWCAP4: c_ulong = 30;
pub const AT_EXECFN: c_ulong = 31;

// defined in arch/<arch>/include/uapi/asm/auxvec.h but has the same value
// wherever it is defined.
pub const AT_SYSINFO_EHDR: c_ulong = 33;
#[cfg(not(target_env = "uclibc"))]
pub const AT_MINSIGSTKSZ: c_ulong = 51;

pub const GLOB_ERR: c_int = 1 << 0;
pub const GLOB_MARK: c_int = 1 << 1;
pub const GLOB_NOSORT: c_int = 1 << 2;
pub const GLOB_DOOFFS: c_int = 1 << 3;
pub const GLOB_NOCHECK: c_int = 1 << 4;
pub const GLOB_APPEND: c_int = 1 << 5;
pub const GLOB_NOESCAPE: c_int = 1 << 6;

pub const GLOB_NOSPACE: c_int = 1;
pub const GLOB_ABORTED: c_int = 2;
pub const GLOB_NOMATCH: c_int = 3;

pub const POSIX_MADV_NORMAL: c_int = 0;
pub const POSIX_MADV_RANDOM: c_int = 1;
pub const POSIX_MADV_SEQUENTIAL: c_int = 2;
pub const POSIX_MADV_WILLNEED: c_int = 3;
pub const POSIX_MADV_DONTNEED: c_int = 4;

pub const S_IEXEC: crate::mode_t = 0o0100;
pub const S_IWRITE: crate::mode_t = 0o0200;
pub const S_IREAD: crate::mode_t = 0o0400;

pub const F_LOCK: c_int = 1;
pub const F_TEST: c_int = 3;
pub const F_TLOCK: c_int = 2;
pub const F_ULOCK: c_int = 0;

pub const ST_RDONLY: c_ulong = 1;
pub const ST_NOSUID: c_ulong = 2;
pub const ST_NODEV: c_ulong = 4;
pub const ST_NOEXEC: c_ulong = 8;
pub const ST_SYNCHRONOUS: c_ulong = 16;
pub const ST_MANDLOCK: c_ulong = 64;
pub const ST_WRITE: c_ulong = 128;
pub const ST_APPEND: c_ulong = 256;
pub const ST_IMMUTABLE: c_ulong = 512;
pub const ST_NOATIME: c_ulong = 1024;
pub const ST_NODIRATIME: c_ulong = 2048;

pub const RTLD_NEXT: *mut c_void = -1i64 as *mut c_void;
pub const RTLD_DEFAULT: *mut c_void = ptr::null_mut();
pub const RTLD_NODELETE: c_int = 0x1000;
pub const RTLD_NOW: c_int = 0x2;

pub const AT_EACCESS: c_int = 0x200;

pub const PTHREAD_BARRIER_SERIAL_THREAD: c_int = -1;
pub const PTHREAD_ONCE_INIT: crate::pthread_once_t = 0;
pub const PTHREAD_MUTEX_NORMAL: c_int = 0;
pub const PTHREAD_MUTEX_RECURSIVE: c_int = 1;
pub const PTHREAD_MUTEX_ERRORCHECK: c_int = 2;
pub const PTHREAD_MUTEX_DEFAULT: c_int = PTHREAD_MUTEX_NORMAL;
#[cfg(not(target_env = "uclibc"))]
pub const PTHREAD_MUTEX_STALLED: c_int = 0;
#[cfg(not(target_env = "uclibc"))]
pub const PTHREAD_MUTEX_ROBUST: c_int = 1;
#[cfg(not(target_env = "uclibc"))]
pub const PTHREAD_PRIO_NONE: c_int = 0;
#[cfg(not(target_env = "uclibc"))]
pub const PTHREAD_PRIO_INHERIT: c_int = 1;
#[cfg(not(target_env = "uclibc"))]
pub const PTHREAD_PRIO_PROTECT: c_int = 2;
pub const PTHREAD_PROCESS_PRIVATE: c_int = 0;
pub const PTHREAD_PROCESS_SHARED: c_int = 1;
pub const PTHREAD_INHERIT_SCHED: c_int = 0;
pub const PTHREAD_EXPLICIT_SCHED: c_int = 1;
#[cfg(not(all(target_os = "l4re", target_env = "uclibc")))]
pub const __SIZEOF_PTHREAD_COND_T: usize = 48;

// netinet/in.h
// NOTE: These are in addition to the constants defined in src/unix/mod.rs

#[deprecated(
    since = "0.2.80",
    note = "This value was increased in the newer kernel \
            and we'll change this following upstream in the future release. \
            See #1896 for more info."
)]
pub const IPPROTO_MAX: c_int = 256;

// System V IPC
pub const IPC_PRIVATE: crate::key_t = 0;

pub const IPC_CREAT: c_int = 0o1000;
pub const IPC_EXCL: c_int = 0o2000;
pub const IPC_NOWAIT: c_int = 0o4000;

pub const IPC_RMID: c_int = 0;
pub const IPC_SET: c_int = 1;
pub const IPC_STAT: c_int = if cfg!(musl32_time64) { 0x102 } else { 2 };
pub const IPC_INFO: c_int = 3;

pub const SHM_R: c_int = 0o400;
pub const SHM_W: c_int = 0o200;

pub const SHM_RDONLY: c_int = 0o10000;
pub const SHM_RND: c_int = 0o20000;
pub const SHM_REMAP: c_int = 0o40000;
pub const SHM_LOCK: c_int = 11;
pub const SHM_UNLOCK: c_int = 12;
pub const SHM_HUGETLB: c_int = 0o4000;
#[cfg(not(all(target_env = "uclibc", target_arch = "mips")))]
pub const SHM_NORESERVE: c_int = 0o10000;

pub const LOG_NFACILITIES: c_int = 24;

pub const SEM_FAILED: *mut crate::sem_t = ptr::null_mut();

pub const AI_PASSIVE: c_int = 0x0001;
pub const AI_CANONNAME: c_int = 0x0002;
pub const AI_NUMERICHOST: c_int = 0x0004;
pub const AI_V4MAPPED: c_int = 0x0008;
pub const AI_ALL: c_int = 0x0010;
pub const AI_ADDRCONFIG: c_int = 0x0020;

pub const AI_NUMERICSERV: c_int = 0x0400;

pub const EAI_BADFLAGS: c_int = -1;
pub const EAI_NONAME: c_int = -2;
pub const EAI_AGAIN: c_int = -3;
pub const EAI_FAIL: c_int = -4;
pub const EAI_NODATA: c_int = -5;
pub const EAI_FAMILY: c_int = -6;
pub const EAI_SOCKTYPE: c_int = -7;
pub const EAI_SERVICE: c_int = -8;
pub const EAI_MEMORY: c_int = -10;
pub const EAI_SYSTEM: c_int = -11;
pub const EAI_OVERFLOW: c_int = -12;

pub const NI_NUMERICHOST: c_int = 1;
pub const NI_NUMERICSERV: c_int = 2;
pub const NI_NOFQDN: c_int = 4;
pub const NI_NAMEREQD: c_int = 8;
pub const NI_DGRAM: c_int = 16;
#[cfg(not(target_env = "uclibc"))]
pub const NI_IDN: c_int = 32;

cfg_if! {
    if #[cfg(not(target_env = "uclibc"))] {
        pub const AIO_CANCELED: c_int = 0;
        pub const AIO_NOTCANCELED: c_int = 1;
        pub const AIO_ALLDONE: c_int = 2;
        pub const LIO_READ: c_int = 0;
        pub const LIO_WRITE: c_int = 1;
        pub const LIO_NOP: c_int = 2;
        pub const LIO_WAIT: c_int = 0;
        pub const LIO_NOWAIT: c_int = 1;
        pub const RUSAGE_THREAD: c_int = 1;
        pub const MSG_COPY: c_int = 0o40000;
        pub const SHM_EXEC: c_int = 0o100000;
        pub const IPV6_MULTICAST_ALL: c_int = 29;
        pub const IPV6_ROUTER_ALERT_ISOLATE: c_int = 30;
        pub const PACKET_MR_UNICAST: c_int = 3;
        pub const PTRACE_EVENT_STOP: c_int = 128;
        pub const UDP_SEGMENT: c_int = 103;
        pub const UDP_GRO: c_int = 104;
    }
}

pub const PR_SET_PDEATHSIG: c_int = 1;
pub const PR_GET_PDEATHSIG: c_int = 2;

pub const PR_GET_DUMPABLE: c_int = 3;
pub const PR_SET_DUMPABLE: c_int = 4;

pub const PR_GET_UNALIGN: c_int = 5;
pub const PR_SET_UNALIGN: c_int = 6;
pub const PR_UNALIGN_NOPRINT: c_int = 1;
pub const PR_UNALIGN_SIGBUS: c_int = 2;

pub const PR_GET_KEEPCAPS: c_int = 7;
pub const PR_SET_KEEPCAPS: c_int = 8;

pub const PR_GET_FPEMU: c_int = 9;
pub const PR_SET_FPEMU: c_int = 10;
pub const PR_FPEMU_NOPRINT: c_int = 1;
pub const PR_FPEMU_SIGFPE: c_int = 2;

pub const PR_GET_FPEXC: c_int = 11;
pub const PR_SET_FPEXC: c_int = 12;
pub const PR_FP_EXC_SW_ENABLE: c_int = 0x80;
pub const PR_FP_EXC_DIV: c_int = 0x010000;
pub const PR_FP_EXC_OVF: c_int = 0x020000;
pub const PR_FP_EXC_UND: c_int = 0x040000;
pub const PR_FP_EXC_RES: c_int = 0x080000;
pub const PR_FP_EXC_INV: c_int = 0x100000;
pub const PR_FP_EXC_DISABLED: c_int = 0;
pub const PR_FP_EXC_NONRECOV: c_int = 1;
pub const PR_FP_EXC_ASYNC: c_int = 2;
pub const PR_FP_EXC_PRECISE: c_int = 3;

pub const PR_GET_TIMING: c_int = 13;
pub const PR_SET_TIMING: c_int = 14;
pub const PR_TIMING_STATISTICAL: c_int = 0;
pub const PR_TIMING_TIMESTAMP: c_int = 1;

pub const PR_SET_NAME: c_int = 15;
pub const PR_GET_NAME: c_int = 16;

pub const PR_GET_ENDIAN: c_int = 19;
pub const PR_SET_ENDIAN: c_int = 20;
pub const PR_ENDIAN_BIG: c_int = 0;
pub const PR_ENDIAN_LITTLE: c_int = 1;
pub const PR_ENDIAN_PPC_LITTLE: c_int = 2;

pub const PR_GET_SECCOMP: c_int = 21;
pub const PR_SET_SECCOMP: c_int = 22;

pub const PR_CAPBSET_READ: c_int = 23;
pub const PR_CAPBSET_DROP: c_int = 24;

pub const PR_GET_TSC: c_int = 25;
pub const PR_SET_TSC: c_int = 26;
pub const PR_TSC_ENABLE: c_int = 1;
pub const PR_TSC_SIGSEGV: c_int = 2;

pub const PR_GET_SECUREBITS: c_int = 27;
pub const PR_SET_SECUREBITS: c_int = 28;

pub const PR_SET_TIMERSLACK: c_int = 29;
pub const PR_GET_TIMERSLACK: c_int = 30;

pub const PR_TASK_PERF_EVENTS_DISABLE: c_int = 31;
pub const PR_TASK_PERF_EVENTS_ENABLE: c_int = 32;

pub const PR_MCE_KILL: c_int = 33;
pub const PR_MCE_KILL_CLEAR: c_int = 0;
pub const PR_MCE_KILL_SET: c_int = 1;

pub const PR_MCE_KILL_LATE: c_int = 0;
pub const PR_MCE_KILL_EARLY: c_int = 1;
pub const PR_MCE_KILL_DEFAULT: c_int = 2;

pub const PR_MCE_KILL_GET: c_int = 34;

pub const PR_SET_MM: c_int = 35;
pub const PR_SET_MM_START_CODE: c_int = 1;
pub const PR_SET_MM_END_CODE: c_int = 2;
pub const PR_SET_MM_START_DATA: c_int = 3;
pub const PR_SET_MM_END_DATA: c_int = 4;
pub const PR_SET_MM_START_STACK: c_int = 5;
pub const PR_SET_MM_START_BRK: c_int = 6;
pub const PR_SET_MM_BRK: c_int = 7;
pub const PR_SET_MM_ARG_START: c_int = 8;
pub const PR_SET_MM_ARG_END: c_int = 9;
pub const PR_SET_MM_ENV_START: c_int = 10;
pub const PR_SET_MM_ENV_END: c_int = 11;
pub const PR_SET_MM_AUXV: c_int = 12;
pub const PR_SET_MM_EXE_FILE: c_int = 13;
pub const PR_SET_MM_MAP: c_int = 14;
pub const PR_SET_MM_MAP_SIZE: c_int = 15;

pub const PR_SET_PTRACER: c_int = 0x59616d61;
pub const PR_SET_PTRACER_ANY: c_ulong = 0xffffffffffffffff;

pub const PR_SET_CHILD_SUBREAPER: c_int = 36;
pub const PR_GET_CHILD_SUBREAPER: c_int = 37;

pub const PR_SET_NO_NEW_PRIVS: c_int = 38;
pub const PR_GET_NO_NEW_PRIVS: c_int = 39;

pub const PR_GET_TID_ADDRESS: c_int = 40;

pub const PR_SET_THP_DISABLE: c_int = 41;
pub const PR_GET_THP_DISABLE: c_int = 42;

pub const PR_MPX_ENABLE_MANAGEMENT: c_int = 43;
pub const PR_MPX_DISABLE_MANAGEMENT: c_int = 44;

pub const PR_SET_FP_MODE: c_int = 45;
pub const PR_GET_FP_MODE: c_int = 46;
pub const PR_FP_MODE_FR: c_int = 1 << 0;
pub const PR_FP_MODE_FRE: c_int = 1 << 1;

pub const PR_CAP_AMBIENT: c_int = 47;
pub const PR_CAP_AMBIENT_IS_SET: c_int = 1;
pub const PR_CAP_AMBIENT_RAISE: c_int = 2;
pub const PR_CAP_AMBIENT_LOWER: c_int = 3;
pub const PR_CAP_AMBIENT_CLEAR_ALL: c_int = 4;

pub const PR_SET_VMA: c_int = 0x53564d41;
pub const PR_SET_VMA_ANON_NAME: c_int = 0;

pub const PR_SCHED_CORE: c_int = 62;
pub const PR_SCHED_CORE_GET: c_int = 0;
pub const PR_SCHED_CORE_CREATE: c_int = 1;
pub const PR_SCHED_CORE_SHARE_TO: c_int = 2;
pub const PR_SCHED_CORE_SHARE_FROM: c_int = 3;
pub const PR_SCHED_CORE_MAX: c_int = 4;
pub const PR_SCHED_CORE_SCOPE_THREAD: c_int = 0;
pub const PR_SCHED_CORE_SCOPE_THREAD_GROUP: c_int = 1;
pub const PR_SCHED_CORE_SCOPE_PROCESS_GROUP: c_int = 2;

pub const ITIMER_REAL: c_int = 0;
pub const ITIMER_VIRTUAL: c_int = 1;
pub const ITIMER_PROF: c_int = 2;

pub const _POSIX_VDISABLE: crate::cc_t = 0;

pub const IPV6_RTHDR_LOOSE: c_int = 0;
pub const IPV6_RTHDR_STRICT: c_int = 1;

pub const IUTF8: crate::tcflag_t = 0x00004000;
#[cfg(not(all(target_env = "uclibc", target_arch = "mips")))]
pub const CMSPAR: crate::tcflag_t = 0o10000000000;

pub const MFD_CLOEXEC: c_uint = 0x0001;
pub const MFD_ALLOW_SEALING: c_uint = 0x0002;
pub const MFD_HUGETLB: c_uint = 0x0004;
cfg_if! {
    if #[cfg(not(target_env = "uclibc"))] {
        pub const MFD_NOEXEC_SEAL: c_uint = 0x0008;
        pub const MFD_EXEC: c_uint = 0x0010;
        pub const MFD_HUGE_64KB: c_uint = 0x40000000;
        pub const MFD_HUGE_512KB: c_uint = 0x4c000000;
        pub const MFD_HUGE_1MB: c_uint = 0x50000000;
        pub const MFD_HUGE_2MB: c_uint = 0x54000000;
        pub const MFD_HUGE_8MB: c_uint = 0x5c000000;
        pub const MFD_HUGE_16MB: c_uint = 0x60000000;
        pub const MFD_HUGE_32MB: c_uint = 0x64000000;
        pub const MFD_HUGE_256MB: c_uint = 0x70000000;
        pub const MFD_HUGE_512MB: c_uint = 0x74000000;
        pub const MFD_HUGE_1GB: c_uint = 0x78000000;
        pub const MFD_HUGE_2GB: c_uint = 0x7c000000;
        pub const MFD_HUGE_16GB: c_uint = 0x88000000;
        pub const MFD_HUGE_MASK: c_uint = 63;
        pub const MFD_HUGE_SHIFT: c_uint = 26;
    }
}

// linux/if_packet.h
pub const PACKET_HOST: c_uchar = 0;
pub const PACKET_BROADCAST: c_uchar = 1;
pub const PACKET_MULTICAST: c_uchar = 2;
pub const PACKET_OTHERHOST: c_uchar = 3;
pub const PACKET_OUTGOING: c_uchar = 4;
pub const PACKET_LOOPBACK: c_uchar = 5;
#[cfg(not(target_os = "l4re"))]
pub const PACKET_USER: c_uchar = 6;
#[cfg(not(target_os = "l4re"))]
pub const PACKET_KERNEL: c_uchar = 7;

pub const PACKET_ADD_MEMBERSHIP: c_int = 1;
pub const PACKET_DROP_MEMBERSHIP: c_int = 2;
pub const PACKET_RECV_OUTPUT: c_int = 3;
pub const PACKET_RX_RING: c_int = 5;
pub const PACKET_STATISTICS: c_int = 6;
cfg_if! {
    if #[cfg(not(target_os = "l4re"))] {
        pub const PACKET_COPY_THRESH: c_int = 7;
        pub const PACKET_AUXDATA: c_int = 8;
        pub const PACKET_ORIGDEV: c_int = 9;
        pub const PACKET_VERSION: c_int = 10;
        pub const PACKET_HDRLEN: c_int = 11;
        pub const PACKET_RESERVE: c_int = 12;
        pub const PACKET_TX_RING: c_int = 13;
        pub const PACKET_LOSS: c_int = 14;
        pub const PACKET_VNET_HDR: c_int = 15;
        pub const PACKET_TX_TIMESTAMP: c_int = 16;
        pub const PACKET_TIMESTAMP: c_int = 17;
    }
}

pub const PACKET_MR_MULTICAST: c_int = 0;
pub const PACKET_MR_PROMISC: c_int = 1;
pub const PACKET_MR_ALLMULTI: c_int = 2;

pub const SIOCADDRT: c_ulong = 0x0000890B;
pub const SIOCDELRT: c_ulong = 0x0000890C;
pub const SIOCGIFNAME: c_ulong = 0x00008910;
pub const SIOCSIFLINK: c_ulong = 0x00008911;
pub const SIOCGIFCONF: c_ulong = 0x00008912;
pub const SIOCGIFFLAGS: c_ulong = 0x00008913;
pub const SIOCSIFFLAGS: c_ulong = 0x00008914;
pub const SIOCGIFADDR: c_ulong = 0x00008915;
pub const SIOCSIFADDR: c_ulong = 0x00008916;
pub const SIOCGIFDSTADDR: c_ulong = 0x00008917;
pub const SIOCSIFDSTADDR: c_ulong = 0x00008918;
pub const SIOCGIFBRDADDR: c_ulong = 0x00008919;
pub const SIOCSIFBRDADDR: c_ulong = 0x0000891A;
pub const SIOCGIFNETMASK: c_ulong = 0x0000891B;
pub const SIOCSIFNETMASK: c_ulong = 0x0000891C;
pub const SIOCGIFMETRIC: c_ulong = 0x0000891D;
pub const SIOCSIFMETRIC: c_ulong = 0x0000891E;
pub const SIOCGIFMEM: c_ulong = 0x0000891F;
pub const SIOCSIFMEM: c_ulong = 0x00008920;
pub const SIOCGIFMTU: c_ulong = 0x00008921;
pub const SIOCSIFMTU: c_ulong = 0x00008922;
pub const SIOCSIFNAME: c_ulong = 0x00008923;
pub const SIOCSIFHWADDR: c_ulong = 0x00008924;
pub const SIOCGIFENCAP: c_ulong = 0x00008925;
pub const SIOCSIFENCAP: c_ulong = 0x00008926;
pub const SIOCGIFHWADDR: c_ulong = 0x00008927;
pub const SIOCGIFSLAVE: c_ulong = 0x00008929;
pub const SIOCSIFSLAVE: c_ulong = 0x00008930;
pub const SIOCADDMULTI: c_ulong = 0x00008931;
pub const SIOCDELMULTI: c_ulong = 0x00008932;
pub const SIOCGIFINDEX: c_ulong = 0x00008933;
pub const SIOGIFINDEX: c_ulong = SIOCGIFINDEX;
pub const SIOCSIFPFLAGS: c_ulong = 0x00008934;
pub const SIOCGIFPFLAGS: c_ulong = 0x00008935;
pub const SIOCDIFADDR: c_ulong = 0x00008936;
pub const SIOCSIFHWBROADCAST: c_ulong = 0x00008937;
pub const SIOCGIFCOUNT: c_ulong = 0x00008938;
pub const SIOCGIFBR: c_ulong = 0x00008940;
pub const SIOCSIFBR: c_ulong = 0x00008941;
pub const SIOCGIFTXQLEN: c_ulong = 0x00008942;
pub const SIOCSIFTXQLEN: c_ulong = 0x00008943;
cfg_if! {
    if #[cfg(not(target_os = "l4re"))] {
        pub const SIOCETHTOOL: c_ulong = 0x00008946;
        pub const SIOCGMIIPHY: c_ulong = 0x00008947;
        pub const SIOCGMIIREG: c_ulong = 0x00008948;
        pub const SIOCSMIIREG: c_ulong = 0x00008949;
        pub const SIOCWANDEV: c_ulong = 0x0000894A;
        pub const SIOCOUTQNSD: c_ulong = 0x0000894B;
        pub const SIOCGSKNS: c_ulong = 0x0000894C;
    }
}
pub const SIOCDARP: c_ulong = 0x00008953;
pub const SIOCGARP: c_ulong = 0x00008954;
pub const SIOCSARP: c_ulong = 0x00008955;
pub const SIOCDRARP: c_ulong = 0x00008960;
pub const SIOCGRARP: c_ulong = 0x00008961;
pub const SIOCSRARP: c_ulong = 0x00008962;
pub const SIOCGIFMAP: c_ulong = 0x00008970;
pub const SIOCSIFMAP: c_ulong = 0x00008971;

pub const IPTOS_TOS_MASK: u8 = 0x1E;
pub const IPTOS_PREC_MASK: u8 = 0xE0;

pub const IPTOS_ECN_NOT_ECT: u8 = 0x00;

pub const RTF_UP: c_ushort = 0x0001;
pub const RTF_GATEWAY: c_ushort = 0x0002;

pub const RTF_HOST: c_ushort = 0x0004;
pub const RTF_REINSTATE: c_ushort = 0x0008;
pub const RTF_DYNAMIC: c_ushort = 0x0010;
pub const RTF_MODIFIED: c_ushort = 0x0020;
pub const RTF_MTU: c_ushort = 0x0040;
pub const RTF_MSS: c_ushort = RTF_MTU;
pub const RTF_WINDOW: c_ushort = 0x0080;
pub const RTF_IRTT: c_ushort = 0x0100;
pub const RTF_REJECT: c_ushort = 0x0200;
pub const RTF_STATIC: c_ushort = 0x0400;
pub const RTF_XRESOLVE: c_ushort = 0x0800;
pub const RTF_NOFORWARD: c_ushort = 0x1000;
pub const RTF_THROW: c_ushort = 0x2000;
pub const RTF_NOPMTUDISC: c_ushort = 0x4000;

pub const RTF_DEFAULT: u32 = 0x00010000;
pub const RTF_ALLONLINK: u32 = 0x00020000;
pub const RTF_ADDRCONF: u32 = 0x00040000;
pub const RTF_LINKRT: u32 = 0x00100000;
pub const RTF_NONEXTHOP: u32 = 0x00200000;
pub const RTF_CACHE: u32 = 0x01000000;
pub const RTF_FLOW: u32 = 0x02000000;
pub const RTF_POLICY: u32 = 0x04000000;

pub const RTCF_VALVE: u32 = 0x00200000;
pub const RTCF_MASQ: u32 = 0x00400000;
pub const RTCF_NAT: u32 = 0x00800000;
pub const RTCF_DOREDIRECT: u32 = 0x01000000;
pub const RTCF_LOG: u32 = 0x02000000;
pub const RTCF_DIRECTSRC: u32 = 0x04000000;

pub const RTF_LOCAL: u32 = 0x80000000;
pub const RTF_INTERFACE: u32 = 0x40000000;
pub const RTF_MULTICAST: u32 = 0x20000000;
pub const RTF_BROADCAST: u32 = 0x10000000;
pub const RTF_NAT: u32 = 0x08000000;
pub const RTF_ADDRCLASSMASK: u32 = 0xF8000000;

pub const RT_CLASS_UNSPEC: u8 = 0;
pub const RT_CLASS_DEFAULT: u8 = 253;
pub const RT_CLASS_MAIN: u8 = 254;
pub const RT_CLASS_LOCAL: u8 = 255;
pub const RT_CLASS_MAX: u8 = 255;

pub const MAX_ADDR_LEN: usize = 7;
pub const ARPD_UPDATE: c_ushort = 0x01;
pub const ARPD_LOOKUP: c_ushort = 0x02;
pub const ARPD_FLUSH: c_ushort = 0x03;
pub const ATF_MAGIC: c_int = 0x80;

// include/uapi/linux/udp.h
pub const UDP_CORK: c_int = 1;
pub const UDP_ENCAP: c_int = 100;
pub const UDP_NO_CHECK6_TX: c_int = 101;
pub const UDP_NO_CHECK6_RX: c_int = 102;

// include/uapi/asm-generic/mman-common.h
pub const MAP_FIXED_NOREPLACE: c_int = 0x100000;
pub const MLOCK_ONFAULT: c_uint = 0x01;

pub const REG_EXTENDED: c_int = 1;
pub const REG_ICASE: c_int = 2;
pub const REG_NEWLINE: c_int = 4;
pub const REG_NOSUB: c_int = 8;

pub const REG_NOTBOL: c_int = 1;
pub const REG_NOTEOL: c_int = 2;

pub const REG_ENOSYS: c_int = -1;
pub const REG_NOMATCH: c_int = 1;
pub const REG_BADPAT: c_int = 2;
pub const REG_ECOLLATE: c_int = 3;
pub const REG_ECTYPE: c_int = 4;
pub const REG_EESCAPE: c_int = 5;
pub const REG_ESUBREG: c_int = 6;
pub const REG_EBRACK: c_int = 7;
pub const REG_EPAREN: c_int = 8;
pub const REG_EBRACE: c_int = 9;
pub const REG_BADBR: c_int = 10;
pub const REG_ERANGE: c_int = 11;
pub const REG_ESPACE: c_int = 12;
pub const REG_BADRPT: c_int = 13;

// errno.h
pub const EPERM: c_int = 1;
pub const ENOENT: c_int = 2;
pub const ESRCH: c_int = 3;
pub const EINTR: c_int = 4;
pub const EIO: c_int = 5;
pub const ENXIO: c_int = 6;
pub const E2BIG: c_int = 7;
pub const ENOEXEC: c_int = 8;
pub const EBADF: c_int = 9;
pub const ECHILD: c_int = 10;
pub const EAGAIN: c_int = 11;
pub const ENOMEM: c_int = 12;
pub const EACCES: c_int = 13;
pub const EFAULT: c_int = 14;
pub const ENOTBLK: c_int = 15;
pub const EBUSY: c_int = 16;
pub const EEXIST: c_int = 17;
pub const EXDEV: c_int = 18;
pub const ENODEV: c_int = 19;
pub const ENOTDIR: c_int = 20;
pub const EISDIR: c_int = 21;
pub const EINVAL: c_int = 22;
pub const ENFILE: c_int = 23;
pub const EMFILE: c_int = 24;
pub const ENOTTY: c_int = 25;
pub const ETXTBSY: c_int = 26;
pub const EFBIG: c_int = 27;
pub const ENOSPC: c_int = 28;
pub const ESPIPE: c_int = 29;
pub const EROFS: c_int = 30;
pub const EMLINK: c_int = 31;
pub const EPIPE: c_int = 32;
pub const EDOM: c_int = 33;
pub const ERANGE: c_int = 34;
pub const EWOULDBLOCK: c_int = EAGAIN;

pub const CSIGNAL: c_int = 0x000000ff;

#[cfg(not(target_os = "l4re"))]
pub const SCHED_NORMAL: c_int = 0;
pub const SCHED_OTHER: c_int = 0;
pub const SCHED_FIFO: c_int = 1;
pub const SCHED_RR: c_int = 2;
pub const SCHED_BATCH: c_int = 3;
pub const SCHED_IDLE: c_int = 5;
pub const SCHED_DEADLINE: c_int = 6;

pub const SCHED_RESET_ON_FORK: c_int = 0x40000000;

// elf.h
pub const NT_PRSTATUS: c_int = 1;
#[cfg(not(target_os = "l4re"))]
pub const NT_PRFPREG: c_int = 2;
pub const NT_FPREGSET: c_int = 2;
pub const NT_PRPSINFO: c_int = 3;
#[cfg(not(target_os = "l4re"))]
pub const NT_PRXREG: c_int = 4;
pub const NT_TASKSTRUCT: c_int = 4;
pub const NT_PLATFORM: c_int = 5;
pub const NT_AUXV: c_int = 6;
pub const NT_GWINDOWS: c_int = 7;
pub const NT_ASRS: c_int = 8;
pub const NT_PSTATUS: c_int = 10;
pub const NT_PSINFO: c_int = 13;
pub const NT_PRCRED: c_int = 14;
pub const NT_UTSNAME: c_int = 15;
pub const NT_LWPSTATUS: c_int = 16;
pub const NT_LWPSINFO: c_int = 17;
pub const NT_PRFPXREG: c_int = 20;

pub const MS_NOUSER: c_ulong = 0xffffffff80000000;

f! {
    pub fn CMSG_NXTHDR(
        mhdr: *const crate::msghdr,
        cmsg: *const crate::cmsghdr,
    ) -> *mut crate::cmsghdr {
        if ((*cmsg).cmsg_len as usize) < size_of::<crate::cmsghdr>() {
            return core::ptr::null_mut::<crate::cmsghdr>();
        }

        // FIXME(msrv): `.wrapping_byte_add()` stabilized in 1.75
        let next_cmsg = cmsg
            .cast::<u8>()
            .wrapping_add(super::CMSG_ALIGN((*cmsg).cmsg_len as usize))
            .cast::<crate::cmsghdr>();

        // In case the addition wrapped. `next_addr > max_addr`
        // would otherwise not work as intended.
        if (next_cmsg as usize) < (cmsg as usize) {
            return core::ptr::null_mut();
        }

        let mut max_addr = (*mhdr).msg_control as usize + (*mhdr).msg_controllen as usize;

        if cfg!(any(target_env = "musl", target_env = "ohos")) {
            // musl and some of its descendants do `>= max_addr`
            // comparisons in the if statement below.
            // https://www.openwall.com/lists/musl/2025/12/27/1
            max_addr -= 1;
        }

        if next_cmsg as usize + size_of::<crate::cmsghdr>() > max_addr {
            core::ptr::null_mut::<crate::cmsghdr>()
        } else {
            next_cmsg as *mut crate::cmsghdr
        }
    }

    pub fn CPU_ALLOC_SIZE(count: c_int) -> size_t {
        let _dummy: cpu_set_t = mem::zeroed();
        let size_in_bits = 8 * size_of_val(&_dummy.bits[0]);
        ((count as size_t + size_in_bits - 1) / 8) as size_t
    }

    pub fn CPU_ZERO(cpuset: &mut cpu_set_t) -> () {
        for slot in &mut cpuset.bits {
            *slot = 0;
        }
    }

    pub fn CPU_SET(cpu: usize, cpuset: &mut cpu_set_t) -> () {
        let size_in_bits = 8 * size_of_val(&cpuset.bits[0]); // 32, 64 etc
        let (idx, offset) = (cpu / size_in_bits, cpu % size_in_bits);
        cpuset.bits[idx] |= 1 << offset;
    }

    pub fn CPU_CLR(cpu: usize, cpuset: &mut cpu_set_t) -> () {
        let size_in_bits = 8 * size_of_val(&cpuset.bits[0]); // 32, 64 etc
        let (idx, offset) = (cpu / size_in_bits, cpu % size_in_bits);
        cpuset.bits[idx] &= !(1 << offset);
    }

    pub fn CPU_ISSET(cpu: usize, cpuset: &cpu_set_t) -> bool {
        let size_in_bits = 8 * size_of_val(&cpuset.bits[0]);
        let (idx, offset) = (cpu / size_in_bits, cpu % size_in_bits);
        0 != (cpuset.bits[idx] & (1 << offset))
    }

    pub fn CPU_COUNT_S(size: usize, cpuset: &cpu_set_t) -> c_int {
        let mut s: u32 = 0;
        let size_of_mask = size_of_val(&cpuset.bits[0]);
        for i in &cpuset.bits[..(size / size_of_mask)] {
            s += i.count_ones();
        }
        s as c_int
    }

    pub fn CPU_COUNT(cpuset: &cpu_set_t) -> c_int {
        CPU_COUNT_S(size_of::<cpu_set_t>(), cpuset)
    }

    pub fn CPU_EQUAL(set1: &cpu_set_t, set2: &cpu_set_t) -> bool {
        set1.bits == set2.bits
    }

    pub fn IPTOS_TOS(tos: u8) -> u8 {
        tos & IPTOS_TOS_MASK
    }

    pub fn IPTOS_PREC(tos: u8) -> u8 {
        tos & IPTOS_PREC_MASK
    }

    pub fn RT_TOS(tos: u8) -> u8 {
        tos & crate::IPTOS_TOS_MASK
    }

    pub fn RT_ADDRCLASS(flags: u32) -> u32 {
        flags >> 23
    }

    pub fn RT_LOCALADDR(flags: u32) -> bool {
        (flags & RTF_ADDRCLASSMASK) == (RTF_LOCAL | RTF_INTERFACE)
    }

    pub fn ELF32_R_SYM(val: Elf32_Word) -> Elf32_Word {
        val >> 8
    }

    pub fn ELF32_R_TYPE(val: Elf32_Word) -> Elf32_Word {
        val & 0xff
    }

    pub fn ELF32_R_INFO(sym: Elf32_Word, t: Elf32_Word) -> Elf32_Word {
        sym << (8 + t) & 0xff
    }

    pub fn ELF64_R_SYM(val: Elf64_Xword) -> Elf64_Xword {
        val >> 32
    }

    pub fn ELF64_R_TYPE(val: Elf64_Xword) -> Elf64_Xword {
        val & 0xffffffff
    }

    pub fn ELF64_R_INFO(sym: Elf64_Xword, t: Elf64_Xword) -> Elf64_Xword {
        sym << (32 + t)
    }
}

safe_f! {
    pub const fn makedev(major: c_uint, minor: c_uint) -> crate::dev_t {
        let major = major as crate::dev_t;
        let minor = minor as crate::dev_t;
        let mut dev = 0;
        dev |= (major & 0x00000fff) << 8;
        dev |= (major & 0xfffff000) << 32;
        dev |= (minor & 0x000000ff) << 0;
        dev |= (minor & 0xffffff00) << 12;
        dev
    }

    pub const fn major(dev: crate::dev_t) -> c_uint {
        let mut major = 0;
        major |= (dev & 0x00000000000fff00) >> 8;
        major |= (dev & 0xfffff00000000000) >> 32;
        major as c_uint
    }

    pub const fn minor(dev: crate::dev_t) -> c_uint {
        let mut minor = 0;
        minor |= (dev & 0x00000000000000ff) >> 0;
        minor |= (dev & 0x00000ffffff00000) >> 12;
        minor as c_uint
    }
}

cfg_if! {
    if #[cfg(all(
        any(target_env = "gnu", target_env = "musl", target_env = "ohos"),
        any(target_arch = "x86_64", target_arch = "x86")
    ))] {
        extern "C" {
            pub fn iopl(level: c_int) -> c_int;
            pub fn ioperm(from: c_ulong, num: c_ulong, turn_on: c_int) -> c_int;
        }
    }
}

cfg_if! {
    if #[cfg(all(not(target_env = "uclibc"), not(target_env = "ohos")))] {
        extern "C" {
            #[cfg_attr(gnu_file_offset_bits64, link_name = "aio_read64")]
            pub fn aio_read(aiocbp: *mut crate::aiocb) -> c_int;
            #[cfg_attr(gnu_file_offset_bits64, link_name = "aio_write64")]
            pub fn aio_write(aiocbp: *mut crate::aiocb) -> c_int;
            pub fn aio_fsync(op: c_int, aiocbp: *mut crate::aiocb) -> c_int;
            #[cfg_attr(gnu_file_offset_bits64, link_name = "aio_error64")]
            pub fn aio_error(aiocbp: *const crate::aiocb) -> c_int;
            #[cfg_attr(gnu_file_offset_bits64, link_name = "aio_return64")]
            pub fn aio_return(aiocbp: *mut crate::aiocb) -> ssize_t;
            #[cfg_attr(
                any(musl32_time64, gnu_time_bits64),
                link_name = "__aio_suspend_time64"
            )]
            pub fn aio_suspend(
                aiocb_list: *const *const crate::aiocb,
                nitems: c_int,
                timeout: *const crate::timespec,
            ) -> c_int;
            #[cfg_attr(gnu_file_offset_bits64, link_name = "aio_cancel64")]
            pub fn aio_cancel(fd: c_int, aiocbp: *mut crate::aiocb) -> c_int;
            #[cfg_attr(gnu_file_offset_bits64, link_name = "lio_listio64")]
            pub fn lio_listio(
                mode: c_int,
                aiocb_list: *const *mut crate::aiocb,
                nitems: c_int,
                sevp: *mut crate::sigevent,
            ) -> c_int;
        }
    }
}

cfg_if! {
    if #[cfg(not(target_env = "uclibc"))] {
        extern "C" {
            #[cfg_attr(gnu_file_offset_bits64, link_name = "pwritev64")]
            pub fn pwritev(
                fd: c_int,
                iov: *const crate::iovec,
                iovcnt: c_int,
                offset: crate::off_t,
            ) -> ssize_t;
            #[cfg_attr(gnu_file_offset_bits64, link_name = "preadv64")]
            pub fn preadv(
                fd: c_int,
                iov: *const crate::iovec,
                iovcnt: c_int,
                offset: crate::off_t,
            ) -> ssize_t;
            pub fn getnameinfo(
                sa: *const crate::sockaddr,
                salen: crate::socklen_t,
                host: *mut c_char,
                hostlen: crate::socklen_t,
                serv: *mut c_char,
                servlen: crate::socklen_t,
                flags: c_int,
            ) -> c_int;
            pub fn getloadavg(loadavg: *mut c_double, nelem: c_int) -> c_int;
            pub fn process_vm_readv(
                pid: crate::pid_t,
                local_iov: *const crate::iovec,
                liovcnt: c_ulong,
                remote_iov: *const crate::iovec,
                riovcnt: c_ulong,
                flags: c_ulong,
            ) -> isize;
            pub fn process_vm_writev(
                pid: crate::pid_t,
                local_iov: *const crate::iovec,
                liovcnt: c_ulong,
                remote_iov: *const crate::iovec,
                riovcnt: c_ulong,
                flags: c_ulong,
            ) -> isize;
            #[cfg_attr(gnu_time_bits64, link_name = "__futimes64")]
            #[cfg_attr(musl32_time64, link_name = "__futimes_time64")]
            pub fn futimes(fd: c_int, times: *const crate::timeval) -> c_int;
        }
    }
}

extern "C" {
    #[cfg_attr(
        not(any(target_env = "musl", target_env = "ohos")),
        link_name = "__xpg_strerror_r"
    )]
    pub fn strerror_r(errnum: c_int, buf: *mut c_char, buflen: size_t) -> c_int;

    pub fn abs(i: c_int) -> c_int;
    pub fn labs(i: c_long) -> c_long;
    pub fn rand() -> c_int;
    pub fn srand(seed: c_uint);

    pub fn drand48() -> c_double;
    pub fn erand48(xseed: *mut c_ushort) -> c_double;
    pub fn lrand48() -> c_long;
    pub fn nrand48(xseed: *mut c_ushort) -> c_long;
    pub fn jrand48(xseed: *mut c_ushort) -> c_long;
    pub fn srand48(seed: c_long);

    pub fn setpwent();
    pub fn endpwent();
    pub fn getpwent() -> *mut passwd;
    pub fn setgrent();
    pub fn endgrent();
    pub fn getgrent() -> *mut crate::group;
    #[cfg(not(target_os = "l4re"))]
    pub fn setspent();
    #[cfg(not(target_os = "l4re"))]
    pub fn endspent();
    #[cfg(not(target_os = "l4re"))]
    pub fn getspent() -> *mut spwd;

    pub fn getspnam(name: *const c_char) -> *mut spwd;

    // System V IPC
    pub fn shmget(key: crate::key_t, size: size_t, shmflg: c_int) -> c_int;
    pub fn shmat(shmid: c_int, shmaddr: *const c_void, shmflg: c_int) -> *mut c_void;
    pub fn shmdt(shmaddr: *const c_void) -> c_int;
    #[cfg_attr(gnu_time_bits64, link_name = "__shmctl64")]
    pub fn shmctl(shmid: c_int, cmd: c_int, buf: *mut crate::shmid_ds) -> c_int;

    pub fn mprotect(addr: *mut c_void, len: size_t, prot: c_int) -> c_int;
    pub fn __errno_location() -> *mut c_int;

    // Not available now on Android
    pub fn mremap(
        addr: *mut c_void,
        len: size_t,
        new_len: size_t,
        flags: c_int,
        ...
    ) -> *mut c_void;

    #[cfg_attr(gnu_time_bits64, link_name = "__glob64_time64")]
    #[cfg_attr(
        all(not(gnu_time_bits64), gnu_file_offset_bits64),
        link_name = "glob64"
    )]
    pub fn glob(
        pattern: *const c_char,
        flags: c_int,
        errfunc: Option<extern "C" fn(epath: *const c_char, errno: c_int) -> c_int>,
        pglob: *mut crate::glob_t,
    ) -> c_int;
    #[cfg_attr(gnu_time_bits64, link_name = "__globfree64_time64")]
    #[cfg_attr(
        all(not(gnu_time_bits64), gnu_file_offset_bits64),
        link_name = "globfree64"
    )]
    pub fn globfree(pglob: *mut crate::glob_t);

    pub fn seekdir(dirp: *mut crate::DIR, loc: c_long);

    pub fn telldir(dirp: *mut crate::DIR) -> c_long;
    pub fn madvise(addr: *mut c_void, len: size_t, advice: c_int) -> c_int;

    pub fn msync(addr: *mut c_void, len: size_t, flags: c_int) -> c_int;
    pub fn recvfrom(
        socket: c_int,
        buf: *mut c_void,
        len: size_t,
        flags: c_int,
        addr: *mut crate::sockaddr,
        addrlen: *mut crate::socklen_t,
    ) -> ssize_t;

    pub fn nl_langinfo(item: crate::nl_item) -> *mut c_char;
    pub fn nl_langinfo_l(item: crate::nl_item, locale: crate::locale_t) -> *mut c_char;

    pub fn sched_getaffinity(
        pid: crate::pid_t,
        cpusetsize: size_t,
        cpuset: *mut cpu_set_t,
    ) -> c_int;
    pub fn sched_get_priority_max(policy: c_int) -> c_int;
    #[cfg_attr(gnu_time_bits64, link_name = "__settimeofday64")]
    #[cfg_attr(musl32_time64, link_name = "__settimeofday_time64")]
    pub fn settimeofday(tv: *const crate::timeval, tz: *const crate::timezone) -> c_int;
    #[cfg_attr(gnu_time_bits64, link_name = "__sem_timedwait64")]
    #[cfg_attr(musl32_time64, link_name = "__sem_timedwait_time64")]
    pub fn sem_timedwait(sem: *mut crate::sem_t, abstime: *const crate::timespec) -> c_int;
    pub fn sem_getvalue(sem: *mut crate::sem_t, sval: *mut c_int) -> c_int;
    pub fn mount(
        src: *const c_char,
        target: *const c_char,
        fstype: *const c_char,
        flags: c_ulong,
        data: *const c_void,
    ) -> c_int;
    #[cfg_attr(gnu_time_bits64, link_name = "__prctl_time64")]
    pub fn prctl(option: c_int, ...) -> c_int;
    #[cfg_attr(gnu_time_bits64, link_name = "__ppoll64")]
    #[cfg_attr(musl32_time64, link_name = "__ppoll_time64")]
    pub fn ppoll(
        fds: *mut crate::pollfd,
        nfds: crate::nfds_t,
        timeout: *const crate::timespec,
        sigmask: *const crate::sigset_t,
    ) -> c_int;
    pub fn sethostname(name: *const c_char, len: size_t) -> c_int;
    pub fn sched_get_priority_min(policy: c_int) -> c_int;
    pub fn sysinfo(info: *mut crate::sysinfo) -> c_int;
    pub fn sigsuspend(mask: *const crate::sigset_t) -> c_int;
    pub fn getgrgid_r(
        gid: crate::gid_t,
        grp: *mut crate::group,
        buf: *mut c_char,
        buflen: size_t,
        result: *mut *mut crate::group,
    ) -> c_int;
    pub fn sem_close(sem: *mut crate::sem_t) -> c_int;
    pub fn getgrnam_r(
        name: *const c_char,
        grp: *mut crate::group,
        buf: *mut c_char,
        buflen: size_t,
        result: *mut *mut crate::group,
    ) -> c_int;
    pub fn initgroups(user: *const c_char, group: crate::gid_t) -> c_int;
    pub fn sem_open(name: *const c_char, oflag: c_int, ...) -> *mut crate::sem_t;
    pub fn getgrnam(name: *const c_char) -> *mut crate::group;
    pub fn sem_unlink(name: *const c_char) -> c_int;
    pub fn daemon(nochdir: c_int, noclose: c_int) -> c_int;
    pub fn sigwait(set: *const crate::sigset_t, sig: *mut c_int) -> c_int;
    pub fn getgrgid(gid: crate::gid_t) -> *mut crate::group;
    pub fn popen(command: *const c_char, mode: *const c_char) -> *mut crate::FILE;
    pub fn faccessat(dirfd: c_int, pathname: *const c_char, mode: c_int, flags: c_int) -> c_int;
    pub fn dl_iterate_phdr(
        callback: Option<
            unsafe extern "C" fn(
                info: *mut crate::dl_phdr_info,
                size: size_t,
                data: *mut c_void,
            ) -> c_int,
        >,
        data: *mut c_void,
    ) -> c_int;

    pub fn setmntent(filename: *const c_char, ty: *const c_char) -> *mut crate::FILE;
    pub fn getmntent(stream: *mut crate::FILE) -> *mut crate::mntent;
    pub fn addmntent(stream: *mut crate::FILE, mnt: *const crate::mntent) -> c_int;
    pub fn endmntent(streamp: *mut crate::FILE) -> c_int;
    pub fn hasmntopt(mnt: *const crate::mntent, opt: *const c_char) -> *mut c_char;

    pub fn regcomp(preg: *mut crate::regex_t, pattern: *const c_char, cflags: c_int) -> c_int;

    pub fn regexec(
        preg: *const crate::regex_t,
        input: *const c_char,
        nmatch: size_t,
        pmatch: *mut regmatch_t,
        eflags: c_int,
    ) -> c_int;

    pub fn regerror(
        errcode: c_int,
        preg: *const crate::regex_t,
        errbuf: *mut c_char,
        errbuf_size: size_t,
    ) -> size_t;

    pub fn regfree(preg: *mut crate::regex_t);

    pub fn iconv_open(tocode: *const c_char, fromcode: *const c_char) -> iconv_t;
    pub fn iconv(
        cd: iconv_t,
        inbuf: *mut *mut c_char,
        inbytesleft: *mut size_t,
        outbuf: *mut *mut c_char,
        outbytesleft: *mut size_t,
    ) -> size_t;
    pub fn iconv_close(cd: iconv_t) -> c_int;

    pub fn gettid() -> crate::pid_t;

    pub fn timer_create(
        clockid: crate::clockid_t,
        sevp: *mut crate::sigevent,
        timerid: *mut crate::timer_t,
    ) -> c_int;
    pub fn timer_delete(timerid: crate::timer_t) -> c_int;
    #[cfg(not(target_os = "l4re"))]
    pub fn timer_getoverrun(timerid: crate::timer_t) -> c_int;
    #[cfg_attr(any(gnu_time_bits64, musl32_time64), link_name = "__timer_gettime64")]
    pub fn timer_gettime(timerid: crate::timer_t, curr_value: *mut crate::itimerspec) -> c_int;
    #[cfg_attr(any(gnu_time_bits64, musl32_time64), link_name = "__timer_settime64")]
    pub fn timer_settime(
        timerid: crate::timer_t,
        flags: c_int,
        new_value: *const crate::itimerspec,
        old_value: *mut crate::itimerspec,
    ) -> c_int;

    pub fn memmem(
        haystack: *const c_void,
        haystacklen: size_t,
        needle: *const c_void,
        needlelen: size_t,
    ) -> *mut c_void;
    pub fn sched_getcpu() -> c_int;

    pub fn getopt_long(
        argc: c_int,
        argv: *const *mut c_char,
        optstring: *const c_char,
        longopts: *const option,
        longindex: *mut c_int,
    ) -> c_int;

    #[cfg(not(target_env = "uclibc"))]
    pub fn copy_file_range(
        fd_in: c_int,
        off_in: *mut crate::off64_t,
        fd_out: c_int,
        off_out: *mut crate::off64_t,
        len: size_t,
        flags: c_uint,
    ) -> ssize_t;
}

cfg_if! {
    if #[cfg(not(any(target_env = "musl", target_env = "ohos")))] {
        extern "C" {
            pub fn freopen64(
                filename: *const c_char,
                mode: *const c_char,
                file: *mut crate::FILE,
            ) -> *mut crate::FILE;
            pub fn fseeko64(
                stream: *mut crate::FILE,
                offset: crate::off64_t,
                whence: c_int,
            ) -> c_int;
            pub fn fsetpos64(stream: *mut crate::FILE, ptr: *const crate::fpos64_t) -> c_int;
            pub fn ftello64(stream: *mut crate::FILE) -> crate::off64_t;
        }
    }
}
