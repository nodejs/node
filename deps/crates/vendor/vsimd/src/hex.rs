use crate::alsw::{self, AlswLut};
use crate::isa::{AVX2, NEON, SSSE3, WASM128};
use crate::mask::{u8x16_highbit_any, u8x32_highbit_any};
use crate::pod::POD;
use crate::vector::{V128, V256, V64};
use crate::{Scalable, SIMD128, SIMD256};

pub const UPPER_CHARSET: &[u8; 16] = b"0123456789ABCDEF";
pub const LOWER_CHARSET: &[u8; 16] = b"0123456789abcdef";

const fn parse_hex(x: u8) -> u8 {
    match x {
        b'0'..=b'9' => x - b'0',
        b'a'..=b'f' => x - b'a' + 10,
        b'A'..=b'F' => x - b'A' + 10,
        _ => 0xff,
    }
}

#[inline(always)]
#[must_use]
pub const fn unhex(x: u8) -> u8 {
    const UNHEX_TABLE: &[u8; 256] = &{
        let mut arr = [0; 256];
        let mut i = 0;
        while i < 256 {
            arr[i] = parse_hex(i as u8);
            i += 1;
        }
        arr
    };
    UNHEX_TABLE[x as usize]
}

#[inline(always)]
pub fn check_xn<S, V>(s: S, x: V) -> bool
where
    S: Scalable<V>,
    V: POD,
{
    let x1 = s.u8xn_sub(x, s.u8xn_splat(0x30 + 0x80));
    let x2 = s.u8xn_sub(s.and(x, s.u8xn_splat(0xdf)), s.u8xn_splat(0x41 + 0x80));
    let m1 = s.i8xn_lt(x1, s.i8xn_splat(-118));
    let m2 = s.i8xn_lt(x2, s.i8xn_splat(-122));
    s.mask8xn_all(s.or(m1, m2))
}

pub const ENCODE_UPPER_LUT: V256 = V256::double_bytes(*UPPER_CHARSET);
pub const ENCODE_LOWER_LUT: V256 = V256::double_bytes(*LOWER_CHARSET);

#[inline(always)]
pub fn encode_bytes16<S: SIMD256>(s: S, x: V128, lut: V256) -> V256 {
    let x = s.u16x16_from_u8x16(x);
    let hi = s.u16x16_shl::<8>(x);
    let lo = s.u16x16_shr::<4>(x);
    let values = s.v256_and(s.v256_or(hi, lo), s.u8x32_splat(0x0f));
    s.u8x16x2_swizzle(lut, values)
}

#[inline(always)]
pub fn encode_bytes32<S: SIMD256>(s: S, x: V256, lut: V256) -> (V256, V256) {
    let m = s.u8x32_splat(0x0f);
    let hi = s.v256_and(s.u16x16_shr::<4>(x), m);
    let lo = s.v256_and(x, m);

    let ac = s.u8x16x2_zip_lo(hi, lo);
    let bd = s.u8x16x2_zip_hi(hi, lo);

    let ab = s.v128x2_zip_lo(ac, bd);
    let cd = s.v128x2_zip_hi(ac, bd);

    let y1 = s.u8x16x2_swizzle(lut, ab);
    let y2 = s.u8x16x2_swizzle(lut, cd);

    (y1, y2)
}

struct HexAlsw;

impl HexAlsw {
    const fn decode(c: u8) -> u8 {
        parse_hex(c)
    }

    const fn check_hash(i: u8) -> u8 {
        match i {
            0 => 1,
            1..=6 => 1,
            7..=9 => 6,
            0xA..=0xF => 8,
            _ => unreachable!(),
        }
    }

    const fn decode_hash(i: u8) -> u8 {
        Self::check_hash(i)
    }
}

impl_alsw!(HexAlsw);

const HEX_ALSW_CHECK: AlswLut<V128> = HexAlsw::check_lut();
const HEX_ALSW_DECODE: AlswLut<V128> = HexAlsw::decode_lut();

const HEX_ALSW_CHECK_X2: AlswLut<V256> = HexAlsw::check_lut().x2();
const HEX_ALSW_DECODE_X2: AlswLut<V256> = HexAlsw::decode_lut().x2();

