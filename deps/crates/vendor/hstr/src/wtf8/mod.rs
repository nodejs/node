// Copyright (c) 2014 Simon Sapin
// Licensed under the MIT License
// Original source: https://github.com/SimonSapin/rust-wtf8

/*!

Implementation of [the WTF-8 encoding](https://simonsapin.github.io/wtf-8/).

This library uses Rust’s type system to maintain
[well-formedness](https://simonsapin.github.io/wtf-8/#well-formed),
like the `String` and `&str` types do for UTF-8.

Since [WTF-8 must not be used
for interchange](https://simonsapin.github.io/wtf-8/#intended-audience),
this library deliberately does not provide access to the underlying bytes
of WTF-8 strings,
nor can it decode WTF-8 from arbitrary bytes.
WTF-8 strings can be obtained from UTF-8, UTF-16, or code points.

*/

extern crate alloc;

use alloc::{
    borrow::{Borrow, Cow},
    string::String,
    vec::Vec,
};
use core::{
    fmt, hash,
    iter::{FromIterator, IntoIterator},
    mem::transmute,
    ops::Deref,
    slice, str,
    str::FromStr,
};
use std::ops::Add;

mod not_quite_std;

static UTF8_REPLACEMENT_CHARACTER: &[u8] = b"\xEF\xBF\xBD";

/// A Unicode code point: from U+0000 to U+10FFFF.
///
/// Compare with the `char` type,
/// which represents a Unicode scalar value:
/// a code point that is not a surrogate (U+D800 to U+DFFF).
#[derive(Eq, PartialEq, Ord, PartialOrd, Clone)]
pub struct CodePoint {
    value: u32,
}

impl Copy for CodePoint {}

/// Format the code point as `U+` followed by four to six hexadecimal digits.
/// Example: `U+1F4A9`
impl fmt::Debug for CodePoint {
    #[inline]
    fn fmt(&self, formatter: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        write!(formatter, "U+{:04X}", self.value)
    }
}

impl CodePoint {
    /// Unsafely create a new `CodePoint` without checking the value.
    ///
    /// # Safety
    ///
    /// Only use when `value` is known to be less than or equal to 0x10FFFF.
    #[inline]
    pub const unsafe fn from_u32_unchecked(value: u32) -> CodePoint {
        CodePoint { value }
    }

    /// Create a new `CodePoint` if the value is a valid code point.
    ///
    /// Return `None` if `value` is above 0x10FFFF.
    #[inline]
    pub const fn from_u32(value: u32) -> Option<CodePoint> {
        match value {
            0..=0x10ffff => Some(CodePoint { value }),
            _ => None,
        }
    }

    /// Create a new `CodePoint` from a `char`.
    ///
    /// Since all Unicode scalar values are code points, this always succeds.
    #[inline]
    pub const fn from_char(value: char) -> CodePoint {
        CodePoint {
            value: value as u32,
        }
    }

    /// Return the numeric value of the code point.
    #[inline]
    pub fn to_u32(&self) -> u32 {
        self.value
    }

    /// Optionally return a Unicode scalar value for the code point.
    ///
    /// Return `None` if the code point is a surrogate (from U+D800 to U+DFFF).
    #[inline]
    pub fn to_char(&self) -> Option<char> {
        match self.value {
            0xd800..=0xdfff => None,
            _ => Some(unsafe { char::from_u32_unchecked(self.value) }),
        }
    }

    /// Return a Unicode scalar value for the code point.
    ///
    /// Return `'\u{FFFD}'` (the replacement character “�”)
    /// if the code point is a surrogate (from U+D800 to U+DFFF).
    #[inline]
    pub fn to_char_lossy(&self) -> char {
        self.to_char().unwrap_or('\u{FFFD}')
    }

    /// Return `true` if the code point is in the ASCII range.
    #[inline]
    pub fn is_ascii(&self) -> bool {
        self.value <= 0x7f
    }
}

impl PartialEq<char> for CodePoint {
    fn eq(&self, other: &char) -> bool {
        self.value == *other as u32
    }
}

/// An owned, growable string of well-formed WTF-8 data.
///
/// Similar to `String`, but can additionally contain surrogate code points
/// if they’re not in a surrogate pair.
#[derive(Eq, PartialEq, Ord, PartialOrd, Clone)]
pub struct Wtf8Buf {
    bytes: Vec<u8>,
}

impl Deref for Wtf8Buf {
    type Target = Wtf8;

    fn deref(&self) -> &Wtf8 {
        unsafe { transmute(&*self.bytes) }
    }
}

/// Format the string with double quotes,
/// and surrogates as `\u` followed by four hexadecimal digits.
/// Example: `"a\u{D800}"` for a string with code points [U+0061, U+D800]
impl fmt::Debug for Wtf8Buf {
    #[inline]
    fn fmt(&self, formatter: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        Wtf8::fmt(self, formatter)
    }
}

impl Default for Wtf8Buf {
    #[inline]
    fn default() -> Self {
        Self::new()
    }
}

impl FromStr for Wtf8Buf {
    type Err = core::convert::Infallible;

    #[inline]
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Ok(Wtf8Buf {
            bytes: s.as_bytes().to_vec(),
        })
    }
}

impl fmt::Write for Wtf8Buf {
    fn write_str(&mut self, s: &str) -> std::fmt::Result {
        self.push_str(s);
        Ok(())
    }
}

impl Add<&Wtf8> for Wtf8Buf {
    type Output = Wtf8Buf;

    fn add(self, rhs: &Wtf8) -> Self::Output {
        let mut result = self;
        result.push_wtf8(rhs);
        result
    }
}

impl Wtf8Buf {
    /// Create an new, empty WTF-8 string.
    #[inline]
    pub fn new() -> Wtf8Buf {
        Wtf8Buf { bytes: Vec::new() }
    }

    /// Create an new, empty WTF-8 string with pre-allocated capacity for `n`
    /// bytes.
    #[inline]
    pub fn with_capacity(n: usize) -> Wtf8Buf {
        Wtf8Buf {
            bytes: Vec::with_capacity(n),
        }
    }

    /// Create a WTF-8 string from an UTF-8 `String`.
    ///
    /// This takes ownership of the `String` and does not copy.
    ///
    /// Since WTF-8 is a superset of UTF-8, this always succeeds.
    #[inline]
    pub fn from_string(string: String) -> Wtf8Buf {
        Wtf8Buf {
            bytes: string.into_bytes(),
        }
    }

    /// Create a WTF-8 string from an UTF-8 `&str` slice.
    ///
    /// This copies the content of the slice.
    ///
    /// Since WTF-8 is a superset of UTF-8, this always succeeds.
    #[inline]
    #[allow(clippy::should_implement_trait)]
    pub fn from_str(s: &str) -> Wtf8Buf {
        Wtf8Buf {
            bytes: s.as_bytes().to_vec(),
        }
    }

