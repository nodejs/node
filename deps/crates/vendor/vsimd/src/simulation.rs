use crate::vector::V128;

use core::mem::transmute as t;

#[cfg(miri)]
use core::cmp::{max, min};

// TODO: waiting for MIRI's support

#[cfg(miri)]
#[inline(always)]
pub fn u8x16_max(a: V128, b: V128) -> V128 {
    let (a, b) = (a.as_bytes(), b.as_bytes());
    let mut c = [0; 16];
    for i in 0..16 {
        c[i] = max(a[i], b[i]);
    }
    V128::from_bytes(c)
}

#[cfg(miri)]
#[inline(always)]
pub fn u8x16_min(a: V128, b: V128) -> V128 {
    let (a, b) = (a.as_bytes(), b.as_bytes());
    let mut c = [0; 16];
    for i in 0..16 {
        c[i] = min(a[i], b[i]);
    }
    V128::from_bytes(c)
}

#[allow(clippy::needless_range_loop)]
#[inline(always)]
pub fn u8x16_bitmask(a: V128) -> u16 {
    // FIXME: is it defined behavior?
    // https://github.com/rust-lang/miri/issues/2617
    // https://github.com/rust-lang/stdarch/issues/1347

    let a = a.as_bytes();
    let mut m: u16 = 0;
    for i in 0..16 {
        m |= ((a[i] >> 7) as u16) << i;
    }
    m
}

#[allow(clippy::needless_range_loop)]
#[inline(always)]
pub fn u16x8_shr(a: V128, imm8: u8) -> V128 {
    let mut a: [u16; 8] = unsafe { t(a) };
    for i in 0..8 {
        a[i] >>= imm8;
    }
    unsafe { t(a) }
}

#[allow(clippy::needless_range_loop)]
#[inline(always)]
pub fn u16x8_shl(a: V128, imm8: u8) -> V128 {
    let mut a: [u16; 8] = unsafe { t(a) };
    for i in 0..8 {
        a[i] <<= imm8;
    }
    unsafe { t(a) }
}

#[inline(always)]
pub fn i16x8_packus(a: V128, b: V128) -> V128 {
    let a: [i16; 8] = unsafe { t(a) };
    let b: [i16; 8] = unsafe { t(b) };
    let sat_u8 = |x: i16| {
        if x < 0 {
            0
        } else if x > 255 {
            255
        } else {
            x as u8
        }
    };
    let mut c: [u8; 16] = [0; 16];
    for i in 0..8 {
        c[i] = sat_u8(a[i]);
        c[i + 8] = sat_u8(b[i]);
    }
    V128::from_bytes(c)
}