const DECODE_UZP1: V256 = V256::double_bytes([
    0x00, 0x02, 0x04, 0x06, 0x08, 0x0a, 0x0c, 0x0e, //
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, //
]);

const DECODE_UZP2: V256 = V256::double_bytes([
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, //
    0x00, 0x02, 0x04, 0x06, 0x08, 0x0a, 0x0c, 0x0e, //
]);

#[inline(always)]
fn merge_bits<S: Scalable<V>, V: POD>(s: S, x: V) -> V {
    // x:  {0000hhhh|0000llll} xn

    let x1 = s.u16xn_shl::<4>(x);
    // x1: {hhhh0000|llll0000} xn

    let x2 = s.u16xn_shr::<12>(x1);
    // x2: {0000llll|00000000} xn

    s.or(x1, x2)
    //     {hhhhllll|????????} xn
}

#[inline(always)]
fn decode16<S: SIMD128>(s: S, x: V128) -> (V128, V128) {
    let (c1, c2) = alsw::decode_ascii_xn(s, x, HEX_ALSW_CHECK, HEX_ALSW_DECODE);
    (merge_bits(s, c2), c1)
}

#[inline(always)]
fn decode32<S: SIMD256>(s: S, x: V256) -> (V256, V256) {
    let (c1, c2) = alsw::decode_ascii_xn(s, x, HEX_ALSW_CHECK_X2, HEX_ALSW_DECODE_X2);
    (merge_bits(s, c2), c1)
}

#[allow(clippy::result_unit_err)]
#[inline(always)]
pub fn decode_ascii16<S: SIMD128>(s: S, x: V128) -> Result<V64, ()> {
    let (y, is_invalid) = decode16(s, x);

    let ans = if matches_isa!(S, SSSE3 | WASM128) {
        const UZP1: V128 = DECODE_UZP1.to_v128x2().0;
        s.u8x16_swizzle(y, UZP1).to_v64x2().0
    } else if matches_isa!(S, NEON) {
        let (a, b) = y.to_v64x2();
        s.u8x8_unzip_even(a, b)
    } else {
        unreachable!()
    };

    if u8x16_highbit_any(s, is_invalid) {
        Err(())
    } else {
        Ok(ans)
    }
}

#[allow(clippy::result_unit_err)]
#[inline(always)]
pub fn decode_ascii32<S: SIMD256>(s: S, x: V256) -> Result<V128, ()> {
    let (y, is_invalid) = decode32(s, x);

    let ans = if matches_isa!(S, SSSE3 | WASM128) {
        let (a, b) = s.u8x16x2_swizzle(y, DECODE_UZP1).to_v128x2();
        s.u64x2_zip_lo(a, b)
    } else if matches_isa!(S, NEON) {
        let (a, b) = y.to_v128x2();
        s.u8x16_unzip_even(a, b)
    } else {
        unreachable!()
    };

    if u8x32_highbit_any(s, is_invalid) {
        Err(())
    } else {
        Ok(ans)
    }
}

#[allow(clippy::result_unit_err)]
#[inline(always)]
pub fn decode_ascii32x2<S: SIMD256>(s: S, x: (V256, V256)) -> Result<V256, ()> {
    let (y1, is_invalid1) = decode32(s, x.0);
    let (y2, is_invalid2) = decode32(s, x.1);
    let is_invalid = s.v256_or(is_invalid1, is_invalid2);

    let ans = if matches_isa!(S, AVX2) {
        let ab = s.u8x16x2_swizzle(y1, DECODE_UZP1);
        let cd = s.u8x16x2_swizzle(y2, DECODE_UZP2);
        let acbd = s.v256_or(ab, cd);
        s.u64x4_permute::<0b_1101_1000>(acbd) // 0213
    } else if matches_isa!(S, SSSE3 | WASM128) {
        let ab = s.u8x16x2_swizzle(y1, DECODE_UZP1);
        let cd = s.u8x16x2_swizzle(y2, DECODE_UZP1);
        s.u64x4_unzip_even(ab, cd)
    } else if matches_isa!(S, NEON) {
        s.u8x32_unzip_even(y1, y2)
    } else {
        unreachable!()
    };

    if u8x32_highbit_any(s, is_invalid) {
        Err(())
    } else {
        Ok(ans)
    }
}

