use crate::bpf_insn;

pub const BPF_LD: u16 = 0x00;
pub const BPF_LDX: u16 = 0x01;
pub const BPF_ST: u16 = 0x02;
pub const BPF_STX: u16 = 0x03;
pub const BPF_ALU: u16 = 0x04;
pub const BPF_JMP: u16 = 0x05;
pub const BPF_RET: u16 = 0x06;
pub const BPF_MISC: u16 = 0x07;
pub const BPF_W: u16 = 0x00;
pub const BPF_H: u16 = 0x08;
pub const BPF_B: u16 = 0x10;
pub const BPF_IMM: u16 = 0x00;
pub const BPF_ABS: u16 = 0x20;
pub const BPF_IND: u16 = 0x40;
pub const BPF_MEM: u16 = 0x60;
pub const BPF_LEN: u16 = 0x80;
pub const BPF_MSH: u16 = 0xa0;
pub const BPF_ADD: u16 = 0x00;
pub const BPF_SUB: u16 = 0x10;
pub const BPF_MUL: u16 = 0x20;
pub const BPF_DIV: u16 = 0x30;
pub const BPF_OR: u16 = 0x40;
pub const BPF_AND: u16 = 0x50;
pub const BPF_LSH: u16 = 0x60;
pub const BPF_RSH: u16 = 0x70;
pub const BPF_NEG: u16 = 0x80;
pub const BPF_MOD: u16 = 0x90;
pub const BPF_XOR: u16 = 0xa0;
pub const BPF_JA: u16 = 0x00;
pub const BPF_JEQ: u16 = 0x10;
pub const BPF_JGT: u16 = 0x20;
pub const BPF_JGE: u16 = 0x30;
pub const BPF_JSET: u16 = 0x40;
pub const BPF_K: u16 = 0x00;
pub const BPF_X: u16 = 0x08;
pub const BPF_A: u16 = 0x10;
pub const BPF_TAX: u16 = 0x00;
pub const BPF_TXA: u16 = 0x80;

f! {
    pub fn BPF_CLASS(code: u32) -> u32 {
        code & 0x07
    }

    pub fn BPF_SIZE(code: u32) -> u32 {
        code & 0x18
    }

    pub fn BPF_MODE(code: u32) -> u32 {
        code & 0xe0
    }

    pub fn BPF_OP(code: u32) -> u32 {
        code & 0xf0
    }

    pub fn BPF_SRC(code: u32) -> u32 {
        code & 0x08
    }

    pub fn BPF_RVAL(code: u32) -> u32 {
        code & 0x18
    }

    pub fn BPF_MISCOP(code: u32) -> u32 {
        code & 0xf8
    }

    pub fn BPF_STMT(code: u16, k: u32) -> bpf_insn {
        bpf_insn {
            code,
            jt: 0,
            jf: 0,
            k,
        }
    }

    pub fn BPF_JUMP(code: u16, k: u32, jt: u8, jf: u8) -> bpf_insn {
        bpf_insn { code, jt, jf, k }
    }
}
