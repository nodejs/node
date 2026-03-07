// Copyright (c) 2014 Simon Sapin
// Licensed under the MIT License
// Original source: https://github.com/SimonSapin/rust-wtf8

//! The code in this module is copied from Rust standard library
//! (the `std` crate and crates it is a facade for)
//! at commit 16d80de231abb2b1756f3951ffd4776d681035eb,
//! with the signature changed to use `Wtf8Buf`, `Wtf8`, and `CodePoint`
//! instead of `String`, `&str`, and `char`.
//!
//! FIXME: if and when this is moved into the standard library,
//! try to avoid the code duplication.
//! Maybe by having private generic code that is monomorphized to UTF-8 and
//! WTF-8?

use core::{char, mem, slice};

use super::{CodePoint, IllFormedUtf16CodeUnits, Wtf8, Wtf8Buf};

// UTF-8 ranges and tags for encoding characters
// Copied from 48d5fe9ec560b53b1f5069219b0d62015e1de5ba^:src/libcore/char.rs
const TAG_CONT: u8 = 0b1000_0000;
const TAG_TWO_B: u8 = 0b1100_0000;
const TAG_THREE_B: u8 = 0b1110_0000;
const TAG_FOUR_B: u8 = 0b1111_0000;
const MAX_ONE_B: u32 = 0x80;
const MAX_TWO_B: u32 = 0x800;
const MAX_THREE_B: u32 = 0x10000;

/// Copied from 48d5fe9ec560b53b1f5069219b0d62015e1de5ba^:src/libcore/char.rs
#[inline]
fn encode_utf8_raw(code: u32, dst: &mut [u8]) -> Option<usize> {
    // Marked #[inline] to allow llvm optimizing it away
    if code < MAX_ONE_B && !dst.is_empty() {
        dst[0] = code as u8;
        Some(1)
    } else if code < MAX_TWO_B && dst.len() >= 2 {
        dst[0] = (code >> 6 & 0x1f) as u8 | TAG_TWO_B;
        dst[1] = (code & 0x3f) as u8 | TAG_CONT;
        Some(2)
    } else if code < MAX_THREE_B && dst.len() >= 3 {
        dst[0] = (code >> 12 & 0x0f) as u8 | TAG_THREE_B;
        dst[1] = (code >> 6 & 0x3f) as u8 | TAG_CONT;
        dst[2] = (code & 0x3f) as u8 | TAG_CONT;
        Some(3)
    } else if dst.len() >= 4 {
        dst[0] = (code >> 18 & 0x07) as u8 | TAG_FOUR_B;
        dst[1] = (code >> 12 & 0x3f) as u8 | TAG_CONT;
        dst[2] = (code >> 6 & 0x3f) as u8 | TAG_CONT;
        dst[3] = (code & 0x3f) as u8 | TAG_CONT;
        Some(4)
    } else {
        None
    }
}

/// Copied from 48d5fe9ec560b53b1f5069219b0d62015e1de5ba^:src/libcore/char.rs
#[inline]
fn encode_utf16_raw(mut ch: u32, dst: &mut [u16]) -> Option<usize> {
    // Marked #[inline] to allow llvm optimizing it away
    if (ch & 0xffff) == ch && !dst.is_empty() {
        // The BMP falls through (assuming non-surrogate, as it should)
        dst[0] = ch as u16;
        Some(1)
    } else if dst.len() >= 2 {
        // Supplementary planes break into surrogates.
        ch -= 0x1_0000;
        dst[0] = 0xd800 | ((ch >> 10) as u16);
        dst[1] = 0xdc00 | ((ch as u16) & 0x3ff);
        Some(2)
    } else {
        None
    }
}