    /// Create a WTF-8 string from a potentially ill-formed UTF-16 slice of
    /// 16-bit code units.
    ///
    /// This is lossless: calling `.to_ill_formed_utf16()` on the resulting
    /// string will always return the original code units.
    pub fn from_ill_formed_utf16(v: &[u16]) -> Wtf8Buf {
        let mut string = Wtf8Buf::with_capacity(v.len());
        for item in not_quite_std::decode_utf16(v.iter().cloned()) {
            match item {
                Ok(c) => string.push_char(c),
                Err(s) => {
                    // Surrogates are known to be in the code point range.
                    let code_point = unsafe { CodePoint::from_u32_unchecked(s as u32) };
                    // Skip the WTF-8 concatenation check,
                    // surrogate pairs are already decoded by utf16_items
                    not_quite_std::push_code_point(&mut string, code_point)
                }
            }
        }
        string
    }

    /// Reserves capacity for at least `additional` more bytes to be inserted
    /// in the given `Wtf8Buf`.
    /// The collection may reserve more space to avoid frequent reallocations.
    ///
    /// # Panics
    ///
    /// Panics if the new capacity overflows `usize`.
    #[inline]
    pub fn reserve(&mut self, additional: usize) {
        self.bytes.reserve(additional)
    }

    /// Returns the number of bytes that this string buffer can hold without
    /// reallocating.
    #[inline]
    pub fn capacity(&self) -> usize {
        self.bytes.capacity()
    }

    /// Append an UTF-8 slice at the end of the string.
    #[inline]
    pub fn push_str(&mut self, other: &str) {
        self.bytes.extend_from_slice(other.as_bytes())
    }

    /// Append a WTF-8 slice at the end of the string.
    ///
    /// This replaces newly paired surrogates at the boundary
    /// with a supplementary code point,
    /// like concatenating ill-formed UTF-16 strings effectively would.
    #[inline]
    pub fn push_wtf8(&mut self, other: &Wtf8) {
        match (self.final_lead_surrogate(), other.initial_trail_surrogate()) {
            // Replace newly paired surrogates by a supplementary code point.
            (Some(lead), Some(trail)) => {
                let len_without_lead_surrogate = self.len() - 3;
                self.bytes.truncate(len_without_lead_surrogate);
                let other_without_trail_surrogate = &other.bytes[3..];
                // 4 bytes for the supplementary code point
                self.bytes.reserve(4 + other_without_trail_surrogate.len());
                self.push_char(decode_surrogate_pair(lead, trail));
                self.bytes.extend_from_slice(other_without_trail_surrogate);
            }
            _ => self.bytes.extend_from_slice(&other.bytes),
        }
    }

    /// Append a Unicode scalar value at the end of the string.
    #[inline]
    pub fn push_char(&mut self, c: char) {
        not_quite_std::push_code_point(self, CodePoint::from_char(c))
    }

    /// Append a code point at the end of the string.
    ///
    /// This replaces newly paired surrogates at the boundary
    /// with a supplementary code point,
    /// like concatenating ill-formed UTF-16 strings effectively would.
    #[inline]
    pub fn push(&mut self, code_point: CodePoint) {
        if let trail @ 0xdc00..=0xdfff = code_point.to_u32() {
            if let Some(lead) = self.final_lead_surrogate() {
                let len_without_lead_surrogate = self.len() - 3;
                self.bytes.truncate(len_without_lead_surrogate);
                self.push_char(decode_surrogate_pair(lead, trail as u16));
                return;
            }
        }

        // No newly paired surrogates at the boundary.
        not_quite_std::push_code_point(self, code_point)
    }

    /// Shortens a string to the specified length.
    ///
    /// # Failure
    ///
    /// Fails if `new_len` > current length,
    /// or if `new_len` is not a code point boundary.
    #[inline]
    pub fn truncate(&mut self, new_len: usize) {
        assert!(not_quite_std::is_code_point_boundary(self, new_len));
        self.bytes.truncate(new_len)
    }

    /// Clear the WTF-8 vector, removing all contents.
    #[inline]
    pub fn clear(&mut self) {
        self.bytes.clear();
    }

    /// Consume the WTF-8 string and try to convert it to UTF-8.
    ///
    /// This does not copy the data.
    ///
    /// If the contents are not well-formed UTF-8
    /// (that is, if the string contains surrogates),
    /// the original WTF-8 string is returned instead.
    pub fn into_string(self) -> Result<String, Wtf8Buf> {
        match self.next_surrogate(0) {
            None => Ok(unsafe { String::from_utf8_unchecked(self.bytes) }),
            Some(_) => Err(self),
        }
    }

    /// Consume the WTF-8 string and convert it lossily to UTF-8.
    ///
    /// This does not copy the data (but may overwrite parts of it in place).
    ///
    /// Surrogates are replaced with `"\u{FFFD}"` (the replacement character
    /// “�”)
    pub fn into_string_lossy(mut self) -> String {
        let mut pos = 0;
        loop {
            match self.next_surrogate(pos) {
                Some((surrogate_pos, _)) => {
                    pos = surrogate_pos + 3;
                    self.bytes[surrogate_pos..pos].copy_from_slice(UTF8_REPLACEMENT_CHARACTER);
                }
                None => return unsafe { String::from_utf8_unchecked(self.bytes) },
            }
        }
    }

    /// Create a [Wtf8Buf] from a WTF-8 encoded byte vector.
    ///
    /// Returns `Ok(Wtf8Buf)` if the bytes are well-formed WTF-8, or
    /// `Err(bytes)` with the original bytes if validation fails.
    ///
    /// This validates that:
    /// - All bytes form valid UTF-8 sequences OR valid surrogate code point
    ///   encodings
    /// - Surrogate code points may appear unpaired and be encoded separately,
    ///   but if they are paired, they must be encoded as a single 4-byte UTF-8
    ///   sequence. For example, the byte sequence `[0xED, 0xA0, 0x80, 0xED,
    ///   0xB0, 0x80]` is not valid WTF-8 because WTF-8 forbids encoding a
    ///   surrogate pair as two separate 3-byte sequences.
    pub fn from_bytes(bytes: Vec<u8>) -> Result<Self, Vec<u8>> {
        if not_quite_std::validate_wtf8(&bytes) {
            Ok(Self { bytes })
        } else {
            Err(bytes)
        }
    }

