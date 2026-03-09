//! Linux-specific definitions for linux-like values
use crate::prelude::*;
use crate::{
    sock_filter,
    _IO,
    _IOR,
    _IOW,
    _IOWR,
};

pub type dev_t = u64;
pub type socklen_t = u32;
pub type mode_t = u32;
pub type ino64_t = u64;
pub type off64_t = i64;
pub type blkcnt64_t = i64;
pub type rlim64_t = u64;
pub type mqd_t = c_int;
pub type nfds_t = c_ulong;
pub type nl_item = c_int;
pub type idtype_t = c_uint;
pub type loff_t = c_longlong;
pub type pthread_key_t = c_uint;
pub type pthread_once_t = c_int;
pub type pthread_spinlock_t = c_int;
pub type __kernel_fsid_t = __c_anonymous__kernel_fsid_t;
pub type __kernel_clockid_t = c_int;

pub type __u8 = c_uchar;
pub type __u16 = c_ushort;
pub type __s16 = c_short;
pub type __u32 = c_uint;
pub type __s32 = c_int;

// linux/sctp.h
pub type sctp_assoc_t = __s32;

pub type eventfd_t = u64;

e! {
    #[repr(u32)]
    pub enum tpacket_versions {
        TPACKET_V1,
        TPACKET_V2,
        TPACKET_V3,
    }
}

c_enum! {
    pub enum pid_type {
        pub PIDTYPE_PID,
        pub PIDTYPE_TGID,
        pub PIDTYPE_PGID,
        pub PIDTYPE_SID,
        pub PIDTYPE_MAX,
    }
}

s! {
    pub struct dqblk {
        pub dqb_bhardlimit: u64,
        pub dqb_bsoftlimit: u64,
        pub dqb_curspace: u64,
        pub dqb_ihardlimit: u64,
        pub dqb_isoftlimit: u64,
        pub dqb_curinodes: u64,
        pub dqb_btime: u64,
        pub dqb_itime: u64,
        pub dqb_valid: u32,
    }

    pub struct signalfd_siginfo {
        pub ssi_signo: u32,
        pub ssi_errno: i32,
        pub ssi_code: i32,
        pub ssi_pid: u32,
        pub ssi_uid: u32,
        pub ssi_fd: i32,
        pub ssi_tid: u32,
        pub ssi_band: u32,
        pub ssi_overrun: u32,
        pub ssi_trapno: u32,
        pub ssi_status: i32,
        pub ssi_int: i32,
        pub ssi_ptr: u64,
        pub ssi_utime: u64,
        pub ssi_stime: u64,
        pub ssi_addr: u64,
        pub ssi_addr_lsb: u16,
        _pad2: Padding<u16>,
        pub ssi_syscall: i32,
        pub ssi_call_addr: u64,
        pub ssi_arch: u32,
        _pad: Padding<[u8; 28]>,
    }

    pub struct fanout_args {
        #[cfg(target_endian = "little")]
        pub id: __u16,
        pub type_flags: __u16,
        #[cfg(target_endian = "big")]
        pub id: __u16,
        pub max_num_members: __u32,
    }

    #[deprecated(since = "0.2.70", note = "sockaddr_ll type must be used instead")]
    pub struct sockaddr_pkt {
        pub spkt_family: c_ushort,
        pub spkt_device: [c_uchar; 14],
        pub spkt_protocol: c_ushort,
    }

    pub struct tpacket_auxdata {
        pub tp_status: __u32,
        pub tp_len: __u32,
        pub tp_snaplen: __u32,
        pub tp_mac: __u16,
        pub tp_net: __u16,
        pub tp_vlan_tci: __u16,
        pub tp_vlan_tpid: __u16,
    }

    pub struct tpacket_hdr {
        pub tp_status: c_ulong,
        pub tp_len: c_uint,
        pub tp_snaplen: c_uint,
        pub tp_mac: c_ushort,
        pub tp_net: c_ushort,
        pub tp_sec: c_uint,
        pub tp_usec: c_uint,
    }

    pub struct tpacket_hdr_variant1 {
        pub tp_rxhash: __u32,
        pub tp_vlan_tci: __u32,
        pub tp_vlan_tpid: __u16,
        pub tp_padding: __u16,
    }

    pub struct tpacket2_hdr {
        pub tp_status: __u32,
        pub tp_len: __u32,
        pub tp_snaplen: __u32,
        pub tp_mac: __u16,
        pub tp_net: __u16,
        pub tp_sec: __u32,
        pub tp_nsec: __u32,
        pub tp_vlan_tci: __u16,
        pub tp_vlan_tpid: __u16,
        pub tp_padding: [__u8; 4],
    }

    pub struct tpacket_req {
        pub tp_block_size: c_uint,
        pub tp_block_nr: c_uint,
        pub tp_frame_size: c_uint,
        pub tp_frame_nr: c_uint,
    }

    pub struct tpacket_req3 {
        pub tp_block_size: c_uint,
        pub tp_block_nr: c_uint,
        pub tp_frame_size: c_uint,
        pub tp_frame_nr: c_uint,
        pub tp_retire_blk_tov: c_uint,
        pub tp_sizeof_priv: c_uint,
        pub tp_feature_req_word: c_uint,
    }

    #[repr(align(8))]
    pub struct tpacket_rollover_stats {
        pub tp_all: crate::__u64,
        pub tp_huge: crate::__u64,
        pub tp_failed: crate::__u64,
    }

    pub struct tpacket_stats {
        pub tp_packets: c_uint,
        pub tp_drops: c_uint,
    }

    pub struct tpacket_stats_v3 {
        pub tp_packets: c_uint,
        pub tp_drops: c_uint,
        pub tp_freeze_q_cnt: c_uint,
    }

    pub struct tpacket3_hdr {
        pub tp_next_offset: __u32,
        pub tp_sec: __u32,
        pub tp_nsec: __u32,
        pub tp_snaplen: __u32,
        pub tp_len: __u32,
        pub tp_status: __u32,
        pub tp_mac: __u16,
        pub tp_net: __u16,
        pub hv1: crate::tpacket_hdr_variant1,
        pub tp_padding: [__u8; 8],
    }

    pub struct tpacket_bd_ts {
        pub ts_sec: c_uint,
        pub ts_usec: c_uint,
    }

    #[repr(align(8))]
    pub struct tpacket_hdr_v1 {
        pub block_status: __u32,
        pub num_pkts: __u32,
        pub offset_to_first_pkt: __u32,
        pub blk_len: __u32,
        pub seq_num: crate::__u64,
        pub ts_first_pkt: crate::tpacket_bd_ts,
        pub ts_last_pkt: crate::tpacket_bd_ts,
    }

    // System V IPC
    pub struct msginfo {
        pub msgpool: c_int,
        pub msgmap: c_int,
        pub msgmax: c_int,
        pub msgmnb: c_int,
        pub msgmni: c_int,
        pub msgssz: c_int,
        pub msgtql: c_int,
        pub msgseg: c_ushort,
    }

    pub struct input_event {
        // FIXME(1.0): Change to the commented variant, see https://github.com/rust-lang/libc/pull/4148#discussion_r1857511742
        #[cfg(any(target_pointer_width = "64", not(linux_time_bits64)))]
        pub time: crate::timeval,
        // #[cfg(any(target_pointer_width = "64", not(linux_time_bits64)))]
        // pub input_event_sec: time_t,
        // #[cfg(any(target_pointer_width = "64", not(linux_time_bits64)))]
        // pub input_event_usec: suseconds_t,
        // #[cfg(target_arch = "sparc64")]
        // _pad1: c_int,
        #[cfg(all(target_pointer_width = "32", linux_time_bits64))]
        pub input_event_sec: c_ulong,

        #[cfg(all(target_pointer_width = "32", linux_time_bits64))]
        pub input_event_usec: c_ulong,

        pub type_: __u16,
        pub code: __u16,
        pub value: __s32,
    }

    pub struct input_id {
        pub bustype: __u16,
        pub vendor: __u16,
        pub product: __u16,
        pub version: __u16,
    }

    pub struct input_absinfo {
        pub value: __s32,
        pub minimum: __s32,
        pub maximum: __s32,
        pub fuzz: __s32,
        pub flat: __s32,
        pub resolution: __s32,
    }

    pub struct input_keymap_entry {
        pub flags: __u8,
        pub len: __u8,
        pub index: __u16,
        pub keycode: __u32,
        pub scancode: [__u8; 32],
    }

    pub struct input_mask {
        pub type_: __u32,
        pub codes_size: __u32,
        pub codes_ptr: crate::__u64,
    }

    pub struct ff_replay {
        pub length: __u16,
        pub delay: __u16,
    }

    pub struct ff_trigger {
        pub button: __u16,
        pub interval: __u16,
    }

    pub struct ff_envelope {
        pub attack_length: __u16,
        pub attack_level: __u16,
        pub fade_length: __u16,
        pub fade_level: __u16,
    }

    pub struct ff_constant_effect {
        pub level: __s16,
        pub envelope: ff_envelope,
    }

    pub struct ff_ramp_effect {
        pub start_level: __s16,
        pub end_level: __s16,
        pub envelope: ff_envelope,
    }

    pub struct ff_condition_effect {
        pub right_saturation: __u16,
        pub left_saturation: __u16,

        pub right_coeff: __s16,
        pub left_coeff: __s16,

        pub deadband: __u16,
        pub center: __s16,
    }

    pub struct ff_periodic_effect {
        pub waveform: __u16,
        pub period: __u16,
        pub magnitude: __s16,
        pub offset: __s16,
        pub phase: __u16,

        pub envelope: ff_envelope,

        pub custom_len: __u32,
        pub custom_data: *mut __s16,
    }

    pub struct ff_rumble_effect {
        pub strong_magnitude: __u16,
        pub weak_magnitude: __u16,
    }

    pub struct ff_effect {
        pub type_: __u16,
        pub id: __s16,
        pub direction: __u16,
        pub trigger: ff_trigger,
        pub replay: ff_replay,
        // FIXME(1.0): this is actually a union
        #[cfg(target_pointer_width = "64")]
        pub u: [u64; 4],
        #[cfg(target_pointer_width = "32")]
        pub u: [u32; 7],
    }

    pub struct uinput_ff_upload {
        pub request_id: __u32,
        pub retval: __s32,
        pub effect: ff_effect,
        pub old: ff_effect,
    }

    pub struct uinput_ff_erase {
        pub request_id: __u32,
        pub retval: __s32,
        pub effect_id: __u32,
    }

    pub struct uinput_abs_setup {
        pub code: __u16,
        pub absinfo: input_absinfo,
    }

    pub struct __c_anonymous__kernel_fsid_t {
        pub val: [c_int; 2],
    }

    pub struct posix_spawn_file_actions_t {
        __allocated: c_int,
        __used: c_int,
        __actions: *mut c_int,
        __pad: Padding<[c_int; 16]>,
    }

    pub struct posix_spawnattr_t {
        __flags: c_short,
        __pgrp: crate::pid_t,
        __sd: crate::sigset_t,
        __ss: crate::sigset_t,
        #[cfg(any(target_env = "musl", target_env = "ohos"))]
        __prio: c_int,
        #[cfg(not(any(target_env = "musl", target_env = "ohos")))]
        __sp: crate::sched_param,
        __policy: c_int,
        __pad: Padding<[c_int; 16]>,
    }

    pub struct genlmsghdr {
        pub cmd: u8,
        pub version: u8,
        pub reserved: u16,
    }

    pub struct inotify_event {
        pub wd: c_int,
        pub mask: u32,
        pub cookie: u32,
        pub len: u32,
    }

    pub struct fanotify_response {
        pub fd: c_int,
        pub response: __u32,
    }

    pub struct fanotify_event_info_header {
        pub info_type: __u8,
        pub pad: __u8,
        pub len: __u16,
    }

    pub struct fanotify_event_info_fid {
        pub hdr: fanotify_event_info_header,
        pub fsid: __kernel_fsid_t,
        pub handle: [c_uchar; 0],
    }

    pub struct sockaddr_vm {
        pub svm_family: crate::sa_family_t,
        pub svm_reserved1: c_ushort,
        pub svm_port: c_uint,
        pub svm_cid: c_uint,
        pub svm_zero: [u8; 4],
    }

    pub struct sock_extended_err {
        pub ee_errno: u32,
        pub ee_origin: u8,
        pub ee_type: u8,
        pub ee_code: u8,
        pub ee_pad: u8,
        pub ee_info: u32,
        pub ee_data: u32,
    }

    // linux/seccomp.h
    pub struct seccomp_data {
        pub nr: c_int,
        pub arch: __u32,
        pub instruction_pointer: crate::__u64,
        pub args: [crate::__u64; 6],
    }

    pub struct seccomp_notif_sizes {
        pub seccomp_notif: __u16,
        pub seccomp_notif_resp: __u16,
        pub seccomp_data: __u16,
    }

    pub struct seccomp_notif {
        pub id: crate::__u64,
        pub pid: __u32,
        pub flags: __u32,
        pub data: seccomp_data,
    }

    pub struct seccomp_notif_resp {
        pub id: crate::__u64,
        pub val: crate::__s64,
        pub error: __s32,
        pub flags: __u32,
    }

    pub struct seccomp_notif_addfd {
        pub id: crate::__u64,
        pub flags: __u32,
        pub srcfd: __u32,
        pub newfd: __u32,
        pub newfd_flags: __u32,
    }

    pub struct in6_ifreq {
        pub ifr6_addr: crate::in6_addr,
        pub ifr6_prefixlen: u32,
        pub ifr6_ifindex: c_int,
    }

    // linux/openat2.h
    #[non_exhaustive]
    pub struct open_how {
        pub flags: crate::__u64,
        pub mode: crate::__u64,
        pub resolve: crate::__u64,
    }

    // linux/ptp_clock.h
    pub struct ptp_clock_time {
        pub sec: crate::__s64,
        pub nsec: __u32,
        pub reserved: __u32,
    }

    pub struct ptp_extts_request {
        pub index: c_uint,
        pub flags: c_uint,
        pub rsv: [c_uint; 2],
    }

    pub struct ptp_sys_offset_extended {
        pub n_samples: c_uint,
        pub clockid: __kernel_clockid_t,
        pub rsv: [c_uint; 2],
        pub ts: [[ptp_clock_time; 3]; PTP_MAX_SAMPLES as usize],
    }

    pub struct ptp_sys_offset_precise {
        pub device: ptp_clock_time,
        pub sys_realtime: ptp_clock_time,
        pub sys_monoraw: ptp_clock_time,
        pub rsv: [c_uint; 4],
    }

    pub struct ptp_extts_event {
        pub t: ptp_clock_time,
        index: c_uint,
        flags: c_uint,
        rsv: [c_uint; 2],
    }

    // linux/sctp.h

    pub struct sctp_initmsg {
        pub sinit_num_ostreams: __u16,
        pub sinit_max_instreams: __u16,
        pub sinit_max_attempts: __u16,
        pub sinit_max_init_timeo: __u16,
    }

    pub struct sctp_sndrcvinfo {
        pub sinfo_stream: __u16,
        pub sinfo_ssn: __u16,
        pub sinfo_flags: __u16,
        pub sinfo_ppid: __u32,
        pub sinfo_context: __u32,
        pub sinfo_timetolive: __u32,
        pub sinfo_tsn: __u32,
        pub sinfo_cumtsn: __u32,
        pub sinfo_assoc_id: crate::sctp_assoc_t,
    }

    pub struct sctp_sndinfo {
        pub snd_sid: __u16,
        pub snd_flags: __u16,
        pub snd_ppid: __u32,
        pub snd_context: __u32,
        pub snd_assoc_id: crate::sctp_assoc_t,
    }

    pub struct sctp_rcvinfo {
        pub rcv_sid: __u16,
        pub rcv_ssn: __u16,
        pub rcv_flags: __u16,
        pub rcv_ppid: __u32,
        pub rcv_tsn: __u32,
        pub rcv_cumtsn: __u32,
        pub rcv_context: __u32,
        pub rcv_assoc_id: crate::sctp_assoc_t,
    }

    pub struct sctp_nxtinfo {
        pub nxt_sid: __u16,
        pub nxt_flags: __u16,
        pub nxt_ppid: __u32,
        pub nxt_length: __u32,
        pub nxt_assoc_id: crate::sctp_assoc_t,
    }

    pub struct sctp_prinfo {
        pub pr_policy: __u16,
        pub pr_value: __u32,
    }

    pub struct sctp_authinfo {
        pub auth_keynumber: __u16,
    }

    // linux/tls.h

    pub struct tls_crypto_info {
        pub version: __u16,
        pub cipher_type: __u16,
    }

    pub struct tls12_crypto_info_aes_gcm_128 {
        pub info: tls_crypto_info,
        pub iv: [c_uchar; TLS_CIPHER_AES_GCM_128_IV_SIZE],
        pub key: [c_uchar; TLS_CIPHER_AES_GCM_128_KEY_SIZE],
        pub salt: [c_uchar; TLS_CIPHER_AES_GCM_128_SALT_SIZE],
        pub rec_seq: [c_uchar; TLS_CIPHER_AES_GCM_128_REC_SEQ_SIZE],
    }

    pub struct tls12_crypto_info_aes_gcm_256 {
        pub info: tls_crypto_info,
        pub iv: [c_uchar; TLS_CIPHER_AES_GCM_256_IV_SIZE],
        pub key: [c_uchar; TLS_CIPHER_AES_GCM_256_KEY_SIZE],
        pub salt: [c_uchar; TLS_CIPHER_AES_GCM_256_SALT_SIZE],
        pub rec_seq: [c_uchar; TLS_CIPHER_AES_GCM_256_REC_SEQ_SIZE],
    }

    pub struct tls12_crypto_info_aes_ccm_128 {
        pub info: tls_crypto_info,
        pub iv: [c_uchar; TLS_CIPHER_AES_CCM_128_IV_SIZE],
        pub key: [c_uchar; TLS_CIPHER_AES_CCM_128_KEY_SIZE],
        pub salt: [c_uchar; TLS_CIPHER_AES_CCM_128_SALT_SIZE],
        pub rec_seq: [c_uchar; TLS_CIPHER_AES_CCM_128_REC_SEQ_SIZE],
    }

    pub struct tls12_crypto_info_chacha20_poly1305 {
        pub info: tls_crypto_info,
        pub iv: [c_uchar; TLS_CIPHER_CHACHA20_POLY1305_IV_SIZE],
        pub key: [c_uchar; TLS_CIPHER_CHACHA20_POLY1305_KEY_SIZE],
        pub salt: [c_uchar; TLS_CIPHER_CHACHA20_POLY1305_SALT_SIZE],
        pub rec_seq: [c_uchar; TLS_CIPHER_CHACHA20_POLY1305_REC_SEQ_SIZE],
    }

    pub struct tls12_crypto_info_sm4_gcm {
        pub info: tls_crypto_info,
        pub iv: [c_uchar; TLS_CIPHER_SM4_GCM_IV_SIZE],
        pub key: [c_uchar; TLS_CIPHER_SM4_GCM_KEY_SIZE],
        pub salt: [c_uchar; TLS_CIPHER_SM4_GCM_SALT_SIZE],
        pub rec_seq: [c_uchar; TLS_CIPHER_SM4_GCM_REC_SEQ_SIZE],
    }

    pub struct tls12_crypto_info_sm4_ccm {
        pub info: tls_crypto_info,
        pub iv: [c_uchar; TLS_CIPHER_SM4_CCM_IV_SIZE],
        pub key: [c_uchar; TLS_CIPHER_SM4_CCM_KEY_SIZE],
        pub salt: [c_uchar; TLS_CIPHER_SM4_CCM_SALT_SIZE],
        pub rec_seq: [c_uchar; TLS_CIPHER_SM4_CCM_REC_SEQ_SIZE],
    }

    pub struct tls12_crypto_info_aria_gcm_128 {
        pub info: tls_crypto_info,
        pub iv: [c_uchar; TLS_CIPHER_ARIA_GCM_128_IV_SIZE],
        pub key: [c_uchar; TLS_CIPHER_ARIA_GCM_128_KEY_SIZE],
        pub salt: [c_uchar; TLS_CIPHER_ARIA_GCM_128_SALT_SIZE],
        pub rec_seq: [c_uchar; TLS_CIPHER_ARIA_GCM_128_REC_SEQ_SIZE],
    }

    pub struct tls12_crypto_info_aria_gcm_256 {
        pub info: tls_crypto_info,
        pub iv: [c_uchar; TLS_CIPHER_ARIA_GCM_256_IV_SIZE],
        pub key: [c_uchar; TLS_CIPHER_ARIA_GCM_256_KEY_SIZE],
        pub salt: [c_uchar; TLS_CIPHER_ARIA_GCM_256_SALT_SIZE],
        pub rec_seq: [c_uchar; TLS_CIPHER_ARIA_GCM_256_REC_SEQ_SIZE],
    }

    // linux/wireless.h

    pub struct iw_param {
        pub value: __s32,
        pub fixed: __u8,
        pub disabled: __u8,
        pub flags: __u16,
    }

    pub struct iw_point {
        pub pointer: *mut c_void,
        pub length: __u16,
        pub flags: __u16,
    }

    pub struct iw_freq {
        pub m: __s32,
        pub e: __s16,
        pub i: __u8,
        pub flags: __u8,
    }

    pub struct iw_quality {
        pub qual: __u8,
        pub level: __u8,
        pub noise: __u8,
        pub updated: __u8,
    }

    pub struct iw_discarded {
        pub nwid: __u32,
        pub code: __u32,
        pub fragment: __u32,
        pub retries: __u32,
        pubmisc: __u32,
    }

    pub struct iw_missed {
        pub beacon: __u32,
    }

    pub struct iw_scan_req {
        pub scan_type: __u8,
        pub essid_len: __u8,
        pub num_channels: __u8,
        pub flags: __u8,
        pub bssid: crate::sockaddr,
        pub essid: [__u8; IW_ESSID_MAX_SIZE],
        pub min_channel_time: __u32,
        pub max_channel_time: __u32,
        pub channel_list: [iw_freq; IW_MAX_FREQUENCIES],
    }

    pub struct iw_encode_ext {
        pub ext_flags: __u32,
        pub tx_seq: [__u8; IW_ENCODE_SEQ_MAX_SIZE],
        pub rx_seq: [__u8; IW_ENCODE_SEQ_MAX_SIZE],
        pub addr: crate::sockaddr,
        pub alg: __u16,
        pub key_len: __u16,
        pub key: [__u8; 0],
    }

    pub struct iw_pmksa {
        pub cmd: __u32,
        pub bssid: crate::sockaddr,
        pub pmkid: [__u8; IW_PMKID_LEN],
    }

    pub struct iw_pmkid_cand {
        pub flags: __u32,
        pub index: __u32,
        pub bssid: crate::sockaddr,
    }

    pub struct iw_statistics {
        pub status: __u16,
        pub qual: iw_quality,
        pub discard: iw_discarded,
        pub miss: iw_missed,
    }

    pub struct iw_range {
        pub throughput: __u32,
        pub min_nwid: __u32,
        pub max_nwid: __u32,
        pub old_num_channels: __u16,
        pub old_num_frequency: __u8,
        pub scan_capa: __u8,
        pub event_capa: [__u32; 6],
        pub sensitivity: __s32,
        pub max_qual: iw_quality,
        pub avg_qual: iw_quality,
        pub num_bitrates: __u8,
        pub bitrate: [__s32; IW_MAX_BITRATES],
        pub min_rts: __s32,
        pub max_rts: __s32,
        pub min_frag: __s32,
        pub max_frag: __s32,
        pub min_pmp: __s32,
        pub max_pmp: __s32,
        pub min_pmt: __s32,
        pub max_pmt: __s32,
        pub pmp_flags: __u16,
        pub pmt_flags: __u16,
        pub pm_capa: __u16,
        pub encoding_size: [__u16; IW_MAX_ENCODING_SIZES],
        pub num_encoding_sizes: __u8,
        pub max_encoding_tokens: __u8,
        pub encoding_login_index: __u8,
        pub txpower_capa: __u16,
        pub num_txpower: __u8,
        pub txpower: [__s32; IW_MAX_TXPOWER],
        pub we_version_compiled: __u8,
        pub we_version_source: __u8,
        pub retry_capa: __u16,
        pub retry_flags: __u16,
        pub r_time_flags: __u16,
        pub min_retry: __s32,
        pub max_retry: __s32,
        pub min_r_time: __s32,
        pub max_r_time: __s32,
        pub num_channels: __u16,
        pub num_frequency: __u8,
        pub freq: [iw_freq; IW_MAX_FREQUENCIES],
        pub enc_capa: __u32,
    }

    pub struct iw_priv_args {
        pub cmd: __u32,
        pub set_args: __u16,
        pub get_args: __u16,
        pub name: [c_char; crate::IFNAMSIZ],
    }

    // #include <linux/eventpoll.h>

    pub struct epoll_params {
        pub busy_poll_usecs: u32,
        pub busy_poll_budget: u16,
        pub prefer_busy_poll: u8,
        pub __pad: u8, // Must be zero
    }

    #[cfg_attr(
        any(
            target_pointer_width = "32",
            target_arch = "x86_64",
            target_arch = "powerpc64",
            target_arch = "mips64",
            target_arch = "mips64r6",
            target_arch = "s390x",
            target_arch = "sparc64",
            target_arch = "aarch64",
            target_arch = "riscv64",
            target_arch = "riscv32",
            target_arch = "loongarch64"
        ),
        repr(align(4))
    )]
    #[cfg_attr(
        not(any(
            target_pointer_width = "32",
            target_arch = "x86_64",
            target_arch = "powerpc64",
            target_arch = "mips64",
            target_arch = "mips64r6",
            target_arch = "s390x",
            target_arch = "sparc64",
            target_arch = "aarch64",
            target_arch = "riscv64",
            target_arch = "riscv32",
            target_arch = "loongarch64"
        )),
        repr(align(8))
    )]
    pub struct pthread_mutexattr_t {
        #[doc(hidden)]
        size: [u8; crate::__SIZEOF_PTHREAD_MUTEXATTR_T],
    }

    #[cfg_attr(
        any(
            target_env = "musl",
            target_env = "ohos",
            target_env = "uclibc",
            target_pointer_width = "32"
        ),
        repr(align(4))
    )]
    #[cfg_attr(
        all(
            not(target_env = "musl"),
            not(target_env = "ohos"),
            not(target_env = "uclibc"),
            target_pointer_width = "64"
        ),
        repr(align(8))
    )]
    pub struct pthread_rwlockattr_t {
        #[doc(hidden)]
        size: [u8; crate::__SIZEOF_PTHREAD_RWLOCKATTR_T],
    }

    #[repr(align(4))]
    pub struct pthread_condattr_t {
        #[doc(hidden)]
        size: [u8; crate::__SIZEOF_PTHREAD_CONDATTR_T],
    }

    #[repr(align(4))]
    pub struct pthread_barrierattr_t {
        #[doc(hidden)]
        size: [u8; crate::__SIZEOF_PTHREAD_BARRIERATTR_T],
    }

    #[cfg(not(any(target_env = "musl", target_env = "ohos")))]
    #[repr(align(8))]
    pub struct fanotify_event_metadata {
        pub event_len: __u32,
        pub vers: __u8,
        pub reserved: __u8,
        pub metadata_len: __u16,
        pub mask: __u64,
        pub fd: c_int,
        pub pid: c_int,
    }

    // linux/ptp_clock.h

    pub struct ptp_sys_offset {
        pub n_samples: c_uint,
        pub rsv: [c_uint; 3],
        // FIXME(garando): replace length with `2 * PTP_MAX_SAMPLES + 1` when supported
        pub ts: [ptp_clock_time; 51],
    }

    pub struct ptp_pin_desc {
        pub name: [c_char; 64],
        pub index: c_uint,
        pub func: c_uint,
        pub chan: c_uint,
        pub rsv: [c_uint; 5],
    }

    pub struct ptp_clock_caps {
        pub max_adj: c_int,
        pub n_alarm: c_int,
        pub n_ext_ts: c_int,
        pub n_per_out: c_int,
        pub pps: c_int,
        pub n_pins: c_int,
        pub cross_timestamping: c_int,
        pub adjust_phase: c_int,
        pub max_phase_adj: c_int,
        pub rsv: [c_int; 11],
    }

    // linux/if_xdp.h

    pub struct sockaddr_xdp {
        pub sxdp_family: crate::__u16,
        pub sxdp_flags: crate::__u16,
        pub sxdp_ifindex: crate::__u32,
        pub sxdp_queue_id: crate::__u32,
        pub sxdp_shared_umem_fd: crate::__u32,
    }

    pub struct xdp_ring_offset {
        pub producer: crate::__u64,
        pub consumer: crate::__u64,
        pub desc: crate::__u64,
        pub flags: crate::__u64,
    }

    pub struct xdp_mmap_offsets {
        pub rx: xdp_ring_offset,
        pub tx: xdp_ring_offset,
        pub fr: xdp_ring_offset,
        pub cr: xdp_ring_offset,
    }

    pub struct xdp_ring_offset_v1 {
        pub producer: crate::__u64,
        pub consumer: crate::__u64,
        pub desc: crate::__u64,
    }

    pub struct xdp_mmap_offsets_v1 {
        pub rx: xdp_ring_offset_v1,
        pub tx: xdp_ring_offset_v1,
        pub fr: xdp_ring_offset_v1,
        pub cr: xdp_ring_offset_v1,
    }

    pub struct xdp_umem_reg {
        pub addr: crate::__u64,
        pub len: crate::__u64,
        pub chunk_size: crate::__u32,
        pub headroom: crate::__u32,
        pub flags: crate::__u32,
        pub tx_metadata_len: crate::__u32,
    }

    pub struct xdp_umem_reg_v1 {
        pub addr: crate::__u64,
        pub len: crate::__u64,
        pub chunk_size: crate::__u32,
        pub headroom: crate::__u32,
    }

    pub struct xdp_statistics {
        pub rx_dropped: crate::__u64,
        pub rx_invalid_descs: crate::__u64,
        pub tx_invalid_descs: crate::__u64,
        pub rx_ring_full: crate::__u64,
        pub rx_fill_ring_empty_descs: crate::__u64,
        pub tx_ring_empty_descs: crate::__u64,
    }

    pub struct xdp_statistics_v1 {
        pub rx_dropped: crate::__u64,
        pub rx_invalid_descs: crate::__u64,
        pub tx_invalid_descs: crate::__u64,
    }

    pub struct xdp_options {
        pub flags: crate::__u32,
    }

    pub struct xdp_desc {
        pub addr: crate::__u64,
        pub len: crate::__u32,
        pub options: crate::__u32,
    }

    pub struct xsk_tx_metadata_completion {
        pub tx_timestamp: crate::__u64,
    }

    pub struct xsk_tx_metadata_request {
        pub csum_start: __u16,
        pub csum_offset: __u16,
    }

    // linux/mount.h

    pub struct mount_attr {
        pub attr_set: crate::__u64,
        pub attr_clr: crate::__u64,
        pub propagation: crate::__u64,
        pub userns_fd: crate::__u64,
    }

    // linux/nsfs.h
    pub struct mnt_ns_info {
        pub size: crate::__u32,
        pub nr_mounts: crate::__u32,
        pub mnt_ns_id: crate::__u64,
    }

    // linux/pidfd.h

    #[non_exhaustive]
    pub struct pidfd_info {
        pub mask: crate::__u64,
        pub cgroupid: crate::__u64,
        pub pid: crate::__u32,
        pub tgid: crate::__u32,
        pub ppid: crate::__u32,
        pub ruid: crate::__u32,
        pub rgid: crate::__u32,
        pub euid: crate::__u32,
        pub egid: crate::__u32,
        pub suid: crate::__u32,
        pub sgid: crate::__u32,
        pub fsuid: crate::__u32,
        pub fsgid: crate::__u32,
        pub exit_code: crate::__s32,
    }

    // linux/uio.h

    pub struct dmabuf_cmsg {
        pub frag_offset: crate::__u64,
        pub frag_size: crate::__u32,
        pub frag_token: crate::__u32,
        pub dmabuf_id: crate::__u32,
        pub flags: crate::__u32,
    }

    pub struct dmabuf_token {
        pub token_start: crate::__u32,
        pub token_count: crate::__u32,
    }

    pub struct sockaddr_alg {
        pub salg_family: crate::sa_family_t,
        pub salg_type: [c_uchar; 14],
        pub salg_feat: u32,
        pub salg_mask: u32,
        pub salg_name: [c_uchar; 64],
    }

    #[cfg_attr(
        all(
            any(target_env = "musl", target_env = "ohos"),
            target_pointer_width = "32"
        ),
        repr(align(4))
    )]
    #[cfg_attr(
        all(
            any(target_env = "musl", target_env = "ohos"),
            target_pointer_width = "64"
        ),
        repr(align(8))
    )]
    #[cfg_attr(
        all(
            not(any(target_env = "musl", target_env = "ohos")),
            target_arch = "x86"
        ),
        repr(align(4))
    )]
    #[cfg_attr(
        all(
            not(any(target_env = "musl", target_env = "ohos")),
            not(target_arch = "x86")
        ),
        repr(align(8))
    )]
    pub struct pthread_cond_t {
        #[doc(hidden)]
        size: [u8; crate::__SIZEOF_PTHREAD_COND_T],
    }

    #[cfg_attr(
        all(
            target_pointer_width = "32",
            any(
                target_arch = "mips",
                target_arch = "mips32r6",
                target_arch = "arm",
                target_arch = "hexagon",
                target_arch = "m68k",
                target_arch = "csky",
                target_arch = "powerpc",
                target_arch = "sparc",
                target_arch = "x86_64",
                target_arch = "x86",
            )
        ),
        repr(align(4))
    )]
    #[cfg_attr(
        any(
            target_pointer_width = "64",
            not(any(
                target_arch = "mips",
                target_arch = "mips32r6",
                target_arch = "arm",
                target_arch = "hexagon",
                target_arch = "m68k",
                target_arch = "csky",
                target_arch = "powerpc",
                target_arch = "sparc",
                target_arch = "x86_64",
                target_arch = "x86",
            ))
        ),
        repr(align(8))
    )]
    pub struct pthread_mutex_t {
        #[doc(hidden)]
        size: [c_char; crate::__SIZEOF_PTHREAD_MUTEX_T],
    }

    #[cfg_attr(
        all(
            target_pointer_width = "32",
            any(
                target_arch = "mips",
                target_arch = "mips32r6",
                target_arch = "arm",
                target_arch = "hexagon",
                target_arch = "m68k",
                target_arch = "csky",
                target_arch = "powerpc",
                target_arch = "sparc",
                target_arch = "x86_64",
                target_arch = "x86"
            )
        ),
        repr(align(4))
    )]
    #[cfg_attr(
        any(
            target_pointer_width = "64",
            not(any(
                target_arch = "mips",
                target_arch = "mips32r6",
                target_arch = "arm",
                target_arch = "hexagon",
                target_arch = "m68k",
                target_arch = "powerpc",
                target_arch = "sparc",
                target_arch = "x86_64",
                target_arch = "x86"
            ))
        ),
        repr(align(8))
    )]
    pub struct pthread_rwlock_t {
        size: [u8; crate::__SIZEOF_PTHREAD_RWLOCK_T],
    }

    #[cfg_attr(
        all(
            target_pointer_width = "32",
            any(
                target_arch = "mips",
                target_arch = "mips32r6",
                target_arch = "arm",
                target_arch = "hexagon",
                target_arch = "m68k",
                target_arch = "csky",
                target_arch = "powerpc",
                target_arch = "sparc",
                target_arch = "x86_64",
                target_arch = "x86"
            )
        ),
        repr(align(4))
    )]
    #[cfg_attr(
        any(
            target_pointer_width = "64",
            not(any(
                target_arch = "mips",
                target_arch = "mips32r6",
                target_arch = "arm",
                target_arch = "hexagon",
                target_arch = "m68k",
                target_arch = "csky",
                target_arch = "powerpc",
                target_arch = "sparc",
                target_arch = "x86_64",
                target_arch = "x86"
            ))
        ),
        repr(align(8))
    )]
    pub struct pthread_barrier_t {
        size: [u8; crate::__SIZEOF_PTHREAD_BARRIER_T],
    }

    pub struct uinput_setup {
        pub id: input_id,
        pub name: [c_char; UINPUT_MAX_NAME_SIZE],
        pub ff_effects_max: __u32,
    }

    pub struct uinput_user_dev {
        pub name: [c_char; UINPUT_MAX_NAME_SIZE],
        pub id: input_id,
        pub ff_effects_max: __u32,
        pub absmax: [__s32; ABS_CNT],
        pub absmin: [__s32; ABS_CNT],
        pub absfuzz: [__s32; ABS_CNT],
        pub absflat: [__s32; ABS_CNT],
    }

    // x32 compatibility
    // See https://sourceware.org/bugzilla/show_bug.cgi?id=21279
    pub struct mq_attr {
        #[cfg(all(target_arch = "x86_64", target_pointer_width = "32"))]
        pub mq_flags: i64,
        #[cfg(all(target_arch = "x86_64", target_pointer_width = "32"))]
        pub mq_maxmsg: i64,
        #[cfg(all(target_arch = "x86_64", target_pointer_width = "32"))]
        pub mq_msgsize: i64,
        #[cfg(all(target_arch = "x86_64", target_pointer_width = "32"))]
        pub mq_curmsgs: i64,
        #[cfg(all(target_arch = "x86_64", target_pointer_width = "32"))]
        pad: Padding<[i64; 4]>,

        #[cfg(not(all(target_arch = "x86_64", target_pointer_width = "32")))]
        pub mq_flags: c_long,
        #[cfg(not(all(target_arch = "x86_64", target_pointer_width = "32")))]
        pub mq_maxmsg: c_long,
        #[cfg(not(all(target_arch = "x86_64", target_pointer_width = "32")))]
        pub mq_msgsize: c_long,
        #[cfg(not(all(target_arch = "x86_64", target_pointer_width = "32")))]
        pub mq_curmsgs: c_long,
        #[cfg(not(all(target_arch = "x86_64", target_pointer_width = "32")))]
        pad: Padding<[c_long; 4]>,
    }

    pub struct hwtstamp_config {
        pub flags: c_int,
        pub tx_type: c_int,
        pub rx_filter: c_int,
    }

    pub struct sched_attr {
        pub size: __u32,
        pub sched_policy: __u32,
        pub sched_flags: crate::__u64,
        pub sched_nice: __s32,
        pub sched_priority: __u32,
        pub sched_runtime: crate::__u64,
        pub sched_deadline: crate::__u64,
        pub sched_period: crate::__u64,
    }
}

