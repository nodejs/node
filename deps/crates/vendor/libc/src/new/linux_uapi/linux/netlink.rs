//! Header: `uapi/linux/netlink.h`

use crate::prelude::*;

pub const NETLINK_ROUTE: c_int = 0;
pub const NETLINK_UNUSED: c_int = 1;
pub const NETLINK_USERSOCK: c_int = 2;
pub const NETLINK_FIREWALL: c_int = 3;
pub const NETLINK_SOCK_DIAG: c_int = 4;
pub const NETLINK_NFLOG: c_int = 5;
pub const NETLINK_XFRM: c_int = 6;
pub const NETLINK_SELINUX: c_int = 7;
pub const NETLINK_ISCSI: c_int = 8;
pub const NETLINK_AUDIT: c_int = 9;
pub const NETLINK_FIB_LOOKUP: c_int = 10;
pub const NETLINK_CONNECTOR: c_int = 11;
pub const NETLINK_NETFILTER: c_int = 12;
pub const NETLINK_IP6_FW: c_int = 13;
pub const NETLINK_DNRTMSG: c_int = 14;
pub const NETLINK_KOBJECT_UEVENT: c_int = 15;
pub const NETLINK_GENERIC: c_int = 16;
pub const NETLINK_SCSITRANSPORT: c_int = 18;
pub const NETLINK_ECRYPTFS: c_int = 19;
pub const NETLINK_RDMA: c_int = 20;
pub const NETLINK_CRYPTO: c_int = 21;

pub const NETLINK_INET_DIAG: c_int = NETLINK_SOCK_DIAG;

pub const MAX_LINKS: c_int = 32;

s! {
    pub struct sockaddr_nl {
        pub nl_family: crate::sa_family_t,
        nl_pad: Padding<c_ushort>,
        pub nl_pid: u32,
        pub nl_groups: u32,
    }

    pub struct nlmsghdr {
        pub nlmsg_len: u32,
        pub nlmsg_type: u16,
        pub nlmsg_flags: u16,
        pub nlmsg_seq: u32,
        pub nlmsg_pid: u32,
    }
}

pub const NLM_F_REQUEST: c_int = 1;
pub const NLM_F_MULTI: c_int = 2;
pub const NLM_F_ACK: c_int = 4;
pub const NLM_F_ECHO: c_int = 8;
pub const NLM_F_DUMP_INTR: c_int = 16;
pub const NLM_F_DUMP_FILTERED: c_int = 32;

pub const NLM_F_ROOT: c_int = 0x100;
pub const NLM_F_MATCH: c_int = 0x200;
pub const NLM_F_ATOMIC: c_int = 0x400;
pub const NLM_F_DUMP: c_int = NLM_F_ROOT | NLM_F_MATCH;

pub const NLM_F_REPLACE: c_int = 0x100;
pub const NLM_F_EXCL: c_int = 0x200;
pub const NLM_F_CREATE: c_int = 0x400;
pub const NLM_F_APPEND: c_int = 0x800;

pub const NLM_F_NONREC: c_int = 0x100;

pub const NLM_F_CAPPED: c_int = 0x100;
pub const NLM_F_ACK_TLVS: c_int = 0x200;

pub const NLMSG_NOOP: c_int = 0x1;
pub const NLMSG_ERROR: c_int = 0x2;
pub const NLMSG_DONE: c_int = 0x3;
pub const NLMSG_OVERRUN: c_int = 0x4;

pub const NLMSG_MIN_TYPE: c_int = 0x10;

s! {
    pub struct nlmsgerr {
        pub error: c_int,
        pub msg: nlmsghdr,
    }
}

pub const NETLINK_ADD_MEMBERSHIP: c_int = 1;
pub const NETLINK_DROP_MEMBERSHIP: c_int = 2;
pub const NETLINK_PKTINFO: c_int = 3;
pub const NETLINK_BROADCAST_ERROR: c_int = 4;
pub const NETLINK_NO_ENOBUFS: c_int = 5;
pub const NETLINK_RX_RING: c_int = 6;
pub const NETLINK_TX_RING: c_int = 7;
pub const NETLINK_LISTEN_ALL_NSID: c_int = 8;
pub const NETLINK_LIST_MEMBERSHIPS: c_int = 9;
pub const NETLINK_CAP_ACK: c_int = 10;
pub const NETLINK_EXT_ACK: c_int = 11;
pub const NETLINK_GET_STRICT_CHK: c_int = 12;

s! {
    pub struct nl_pktinfo {
        pub group: u32,
    }

    pub struct nl_mmap_req {
        pub nm_block_size: c_uint,
        pub nm_block_nr: c_uint,
        pub nm_frame_size: c_uint,
        pub nm_frame_nr: c_uint,
    }

    pub struct nl_mmap_hdr {
        pub nm_status: c_uint,
        pub nm_len: c_uint,
        pub nm_group: u32,
        pub nm_pid: u32,
        pub nm_uid: u32,
        pub nm_gid: u32,
    }
}

s! {
    pub struct nlattr {
        pub nla_len: u16,
        pub nla_type: u16,
    }
}

pub const NLA_F_NESTED: c_int = 1 << 15;
pub const NLA_F_NET_BYTEORDER: c_int = 1 << 14;
pub const NLA_TYPE_MASK: c_int = !(NLA_F_NESTED | NLA_F_NET_BYTEORDER);

pub const NLA_ALIGNTO: c_int = 4;

f! {
    pub fn NLA_ALIGN(len: c_int) -> c_int {
        return ((len) + NLA_ALIGNTO - 1) & !(NLA_ALIGNTO - 1);
    }
}