    /// Create a [Wtf8Buf] from a WTF-8 encoded byte vector without checking
    /// that the bytes contain valid WTF-8.
    ///
    /// For the safe version, see [Wtf8Buf::from_bytes].
    ///
    /// # Safety
    ///
    /// The bytes passed in must be valid WTF-8. See [Wtf8Buf::from_bytes] for
    /// the requirements.
    #[inline]
    pub unsafe fn from_bytes_unchecked(bytes: Vec<u8>) -> Self {
        Self { bytes }
    }
}

/// Create a new WTF-8 string from an iterator of code points.
///
/// This replaces surrogate code point pairs with supplementary code points,
/// like concatenating ill-formed UTF-16 strings effectively would.
impl FromIterator<CodePoint> for Wtf8Buf {
    fn from_iter<T: IntoIterator<Item = CodePoint>>(iterable: T) -> Wtf8Buf {
        let mut string = Wtf8Buf::new();
        string.extend(iterable);
        string
    }
}

/// Append code points from an iterator to the string.
///
/// This replaces surrogate code point pairs with supplementary code points,
/// like concatenating ill-formed UTF-16 strings effectively would.
impl Extend<CodePoint> for Wtf8Buf {
    fn extend<T: IntoIterator<Item = CodePoint>>(&mut self, iterable: T) {
        let iterator = iterable.into_iter();
        let (low, _high) = iterator.size_hint();
        // Lower bound of one byte per code point (ASCII only)
        self.bytes.reserve(low);
        for code_point in iterator {
            self.push(code_point);
        }
    }
}

/// A borrowed slice of well-formed WTF-8 data.
///
/// Similar to `&str`, but can additionally contain surrogate code points
/// if they're not in a surrogate pair.
#[repr(transparent)]
#[derive(PartialEq, Eq, PartialOrd, Ord)]
pub struct Wtf8 {
    bytes: [u8],
}

/// Format the slice with double quotes,
/// and surrogates as `\u` followed by four hexadecimal digits.
/// Example: `"a\u{D800}"` for a slice with code points [U+0061, U+D800]
impl fmt::Debug for Wtf8 {
    fn fmt(&self, formatter: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        formatter.write_str("\"")?;
        let mut pos = 0;
        loop {
            match self.next_surrogate(pos) {
                None => break,
                Some((surrogate_pos, surrogate)) => {
                    formatter.write_str(unsafe {
                        str::from_utf8_unchecked(&self.bytes[pos..surrogate_pos])
                    })?;
                    write!(formatter, "\\u{{{surrogate:X}}}")?;
                    pos = surrogate_pos + 3;
                }
            }
        }
        formatter.write_str(unsafe { str::from_utf8_unchecked(&self.bytes[pos..]) })?;
        formatter.write_str("\"")
    }
}

impl Wtf8 {
    /// Create a WTF-8 slice from a UTF-8 `&str` slice.
    ///
    /// Since WTF-8 is a superset of UTF-8, this always succeeds.
    #[inline]
    pub const fn from_str(value: &str) -> &Wtf8 {
        unsafe { transmute(value.as_bytes()) }
    }

    /// Return the length, in WTF-8 bytes.
    #[inline]
    pub const fn len(&self) -> usize {
        self.bytes.len()
    }

    /// Return `true` if the string has a length of zero bytes.
    #[inline]
    pub const fn is_empty(&self) -> bool {
        self.bytes.is_empty()
    }

    /// Return `true` if the string contains only ASCII characters.
    #[inline]
    pub const fn is_ascii(&self) -> bool {
        self.bytes.is_ascii()
    }

    /// Return a slice of the given string for the byte range [`begin`..`end`).
    ///
    /// # Failure
    ///
    /// Fails when `begin` and `end` do not point to code point boundaries,
    /// or point beyond the end of the string.
    #[inline]
    pub fn slice(&self, begin: usize, end: usize) -> &Wtf8 {
        // is_code_point_boundary checks that the index is in [0, .len()]
        if begin <= end
            && not_quite_std::is_code_point_boundary(self, begin)
            && not_quite_std::is_code_point_boundary(self, end)
        {
            unsafe { not_quite_std::slice_unchecked(self, begin, end) }
        } else {
            not_quite_std::slice_error_fail(self, begin, end)
        }
    }

    /// Return a slice of the given string from byte `begin` to its end.
    ///
    /// # Failure
    ///
    /// Fails when `begin` is not at a code point boundary,
    /// or is beyond the end of the string.
    #[inline]
    pub fn slice_from(&self, begin: usize) -> &Wtf8 {
        // is_code_point_boundary checks that the index is in [0, .len()]
        if not_quite_std::is_code_point_boundary(self, begin) {
            unsafe { not_quite_std::slice_unchecked(self, begin, self.len()) }
        } else {
            not_quite_std::slice_error_fail(self, begin, self.len())
        }
    }

    /// Return a slice of the given string from its beginning to byte `end`.
    ///
    /// # Failure
    ///
    /// Fails when `end` is not at a code point boundary,
    /// or is beyond the end of the string.
    #[inline]
    pub fn slice_to(&self, end: usize) -> &Wtf8 {
        // is_code_point_boundary checks that the index is in [0, .len()]
        if not_quite_std::is_code_point_boundary(self, end) {
            unsafe { not_quite_std::slice_unchecked(self, 0, end) }
        } else {
            not_quite_std::slice_error_fail(self, 0, end)
        }
    }

    /// Return the code point at `position` if it is in the ASCII range,
    /// or `b'\xFF' otherwise.
    ///
    /// # Failure
    ///
    /// Fails if `position` is beyond the end of the string.
    #[inline]
    pub fn ascii_byte_at(&self, position: usize) -> u8 {
        match self.bytes[position] {
            ascii_byte @ 0x00..=0x7f => ascii_byte,
            _ => 0xff,
        }
    }

    /// Return an iterator for the string’s code points.
    #[inline]
    pub fn code_points(&self) -> Wtf8CodePoints {
        Wtf8CodePoints {
            bytes: self.bytes.iter(),
        }
    }

    /// Returns `true` if this WTF-8 string contains the given character.
    #[inline]
    pub fn contains_char(&self, ch: char) -> bool {
        let target = CodePoint::from_char(ch);
        self.contains(target)
    }

    /// Returns `true` if this WTF-8 string contains the given code point.
    #[inline]
    pub fn contains(&self, code_point: CodePoint) -> bool {
        self.code_points().any(|cp| cp == code_point)
    }

    /// Returns `true` if this WTF-8 string starts with the given UTF-8 string.
    #[inline]
    pub fn starts_with(&self, pattern: &str) -> bool {
        if pattern.len() > self.len() {
            return false;
        }

        let pattern_wtf8 = self.slice_to(pattern.len());
        if let Some(pattern_str) = pattern_wtf8.as_str() {
            pattern_str == pattern
        } else {
            false
        }
    }