cfg_if! {
    if #[cfg(not(target_arch = "sparc64"))] {
        s! {
            pub struct iw_thrspy {
                pub addr: crate::sockaddr,
                pub qual: iw_quality,
                pub low: iw_quality,
                pub high: iw_quality,
            }

            pub struct iw_mlme {
                pub cmd: __u16,
                pub reason_code: __u16,
                pub addr: crate::sockaddr,
            }

            pub struct iw_michaelmicfailure {
                pub flags: __u32,
                pub src_addr: crate::sockaddr,
                pub tsc: [__u8; IW_ENCODE_SEQ_MAX_SIZE],
            }
        }
    }
}

s_no_extra_traits! {
    /// WARNING: The `PartialEq`, `Eq` and `Hash` implementations of this
    /// type are unsound and will be removed in the future.
    #[deprecated(
        note = "this struct has unsafe trait implementations that will be \
                removed in the future",
        since = "0.2.80"
    )]
    pub struct af_alg_iv {
        pub ivlen: u32,
        pub iv: [c_uchar; 0],
    }

    pub union tpacket_req_u {
        pub req: crate::tpacket_req,
        pub req3: crate::tpacket_req3,
    }

    pub union tpacket_bd_header_u {
        pub bh1: crate::tpacket_hdr_v1,
    }

    pub struct tpacket_block_desc {
        pub version: __u32,
        pub offset_to_priv: __u32,
        pub hdr: crate::tpacket_bd_header_u,
    }

    // linux/net_tstamp.h
    pub struct sock_txtime {
        pub clockid: crate::clockid_t,
        pub flags: __u32,
    }

    // linux/wireless.h
    pub union iwreq_data {
        pub name: [c_char; crate::IFNAMSIZ],
        pub essid: iw_point,
        pub nwid: iw_param,
        pub freq: iw_freq,
        pub sens: iw_param,
        pub bitrate: iw_param,
        pub txpower: iw_param,
        pub rts: iw_param,
        pub frag: iw_param,
        pub mode: __u32,
        pub retry: iw_param,
        pub encoding: iw_point,
        pub power: iw_param,
        pub qual: iw_quality,
        pub ap_addr: crate::sockaddr,
        pub addr: crate::sockaddr,
        pub param: iw_param,
        pub data: iw_point,
    }

    pub struct iw_event {
        pub len: __u16,
        pub cmd: __u16,
        pub u: iwreq_data,
    }

    pub union __c_anonymous_iwreq {
        pub ifrn_name: [c_char; crate::IFNAMSIZ],
    }

    pub struct iwreq {
        pub ifr_ifrn: __c_anonymous_iwreq,
        pub u: iwreq_data,
    }

    // linux/ptp_clock.h
    pub union __c_anonymous_ptp_perout_request_1 {
        pub start: ptp_clock_time,
        pub phase: ptp_clock_time,
    }

    pub union __c_anonymous_ptp_perout_request_2 {
        pub on: ptp_clock_time,
        pub rsv: [c_uint; 4],
    }

    pub struct ptp_perout_request {
        pub anonymous_1: __c_anonymous_ptp_perout_request_1,
        pub period: ptp_clock_time,
        pub index: c_uint,
        pub flags: c_uint,
        pub anonymous_2: __c_anonymous_ptp_perout_request_2,
    }

    // linux/if_xdp.h
    pub struct xsk_tx_metadata {
        pub flags: crate::__u64,
        pub xsk_tx_metadata_union: __c_anonymous_xsk_tx_metadata_union,
    }

    pub union __c_anonymous_xsk_tx_metadata_union {
        pub request: xsk_tx_metadata_request,
        pub completion: xsk_tx_metadata_completion,
    }
}

cfg_if! {
    if #[cfg(feature = "extra_traits")] {
        #[allow(deprecated)]
        impl af_alg_iv {
            fn as_slice(&self) -> &[u8] {
                unsafe { ::core::slice::from_raw_parts(self.iv.as_ptr(), self.ivlen as usize) }
            }
        }

        #[allow(deprecated)]
        impl PartialEq for af_alg_iv {
            fn eq(&self, other: &af_alg_iv) -> bool {
                *self.as_slice() == *other.as_slice()
            }
        }

        #[allow(deprecated)]
        impl Eq for af_alg_iv {}

        #[allow(deprecated)]
        impl hash::Hash for af_alg_iv {
            fn hash<H: hash::Hasher>(&self, state: &mut H) {
                self.as_slice().hash(state);
            }
        }
    }
}

pub const POSIX_SPAWN_USEVFORK: c_int = 64;
pub const POSIX_SPAWN_SETSID: c_int = 128;

pub const F_SEAL_FUTURE_WRITE: c_int = 0x0010;
pub const F_SEAL_EXEC: c_int = 0x0020;

pub const IFF_LOWER_UP: c_int = 0x10000;
pub const IFF_DORMANT: c_int = 0x20000;
pub const IFF_ECHO: c_int = 0x40000;

// linux/fcntl.h
pub const AT_EXECVE_CHECK: c_int = 0x10000;

// linux/if_addr.h
pub const IFA_UNSPEC: c_ushort = 0;
pub const IFA_ADDRESS: c_ushort = 1;
pub const IFA_LOCAL: c_ushort = 2;
pub const IFA_LABEL: c_ushort = 3;
pub const IFA_BROADCAST: c_ushort = 4;
pub const IFA_ANYCAST: c_ushort = 5;
pub const IFA_CACHEINFO: c_ushort = 6;
pub const IFA_MULTICAST: c_ushort = 7;
pub const IFA_FLAGS: c_ushort = 8;

pub const IFA_F_SECONDARY: u32 = 0x01;
pub const IFA_F_TEMPORARY: u32 = 0x01;
pub const IFA_F_NODAD: u32 = 0x02;
pub const IFA_F_OPTIMISTIC: u32 = 0x04;
pub const IFA_F_DADFAILED: u32 = 0x08;
pub const IFA_F_HOMEADDRESS: u32 = 0x10;
pub const IFA_F_DEPRECATED: u32 = 0x20;
pub const IFA_F_TENTATIVE: u32 = 0x40;
pub const IFA_F_PERMANENT: u32 = 0x80;
pub const IFA_F_MANAGETEMPADDR: u32 = 0x100;
pub const IFA_F_NOPREFIXROUTE: u32 = 0x200;
pub const IFA_F_MCAUTOJOIN: u32 = 0x400;
pub const IFA_F_STABLE_PRIVACY: u32 = 0x800;

// linux/fs.h

// Flags for preadv2/pwritev2
pub const RWF_HIPRI: c_int = 0x00000001;
pub const RWF_DSYNC: c_int = 0x00000002;
pub const RWF_SYNC: c_int = 0x00000004;
pub const RWF_NOWAIT: c_int = 0x00000008;
pub const RWF_APPEND: c_int = 0x00000010;
pub const RWF_NOAPPEND: c_int = 0x00000020;
pub const RWF_ATOMIC: c_int = 0x00000040;
pub const RWF_DONTCACHE: c_int = 0x00000080;

// linux/if_link.h
pub const IFLA_UNSPEC: c_ushort = 0;
pub const IFLA_ADDRESS: c_ushort = 1;
pub const IFLA_BROADCAST: c_ushort = 2;
pub const IFLA_IFNAME: c_ushort = 3;
pub const IFLA_MTU: c_ushort = 4;
pub const IFLA_LINK: c_ushort = 5;
pub const IFLA_QDISC: c_ushort = 6;
pub const IFLA_STATS: c_ushort = 7;
pub const IFLA_COST: c_ushort = 8;
pub const IFLA_PRIORITY: c_ushort = 9;
pub const IFLA_MASTER: c_ushort = 10;
pub const IFLA_WIRELESS: c_ushort = 11;
pub const IFLA_PROTINFO: c_ushort = 12;
pub const IFLA_TXQLEN: c_ushort = 13;
pub const IFLA_MAP: c_ushort = 14;
pub const IFLA_WEIGHT: c_ushort = 15;
pub const IFLA_OPERSTATE: c_ushort = 16;
pub const IFLA_LINKMODE: c_ushort = 17;
pub const IFLA_LINKINFO: c_ushort = 18;
pub const IFLA_NET_NS_PID: c_ushort = 19;
pub const IFLA_IFALIAS: c_ushort = 20;
pub const IFLA_NUM_VF: c_ushort = 21;
pub const IFLA_VFINFO_LIST: c_ushort = 22;
pub const IFLA_STATS64: c_ushort = 23;
pub const IFLA_VF_PORTS: c_ushort = 24;
pub const IFLA_PORT_SELF: c_ushort = 25;
pub const IFLA_AF_SPEC: c_ushort = 26;
pub const IFLA_GROUP: c_ushort = 27;
pub const IFLA_NET_NS_FD: c_ushort = 28;
pub const IFLA_EXT_MASK: c_ushort = 29;
pub const IFLA_PROMISCUITY: c_ushort = 30;
pub const IFLA_NUM_TX_QUEUES: c_ushort = 31;
pub const IFLA_NUM_RX_QUEUES: c_ushort = 32;
pub const IFLA_CARRIER: c_ushort = 33;
pub const IFLA_PHYS_PORT_ID: c_ushort = 34;
pub const IFLA_CARRIER_CHANGES: c_ushort = 35;
pub const IFLA_PHYS_SWITCH_ID: c_ushort = 36;
pub const IFLA_LINK_NETNSID: c_ushort = 37;
pub const IFLA_PHYS_PORT_NAME: c_ushort = 38;
pub const IFLA_PROTO_DOWN: c_ushort = 39;
pub const IFLA_GSO_MAX_SEGS: c_ushort = 40;
pub const IFLA_GSO_MAX_SIZE: c_ushort = 41;
pub const IFLA_PAD: c_ushort = 42;
pub const IFLA_XDP: c_ushort = 43;
pub const IFLA_EVENT: c_ushort = 44;
pub const IFLA_NEW_NETNSID: c_ushort = 45;
pub const IFLA_IF_NETNSID: c_ushort = 46;
pub const IFLA_TARGET_NETNSID: c_ushort = IFLA_IF_NETNSID;
pub const IFLA_CARRIER_UP_COUNT: c_ushort = 47;
pub const IFLA_CARRIER_DOWN_COUNT: c_ushort = 48;
pub const IFLA_NEW_IFINDEX: c_ushort = 49;
pub const IFLA_MIN_MTU: c_ushort = 50;
pub const IFLA_MAX_MTU: c_ushort = 51;
pub const IFLA_PROP_LIST: c_ushort = 52;
pub const IFLA_ALT_IFNAME: c_ushort = 53;
pub const IFLA_PERM_ADDRESS: c_ushort = 54;
pub const IFLA_PROTO_DOWN_REASON: c_ushort = 55;
pub const IFLA_PARENT_DEV_NAME: c_ushort = 56;
pub const IFLA_PARENT_DEV_BUS_NAME: c_ushort = 57;
pub const IFLA_GRO_MAX_SIZE: c_ushort = 58;
pub const IFLA_TSO_MAX_SIZE: c_ushort = 59;
pub const IFLA_TSO_MAX_SEGS: c_ushort = 60;
pub const IFLA_ALLMULTI: c_ushort = 61;

pub const IFLA_INFO_UNSPEC: c_ushort = 0;
pub const IFLA_INFO_KIND: c_ushort = 1;
pub const IFLA_INFO_DATA: c_ushort = 2;
pub const IFLA_INFO_XSTATS: c_ushort = 3;
pub const IFLA_INFO_SLAVE_KIND: c_ushort = 4;
pub const IFLA_INFO_SLAVE_DATA: c_ushort = 5;

// Since Linux 3.1
pub const SEEK_DATA: c_int = 3;
pub const SEEK_HOLE: c_int = 4;

// linux/mempolicy.h
pub const MPOL_DEFAULT: c_int = 0;
pub const MPOL_PREFERRED: c_int = 1;
pub const MPOL_BIND: c_int = 2;
pub const MPOL_INTERLEAVE: c_int = 3;
pub const MPOL_LOCAL: c_int = 4;
pub const MPOL_F_NUMA_BALANCING: c_int = 1 << 13;
pub const MPOL_F_RELATIVE_NODES: c_int = 1 << 14;
pub const MPOL_F_STATIC_NODES: c_int = 1 << 15;

pub const PTHREAD_MUTEX_INITIALIZER: crate::pthread_mutex_t = crate::pthread_mutex_t {
    size: [0; crate::__SIZEOF_PTHREAD_MUTEX_T],
};
pub const PTHREAD_COND_INITIALIZER: crate::pthread_cond_t = crate::pthread_cond_t {
    size: [0; crate::__SIZEOF_PTHREAD_COND_T],
};
pub const PTHREAD_RWLOCK_INITIALIZER: crate::pthread_rwlock_t = crate::pthread_rwlock_t {
    size: [0; crate::__SIZEOF_PTHREAD_RWLOCK_T],
};

pub const RENAME_NOREPLACE: c_uint = 1;
pub const RENAME_EXCHANGE: c_uint = 2;
pub const RENAME_WHITEOUT: c_uint = 4;

pub const MSG_STAT: c_int = 11 | (crate::IPC_STAT & 0x100);
pub const MSG_INFO: c_int = 12;
pub const MSG_NOTIFICATION: c_int = 0x8000;

pub const MSG_NOERROR: c_int = 0o10000;
pub const MSG_EXCEPT: c_int = 0o20000;
pub const MSG_ZEROCOPY: c_int = 0x4000000;

pub const SEM_UNDO: c_int = 0x1000;

pub const GETPID: c_int = 11;
pub const GETVAL: c_int = 12;
pub const GETALL: c_int = 13;
pub const GETNCNT: c_int = 14;
pub const GETZCNT: c_int = 15;
pub const SETVAL: c_int = 16;
pub const SETALL: c_int = 17;
pub const SEM_STAT: c_int = 18 | (crate::IPC_STAT & 0x100);
pub const SEM_INFO: c_int = 19;
pub const SEM_STAT_ANY: c_int = 20 | (crate::IPC_STAT & 0x100);

pub const QFMT_VFS_OLD: c_int = 1;
pub const QFMT_VFS_V0: c_int = 2;
pub const QFMT_VFS_V1: c_int = 4;

pub const EFD_SEMAPHORE: c_int = 0x1;

pub const RB_AUTOBOOT: c_int = 0x01234567u32 as i32;
pub const RB_HALT_SYSTEM: c_int = 0xcdef0123u32 as i32;
pub const RB_ENABLE_CAD: c_int = 0x89abcdefu32 as i32;
pub const RB_DISABLE_CAD: c_int = 0x00000000u32 as i32;
pub const RB_POWER_OFF: c_int = 0x4321fedcu32 as i32;
pub const RB_SW_SUSPEND: c_int = 0xd000fce2u32 as i32;
pub const RB_KEXEC: c_int = 0x45584543u32 as i32;

pub const SYNC_FILE_RANGE_WAIT_BEFORE: c_uint = 1;
pub const SYNC_FILE_RANGE_WRITE: c_uint = 2;
pub const SYNC_FILE_RANGE_WAIT_AFTER: c_uint = 4;

