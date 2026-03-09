use crate::alsw::{STANDARD_ALSW_CHECK_X2, URL_SAFE_ALSW_CHECK_X2};
use crate::decode::{decode_ascii4, decode_ascii8, decode_extra};
use crate::decode::{STANDARD_DECODE_TABLE, URL_SAFE_DECODE_TABLE};
use crate::{Config, Error, Kind};

use vsimd::alsw::AlswLut;
use vsimd::vector::V256;
use vsimd::SIMD256;

use core::ptr::null_mut;

#[inline]
pub(crate) unsafe fn check_fallback(mut src: *const u8, mut n: usize, config: Config) -> Result<(), Error> {
    let kind = config.kind;
    let forgiving = config.extra.forgiving();

    let table = match kind {
        Kind::Standard => STANDARD_DECODE_TABLE.as_ptr(),
        Kind::UrlSafe => URL_SAFE_DECODE_TABLE.as_ptr(),
    };

    unsafe {
        // n*3/4 >= 6+2
        while n >= 11 {
            decode_ascii8::<false>(src, null_mut(), table)?;
            src = src.add(8);
            n -= 8;
        }

        while n >= 4 {
            decode_ascii4::<false>(src, null_mut(), table)?;
            src = src.add(4);
            n -= 4;
        }

        decode_extra::<false>(n, src, null_mut(), table, forgiving)
    }
}

#[inline(always)]
pub(crate) unsafe fn check_simd<S: SIMD256>(
    s: S,
    mut src: *const u8,
    mut n: usize,
    config: Config,
) -> Result<(), Error> {
    let kind = config.kind;

    let check_lut = match kind {
        Kind::Standard => STANDARD_ALSW_CHECK_X2,
        Kind::UrlSafe => URL_SAFE_ALSW_CHECK_X2,
    };

    unsafe {
        // n*3/4 >= 24+4
        while n >= 38 {
            let x = s.v256_load_unaligned(src);
            let is_valid = check_ascii32(s, x, check_lut);
            ensure!(is_valid);
            src = src.add(32);
            n -= 32;
        }

        check_fallback(src, n, config)
    }
}

#[inline(always)]
fn check_ascii32<S: SIMD256>(s: S, x: V256, check: AlswLut<V256>) -> bool {
    vsimd::alsw::check_ascii_xn(s, x, check)
}