/// Copied from core::str::next_code_point
#[inline]
pub fn next_code_point(bytes: &mut slice::Iter<u8>) -> Option<u32> {
    // Decode UTF-8
    let x = match bytes.next() {
        None => return None,
        Some(&next_byte) if next_byte < 128 => return Some(next_byte as u32),
        Some(&next_byte) => next_byte,
    };

    // Multibyte case follows
    // Decode from a byte combination out of: [[[x y] z] w]
    // NOTE: Performance is sensitive to the exact formulation here
    let init = utf8_first_byte(x, 2);
    let y = unwrap_or_0(bytes.next());
    let mut ch = utf8_acc_cont_byte(init, y);
    if x >= 0xe0 {
        // [[x y z] w] case
        // 5th bit in 0xE0 .. 0xEF is always clear, so `init` is still valid
        let z = unwrap_or_0(bytes.next());
        let y_z = utf8_acc_cont_byte((y & CONT_MASK) as u32, z);
        ch = init << 12 | y_z;
        if x >= 0xf0 {
            // [x y z w] case
            // use only the lower 3 bits of `init`
            let w = unwrap_or_0(bytes.next());
            ch = (init & 7) << 18 | utf8_acc_cont_byte(y_z, w);
        }
    }

    Some(ch)
}

#[inline]
fn utf8_first_byte(byte: u8, width: u32) -> u32 {
    (byte & (0x7f >> width)) as u32
}

/// Return the value of `ch` updated with continuation byte `byte`.
#[inline]
fn utf8_acc_cont_byte(ch: u32, byte: u8) -> u32 {
    (ch << 6) | (byte & CONT_MASK) as u32
}

#[inline]
fn unwrap_or_0(opt: Option<&u8>) -> u8 {
    match opt {
        Some(&byte) => byte,
        None => 0,
    }
}

/// Mask of the value bits of a continuation byte
const CONT_MASK: u8 = 0b0011_1111;

/// Copied from String::push
/// This does **not** include the WTF-8 concatenation check.
#[inline]
pub fn push_code_point(string: &mut Wtf8Buf, code_point: CodePoint) {
    let cur_len = string.len();
    // This may use up to 4 bytes.
    string.reserve(4);

    unsafe {
        // Attempt to not use an intermediate buffer by just pushing bytes
        // directly onto this string.
        let slice = slice::from_raw_parts_mut(string.bytes.as_mut_ptr().add(cur_len), 4);
        let used = encode_utf8_raw(code_point.to_u32(), slice).unwrap_or(0);
        string.bytes.set_len(cur_len + used);
    }
}

/// Copied from core::str::StrPrelude::is_char_boundary
#[inline]
pub fn is_code_point_boundary(slice: &Wtf8, index: usize) -> bool {
    if index == slice.len() {
        return true;
    }
    match slice.bytes.get(index) {
        None => false,
        Some(&b) => !(128u8..192u8).contains(&b),
    }
}

/// Copied from core::str::raw::slice_unchecked
#[inline]
pub unsafe fn slice_unchecked(s: &Wtf8, begin: usize, end: usize) -> &Wtf8 {
    mem::transmute(slice::from_raw_parts(
        s.bytes.as_ptr().add(begin),
        end - begin,
    ))
}

/// Copied from core::str::raw::slice_error_fail
#[inline(never)]
pub fn slice_error_fail(s: &Wtf8, begin: usize, end: usize) -> ! {
    assert!(begin <= end);
    panic!("index {begin} and/or {end} in {s:?} do not lie on character boundary");
}

/// Copied from core::str::Utf16CodeUnits::next
pub fn next_utf16_code_unit(iter: &mut IllFormedUtf16CodeUnits) -> Option<u16> {
    if iter.extra != 0 {
        let tmp = iter.extra;
        iter.extra = 0;
        return Some(tmp);
    }

    let mut buf = [0u16; 2];
    iter.code_points.next().map(|code_point| {
        let n = encode_utf16_raw(code_point.to_u32(), &mut buf).unwrap_or(0);
        if n == 2 {
            iter.extra = buf[1];
        }
        buf[0]
    })
}

/// Copied from src/librustc_unicode/char.rs
pub struct DecodeUtf16<I>
where
    I: Iterator<Item = u16>,
{
    iter: I,
    buf: Option<u16>,
}

/// Copied from src/librustc_unicode/char.rs
#[inline]
pub fn decode_utf16<I: IntoIterator<Item = u16>>(iterable: I) -> DecodeUtf16<I::IntoIter> {
    DecodeUtf16 {
        iter: iterable.into_iter(),
        buf: None,
    }
}

/// Copied from src/librustc_unicode/char.rs
impl<I: Iterator<Item = u16>> Iterator for DecodeUtf16<I> {
    type Item = Result<char, u16>;

