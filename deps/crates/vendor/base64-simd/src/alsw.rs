use vsimd::alsw::AlswLut;
use vsimd::vector::{V128, V256};

struct StandardAlsw;

impl StandardAlsw {
    #[inline]
    const fn decode(c: u8) -> u8 {
        match c {
            b'A'..=b'Z' => c - b'A',
            b'a'..=b'z' => c - b'a' + 26,
            b'0'..=b'9' => c - b'0' + 52,
            b'+' => 62,
            b'/' => 63,
            _ => 0xff,
        }
    }

    #[inline]
    const fn check_hash(i: u8) -> u8 {
        match i {
            0 => 5,
            1..=9 => 2,
            0xA => 4,
            0xB => 6,
            0xC..=0xE => 8,
            0xF => 6,
            _ => unreachable!(),
        }
    }

    #[inline]
    const fn decode_hash(i: u8) -> u8 {
        match i {
            0xB => 0x07,
            0xF => 0x08,
            _ => 0x01,
        }
    }
}

vsimd::impl_alsw!(StandardAlsw);

struct UrlSafeAlsw;

impl UrlSafeAlsw {
    #[inline]
    const fn decode(c: u8) -> u8 {
        match c {
            b'A'..=b'Z' => c - b'A',
            b'a'..=b'z' => c - b'a' + 26,
            b'0'..=b'9' => c - b'0' + 52,
            b'-' => 62,
            b'_' => 63,
            _ => 0xff,
        }
    }

    #[inline]
    const fn check_hash(i: u8) -> u8 {
        match i {
            0 => 7,
            1..=9 => 2,
            0xA => 4,
            0xB | 0xC => 6,
            0xD => 8,
            0xE => 6,
            0xF => 6,
            _ => unreachable!(),
        }
    }

    #[inline]
    const fn decode_hash(i: u8) -> u8 {
        match i {
            0xD => 0x01,
            0xF => 0x05,
            _ => 0x01,
        }
    }
}

vsimd::impl_alsw!(UrlSafeAlsw);

pub const STANDARD_ALSW_CHECK_X2: AlswLut<V256> = StandardAlsw::check_lut().x2();
pub const STANDARD_ALSW_DECODE_X2: AlswLut<V256> = StandardAlsw::decode_lut().x2();

pub const URL_SAFE_ALSW_CHECK_X2: AlswLut<V256> = UrlSafeAlsw::check_lut().x2();
pub const URL_SAFE_ALSW_DECODE_X2: AlswLut<V256> = UrlSafeAlsw::decode_lut().x2();

#[cfg(test)]
mod algorithm {
    use super::*;

    #[test]
    #[ignore]
    fn standard_alsw() {
        StandardAlsw::test_check();
        StandardAlsw::test_decode();
    }

    #[test]
    #[ignore]
    fn url_safe_alsw() {
        UrlSafeAlsw::test_check();
        UrlSafeAlsw::test_decode();
    }
}
