use crate::{Config, Kind};
use crate::{STANDARD_CHARSET, URL_SAFE_CHARSET};

use vsimd::isa::{NEON, SSE2, WASM128};
use vsimd::tools::{read, write};
use vsimd::vector::{V128, V256};
use vsimd::{matches_isa, POD};
use vsimd::{Scalable, SIMD128, SIMD256};

#[inline(always)]
pub(crate) const fn encoded_length_unchecked(len: usize, config: Config) -> usize {
    let extra = len % 3;
    if extra == 0 {
        len / 3 * 4
    } else if config.extra.padding() {
        len / 3 * 4 + 4
    } else {
        len / 3 * 4 + extra + 1
    }
}

#[inline(always)]
unsafe fn encode_bits24(src: *const u8, dst: *mut u8, charset: *const u8) {
    let x = u32::from_be_bytes([0, read(src, 0), read(src, 1), read(src, 2)]);
    let mut i = 0;
    while i < 4 {
        let bits = (x >> (18 - i * 6)) & 0x3f;
        let y = read(charset, bits as usize);
        write(dst, i, y);
        i += 1;
    }
}

#[inline(always)]
unsafe fn encode_bits48(src: *const u8, dst: *mut u8, charset: *const u8) {
    let x = u64::from_be_bytes(src.cast::<[u8; 8]>().read());
    let mut i = 0;
    while i < 8 {
        let bits = (x >> (58 - i * 6)) & 0x3f;
        let y = read(charset, bits as usize);
        write(dst, i, y);
        i += 1;
    }
}

#[inline(always)]
unsafe fn encode_extra(extra: usize, src: *const u8, dst: *mut u8, charset: *const u8, padding: bool) {
    match extra {
        0 => {}
        1 => {
            let x = read(src, 0);
            let y1 = read(charset, (x >> 2) as usize);
            let y2 = read(charset, ((x << 6) >> 2) as usize);
            write(dst, 0, y1);
            write(dst, 1, y2);
            if padding {
                write(dst, 2, b'=');
                write(dst, 3, b'=');
            }
        }
        2 => {
            let x1 = read(src, 0);
            let x2 = read(src, 1);
            let y1 = read(charset, (x1 >> 2) as usize);
            let y2 = read(charset, (((x1 << 6) >> 2) | (x2 >> 4)) as usize);
            let y3 = read(charset, ((x2 << 4) >> 2) as usize);
            write(dst, 0, y1);
            write(dst, 1, y2);
            write(dst, 2, y3);
            if padding {
                write(dst, 3, b'=');
            }
        }
        _ => core::hint::unreachable_unchecked(),
    }
}

#[inline]
pub(crate) unsafe fn encode_fallback(mut src: *const u8, mut len: usize, mut dst: *mut u8, config: Config) {
    let kind = config.kind;
    let padding = config.extra.padding();

    let charset = match kind {
        Kind::Standard => STANDARD_CHARSET.as_ptr(),
        Kind::UrlSafe => URL_SAFE_CHARSET.as_ptr(),
    };

    const L: usize = 4;
    while len >= L * 6 + 2 {
        let mut i = 0;
        while i < L {
            encode_bits48(src, dst, charset);
            src = src.add(6);
            dst = dst.add(8);
            i += 1;
        }
        len -= L * 6;
    }

    while len >= 6 + 2 {
        encode_bits48(src, dst, charset);
        src = src.add(6);
        dst = dst.add(8);
        len -= 6;
    }

    let end = src.add(len / 3 * 3);
    while src < end {
        encode_bits24(src, dst, charset);
        src = src.add(3);
        dst = dst.add(4);
    }
    len %= 3;

    encode_extra(len, src, dst, charset, padding);
}

#[inline(always)]
pub(crate) unsafe fn encode_simd<S: SIMD256>(
    s: S,
    mut src: *const u8,
    mut len: usize,
    mut dst: *mut u8,
    config: Config,
) {
    let kind = config.kind;

    if len >= (6 + 24 + 4) {
        let (charset, shift_lut) = match kind {
            Kind::Standard => (STANDARD_CHARSET.as_ptr(), STANDARD_ENCODING_SHIFT_X2),
            Kind::UrlSafe => (URL_SAFE_CHARSET.as_ptr(), URL_SAFE_ENCODING_SHIFT_X2),
        };

        for _ in 0..2 {
            encode_bits24(src, dst, charset);
            src = src.add(3);
            dst = dst.add(4);
            len -= 3;
        }

        while len >= (24 + 4) {
            let x = s.v256_load_unaligned(src.sub(4));
            let y = encode_bytes24(s, x, shift_lut);
            s.v256_store_unaligned(dst, y);
            src = src.add(24);
            dst = dst.add(32);
            len -= 24;
        }
    }

    if len >= 12 + 4 {
        let shift_lut = match kind {
            Kind::Standard => STANDARD_ENCODING_SHIFT,
            Kind::UrlSafe => URL_SAFE_ENCODING_SHIFT,
        };

        let x = s.v128_load_unaligned(src);
        let y = encode_bytes12(s, x, shift_lut);
        s.v128_store_unaligned(dst, y);
        src = src.add(12);
        dst = dst.add(16);
        len -= 12;
    }

    encode_fallback(src, len, dst, config);
}

