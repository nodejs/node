use crate::alsw::{STANDARD_ALSW_CHECK_X2, URL_SAFE_ALSW_CHECK_X2};
use crate::alsw::{STANDARD_ALSW_DECODE_X2, URL_SAFE_ALSW_DECODE_X2};
use crate::{Config, Error, Extra, Kind};
use crate::{STANDARD_CHARSET, URL_SAFE_CHARSET};

use vsimd::alsw::AlswLut;
use vsimd::isa::{NEON, SSSE3, WASM128};
use vsimd::mask::u8x32_highbit_any;
use vsimd::matches_isa;
use vsimd::tools::{read, write};
use vsimd::vector::V256;
use vsimd::SIMD256;

use core::ops::Not;

const fn decode_table(charset: &'static [u8; 64]) -> [u8; 256] {
    let mut table = [0xff; 256];
    let mut i = 0;
    while i < charset.len() {
        table[charset[i] as usize] = i as u8;
        i += 1;
    }
    table
}

pub const STANDARD_DECODE_TABLE: &[u8; 256] = &decode_table(STANDARD_CHARSET);
pub const URL_SAFE_DECODE_TABLE: &[u8; 256] = &decode_table(URL_SAFE_CHARSET);

#[inline(always)]
pub(crate) fn decoded_length(src: &[u8], config: Config) -> Result<(usize, usize), Error> {
    if src.is_empty() {
        return Ok((0, 0));
    }

    let n = unsafe {
        let len = src.len();

        let count_pad = || {
            let last1 = *src.get_unchecked(len - 1);
            let last2 = *src.get_unchecked(len - 2);
            if last1 == b'=' {
                if last2 == b'=' {
                    2
                } else {
                    1
                }
            } else {
                0
            }
        };

        match config.extra {
            Extra::Pad => {
                ensure!(len % 4 == 0);
                len - count_pad()
            }
            Extra::NoPad => len,
            Extra::Forgiving => {
                if len % 4 == 0 {
                    len - count_pad()
                } else {
                    len
                }
            }
        }
    };

    let m = match n % 4 {
        0 => n / 4 * 3,
        1 => return Err(Error::new()),
        2 => n / 4 * 3 + 1,
        3 => n / 4 * 3 + 2,
        _ => unsafe { core::hint::unreachable_unchecked() },
    };

    Ok((n, m))
}

#[inline(always)]
pub unsafe fn decode_ascii8<const WRITE: bool>(src: *const u8, dst: *mut u8, table: *const u8) -> Result<(), Error> {
    let mut y: u64 = 0;
    let mut flag = 0;

    let mut i = 0;
    while i < 8 {
        let x = read(src, i);
        let bits = read(table, x as usize);
        flag |= bits;

        if WRITE {
            y |= (bits as u64) << (58 - i * 6);
        }

        i += 1;
    }

    if WRITE {
        dst.cast::<u64>().write_unaligned(y.to_be());
    }

    ensure!(flag != 0xff);
    Ok(())
}

#[inline(always)]
pub unsafe fn decode_ascii4<const WRITE: bool>(src: *const u8, dst: *mut u8, table: *const u8) -> Result<(), Error> {
    let mut y: u32 = 0;
    let mut flag = 0;

    let mut i = 0;
    while i < 4 {
        let x = read(src, i);
        let bits = read(table, x as usize);
        flag |= bits;

        if WRITE {
            y |= (bits as u32) << (18 - i * 6);
        }

        i += 1;
    }

    if WRITE {
        let y = y.to_be_bytes();
        write(dst, 0, y[1]);
        write(dst, 1, y[2]);
        write(dst, 2, y[3]);
    }

    ensure!(flag != 0xff);
    Ok(())
}

#[inline(always)]
pub unsafe fn decode_extra<const WRITE: bool>(
    extra: usize,
    src: *const u8,
    dst: *mut u8,
    table: *const u8,
    forgiving: bool,
) -> Result<(), Error> {
    match extra {
        0 => {}
        1 => core::hint::unreachable_unchecked(),
        2 => {
            let [x1, x2] = src.cast::<[u8; 2]>().read();

            let y1 = read(table, x1 as usize);
            let y2 = read(table, x2 as usize);
            ensure!((y1 | y2) != 0xff && (forgiving || (y2 & 0x0f) == 0));

            if WRITE {
                write(dst, 0, (y1 << 2) | (y2 >> 4));
            }
        }
        3 => {
            let [x1, x2, x3] = src.cast::<[u8; 3]>().read();

            let y1 = read(table, x1 as usize);
            let y2 = read(table, x2 as usize);
            let y3 = read(table, x3 as usize);
            ensure!((y1 | y2 | y3) != 0xff && (forgiving || (y3 & 0x03) == 0));

            if WRITE {
                write(dst, 0, (y1 << 2) | (y2 >> 4));
                write(dst, 1, (y2 << 4) | (y3 >> 2));
            }
        }
        _ => core::hint::unreachable_unchecked(),
    }
    Ok(())
}

