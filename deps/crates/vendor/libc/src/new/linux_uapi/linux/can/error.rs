//! Header: `linux/can/error.h`

pub use crate::linux::can::*;

pub const CAN_ERR_DLC: c_int = 8;

pub const CAN_ERR_TX_TIMEOUT: c_uint = 0x00000001;
pub const CAN_ERR_LOSTARB: c_uint = 0x00000002;
pub const CAN_ERR_CRTL: c_uint = 0x00000004;
pub const CAN_ERR_PROT: c_uint = 0x00000008;
pub const CAN_ERR_TRX: c_uint = 0x00000010;
pub const CAN_ERR_ACK: c_uint = 0x00000020;
pub const CAN_ERR_BUSOFF: c_uint = 0x00000040;
pub const CAN_ERR_BUSERROR: c_uint = 0x00000080;
pub const CAN_ERR_RESTARTED: c_uint = 0x00000100;
pub const CAN_ERR_CNT: c_uint = 0x00000200;

pub const CAN_ERR_LOSTARB_UNSPEC: c_int = 0x00;

pub const CAN_ERR_CRTL_UNSPEC: c_int = 0x00;
pub const CAN_ERR_CRTL_RX_OVERFLOW: c_int = 0x01;
pub const CAN_ERR_CRTL_TX_OVERFLOW: c_int = 0x02;
pub const CAN_ERR_CRTL_RX_WARNING: c_int = 0x04;
pub const CAN_ERR_CRTL_TX_WARNING: c_int = 0x08;
pub const CAN_ERR_CRTL_RX_PASSIVE: c_int = 0x10;
pub const CAN_ERR_CRTL_TX_PASSIVE: c_int = 0x20;
pub const CAN_ERR_CRTL_ACTIVE: c_int = 0x40;

pub const CAN_ERR_PROT_UNSPEC: c_int = 0x00;
pub const CAN_ERR_PROT_BIT: c_int = 0x01;
pub const CAN_ERR_PROT_FORM: c_int = 0x02;
pub const CAN_ERR_PROT_STUFF: c_int = 0x04;
pub const CAN_ERR_PROT_BIT0: c_int = 0x08;
pub const CAN_ERR_PROT_BIT1: c_int = 0x10;
pub const CAN_ERR_PROT_OVERLOAD: c_int = 0x20;
pub const CAN_ERR_PROT_ACTIVE: c_int = 0x40;
pub const CAN_ERR_PROT_TX: c_int = 0x80;

pub const CAN_ERR_PROT_LOC_UNSPEC: c_int = 0x00;
pub const CAN_ERR_PROT_LOC_SOF: c_int = 0x03;
pub const CAN_ERR_PROT_LOC_ID28_21: c_int = 0x02;
pub const CAN_ERR_PROT_LOC_ID20_18: c_int = 0x06;
pub const CAN_ERR_PROT_LOC_SRTR: c_int = 0x04;
pub const CAN_ERR_PROT_LOC_IDE: c_int = 0x05;
pub const CAN_ERR_PROT_LOC_ID17_13: c_int = 0x07;
pub const CAN_ERR_PROT_LOC_ID12_05: c_int = 0x0F;
pub const CAN_ERR_PROT_LOC_ID04_00: c_int = 0x0E;
pub const CAN_ERR_PROT_LOC_RTR: c_int = 0x0C;
pub const CAN_ERR_PROT_LOC_RES1: c_int = 0x0D;
pub const CAN_ERR_PROT_LOC_RES0: c_int = 0x09;
pub const CAN_ERR_PROT_LOC_DLC: c_int = 0x0B;
pub const CAN_ERR_PROT_LOC_DATA: c_int = 0x0A;
pub const CAN_ERR_PROT_LOC_CRC_SEQ: c_int = 0x08;
pub const CAN_ERR_PROT_LOC_CRC_DEL: c_int = 0x18;
pub const CAN_ERR_PROT_LOC_ACK: c_int = 0x19;
pub const CAN_ERR_PROT_LOC_ACK_DEL: c_int = 0x1B;
pub const CAN_ERR_PROT_LOC_EOF: c_int = 0x1A;
pub const CAN_ERR_PROT_LOC_INTERM: c_int = 0x12;

pub const CAN_ERR_TRX_UNSPEC: c_int = 0x00;
pub const CAN_ERR_TRX_CANH_NO_WIRE: c_int = 0x04;
pub const CAN_ERR_TRX_CANH_SHORT_TO_BAT: c_int = 0x05;
pub const CAN_ERR_TRX_CANH_SHORT_TO_VCC: c_int = 0x06;
pub const CAN_ERR_TRX_CANH_SHORT_TO_GND: c_int = 0x07;
pub const CAN_ERR_TRX_CANL_NO_WIRE: c_int = 0x40;
pub const CAN_ERR_TRX_CANL_SHORT_TO_BAT: c_int = 0x50;
pub const CAN_ERR_TRX_CANL_SHORT_TO_VCC: c_int = 0x60;
pub const CAN_ERR_TRX_CANL_SHORT_TO_GND: c_int = 0x70;
pub const CAN_ERR_TRX_CANL_SHORT_TO_CANH: c_int = 0x80;

pub const CAN_ERROR_WARNING_THRESHOLD: c_int = 96;
pub const CAN_ERROR_PASSIVE_THRESHOLD: c_int = 128;
pub const CAN_BUS_OFF_THRESHOLD: c_int = 256;
