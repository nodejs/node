//! 32-bit specific Apple (ios/darwin) definitions

use crate::prelude::*;

pub type boolean_t = c_int;

s! {
    pub struct if_data {
        pub ifi_type: c_uchar,
        pub ifi_typelen: c_uchar,
        pub ifi_physical: c_uchar,
        pub ifi_addrlen: c_uchar,
        pub ifi_hdrlen: c_uchar,
        pub ifi_recvquota: c_uchar,
        pub ifi_xmitquota: c_uchar,
        pub ifi_unused1: c_uchar,
        pub ifi_mtu: u32,
        pub ifi_metric: u32,
        pub ifi_baudrate: u32,
        pub ifi_ipackets: u32,
        pub ifi_ierrors: u32,
        pub ifi_opackets: u32,
        pub ifi_oerrors: u32,
        pub ifi_collisions: u32,
        pub ifi_ibytes: u32,
        pub ifi_obytes: u32,
        pub ifi_imcasts: u32,
        pub ifi_omcasts: u32,
        pub ifi_iqdrops: u32,
        pub ifi_noproto: u32,
        pub ifi_recvtiming: u32,
        pub ifi_xmittiming: u32,
        pub ifi_lastchange: crate::timeval,
        pub ifi_unused2: u32,
        pub ifi_hwassist: u32,
        pub ifi_reserved1: u32,
        pub ifi_reserved2: u32,
    }

    pub struct bpf_hdr {
        pub bh_tstamp: crate::timeval,
        pub bh_caplen: u32,
        pub bh_datalen: u32,
        pub bh_hdrlen: c_ushort,
    }

    pub struct malloc_zone_t {
        __private: [crate::uintptr_t; 18], // FIXME(macos): keeping private for now
    }
}

s_no_extra_traits! {
    #[repr(align(16))]
    pub struct max_align_t {
        priv_: [f64; 2],
    }
}

#[doc(hidden)]
#[deprecated(since = "0.2.55")]
pub const NET_RT_MAXID: c_int = 10;

pub const TIOCTIMESTAMP: c_ulong = 0x40087459;
pub const TIOCDCDTIMESTAMP: c_ulong = 0x40087458;

pub const BIOCSETF: c_ulong = 0x80084267;
pub const BIOCSRTIMEOUT: c_ulong = 0x8008426d;
pub const BIOCGRTIMEOUT: c_ulong = 0x4008426e;
pub const BIOCSETFNR: c_ulong = 0x8008427e;

extern "C" {
    pub fn exchangedata(path1: *const c_char, path2: *const c_char, options: c_ulong) -> c_int;
}