#[inline]
pub(crate) unsafe fn decode_fallback(
    mut src: *const u8,
    mut dst: *mut u8,
    mut n: usize,
    config: Config,
) -> Result<(), Error> {
    let kind = config.kind;
    let forgiving = config.extra.forgiving();

    let table = match kind {
        Kind::Standard => STANDARD_DECODE_TABLE.as_ptr(),
        Kind::UrlSafe => URL_SAFE_DECODE_TABLE.as_ptr(),
    };

    // n*3/4 >= 6+2
    while n >= 11 {
        decode_ascii8::<true>(src, dst, table)?;
        src = src.add(8);
        dst = dst.add(6);
        n -= 8;
    }

    let end = src.add(n / 4 * 4);
    while src < end {
        decode_ascii4::<true>(src, dst, table)?;
        src = src.add(4);
        dst = dst.add(3);
    }
    n %= 4;

    decode_extra::<true>(n, src, dst, table, forgiving)
}

#[inline(always)]
pub(crate) unsafe fn decode_simd<S: SIMD256>(
    s: S,
    mut src: *const u8,
    mut dst: *mut u8,
    mut n: usize,
    config: Config,
) -> Result<(), Error> {
    let kind = config.kind;

    let (check_lut, decode_lut) = match kind {
        Kind::Standard => (STANDARD_ALSW_CHECK_X2, STANDARD_ALSW_DECODE_X2),
        Kind::UrlSafe => (URL_SAFE_ALSW_CHECK_X2, URL_SAFE_ALSW_DECODE_X2),
    };

    // n*3/4 >= 24+4
    while n >= 38 {
        let x = s.v256_load_unaligned(src);
        let y = try_!(decode_ascii32(s, x, check_lut, decode_lut));

        let (y1, y2) = y.to_v128x2();
        s.v128_store_unaligned(dst, y1);
        s.v128_store_unaligned(dst.add(12), y2);

        src = src.add(32);
        dst = dst.add(24);
        n -= 32;
    }

    decode_fallback(src, dst, n, config)
}

#[inline(always)]
fn merge_bits_x2<S: SIMD256>(s: S, x: V256) -> V256 {
    // x : {00aaaaaa|00bbbbbb|00cccccc|00dddddd} x8

    let y = if matches_isa!(S, SSSE3) {
        let m1 = s.u16x16_splat(u16::from_le_bytes([0x40, 0x01]));
        let x1 = s.i16x16_maddubs(x, m1);
        // x1: {aabbbbbb|0000aaaa|ccdddddd|0000cccc} x8

        let m2 = s.u32x8_splat(u32::from_le_bytes([0x00, 0x10, 0x01, 0x00]));
        s.i16x16_madd(x1, m2)
        // {ccdddddd|bbbbcccc|aaaaaabb|00000000} x8
    } else if matches_isa!(S, NEON | WASM128) {
        let m1 = s.u32x8_splat(u32::from_le_bytes([0x3f, 0x00, 0x3f, 0x00]));
        let x1 = s.v256_and(x, m1);
        // x1: {00aaaaaa|00000000|00cccccc|00000000} x8

        let m2 = s.u32x8_splat(u32::from_le_bytes([0x00, 0x3f, 0x00, 0x3f]));
        let x2 = s.v256_and(x, m2);
        // x2: {00000000|00bbbbbb|00000000|00dddddd} x8

        let x3 = s.v256_or(s.u32x8_shl::<18>(x1), s.u32x8_shr::<10>(x1));
        // x3: {cc000000|0000cccc|aaaaaa00|00000000} x8

        let x4 = s.v256_or(s.u32x8_shl::<4>(x2), s.u32x8_shr::<24>(x2));
        // x4: {00dddddd|bbbb0000|000000bb|dddd0000}

        let mask = s.u32x8_splat(u32::from_le_bytes([0xff, 0xff, 0xff, 0x00]));
        s.v256_and(s.v256_or(x3, x4), mask)
        // {ccdddddd|bbbbcccc|aaaaaabb|00000000} x8
    } else {
        unreachable!()
    };

    const SHUFFLE: V256 = V256::double_bytes([
        0x02, 0x01, 0x00, 0x06, 0x05, 0x04, 0x0a, 0x09, //
        0x08, 0x0e, 0x0d, 0x0c, 0x80, 0x80, 0x80, 0x80, //
    ]);
    s.u8x16x2_swizzle(y, SHUFFLE)
    // {AAAB|BBCC|CDDD|0000|EEEF|FFGG|GHHH|0000}
}

#[inline(always)]
fn decode_ascii32<S: SIMD256>(s: S, x: V256, check: AlswLut<V256>, decode: AlswLut<V256>) -> Result<V256, Error> {
    let (c1, c2) = vsimd::alsw::decode_ascii_xn(s, x, check, decode);
    let y = merge_bits_x2(s, c2);
    ensure!(u8x32_highbit_any(s, c1).not());
    Ok(y)
}
