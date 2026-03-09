use vsimd::isa::AVX2;
use vsimd::tools::slice_parts;
use vsimd::{matches_isa, Scalable, POD, SIMD256};

use core::ops::Not;

#[inline(always)]
#[must_use]
fn lookup_ascii_whitespace(c: u8) -> u8 {
    const TABLE: &[u8; 256] = &{
        let mut ans = [0; 256];
        let mut i: u8 = 0;
        loop {
            ans[i as usize] = if i.is_ascii_whitespace() { 0xff } else { 0 };
            if i == 255 {
                break;
            }
            i += 1;
        }
        ans
    };
    unsafe { *TABLE.get_unchecked(c as usize) }
}

#[inline(always)]
fn has_ascii_whitespace<S: Scalable<V>, V: POD>(s: S, x: V) -> bool {
    // ASCII whitespaces
    // TAB      0x09    00001001
    // LF       0x0a    00001010
    // FF       0x0c    00001100
    // CR       0x0d    00001101
    // SPACE    0x20    00010000
    //

    // m1 = {{byte in 0x09..=0x0d}}x32
    let m1 = s.i8xn_lt(s.u8xn_sub(x, s.u8xn_splat(0x89)), s.i8xn_splat(-128 + 5));

    // m2 = {{byte == 0x0b}}
    let m2 = s.u8xn_eq(x, s.u8xn_splat(0x0b));

    // m3 = {{byte is SPACE}}
    let m3 = s.u8xn_eq(x, s.u8xn_splat(0x20));

    // any((m1 & !m2) | m3)
    s.mask8xn_any(s.or(s.andnot(m1, m2), m3))
}

#[inline(always)]
unsafe fn find_non_ascii_whitespace_short(mut src: *const u8, len: usize) -> usize {
    let base = src;
    let end = base.add(len);
    while src < end {
        if lookup_ascii_whitespace(src.read()) != 0 {
            break;
        }
        src = src.add(1);
    }

    src.offset_from(base) as usize
}

#[inline(always)]
pub unsafe fn find_non_ascii_whitespace_fallback(src: *const u8, len: usize) -> usize {
    find_non_ascii_whitespace_short(src, len)
}

#[inline(always)]
pub unsafe fn find_non_ascii_whitespace_simd<S: SIMD256>(s: S, mut src: *const u8, len: usize) -> usize {
    let base = src;

    if matches_isa!(S, AVX2) {
        let end = src.add(len / 32 * 32);
        while src < end {
            let x = s.v256_load_unaligned(src);
            if has_ascii_whitespace(s, x) {
                break;
            }
            src = src.add(32);
        }
        if (len % 32) >= 16 {
            let x = s.v128_load_unaligned(src);
            if has_ascii_whitespace(s, x).not() {
                src = src.add(16);
            }
        }
    } else {
        let end = src.add(len / 16 * 16);
        while src < end {
            let x = s.v128_load_unaligned(src);
            if has_ascii_whitespace(s, x) {
                break;
            }
            src = src.add(16);
        }
    }

    let checked_len = src.offset_from(base) as usize;
    let pos = find_non_ascii_whitespace_short(src, len - checked_len);
    checked_len + pos
}

#[inline(always)]
#[must_use]
pub fn find_non_ascii_whitespace(data: &[u8]) -> usize {
    let (src, len) = slice_parts(data);
    unsafe { crate::multiversion::find_non_ascii_whitespace::auto(src, len) }
}

#[inline(always)]
#[must_use]
pub unsafe fn remove_ascii_whitespace_fallback(mut src: *const u8, len: usize, mut dst: *mut u8) -> usize {
    let dst_base = dst;

    let end = src.add(len);
    while src < end {
        let x = src.read();
        if lookup_ascii_whitespace(x) == 0 {
            dst.write(x);
            dst = dst.add(1);
        }
        src = src.add(1);
    }

    dst.offset_from(dst_base) as usize
}

#[inline(always)]
#[must_use]
pub fn remove_ascii_whitespace_inplace(data: &mut [u8]) -> &mut [u8] {
    let pos = find_non_ascii_whitespace(data);
    debug_assert!(pos <= data.len());

    if pos == data.len() {
        return data;
    }

    unsafe {
        let len = data.len() - pos;
        let dst = data.as_mut_ptr().add(pos);
        let src = dst;

        let rem = remove_ascii_whitespace_fallback(src, len, dst);
        debug_assert!(rem <= len);

        data.get_unchecked_mut(..(pos + rem))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[cfg_attr(not(target_arch = "wasm32"), test)]
    #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test::wasm_bindgen_test)]
    fn test_remove_ascii_whitespace() {
        let cases = [
            "\0\0\0\0",
            "abcd",
            "ab\tcd",
            "ab\ncd",
            "ab\x0Ccd",
            "ab\rcd",
            "ab cd",
            "ab\t\n\x0C\r cd",
            "ab\t\n\x0C\r =\t\n\x0C\r =\t\n\x0C\r ",
        ];

        let check = |case: &str, repeat: usize| {
            let mut buf = case.repeat(repeat).into_bytes();
            let expected = {
                let mut v = buf.clone();
                v.retain(|c| !c.is_ascii_whitespace());
                v
            };
            let ans = remove_ascii_whitespace_inplace(&mut buf);
            assert_eq!(ans, &*expected, "case = {case:?}");
        };

        for case in cases {
            check(case, 1);

            if cfg!(not(miri)) {
                check(case, 10);
            }
        }
    }
}

#[cfg(test)]
mod algorithm {
    #[test]
    #[ignore]
    fn is_ascii_whitespace() {
        for x in 0..=255u8 {
            let m1 = (x.wrapping_sub(0x89) as i8) < (-128 + 5);
            let m2 = x == 0x0b;
            let m3 = x == 0x20;
            let ans = (m1 && !m2) || m3;
            assert_eq!(ans, x.is_ascii_whitespace());
        }
    }
}
