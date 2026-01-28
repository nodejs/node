// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use super::{read_u16, BinaryDeserializerError, CharsetFamily, Endianness, FormatVersion};

/// The `BinHeader` struct represents the in-memory layout of a binary resource
/// bundle header.
#[repr(C)]
pub(super) struct BinHeader {
    /// The size of the header in bytes, padded such that it is divisible by 16.
    pub size: u16,

    /// A magic word. See [`MAGIC_WORD`].
    pub magic: [u8; 2],

    /// Information on the representation of data in the binary bundle.
    pub repr_info: BinReprInfo,
}

/// The `BinReprInfo` struct represents the in-memory layout of a binary
/// resource bundle's representation specifiers. These data describe the
/// parameters necessary for correctly reading a bundle file.
#[repr(C)]
pub(super) struct BinReprInfo {
    /// The size of the representation info in bytes.
    pub size: u16,

    /// Reserved. Always 0.
    pub reserved_word: u16,

    /// The endianness of values in the file. `0` for little endian and `1` for
    /// big endian.
    pub endianness: Endianness,

    /// The character set family in which key strings are represented.
    pub charset_family: CharsetFamily,

    /// The size of a character in a string resource in bytes. May be 1, 2, or
    /// 4.
    pub size_of_char: u8,

    /// Reserved. Always 0.
    pub reserved_byte: u8,

    /// The data format identifier. Always `b"ResB"`.
    pub data_format: [u8; 4],

    /// The format version used for laying out the bundle.
    pub format_version: FormatVersion,

    /// The version of the data in the file. This is `[1, 4, 0, 0]` in all known
    /// versions of ICU4C's `genrb`.
    pub data_version: [u8; 4],
}

impl TryFrom<&[u8]> for BinHeader {
    type Error = BinaryDeserializerError;

    // We can safely index in this function as we first guarantee that the input
    // is sufficiently large.
    #[expect(clippy::indexing_slicing)]
    fn try_from(value: &[u8]) -> Result<Self, Self::Error> {
        if value.len() < core::mem::size_of::<BinHeader>() {
            return Err(BinaryDeserializerError::invalid_data(
                "input is too short to contain file header",
            ));
        }

        let (size, value) = read_u16(value)?;

        // If the magic word does not match, we are not reading a valid resource
        // bundle and will not be able to proceed.
        let (magic, value) = ([value[0], value[1]], &value[2..]);
        match magic {
            [0xda, 0x27] => (),
            _ => {
                return Err(BinaryDeserializerError::invalid_data(
                    "magic word does not match",
                ))
            }
        }

        let repr_info = BinReprInfo::try_from(value)?;

        if (size as usize) < core::mem::size_of::<BinHeader>() {
            return Err(BinaryDeserializerError::invalid_data(
                "header size specified in file is smaller than expected",
            ));
        }

        Ok(BinHeader {
            size,
            magic,
            repr_info,
        })
    }
}

impl TryFrom<&[u8]> for BinReprInfo {
    type Error = BinaryDeserializerError;

    // We can safely index in this function as we first guarantee that the input
    // is sufficiently large.
    #[expect(clippy::indexing_slicing)]
    fn try_from(value: &[u8]) -> Result<Self, Self::Error> {
        if value.len() < core::mem::size_of::<BinReprInfo>() {
            return Err(BinaryDeserializerError::invalid_data(
                "input is too short to contain representation info",
            ));
        }

        let (size, value) = read_u16(value)?;
        let (reserved_word, value) = read_u16(value)?;

        // While the consumer is responsible for verifying acceptability of most
        // contents of the repr info, we explicitly depend on little endian data
        // in order to ensure compatibility with `zerovec`.
        let (endianness, value) = (Endianness::try_from(value[0])?, &value[1..]);
        if endianness != Endianness::Little {
            return Err(BinaryDeserializerError::unsupported_format(
                "big-endian bundles are not supported",
            ));
        }

        let (charset_family, value) = (CharsetFamily::try_from(value[0])?, &value[1..]);
        let (size_of_char, value) = (value[0], &value[1..]);
        let (reserved_byte, value) = (value[0], &value[1..]);

        // A mismatch in the data format indicates that we are not reading a
        // valid resource bundle.
        let (data_format, value) = ([value[0], value[1], value[2], value[3]], &value[4..]);
        if &data_format != b"ResB" {
            return Err(BinaryDeserializerError::invalid_data(
                "unexpected data format value",
            ));
        }

        let (format_version, value) = (FormatVersion::try_from(&value[..4])?, &value[4..]);
        let (data_version, _) = ([value[0], value[1], value[2], value[3]], &value[4..]);

        Ok(BinReprInfo {
            size,
            reserved_word,
            endianness,
            charset_family,
            size_of_char,
            reserved_byte,
            data_format,
            format_version,
            data_version,
        })
    }
}