pub const MREMAP_MAYMOVE: c_int = 1;
pub const MREMAP_FIXED: c_int = 2;
pub const MREMAP_DONTUNMAP: c_int = 4;

// linux/nsfs.h
const NSIO: c_uint = 0xb7;

pub const NS_GET_USERNS: Ioctl = _IO(NSIO, 0x1);
pub const NS_GET_PARENT: Ioctl = _IO(NSIO, 0x2);
pub const NS_GET_NSTYPE: Ioctl = _IO(NSIO, 0x3);
pub const NS_GET_OWNER_UID: Ioctl = _IO(NSIO, 0x4);

pub const NS_GET_MNTNS_ID: Ioctl = _IOR::<__u64>(NSIO, 0x5);

pub const NS_GET_PID_FROM_PIDNS: Ioctl = _IOR::<c_int>(NSIO, 0x6);
pub const NS_GET_TGID_FROM_PIDNS: Ioctl = _IOR::<c_int>(NSIO, 0x7);
pub const NS_GET_PID_IN_PIDNS: Ioctl = _IOR::<c_int>(NSIO, 0x8);
pub const NS_GET_TGID_IN_PIDNS: Ioctl = _IOR::<c_int>(NSIO, 0x9);

pub const MNT_NS_INFO_SIZE_VER0: Ioctl = 16;

pub const NS_MNT_GET_INFO: Ioctl = _IOR::<mnt_ns_info>(NSIO, 10);
pub const NS_MNT_GET_NEXT: Ioctl = _IOR::<mnt_ns_info>(NSIO, 11);
pub const NS_MNT_GET_PREV: Ioctl = _IOR::<mnt_ns_info>(NSIO, 12);

// linux/pidfd.h
pub const PIDFD_NONBLOCK: c_uint = O_NONBLOCK as c_uint;
pub const PIDFD_THREAD: c_uint = O_EXCL as c_uint;

pub const PIDFD_SIGNAL_THREAD: c_uint = 1 << 0;
pub const PIDFD_SIGNAL_THREAD_GROUP: c_uint = 1 << 1;
pub const PIDFD_SIGNAL_PROCESS_GROUP: c_uint = 1 << 2;

pub const PIDFD_INFO_PID: c_uint = 1 << 0;
pub const PIDFD_INFO_CREDS: c_uint = 1 << 1;
pub const PIDFD_INFO_CGROUPID: c_uint = 1 << 2;
pub const PIDFD_INFO_EXIT: c_uint = 1 << 3;

pub const PIDFD_INFO_SIZE_VER0: c_uint = 64;

const PIDFS_IOCTL_MAGIC: c_uint = 0xFF;
pub const PIDFD_GET_CGROUP_NAMESPACE: Ioctl = _IO(PIDFS_IOCTL_MAGIC, 1);
pub const PIDFD_GET_IPC_NAMESPACE: Ioctl = _IO(PIDFS_IOCTL_MAGIC, 2);
pub const PIDFD_GET_MNT_NAMESPACE: Ioctl = _IO(PIDFS_IOCTL_MAGIC, 3);
pub const PIDFD_GET_NET_NAMESPACE: Ioctl = _IO(PIDFS_IOCTL_MAGIC, 4);
pub const PIDFD_GET_PID_NAMESPACE: Ioctl = _IO(PIDFS_IOCTL_MAGIC, 5);
pub const PIDFD_GET_PID_FOR_CHILDREN_NAMESPACE: Ioctl = _IO(PIDFS_IOCTL_MAGIC, 6);
pub const PIDFD_GET_TIME_NAMESPACE: Ioctl = _IO(PIDFS_IOCTL_MAGIC, 7);
pub const PIDFD_GET_TIME_FOR_CHILDREN_NAMESPACE: Ioctl = _IO(PIDFS_IOCTL_MAGIC, 8);
pub const PIDFD_GET_USER_NAMESPACE: Ioctl = _IO(PIDFS_IOCTL_MAGIC, 9);
pub const PIDFD_GET_UTS_NAMESPACE: Ioctl = _IO(PIDFS_IOCTL_MAGIC, 10);
pub const PIDFD_GET_INFO: Ioctl = _IOWR::<pidfd_info>(PIDFS_IOCTL_MAGIC, 11);

pub const PR_SET_MDWE: c_int = 65;
pub const PR_GET_MDWE: c_int = 66;
pub const PR_MDWE_REFUSE_EXEC_GAIN: c_uint = 1 << 0;
pub const PR_MDWE_NO_INHERIT: c_uint = 1 << 1;

pub const GRND_NONBLOCK: c_uint = 0x0001;
pub const GRND_RANDOM: c_uint = 0x0002;
pub const GRND_INSECURE: c_uint = 0x0004;

// <linux/seccomp.h>
pub const SECCOMP_MODE_DISABLED: c_uint = 0;
pub const SECCOMP_MODE_STRICT: c_uint = 1;
pub const SECCOMP_MODE_FILTER: c_uint = 2;

pub const SECCOMP_SET_MODE_STRICT: c_uint = 0;
pub const SECCOMP_SET_MODE_FILTER: c_uint = 1;
pub const SECCOMP_GET_ACTION_AVAIL: c_uint = 2;
pub const SECCOMP_GET_NOTIF_SIZES: c_uint = 3;

pub const SECCOMP_FILTER_FLAG_TSYNC: c_ulong = 1 << 0;
pub const SECCOMP_FILTER_FLAG_LOG: c_ulong = 1 << 1;
pub const SECCOMP_FILTER_FLAG_SPEC_ALLOW: c_ulong = 1 << 2;
pub const SECCOMP_FILTER_FLAG_NEW_LISTENER: c_ulong = 1 << 3;
pub const SECCOMP_FILTER_FLAG_TSYNC_ESRCH: c_ulong = 1 << 4;
pub const SECCOMP_FILTER_FLAG_WAIT_KILLABLE_RECV: c_ulong = 1 << 5;

pub const SECCOMP_RET_KILL_PROCESS: c_uint = 0x80000000;
pub const SECCOMP_RET_KILL_THREAD: c_uint = 0x00000000;
pub const SECCOMP_RET_KILL: c_uint = SECCOMP_RET_KILL_THREAD;
pub const SECCOMP_RET_TRAP: c_uint = 0x00030000;
pub const SECCOMP_RET_ERRNO: c_uint = 0x00050000;
pub const SECCOMP_RET_USER_NOTIF: c_uint = 0x7fc00000;
pub const SECCOMP_RET_TRACE: c_uint = 0x7ff00000;
pub const SECCOMP_RET_LOG: c_uint = 0x7ffc0000;
pub const SECCOMP_RET_ALLOW: c_uint = 0x7fff0000;

pub const SECCOMP_RET_ACTION_FULL: c_uint = 0xffff0000;
pub const SECCOMP_RET_ACTION: c_uint = 0x7fff0000;
pub const SECCOMP_RET_DATA: c_uint = 0x0000ffff;

pub const SECCOMP_USER_NOTIF_FLAG_CONTINUE: c_ulong = 1;

pub const SECCOMP_ADDFD_FLAG_SETFD: c_ulong = 1;
pub const SECCOMP_ADDFD_FLAG_SEND: c_ulong = 2;

pub const TFD_CLOEXEC: c_int = O_CLOEXEC;
pub const TFD_NONBLOCK: c_int = O_NONBLOCK;
pub const TFD_TIMER_ABSTIME: c_int = 1;
pub const TFD_TIMER_CANCEL_ON_SET: c_int = 2;

pub const FALLOC_FL_KEEP_SIZE: c_int = 0x01;
pub const FALLOC_FL_PUNCH_HOLE: c_int = 0x02;
pub const FALLOC_FL_COLLAPSE_RANGE: c_int = 0x08;
pub const FALLOC_FL_ZERO_RANGE: c_int = 0x10;
pub const FALLOC_FL_INSERT_RANGE: c_int = 0x20;
pub const FALLOC_FL_UNSHARE_RANGE: c_int = 0x40;

#[deprecated(
    since = "0.2.55",
    note = "ENOATTR is not available on Linux; use ENODATA instead"
)]
pub const ENOATTR: c_int = crate::ENODATA;

pub const SO_ORIGINAL_DST: c_int = 80;

pub const IP_RECVFRAGSIZE: c_int = 25;

pub const IPV6_FLOWINFO: c_int = 11;
pub const IPV6_FLOWLABEL_MGR: c_int = 32;
pub const IPV6_FLOWINFO_SEND: c_int = 33;
pub const IPV6_RECVFRAGSIZE: c_int = 77;
pub const IPV6_FREEBIND: c_int = 78;
pub const IPV6_FLOWINFO_FLOWLABEL: c_int = 0x000fffff;
pub const IPV6_FLOWINFO_PRIORITY: c_int = 0x0ff00000;

// SO_MEMINFO offsets
pub const SK_MEMINFO_RMEM_ALLOC: c_int = 0;
pub const SK_MEMINFO_RCVBUF: c_int = 1;
pub const SK_MEMINFO_WMEM_ALLOC: c_int = 2;
pub const SK_MEMINFO_SNDBUF: c_int = 3;
pub const SK_MEMINFO_FWD_ALLOC: c_int = 4;
pub const SK_MEMINFO_WMEM_QUEUED: c_int = 5;
pub const SK_MEMINFO_OPTMEM: c_int = 6;
pub const SK_MEMINFO_BACKLOG: c_int = 7;
pub const SK_MEMINFO_DROPS: c_int = 8;

// linux/close_range.h
pub const CLOSE_RANGE_UNSHARE: c_uint = 1 << 1;
pub const CLOSE_RANGE_CLOEXEC: c_uint = 1 << 2;

// linux/filter.h
pub const SKF_AD_OFF: c_int = -0x1000;
pub const SKF_AD_PROTOCOL: c_int = 0;
pub const SKF_AD_PKTTYPE: c_int = 4;
pub const SKF_AD_IFINDEX: c_int = 8;
pub const SKF_AD_NLATTR: c_int = 12;
pub const SKF_AD_NLATTR_NEST: c_int = 16;
pub const SKF_AD_MARK: c_int = 20;
pub const SKF_AD_QUEUE: c_int = 24;
pub const SKF_AD_HATYPE: c_int = 28;
pub const SKF_AD_RXHASH: c_int = 32;
pub const SKF_AD_CPU: c_int = 36;
pub const SKF_AD_ALU_XOR_X: c_int = 40;
pub const SKF_AD_VLAN_TAG: c_int = 44;
pub const SKF_AD_VLAN_TAG_PRESENT: c_int = 48;
pub const SKF_AD_PAY_OFFSET: c_int = 52;
pub const SKF_AD_RANDOM: c_int = 56;
pub const SKF_AD_VLAN_TPID: c_int = 60;
pub const SKF_AD_MAX: c_int = 64;
pub const SKF_NET_OFF: c_int = -0x100000;
pub const SKF_LL_OFF: c_int = -0x200000;
pub const BPF_NET_OFF: c_int = SKF_NET_OFF;
pub const BPF_LL_OFF: c_int = SKF_LL_OFF;
pub const BPF_MEMWORDS: c_int = 16;
pub const BPF_MAXINSNS: c_int = 4096;

// linux/bpf_common.h
pub const BPF_LD: __u32 = 0x00;
pub const BPF_LDX: __u32 = 0x01;
pub const BPF_ST: __u32 = 0x02;
pub const BPF_STX: __u32 = 0x03;
pub const BPF_ALU: __u32 = 0x04;
pub const BPF_JMP: __u32 = 0x05;
pub const BPF_RET: __u32 = 0x06;
pub const BPF_MISC: __u32 = 0x07;
pub const BPF_W: __u32 = 0x00;
pub const BPF_H: __u32 = 0x08;
pub const BPF_B: __u32 = 0x10;
pub const BPF_IMM: __u32 = 0x00;
pub const BPF_ABS: __u32 = 0x20;
pub const BPF_IND: __u32 = 0x40;
pub const BPF_MEM: __u32 = 0x60;
pub const BPF_LEN: __u32 = 0x80;
pub const BPF_MSH: __u32 = 0xa0;
pub const BPF_ADD: __u32 = 0x00;
pub const BPF_SUB: __u32 = 0x10;
pub const BPF_MUL: __u32 = 0x20;
pub const BPF_DIV: __u32 = 0x30;
pub const BPF_OR: __u32 = 0x40;
pub const BPF_AND: __u32 = 0x50;
pub const BPF_LSH: __u32 = 0x60;
pub const BPF_RSH: __u32 = 0x70;
pub const BPF_NEG: __u32 = 0x80;
pub const BPF_MOD: __u32 = 0x90;
pub const BPF_XOR: __u32 = 0xa0;
pub const BPF_JA: __u32 = 0x00;
pub const BPF_JEQ: __u32 = 0x10;
pub const BPF_JGT: __u32 = 0x20;
pub const BPF_JGE: __u32 = 0x30;
pub const BPF_JSET: __u32 = 0x40;
pub const BPF_K: __u32 = 0x00;
pub const BPF_X: __u32 = 0x08;

// linux/filter.h

pub const BPF_A: __u32 = 0x10;
pub const BPF_TAX: __u32 = 0x00;
pub const BPF_TXA: __u32 = 0x80;

// linux/openat2.h
pub const RESOLVE_NO_XDEV: crate::__u64 = 0x01;
pub const RESOLVE_NO_MAGICLINKS: crate::__u64 = 0x02;
pub const RESOLVE_NO_SYMLINKS: crate::__u64 = 0x04;
pub const RESOLVE_BENEATH: crate::__u64 = 0x08;
pub const RESOLVE_IN_ROOT: crate::__u64 = 0x10;
pub const RESOLVE_CACHED: crate::__u64 = 0x20;

// linux/if_ether.h
pub const ETH_ALEN: c_int = 6;
pub const ETH_HLEN: c_int = 14;
pub const ETH_ZLEN: c_int = 60;
pub const ETH_DATA_LEN: c_int = 1500;
pub const ETH_FRAME_LEN: c_int = 1514;
pub const ETH_FCS_LEN: c_int = 4;

// These are the defined Ethernet Protocol ID's.
pub const ETH_P_LOOP: c_int = 0x0060;
pub const ETH_P_PUP: c_int = 0x0200;
pub const ETH_P_PUPAT: c_int = 0x0201;
pub const ETH_P_IP: c_int = 0x0800;
pub const ETH_P_X25: c_int = 0x0805;
pub const ETH_P_ARP: c_int = 0x0806;
pub const ETH_P_BPQ: c_int = 0x08FF;
pub const ETH_P_IEEEPUP: c_int = 0x0a00;
pub const ETH_P_IEEEPUPAT: c_int = 0x0a01;
pub const ETH_P_BATMAN: c_int = 0x4305;
pub const ETH_P_DEC: c_int = 0x6000;
pub const ETH_P_DNA_DL: c_int = 0x6001;
pub const ETH_P_DNA_RC: c_int = 0x6002;
pub const ETH_P_DNA_RT: c_int = 0x6003;
pub const ETH_P_LAT: c_int = 0x6004;
pub const ETH_P_DIAG: c_int = 0x6005;
pub const ETH_P_CUST: c_int = 0x6006;
pub const ETH_P_SCA: c_int = 0x6007;
pub const ETH_P_TEB: c_int = 0x6558;
pub const ETH_P_RARP: c_int = 0x8035;
pub const ETH_P_ATALK: c_int = 0x809B;
pub const ETH_P_AARP: c_int = 0x80F3;
pub const ETH_P_8021Q: c_int = 0x8100;
pub const ETH_P_IPX: c_int = 0x8137;
pub const ETH_P_IPV6: c_int = 0x86DD;
pub const ETH_P_PAUSE: c_int = 0x8808;
pub const ETH_P_SLOW: c_int = 0x8809;
pub const ETH_P_WCCP: c_int = 0x883E;
pub const ETH_P_MPLS_UC: c_int = 0x8847;
pub const ETH_P_MPLS_MC: c_int = 0x8848;
pub const ETH_P_ATMMPOA: c_int = 0x884c;
pub const ETH_P_PPP_DISC: c_int = 0x8863;
pub const ETH_P_PPP_SES: c_int = 0x8864;
pub const ETH_P_LINK_CTL: c_int = 0x886c;
pub const ETH_P_ATMFATE: c_int = 0x8884;
pub const ETH_P_PAE: c_int = 0x888E;
pub const ETH_P_AOE: c_int = 0x88A2;
pub const ETH_P_8021AD: c_int = 0x88A8;
pub const ETH_P_802_EX1: c_int = 0x88B5;
pub const ETH_P_TIPC: c_int = 0x88CA;
pub const ETH_P_MACSEC: c_int = 0x88E5;
pub const ETH_P_8021AH: c_int = 0x88E7;
pub const ETH_P_MVRP: c_int = 0x88F5;
pub const ETH_P_1588: c_int = 0x88F7;
pub const ETH_P_PRP: c_int = 0x88FB;
pub const ETH_P_FCOE: c_int = 0x8906;
pub const ETH_P_TDLS: c_int = 0x890D;
pub const ETH_P_FIP: c_int = 0x8914;
pub const ETH_P_80221: c_int = 0x8917;
pub const ETH_P_LOOPBACK: c_int = 0x9000;
pub const ETH_P_QINQ1: c_int = 0x9100;
pub const ETH_P_QINQ2: c_int = 0x9200;
pub const ETH_P_QINQ3: c_int = 0x9300;
pub const ETH_P_EDSA: c_int = 0xDADA;
pub const ETH_P_AF_IUCV: c_int = 0xFBFB;

pub const ETH_P_802_3_MIN: c_int = 0x0600;

// Non DIX types. Won't clash for 1500 types.
pub const ETH_P_802_3: c_int = 0x0001;
pub const ETH_P_AX25: c_int = 0x0002;
pub const ETH_P_ALL: c_int = 0x0003;
pub const ETH_P_802_2: c_int = 0x0004;
pub const ETH_P_SNAP: c_int = 0x0005;
pub const ETH_P_DDCMP: c_int = 0x0006;
pub const ETH_P_WAN_PPP: c_int = 0x0007;
pub const ETH_P_PPP_MP: c_int = 0x0008;
pub const ETH_P_LOCALTALK: c_int = 0x0009;
pub const ETH_P_CANFD: c_int = 0x000D;
pub const ETH_P_PPPTALK: c_int = 0x0010;
pub const ETH_P_TR_802_2: c_int = 0x0011;
pub const ETH_P_MOBITEX: c_int = 0x0015;
pub const ETH_P_CONTROL: c_int = 0x0016;
pub const ETH_P_IRDA: c_int = 0x0017;
pub const ETH_P_ECONET: c_int = 0x0018;
pub const ETH_P_HDLC: c_int = 0x0019;
pub const ETH_P_ARCNET: c_int = 0x001A;
pub const ETH_P_DSA: c_int = 0x001B;
pub const ETH_P_TRAILER: c_int = 0x001C;
pub const ETH_P_PHONET: c_int = 0x00F5;
pub const ETH_P_IEEE802154: c_int = 0x00F6;
pub const ETH_P_CAIF: c_int = 0x00F7;

// DIFF(main): changed to `c_short` in f62eb023ab
pub const POSIX_SPAWN_RESETIDS: c_int = 0x01;
pub const POSIX_SPAWN_SETPGROUP: c_int = 0x02;
pub const POSIX_SPAWN_SETSIGDEF: c_int = 0x04;
pub const POSIX_SPAWN_SETSIGMASK: c_int = 0x08;
pub const POSIX_SPAWN_SETSCHEDPARAM: c_int = 0x10;
pub const POSIX_SPAWN_SETSCHEDULER: c_int = 0x20;

// linux/netfilter/nfnetlink.h
pub const NFNLGRP_NONE: c_int = 0;
pub const NFNLGRP_CONNTRACK_NEW: c_int = 1;
pub const NFNLGRP_CONNTRACK_UPDATE: c_int = 2;
pub const NFNLGRP_CONNTRACK_DESTROY: c_int = 3;
pub const NFNLGRP_CONNTRACK_EXP_NEW: c_int = 4;
pub const NFNLGRP_CONNTRACK_EXP_UPDATE: c_int = 5;
pub const NFNLGRP_CONNTRACK_EXP_DESTROY: c_int = 6;
pub const NFNLGRP_NFTABLES: c_int = 7;
pub const NFNLGRP_ACCT_QUOTA: c_int = 8;
pub const NFNLGRP_NFTRACE: c_int = 9;

pub const NFNETLINK_V0: c_int = 0;

pub const NFNL_SUBSYS_NONE: c_int = 0;
pub const NFNL_SUBSYS_CTNETLINK: c_int = 1;
pub const NFNL_SUBSYS_CTNETLINK_EXP: c_int = 2;
pub const NFNL_SUBSYS_QUEUE: c_int = 3;
pub const NFNL_SUBSYS_ULOG: c_int = 4;
pub const NFNL_SUBSYS_OSF: c_int = 5;
pub const NFNL_SUBSYS_IPSET: c_int = 6;
pub const NFNL_SUBSYS_ACCT: c_int = 7;
pub const NFNL_SUBSYS_CTNETLINK_TIMEOUT: c_int = 8;
pub const NFNL_SUBSYS_CTHELPER: c_int = 9;
pub const NFNL_SUBSYS_NFTABLES: c_int = 10;
pub const NFNL_SUBSYS_NFT_COMPAT: c_int = 11;
pub const NFNL_SUBSYS_HOOK: c_int = 12;
pub const NFNL_SUBSYS_COUNT: c_int = 13;

pub const NFNL_MSG_BATCH_BEGIN: c_int = crate::NLMSG_MIN_TYPE;
pub const NFNL_MSG_BATCH_END: c_int = crate::NLMSG_MIN_TYPE + 1;

pub const NFNL_BATCH_UNSPEC: c_int = 0;
pub const NFNL_BATCH_GENID: c_int = 1;

// linux/netfilter/nfnetlink_log.h
pub const NFULNL_MSG_PACKET: c_int = 0;
pub const NFULNL_MSG_CONFIG: c_int = 1;

pub const NFULA_VLAN_UNSPEC: c_int = 0;
pub const NFULA_VLAN_PROTO: c_int = 1;
pub const NFULA_VLAN_TCI: c_int = 2;

pub const NFULA_UNSPEC: c_int = 0;
pub const NFULA_PACKET_HDR: c_int = 1;
pub const NFULA_MARK: c_int = 2;
pub const NFULA_TIMESTAMP: c_int = 3;
pub const NFULA_IFINDEX_INDEV: c_int = 4;
pub const NFULA_IFINDEX_OUTDEV: c_int = 5;
pub const NFULA_IFINDEX_PHYSINDEV: c_int = 6;
pub const NFULA_IFINDEX_PHYSOUTDEV: c_int = 7;
pub const NFULA_HWADDR: c_int = 8;
pub const NFULA_PAYLOAD: c_int = 9;
pub const NFULA_PREFIX: c_int = 10;
pub const NFULA_UID: c_int = 11;
pub const NFULA_SEQ: c_int = 12;
pub const NFULA_SEQ_GLOBAL: c_int = 13;
pub const NFULA_GID: c_int = 14;
pub const NFULA_HWTYPE: c_int = 15;
pub const NFULA_HWHEADER: c_int = 16;
pub const NFULA_HWLEN: c_int = 17;
pub const NFULA_CT: c_int = 18;
pub const NFULA_CT_INFO: c_int = 19;
pub const NFULA_VLAN: c_int = 20;
pub const NFULA_L2HDR: c_int = 21;

pub const NFULNL_CFG_CMD_NONE: c_int = 0;
pub const NFULNL_CFG_CMD_BIND: c_int = 1;
pub const NFULNL_CFG_CMD_UNBIND: c_int = 2;
pub const NFULNL_CFG_CMD_PF_BIND: c_int = 3;
pub const NFULNL_CFG_CMD_PF_UNBIND: c_int = 4;

pub const NFULA_CFG_UNSPEC: c_int = 0;
pub const NFULA_CFG_CMD: c_int = 1;
pub const NFULA_CFG_MODE: c_int = 2;
pub const NFULA_CFG_NLBUFSIZ: c_int = 3;
pub const NFULA_CFG_TIMEOUT: c_int = 4;
pub const NFULA_CFG_QTHRESH: c_int = 5;
pub const NFULA_CFG_FLAGS: c_int = 6;

pub const NFULNL_COPY_NONE: c_int = 0x00;
pub const NFULNL_COPY_META: c_int = 0x01;
pub const NFULNL_COPY_PACKET: c_int = 0x02;

pub const NFULNL_CFG_F_SEQ: c_int = 0x0001;
pub const NFULNL_CFG_F_SEQ_GLOBAL: c_int = 0x0002;
pub const NFULNL_CFG_F_CONNTRACK: c_int = 0x0004;

// linux/netfilter/nfnetlink_queue.h
pub const NFQNL_MSG_PACKET: c_int = 0;
pub const NFQNL_MSG_VERDICT: c_int = 1;
pub const NFQNL_MSG_CONFIG: c_int = 2;
pub const NFQNL_MSG_VERDICT_BATCH: c_int = 3;

pub const NFQA_UNSPEC: c_int = 0;
pub const NFQA_PACKET_HDR: c_int = 1;
pub const NFQA_VERDICT_HDR: c_int = 2;
pub const NFQA_MARK: c_int = 3;
pub const NFQA_TIMESTAMP: c_int = 4;
pub const NFQA_IFINDEX_INDEV: c_int = 5;
pub const NFQA_IFINDEX_OUTDEV: c_int = 6;
pub const NFQA_IFINDEX_PHYSINDEV: c_int = 7;
pub const NFQA_IFINDEX_PHYSOUTDEV: c_int = 8;
pub const NFQA_HWADDR: c_int = 9;
pub const NFQA_PAYLOAD: c_int = 10;
pub const NFQA_CT: c_int = 11;
pub const NFQA_CT_INFO: c_int = 12;
pub const NFQA_CAP_LEN: c_int = 13;
pub const NFQA_SKB_INFO: c_int = 14;
pub const NFQA_EXP: c_int = 15;
pub const NFQA_UID: c_int = 16;
pub const NFQA_GID: c_int = 17;
pub const NFQA_SECCTX: c_int = 18;
pub const NFQA_VLAN: c_int = 19;
pub const NFQA_L2HDR: c_int = 20;
pub const NFQA_PRIORITY: c_int = 21;

pub const NFQA_VLAN_UNSPEC: c_int = 0;
pub const NFQA_VLAN_PROTO: c_int = 1;
pub const NFQA_VLAN_TCI: c_int = 2;

pub const NFQNL_CFG_CMD_NONE: c_int = 0;
pub const NFQNL_CFG_CMD_BIND: c_int = 1;
pub const NFQNL_CFG_CMD_UNBIND: c_int = 2;
pub const NFQNL_CFG_CMD_PF_BIND: c_int = 3;
pub const NFQNL_CFG_CMD_PF_UNBIND: c_int = 4;