    /// Try to convert the string to UTF-8 and return a `&str` slice.
    ///
    /// Return `None` if the string contains surrogates.
    ///
    /// This does not copy the data.
    #[inline]
    pub fn as_str(&self) -> Option<&str> {
        // Well-formed WTF-8 is also well-formed UTF-8
        // if and only if it contains no surrogate.
        match self.next_surrogate(0) {
            None => Some(unsafe { str::from_utf8_unchecked(&self.bytes) }),
            Some(_) => None,
        }
    }

    /// Return the underlying WTF-8 bytes.
    #[inline]
    pub const fn as_bytes(&self) -> &[u8] {
        &self.bytes
    }

    /// Lossily convert the string to UTF-8.
    /// Return an UTF-8 `&str` slice if the contents are well-formed in UTF-8.
    ///
    /// Surrogates are replaced with `"\u{FFFD}"` (the replacement character
    /// “�”).
    ///
    /// This only copies the data if necessary (if it contains any surrogate).
    pub fn to_string_lossy(&self) -> Cow<str> {
        let surrogate_pos = match self.next_surrogate(0) {
            None => return Cow::Borrowed(unsafe { str::from_utf8_unchecked(&self.bytes) }),
            Some((pos, _)) => pos,
        };
        let wtf8_bytes = &self.bytes;
        let mut utf8_bytes = Vec::with_capacity(self.len());
        utf8_bytes.extend_from_slice(&wtf8_bytes[..surrogate_pos]);
        utf8_bytes.extend_from_slice(UTF8_REPLACEMENT_CHARACTER);
        let mut pos = surrogate_pos + 3;
        loop {
            match self.next_surrogate(pos) {
                Some((surrogate_pos, _)) => {
                    utf8_bytes.extend_from_slice(&wtf8_bytes[pos..surrogate_pos]);
                    utf8_bytes.extend_from_slice(UTF8_REPLACEMENT_CHARACTER);
                    pos = surrogate_pos + 3;
                }
                None => {
                    utf8_bytes.extend_from_slice(&wtf8_bytes[pos..]);
                    return Cow::Owned(unsafe { String::from_utf8_unchecked(utf8_bytes) });
                }
            }
        }
    }

    /// Convert the WTF-8 string to potentially ill-formed UTF-16
    /// and return an iterator of 16-bit code units.
    ///
    /// This is lossless:
    /// calling `Wtf8Buf::from_ill_formed_utf16` on the resulting code units
    /// would always return the original WTF-8 string.
    #[inline]
    pub fn to_ill_formed_utf16(&self) -> IllFormedUtf16CodeUnits {
        IllFormedUtf16CodeUnits {
            code_points: self.code_points(),
            extra: 0,
        }
    }

    /// Returns the uppercase equivalent of this wtf8 slice, as a new [Wtf8Buf].
    #[inline]
    pub fn to_uppercase(&self) -> Wtf8Buf {
        let mut result = Wtf8Buf::with_capacity(self.len());
        for cp in self.code_points() {
            if let Some(ch) = cp.to_char() {
                for upper_ch in ch.to_uppercase() {
                    result.push_char(upper_ch);
                }
            } else {
                // Surrogates are known to be in the code point range.
                let code_point = unsafe { CodePoint::from_u32_unchecked(cp.to_u32()) };
                // Skip the WTF-8 concatenation check,
                // surrogate pairs are already decoded by utf16_items
                not_quite_std::push_code_point(&mut result, code_point)
            }
        }
        result
    }

    /// Returns the lowercase equivalent of this wtf8 slice, as a new [Wtf8Buf].
    #[inline]
    pub fn to_lowercase(&self) -> Wtf8Buf {
        let mut result = Wtf8Buf::with_capacity(self.len());
        for cp in self.code_points() {
            if let Some(ch) = cp.to_char() {
                for lower_ch in ch.to_lowercase() {
                    result.push_char(lower_ch);
                }
            } else {
                // Surrogates are known to be in the code point range.
                let code_point = unsafe { CodePoint::from_u32_unchecked(cp.to_u32()) };
                // Skip the WTF-8 concatenation check,
                // surrogate pairs are already decoded by utf16_items
                not_quite_std::push_code_point(&mut result, code_point)
            }
        }
        result
    }

    /// Create a WTF-8 slice from a WTF-8 encoded byte slice.
    ///
    /// Returns `Ok(&Wtf8)` if the bytes are well-formed WTF-8, or
    /// `Err(bytes)` with the original byte slice if validation fails.
    ///
    /// This validates that:
    /// - All bytes form valid UTF-8 sequences OR valid surrogate code point
    ///   encodings
    /// - Surrogate code points may appear unpaired and be encoded separately,
    ///   but if they are paired, they must be encoded as a single 4-byte UTF-8
    ///   sequence. For example, the byte sequence `[0xED, 0xA0, 0x80, 0xED,
    ///   0xB0, 0x80]` is not valid WTF-8 because WTF-8 forbids encoding a
    ///   surrogate pair as two separate 3-byte sequences.
    pub fn from_bytes(bytes: &[u8]) -> Result<&Wtf8, &[u8]> {
        if not_quite_std::validate_wtf8(bytes) {
            Ok(unsafe { transmute::<&[u8], &Wtf8>(bytes) })
        } else {
            Err(bytes)
        }
    }

    /// Create a WTF-8 slice from a WTF-8 encoded byte slice without checking
    /// that the bytes contain valid WTF-8.
    ///
    /// For the safe version, see [Wtf8::from_bytes].
    ///
    /// # Safety
    ///
    /// The bytes passed in must be valid WTF-8. See [Wtf8::from_bytes] for
    /// the requirements.
    #[inline]
    pub const unsafe fn from_bytes_unchecked(bytes: &[u8]) -> &Wtf8 {
        unsafe { transmute(bytes) }
    }

    #[inline]
    fn next_surrogate(&self, mut pos: usize) -> Option<(usize, u16)> {
        let mut iter = self.bytes[pos..].iter();
        loop {
            let b = match iter.next() {
                None => return None,
                Some(&b) => b,
            };
            if b < 0x80 {
                pos += 1;
            } else if b < 0xe0 {
                iter.next();
                pos += 2;
            } else if b == 0xed {
                match (iter.next(), iter.next()) {
                    (Some(&b2), Some(&b3)) if b2 >= 0xa0 => {
                        return Some((pos, decode_surrogate(b2, b3)))
                    }
                    _ => pos += 3,
                }
            } else if b < 0xf0 {
                iter.next();
                iter.next();
                pos += 3;
            } else {
                iter.next();
                iter.next();
                iter.next();
                pos += 4;
            }
        }
    }

