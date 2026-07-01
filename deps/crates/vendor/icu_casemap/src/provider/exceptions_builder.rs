// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::provider::exception_helpers::{
    ExceptionBits, ExceptionBitsULE, ExceptionSlot, SlotPresence,
};
use crate::provider::exceptions::{CaseMapExceptions, DecodedException};
use alloc::borrow::Cow;
use alloc::collections::BTreeMap;
use alloc::string::String;
use alloc::vec::Vec;
use icu_provider::DataError;
use zerovec::ule::{AsULE, ULE};

/// The header for exception types as found in ICU4C data. See [`ExceptionHeaderULE`]
/// for the wire format
#[derive(Copy, Clone, PartialEq, Eq)]
pub struct ExceptionHeader {
    /// The various slots that are present, masked by [`ExceptionSlot`]
    ///
    /// We still store this as a bitmask since it's more convenient to access as one
    pub slot_presence: SlotPresence,
    pub bits: ExceptionBits,
}

impl ExceptionHeader {
    /// Construct from an ICU4C-format u16.
    pub(crate) fn from_integer(int: u16) -> Self {
        let slot_presence =
            SlotPresence(u8::try_from(int & ExceptionHeaderULE::SLOTS_MASK).unwrap_or(0));
        let bits = ExceptionBits::from_integer(
            u8::try_from(int >> ExceptionHeaderULE::BITS_SHIFT).unwrap_or(0),
        );
        Self {
            slot_presence,
            bits,
        }
    }

    // Returns true if the given slot exists for this exception
    pub(crate) fn has_slot(self, slot: ExceptionSlot) -> bool {
        self.slot_presence.has_slot(slot)
    }
}

/// Packed exception header (format from icu4c, documented in casepropsbuilder.cpp)
///
/// ```text
///       Bits:
///         0..7  Flag bits indicating which optional slots are present (if any):
///               0: Lowercase mapping (code point)
///               1: Case folding (code point)
///               2: Uppercase mapping (code point)
///               3: Titlecase mapping (code point)
///               4: Delta to simple case mapping (code point) (sign stored separately)
///               5: RESERVED
///               6: Closure mappings (string; see below)
///               7: Full mappings (strings; see below)
///            8  Double-width slots. If set, then each optional slot is stored as two
///               elements of the array (high and low halves of 32-bit values) instead of
///               a single element.
///            9  Has no simple case folding, even if there is a simple lowercase mapping
///           10  The value in the delta slot is negative
///           11  Is case-sensitive (not exposed)
///       12..13  Dot type
///           14  Has conditional special casing
///           15  Has conditional case folding
/// ```
///
/// In this struct the RESERVED bit is still allowed to be set, and it will produce a different
/// exception header, but it will not have any other effects.
#[derive(Copy, Clone, PartialEq, Eq, ULE)]
#[repr(C, packed)]
pub struct ExceptionHeaderULE {
    slot_presence: SlotPresence,
    bits: ExceptionBitsULE,
}

impl ExceptionHeaderULE {
    const SLOTS_MASK: u16 = 0xff;
    const BITS_SHIFT: u16 = 8;
}

impl AsULE for ExceptionHeader {
    type ULE = ExceptionHeaderULE;
    fn from_unaligned(u: ExceptionHeaderULE) -> Self {
        Self {
            slot_presence: u.slot_presence,
            bits: ExceptionBits::from_integer(u.bits.0),
        }
    }

    fn to_unaligned(self) -> ExceptionHeaderULE {
        ExceptionHeaderULE {
            slot_presence: self.slot_presence,
            bits: ExceptionBitsULE(self.bits.to_integer()),
        }
    }
}
// CaseMapExceptionsBuilder consumes the exceptions data produced by
// casepropsbuilder.cpp in ICU4C. It generates an instance of CaseMapExceptions. The
// primary difference is that the ICU4C representation stores full mapping and closure
// strings inline in the data, while CaseMapExceptions uses a side table. As a result,
// the starting index of each exception in the resulting CaseMapExceptions may have
// changed, so we also produce a map from old indices to new indices that will be used to
// update the data stored in the code point trie.
pub struct CaseMapExceptionsBuilder<'a> {
    raw_data: &'a [u16],
    raw_data_idx: usize,
    double_slots: bool,
}

impl<'a> CaseMapExceptionsBuilder<'a> {
    const MAPPINGS_ALL_LENGTHS_MASK: u32 = 0xffff;
    const FULL_MAPPINGS_LENGTH_MASK: u32 = 0xf;
    const FULL_MAPPINGS_LENGTH_SHIFT: u32 = 4;

    const CLOSURE_MAX_LENGTH: u32 = 0xf;

    pub fn new(raw_data: &'a [u16]) -> Self {
        Self {
            raw_data,
            raw_data_idx: 0,
            double_slots: false,
        }
    }

    fn done(&self) -> bool {
        self.raw_data_idx >= self.raw_data.len()
    }
    fn read_raw(&mut self) -> Result<u16, DataError> {
        let result = self
            .raw_data
            .get(self.raw_data_idx)
            .ok_or(DataError::custom("Incomplete exception data"))?;
        self.raw_data_idx += 1;
        Ok(*result)
    }

    fn read_slot(&mut self) -> Result<u32, DataError> {
        if self.double_slots {
            let hi = self.read_raw()? as u32;
            let lo = self.read_raw()? as u32;
            Ok((hi << 16) | lo)
        } else {
            Ok(self.read_raw()? as u32)
        }
    }

