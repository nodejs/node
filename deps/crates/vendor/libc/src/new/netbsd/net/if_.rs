//! Header: `net/if.h`
//!
//! <https://github.com/NetBSD/src/tree/trunk/sys/timex.h>

use crate::prelude::*;
use crate::IFNAMSIZ;

s! {
    pub struct if_data {
        pub ifi_type: c_uchar,
        pub ifi_addrlen: c_uchar,
        pub ifi_hdrlen: c_uchar,
        pub ifi_link_state: c_int,
        pub ifi_mtu: u64,
        pub ifi_metric: u64,
        pub ifi_baudrate: u64,
        pub ifi_ipackets: u64,
        pub ifi_ierrors: u64,
        pub ifi_opackets: u64,
        pub ifi_oerrors: u64,
        pub ifi_collisions: u64,
        pub ifi_ibytes: u64,
        pub ifi_obytes: u64,
        pub ifi_imcasts: u64,
        pub ifi_omcasts: u64,
        pub ifi_iqdrops: u64,
        pub ifi_noproto: u64,
        pub ifi_lastchange: crate::timespec,
    }
}

pub const LINK_STATE_UNKNOWN: c_int = 0; // link invalid/unknown
pub const LINK_STATE_DOWN: c_int = 1; // link is down
pub const LINK_STATE_UP: c_int = 2; // link is up

pub const IFF_UP: c_int = 0x0001; // interface is up
pub const IFF_BROADCAST: c_int = 0x0002; // broadcast address valid
pub const IFF_DEBUG: c_int = 0x0004; // turn on debugging
pub const IFF_LOOPBACK: c_int = 0x0008; // is a loopback net
pub const IFF_POINTOPOINT: c_int = 0x0010; // interface is point-to-point link
pub const IFF_RUNNING: c_int = 0x0040; // resources allocated
pub const IFF_NOARP: c_int = 0x0080; // no address resolution protocol
pub const IFF_PROMISC: c_int = 0x0100; // receive all packets
pub const IFF_ALLMULTI: c_int = 0x0200; // receive all multicast packets
pub const IFF_OACTIVE: c_int = 0x0400; // transmission in progress
pub const IFF_SIMPLEX: c_int = 0x0800; // can't hear own transmissions
pub const IFF_LINK0: c_int = 0x1000; // per link layer defined bit
pub const IFF_LINK1: c_int = 0x2000; // per link layer defined bit
pub const IFF_LINK2: c_int = 0x4000; // per link layer defined bit
pub const IFF_MULTICAST: c_int = 0x8000; // supports multicast

s! {
    #[repr(C, align(8))]
    pub struct if_msghdr {
        pub ifm_msglen: c_ushort,
        pub ifm_version: c_uchar,
        pub ifm_type: c_uchar,
        pub ifm_addrs: c_int,
        pub ifm_flags: c_int,
        pub ifm_index: c_ushort,
        pub ifm_data: if_data,
    }
}

s_no_extra_traits! {
    pub struct ifreq {
        pub ifr_name: [c_char; IFNAMSIZ],
        pub ifr_ifru: __c_anonymous_ifr_ifru,
    }

    pub union __c_anonymous_ifr_ifru {
        pub ifru_addr: crate::sockaddr,
        pub ifru_dstaddr: crate::sockaddr,
        pub ifru_broadaddr: crate::sockaddr,
        pub space: crate::sockaddr_storage,
        pub ifru_flags: c_short,
        pub ifru_addrflags: c_int,
        pub ifru_metrics: c_int,
        pub ifru_mtu: c_int,
        pub ifru_dlt: c_int,
        pub ifru_value: c_uint,
        pub ifru_data: *mut c_void,
        // buf and buflen are deprecated but they contribute to union size
        ifru_b: __c_anonymous_ifr_ifru_ifru_b,
    }

    struct __c_anonymous_ifr_ifru_ifru_b {
        b_buflen: u32,
        b_buf: *mut c_void,
    }

    pub struct ifconf {
        pub ifc_len: c_int,
        pub ifc_ifcu: __c_anonymous_ifc_ifcu,
    }

    pub union __c_anonymous_ifc_ifcu {
        pub ifcu_buf: *mut c_void,
        pub ifcu_req: *mut ifreq,
    }
}