    #[inline]
    fn final_lead_surrogate(&self) -> Option<u16> {
        let len = self.len();
        if len < 3 {
            return None;
        }
        let seq = &self.bytes[len - 3..];
        if seq[0] == 0xed && 0xa0 <= seq[1] && seq[1] <= 0xaf {
            Some(decode_surrogate(seq[1], seq[2]))
        } else {
            None
        }
    }

    #[inline]
    fn initial_trail_surrogate(&self) -> Option<u16> {
        let len = self.len();
        if len < 3 {
            return None;
        }
        let seq = &self.bytes[..3];
        if seq[0] == 0xed && 0xb0 <= seq[1] && seq[1] <= 0xbf {
            Some(decode_surrogate(seq[1], seq[2]))
        } else {
            None
        }
    }
}

#[inline]
fn decode_surrogate(second_byte: u8, third_byte: u8) -> u16 {
    // The first byte is assumed to be 0xED
    0xd800 | (second_byte as u16 & 0x3f) << 6 | third_byte as u16 & 0x3f
}

#[inline]
fn decode_surrogate_pair(lead: u16, trail: u16) -> char {
    let code_point = 0x10000 + (((lead as u32 - 0xd800) << 10) | (trail as u32 - 0xdc00));
    unsafe { char::from_u32_unchecked(code_point) }
}

/// Iterator for the code points of a WTF-8 string.
///
/// Created with the method `.code_points()`.
#[derive(Clone)]
pub struct Wtf8CodePoints<'a> {
    bytes: slice::Iter<'a, u8>,
}

impl<'a> Iterator for Wtf8CodePoints<'a> {
    type Item = CodePoint;

    #[inline]
    fn next(&mut self) -> Option<CodePoint> {
        not_quite_std::next_code_point(&mut self.bytes).map(|value| {
            // Wtf8 invariant says `value` is a valid code point
            unsafe { CodePoint::from_u32_unchecked(value) }
        })
    }

    #[inline]
    fn size_hint(&self) -> (usize, Option<usize>) {
        let (len, _) = self.bytes.size_hint();
        (len.saturating_add(3) / 4, Some(len))
    }
}

#[derive(Clone)]
pub struct IllFormedUtf16CodeUnits<'a> {
    code_points: Wtf8CodePoints<'a>,
    extra: u16,
}

impl<'a> Iterator for IllFormedUtf16CodeUnits<'a> {
    type Item = u16;

    #[inline]
    fn next(&mut self) -> Option<u16> {
        not_quite_std::next_utf16_code_unit(self)
    }

    #[inline]
    fn size_hint(&self) -> (usize, Option<usize>) {
        let (low, high) = self.code_points.size_hint();
        // every code point gets either one u16 or two u16,
        // so this iterator is between 1 or 2 times as
        // long as the underlying iterator.
        (low, high.and_then(|n| n.checked_mul(2)))
    }
}

impl PartialEq<&Wtf8> for Wtf8Buf {
    fn eq(&self, other: &&Wtf8) -> bool {
        **self == **other
    }
}

impl PartialEq<Wtf8Buf> for &Wtf8 {
    fn eq(&self, other: &Wtf8Buf) -> bool {
        **self == **other
    }
}

impl PartialEq<str> for &Wtf8 {
    fn eq(&self, other: &str) -> bool {
        match self.as_str() {
            Some(s) => s == other,
            None => false,
        }
    }
}

impl PartialEq<&str> for &Wtf8 {
    fn eq(&self, other: &&str) -> bool {
        match self.as_str() {
            Some(s) => s == *other,
            None => false,
        }
    }
}

impl hash::Hash for CodePoint {
    #[inline]
    fn hash<H: hash::Hasher>(&self, state: &mut H) {
        self.value.hash(state)
    }
}

impl hash::Hash for Wtf8Buf {
    #[inline]
    fn hash<H: hash::Hasher>(&self, state: &mut H) {
        Wtf8::hash(self, state)
    }
}

impl hash::Hash for Wtf8 {
    #[inline]
    fn hash<H: hash::Hasher>(&self, state: &mut H) {
        state.write(&self.bytes);
        0xfeu8.hash(state)
    }
}

impl Borrow<Wtf8> for Wtf8Buf {
    #[inline]
    fn borrow(&self) -> &Wtf8 {
        self
    }
}

impl ToOwned for Wtf8 {
    type Owned = Wtf8Buf;

    #[inline]
    fn to_owned(&self) -> Wtf8Buf {
        Wtf8Buf {
            bytes: self.bytes.to_vec(),
        }
    }
}

impl<'a> From<&'a Wtf8> for Cow<'a, Wtf8> {
    #[inline]
    fn from(s: &'a Wtf8) -> Cow<'a, Wtf8> {
        Cow::Borrowed(s)
    }
}

impl<'a> From<&'a str> for &'a Wtf8 {
    #[inline]
    fn from(s: &'a str) -> &'a Wtf8 {
        Wtf8::from_str(s)
    }
}

impl<'a> From<Wtf8Buf> for Cow<'a, Wtf8> {
    #[inline]
    fn from(s: Wtf8Buf) -> Cow<'a, Wtf8> {
        Cow::Owned(s)
    }
}

#[cfg(test)]
mod tests {
    use alloc::{format, vec};
    use core::mem::transmute;

    use super::*;

    #[test]
    fn code_point_from_u32() {
        assert!(CodePoint::from_u32(0).is_some());
        assert!(CodePoint::from_u32(0xd800).is_some());
        assert!(CodePoint::from_u32(0x10ffff).is_some());
        assert!(CodePoint::from_u32(0x110000).is_none());
    }

    #[test]
    fn code_point_to_u32() {
        fn c(value: u32) -> CodePoint {
            CodePoint::from_u32(value).unwrap()
        }
        assert_eq!(c(0).to_u32(), 0);
        assert_eq!(c(0xd800).to_u32(), 0xd800);
        assert_eq!(c(0x10ffff).to_u32(), 0x10ffff);
    }

    #[test]
    fn code_point_from_char() {
        assert_eq!(CodePoint::from_char('a').to_u32(), 0x61);
        assert_eq!(CodePoint::from_char('💩').to_u32(), 0x1f4a9);
    }

    #[test]
    fn code_point_to_string() {
        let cp_a = CodePoint::from_char('a');
        assert_eq!(format!("{cp_a:?}"), "U+0061");
        let cp_poop = CodePoint::from_char('💩');
        assert_eq!(format!("{cp_poop:?}"), "U+1F4A9");
    }

    #[test]
    fn code_point_to_char() {
        fn c(value: u32) -> CodePoint {
            CodePoint::from_u32(value).unwrap()
        }
        assert_eq!(c(0x61).to_char(), Some('a'));
        assert_eq!(c(0x1f4a9).to_char(), Some('💩'));
        assert_eq!(c(0xd800).to_char(), None);
    }

