// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! This module contains the supported encoding for `ixdtf` parsing.

use crate::{ParseError, ParserResult};

mod private {
    pub trait Sealed {}
}

/// A trait for defining various supported encodings
/// and implementing functionality that is encoding
/// sensitive / specific.
pub trait EncodingType: private::Sealed {
    /// The code unit for the current encoding.
    type CodeUnit: PartialEq + core::fmt::Debug + Clone;

    /// Get a slice from the underlying source using for start..end
    #[doc(hidden)]
    fn slice(source: &[Self::CodeUnit], start: usize, end: usize) -> Option<&[Self::CodeUnit]>;

    /// Retrieve the provided code unit index and returns the value as an ASCII byte
    /// or None if the value is not ASCII representable.
    #[doc(hidden)]
    fn get_ascii(source: &[Self::CodeUnit], index: usize) -> ParserResult<Option<u8>>;

    /// Checks for the known calendar annotation key `u-ca`.
    #[doc(hidden)]
    fn check_calendar_key(key: &[Self::CodeUnit]) -> bool;
}

/// A marker type that signals a parser should parse the source as UTF-16 bytes.
#[derive(Debug, PartialEq, Clone)]
#[allow(clippy::exhaustive_structs)] // ZST Marker trait, no fields should be added
pub struct Utf16;

impl private::Sealed for Utf16 {}

impl EncodingType for Utf16 {
    type CodeUnit = u16;
    fn slice(source: &[Self::CodeUnit], start: usize, end: usize) -> Option<&[Self::CodeUnit]> {
        source.get(start..end)
    }

    fn get_ascii(source: &[Self::CodeUnit], index: usize) -> ParserResult<Option<u8>> {
        source.get(index).copied().map(to_ascii_byte).transpose()
    }

    fn check_calendar_key(key: &[Self::CodeUnit]) -> bool {
        key == [0x75, 0x2d, 0x63, 0x61]
    }
}

#[inline]
fn to_ascii_byte(b: u16) -> ParserResult<u8> {
    if !(0x01..0x7F).contains(&b) {
        return Err(ParseError::NonAsciiCodePoint);
    }
    Ok(b as u8)
}

/// A marker type that signals a parser should parse the source as UTF-8 bytes.
#[derive(Debug, PartialEq, Clone)]
#[allow(clippy::exhaustive_structs)] // ZST Marker trait, no fields should be added.
pub struct Utf8;

impl private::Sealed for Utf8 {}

impl EncodingType for Utf8 {
    type CodeUnit = u8;

    fn slice(source: &[Self::CodeUnit], start: usize, end: usize) -> Option<&[Self::CodeUnit]> {
        source.get(start..end)
    }

    fn get_ascii(source: &[Self::CodeUnit], index: usize) -> ParserResult<Option<u8>> {
        Ok(source.get(index).copied())
    }

    fn check_calendar_key(key: &[Self::CodeUnit]) -> bool {
        key == "u-ca".as_bytes()
    }
}