pub const NFQNL_COPY_NONE: c_int = 0;
pub const NFQNL_COPY_META: c_int = 1;
pub const NFQNL_COPY_PACKET: c_int = 2;

pub const NFQA_CFG_UNSPEC: c_int = 0;
pub const NFQA_CFG_CMD: c_int = 1;
pub const NFQA_CFG_PARAMS: c_int = 2;
pub const NFQA_CFG_QUEUE_MAXLEN: c_int = 3;
pub const NFQA_CFG_MASK: c_int = 4;
pub const NFQA_CFG_FLAGS: c_int = 5;

pub const NFQA_CFG_F_FAIL_OPEN: c_int = 0x0001;
pub const NFQA_CFG_F_CONNTRACK: c_int = 0x0002;
pub const NFQA_CFG_F_GSO: c_int = 0x0004;
pub const NFQA_CFG_F_UID_GID: c_int = 0x0008;
pub const NFQA_CFG_F_SECCTX: c_int = 0x0010;
pub const NFQA_CFG_F_MAX: c_int = 0x0020;

pub const NFQA_SKB_CSUMNOTREADY: c_int = 0x0001;
pub const NFQA_SKB_GSO: c_int = 0x0002;
pub const NFQA_SKB_CSUM_NOTVERIFIED: c_int = 0x0004;

// linux/genetlink.h

pub const GENL_NAMSIZ: c_int = 16;

pub const GENL_MIN_ID: c_int = crate::NLMSG_MIN_TYPE;
pub const GENL_MAX_ID: c_int = 1023;

pub const GENL_ADMIN_PERM: c_int = 0x01;
pub const GENL_CMD_CAP_DO: c_int = 0x02;
pub const GENL_CMD_CAP_DUMP: c_int = 0x04;
pub const GENL_CMD_CAP_HASPOL: c_int = 0x08;

pub const GENL_ID_CTRL: c_int = crate::NLMSG_MIN_TYPE;

pub const CTRL_CMD_UNSPEC: c_int = 0;
pub const CTRL_CMD_NEWFAMILY: c_int = 1;
pub const CTRL_CMD_DELFAMILY: c_int = 2;
pub const CTRL_CMD_GETFAMILY: c_int = 3;
pub const CTRL_CMD_NEWOPS: c_int = 4;
pub const CTRL_CMD_DELOPS: c_int = 5;
pub const CTRL_CMD_GETOPS: c_int = 6;
pub const CTRL_CMD_NEWMCAST_GRP: c_int = 7;
pub const CTRL_CMD_DELMCAST_GRP: c_int = 8;
pub const CTRL_CMD_GETMCAST_GRP: c_int = 9;

pub const CTRL_ATTR_UNSPEC: c_int = 0;
pub const CTRL_ATTR_FAMILY_ID: c_int = 1;
pub const CTRL_ATTR_FAMILY_NAME: c_int = 2;
pub const CTRL_ATTR_VERSION: c_int = 3;
pub const CTRL_ATTR_HDRSIZE: c_int = 4;
pub const CTRL_ATTR_MAXATTR: c_int = 5;
pub const CTRL_ATTR_OPS: c_int = 6;
pub const CTRL_ATTR_MCAST_GROUPS: c_int = 7;

pub const CTRL_ATTR_OP_UNSPEC: c_int = 0;
pub const CTRL_ATTR_OP_ID: c_int = 1;
pub const CTRL_ATTR_OP_FLAGS: c_int = 2;

pub const CTRL_ATTR_MCAST_GRP_UNSPEC: c_int = 0;
pub const CTRL_ATTR_MCAST_GRP_NAME: c_int = 1;
pub const CTRL_ATTR_MCAST_GRP_ID: c_int = 2;

pub const PACKET_FANOUT: c_int = 18;
pub const PACKET_TX_HAS_OFF: c_int = 19;
pub const PACKET_QDISC_BYPASS: c_int = 20;
pub const PACKET_ROLLOVER_STATS: c_int = 21;
pub const PACKET_FANOUT_DATA: c_int = 22;
pub const PACKET_IGNORE_OUTGOING: c_int = 23;
pub const PACKET_VNET_HDR_SZ: c_int = 24;

pub const PACKET_FANOUT_HASH: c_uint = 0;
pub const PACKET_FANOUT_LB: c_uint = 1;
pub const PACKET_FANOUT_CPU: c_uint = 2;
pub const PACKET_FANOUT_ROLLOVER: c_uint = 3;
pub const PACKET_FANOUT_RND: c_uint = 4;
pub const PACKET_FANOUT_QM: c_uint = 5;
pub const PACKET_FANOUT_CBPF: c_uint = 6;
pub const PACKET_FANOUT_EBPF: c_uint = 7;
pub const PACKET_FANOUT_FLAG_ROLLOVER: c_uint = 0x1000;
pub const PACKET_FANOUT_FLAG_UNIQUEID: c_uint = 0x2000;
pub const PACKET_FANOUT_FLAG_IGNORE_OUTGOING: c_uint = 0x4000;
pub const PACKET_FANOUT_FLAG_DEFRAG: c_uint = 0x8000;

pub const TP_STATUS_KERNEL: __u32 = 0;
pub const TP_STATUS_USER: __u32 = 1 << 0;
pub const TP_STATUS_COPY: __u32 = 1 << 1;
pub const TP_STATUS_LOSING: __u32 = 1 << 2;
pub const TP_STATUS_CSUMNOTREADY: __u32 = 1 << 3;
pub const TP_STATUS_VLAN_VALID: __u32 = 1 << 4;
pub const TP_STATUS_BLK_TMO: __u32 = 1 << 5;
pub const TP_STATUS_VLAN_TPID_VALID: __u32 = 1 << 6;
pub const TP_STATUS_CSUM_VALID: __u32 = 1 << 7;

pub const TP_STATUS_AVAILABLE: __u32 = 0;
pub const TP_STATUS_SEND_REQUEST: __u32 = 1 << 0;
pub const TP_STATUS_SENDING: __u32 = 1 << 1;
pub const TP_STATUS_WRONG_FORMAT: __u32 = 1 << 2;

pub const TP_STATUS_TS_SOFTWARE: __u32 = 1 << 29;
pub const TP_STATUS_TS_SYS_HARDWARE: __u32 = 1 << 30;
pub const TP_STATUS_TS_RAW_HARDWARE: __u32 = 1 << 31;

pub const TP_FT_REQ_FILL_RXHASH: __u32 = 1;

pub const TPACKET_ALIGNMENT: usize = 16;

pub const TPACKET_HDRLEN: usize = ((size_of::<crate::tpacket_hdr>() + TPACKET_ALIGNMENT - 1)
    & !(TPACKET_ALIGNMENT - 1))
    + size_of::<crate::sockaddr_ll>();
pub const TPACKET2_HDRLEN: usize = ((size_of::<crate::tpacket2_hdr>() + TPACKET_ALIGNMENT - 1)
    & !(TPACKET_ALIGNMENT - 1))
    + size_of::<crate::sockaddr_ll>();
pub const TPACKET3_HDRLEN: usize = ((size_of::<crate::tpacket3_hdr>() + TPACKET_ALIGNMENT - 1)
    & !(TPACKET_ALIGNMENT - 1))
    + size_of::<crate::sockaddr_ll>();

// linux/netfilter.h
pub const NF_DROP: c_int = 0;
pub const NF_ACCEPT: c_int = 1;
pub const NF_STOLEN: c_int = 2;
pub const NF_QUEUE: c_int = 3;
pub const NF_REPEAT: c_int = 4;
pub const NF_STOP: c_int = 5;
pub const NF_MAX_VERDICT: c_int = NF_STOP;

pub const NF_VERDICT_MASK: c_int = 0x000000ff;
pub const NF_VERDICT_FLAG_QUEUE_BYPASS: c_int = 0x00008000;

pub const NF_VERDICT_QMASK: c_int = 0xffff0000;
pub const NF_VERDICT_QBITS: c_int = 16;

pub const NF_VERDICT_BITS: c_int = 16;

pub const NF_INET_PRE_ROUTING: c_int = 0;
pub const NF_INET_LOCAL_IN: c_int = 1;
pub const NF_INET_FORWARD: c_int = 2;
pub const NF_INET_LOCAL_OUT: c_int = 3;
pub const NF_INET_POST_ROUTING: c_int = 4;
pub const NF_INET_NUMHOOKS: c_int = 5;
pub const NF_INET_INGRESS: c_int = NF_INET_NUMHOOKS;

pub const NF_NETDEV_INGRESS: c_int = 0;
pub const NF_NETDEV_EGRESS: c_int = 1;
pub const NF_NETDEV_NUMHOOKS: c_int = 2;

// Some NFPROTO are not compatible with musl and are defined in submodules.
pub const NFPROTO_UNSPEC: c_int = 0;
pub const NFPROTO_INET: c_int = 1;
pub const NFPROTO_IPV4: c_int = 2;
pub const NFPROTO_ARP: c_int = 3;
pub const NFPROTO_NETDEV: c_int = 5;
pub const NFPROTO_BRIDGE: c_int = 7;
pub const NFPROTO_IPV6: c_int = 10;
pub const NFPROTO_DECNET: c_int = 12;
pub const NFPROTO_NUMPROTO: c_int = 13;

// linux/netfilter_arp.h
pub const NF_ARP: c_int = 0;
pub const NF_ARP_IN: c_int = 0;
pub const NF_ARP_OUT: c_int = 1;
pub const NF_ARP_FORWARD: c_int = 2;
pub const NF_ARP_NUMHOOKS: c_int = 3;

// linux/netfilter_bridge.h
pub const NF_BR_PRE_ROUTING: c_int = 0;
pub const NF_BR_LOCAL_IN: c_int = 1;
pub const NF_BR_FORWARD: c_int = 2;
pub const NF_BR_LOCAL_OUT: c_int = 3;
pub const NF_BR_POST_ROUTING: c_int = 4;
pub const NF_BR_BROUTING: c_int = 5;
pub const NF_BR_NUMHOOKS: c_int = 6;

pub const NF_BR_PRI_FIRST: c_int = crate::INT_MIN;
pub const NF_BR_PRI_NAT_DST_BRIDGED: c_int = -300;
pub const NF_BR_PRI_FILTER_BRIDGED: c_int = -200;
pub const NF_BR_PRI_BRNF: c_int = 0;
pub const NF_BR_PRI_NAT_DST_OTHER: c_int = 100;
pub const NF_BR_PRI_FILTER_OTHER: c_int = 200;
pub const NF_BR_PRI_NAT_SRC: c_int = 300;
pub const NF_BR_PRI_LAST: c_int = crate::INT_MAX;

// linux/netfilter_ipv4.h
pub const NF_IP_PRE_ROUTING: c_int = 0;
pub const NF_IP_LOCAL_IN: c_int = 1;
pub const NF_IP_FORWARD: c_int = 2;
pub const NF_IP_LOCAL_OUT: c_int = 3;
pub const NF_IP_POST_ROUTING: c_int = 4;
pub const NF_IP_NUMHOOKS: c_int = 5;

pub const NF_IP_PRI_FIRST: c_int = crate::INT_MIN;
pub const NF_IP_PRI_RAW_BEFORE_DEFRAG: c_int = -450;
pub const NF_IP_PRI_CONNTRACK_DEFRAG: c_int = -400;
pub const NF_IP_PRI_RAW: c_int = -300;
pub const NF_IP_PRI_SELINUX_FIRST: c_int = -225;
pub const NF_IP_PRI_CONNTRACK: c_int = -200;
pub const NF_IP_PRI_MANGLE: c_int = -150;
pub const NF_IP_PRI_NAT_DST: c_int = -100;
pub const NF_IP_PRI_FILTER: c_int = 0;
pub const NF_IP_PRI_SECURITY: c_int = 50;
pub const NF_IP_PRI_NAT_SRC: c_int = 100;
pub const NF_IP_PRI_SELINUX_LAST: c_int = 225;
pub const NF_IP_PRI_CONNTRACK_HELPER: c_int = 300;
pub const NF_IP_PRI_CONNTRACK_CONFIRM: c_int = crate::INT_MAX;
pub const NF_IP_PRI_LAST: c_int = crate::INT_MAX;

// linux/netfilter_ipv6.h
pub const NF_IP6_PRE_ROUTING: c_int = 0;
pub const NF_IP6_LOCAL_IN: c_int = 1;
pub const NF_IP6_FORWARD: c_int = 2;
pub const NF_IP6_LOCAL_OUT: c_int = 3;
pub const NF_IP6_POST_ROUTING: c_int = 4;
pub const NF_IP6_NUMHOOKS: c_int = 5;

pub const NF_IP6_PRI_FIRST: c_int = crate::INT_MIN;
pub const NF_IP6_PRI_RAW_BEFORE_DEFRAG: c_int = -450;
pub const NF_IP6_PRI_CONNTRACK_DEFRAG: c_int = -400;
pub const NF_IP6_PRI_RAW: c_int = -300;
pub const NF_IP6_PRI_SELINUX_FIRST: c_int = -225;
pub const NF_IP6_PRI_CONNTRACK: c_int = -200;
pub const NF_IP6_PRI_MANGLE: c_int = -150;
pub const NF_IP6_PRI_NAT_DST: c_int = -100;
pub const NF_IP6_PRI_FILTER: c_int = 0;
pub const NF_IP6_PRI_SECURITY: c_int = 50;
pub const NF_IP6_PRI_NAT_SRC: c_int = 100;
pub const NF_IP6_PRI_SELINUX_LAST: c_int = 225;
pub const NF_IP6_PRI_CONNTRACK_HELPER: c_int = 300;
pub const NF_IP6_PRI_LAST: c_int = crate::INT_MAX;

// linux/netfilter_ipv6/ip6_tables.h
pub const IP6T_SO_ORIGINAL_DST: c_int = 80;

pub const SIOCSHWTSTAMP: c_ulong = 0x000089b0;
pub const SIOCGHWTSTAMP: c_ulong = 0x000089b1;

// wireless.h
pub const WIRELESS_EXT: c_ulong = 0x16;

pub const SIOCSIWCOMMIT: c_ulong = 0x8B00;
pub const SIOCGIWNAME: c_ulong = 0x8B01;

pub const SIOCSIWNWID: c_ulong = 0x8B02;
pub const SIOCGIWNWID: c_ulong = 0x8B03;
pub const SIOCSIWFREQ: c_ulong = 0x8B04;
pub const SIOCGIWFREQ: c_ulong = 0x8B05;
pub const SIOCSIWMODE: c_ulong = 0x8B06;
pub const SIOCGIWMODE: c_ulong = 0x8B07;
pub const SIOCSIWSENS: c_ulong = 0x8B08;
pub const SIOCGIWSENS: c_ulong = 0x8B09;

pub const SIOCSIWRANGE: c_ulong = 0x8B0A;
pub const SIOCGIWRANGE: c_ulong = 0x8B0B;
pub const SIOCSIWPRIV: c_ulong = 0x8B0C;
pub const SIOCGIWPRIV: c_ulong = 0x8B0D;
pub const SIOCSIWSTATS: c_ulong = 0x8B0E;
pub const SIOCGIWSTATS: c_ulong = 0x8B0F;

pub const SIOCSIWSPY: c_ulong = 0x8B10;
pub const SIOCGIWSPY: c_ulong = 0x8B11;
pub const SIOCSIWTHRSPY: c_ulong = 0x8B12;
pub const SIOCGIWTHRSPY: c_ulong = 0x8B13;

pub const SIOCSIWAP: c_ulong = 0x8B14;
pub const SIOCGIWAP: c_ulong = 0x8B15;
pub const SIOCGIWAPLIST: c_ulong = 0x8B17;
pub const SIOCSIWSCAN: c_ulong = 0x8B18;
pub const SIOCGIWSCAN: c_ulong = 0x8B19;

pub const SIOCSIWESSID: c_ulong = 0x8B1A;
pub const SIOCGIWESSID: c_ulong = 0x8B1B;
pub const SIOCSIWNICKN: c_ulong = 0x8B1C;
pub const SIOCGIWNICKN: c_ulong = 0x8B1D;

pub const SIOCSIWRATE: c_ulong = 0x8B20;
pub const SIOCGIWRATE: c_ulong = 0x8B21;
pub const SIOCSIWRTS: c_ulong = 0x8B22;
pub const SIOCGIWRTS: c_ulong = 0x8B23;
pub const SIOCSIWFRAG: c_ulong = 0x8B24;
pub const SIOCGIWFRAG: c_ulong = 0x8B25;
pub const SIOCSIWTXPOW: c_ulong = 0x8B26;
pub const SIOCGIWTXPOW: c_ulong = 0x8B27;
pub const SIOCSIWRETRY: c_ulong = 0x8B28;
pub const SIOCGIWRETRY: c_ulong = 0x8B29;

pub const SIOCSIWENCODE: c_ulong = 0x8B2A;
pub const SIOCGIWENCODE: c_ulong = 0x8B2B;

pub const SIOCSIWPOWER: c_ulong = 0x8B2C;
pub const SIOCGIWPOWER: c_ulong = 0x8B2D;

pub const SIOCSIWGENIE: c_ulong = 0x8B30;
pub const SIOCGIWGENIE: c_ulong = 0x8B31;

pub const SIOCSIWMLME: c_ulong = 0x8B16;

pub const SIOCSIWAUTH: c_ulong = 0x8B32;
pub const SIOCGIWAUTH: c_ulong = 0x8B33;

pub const SIOCSIWENCODEEXT: c_ulong = 0x8B34;
pub const SIOCGIWENCODEEXT: c_ulong = 0x8B35;

pub const SIOCSIWPMKSA: c_ulong = 0x8B36;

pub const SIOCIWFIRSTPRIV: c_ulong = 0x8BE0;
pub const SIOCIWLASTPRIV: c_ulong = 0x8BFF;

pub const SIOCIWFIRST: c_ulong = 0x8B00;
pub const SIOCIWLAST: c_ulong = SIOCIWLASTPRIV;

pub const IWEVTXDROP: c_ulong = 0x8C00;
pub const IWEVQUAL: c_ulong = 0x8C01;
pub const IWEVCUSTOM: c_ulong = 0x8C02;
pub const IWEVREGISTERED: c_ulong = 0x8C03;
pub const IWEVEXPIRED: c_ulong = 0x8C04;
pub const IWEVGENIE: c_ulong = 0x8C05;
pub const IWEVMICHAELMICFAILURE: c_ulong = 0x8C06;
pub const IWEVASSOCREQIE: c_ulong = 0x8C07;
pub const IWEVASSOCRESPIE: c_ulong = 0x8C08;
pub const IWEVPMKIDCAND: c_ulong = 0x8C09;
pub const IWEVFIRST: c_ulong = 0x8C00;

pub const IW_PRIV_TYPE_MASK: c_ulong = 0x7000;
pub const IW_PRIV_TYPE_NONE: c_ulong = 0x0000;
pub const IW_PRIV_TYPE_BYTE: c_ulong = 0x1000;
pub const IW_PRIV_TYPE_CHAR: c_ulong = 0x2000;
pub const IW_PRIV_TYPE_INT: c_ulong = 0x4000;
pub const IW_PRIV_TYPE_FLOAT: c_ulong = 0x5000;
pub const IW_PRIV_TYPE_ADDR: c_ulong = 0x6000;

pub const IW_PRIV_SIZE_FIXED: c_ulong = 0x0800;

pub const IW_PRIV_SIZE_MASK: c_ulong = 0x07FF;

pub const IW_MAX_FREQUENCIES: usize = 32;
pub const IW_MAX_BITRATES: usize = 32;
pub const IW_MAX_TXPOWER: usize = 8;
pub const IW_MAX_SPY: usize = 8;
pub const IW_MAX_AP: usize = 64;
pub const IW_ESSID_MAX_SIZE: usize = 32;

pub const IW_MODE_AUTO: usize = 0;
pub const IW_MODE_ADHOC: usize = 1;
pub const IW_MODE_INFRA: usize = 2;
pub const IW_MODE_MASTER: usize = 3;
pub const IW_MODE_REPEAT: usize = 4;
pub const IW_MODE_SECOND: usize = 5;
pub const IW_MODE_MONITOR: usize = 6;
pub const IW_MODE_MESH: usize = 7;

pub const IW_QUAL_QUAL_UPDATED: c_ulong = 0x01;
pub const IW_QUAL_LEVEL_UPDATED: c_ulong = 0x02;
pub const IW_QUAL_NOISE_UPDATED: c_ulong = 0x04;
pub const IW_QUAL_ALL_UPDATED: c_ulong = 0x07;
pub const IW_QUAL_DBM: c_ulong = 0x08;
pub const IW_QUAL_QUAL_INVALID: c_ulong = 0x10;
pub const IW_QUAL_LEVEL_INVALID: c_ulong = 0x20;
pub const IW_QUAL_NOISE_INVALID: c_ulong = 0x40;
pub const IW_QUAL_RCPI: c_ulong = 0x80;
pub const IW_QUAL_ALL_INVALID: c_ulong = 0x70;

pub const IW_FREQ_AUTO: c_ulong = 0x00;
pub const IW_FREQ_FIXED: c_ulong = 0x01;

pub const IW_MAX_ENCODING_SIZES: usize = 8;
pub const IW_ENCODING_TOKEN_MAX: usize = 64;

pub const IW_ENCODE_INDEX: c_ulong = 0x00FF;
pub const IW_ENCODE_FLAGS: c_ulong = 0xFF00;
pub const IW_ENCODE_MODE: c_ulong = 0xF000;
pub const IW_ENCODE_DISABLED: c_ulong = 0x8000;
pub const IW_ENCODE_ENABLED: c_ulong = 0x0000;
pub const IW_ENCODE_RESTRICTED: c_ulong = 0x4000;
pub const IW_ENCODE_OPEN: c_ulong = 0x2000;
pub const IW_ENCODE_NOKEY: c_ulong = 0x0800;
pub const IW_ENCODE_TEMP: c_ulong = 0x0400;

pub const IW_POWER_ON: c_ulong = 0x0000;
pub const IW_POWER_TYPE: c_ulong = 0xF000;
pub const IW_POWER_PERIOD: c_ulong = 0x1000;
pub const IW_POWER_TIMEOUT: c_ulong = 0x2000;
pub const IW_POWER_MODE: c_ulong = 0x0F00;
pub const IW_POWER_UNICAST_R: c_ulong = 0x0100;
pub const IW_POWER_MULTICAST_R: c_ulong = 0x0200;
pub const IW_POWER_ALL_R: c_ulong = 0x0300;
pub const IW_POWER_FORCE_S: c_ulong = 0x0400;
pub const IW_POWER_REPEATER: c_ulong = 0x0800;
pub const IW_POWER_MODIFIER: c_ulong = 0x000F;
pub const IW_POWER_MIN: c_ulong = 0x0001;
pub const IW_POWER_MAX: c_ulong = 0x0002;
pub const IW_POWER_RELATIVE: c_ulong = 0x0004;

pub const IW_TXPOW_TYPE: c_ulong = 0x00FF;
pub const IW_TXPOW_DBM: c_ulong = 0x0000;
pub const IW_TXPOW_MWATT: c_ulong = 0x0001;
pub const IW_TXPOW_RELATIVE: c_ulong = 0x0002;
pub const IW_TXPOW_RANGE: c_ulong = 0x1000;

pub const IW_RETRY_ON: c_ulong = 0x0000;
pub const IW_RETRY_TYPE: c_ulong = 0xF000;
pub const IW_RETRY_LIMIT: c_ulong = 0x1000;
pub const IW_RETRY_LIFETIME: c_ulong = 0x2000;
pub const IW_RETRY_MODIFIER: c_ulong = 0x00FF;
pub const IW_RETRY_MIN: c_ulong = 0x0001;
pub const IW_RETRY_MAX: c_ulong = 0x0002;
pub const IW_RETRY_RELATIVE: c_ulong = 0x0004;
pub const IW_RETRY_SHORT: c_ulong = 0x0010;
pub const IW_RETRY_LONG: c_ulong = 0x0020;

pub const IW_SCAN_DEFAULT: c_ulong = 0x0000;
pub const IW_SCAN_ALL_ESSID: c_ulong = 0x0001;
pub const IW_SCAN_THIS_ESSID: c_ulong = 0x0002;
pub const IW_SCAN_ALL_FREQ: c_ulong = 0x0004;
pub const IW_SCAN_THIS_FREQ: c_ulong = 0x0008;
pub const IW_SCAN_ALL_MODE: c_ulong = 0x0010;
pub const IW_SCAN_THIS_MODE: c_ulong = 0x0020;
pub const IW_SCAN_ALL_RATE: c_ulong = 0x0040;
pub const IW_SCAN_THIS_RATE: c_ulong = 0x0080;

pub const IW_SCAN_TYPE_ACTIVE: usize = 0;
pub const IW_SCAN_TYPE_PASSIVE: usize = 1;

pub const IW_SCAN_MAX_DATA: usize = 4096;

pub const IW_SCAN_CAPA_NONE: c_ulong = 0x00;
pub const IW_SCAN_CAPA_ESSID: c_ulong = 0x01;
pub const IW_SCAN_CAPA_BSSID: c_ulong = 0x02;
pub const IW_SCAN_CAPA_CHANNEL: c_ulong = 0x04;
pub const IW_SCAN_CAPA_MODE: c_ulong = 0x08;
pub const IW_SCAN_CAPA_RATE: c_ulong = 0x10;
pub const IW_SCAN_CAPA_TYPE: c_ulong = 0x20;
pub const IW_SCAN_CAPA_TIME: c_ulong = 0x40;

pub const IW_CUSTOM_MAX: c_ulong = 256;

pub const IW_GENERIC_IE_MAX: c_ulong = 1024;

pub const IW_MLME_DEAUTH: c_ulong = 0;
pub const IW_MLME_DISASSOC: c_ulong = 1;
pub const IW_MLME_AUTH: c_ulong = 2;
pub const IW_MLME_ASSOC: c_ulong = 3;

pub const IW_AUTH_INDEX: c_ulong = 0x0FFF;
pub const IW_AUTH_FLAGS: c_ulong = 0xF000;

pub const IW_AUTH_WPA_VERSION: usize = 0;
pub const IW_AUTH_CIPHER_PAIRWISE: usize = 1;
pub const IW_AUTH_CIPHER_GROUP: usize = 2;
pub const IW_AUTH_KEY_MGMT: usize = 3;
pub const IW_AUTH_TKIP_COUNTERMEASURES: usize = 4;
pub const IW_AUTH_DROP_UNENCRYPTED: usize = 5;
pub const IW_AUTH_80211_AUTH_ALG: usize = 6;
pub const IW_AUTH_WPA_ENABLED: usize = 7;
pub const IW_AUTH_RX_UNENCRYPTED_EAPOL: usize = 8;
pub const IW_AUTH_ROAMING_CONTROL: usize = 9;
pub const IW_AUTH_PRIVACY_INVOKED: usize = 10;
pub const IW_AUTH_CIPHER_GROUP_MGMT: usize = 11;
pub const IW_AUTH_MFP: usize = 12;