pub mod sse2 {
    use crate::isa::SSE2;
    use crate::vector::{V128, V64};
    use crate::SIMD128;

    #[inline(always)]
    #[must_use]
    pub fn decode_nibbles(s: SSE2, x: V128) -> (V128, V128) {
        // http://0x80.pl/notesen/2022-01-17-validating-hex-parse.html
        // Algorithm 3

        let t1 = s.u8x16_add(x, s.u8x16_splat(0xff - b'9'));
        let t2 = s.u8x16_sub_sat(t1, s.u8x16_splat(6));
        let t3 = s.u8x16_sub(t2, s.u8x16_splat(0xf0));
        let t4 = s.v128_and(x, s.u8x16_splat(0xdf));
        let t5 = s.u8x16_sub(t4, s.u8x16_splat(0x41));
        let t6 = s.u8x16_add_sat(t5, s.u8x16_splat(10));
        let t7 = s.u8x16_min(t3, t6);
        let t8 = s.u8x16_add_sat(t7, s.u8x16_splat(127 - 15));
        (t7, t8)
    }

    #[inline(always)]
    #[must_use]
    pub fn merge_bits(s: SSE2, x: V128) -> V64 {
        let lo = s.u16x8_shr::<8>(x);
        let hi = s.u16x8_shl::<4>(x);
        let t1 = s.v128_or(lo, hi);
        let t2 = s.v128_and(t1, s.u16x8_splat(0x00ff));
        let t3 = s.i16x8_packus(t2, s.v128_create_zero());
        t3.to_v64x2().0
    }

    pub const LOWER_OFFSET: V128 = V128::from_bytes([0x27; 16]);
    pub const UPPER_OFFSET: V128 = V128::from_bytes([0x07; 16]);

    #[inline(always)]
    #[must_use]
    pub fn encode16(s: SSE2, x: V128, offset: V128) -> (V128, V128) {
        let m = s.u8x16_splat(0x0f);
        let hi = s.v128_and(s.u16x8_shr::<4>(x), m);
        let lo = s.v128_and(x, m);

        let c1 = s.u8x16_splat(0x30);
        let h1 = s.u8x16_add(hi, c1);
        let l1 = s.u8x16_add(lo, c1);

        let c2 = s.u8x16_splat(0x39);
        let h2 = s.v128_and(s.i8x16_lt(c2, h1), offset);
        let l2 = s.v128_and(s.i8x16_lt(c2, l1), offset);

        let h3 = s.u8x16_add(h1, h2);
        let l3 = s.u8x16_add(l1, l2);

        let y1 = s.u8x16_zip_lo(h3, l3);
        let y2 = s.u8x16_zip_hi(h3, l3);

        (y1, y2)
    }
}

#[cfg(test)]
mod algorithm {
    use super::*;

    #[test]
    #[ignore]
    fn check() {
        fn is_hex_v1(c: u8) -> bool {
            matches!(c, b'0'..=b'9' | b'a'..=b'f' | b'A'..=b'F')
        }

        fn is_hex_v2(c: u8) -> bool {
            let x1 = c.wrapping_sub(0x30);
            let x2 = (c & 0xdf).wrapping_sub(0x41);
            x1 < 10 || x2 < 6
        }

        fn is_hex_v3(c: u8) -> bool {
            let x1 = c.wrapping_sub(0x30 + 0x80);
            let x2 = (c & 0xdf).wrapping_sub(0x41 + 0x80);
            ((x1 as i8) < -118) || ((x2 as i8) < -122)
        }

        for c in 0..=255_u8 {
            let (v1, v2, v3) = (is_hex_v1(c), is_hex_v2(c), is_hex_v3(c));
            assert_eq!(v1, v2);
            assert_eq!(v1, v3);
        }
    }

    #[test]
    #[ignore]
    fn hex_alsw() {
        HexAlsw::test_check();
        HexAlsw::test_decode();
    }
}