    // After reading a string out of the raw data, advance raw_data_idx.
    fn skip_string(&mut self, s: &str) {
        for c in s.chars() {
            self.raw_data_idx += c.len_utf16();
        }
    }

    pub(crate) fn build(
        mut self,
    ) -> Result<(CaseMapExceptions<'static>, BTreeMap<u16, u16>), DataError> {
        let mut exceptions = Vec::new();
        let mut idx_map = BTreeMap::new();
        // The format of the raw data from ICU4C is the same as the format described in
        // exceptions.rs, with the exception of full mapping and closure strings. The
        // header and non-string slots can be copied over without modification. For string
        // slots, we read the length information from the ICU4C slot (described below),
        // read the strings, add the strings to the CaseMapExceptions string table,
        // and write an updated slot value containing the index of the string in the
        // table. In the case of full mappings, we store the index of the lowercase
        // mapping; the remaining mappings are stored at sequential indices.
        //
        // Full mappings: If there is at least one full (string) case mapping, then the
        // lengths of the mappings are encoded as nibbles in the full mappings slot:
        //     Bits:
        //        0..4   Length of lowercase string
        //        5..7   Length of case folding string
        //        8..11  Length of uppercase string
        //        12..15 Length of titlecase string
        // Mappings that do not exist have length 0. The strings themselves are stored in
        // the above order immediately following the last optional slot, encoded as UTF16.
        //
        // Case closure: If the case closure for a code point includes code points that
        // are not included in the simple or full mappings, then bits 0..3 of the closure
        // mappings slot will contain the number of codepoints in the closure string.
        // (Other bits are reserved.) The closure string itself is encoded as UTF16 and
        // stored following the full mappings data (if it exists) or the final optional
        // slot.
        while !self.done() {
            let old_idx = self.raw_data_idx as u16;

            let mut exception = DecodedException::default();

            // Copy header.
            let header = ExceptionHeader::from_integer(self.read_raw()?);
            self.double_slots = header.bits.double_width_slots;

            // Copy unmodified slots.
            for (slot, output) in [
                (ExceptionSlot::Lower, &mut exception.lowercase),
                (ExceptionSlot::Fold, &mut exception.casefold),
                (ExceptionSlot::Upper, &mut exception.uppercase),
                (ExceptionSlot::Title, &mut exception.titlecase),
            ] {
                if header.has_slot(slot) {
                    let value = self.read_slot()?;
                    if let Ok(ch) = char::try_from(value) {
                        *output = Some(ch)
                    } else {
                        return Err(DataError::custom(
                            "Found non-char value in casemapping exceptions data",
                        ));
                    }
                }
            }
            if header.has_slot(ExceptionSlot::Delta) {
                let delta = self.read_slot()?;

                exception.simple_case_delta = Some(delta)
            }

            // Read the closure and full mappings slots, if they exist.
            let closure_length = if header.has_slot(ExceptionSlot::Closure) {
                Some((self.read_slot()? & Self::CLOSURE_MAX_LENGTH) as usize)
            } else {
                None
            };
            let mappings_lengths = if header.has_slot(ExceptionSlot::FullMappings) {
                Some(self.read_slot()? & Self::MAPPINGS_ALL_LENGTHS_MASK)
            } else {
                None
            };

            // Copy the full mappings strings into the strings table, if they exist.
            if let Some(mut lengths) = mappings_lengths {
                let mut arr: [Cow<_>; 4] = Default::default();
                for mapping in &mut arr {
                    let len = lengths & Self::FULL_MAPPINGS_LENGTH_MASK;
                    lengths >>= Self::FULL_MAPPINGS_LENGTH_SHIFT;

                    let start = self.raw_data_idx;
                    let end = start + len as usize;
                    let slice = &self
                        .raw_data
                        .get(start..end)
                        .ok_or(DataError::custom("Incomplete string data"))?;
                    let string = char::decode_utf16(slice.iter().copied())
                        .collect::<Result<String, _>>()
                        .map_err(|_| DataError::custom("Found non-utf16 exceptions data"))?;
                    self.skip_string(&string);
                    *mapping = string.into()
                }
                exception.full = Some(arr)
            }

            // Copy the closure string into the strings table, if it exists.
            if let Some(len) = closure_length {
                let start = self.raw_data_idx;
                let slice = &self
                    .raw_data
                    .get(start..)
                    .ok_or(DataError::custom("Incomplete string data"))?;
                let string = char::decode_utf16(slice.iter().copied())
                    .take(len)
                    .collect::<Result<String, _>>()
                    .map_err(|_| DataError::custom("Found non-utf16 exceptions data"))?;
                self.skip_string(&string);
                exception.closure = Some(string.into())
            }

            exception.bits = header.bits;
            // unused bits in ICU4X
            exception.bits.double_width_slots = false;

            let new_exception_index = if let Ok(idx) = u16::try_from(exceptions.len()) {
                idx
            } else {
                return Err(DataError::custom("More than u16 exceptions"));
            };
            idx_map.insert(old_idx, new_exception_index);
            exceptions.push(exception.encode());
        }

        Ok((
            CaseMapExceptions {
                exceptions: (&exceptions).into(),
            },
            idx_map,
        ))
    }
}