pub const IW_AUTH_WPA_VERSION_DISABLED: c_ulong = 0x00000001;
pub const IW_AUTH_WPA_VERSION_WPA: c_ulong = 0x00000002;
pub const IW_AUTH_WPA_VERSION_WPA2: c_ulong = 0x00000004;

pub const IW_AUTH_CIPHER_NONE: c_ulong = 0x00000001;
pub const IW_AUTH_CIPHER_WEP40: c_ulong = 0x00000002;
pub const IW_AUTH_CIPHER_TKIP: c_ulong = 0x00000004;
pub const IW_AUTH_CIPHER_CCMP: c_ulong = 0x00000008;
pub const IW_AUTH_CIPHER_WEP104: c_ulong = 0x00000010;
pub const IW_AUTH_CIPHER_AES_CMAC: c_ulong = 0x00000020;

pub const IW_AUTH_KEY_MGMT_802_1X: usize = 1;
pub const IW_AUTH_KEY_MGMT_PSK: usize = 2;

pub const IW_AUTH_ALG_OPEN_SYSTEM: c_ulong = 0x00000001;
pub const IW_AUTH_ALG_SHARED_KEY: c_ulong = 0x00000002;
pub const IW_AUTH_ALG_LEAP: c_ulong = 0x00000004;

pub const IW_AUTH_ROAMING_ENABLE: usize = 0;
pub const IW_AUTH_ROAMING_DISABLE: usize = 1;

pub const IW_AUTH_MFP_DISABLED: usize = 0;
pub const IW_AUTH_MFP_OPTIONAL: usize = 1;
pub const IW_AUTH_MFP_REQUIRED: usize = 2;

pub const IW_ENCODE_SEQ_MAX_SIZE: usize = 8;

pub const IW_ENCODE_ALG_NONE: usize = 0;
pub const IW_ENCODE_ALG_WEP: usize = 1;
pub const IW_ENCODE_ALG_TKIP: usize = 2;
pub const IW_ENCODE_ALG_CCMP: usize = 3;
pub const IW_ENCODE_ALG_PMK: usize = 4;
pub const IW_ENCODE_ALG_AES_CMAC: usize = 5;

pub const IW_ENCODE_EXT_TX_SEQ_VALID: c_ulong = 0x00000001;
pub const IW_ENCODE_EXT_RX_SEQ_VALID: c_ulong = 0x00000002;
pub const IW_ENCODE_EXT_GROUP_KEY: c_ulong = 0x00000004;
pub const IW_ENCODE_EXT_SET_TX_KEY: c_ulong = 0x00000008;

pub const IW_MICFAILURE_KEY_ID: c_ulong = 0x00000003;
pub const IW_MICFAILURE_GROUP: c_ulong = 0x00000004;
pub const IW_MICFAILURE_PAIRWISE: c_ulong = 0x00000008;
pub const IW_MICFAILURE_STAKEY: c_ulong = 0x00000010;
pub const IW_MICFAILURE_COUNT: c_ulong = 0x00000060;

pub const IW_ENC_CAPA_WPA: c_ulong = 0x00000001;
pub const IW_ENC_CAPA_WPA2: c_ulong = 0x00000002;
pub const IW_ENC_CAPA_CIPHER_TKIP: c_ulong = 0x00000004;
pub const IW_ENC_CAPA_CIPHER_CCMP: c_ulong = 0x00000008;
pub const IW_ENC_CAPA_4WAY_HANDSHAKE: c_ulong = 0x00000010;

pub const IW_EVENT_CAPA_K_0: c_ulong = 0x4000050; //   IW_EVENT_CAPA_MASK(0x8B04) | IW_EVENT_CAPA_MASK(0x8B06) | IW_EVENT_CAPA_MASK(0x8B1A);
pub const IW_EVENT_CAPA_K_1: c_ulong = 0x400; //   W_EVENT_CAPA_MASK(0x8B2A);

pub const IW_PMKSA_ADD: usize = 1;
pub const IW_PMKSA_REMOVE: usize = 2;
pub const IW_PMKSA_FLUSH: usize = 3;

pub const IW_PMKID_LEN: usize = 16;

pub const IW_PMKID_CAND_PREAUTH: c_ulong = 0x00000001;

pub const IW_EV_LCP_PK_LEN: usize = 4;

pub const IW_EV_CHAR_PK_LEN: usize = 20; // IW_EV_LCP_PK_LEN + crate::IFNAMSIZ;
pub const IW_EV_UINT_PK_LEN: usize = 8; // IW_EV_LCP_PK_LEN + size_of::<u32>();
pub const IW_EV_FREQ_PK_LEN: usize = 12; // IW_EV_LCP_PK_LEN + size_of::<iw_freq>();
pub const IW_EV_PARAM_PK_LEN: usize = 12; // IW_EV_LCP_PK_LEN + size_of::<iw_param>();
pub const IW_EV_ADDR_PK_LEN: usize = 20; // IW_EV_LCP_PK_LEN + size_of::<crate::sockaddr>();
pub const IW_EV_QUAL_PK_LEN: usize = 8; // IW_EV_LCP_PK_LEN + size_of::<iw_quality>();
pub const IW_EV_POINT_PK_LEN: usize = 8; // IW_EV_LCP_PK_LEN + 4;

// linux/neighbor.h
pub const NUD_NONE: u16 = 0x00;
pub const NUD_INCOMPLETE: u16 = 0x01;
pub const NUD_REACHABLE: u16 = 0x02;
pub const NUD_STALE: u16 = 0x04;
pub const NUD_DELAY: u16 = 0x08;
pub const NUD_PROBE: u16 = 0x10;
pub const NUD_FAILED: u16 = 0x20;
pub const NUD_NOARP: u16 = 0x40;
pub const NUD_PERMANENT: u16 = 0x80;

pub const NTF_USE: u8 = 0x01;
pub const NTF_SELF: u8 = 0x02;
pub const NTF_MASTER: u8 = 0x04;
pub const NTF_PROXY: u8 = 0x08;
pub const NTF_ROUTER: u8 = 0x80;

pub const NDA_UNSPEC: c_ushort = 0;
pub const NDA_DST: c_ushort = 1;
pub const NDA_LLADDR: c_ushort = 2;
pub const NDA_CACHEINFO: c_ushort = 3;
pub const NDA_PROBES: c_ushort = 4;
pub const NDA_VLAN: c_ushort = 5;
pub const NDA_PORT: c_ushort = 6;
pub const NDA_VNI: c_ushort = 7;
pub const NDA_IFINDEX: c_ushort = 8;

// linux/netlink.h

pub const NLM_F_BULK: c_int = 0x200;

// linux/rtnetlink.h
pub const TCA_UNSPEC: c_ushort = 0;
pub const TCA_KIND: c_ushort = 1;
pub const TCA_OPTIONS: c_ushort = 2;
pub const TCA_STATS: c_ushort = 3;
pub const TCA_XSTATS: c_ushort = 4;
pub const TCA_RATE: c_ushort = 5;
pub const TCA_FCNT: c_ushort = 6;
pub const TCA_STATS2: c_ushort = 7;
pub const TCA_STAB: c_ushort = 8;

pub const RTM_NEWLINK: u16 = 16;
pub const RTM_DELLINK: u16 = 17;
pub const RTM_GETLINK: u16 = 18;
pub const RTM_SETLINK: u16 = 19;
pub const RTM_NEWADDR: u16 = 20;
pub const RTM_DELADDR: u16 = 21;
pub const RTM_GETADDR: u16 = 22;
pub const RTM_NEWROUTE: u16 = 24;
pub const RTM_DELROUTE: u16 = 25;
pub const RTM_GETROUTE: u16 = 26;
pub const RTM_NEWNEIGH: u16 = 28;
pub const RTM_DELNEIGH: u16 = 29;
pub const RTM_GETNEIGH: u16 = 30;
pub const RTM_NEWRULE: u16 = 32;
pub const RTM_DELRULE: u16 = 33;
pub const RTM_GETRULE: u16 = 34;
pub const RTM_NEWQDISC: u16 = 36;
pub const RTM_DELQDISC: u16 = 37;
pub const RTM_GETQDISC: u16 = 38;
pub const RTM_NEWTCLASS: u16 = 40;
pub const RTM_DELTCLASS: u16 = 41;
pub const RTM_GETTCLASS: u16 = 42;
pub const RTM_NEWTFILTER: u16 = 44;
pub const RTM_DELTFILTER: u16 = 45;
pub const RTM_GETTFILTER: u16 = 46;
pub const RTM_NEWACTION: u16 = 48;
pub const RTM_DELACTION: u16 = 49;
pub const RTM_GETACTION: u16 = 50;
pub const RTM_NEWPREFIX: u16 = 52;
pub const RTM_GETMULTICAST: u16 = 58;
pub const RTM_GETANYCAST: u16 = 62;
pub const RTM_NEWNEIGHTBL: u16 = 64;
pub const RTM_GETNEIGHTBL: u16 = 66;
pub const RTM_SETNEIGHTBL: u16 = 67;
pub const RTM_NEWNDUSEROPT: u16 = 68;
pub const RTM_NEWADDRLABEL: u16 = 72;
pub const RTM_DELADDRLABEL: u16 = 73;
pub const RTM_GETADDRLABEL: u16 = 74;
pub const RTM_GETDCB: u16 = 78;
pub const RTM_SETDCB: u16 = 79;
pub const RTM_NEWNETCONF: u16 = 80;
pub const RTM_GETNETCONF: u16 = 82;
pub const RTM_NEWMDB: u16 = 84;
pub const RTM_DELMDB: u16 = 85;
pub const RTM_GETMDB: u16 = 86;
pub const RTM_NEWNSID: u16 = 88;
pub const RTM_DELNSID: u16 = 89;
pub const RTM_GETNSID: u16 = 90;

pub const RTM_F_NOTIFY: c_uint = 0x100;
pub const RTM_F_CLONED: c_uint = 0x200;
pub const RTM_F_EQUALIZE: c_uint = 0x400;
pub const RTM_F_PREFIX: c_uint = 0x800;

pub const RTA_UNSPEC: c_ushort = 0;
pub const RTA_DST: c_ushort = 1;
pub const RTA_SRC: c_ushort = 2;
pub const RTA_IIF: c_ushort = 3;
pub const RTA_OIF: c_ushort = 4;
pub const RTA_GATEWAY: c_ushort = 5;
pub const RTA_PRIORITY: c_ushort = 6;
pub const RTA_PREFSRC: c_ushort = 7;
pub const RTA_METRICS: c_ushort = 8;
pub const RTA_MULTIPATH: c_ushort = 9;
pub const RTA_PROTOINFO: c_ushort = 10; // No longer used
pub const RTA_FLOW: c_ushort = 11;
pub const RTA_CACHEINFO: c_ushort = 12;
pub const RTA_SESSION: c_ushort = 13; // No longer used
pub const RTA_MP_ALGO: c_ushort = 14; // No longer used
pub const RTA_TABLE: c_ushort = 15;
pub const RTA_MARK: c_ushort = 16;
pub const RTA_MFC_STATS: c_ushort = 17;

pub const RTN_UNSPEC: c_uchar = 0;
pub const RTN_UNICAST: c_uchar = 1;
pub const RTN_LOCAL: c_uchar = 2;
pub const RTN_BROADCAST: c_uchar = 3;
pub const RTN_ANYCAST: c_uchar = 4;
pub const RTN_MULTICAST: c_uchar = 5;
pub const RTN_BLACKHOLE: c_uchar = 6;
pub const RTN_UNREACHABLE: c_uchar = 7;
pub const RTN_PROHIBIT: c_uchar = 8;
pub const RTN_THROW: c_uchar = 9;
pub const RTN_NAT: c_uchar = 10;
pub const RTN_XRESOLVE: c_uchar = 11;

pub const RTPROT_UNSPEC: c_uchar = 0;
pub const RTPROT_REDIRECT: c_uchar = 1;
pub const RTPROT_KERNEL: c_uchar = 2;
pub const RTPROT_BOOT: c_uchar = 3;
pub const RTPROT_STATIC: c_uchar = 4;

pub const RT_SCOPE_UNIVERSE: c_uchar = 0;
pub const RT_SCOPE_SITE: c_uchar = 200;
pub const RT_SCOPE_LINK: c_uchar = 253;
pub const RT_SCOPE_HOST: c_uchar = 254;
pub const RT_SCOPE_NOWHERE: c_uchar = 255;

pub const RT_TABLE_UNSPEC: c_uchar = 0;
pub const RT_TABLE_COMPAT: c_uchar = 252;
pub const RT_TABLE_DEFAULT: c_uchar = 253;
pub const RT_TABLE_MAIN: c_uchar = 254;
pub const RT_TABLE_LOCAL: c_uchar = 255;

pub const RTMSG_OVERRUN: u32 = crate::NLMSG_OVERRUN as u32;
pub const RTMSG_NEWDEVICE: u32 = 0x11;
pub const RTMSG_DELDEVICE: u32 = 0x12;
pub const RTMSG_NEWROUTE: u32 = 0x21;
pub const RTMSG_DELROUTE: u32 = 0x22;
pub const RTMSG_NEWRULE: u32 = 0x31;
pub const RTMSG_DELRULE: u32 = 0x32;
pub const RTMSG_CONTROL: u32 = 0x40;
pub const RTMSG_AR_FAILED: u32 = 0x51;

pub const RTEXT_FILTER_VF: c_int = 1 << 0;
pub const RTEXT_FILTER_BRVLAN: c_int = 1 << 1;
pub const RTEXT_FILTER_BRVLAN_COMPRESSED: c_int = 1 << 2;
pub const RTEXT_FILTER_SKIP_STATS: c_int = 1 << 3;
pub const RTEXT_FILTER_MRP: c_int = 1 << 4;
pub const RTEXT_FILTER_CFM_CONFIG: c_int = 1 << 5;
pub const RTEXT_FILTER_CFM_STATUS: c_int = 1 << 6;

// userspace compat definitions for RTNLGRP_*
pub const RTMGRP_LINK: c_int = 0x00001;
pub const RTMGRP_NOTIFY: c_int = 0x00002;
pub const RTMGRP_NEIGH: c_int = 0x00004;
pub const RTMGRP_TC: c_int = 0x00008;
pub const RTMGRP_IPV4_IFADDR: c_int = 0x00010;
pub const RTMGRP_IPV4_MROUTE: c_int = 0x00020;
pub const RTMGRP_IPV4_ROUTE: c_int = 0x00040;
pub const RTMGRP_IPV4_RULE: c_int = 0x00080;
pub const RTMGRP_IPV6_IFADDR: c_int = 0x00100;
pub const RTMGRP_IPV6_MROUTE: c_int = 0x00200;
pub const RTMGRP_IPV6_ROUTE: c_int = 0x00400;
pub const RTMGRP_IPV6_IFINFO: c_int = 0x00800;
pub const RTMGRP_DECnet_IFADDR: c_int = 0x01000;
pub const RTMGRP_DECnet_ROUTE: c_int = 0x04000;
pub const RTMGRP_IPV6_PREFIX: c_int = 0x20000;

// enum rtnetlink_groups
pub const RTNLGRP_NONE: c_uint = 0x00;
pub const RTNLGRP_LINK: c_uint = 0x01;
pub const RTNLGRP_NOTIFY: c_uint = 0x02;
pub const RTNLGRP_NEIGH: c_uint = 0x03;
pub const RTNLGRP_TC: c_uint = 0x04;
pub const RTNLGRP_IPV4_IFADDR: c_uint = 0x05;
pub const RTNLGRP_IPV4_MROUTE: c_uint = 0x06;
pub const RTNLGRP_IPV4_ROUTE: c_uint = 0x07;
pub const RTNLGRP_IPV4_RULE: c_uint = 0x08;
pub const RTNLGRP_IPV6_IFADDR: c_uint = 0x09;
pub const RTNLGRP_IPV6_MROUTE: c_uint = 0x0a;
pub const RTNLGRP_IPV6_ROUTE: c_uint = 0x0b;
pub const RTNLGRP_IPV6_IFINFO: c_uint = 0x0c;
pub const RTNLGRP_DECnet_IFADDR: c_uint = 0x0d;
pub const RTNLGRP_NOP2: c_uint = 0x0e;
pub const RTNLGRP_DECnet_ROUTE: c_uint = 0x0f;
pub const RTNLGRP_DECnet_RULE: c_uint = 0x10;
pub const RTNLGRP_NOP4: c_uint = 0x11;
pub const RTNLGRP_IPV6_PREFIX: c_uint = 0x12;
pub const RTNLGRP_IPV6_RULE: c_uint = 0x13;
pub const RTNLGRP_ND_USEROPT: c_uint = 0x14;
pub const RTNLGRP_PHONET_IFADDR: c_uint = 0x15;
pub const RTNLGRP_PHONET_ROUTE: c_uint = 0x16;
pub const RTNLGRP_DCB: c_uint = 0x17;
pub const RTNLGRP_IPV4_NETCONF: c_uint = 0x18;
pub const RTNLGRP_IPV6_NETCONF: c_uint = 0x19;
pub const RTNLGRP_MDB: c_uint = 0x1a;
pub const RTNLGRP_MPLS_ROUTE: c_uint = 0x1b;
pub const RTNLGRP_NSID: c_uint = 0x1c;
pub const RTNLGRP_MPLS_NETCONF: c_uint = 0x1d;
pub const RTNLGRP_IPV4_MROUTE_R: c_uint = 0x1e;
pub const RTNLGRP_IPV6_MROUTE_R: c_uint = 0x1f;
pub const RTNLGRP_NEXTHOP: c_uint = 0x20;
pub const RTNLGRP_BRVLAN: c_uint = 0x21;
pub const RTNLGRP_MCTP_IFADDR: c_uint = 0x22;
pub const RTNLGRP_TUNNEL: c_uint = 0x23;
pub const RTNLGRP_STATS: c_uint = 0x24;

// linux/cn_proc.h
c_enum! {
    pub enum proc_cn_mcast_op {
        pub PROC_CN_MCAST_LISTEN = 1,
        pub PROC_CN_MCAST_IGNORE = 2,
    }

    pub enum proc_cn_event {
        pub PROC_EVENT_NONE = 0x00000000,
        pub PROC_EVENT_FORK = 0x00000001,
        pub PROC_EVENT_EXEC = 0x00000002,
        pub PROC_EVENT_UID = 0x00000004,
        pub PROC_EVENT_GID = 0x00000040,
        pub PROC_EVENT_SID = 0x00000080,
        pub PROC_EVENT_PTRACE = 0x00000100,
        pub PROC_EVENT_COMM = 0x00000200,
        pub PROC_EVENT_NONZERO_EXIT = 0x20000000,
        pub PROC_EVENT_COREDUMP = 0x40000000,
        pub PROC_EVENT_EXIT = 0x80000000,
    }
}

// linux/connector.h
pub const CN_IDX_PROC: c_uint = 0x1;
pub const CN_VAL_PROC: c_uint = 0x1;
pub const CN_IDX_CIFS: c_uint = 0x2;
pub const CN_VAL_CIFS: c_uint = 0x1;
pub const CN_W1_IDX: c_uint = 0x3;
pub const CN_W1_VAL: c_uint = 0x1;
pub const CN_IDX_V86D: c_uint = 0x4;
pub const CN_VAL_V86D_UVESAFB: c_uint = 0x1;
pub const CN_IDX_BB: c_uint = 0x5;
pub const CN_DST_IDX: c_uint = 0x6;
pub const CN_DST_VAL: c_uint = 0x1;
pub const CN_IDX_DM: c_uint = 0x7;
pub const CN_VAL_DM_USERSPACE_LOG: c_uint = 0x1;
pub const CN_IDX_DRBD: c_uint = 0x8;
pub const CN_VAL_DRBD: c_uint = 0x1;
pub const CN_KVP_IDX: c_uint = 0x9;
pub const CN_KVP_VAL: c_uint = 0x1;
pub const CN_VSS_IDX: c_uint = 0xA;
pub const CN_VSS_VAL: c_uint = 0x1;

// linux/module.h
pub const MODULE_INIT_IGNORE_MODVERSIONS: c_uint = 0x0001;
pub const MODULE_INIT_IGNORE_VERMAGIC: c_uint = 0x0002;

// linux/net_tstamp.h
pub const SOF_TIMESTAMPING_TX_HARDWARE: c_uint = 1 << 0;
pub const SOF_TIMESTAMPING_TX_SOFTWARE: c_uint = 1 << 1;
pub const SOF_TIMESTAMPING_RX_HARDWARE: c_uint = 1 << 2;
pub const SOF_TIMESTAMPING_RX_SOFTWARE: c_uint = 1 << 3;
pub const SOF_TIMESTAMPING_SOFTWARE: c_uint = 1 << 4;
pub const SOF_TIMESTAMPING_SYS_HARDWARE: c_uint = 1 << 5;
pub const SOF_TIMESTAMPING_RAW_HARDWARE: c_uint = 1 << 6;
pub const SOF_TIMESTAMPING_OPT_ID: c_uint = 1 << 7;
pub const SOF_TIMESTAMPING_TX_SCHED: c_uint = 1 << 8;
pub const SOF_TIMESTAMPING_TX_ACK: c_uint = 1 << 9;
pub const SOF_TIMESTAMPING_OPT_CMSG: c_uint = 1 << 10;
pub const SOF_TIMESTAMPING_OPT_TSONLY: c_uint = 1 << 11;
pub const SOF_TIMESTAMPING_OPT_STATS: c_uint = 1 << 12;
pub const SOF_TIMESTAMPING_OPT_PKTINFO: c_uint = 1 << 13;
pub const SOF_TIMESTAMPING_OPT_TX_SWHW: c_uint = 1 << 14;
pub const SOF_TIMESTAMPING_BIND_PHC: c_uint = 1 << 15;
pub const SOF_TIMESTAMPING_OPT_ID_TCP: c_uint = 1 << 16;
pub const SOF_TIMESTAMPING_OPT_RX_FILTER: c_uint = 1 << 17;
pub const SOF_TXTIME_DEADLINE_MODE: u32 = 1 << 0;
pub const SOF_TXTIME_REPORT_ERRORS: u32 = 1 << 1;

pub const HWTSTAMP_TX_OFF: c_uint = 0;
pub const HWTSTAMP_TX_ON: c_uint = 1;
pub const HWTSTAMP_TX_ONESTEP_SYNC: c_uint = 2;
pub const HWTSTAMP_TX_ONESTEP_P2P: c_uint = 3;

pub const HWTSTAMP_FILTER_NONE: c_uint = 0;
pub const HWTSTAMP_FILTER_ALL: c_uint = 1;
pub const HWTSTAMP_FILTER_SOME: c_uint = 2;
pub const HWTSTAMP_FILTER_PTP_V1_L4_EVENT: c_uint = 3;
pub const HWTSTAMP_FILTER_PTP_V1_L4_SYNC: c_uint = 4;
pub const HWTSTAMP_FILTER_PTP_V1_L4_DELAY_REQ: c_uint = 5;
pub const HWTSTAMP_FILTER_PTP_V2_L4_EVENT: c_uint = 6;
pub const HWTSTAMP_FILTER_PTP_V2_L4_SYNC: c_uint = 7;
pub const HWTSTAMP_FILTER_PTP_V2_L4_DELAY_REQ: c_uint = 8;
pub const HWTSTAMP_FILTER_PTP_V2_L2_EVENT: c_uint = 9;
pub const HWTSTAMP_FILTER_PTP_V2_L2_SYNC: c_uint = 10;
pub const HWTSTAMP_FILTER_PTP_V2_L2_DELAY_REQ: c_uint = 11;
pub const HWTSTAMP_FILTER_PTP_V2_EVENT: c_uint = 12;
pub const HWTSTAMP_FILTER_PTP_V2_SYNC: c_uint = 13;
pub const HWTSTAMP_FILTER_PTP_V2_DELAY_REQ: c_uint = 14;
pub const HWTSTAMP_FILTER_NTP_ALL: c_uint = 15;

// linux/ptp_clock.h
pub const PTP_MAX_SAMPLES: c_uint = 25; // Maximum allowed offset measurement samples.

const PTP_CLK_MAGIC: u32 = b'=' as u32;

pub const PTP_CLOCK_GETCAPS: Ioctl = _IOR::<ptp_clock_caps>(PTP_CLK_MAGIC, 1);
pub const PTP_EXTTS_REQUEST: Ioctl = _IOW::<ptp_extts_request>(PTP_CLK_MAGIC, 2);
pub const PTP_PEROUT_REQUEST: Ioctl = _IOW::<ptp_perout_request>(PTP_CLK_MAGIC, 3);
pub const PTP_ENABLE_PPS: Ioctl = _IOW::<c_int>(PTP_CLK_MAGIC, 4);
pub const PTP_SYS_OFFSET: Ioctl = _IOW::<ptp_sys_offset>(PTP_CLK_MAGIC, 5);
pub const PTP_PIN_GETFUNC: Ioctl = _IOWR::<ptp_pin_desc>(PTP_CLK_MAGIC, 6);
pub const PTP_PIN_SETFUNC: Ioctl = _IOW::<ptp_pin_desc>(PTP_CLK_MAGIC, 7);
pub const PTP_SYS_OFFSET_PRECISE: Ioctl = _IOWR::<ptp_sys_offset_precise>(PTP_CLK_MAGIC, 8);
pub const PTP_SYS_OFFSET_EXTENDED: Ioctl = _IOWR::<ptp_sys_offset_extended>(PTP_CLK_MAGIC, 9);

pub const PTP_CLOCK_GETCAPS2: Ioctl = _IOR::<ptp_clock_caps>(PTP_CLK_MAGIC, 10);
pub const PTP_EXTTS_REQUEST2: Ioctl = _IOW::<ptp_extts_request>(PTP_CLK_MAGIC, 11);
pub const PTP_PEROUT_REQUEST2: Ioctl = _IOW::<ptp_perout_request>(PTP_CLK_MAGIC, 12);
pub const PTP_ENABLE_PPS2: Ioctl = _IOW::<c_int>(PTP_CLK_MAGIC, 13);
pub const PTP_SYS_OFFSET2: Ioctl = _IOW::<ptp_sys_offset>(PTP_CLK_MAGIC, 14);
pub const PTP_PIN_GETFUNC2: Ioctl = _IOWR::<ptp_pin_desc>(PTP_CLK_MAGIC, 15);
pub const PTP_PIN_SETFUNC2: Ioctl = _IOW::<ptp_pin_desc>(PTP_CLK_MAGIC, 16);
pub const PTP_SYS_OFFSET_PRECISE2: Ioctl = _IOWR::<ptp_sys_offset_precise>(PTP_CLK_MAGIC, 17);
pub const PTP_SYS_OFFSET_EXTENDED2: Ioctl = _IOWR::<ptp_sys_offset_extended>(PTP_CLK_MAGIC, 18);

