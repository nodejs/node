use crate::prelude::*;

s_no_extra_traits! {
    pub union __c_anonymous_ifr_ifru {
        pub ifru_addr: crate::sockaddr,
        pub ifru_dstaddr: crate::sockaddr,
        pub ifru_broadaddr: crate::sockaddr,
        pub ifru_buffer: ifreq_buffer,
        pub ifru_flags: [c_short; 2],
        pub ifru_index: c_short,
        pub ifru_jid: c_int,
        pub ifru_metric: c_int,
        pub ifru_mtu: c_int,
        pub ifru_phys: c_int,
        pub ifru_media: c_int,
        pub ifru_data: *mut c_char,
        pub ifru_cap: [c_int; 2],
        pub ifru_fib: c_uint,
        pub ifru_vlan_pcp: c_uchar,
    }

    pub struct ifreq {
        /// if name, e.g. "en0"
        pub ifr_name: [c_char; crate::IFNAMSIZ],
        pub ifr_ifru: __c_anonymous_ifr_ifru,
    }

    pub struct ifreq_buffer {
        pub length: size_t,
        pub buffer: *mut c_void,
    }
}