    #[test]
    fn code_point_to_char_lossy() {
        fn c(value: u32) -> CodePoint {
            CodePoint::from_u32(value).unwrap()
        }
        assert_eq!(c(0x61).to_char_lossy(), 'a');
        assert_eq!(c(0x1f4a9).to_char_lossy(), '💩');
        assert_eq!(c(0xd800).to_char_lossy(), '\u{FFFD}');
    }

    #[test]
    fn wtf8buf_new() {
        assert_eq!(Wtf8Buf::new().bytes, b"");
    }

    #[test]
    fn wtf8buf_from_str() {
        assert_eq!(Wtf8Buf::from_str("").bytes, b"");
        assert_eq!(
            Wtf8Buf::from_str("aé 💩").bytes,
            b"a\xC3\xA9 \xF0\x9F\x92\xA9"
        );
    }

    #[test]
    fn wtf8buf_from_string() {
        assert_eq!(Wtf8Buf::from_string(String::from("")).bytes, b"");
        assert_eq!(
            Wtf8Buf::from_string(String::from("aé 💩")).bytes,
            b"a\xC3\xA9 \xF0\x9F\x92\xA9"
        );
    }

    #[test]
    fn wtf8buf_from_ill_formed_utf16() {
        assert_eq!(Wtf8Buf::from_ill_formed_utf16(&[]).bytes, b"");
        assert_eq!(
            Wtf8Buf::from_ill_formed_utf16(&[0x61, 0xe9, 0x20, 0xd83d, 0xd83d, 0xdca9]).bytes,
            b"a\xC3\xA9 \xED\xA0\xBD\xF0\x9F\x92\xA9"
        );
    }

    #[test]
    fn wtf8buf_push_str() {
        let mut string = Wtf8Buf::new();
        assert_eq!(string.bytes, b"");
        string.push_str("aé 💩");
        assert_eq!(string.bytes, b"a\xC3\xA9 \xF0\x9F\x92\xA9");
    }

    #[test]
    fn wtf8buf_push_char() {
        let mut string = Wtf8Buf::from_str("aé ");
        assert_eq!(string.bytes, b"a\xC3\xA9 ");
        string.push_char('💩');
        assert_eq!(string.bytes, b"a\xC3\xA9 \xF0\x9F\x92\xA9");
    }

    #[test]
    fn wtf8buf_push() {
        let mut string = Wtf8Buf::from_str("aé ");
        assert_eq!(string.bytes, b"a\xC3\xA9 ");
        string.push(CodePoint::from_char('💩'));
        assert_eq!(string.bytes, b"a\xC3\xA9 \xF0\x9F\x92\xA9");

        fn c(value: u32) -> CodePoint {
            CodePoint::from_u32(value).unwrap()
        }

        let mut string = Wtf8Buf::new();
        string.push(c(0xd83d)); // lead
        string.push(c(0xdca9)); // trail
        assert_eq!(string.bytes, b"\xF0\x9F\x92\xA9"); // Magic!

        let mut string = Wtf8Buf::new();
        string.push(c(0xd83d)); // lead
        string.push(c(0x20)); // not surrogate
        string.push(c(0xdca9)); // trail
        assert_eq!(string.bytes, b"\xED\xA0\xBD \xED\xB2\xA9");

        let mut string = Wtf8Buf::new();
        string.push(c(0xd800)); // lead
        string.push(c(0xdbff)); // lead
        assert_eq!(string.bytes, b"\xED\xA0\x80\xED\xAF\xBF");

        let mut string = Wtf8Buf::new();
        string.push(c(0xd800)); // lead
        string.push(c(0xe000)); // not surrogate
        assert_eq!(string.bytes, b"\xED\xA0\x80\xEE\x80\x80");

        let mut string = Wtf8Buf::new();
        string.push(c(0xd7ff)); // not surrogate
        string.push(c(0xdc00)); // trail
        assert_eq!(string.bytes, b"\xED\x9F\xBF\xED\xB0\x80");

        let mut string = Wtf8Buf::new();
        string.push(c(0x61)); // not surrogate, < 3 bytes
        string.push(c(0xdc00)); // trail
        assert_eq!(string.bytes, b"\x61\xED\xB0\x80");

        let mut string = Wtf8Buf::new();
        string.push(c(0xdc00)); // trail
        assert_eq!(string.bytes, b"\xED\xB0\x80");
    }

    #[test]
    fn wtf8buf_push_wtf8() {
        let mut string = Wtf8Buf::from_str("aé");
        assert_eq!(string.bytes, b"a\xC3\xA9");
        string.push_wtf8(Wtf8::from_str(" 💩"));
        assert_eq!(string.bytes, b"a\xC3\xA9 \xF0\x9F\x92\xA9");

        fn w(value: &[u8]) -> &Wtf8 {
            unsafe { transmute(value) }
        }

        let mut string = Wtf8Buf::new();
        string.push_wtf8(w(b"\xED\xA0\xBD")); // lead
        string.push_wtf8(w(b"\xED\xB2\xA9")); // trail
        assert_eq!(string.bytes, b"\xF0\x9F\x92\xA9"); // Magic!

        let mut string = Wtf8Buf::new();
        string.push_wtf8(w(b"\xED\xA0\xBD")); // lead
        string.push_wtf8(w(b" ")); // not surrogate
        string.push_wtf8(w(b"\xED\xB2\xA9")); // trail
        assert_eq!(string.bytes, b"\xED\xA0\xBD \xED\xB2\xA9");

        let mut string = Wtf8Buf::new();
        string.push_wtf8(w(b"\xED\xA0\x80")); // lead
        string.push_wtf8(w(b"\xED\xAF\xBF")); // lead
        assert_eq!(string.bytes, b"\xED\xA0\x80\xED\xAF\xBF");

        let mut string = Wtf8Buf::new();
        string.push_wtf8(w(b"\xED\xA0\x80")); // lead
        string.push_wtf8(w(b"\xEE\x80\x80")); // not surrogate
        assert_eq!(string.bytes, b"\xED\xA0\x80\xEE\x80\x80");

        let mut string = Wtf8Buf::new();
        string.push_wtf8(w(b"\xED\x9F\xBF")); // not surrogate
        string.push_wtf8(w(b"\xED\xB0\x80")); // trail
        assert_eq!(string.bytes, b"\xED\x9F\xBF\xED\xB0\x80");

        let mut string = Wtf8Buf::new();
        string.push_wtf8(w(b"a")); // not surrogate, < 3 bytes
        string.push_wtf8(w(b"\xED\xB0\x80")); // trail
        assert_eq!(string.bytes, b"\x61\xED\xB0\x80");

        let mut string = Wtf8Buf::new();
        string.push_wtf8(w(b"\xED\xB0\x80")); // trail
        assert_eq!(string.bytes, b"\xED\xB0\x80");
    }