const SPLIT_SHUFFLE: V256 = V256::from_bytes([
    0x05, 0x04, 0x06, 0x05, 0x08, 0x07, 0x09, 0x08, //
    0x0b, 0x0a, 0x0c, 0x0b, 0x0e, 0x0d, 0x0f, 0x0e, //
    0x01, 0x00, 0x02, 0x01, 0x04, 0x03, 0x05, 0x04, //
    0x07, 0x06, 0x08, 0x07, 0x0a, 0x09, 0x0b, 0x0a, //
]);

#[inline(always)]
fn split_bits_x2<S: SIMD256>(s: S, x: V256) -> V256 {
    // x: {????|AAAB|BBCC|CDDD|EEEF|FFGG|GHHH|????}

    let x0 = s.u8x16x2_swizzle(x, SPLIT_SHUFFLE);
    // x0: {bbbbcccc|aaaaaabb|ccdddddd|bbbbcccc} x8 (1021)

    if matches_isa!(S, SSE2) {
        let m1 = s.u32x8_splat(u32::from_le_bytes([0x00, 0xfc, 0xc0, 0x0f]));
        let x1 = s.v256_and(x0, m1);
        // x1: {00000000|aaaaaa00|cc000000|0000cccc} x8

        let m2 = s.u32x8_splat(u32::from_le_bytes([0xf0, 0x03, 0x3f, 0x00]));
        let x2 = s.v256_and(x0, m2);
        // x2: {bbbb0000|000000bb|00dddddd|00000000} x8

        let m3 = s.u32x8_splat(u32::from_le_bytes([0x40, 0x00, 0x00, 0x04]));
        let x3 = s.u16x16_mul_hi(x1, m3);
        // x3: {00aaaaaa|00000000|00cccccc|00000000} x8

        let m4 = s.u32x8_splat(u32::from_le_bytes([0x10, 0x00, 0x00, 0x01]));
        let x4 = s.i16x16_mul_lo(x2, m4);
        // x4: {00000000|00bbbbbb|00000000|00dddddd} x8

        return s.v256_or(x3, x4);
        // {00aaaaaa|00bbbbbb|00cccccc|00dddddd} x8
    }

    if matches_isa!(S, NEON | WASM128) {
        let m1 = s.u32x8_splat(u32::from_le_bytes([0x00, 0xfc, 0x00, 0x00]));
        let x1 = s.u16x16_shr::<10>(s.v256_and(x0, m1));
        // x1: {00aaaaaa|000000000|00000000|00000000} x8

        let m2 = s.u32x8_splat(u32::from_le_bytes([0xf0, 0x03, 0x00, 0x00]));
        let x2 = s.u16x16_shl::<4>(s.v256_and(x0, m2));
        // x2: {00000000|00bbbbbb|00000000|00000000} x8

        let m3 = s.u32x8_splat(u32::from_le_bytes([0x00, 0x00, 0xc0, 0x0f]));
        let x3 = s.u16x16_shr::<6>(s.v256_and(x0, m3));
        // x3: {00000000|00000000|00cccccc|00000000} x8

        let m4 = s.u32x8_splat(u32::from_le_bytes([0x00, 0x00, 0x3f, 0x00]));
        let x4 = s.u16x16_shl::<8>(s.v256_and(x0, m4));
        // x4: {00000000|00000000|00000000|00dddddd} x8

        return s.v256_or(s.v256_or(x1, x2), s.v256_or(x3, x4));
        // {00aaaaaa|00bbbbbb|00cccccc|00dddddd} x8
    }

    unreachable!()
}