    fn next(&mut self) -> Option<Result<char, u16>> {
        let u = match self.buf.take() {
            Some(buf) => buf,
            None => self.iter.next()?,
        };

        if !(0xd800..=0xdfff).contains(&u) {
            // not a surrogate
            Some(Ok(unsafe { char::from_u32_unchecked(u as u32) }))
        } else if u >= 0xdc00 {
            // a trailing surrogate
            Some(Err(u))
        } else {
            let u2 = match self.iter.next() {
                Some(u2) => u2,
                // eof
                None => return Some(Err(u)),
            };
            if !(0xdc00..=0xdfff).contains(&u2) {
                // not a trailing surrogate so we're not a valid
                // surrogate pair, so rewind to redecode u2 next time.
                self.buf = Some(u2);
                return Some(Err(u));
            }

            // all ok, so lets decode it.
            let c = (((u - 0xd800) as u32) << 10 | (u2 - 0xdc00) as u32) + 0x1_0000;
            Some(Ok(unsafe { char::from_u32_unchecked(c) }))
        }
    }

    #[inline]
    fn size_hint(&self) -> (usize, Option<usize>) {
        let (low, high) = self.iter.size_hint();
        // we could be entirely valid surrogates (2 elements per
        // char), or entirely non-surrogates (1 element per char)
        (low / 2, high)
    }
}

/// Check if a byte is a valid UTF-8 continuation byte.
#[inline]
fn is_continuation_byte(byte: u8) -> bool {
    byte & 0xc0 == 0x80
}

/// Check if bytes at position form a surrogate pair. A surrogate pair should be
/// encoded as 4-byte sequence in UTF-8 instead of two separate 3-byte
/// sequences.
#[inline]
fn is_surrogate_pair(bytes: &[u8], pos: usize) -> bool {
    if pos + 5 >= bytes.len() {
        return false;
    }
    bytes[pos] == 0xed
        && (0xa0..0xb0).contains(&bytes[pos + 1])
        && is_continuation_byte(bytes[pos + 2])
        && bytes[pos + 3] == 0xed
        && (0xb0..0xc0).contains(&bytes[pos + 4])
        && is_continuation_byte(bytes[pos + 5])
}

/// Validate that bytes represent well-formed WTF-8.
///
/// This checks that:
/// - All bytes form valid UTF-8 sequences OR valid surrogate code point
///   encodings
/// - Surrogate pairs are not encoded as two separate 3-byte sequences
pub fn validate_wtf8(bytes: &[u8]) -> bool {
    let mut i = 0;
    while i < bytes.len() {
        let byte = bytes[i];

        // ASCII range (1-byte sequence)
        if byte < 0x80 {
            i += 1;
            continue;
        }

        // 2-byte sequence
        if byte < 0xe0 {
            if i + 1 >= bytes.len() || !is_continuation_byte(bytes[i + 1]) || byte < 0xc2 {
                return false; // Truncated, invalid continuation, or overlong
            }
            i += 2;
            continue;
        }

        // 3-byte sequence
        if byte < 0xf0 {
            if i + 2 >= bytes.len()
                || !is_continuation_byte(bytes[i + 1])
                || !is_continuation_byte(bytes[i + 2])
            {
                return false; // Truncated or invalid continuation bytes
            }

            let b2 = bytes[i + 1];

            // Check for overlong encoding
            if byte == 0xe0 && b2 < 0xa0 {
                return false;
            }

            // Check for surrogate pair (lead + trail) - invalid in WTF-8
            if is_surrogate_pair(bytes, i) {
                return false;
            }

            i += 3;
            continue;
        }

        // 4-byte sequence
        if byte < 0xf8 {
            if i + 3 >= bytes.len()
                || !is_continuation_byte(bytes[i + 1])
                || !is_continuation_byte(bytes[i + 2])
                || !is_continuation_byte(bytes[i + 3])
            {
                return false; // Truncated or invalid continuation bytes
            }

            let b2 = bytes[i + 1];

            // Check for overlong encoding and valid range
            if (byte == 0xf0 && b2 < 0x90) || (byte == 0xf4 && b2 >= 0x90) || byte > 0xf4 {
                return false;
            }

            i += 4;
            continue;
        }

        // Invalid byte (>= 0xF8)
        return false;
    }

    true
}