// enum ptp_pin_function
pub const PTP_PF_NONE: c_uint = 0;
pub const PTP_PF_EXTTS: c_uint = 1;
pub const PTP_PF_PEROUT: c_uint = 2;
pub const PTP_PF_PHYSYNC: c_uint = 3;

// linux/tls.h
pub const TLS_TX: c_int = 1;
pub const TLS_RX: c_int = 2;

pub const TLS_TX_ZEROCOPY_RO: c_int = 3;
pub const TLS_RX_EXPECT_NO_PAD: c_int = 4;

pub const TLS_1_2_VERSION_MAJOR: __u8 = 0x3;
pub const TLS_1_2_VERSION_MINOR: __u8 = 0x3;
pub const TLS_1_2_VERSION: __u16 =
    ((TLS_1_2_VERSION_MAJOR as __u16) << 8) | (TLS_1_2_VERSION_MINOR as __u16);

pub const TLS_1_3_VERSION_MAJOR: __u8 = 0x3;
pub const TLS_1_3_VERSION_MINOR: __u8 = 0x4;
pub const TLS_1_3_VERSION: __u16 =
    ((TLS_1_3_VERSION_MAJOR as __u16) << 8) | (TLS_1_3_VERSION_MINOR as __u16);

pub const TLS_CIPHER_AES_GCM_128: __u16 = 51;
pub const TLS_CIPHER_AES_GCM_128_IV_SIZE: usize = 8;
pub const TLS_CIPHER_AES_GCM_128_KEY_SIZE: usize = 16;
pub const TLS_CIPHER_AES_GCM_128_SALT_SIZE: usize = 4;
pub const TLS_CIPHER_AES_GCM_128_TAG_SIZE: usize = 16;
pub const TLS_CIPHER_AES_GCM_128_REC_SEQ_SIZE: usize = 8;

pub const TLS_CIPHER_AES_GCM_256: __u16 = 52;
pub const TLS_CIPHER_AES_GCM_256_IV_SIZE: usize = 8;
pub const TLS_CIPHER_AES_GCM_256_KEY_SIZE: usize = 32;
pub const TLS_CIPHER_AES_GCM_256_SALT_SIZE: usize = 4;
pub const TLS_CIPHER_AES_GCM_256_TAG_SIZE: usize = 16;
pub const TLS_CIPHER_AES_GCM_256_REC_SEQ_SIZE: usize = 8;

pub const TLS_CIPHER_AES_CCM_128: __u16 = 53;
pub const TLS_CIPHER_AES_CCM_128_IV_SIZE: usize = 8;
pub const TLS_CIPHER_AES_CCM_128_KEY_SIZE: usize = 16;
pub const TLS_CIPHER_AES_CCM_128_SALT_SIZE: usize = 4;
pub const TLS_CIPHER_AES_CCM_128_TAG_SIZE: usize = 16;
pub const TLS_CIPHER_AES_CCM_128_REC_SEQ_SIZE: usize = 8;

pub const TLS_CIPHER_CHACHA20_POLY1305: __u16 = 54;
pub const TLS_CIPHER_CHACHA20_POLY1305_IV_SIZE: usize = 12;
pub const TLS_CIPHER_CHACHA20_POLY1305_KEY_SIZE: usize = 32;
pub const TLS_CIPHER_CHACHA20_POLY1305_SALT_SIZE: usize = 0;
pub const TLS_CIPHER_CHACHA20_POLY1305_TAG_SIZE: usize = 16;
pub const TLS_CIPHER_CHACHA20_POLY1305_REC_SEQ_SIZE: usize = 8;

pub const TLS_CIPHER_SM4_GCM: __u16 = 55;
pub const TLS_CIPHER_SM4_GCM_IV_SIZE: usize = 8;
pub const TLS_CIPHER_SM4_GCM_KEY_SIZE: usize = 16;
pub const TLS_CIPHER_SM4_GCM_SALT_SIZE: usize = 4;
pub const TLS_CIPHER_SM4_GCM_TAG_SIZE: usize = 16;
pub const TLS_CIPHER_SM4_GCM_REC_SEQ_SIZE: usize = 8;

pub const TLS_CIPHER_SM4_CCM: __u16 = 56;
pub const TLS_CIPHER_SM4_CCM_IV_SIZE: usize = 8;
pub const TLS_CIPHER_SM4_CCM_KEY_SIZE: usize = 16;
pub const TLS_CIPHER_SM4_CCM_SALT_SIZE: usize = 4;
pub const TLS_CIPHER_SM4_CCM_TAG_SIZE: usize = 16;
pub const TLS_CIPHER_SM4_CCM_REC_SEQ_SIZE: usize = 8;

pub const TLS_CIPHER_ARIA_GCM_128: __u16 = 57;
pub const TLS_CIPHER_ARIA_GCM_128_IV_SIZE: usize = 8;
pub const TLS_CIPHER_ARIA_GCM_128_KEY_SIZE: usize = 16;
pub const TLS_CIPHER_ARIA_GCM_128_SALT_SIZE: usize = 4;
pub const TLS_CIPHER_ARIA_GCM_128_TAG_SIZE: usize = 16;
pub const TLS_CIPHER_ARIA_GCM_128_REC_SEQ_SIZE: usize = 8;

pub const TLS_CIPHER_ARIA_GCM_256: __u16 = 58;
pub const TLS_CIPHER_ARIA_GCM_256_IV_SIZE: usize = 8;
pub const TLS_CIPHER_ARIA_GCM_256_KEY_SIZE: usize = 32;
pub const TLS_CIPHER_ARIA_GCM_256_SALT_SIZE: usize = 4;
pub const TLS_CIPHER_ARIA_GCM_256_TAG_SIZE: usize = 16;
pub const TLS_CIPHER_ARIA_GCM_256_REC_SEQ_SIZE: usize = 8;

pub const TLS_SET_RECORD_TYPE: c_int = 1;
pub const TLS_GET_RECORD_TYPE: c_int = 2;

pub const SOL_TLS: c_int = 282;

// enum
pub const TLS_INFO_UNSPEC: c_int = 0x00;
pub const TLS_INFO_VERSION: c_int = 0x01;
pub const TLS_INFO_CIPHER: c_int = 0x02;
pub const TLS_INFO_TXCONF: c_int = 0x03;
pub const TLS_INFO_RXCONF: c_int = 0x04;
pub const TLS_INFO_ZC_RO_TX: c_int = 0x05;
pub const TLS_INFO_RX_NO_PAD: c_int = 0x06;

pub const TLS_CONF_BASE: c_int = 1;
pub const TLS_CONF_SW: c_int = 2;
pub const TLS_CONF_HW: c_int = 3;
pub const TLS_CONF_HW_RECORD: c_int = 4;

// linux/if_alg.h
pub const ALG_SET_KEY: c_int = 1;
pub const ALG_SET_IV: c_int = 2;
pub const ALG_SET_OP: c_int = 3;
pub const ALG_SET_AEAD_ASSOCLEN: c_int = 4;
pub const ALG_SET_AEAD_AUTHSIZE: c_int = 5;
pub const ALG_SET_DRBG_ENTROPY: c_int = 6;
pub const ALG_SET_KEY_BY_KEY_SERIAL: c_int = 7;

pub const ALG_OP_DECRYPT: c_int = 0;
pub const ALG_OP_ENCRYPT: c_int = 1;

// include/uapi/linux/if.h
pub const IF_OPER_UNKNOWN: c_int = 0;
pub const IF_OPER_NOTPRESENT: c_int = 1;
pub const IF_OPER_DOWN: c_int = 2;
pub const IF_OPER_LOWERLAYERDOWN: c_int = 3;
pub const IF_OPER_TESTING: c_int = 4;
pub const IF_OPER_DORMANT: c_int = 5;
pub const IF_OPER_UP: c_int = 6;

pub const IF_LINK_MODE_DEFAULT: c_int = 0;
pub const IF_LINK_MODE_DORMANT: c_int = 1;
pub const IF_LINK_MODE_TESTING: c_int = 2;

// include/uapi/linux/mman.h
pub const MAP_SHARED_VALIDATE: c_int = 0x3;
pub const MAP_DROPPABLE: c_int = 0x8;

// uapi/linux/vm_sockets.h
pub const VMADDR_CID_ANY: c_uint = 0xFFFFFFFF;
pub const VMADDR_CID_HYPERVISOR: c_uint = 0;
#[deprecated(
    since = "0.2.74",
    note = "VMADDR_CID_RESERVED is removed since Linux v5.6 and \
            replaced with VMADDR_CID_LOCAL"
)]
pub const VMADDR_CID_RESERVED: c_uint = 1;
pub const VMADDR_CID_LOCAL: c_uint = 1;
pub const VMADDR_CID_HOST: c_uint = 2;
pub const VMADDR_PORT_ANY: c_uint = 0xFFFFFFFF;

// uapi/linux/inotify.h
pub const IN_ACCESS: u32 = 0x0000_0001;
pub const IN_MODIFY: u32 = 0x0000_0002;
pub const IN_ATTRIB: u32 = 0x0000_0004;
pub const IN_CLOSE_WRITE: u32 = 0x0000_0008;
pub const IN_CLOSE_NOWRITE: u32 = 0x0000_0010;
pub const IN_CLOSE: u32 = IN_CLOSE_WRITE | IN_CLOSE_NOWRITE;
pub const IN_OPEN: u32 = 0x0000_0020;
pub const IN_MOVED_FROM: u32 = 0x0000_0040;
pub const IN_MOVED_TO: u32 = 0x0000_0080;
pub const IN_MOVE: u32 = IN_MOVED_FROM | IN_MOVED_TO;
pub const IN_CREATE: u32 = 0x0000_0100;
pub const IN_DELETE: u32 = 0x0000_0200;
pub const IN_DELETE_SELF: u32 = 0x0000_0400;
pub const IN_MOVE_SELF: u32 = 0x0000_0800;
pub const IN_UNMOUNT: u32 = 0x0000_2000;
pub const IN_Q_OVERFLOW: u32 = 0x0000_4000;
pub const IN_IGNORED: u32 = 0x0000_8000;
pub const IN_ONLYDIR: u32 = 0x0100_0000;
pub const IN_DONT_FOLLOW: u32 = 0x0200_0000;
pub const IN_EXCL_UNLINK: u32 = 0x0400_0000;

// uapi/linux/securebits.h
const SECURE_NOROOT: c_int = 0;
const SECURE_NOROOT_LOCKED: c_int = 1;

pub const SECBIT_NOROOT: c_int = issecure_mask(SECURE_NOROOT);
pub const SECBIT_NOROOT_LOCKED: c_int = issecure_mask(SECURE_NOROOT_LOCKED);

const SECURE_NO_SETUID_FIXUP: c_int = 2;
const SECURE_NO_SETUID_FIXUP_LOCKED: c_int = 3;

pub const SECBIT_NO_SETUID_FIXUP: c_int = issecure_mask(SECURE_NO_SETUID_FIXUP);
pub const SECBIT_NO_SETUID_FIXUP_LOCKED: c_int = issecure_mask(SECURE_NO_SETUID_FIXUP_LOCKED);

const SECURE_KEEP_CAPS: c_int = 4;
const SECURE_KEEP_CAPS_LOCKED: c_int = 5;

pub const SECBIT_KEEP_CAPS: c_int = issecure_mask(SECURE_KEEP_CAPS);
pub const SECBIT_KEEP_CAPS_LOCKED: c_int = issecure_mask(SECURE_KEEP_CAPS_LOCKED);

const SECURE_NO_CAP_AMBIENT_RAISE: c_int = 6;
const SECURE_NO_CAP_AMBIENT_RAISE_LOCKED: c_int = 7;

pub const SECBIT_NO_CAP_AMBIENT_RAISE: c_int = issecure_mask(SECURE_NO_CAP_AMBIENT_RAISE);
pub const SECBIT_NO_CAP_AMBIENT_RAISE_LOCKED: c_int =
    issecure_mask(SECURE_NO_CAP_AMBIENT_RAISE_LOCKED);

const SECURE_EXEC_RESTRICT_FILE: c_int = 8;
const SECURE_EXEC_RESTRICT_FILE_LOCKED: c_int = 9;

pub const SECBIT_EXEC_RESTRICT_FILE: c_int = issecure_mask(SECURE_EXEC_RESTRICT_FILE);
pub const SECBIT_EXEC_RESTRICT_FILE_LOCKED: c_int = issecure_mask(SECURE_EXEC_RESTRICT_FILE_LOCKED);

const SECURE_EXEC_DENY_INTERACTIVE: c_int = 10;
const SECURE_EXEC_DENY_INTERACTIVE_LOCKED: c_int = 11;

pub const SECBIT_EXEC_DENY_INTERACTIVE: c_int = issecure_mask(SECURE_EXEC_DENY_INTERACTIVE);
pub const SECBIT_EXEC_DENY_INTERACTIVE_LOCKED: c_int =
    issecure_mask(SECURE_EXEC_DENY_INTERACTIVE_LOCKED);

pub const SECUREBITS_DEFAULT: c_int = 0x00000000;
pub const SECURE_ALL_BITS: c_int = SECBIT_NOROOT
    | SECBIT_NO_SETUID_FIXUP
    | SECBIT_KEEP_CAPS
    | SECBIT_NO_CAP_AMBIENT_RAISE
    | SECBIT_EXEC_RESTRICT_FILE
    | SECBIT_EXEC_DENY_INTERACTIVE;
pub const SECURE_ALL_LOCKS: c_int = SECURE_ALL_BITS << 1;

pub const SECURE_ALL_UNPRIVILEGED: c_int =
    issecure_mask(SECURE_EXEC_RESTRICT_FILE) | issecure_mask(SECURE_EXEC_DENY_INTERACTIVE);

const fn issecure_mask(x: c_int) -> c_int {
    1 << x
}

pub const IN_MASK_CREATE: u32 = 0x1000_0000;
pub const IN_MASK_ADD: u32 = 0x2000_0000;
pub const IN_ISDIR: u32 = 0x4000_0000;
pub const IN_ONESHOT: u32 = 0x8000_0000;

pub const IN_ALL_EVENTS: u32 = IN_ACCESS
    | IN_MODIFY
    | IN_ATTRIB
    | IN_CLOSE_WRITE
    | IN_CLOSE_NOWRITE
    | IN_OPEN
    | IN_MOVED_FROM
    | IN_MOVED_TO
    | IN_DELETE
    | IN_CREATE
    | IN_DELETE_SELF
    | IN_MOVE_SELF;

pub const IN_CLOEXEC: c_int = O_CLOEXEC;
pub const IN_NONBLOCK: c_int = O_NONBLOCK;

// uapi/linux/mount.h
pub const OPEN_TREE_CLONE: c_uint = 0x01;
pub const OPEN_TREE_CLOEXEC: c_uint = O_CLOEXEC as c_uint;

// uapi/linux/netfilter/nf_tables.h
pub const NFT_TABLE_MAXNAMELEN: c_int = 256;
pub const NFT_CHAIN_MAXNAMELEN: c_int = 256;
pub const NFT_SET_MAXNAMELEN: c_int = 256;
pub const NFT_OBJ_MAXNAMELEN: c_int = 256;
pub const NFT_USERDATA_MAXLEN: c_int = 256;

pub const NFT_REG_VERDICT: c_int = 0;
pub const NFT_REG_1: c_int = 1;
pub const NFT_REG_2: c_int = 2;
pub const NFT_REG_3: c_int = 3;
pub const NFT_REG_4: c_int = 4;
pub const __NFT_REG_MAX: c_int = 5;
pub const NFT_REG32_00: c_int = 8;
pub const NFT_REG32_01: c_int = 9;
pub const NFT_REG32_02: c_int = 10;
pub const NFT_REG32_03: c_int = 11;
pub const NFT_REG32_04: c_int = 12;
pub const NFT_REG32_05: c_int = 13;
pub const NFT_REG32_06: c_int = 14;
pub const NFT_REG32_07: c_int = 15;
pub const NFT_REG32_08: c_int = 16;
pub const NFT_REG32_09: c_int = 17;
pub const NFT_REG32_10: c_int = 18;
pub const NFT_REG32_11: c_int = 19;
pub const NFT_REG32_12: c_int = 20;
pub const NFT_REG32_13: c_int = 21;
pub const NFT_REG32_14: c_int = 22;
pub const NFT_REG32_15: c_int = 23;

pub const NFT_REG_SIZE: c_int = 16;
pub const NFT_REG32_SIZE: c_int = 4;

pub const NFT_CONTINUE: c_int = -1;
pub const NFT_BREAK: c_int = -2;
pub const NFT_JUMP: c_int = -3;
pub const NFT_GOTO: c_int = -4;
pub const NFT_RETURN: c_int = -5;

pub const NFT_MSG_NEWTABLE: c_int = 0;
pub const NFT_MSG_GETTABLE: c_int = 1;
pub const NFT_MSG_DELTABLE: c_int = 2;
pub const NFT_MSG_NEWCHAIN: c_int = 3;
pub const NFT_MSG_GETCHAIN: c_int = 4;
pub const NFT_MSG_DELCHAIN: c_int = 5;
pub const NFT_MSG_NEWRULE: c_int = 6;
pub const NFT_MSG_GETRULE: c_int = 7;
pub const NFT_MSG_DELRULE: c_int = 8;
pub const NFT_MSG_NEWSET: c_int = 9;
pub const NFT_MSG_GETSET: c_int = 10;
pub const NFT_MSG_DELSET: c_int = 11;
pub const NFT_MSG_NEWSETELEM: c_int = 12;
pub const NFT_MSG_GETSETELEM: c_int = 13;
pub const NFT_MSG_DELSETELEM: c_int = 14;
pub const NFT_MSG_NEWGEN: c_int = 15;
pub const NFT_MSG_GETGEN: c_int = 16;
pub const NFT_MSG_TRACE: c_int = 17;
cfg_if! {
    if #[cfg(not(target_arch = "sparc64"))] {
        pub const NFT_MSG_NEWOBJ: c_int = 18;
        pub const NFT_MSG_GETOBJ: c_int = 19;
        pub const NFT_MSG_DELOBJ: c_int = 20;
        pub const NFT_MSG_GETOBJ_RESET: c_int = 21;
    }
}

pub const NFT_MSG_MAX: c_int = 34;

pub const NFT_SET_ANONYMOUS: c_int = 0x1;
pub const NFT_SET_CONSTANT: c_int = 0x2;
pub const NFT_SET_INTERVAL: c_int = 0x4;
pub const NFT_SET_MAP: c_int = 0x8;
pub const NFT_SET_TIMEOUT: c_int = 0x10;
pub const NFT_SET_EVAL: c_int = 0x20;

pub const NFT_SET_POL_PERFORMANCE: c_int = 0;
pub const NFT_SET_POL_MEMORY: c_int = 1;

pub const NFT_SET_ELEM_INTERVAL_END: c_int = 0x1;

pub const NFT_DATA_VALUE: c_uint = 0;
pub const NFT_DATA_VERDICT: c_uint = 0xffffff00;

pub const NFT_DATA_RESERVED_MASK: c_uint = 0xffffff00;

pub const NFT_DATA_VALUE_MAXLEN: c_int = 64;

pub const NFT_BYTEORDER_NTOH: c_int = 0;
pub const NFT_BYTEORDER_HTON: c_int = 1;

pub const NFT_CMP_EQ: c_int = 0;
pub const NFT_CMP_NEQ: c_int = 1;
pub const NFT_CMP_LT: c_int = 2;
pub const NFT_CMP_LTE: c_int = 3;
pub const NFT_CMP_GT: c_int = 4;
pub const NFT_CMP_GTE: c_int = 5;

pub const NFT_RANGE_EQ: c_int = 0;
pub const NFT_RANGE_NEQ: c_int = 1;

pub const NFT_LOOKUP_F_INV: c_int = 1 << 0;

pub const NFT_DYNSET_OP_ADD: c_int = 0;
pub const NFT_DYNSET_OP_UPDATE: c_int = 1;

pub const NFT_DYNSET_F_INV: c_int = 1 << 0;

pub const NFT_PAYLOAD_LL_HEADER: c_int = 0;
pub const NFT_PAYLOAD_NETWORK_HEADER: c_int = 1;
pub const NFT_PAYLOAD_TRANSPORT_HEADER: c_int = 2;

pub const NFT_PAYLOAD_CSUM_NONE: c_int = 0;
pub const NFT_PAYLOAD_CSUM_INET: c_int = 1;

pub const NFT_META_LEN: c_int = 0;
pub const NFT_META_PROTOCOL: c_int = 1;
pub const NFT_META_PRIORITY: c_int = 2;
pub const NFT_META_MARK: c_int = 3;
pub const NFT_META_IIF: c_int = 4;
pub const NFT_META_OIF: c_int = 5;
pub const NFT_META_IIFNAME: c_int = 6;
pub const NFT_META_OIFNAME: c_int = 7;
pub const NFT_META_IIFTYPE: c_int = 8;
pub const NFT_META_OIFTYPE: c_int = 9;
pub const NFT_META_SKUID: c_int = 10;
pub const NFT_META_SKGID: c_int = 11;
pub const NFT_META_NFTRACE: c_int = 12;
pub const NFT_META_RTCLASSID: c_int = 13;
pub const NFT_META_SECMARK: c_int = 14;
pub const NFT_META_NFPROTO: c_int = 15;
pub const NFT_META_L4PROTO: c_int = 16;
pub const NFT_META_BRI_IIFNAME: c_int = 17;
pub const NFT_META_BRI_OIFNAME: c_int = 18;
pub const NFT_META_PKTTYPE: c_int = 19;
pub const NFT_META_CPU: c_int = 20;
pub const NFT_META_IIFGROUP: c_int = 21;
pub const NFT_META_OIFGROUP: c_int = 22;
pub const NFT_META_CGROUP: c_int = 23;
pub const NFT_META_PRANDOM: c_int = 24;

pub const NFT_CT_STATE: c_int = 0;
pub const NFT_CT_DIRECTION: c_int = 1;
pub const NFT_CT_STATUS: c_int = 2;
pub const NFT_CT_MARK: c_int = 3;
pub const NFT_CT_SECMARK: c_int = 4;
pub const NFT_CT_EXPIRATION: c_int = 5;
pub const NFT_CT_HELPER: c_int = 6;
pub const NFT_CT_L3PROTOCOL: c_int = 7;
pub const NFT_CT_SRC: c_int = 8;
pub const NFT_CT_DST: c_int = 9;
pub const NFT_CT_PROTOCOL: c_int = 10;
pub const NFT_CT_PROTO_SRC: c_int = 11;
pub const NFT_CT_PROTO_DST: c_int = 12;
pub const NFT_CT_LABELS: c_int = 13;
pub const NFT_CT_PKTS: c_int = 14;
pub const NFT_CT_BYTES: c_int = 15;
pub const NFT_CT_AVGPKT: c_int = 16;
pub const NFT_CT_ZONE: c_int = 17;
pub const NFT_CT_EVENTMASK: c_int = 18;
pub const NFT_CT_SRC_IP: c_int = 19;
pub const NFT_CT_DST_IP: c_int = 20;
pub const NFT_CT_SRC_IP6: c_int = 21;
pub const NFT_CT_DST_IP6: c_int = 22;

pub const NFT_LIMIT_PKTS: c_int = 0;
pub const NFT_LIMIT_PKT_BYTES: c_int = 1;

pub const NFT_LIMIT_F_INV: c_int = 1 << 0;

pub const NFT_QUEUE_FLAG_BYPASS: c_int = 0x01;
pub const NFT_QUEUE_FLAG_CPU_FANOUT: c_int = 0x02;
pub const NFT_QUEUE_FLAG_MASK: c_int = 0x03;

pub const NFT_QUOTA_F_INV: c_int = 1 << 0;

pub const NFT_REJECT_ICMP_UNREACH: c_int = 0;
pub const NFT_REJECT_TCP_RST: c_int = 1;
pub const NFT_REJECT_ICMPX_UNREACH: c_int = 2;

pub const NFT_REJECT_ICMPX_NO_ROUTE: c_int = 0;
pub const NFT_REJECT_ICMPX_PORT_UNREACH: c_int = 1;
pub const NFT_REJECT_ICMPX_HOST_UNREACH: c_int = 2;
pub const NFT_REJECT_ICMPX_ADMIN_PROHIBITED: c_int = 3;

pub const NFT_NAT_SNAT: c_int = 0;
pub const NFT_NAT_DNAT: c_int = 1;

pub const NFT_TRACETYPE_UNSPEC: c_int = 0;
pub const NFT_TRACETYPE_POLICY: c_int = 1;
pub const NFT_TRACETYPE_RETURN: c_int = 2;
pub const NFT_TRACETYPE_RULE: c_int = 3;

pub const NFT_NG_INCREMENTAL: c_int = 0;
pub const NFT_NG_RANDOM: c_int = 1;

// linux/input.h
pub const FF_MAX: __u16 = 0x7f;
pub const FF_CNT: usize = FF_MAX as usize + 1;

// linux/input-event-codes.h
pub const INPUT_PROP_POINTER: __u16 = 0x00;
pub const INPUT_PROP_DIRECT: __u16 = 0x01;
pub const INPUT_PROP_BUTTONPAD: __u16 = 0x02;
pub const INPUT_PROP_SEMI_MT: __u16 = 0x03;
pub const INPUT_PROP_TOPBUTTONPAD: __u16 = 0x04;
pub const INPUT_PROP_POINTING_STICK: __u16 = 0x05;
pub const INPUT_PROP_ACCELEROMETER: __u16 = 0x06;
pub const INPUT_PROP_MAX: __u16 = 0x1f;
pub const INPUT_PROP_CNT: usize = INPUT_PROP_MAX as usize + 1;
pub const EV_MAX: __u16 = 0x1f;
pub const EV_CNT: usize = EV_MAX as usize + 1;
pub const SYN_MAX: __u16 = 0xf;
pub const SYN_CNT: usize = SYN_MAX as usize + 1;
pub const KEY_MAX: __u16 = 0x2ff;
pub const KEY_CNT: usize = KEY_MAX as usize + 1;
pub const REL_MAX: __u16 = 0x0f;
pub const REL_CNT: usize = REL_MAX as usize + 1;
pub const ABS_MAX: __u16 = 0x3f;
pub const ABS_CNT: usize = ABS_MAX as usize + 1;
pub const SW_MAX: __u16 = 0x10;
pub const SW_CNT: usize = SW_MAX as usize + 1;
pub const MSC_MAX: __u16 = 0x07;
pub const MSC_CNT: usize = MSC_MAX as usize + 1;
pub const LED_MAX: __u16 = 0x0f;
pub const LED_CNT: usize = LED_MAX as usize + 1;
pub const REP_MAX: __u16 = 0x01;
pub const REP_CNT: usize = REP_MAX as usize + 1;
pub const SND_MAX: __u16 = 0x07;
pub const SND_CNT: usize = SND_MAX as usize + 1;

