//! Header: `linux/can/bcm.h`

pub use crate::linux::can::*;

s! {
    pub struct bcm_timeval {
        pub tv_sec: c_long,
        pub tv_usec: c_long,
    }

    pub struct bcm_msg_head {
        pub opcode: u32,
        pub flags: u32,
        pub count: u32,
        pub ival1: bcm_timeval,
        pub ival2: bcm_timeval,
        pub can_id: canid_t,
        pub nframes: u32,
        pub frames: [can_frame; 0],
    }
}

c_enum! {
    #[repr(u32)]
    pub enum #anon {
        pub TX_SETUP = 1,
        pub TX_DELETE,
        pub TX_READ,
        pub TX_SEND,
        pub RX_SETUP,
        pub RX_DELETE,
        pub RX_READ,
        pub TX_STATUS,
        pub TX_EXPIRED,
        pub RX_STATUS,
        pub RX_TIMEOUT,
        pub RX_CHANGED,
    }
}

pub const SETTIMER: u32 = 0x0001;
pub const STARTTIMER: u32 = 0x0002;
pub const TX_COUNTEVT: u32 = 0x0004;
pub const TX_ANNOUNCE: u32 = 0x0008;
pub const TX_CP_CAN_ID: u32 = 0x0010;
pub const RX_FILTER_ID: u32 = 0x0020;
pub const RX_CHECK_DLC: u32 = 0x0040;
pub const RX_NO_AUTOTIMER: u32 = 0x0080;
pub const RX_ANNOUNCE_RESUME: u32 = 0x0100;
pub const TX_RESET_MULTI_IDX: u32 = 0x0200;
pub const RX_RTR_FRAME: u32 = 0x0400;
pub const CAN_FD_FRAME: u32 = 0x0800;