    #[test]
    fn wtf8buf_truncate() {
        let mut string = Wtf8Buf::from_str("aé");
        string.truncate(1);
        assert_eq!(string.bytes, b"a");
    }

    #[test]
    #[should_panic]
    fn wtf8buf_truncate_fail_code_point_boundary() {
        let mut string = Wtf8Buf::from_str("aé");
        string.truncate(2);
    }

    #[test]
    #[should_panic]
    fn wtf8buf_truncate_fail_longer() {
        let mut string = Wtf8Buf::from_str("aé");
        string.truncate(4);
    }

    #[test]
    fn wtf8buf_into_string() {
        let mut string = Wtf8Buf::from_str("aé 💩");
        assert_eq!(string.clone().into_string(), Ok(String::from("aé 💩")));
        string.push(CodePoint::from_u32(0xd800).unwrap());
        assert_eq!(string.clone().into_string(), Err(string));
    }

    #[test]
    fn wtf8buf_into_string_lossy() {
        let mut string = Wtf8Buf::from_str("aé 💩");
        assert_eq!(string.clone().into_string_lossy(), String::from("aé 💩"));
        string.push(CodePoint::from_u32(0xd800).unwrap());
        assert_eq!(string.clone().into_string_lossy(), String::from("aé 💩�"));
    }

    #[test]
    fn wtf8buf_from_iterator() {
        fn f(values: &[u32]) -> Wtf8Buf {
            values
                .iter()
                .map(|&c| CodePoint::from_u32(c).unwrap())
                .collect::<Wtf8Buf>()
        }
        assert_eq!(
            f(&[0x61, 0xe9, 0x20, 0x1f4a9]).bytes,
            b"a\xC3\xA9 \xF0\x9F\x92\xA9"
        );

        assert_eq!(f(&[0xd83d, 0xdca9]).bytes, b"\xF0\x9F\x92\xA9"); // Magic!
        assert_eq!(
            f(&[0xd83d, 0x20, 0xdca9]).bytes,
            b"\xED\xA0\xBD \xED\xB2\xA9"
        );
        assert_eq!(f(&[0xd800, 0xdbff]).bytes, b"\xED\xA0\x80\xED\xAF\xBF");
        assert_eq!(f(&[0xd800, 0xe000]).bytes, b"\xED\xA0\x80\xEE\x80\x80");
        assert_eq!(f(&[0xd7ff, 0xdc00]).bytes, b"\xED\x9F\xBF\xED\xB0\x80");
        assert_eq!(f(&[0x61, 0xdc00]).bytes, b"\x61\xED\xB0\x80");
        assert_eq!(f(&[0xdc00]).bytes, b"\xED\xB0\x80");
    }

    #[test]
    fn wtf8buf_extend() {
        fn e(initial: &[u32], extended: &[u32]) -> Wtf8Buf {
            fn c(value: &u32) -> CodePoint {
                CodePoint::from_u32(*value).unwrap()
            }
            let mut string = initial.iter().map(c).collect::<Wtf8Buf>();
            string.extend(extended.iter().map(c));
            string
        }

        assert_eq!(
            e(&[0x61, 0xe9], &[0x20, 0x1f4a9]).bytes,
            b"a\xC3\xA9 \xF0\x9F\x92\xA9"
        );

        assert_eq!(e(&[0xd83d], &[0xdca9]).bytes, b"\xF0\x9F\x92\xA9"); // Magic!
        assert_eq!(
            e(&[0xd83d, 0x20], &[0xdca9]).bytes,
            b"\xED\xA0\xBD \xED\xB2\xA9"
        );
        assert_eq!(e(&[0xd800], &[0xdbff]).bytes, b"\xED\xA0\x80\xED\xAF\xBF");
        assert_eq!(e(&[0xd800], &[0xe000]).bytes, b"\xED\xA0\x80\xEE\x80\x80");
        assert_eq!(e(&[0xd7ff], &[0xdc00]).bytes, b"\xED\x9F\xBF\xED\xB0\x80");
        assert_eq!(e(&[0x61], &[0xdc00]).bytes, b"\x61\xED\xB0\x80");
        assert_eq!(e(&[], &[0xdc00]).bytes, b"\xED\xB0\x80");
    }