// linux/uinput.h
pub const UINPUT_VERSION: c_uint = 5;
pub const UINPUT_MAX_NAME_SIZE: usize = 80;

// uapi/linux/fanotify.h
pub const FAN_ACCESS: u64 = 0x0000_0001;
pub const FAN_MODIFY: u64 = 0x0000_0002;
pub const FAN_ATTRIB: u64 = 0x0000_0004;
pub const FAN_CLOSE_WRITE: u64 = 0x0000_0008;
pub const FAN_CLOSE_NOWRITE: u64 = 0x0000_0010;
pub const FAN_OPEN: u64 = 0x0000_0020;
pub const FAN_MOVED_FROM: u64 = 0x0000_0040;
pub const FAN_MOVED_TO: u64 = 0x0000_0080;
pub const FAN_CREATE: u64 = 0x0000_0100;
pub const FAN_DELETE: u64 = 0x0000_0200;
pub const FAN_DELETE_SELF: u64 = 0x0000_0400;
pub const FAN_MOVE_SELF: u64 = 0x0000_0800;
pub const FAN_OPEN_EXEC: u64 = 0x0000_1000;

pub const FAN_Q_OVERFLOW: u64 = 0x0000_4000;
pub const FAN_FS_ERROR: u64 = 0x0000_8000;

pub const FAN_OPEN_PERM: u64 = 0x0001_0000;
pub const FAN_ACCESS_PERM: u64 = 0x0002_0000;
pub const FAN_OPEN_EXEC_PERM: u64 = 0x0004_0000;

pub const FAN_EVENT_ON_CHILD: u64 = 0x0800_0000;

pub const FAN_RENAME: u64 = 0x1000_0000;

pub const FAN_ONDIR: u64 = 0x4000_0000;

pub const FAN_CLOSE: u64 = FAN_CLOSE_WRITE | FAN_CLOSE_NOWRITE;
pub const FAN_MOVE: u64 = FAN_MOVED_FROM | FAN_MOVED_TO;

pub const FAN_CLOEXEC: c_uint = 0x0000_0001;
pub const FAN_NONBLOCK: c_uint = 0x0000_0002;

pub const FAN_CLASS_NOTIF: c_uint = 0x0000_0000;
pub const FAN_CLASS_CONTENT: c_uint = 0x0000_0004;
pub const FAN_CLASS_PRE_CONTENT: c_uint = 0x0000_0008;

pub const FAN_UNLIMITED_QUEUE: c_uint = 0x0000_0010;
pub const FAN_UNLIMITED_MARKS: c_uint = 0x0000_0020;
pub const FAN_ENABLE_AUDIT: c_uint = 0x0000_0040;

pub const FAN_REPORT_PIDFD: c_uint = 0x0000_0080;
pub const FAN_REPORT_TID: c_uint = 0x0000_0100;
pub const FAN_REPORT_FID: c_uint = 0x0000_0200;
pub const FAN_REPORT_DIR_FID: c_uint = 0x0000_0400;
pub const FAN_REPORT_NAME: c_uint = 0x0000_0800;
pub const FAN_REPORT_TARGET_FID: c_uint = 0x0000_1000;

pub const FAN_REPORT_DFID_NAME: c_uint = FAN_REPORT_DIR_FID | FAN_REPORT_NAME;
pub const FAN_REPORT_DFID_NAME_TARGET: c_uint =
    FAN_REPORT_DFID_NAME | FAN_REPORT_FID | FAN_REPORT_TARGET_FID;

pub const FAN_MARK_ADD: c_uint = 0x0000_0001;
pub const FAN_MARK_REMOVE: c_uint = 0x0000_0002;
pub const FAN_MARK_DONT_FOLLOW: c_uint = 0x0000_0004;
pub const FAN_MARK_ONLYDIR: c_uint = 0x0000_0008;
pub const FAN_MARK_IGNORED_MASK: c_uint = 0x0000_0020;
pub const FAN_MARK_IGNORED_SURV_MODIFY: c_uint = 0x0000_0040;
pub const FAN_MARK_FLUSH: c_uint = 0x0000_0080;
pub const FAN_MARK_EVICTABLE: c_uint = 0x0000_0200;
pub const FAN_MARK_IGNORE: c_uint = 0x0000_0400;

pub const FAN_MARK_INODE: c_uint = 0x0000_0000;
pub const FAN_MARK_MOUNT: c_uint = 0x0000_0010;
pub const FAN_MARK_FILESYSTEM: c_uint = 0x0000_0100;

pub const FAN_MARK_IGNORE_SURV: c_uint = FAN_MARK_IGNORE | FAN_MARK_IGNORED_SURV_MODIFY;

pub const FANOTIFY_METADATA_VERSION: u8 = 3;

pub const FAN_EVENT_INFO_TYPE_FID: u8 = 1;
pub const FAN_EVENT_INFO_TYPE_DFID_NAME: u8 = 2;
pub const FAN_EVENT_INFO_TYPE_DFID: u8 = 3;
pub const FAN_EVENT_INFO_TYPE_PIDFD: u8 = 4;
pub const FAN_EVENT_INFO_TYPE_ERROR: u8 = 5;

pub const FAN_EVENT_INFO_TYPE_OLD_DFID_NAME: u8 = 10;
pub const FAN_EVENT_INFO_TYPE_NEW_DFID_NAME: u8 = 12;

pub const FAN_RESPONSE_INFO_NONE: u8 = 0;
pub const FAN_RESPONSE_INFO_AUDIT_RULE: u8 = 1;

pub const FAN_ALLOW: u32 = 0x01;
pub const FAN_DENY: u32 = 0x02;
pub const FAN_AUDIT: u32 = 0x10;
pub const FAN_INFO: u32 = 0x20;

pub const FAN_NOFD: c_int = -1;
pub const FAN_NOPIDFD: c_int = FAN_NOFD;
pub const FAN_EPIDFD: c_int = -2;

// linux/futex.h
pub const FUTEX_WAIT: c_int = 0;
pub const FUTEX_WAKE: c_int = 1;
pub const FUTEX_FD: c_int = 2;
pub const FUTEX_REQUEUE: c_int = 3;
pub const FUTEX_CMP_REQUEUE: c_int = 4;
pub const FUTEX_WAKE_OP: c_int = 5;
pub const FUTEX_LOCK_PI: c_int = 6;
pub const FUTEX_UNLOCK_PI: c_int = 7;
pub const FUTEX_TRYLOCK_PI: c_int = 8;
pub const FUTEX_WAIT_BITSET: c_int = 9;
pub const FUTEX_WAKE_BITSET: c_int = 10;
pub const FUTEX_WAIT_REQUEUE_PI: c_int = 11;
pub const FUTEX_CMP_REQUEUE_PI: c_int = 12;
pub const FUTEX_LOCK_PI2: c_int = 13;

pub const FUTEX_PRIVATE_FLAG: c_int = 128;
pub const FUTEX_CLOCK_REALTIME: c_int = 256;
pub const FUTEX_CMD_MASK: c_int = !(FUTEX_PRIVATE_FLAG | FUTEX_CLOCK_REALTIME);

pub const FUTEX_WAITERS: u32 = 0x80000000;
pub const FUTEX_OWNER_DIED: u32 = 0x40000000;
pub const FUTEX_TID_MASK: u32 = 0x3fffffff;

pub const FUTEX_BITSET_MATCH_ANY: c_int = 0xffffffff;

pub const FUTEX_OP_SET: c_int = 0;
pub const FUTEX_OP_ADD: c_int = 1;
pub const FUTEX_OP_OR: c_int = 2;
pub const FUTEX_OP_ANDN: c_int = 3;
pub const FUTEX_OP_XOR: c_int = 4;

pub const FUTEX_OP_OPARG_SHIFT: c_int = 8;

pub const FUTEX_OP_CMP_EQ: c_int = 0;
pub const FUTEX_OP_CMP_NE: c_int = 1;
pub const FUTEX_OP_CMP_LT: c_int = 2;
pub const FUTEX_OP_CMP_LE: c_int = 3;
pub const FUTEX_OP_CMP_GT: c_int = 4;
pub const FUTEX_OP_CMP_GE: c_int = 5;

pub fn FUTEX_OP(op: c_int, oparg: c_int, cmp: c_int, cmparg: c_int) -> c_int {
    ((op & 0xf) << 28) | ((cmp & 0xf) << 24) | ((oparg & 0xfff) << 12) | (cmparg & 0xfff)
}

// linux/kexec.h
pub const KEXEC_ON_CRASH: c_int = 0x00000001;
pub const KEXEC_PRESERVE_CONTEXT: c_int = 0x00000002;
pub const KEXEC_ARCH_MASK: c_int = 0xffff0000;
pub const KEXEC_FILE_UNLOAD: c_int = 0x00000001;
pub const KEXEC_FILE_ON_CRASH: c_int = 0x00000002;
pub const KEXEC_FILE_NO_INITRAMFS: c_int = 0x00000004;

// linux/reboot.h
pub const LINUX_REBOOT_MAGIC1: c_int = 0xfee1dead;
pub const LINUX_REBOOT_MAGIC2: c_int = 672274793;
pub const LINUX_REBOOT_MAGIC2A: c_int = 85072278;
pub const LINUX_REBOOT_MAGIC2B: c_int = 369367448;
pub const LINUX_REBOOT_MAGIC2C: c_int = 537993216;

pub const LINUX_REBOOT_CMD_RESTART: c_int = 0x01234567;
pub const LINUX_REBOOT_CMD_HALT: c_int = 0xCDEF0123;
pub const LINUX_REBOOT_CMD_CAD_ON: c_int = 0x89ABCDEF;
pub const LINUX_REBOOT_CMD_CAD_OFF: c_int = 0x00000000;
pub const LINUX_REBOOT_CMD_POWER_OFF: c_int = 0x4321FEDC;
pub const LINUX_REBOOT_CMD_RESTART2: c_int = 0xA1B2C3D4;
pub const LINUX_REBOOT_CMD_SW_SUSPEND: c_int = 0xD000FCE2;
pub const LINUX_REBOOT_CMD_KEXEC: c_int = 0x45584543;

// linux/errqueue.h
pub const SO_EE_ORIGIN_NONE: u8 = 0;
pub const SO_EE_ORIGIN_LOCAL: u8 = 1;
pub const SO_EE_ORIGIN_ICMP: u8 = 2;
pub const SO_EE_ORIGIN_ICMP6: u8 = 3;
pub const SO_EE_ORIGIN_TXSTATUS: u8 = 4;
pub const SO_EE_ORIGIN_TIMESTAMPING: u8 = SO_EE_ORIGIN_TXSTATUS;

// linux/sctp.h
pub const SCTP_FUTURE_ASSOC: c_int = 0;
pub const SCTP_CURRENT_ASSOC: c_int = 1;
pub const SCTP_ALL_ASSOC: c_int = 2;
pub const SCTP_RTOINFO: c_int = 0;
pub const SCTP_ASSOCINFO: c_int = 1;
pub const SCTP_INITMSG: c_int = 2;
pub const SCTP_NODELAY: c_int = 3;
pub const SCTP_AUTOCLOSE: c_int = 4;
pub const SCTP_SET_PEER_PRIMARY_ADDR: c_int = 5;
pub const SCTP_PRIMARY_ADDR: c_int = 6;
pub const SCTP_ADAPTATION_LAYER: c_int = 7;
pub const SCTP_DISABLE_FRAGMENTS: c_int = 8;
pub const SCTP_PEER_ADDR_PARAMS: c_int = 9;
pub const SCTP_DEFAULT_SEND_PARAM: c_int = 10;
pub const SCTP_EVENTS: c_int = 11;
pub const SCTP_I_WANT_MAPPED_V4_ADDR: c_int = 12;
pub const SCTP_MAXSEG: c_int = 13;
pub const SCTP_STATUS: c_int = 14;
pub const SCTP_GET_PEER_ADDR_INFO: c_int = 15;
pub const SCTP_DELAYED_ACK_TIME: c_int = 16;
pub const SCTP_DELAYED_ACK: c_int = SCTP_DELAYED_ACK_TIME;
pub const SCTP_DELAYED_SACK: c_int = SCTP_DELAYED_ACK_TIME;
pub const SCTP_CONTEXT: c_int = 17;
pub const SCTP_FRAGMENT_INTERLEAVE: c_int = 18;
pub const SCTP_PARTIAL_DELIVERY_POINT: c_int = 19;
pub const SCTP_MAX_BURST: c_int = 20;
pub const SCTP_AUTH_CHUNK: c_int = 21;
pub const SCTP_HMAC_IDENT: c_int = 22;
pub const SCTP_AUTH_KEY: c_int = 23;
pub const SCTP_AUTH_ACTIVE_KEY: c_int = 24;
pub const SCTP_AUTH_DELETE_KEY: c_int = 25;
pub const SCTP_PEER_AUTH_CHUNKS: c_int = 26;
pub const SCTP_LOCAL_AUTH_CHUNKS: c_int = 27;
pub const SCTP_GET_ASSOC_NUMBER: c_int = 28;
pub const SCTP_GET_ASSOC_ID_LIST: c_int = 29;
pub const SCTP_AUTO_ASCONF: c_int = 30;
pub const SCTP_PEER_ADDR_THLDS: c_int = 31;
pub const SCTP_RECVRCVINFO: c_int = 32;
pub const SCTP_RECVNXTINFO: c_int = 33;
pub const SCTP_DEFAULT_SNDINFO: c_int = 34;
pub const SCTP_AUTH_DEACTIVATE_KEY: c_int = 35;
pub const SCTP_REUSE_PORT: c_int = 36;
pub const SCTP_PEER_ADDR_THLDS_V2: c_int = 37;
pub const SCTP_PR_SCTP_NONE: c_int = 0x0000;
pub const SCTP_PR_SCTP_TTL: c_int = 0x0010;
pub const SCTP_PR_SCTP_RTX: c_int = 0x0020;
pub const SCTP_PR_SCTP_PRIO: c_int = 0x0030;
pub const SCTP_PR_SCTP_MAX: c_int = SCTP_PR_SCTP_PRIO;
pub const SCTP_PR_SCTP_MASK: c_int = 0x0030;
pub const SCTP_ENABLE_RESET_STREAM_REQ: c_int = 0x01;
pub const SCTP_ENABLE_RESET_ASSOC_REQ: c_int = 0x02;
pub const SCTP_ENABLE_CHANGE_ASSOC_REQ: c_int = 0x04;
pub const SCTP_ENABLE_STRRESET_MASK: c_int = 0x07;
pub const SCTP_STREAM_RESET_INCOMING: c_int = 0x01;
pub const SCTP_STREAM_RESET_OUTGOING: c_int = 0x02;

pub const SCTP_INIT: c_int = 0;
pub const SCTP_SNDRCV: c_int = 1;
pub const SCTP_SNDINFO: c_int = 2;
pub const SCTP_RCVINFO: c_int = 3;
pub const SCTP_NXTINFO: c_int = 4;
pub const SCTP_PRINFO: c_int = 5;
pub const SCTP_AUTHINFO: c_int = 6;
pub const SCTP_DSTADDRV4: c_int = 7;
pub const SCTP_DSTADDRV6: c_int = 8;

pub const SCTP_UNORDERED: c_int = 1 << 0;
pub const SCTP_ADDR_OVER: c_int = 1 << 1;
pub const SCTP_ABORT: c_int = 1 << 2;
pub const SCTP_SACK_IMMEDIATELY: c_int = 1 << 3;
pub const SCTP_SENDALL: c_int = 1 << 6;
pub const SCTP_PR_SCTP_ALL: c_int = 1 << 7;
pub const SCTP_NOTIFICATION: c_int = MSG_NOTIFICATION;
pub const SCTP_EOF: c_int = crate::MSG_FIN;

/* DCCP socket options */
pub const DCCP_SOCKOPT_PACKET_SIZE: c_int = 1;
pub const DCCP_SOCKOPT_SERVICE: c_int = 2;
pub const DCCP_SOCKOPT_CHANGE_L: c_int = 3;
pub const DCCP_SOCKOPT_CHANGE_R: c_int = 4;
pub const DCCP_SOCKOPT_GET_CUR_MPS: c_int = 5;
pub const DCCP_SOCKOPT_SERVER_TIMEWAIT: c_int = 6;
pub const DCCP_SOCKOPT_SEND_CSCOV: c_int = 10;
pub const DCCP_SOCKOPT_RECV_CSCOV: c_int = 11;
pub const DCCP_SOCKOPT_AVAILABLE_CCIDS: c_int = 12;
pub const DCCP_SOCKOPT_CCID: c_int = 13;
pub const DCCP_SOCKOPT_TX_CCID: c_int = 14;
pub const DCCP_SOCKOPT_RX_CCID: c_int = 15;
pub const DCCP_SOCKOPT_QPOLICY_ID: c_int = 16;
pub const DCCP_SOCKOPT_QPOLICY_TXQLEN: c_int = 17;
pub const DCCP_SOCKOPT_CCID_RX_INFO: c_int = 128;
pub const DCCP_SOCKOPT_CCID_TX_INFO: c_int = 192;

/// maximum number of services provided on the same listening port
pub const DCCP_SERVICE_LIST_MAX_LEN: c_int = 32;

pub const CTL_KERN: c_int = 1;
pub const CTL_VM: c_int = 2;
pub const CTL_NET: c_int = 3;
pub const CTL_FS: c_int = 5;
pub const CTL_DEBUG: c_int = 6;
pub const CTL_DEV: c_int = 7;
pub const CTL_BUS: c_int = 8;
pub const CTL_ABI: c_int = 9;
pub const CTL_CPU: c_int = 10;

pub const CTL_BUS_ISA: c_int = 1;

pub const INOTIFY_MAX_USER_INSTANCES: c_int = 1;
pub const INOTIFY_MAX_USER_WATCHES: c_int = 2;
pub const INOTIFY_MAX_QUEUED_EVENTS: c_int = 3;

pub const KERN_OSTYPE: c_int = 1;
pub const KERN_OSRELEASE: c_int = 2;
pub const KERN_OSREV: c_int = 3;
pub const KERN_VERSION: c_int = 4;
pub const KERN_SECUREMASK: c_int = 5;
pub const KERN_PROF: c_int = 6;
pub const KERN_NODENAME: c_int = 7;
pub const KERN_DOMAINNAME: c_int = 8;
pub const KERN_PANIC: c_int = 15;
pub const KERN_REALROOTDEV: c_int = 16;
pub const KERN_SPARC_REBOOT: c_int = 21;
pub const KERN_CTLALTDEL: c_int = 22;
pub const KERN_PRINTK: c_int = 23;
pub const KERN_NAMETRANS: c_int = 24;
pub const KERN_PPC_HTABRECLAIM: c_int = 25;
pub const KERN_PPC_ZEROPAGED: c_int = 26;
pub const KERN_PPC_POWERSAVE_NAP: c_int = 27;
pub const KERN_MODPROBE: c_int = 28;
pub const KERN_SG_BIG_BUFF: c_int = 29;
pub const KERN_ACCT: c_int = 30;
pub const KERN_PPC_L2CR: c_int = 31;
pub const KERN_RTSIGNR: c_int = 32;
pub const KERN_RTSIGMAX: c_int = 33;
pub const KERN_SHMMAX: c_int = 34;
pub const KERN_MSGMAX: c_int = 35;
pub const KERN_MSGMNB: c_int = 36;
pub const KERN_MSGPOOL: c_int = 37;
pub const KERN_SYSRQ: c_int = 38;
pub const KERN_MAX_THREADS: c_int = 39;
pub const KERN_RANDOM: c_int = 40;
pub const KERN_SHMALL: c_int = 41;
pub const KERN_MSGMNI: c_int = 42;
pub const KERN_SEM: c_int = 43;
pub const KERN_SPARC_STOP_A: c_int = 44;
pub const KERN_SHMMNI: c_int = 45;
pub const KERN_OVERFLOWUID: c_int = 46;
pub const KERN_OVERFLOWGID: c_int = 47;
pub const KERN_SHMPATH: c_int = 48;
pub const KERN_HOTPLUG: c_int = 49;
pub const KERN_IEEE_EMULATION_WARNINGS: c_int = 50;
pub const KERN_S390_USER_DEBUG_LOGGING: c_int = 51;
pub const KERN_CORE_USES_PID: c_int = 52;
pub const KERN_TAINTED: c_int = 53;
pub const KERN_CADPID: c_int = 54;
pub const KERN_PIDMAX: c_int = 55;
pub const KERN_CORE_PATTERN: c_int = 56;
pub const KERN_PANIC_ON_OOPS: c_int = 57;
pub const KERN_HPPA_PWRSW: c_int = 58;
pub const KERN_HPPA_UNALIGNED: c_int = 59;
pub const KERN_PRINTK_RATELIMIT: c_int = 60;
pub const KERN_PRINTK_RATELIMIT_BURST: c_int = 61;
pub const KERN_PTY: c_int = 62;
pub const KERN_NGROUPS_MAX: c_int = 63;
pub const KERN_SPARC_SCONS_PWROFF: c_int = 64;
pub const KERN_HZ_TIMER: c_int = 65;
pub const KERN_UNKNOWN_NMI_PANIC: c_int = 66;
pub const KERN_BOOTLOADER_TYPE: c_int = 67;
pub const KERN_RANDOMIZE: c_int = 68;
pub const KERN_SETUID_DUMPABLE: c_int = 69;
pub const KERN_SPIN_RETRY: c_int = 70;
pub const KERN_ACPI_VIDEO_FLAGS: c_int = 71;
pub const KERN_IA64_UNALIGNED: c_int = 72;
pub const KERN_COMPAT_LOG: c_int = 73;
pub const KERN_MAX_LOCK_DEPTH: c_int = 74;
pub const KERN_NMI_WATCHDOG: c_int = 75;
pub const KERN_PANIC_ON_NMI: c_int = 76;

pub const VM_OVERCOMMIT_MEMORY: c_int = 5;
pub const VM_PAGE_CLUSTER: c_int = 10;
pub const VM_DIRTY_BACKGROUND: c_int = 11;
pub const VM_DIRTY_RATIO: c_int = 12;
pub const VM_DIRTY_WB_CS: c_int = 13;
pub const VM_DIRTY_EXPIRE_CS: c_int = 14;
pub const VM_NR_PDFLUSH_THREADS: c_int = 15;
pub const VM_OVERCOMMIT_RATIO: c_int = 16;
pub const VM_PAGEBUF: c_int = 17;
pub const VM_HUGETLB_PAGES: c_int = 18;
pub const VM_SWAPPINESS: c_int = 19;
pub const VM_LOWMEM_RESERVE_RATIO: c_int = 20;
pub const VM_MIN_FREE_KBYTES: c_int = 21;
pub const VM_MAX_MAP_COUNT: c_int = 22;
pub const VM_LAPTOP_MODE: c_int = 23;
pub const VM_BLOCK_DUMP: c_int = 24;
pub const VM_HUGETLB_GROUP: c_int = 25;
pub const VM_VFS_CACHE_PRESSURE: c_int = 26;
pub const VM_LEGACY_VA_LAYOUT: c_int = 27;
pub const VM_SWAP_TOKEN_TIMEOUT: c_int = 28;
pub const VM_DROP_PAGECACHE: c_int = 29;
pub const VM_PERCPU_PAGELIST_FRACTION: c_int = 30;
pub const VM_ZONE_RECLAIM_MODE: c_int = 31;
pub const VM_MIN_UNMAPPED: c_int = 32;
pub const VM_PANIC_ON_OOM: c_int = 33;
pub const VM_VDSO_ENABLED: c_int = 34;
pub const VM_MIN_SLAB: c_int = 35;

pub const NET_CORE: c_int = 1;
pub const NET_ETHER: c_int = 2;
pub const NET_802: c_int = 3;
pub const NET_UNIX: c_int = 4;
pub const NET_IPV4: c_int = 5;
pub const NET_IPX: c_int = 6;
pub const NET_ATALK: c_int = 7;
pub const NET_NETROM: c_int = 8;
pub const NET_AX25: c_int = 9;
pub const NET_BRIDGE: c_int = 10;
pub const NET_ROSE: c_int = 11;
pub const NET_IPV6: c_int = 12;
pub const NET_X25: c_int = 13;
pub const NET_TR: c_int = 14;
pub const NET_DECNET: c_int = 15;
pub const NET_ECONET: c_int = 16;
pub const NET_SCTP: c_int = 17;
pub const NET_LLC: c_int = 18;
pub const NET_NETFILTER: c_int = 19;
pub const NET_DCCP: c_int = 20;
pub const NET_IRDA: c_int = 412;

// include/linux/sched.h
/// I'm a virtual CPU.
pub const PF_VCPU: c_int = 0x00000001;
/// I am an IDLE thread.
pub const PF_IDLE: c_int = 0x00000002;
/// Getting shut down.
pub const PF_EXITING: c_int = 0x00000004;
/// Coredumps should ignore this task.
pub const PF_POSTCOREDUMP: c_int = 0x00000008;
/// Task is an IO worker.
pub const PF_IO_WORKER: c_int = 0x00000010;
/// I'm a workqueue worker.
pub const PF_WQ_WORKER: c_int = 0x00000020;
/// Forked but didn't exec.
pub const PF_FORKNOEXEC: c_int = 0x00000040;
/// Process policy on mce errors.
pub const PF_MCE_PROCESS: c_int = 0x00000080;
/// Used super-user privileges.
pub const PF_SUPERPRIV: c_int = 0x00000100;
/// Dumped core.
pub const PF_DUMPCORE: c_int = 0x00000200;
/// Killed by a signal.
pub const PF_SIGNALED: c_int = 0x00000400;
/// Allocating memory to free memory.
///
/// See `memalloc_noreclaim_save()`.
pub const PF_MEMALLOC: c_int = 0x00000800;
/// `set_user()` noticed that `RLIMIT_NPROC` was exceeded.
pub const PF_NPROC_EXCEEDED: c_int = 0x00001000;
/// If unset the fpu must be initialized before use.
pub const PF_USED_MATH: c_int = 0x00002000;
/// Kernel thread cloned from userspace thread.
pub const PF_USER_WORKER: c_int = 0x00004000;
/// This thread should not be frozen.
pub const PF_NOFREEZE: c_int = 0x00008000;
/// I am `kswapd`.
pub const PF_KSWAPD: c_int = 0x00020000;
/// All allocations inherit `GFP_NOFS`.
///
/// See `memalloc_nfs_save()`.
pub const PF_MEMALLOC_NOFS: c_int = 0x00040000;
/// All allocations inherit `GFP_NOIO`.
///
/// See `memalloc_noio_save()`.
pub const PF_MEMALLOC_NOIO: c_int = 0x00080000;
/// Throttle writes only against the bdi I write to, I am cleaning
/// dirty pages from some other bdi.
pub const PF_LOCAL_THROTTLE: c_int = 0x00100000;
/// I am a kernel thread.
pub const PF_KTHREAD: c_int = 0x00200000;
/// Randomize virtual address space.
pub const PF_RANDOMIZE: c_int = 0x00400000;
/// Userland is not allowed to meddle with `cpus_mask`.
pub const PF_NO_SETAFFINITY: c_int = 0x04000000;
/// Early kill for mce process policy.
pub const PF_MCE_EARLY: c_int = 0x08000000;
/// Allocations constrained to zones which allow long term pinning.
///
/// See `memalloc_pin_save()`.
pub const PF_MEMALLOC_PIN: c_int = 0x10000000;
/// Plug has ts that needs updating.
pub const PF_BLOCK_TS: c_int = 0x20000000;
/// This thread called `freeze_processes()` and should not be frozen.
pub const PF_SUSPEND_TASK: c_int = PF_SUSPEND_TASK_UINT as _;
// The used value is the highest possible bit fitting on 32 bits, so directly
// defining it as a signed integer causes the compiler to report an overflow.
// Use instead a private intermediary that assuringly has the correct type and
// cast it where necessary to the wanted final type, which preserves the
// desired information as-is in terms of integer representation.
const PF_SUSPEND_TASK_UINT: c_uint = 0x80000000;