#[inline(always)]
fn split_bits_x1<S: SIMD128>(s: S, x: V128) -> V128 {
    // x: {AAAB|BBCC|CDDD|????}

    const SHUFFLE: V128 = SPLIT_SHUFFLE.to_v128x2().1;
    let x0 = s.u8x16_swizzle(x, SHUFFLE);
    // x0: {bbbbcccc|aaaaaabb|ccdddddd|bbbbcccc} x8 (1021)

    if matches_isa!(S, SSE2) {
        let m1 = s.u32x4_splat(u32::from_le_bytes([0x00, 0xfc, 0xc0, 0x0f]));
        let x1 = s.v128_and(x0, m1);

        let m2 = s.u32x4_splat(u32::from_le_bytes([0xf0, 0x03, 0x3f, 0x00]));
        let x2 = s.v128_and(x0, m2);

        let m3 = s.u32x4_splat(u32::from_le_bytes([0x40, 0x00, 0x00, 0x04]));
        let x3 = s.u16x8_mul_hi(x1, m3);

        let m4 = s.u32x4_splat(u32::from_le_bytes([0x10, 0x00, 0x00, 0x01]));
        let x4 = s.i16x8_mul_lo(x2, m4);

        return s.v128_or(x3, x4);
    }

    if matches_isa!(S, NEON | WASM128) {
        let m1 = s.u32x4_splat(u32::from_le_bytes([0x00, 0xfc, 0x00, 0x00]));
        let x1 = s.u16x8_shr::<10>(s.v128_and(x0, m1));

        let m2 = s.u32x4_splat(u32::from_le_bytes([0xf0, 0x03, 0x00, 0x00]));
        let x2 = s.u16x8_shl::<4>(s.v128_and(x0, m2));

        let m3 = s.u32x4_splat(u32::from_le_bytes([0x00, 0x00, 0xc0, 0x0f]));
        let x3 = s.u16x8_shr::<6>(s.v128_and(x0, m3));

        let m4 = s.u32x4_splat(u32::from_le_bytes([0x00, 0x00, 0x3f, 0x00]));
        let x4 = s.u16x8_shl::<8>(s.v128_and(x0, m4));

        return s.v128_or(s.v128_or(x1, x2), s.v128_or(x3, x4));
    }

    unreachable!()
}

#[inline]
const fn encoding_shift(charset: &'static [u8; 64]) -> V128 {
    // 0~25     'A'   [13]
    // 26~51    'a'   [0]
    // 52~61    '0'   [1~10]
    // 62       c62   [11]
    // 63       c63   [12]

    let mut lut = [0x80; 16];
    lut[13] = b'A';
    lut[0] = b'a' - 26;
    let mut i = 1;
    while i <= 10 {
        lut[i] = b'0'.wrapping_sub(52);
        i += 1;
    }
    lut[11] = charset[62].wrapping_sub(62);
    lut[12] = charset[63].wrapping_sub(63);
    V128::from_bytes(lut)
}

const STANDARD_ENCODING_SHIFT: V128 = encoding_shift(STANDARD_CHARSET);
const URL_SAFE_ENCODING_SHIFT: V128 = encoding_shift(URL_SAFE_CHARSET);

const STANDARD_ENCODING_SHIFT_X2: V256 = STANDARD_ENCODING_SHIFT.x2();
const URL_SAFE_ENCODING_SHIFT_X2: V256 = URL_SAFE_ENCODING_SHIFT.x2();

#[inline(always)]
fn encode_values<S: Scalable<V>, V: POD>(s: S, x: V, shift_lut: V) -> V {
    // x: {00aaaaaa|00bbbbbb|00cccccc|00dddddd} xn

    let x1 = s.u8xn_sub_sat(x, s.u8xn_splat(51));
    // 0~25    => 0
    // 26~51   => 0
    // 52~61   => 1~10
    // 62      => 11
    // 63      => 12

    let x2 = s.i8xn_lt(x, s.u8xn_splat(26));
    let x3 = s.and(x2, s.u8xn_splat(13));
    let x4 = s.or(x1, x3);
    // 0~25    => 0xff  => 13
    // 26~51   => 0     => 0
    // 52~61   => 0     => 1~10
    // 62      => 0     => 11
    // 63      => 0     => 12

    let shift = s.u8x16xn_swizzle(shift_lut, x4);
    s.u8xn_add(x, shift)
    // {{ascii}} xn
}

#[inline(always)]
fn encode_bytes24<S: SIMD256>(s: S, x: V256, shift_lut: V256) -> V256 {
    // x: {????|AAAB|BBCC|CDDD|EEEF|FFGG|GHHH|????}

    let values = split_bits_x2(s, x);
    // values: {00aaaaaa|00bbbbbb|00cccccc|00dddddd} x8

    encode_values(s, values, shift_lut)
    // {{ascii}} x32
}

#[inline(always)]
fn encode_bytes12<S: SIMD256>(s: S, x: V128, shift_lut: V128) -> V128 {
    // x: {AAAB|BBCC|CDDD|????}

    let values = split_bits_x1(s, x);
    // values: {00aaaaaa|00bbbbbb|00cccccc|00dddddd} x4

    encode_values(s, values, shift_lut)
    // {{ascii}} x16
}