    #[test]
    fn wtf8buf_debug() {
        let mut string = Wtf8Buf::from_str("aé 💩");
        string.push(CodePoint::from_u32(0xd800).unwrap());
        assert_eq!(format!("{string:?}"), r#""aé 💩\u{D800}""#);
    }

    #[test]
    fn wtf8buf_as_slice() {
        assert_eq!(Wtf8Buf::from_str("aé"), Wtf8::from_str("aé"));
    }

    #[test]
    fn wtf8_debug() {
        let mut string = Wtf8Buf::from_str("aé 💩");
        string.push(CodePoint::from_u32(0xd800).unwrap());
        let string_ref = &*string;
        assert_eq!(format!("{string_ref:?}"), r#""aé 💩\u{D800}""#);
    }

    #[test]
    fn wtf8_from_str() {
        assert_eq!(&Wtf8::from_str("").bytes, b"");
        assert_eq!(
            &Wtf8::from_str("aé 💩").bytes,
            b"a\xC3\xA9 \xF0\x9F\x92\xA9"
        );
    }

    #[test]
    fn wtf8_as_bytes() {
        assert_eq!(Wtf8::from_str("").as_bytes(), b"");
        assert_eq!(
            Wtf8::from_str("aé 💩").as_bytes(),
            b"a\xC3\xA9 \xF0\x9F\x92\xA9"
        );
    }

    #[test]
    fn wtf8_from_bytes_unchecked() {
        assert_eq!(unsafe { &Wtf8::from_bytes_unchecked(b"").bytes }, b"");
        assert_eq!(
            unsafe { &Wtf8::from_bytes_unchecked(b"a\xC3\xA9 \xF0\x9F\x92\xA9").bytes },
            b"a\xC3\xA9 \xF0\x9F\x92\xA9"
        );
        assert_eq!(
            unsafe { Wtf8::from_bytes_unchecked(b"a\xC3\xA9 \xF0\x9F\x92\xA9") },
            Wtf8::from_str("aé 💩")
        )
    }

    #[test]
    fn wtf8_cow() {
        let s: Cow<Wtf8> = Cow::from(Wtf8::from_str("aé 💩"));
        assert!(matches!(s, Cow::Borrowed(_)));
        let owned: Wtf8Buf = s.into_owned();
        assert_eq!(owned, Wtf8Buf::from_str("aé 💩"));
    }

    #[test]
    fn wtf8_len() {
        assert_eq!(Wtf8::from_str("").len(), 0);
        assert_eq!(Wtf8::from_str("aé 💩").len(), 8);
    }

    #[test]
    fn wtf8_slice() {
        assert_eq!(&Wtf8::from_str("aé 💩").slice(1, 4).bytes, b"\xC3\xA9 ");
    }

    #[test]
    #[should_panic]
    fn wtf8_slice_not_code_point_boundary() {
        Wtf8::from_str("aé 💩").slice(2, 4);
    }

    #[test]
    fn wtf8_slice_from() {
        assert_eq!(
            &Wtf8::from_str("aé 💩").slice_from(1).bytes,
            b"\xC3\xA9 \xF0\x9F\x92\xA9"
        );
    }

    #[test]
    #[should_panic]
    fn wtf8_slice_from_not_code_point_boundary() {
        Wtf8::from_str("aé 💩").slice_from(2);
    }

    #[test]
    fn wtf8_slice_to() {
        assert_eq!(&Wtf8::from_str("aé 💩").slice_to(4).bytes, b"a\xC3\xA9 ");
    }

    #[test]
    #[should_panic]
    fn wtf8_slice_to_not_code_point_boundary() {
        Wtf8::from_str("aé 💩").slice_from(5);
    }

    #[test]
    fn wtf8_ascii_byte_at() {
        let slice = Wtf8::from_str("aé 💩");
        assert_eq!(slice.ascii_byte_at(0), b'a');
        assert_eq!(slice.ascii_byte_at(1), b'\xFF');
        assert_eq!(slice.ascii_byte_at(2), b'\xFF');
        assert_eq!(slice.ascii_byte_at(3), b' ');
        assert_eq!(slice.ascii_byte_at(4), b'\xFF');
    }

    #[test]
    fn wtf8_code_points() {
        fn c(value: u32) -> CodePoint {
            CodePoint::from_u32(value).unwrap()
        }
        fn cp(string: &Wtf8Buf) -> Vec<Option<char>> {
            string
                .code_points()
                .map(|c| c.to_char())
                .collect::<Vec<_>>()
        }
        let mut string = Wtf8Buf::from_str("é ");
        assert_eq!(cp(&string), vec![Some('é'), Some(' ')]);
        string.push(c(0xd83d));
        assert_eq!(cp(&string), vec![Some('é'), Some(' '), None]);
        string.push(c(0xdca9));
        assert_eq!(cp(&string), vec![Some('é'), Some(' '), Some('💩')]);
    }

    #[test]
    fn wtf8_as_str() {
        assert_eq!(Wtf8::from_str("").as_str(), Some(""));
        assert_eq!(Wtf8::from_str("aé 💩").as_str(), Some("aé 💩"));
        let mut string = Wtf8Buf::new();
        string.push(CodePoint::from_u32(0xd800).unwrap());
        assert_eq!(string.as_str(), None);
    }

    #[test]
    fn wtf8_to_string_lossy() {
        assert_eq!(Wtf8::from_str("").to_string_lossy(), Cow::Borrowed(""));
        assert_eq!(
            Wtf8::from_str("aé 💩").to_string_lossy(),
            Cow::Borrowed("aé 💩")
        );
        let mut string = Wtf8Buf::from_str("aé 💩");
        string.push(CodePoint::from_u32(0xd800).unwrap());
        assert_eq!(string.to_string_lossy(), {
            let o: Cow<str> = Cow::Owned(String::from("aé 💩�"));
            o
        });
    }

    #[test]
    fn wtf8_to_ill_formed_utf16() {
        let mut string = Wtf8Buf::from_str("aé ");
        string.push(CodePoint::from_u32(0xd83d).unwrap());
        string.push_char('💩');
        assert_eq!(
            string.to_ill_formed_utf16().collect::<Vec<_>>(),
            vec![0x61, 0xe9, 0x20, 0xd83d, 0xd83d, 0xdca9]
        );
    }

    #[test]
    fn wtf8buf_wtf8_from_bytes_valid() {
        // Valid UTF-8
        assert!(Wtf8Buf::from_bytes(b"hello".to_vec()).is_ok());
        assert!(Wtf8Buf::from_bytes(b"a\xC3\xA9 \xF0\x9F\x92\xA9".to_vec()).is_ok());
        assert!(Wtf8::from_bytes(b"hello").is_ok());
        assert!(Wtf8::from_bytes(b"a\xC3\xA9 \xF0\x9F\x92\xA9").is_ok());

        // Valid WTF-8 with unpaired surrogates
        assert!(Wtf8Buf::from_bytes(b"\xED\xA0\x80".to_vec()).is_ok()); // lead surrogate
        assert!(Wtf8Buf::from_bytes(b"\xED\xB0\x80".to_vec()).is_ok()); // trail surrogate
        assert!(Wtf8Buf::from_bytes(b"a\xED\xA0\xBD".to_vec()).is_ok()); // text + lead
        assert!(Wtf8Buf::from_bytes(b"\xED\xB2\xA9z".to_vec()).is_ok()); // trail + text
        assert!(Wtf8Buf::from_bytes(b"\xED\xB2\xA9\xED\xA0\xBD".to_vec()).is_ok());
        // trail + lead
    }

    #[test]
    fn wtf8buf_wtf8_from_bytes_invalid() {
        // Invalid: surrogate pair encoded as two 3-byte sequences
        assert!(Wtf8Buf::from_bytes(b"\xED\xA0\x80\xED\xB0\x80".to_vec()).is_err());
        assert!(Wtf8Buf::from_bytes(b"\xED\xA0\xBD\xED\xB2\xA9".to_vec()).is_err());
        assert!(Wtf8::from_bytes(b"\xED\xA0\x80\xED\xB0\x80").is_err());
        assert!(Wtf8::from_bytes(b"\xED\xA0\xBD\xED\xB2\xA9").is_err());

        // Invalid UTF-8
        assert!(Wtf8Buf::from_bytes(vec![0xff]).is_err());
        assert!(Wtf8Buf::from_bytes(vec![0xc0, 0x80]).is_err()); // overlong
        assert!(Wtf8Buf::from_bytes(vec![0xed, 0xa0]).is_err()); // truncated lead surrogate
        assert!(Wtf8Buf::from_bytes(vec![0xf4, 0x90, 0x80, 0x80]).is_err()); // > U+10FFFF

        // Verify we can recover the original bytes on failure
        let original = vec![0xff, 0xfe];
        let result = Wtf8Buf::from_bytes(original.clone());
        assert_eq!(result.unwrap_err(), original);

        let result = Wtf8::from_bytes(&original);
        assert_eq!(result.unwrap_err(), original);
    }
}