pub const CLONE_PIDFD: c_int = 0x1000;

pub const SCHED_FLAG_RESET_ON_FORK: c_int = 0x01;
pub const SCHED_FLAG_RECLAIM: c_int = 0x02;
pub const SCHED_FLAG_DL_OVERRUN: c_int = 0x04;
pub const SCHED_FLAG_KEEP_POLICY: c_int = 0x08;
pub const SCHED_FLAG_KEEP_PARAMS: c_int = 0x10;
pub const SCHED_FLAG_UTIL_CLAMP_MIN: c_int = 0x20;
pub const SCHED_FLAG_UTIL_CLAMP_MAX: c_int = 0x40;

// linux/if_xdp.h
pub const XDP_SHARED_UMEM: crate::__u16 = 1 << 0;
pub const XDP_COPY: crate::__u16 = 1 << 1;
pub const XDP_ZEROCOPY: crate::__u16 = 1 << 2;
pub const XDP_USE_NEED_WAKEUP: crate::__u16 = 1 << 3;
pub const XDP_USE_SG: crate::__u16 = 1 << 4;

pub const XDP_UMEM_UNALIGNED_CHUNK_FLAG: crate::__u32 = 1 << 0;

pub const XDP_RING_NEED_WAKEUP: crate::__u32 = 1 << 0;

pub const XDP_MMAP_OFFSETS: c_int = 1;
pub const XDP_RX_RING: c_int = 2;
pub const XDP_TX_RING: c_int = 3;
pub const XDP_UMEM_REG: c_int = 4;
pub const XDP_UMEM_FILL_RING: c_int = 5;
pub const XDP_UMEM_COMPLETION_RING: c_int = 6;
pub const XDP_STATISTICS: c_int = 7;
pub const XDP_OPTIONS: c_int = 8;

pub const XDP_OPTIONS_ZEROCOPY: crate::__u32 = 1 << 0;

pub const XDP_PGOFF_RX_RING: crate::off_t = 0;
pub const XDP_PGOFF_TX_RING: crate::off_t = 0x80000000;
pub const XDP_UMEM_PGOFF_FILL_RING: crate::c_ulonglong = 0x100000000;
pub const XDP_UMEM_PGOFF_COMPLETION_RING: crate::c_ulonglong = 0x180000000;

pub const XSK_UNALIGNED_BUF_OFFSET_SHIFT: crate::c_int = 48;
pub const XSK_UNALIGNED_BUF_ADDR_MASK: crate::c_ulonglong =
    (1 << XSK_UNALIGNED_BUF_OFFSET_SHIFT) - 1;

pub const XDP_PKT_CONTD: crate::__u32 = 1 << 0;

pub const XDP_UMEM_TX_SW_CSUM: crate::__u32 = 1 << 1;
pub const XDP_UMEM_TX_METADATA_LEN: crate::__u32 = 1 << 2;

pub const XDP_TXMD_FLAGS_TIMESTAMP: crate::__u32 = 1 << 0;
pub const XDP_TXMD_FLAGS_CHECKSUM: crate::__u32 = 1 << 1;

pub const XDP_TX_METADATA: crate::__u32 = 1 << 1;

pub const SOL_XDP: c_int = 283;

// linux/mount.h
pub const MOUNT_ATTR_RDONLY: crate::__u64 = 0x00000001;
pub const MOUNT_ATTR_NOSUID: crate::__u64 = 0x00000002;
pub const MOUNT_ATTR_NODEV: crate::__u64 = 0x00000004;
pub const MOUNT_ATTR_NOEXEC: crate::__u64 = 0x00000008;
pub const MOUNT_ATTR__ATIME: crate::__u64 = 0x00000070;
pub const MOUNT_ATTR_RELATIME: crate::__u64 = 0x00000000;
pub const MOUNT_ATTR_NOATIME: crate::__u64 = 0x00000010;
pub const MOUNT_ATTR_STRICTATIME: crate::__u64 = 0x00000020;
pub const MOUNT_ATTR_NODIRATIME: crate::__u64 = 0x00000080;
pub const MOUNT_ATTR_IDMAP: crate::__u64 = 0x00100000;
pub const MOUNT_ATTR_NOSYMFOLLOW: crate::__u64 = 0x00200000;

pub const MOUNT_ATTR_SIZE_VER0: c_int = 32;

pub const SCHED_FLAG_KEEP_ALL: c_int = SCHED_FLAG_KEEP_POLICY | SCHED_FLAG_KEEP_PARAMS;

pub const SCHED_FLAG_UTIL_CLAMP: c_int = SCHED_FLAG_UTIL_CLAMP_MIN | SCHED_FLAG_UTIL_CLAMP_MAX;

pub const SCHED_FLAG_ALL: c_int = SCHED_FLAG_RESET_ON_FORK
    | SCHED_FLAG_RECLAIM
    | SCHED_FLAG_DL_OVERRUN
    | SCHED_FLAG_KEEP_ALL
    | SCHED_FLAG_UTIL_CLAMP;

// ioctl_eventpoll: added in Linux 6.9
pub const EPIOCSPARAMS: Ioctl = 0x40088a01;
pub const EPIOCGPARAMS: Ioctl = 0x80088a02;

// siginfo.h
pub const SI_DETHREAD: c_int = -7;
pub const TRAP_PERF: c_int = 6;

f! {
    pub fn SCTP_PR_INDEX(policy: c_int) -> c_int {
        policy >> (4 - 1)
    }

    pub fn SCTP_PR_POLICY(policy: c_int) -> c_int {
        policy & SCTP_PR_SCTP_MASK
    }

    pub fn SCTP_PR_SET_POLICY(flags: &mut c_int, policy: c_int) -> () {
        *flags &= !SCTP_PR_SCTP_MASK;
        *flags |= policy;
    }

    pub fn SO_EE_OFFENDER(ee: *const crate::sock_extended_err) -> *mut crate::sockaddr {
        ee.offset(1) as *mut crate::sockaddr
    }

    pub fn TPACKET_ALIGN(x: usize) -> usize {
        (x + TPACKET_ALIGNMENT - 1) & !(TPACKET_ALIGNMENT - 1)
    }

    pub fn BPF_CLASS(code: __u32) -> __u32 {
        code & 0x07
    }

    pub fn BPF_SIZE(code: __u32) -> __u32 {
        code & 0x18
    }

    pub fn BPF_MODE(code: __u32) -> __u32 {
        code & 0xe0
    }

    pub fn BPF_OP(code: __u32) -> __u32 {
        code & 0xf0
    }

    pub fn BPF_SRC(code: __u32) -> __u32 {
        code & 0x08
    }

    pub fn BPF_RVAL(code: __u32) -> __u32 {
        code & 0x18
    }

    pub fn BPF_MISCOP(code: __u32) -> __u32 {
        code & 0xf8
    }

    pub fn BPF_STMT(code: __u16, k: __u32) -> sock_filter {
        sock_filter {
            code,
            jt: 0,
            jf: 0,
            k,
        }
    }

    pub fn BPF_JUMP(code: __u16, k: __u32, jt: __u8, jf: __u8) -> sock_filter {
        sock_filter { code, jt, jf, k }
    }

    #[cfg(target_env = "gnu")]
    pub fn SUN_LEN(s: crate::sockaddr_un) -> usize {
        offset_of!(crate::sockaddr_un, sun_path) + crate::strlen(s.sun_path.as_ptr())
    }

    #[cfg(target_env = "musl")]
    pub fn SUN_LEN(s: crate::sockaddr_un) -> usize {
        2 * crate::strlen(s.sun_path.as_ptr())
    }
}

safe_f! {
    pub const fn SCTP_PR_TTL_ENABLED(policy: c_int) -> bool {
        policy == SCTP_PR_SCTP_TTL
    }

    pub const fn SCTP_PR_RTX_ENABLED(policy: c_int) -> bool {
        policy == SCTP_PR_SCTP_RTX
    }

    pub const fn SCTP_PR_PRIO_ENABLED(policy: c_int) -> bool {
        policy == SCTP_PR_SCTP_PRIO
    }
}

// These functions are not available on OpenHarmony
cfg_if! {
    if #[cfg(not(target_env = "ohos"))] {
        extern "C" {
            // Only `getspnam_r` is implemented for musl, out of all of the reenterant
            // functions from `shadow.h`.
            // https://git.musl-libc.org/cgit/musl/tree/include/shadow.h
            pub fn getspnam_r(
                name: *const c_char,
                spbuf: *mut crate::spwd,
                buf: *mut c_char,
                buflen: size_t,
                spbufp: *mut *mut crate::spwd,
            ) -> c_int;

            pub fn mq_open(name: *const c_char, oflag: c_int, ...) -> mqd_t;
            pub fn mq_close(mqd: mqd_t) -> c_int;
            pub fn mq_unlink(name: *const c_char) -> c_int;
            pub fn mq_receive(
                mqd: mqd_t,
                msg_ptr: *mut c_char,
                msg_len: size_t,
                msg_prio: *mut c_uint,
            ) -> ssize_t;
            #[cfg_attr(
                any(gnu_time_bits64, musl32_time64),
                link_name = "__mq_timedreceive_time64"
            )]
            pub fn mq_timedreceive(
                mqd: mqd_t,
                msg_ptr: *mut c_char,
                msg_len: size_t,
                msg_prio: *mut c_uint,
                abs_timeout: *const crate::timespec,
            ) -> ssize_t;
            pub fn mq_send(
                mqd: mqd_t,
                msg_ptr: *const c_char,
                msg_len: size_t,
                msg_prio: c_uint,
            ) -> c_int;
            #[cfg_attr(
                any(gnu_time_bits64, musl32_time64),
                link_name = "__mq_timedsend_time64"
            )]
            pub fn mq_timedsend(
                mqd: mqd_t,
                msg_ptr: *const c_char,
                msg_len: size_t,
                msg_prio: c_uint,
                abs_timeout: *const crate::timespec,
            ) -> c_int;
            pub fn mq_getattr(mqd: mqd_t, attr: *mut crate::mq_attr) -> c_int;
            pub fn mq_setattr(
                mqd: mqd_t,
                newattr: *const crate::mq_attr,
                oldattr: *mut crate::mq_attr,
            ) -> c_int;
        }
    }
}

extern "C" {
    pub fn mrand48() -> c_long;
    pub fn seed48(xseed: *mut c_ushort) -> *mut c_ushort;
    pub fn lcong48(p: *mut c_ushort);

    #[cfg_attr(gnu_time_bits64, link_name = "__lutimes64")]
    #[cfg_attr(musl32_time64, link_name = "__lutimes_time64")]
    pub fn lutimes(file: *const c_char, times: *const crate::timeval) -> c_int;

    pub fn shm_open(name: *const c_char, oflag: c_int, mode: mode_t) -> c_int;
    pub fn shm_unlink(name: *const c_char) -> c_int;

    // System V IPC
    pub fn ftok(pathname: *const c_char, proj_id: c_int) -> crate::key_t;
    pub fn semget(key: crate::key_t, nsems: c_int, semflag: c_int) -> c_int;
    pub fn semop(semid: c_int, sops: *mut crate::sembuf, nsops: size_t) -> c_int;
    #[cfg_attr(gnu_time_bits64, link_name = "__semctl64")]
    pub fn semctl(semid: c_int, semnum: c_int, cmd: c_int, ...) -> c_int;
    #[cfg_attr(gnu_time_bits64, link_name = "__msgctl64")]
    pub fn msgctl(msqid: c_int, cmd: c_int, buf: *mut msqid_ds) -> c_int;
    pub fn msgget(key: crate::key_t, msgflg: c_int) -> c_int;
    pub fn msgrcv(
        msqid: c_int,
        msgp: *mut c_void,
        msgsz: size_t,
        msgtyp: c_long,
        msgflg: c_int,
    ) -> ssize_t;
    pub fn msgsnd(msqid: c_int, msgp: *const c_void, msgsz: size_t, msgflg: c_int) -> c_int;

    #[cfg_attr(gnu_file_offset_bits64, link_name = "fallocate64")]
    pub fn fallocate(fd: c_int, mode: c_int, offset: off_t, len: off_t) -> c_int;
    #[cfg_attr(gnu_file_offset_bits64, link_name = "posix_fallocate64")]
    pub fn posix_fallocate(fd: c_int, offset: off_t, len: off_t) -> c_int;
    pub fn readahead(fd: c_int, offset: off64_t, count: size_t) -> ssize_t;
    pub fn getxattr(
        path: *const c_char,
        name: *const c_char,
        value: *mut c_void,
        size: size_t,
    ) -> ssize_t;
    pub fn lgetxattr(
        path: *const c_char,
        name: *const c_char,
        value: *mut c_void,
        size: size_t,
    ) -> ssize_t;
    pub fn fgetxattr(
        filedes: c_int,
        name: *const c_char,
        value: *mut c_void,
        size: size_t,
    ) -> ssize_t;
    pub fn setxattr(
        path: *const c_char,
        name: *const c_char,
        value: *const c_void,
        size: size_t,
        flags: c_int,
    ) -> c_int;
    pub fn lsetxattr(
        path: *const c_char,
        name: *const c_char,
        value: *const c_void,
        size: size_t,
        flags: c_int,
    ) -> c_int;
    pub fn fsetxattr(
        filedes: c_int,
        name: *const c_char,
        value: *const c_void,
        size: size_t,
        flags: c_int,
    ) -> c_int;
    pub fn listxattr(path: *const c_char, list: *mut c_char, size: size_t) -> ssize_t;
    pub fn llistxattr(path: *const c_char, list: *mut c_char, size: size_t) -> ssize_t;
    pub fn flistxattr(filedes: c_int, list: *mut c_char, size: size_t) -> ssize_t;
    pub fn removexattr(path: *const c_char, name: *const c_char) -> c_int;
    pub fn lremovexattr(path: *const c_char, name: *const c_char) -> c_int;
    pub fn fremovexattr(filedes: c_int, name: *const c_char) -> c_int;
    pub fn signalfd(fd: c_int, mask: *const crate::sigset_t, flags: c_int) -> c_int;
    pub fn timerfd_create(clockid: crate::clockid_t, flags: c_int) -> c_int;
    #[cfg_attr(any(gnu_time_bits64, musl32_time64), link_name = "__timerfd_gettime64")]
    pub fn timerfd_gettime(fd: c_int, curr_value: *mut crate::itimerspec) -> c_int;
    #[cfg_attr(any(gnu_time_bits64, musl32_time64), link_name = "__timerfd_settime64")]
    pub fn timerfd_settime(
        fd: c_int,
        flags: c_int,
        new_value: *const crate::itimerspec,
        old_value: *mut crate::itimerspec,
    ) -> c_int;
    pub fn quotactl(cmd: c_int, special: *const c_char, id: c_int, data: *mut c_char) -> c_int;
    pub fn epoll_pwait(
        epfd: c_int,
        events: *mut crate::epoll_event,
        maxevents: c_int,
        timeout: c_int,
        sigmask: *const crate::sigset_t,
    ) -> c_int;
    pub fn dup3(oldfd: c_int, newfd: c_int, flags: c_int) -> c_int;
    #[cfg_attr(gnu_time_bits64, link_name = "__sigtimedwait64")]
    #[cfg_attr(musl32_time64, link_name = "__sigtimedwait_time64")]
    pub fn sigtimedwait(
        set: *const sigset_t,
        info: *mut siginfo_t,
        timeout: *const crate::timespec,
    ) -> c_int;
    pub fn sigwaitinfo(set: *const sigset_t, info: *mut siginfo_t) -> c_int;
    pub fn accept4(fd: c_int, addr: *mut crate::sockaddr, len: *mut socklen_t, flg: c_int)
        -> c_int;
    pub fn reboot(how_to: c_int) -> c_int;
    pub fn setfsgid(gid: crate::gid_t) -> c_int;
    pub fn setfsuid(uid: crate::uid_t) -> c_int;

    // Not available now on Android
    pub fn mkfifoat(dirfd: c_int, pathname: *const c_char, mode: mode_t) -> c_int;
    pub fn sync_file_range(fd: c_int, offset: off64_t, nbytes: off64_t, flags: c_uint) -> c_int;

    pub fn posix_madvise(addr: *mut c_void, len: size_t, advice: c_int) -> c_int;

    pub fn remap_file_pages(
        addr: *mut c_void,
        size: size_t,
        prot: c_int,
        pgoff: size_t,
        flags: c_int,
    ) -> c_int;
    #[cfg_attr(gnu_file_offset_bits64, link_name = "mkstemps64")]
    pub fn mkstemps(template: *mut c_char, suffixlen: c_int) -> c_int;

    pub fn vhangup() -> c_int;
    pub fn sync();
    pub fn syncfs(fd: c_int) -> c_int;
    pub fn syscall(num: c_long, ...) -> c_long;
    pub fn sched_setaffinity(
        pid: crate::pid_t,
        cpusetsize: size_t,
        cpuset: *const crate::cpu_set_t,
    ) -> c_int;
    pub fn epoll_create(size: c_int) -> c_int;
    pub fn epoll_create1(flags: c_int) -> c_int;
    pub fn epoll_wait(
        epfd: c_int,
        events: *mut crate::epoll_event,
        maxevents: c_int,
        timeout: c_int,
    ) -> c_int;
    pub fn epoll_ctl(epfd: c_int, op: c_int, fd: c_int, event: *mut crate::epoll_event) -> c_int;
    pub fn unshare(flags: c_int) -> c_int;
    pub fn umount(target: *const c_char) -> c_int;
    pub fn tee(fd_in: c_int, fd_out: c_int, len: size_t, flags: c_uint) -> ssize_t;
    pub fn splice(
        fd_in: c_int,
        off_in: *mut loff_t,
        fd_out: c_int,
        off_out: *mut loff_t,
        len: size_t,
        flags: c_uint,
    ) -> ssize_t;
    pub fn eventfd(initval: c_uint, flags: c_int) -> c_int;
    pub fn eventfd_read(fd: c_int, value: *mut eventfd_t) -> c_int;
    pub fn eventfd_write(fd: c_int, value: eventfd_t) -> c_int;

    #[cfg_attr(gnu_time_bits64, link_name = "__sched_rr_get_interval64")]
    #[cfg_attr(musl32_time64, link_name = "__sched_rr_get_interval_time64")]
    pub fn sched_rr_get_interval(pid: crate::pid_t, tp: *mut crate::timespec) -> c_int;
    pub fn sched_setparam(pid: crate::pid_t, param: *const crate::sched_param) -> c_int;
    pub fn setns(fd: c_int, nstype: c_int) -> c_int;
    pub fn swapoff(path: *const c_char) -> c_int;
    pub fn vmsplice(fd: c_int, iov: *const crate::iovec, nr_segs: size_t, flags: c_uint)
        -> ssize_t;
    pub fn personality(persona: c_ulong) -> c_int;
    pub fn sched_getparam(pid: crate::pid_t, param: *mut crate::sched_param) -> c_int;
    pub fn clone(
        cb: extern "C" fn(*mut c_void) -> c_int,
        child_stack: *mut c_void,
        flags: c_int,
        arg: *mut c_void,
        ...
    ) -> c_int;
    pub fn sched_getscheduler(pid: crate::pid_t) -> c_int;
    #[cfg_attr(
        any(gnu_time_bits64, musl32_time64),
        link_name = "__clock_nanosleep_time64"
    )]
    pub fn clock_nanosleep(
        clk_id: crate::clockid_t,
        flags: c_int,
        rqtp: *const crate::timespec,
        rmtp: *mut crate::timespec,
    ) -> c_int;
    pub fn umount2(target: *const c_char, flags: c_int) -> c_int;
    pub fn swapon(path: *const c_char, swapflags: c_int) -> c_int;
    pub fn sched_setscheduler(
        pid: crate::pid_t,
        policy: c_int,
        param: *const crate::sched_param,
    ) -> c_int;
    #[cfg_attr(gnu_file_offset_bits64, link_name = "sendfile64")]
    pub fn sendfile(out_fd: c_int, in_fd: c_int, offset: *mut off_t, count: size_t) -> ssize_t;
    pub fn sigaltstack(ss: *const stack_t, oss: *mut stack_t) -> c_int;
    pub fn getdtablesize() -> c_int;
    pub fn getgrouplist(
        user: *const c_char,
        group: crate::gid_t,
        groups: *mut crate::gid_t,
        ngroups: *mut c_int,
    ) -> c_int;
    pub fn posix_spawn(
        pid: *mut crate::pid_t,
        path: *const c_char,
        file_actions: *const crate::posix_spawn_file_actions_t,
        attrp: *const crate::posix_spawnattr_t,
        argv: *const *mut c_char,
        envp: *const *mut c_char,
    ) -> c_int;
    pub fn posix_spawnp(
        pid: *mut crate::pid_t,
        file: *const c_char,
        file_actions: *const crate::posix_spawn_file_actions_t,
        attrp: *const crate::posix_spawnattr_t,
        argv: *const *mut c_char,
        envp: *const *mut c_char,
    ) -> c_int;
    pub fn posix_spawnattr_init(attr: *mut posix_spawnattr_t) -> c_int;
    pub fn posix_spawnattr_destroy(attr: *mut posix_spawnattr_t) -> c_int;
    pub fn posix_spawnattr_getsigdefault(
        attr: *const posix_spawnattr_t,
        default: *mut crate::sigset_t,
    ) -> c_int;
    pub fn posix_spawnattr_setsigdefault(
        attr: *mut posix_spawnattr_t,
        default: *const crate::sigset_t,
    ) -> c_int;
    pub fn posix_spawnattr_getsigmask(
        attr: *const posix_spawnattr_t,
        default: *mut crate::sigset_t,
    ) -> c_int;
    pub fn posix_spawnattr_setsigmask(
        attr: *mut posix_spawnattr_t,
        default: *const crate::sigset_t,
    ) -> c_int;
    pub fn posix_spawnattr_getflags(attr: *const posix_spawnattr_t, flags: *mut c_short) -> c_int;
    pub fn posix_spawnattr_setflags(attr: *mut posix_spawnattr_t, flags: c_short) -> c_int;
    pub fn posix_spawnattr_getpgroup(
        attr: *const posix_spawnattr_t,
        flags: *mut crate::pid_t,
    ) -> c_int;
    pub fn posix_spawnattr_setpgroup(attr: *mut posix_spawnattr_t, flags: crate::pid_t) -> c_int;
    pub fn posix_spawnattr_getschedpolicy(
        attr: *const posix_spawnattr_t,
        flags: *mut c_int,
    ) -> c_int;
    pub fn posix_spawnattr_setschedpolicy(attr: *mut posix_spawnattr_t, flags: c_int) -> c_int;
    pub fn posix_spawnattr_getschedparam(
        attr: *const posix_spawnattr_t,
        param: *mut crate::sched_param,
    ) -> c_int;
    pub fn posix_spawnattr_setschedparam(
        attr: *mut posix_spawnattr_t,
        param: *const crate::sched_param,
    ) -> c_int;

    pub fn posix_spawn_file_actions_init(actions: *mut posix_spawn_file_actions_t) -> c_int;
    pub fn posix_spawn_file_actions_destroy(actions: *mut posix_spawn_file_actions_t) -> c_int;
    pub fn posix_spawn_file_actions_addopen(
        actions: *mut posix_spawn_file_actions_t,
        fd: c_int,
        path: *const c_char,
        oflag: c_int,
        mode: mode_t,
    ) -> c_int;
    pub fn posix_spawn_file_actions_addclose(
        actions: *mut posix_spawn_file_actions_t,
        fd: c_int,
    ) -> c_int;
    pub fn posix_spawn_file_actions_adddup2(
        actions: *mut posix_spawn_file_actions_t,
        fd: c_int,
        newfd: c_int,
    ) -> c_int;
    pub fn fread_unlocked(
        buf: *mut c_void,
        size: size_t,
        nobj: size_t,
        stream: *mut crate::FILE,
    ) -> size_t;
    pub fn inotify_rm_watch(fd: c_int, wd: c_int) -> c_int;
    pub fn inotify_init() -> c_int;
    pub fn inotify_init1(flags: c_int) -> c_int;
    pub fn inotify_add_watch(fd: c_int, path: *const c_char, mask: u32) -> c_int;
    pub fn fanotify_init(flags: c_uint, event_f_flags: c_uint) -> c_int;

    pub fn gethostid() -> c_long;

    pub fn klogctl(syslog_type: c_int, bufp: *mut c_char, len: c_int) -> c_int;
}

// LFS64 extensions
//
// * musl has 64-bit versions only so aliases the LFS64 symbols to the standard ones
cfg_if! {
    if #[cfg(not(any(target_env = "musl", target_env = "ohos")))] {
        extern "C" {
            pub fn fallocate64(fd: c_int, mode: c_int, offset: off64_t, len: off64_t) -> c_int;
            pub fn fgetpos64(stream: *mut crate::FILE, ptr: *mut crate::fpos64_t) -> c_int;
            pub fn fopen64(filename: *const c_char, mode: *const c_char) -> *mut crate::FILE;
            pub fn posix_fallocate64(fd: c_int, offset: off64_t, len: off64_t) -> c_int;
            pub fn sendfile64(
                out_fd: c_int,
                in_fd: c_int,
                offset: *mut off64_t,
                count: size_t,
            ) -> ssize_t;
            pub fn tmpfile64() -> *mut crate::FILE;
        }
    }
}

cfg_if! {
    if #[cfg(target_env = "uclibc")] {
        mod uclibc;
        pub use self::uclibc::*;
    } else if #[cfg(any(target_env = "musl", target_env = "ohos"))] {
        mod musl;
        pub use self::musl::*;
    } else if #[cfg(target_env = "gnu")] {
        mod gnu;
        pub use self::gnu::*;
    }
}

mod arch;
pub use self::arch::*;
