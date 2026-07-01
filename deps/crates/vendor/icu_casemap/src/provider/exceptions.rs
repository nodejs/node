// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! This is the main module pertaining to casemapping exceptions.
//!
//! A single exception is represented by the [`Exception`] type and its ULE equivalent.
//!
//! The storage format is complicated (and documented on [`Exception`]), but the data format is
//! represented equally by [`DecodedException`], which is more human-readable.
use icu_provider::prelude::*;

use super::data::MappingKind;
use super::exception_helpers::{ExceptionBits, ExceptionSlot, SlotPresence};
use crate::set::ClosureSink;
use alloc::borrow::Cow;
use alloc::string::String;
use core::fmt;
#[cfg(any(feature = "serde", feature = "datagen"))]
use core::ops::Range;
use core::ptr;
use zerovec::ule::AsULE;
use zerovec::VarZeroVec;

const SURROGATES_START: u32 = 0xD800;
const SURROGATES_LEN: u32 = 0xDFFF - SURROGATES_START + 1;

/// This represents case mapping exceptions that can't be represented as a delta applied to
/// the original code point. The codepoint
/// trie in [`CaseMap`](super::CaseMap) stores indices into this [`VarZeroVec`].
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_casemap::provider::exceptions))]
#[derive(Debug, Eq, PartialEq, Clone, yoke::Yokeable, zerofrom::ZeroFrom)]
pub struct CaseMapExceptions<'data> {
    #[cfg_attr(feature = "serde", serde(borrow))]
    /// The list of exceptions
    pub exceptions: VarZeroVec<'data, ExceptionULE>,
}

impl CaseMapExceptions<'_> {
    /// Obtain the exception at index `idx`. Will
    /// return a default value if not present (GIGO behavior),
    /// as these indices should come from a paired [`CaseMap`](super::CaseMap) object
    ///
    /// Will also panic in debug mode
    pub fn get(&self, idx: u16) -> &ExceptionULE {
        let exception = self.exceptions.get(idx.into());
        debug_assert!(exception.is_some());

        exception.unwrap_or(ExceptionULE::EMPTY_EXCEPTION)
    }

    #[cfg(any(feature = "serde", feature = "datagen"))]
    pub(crate) fn validate(&self) -> Result<Range<u16>, &'static str> {
        for exception in self.exceptions.iter() {
            exception.validate()?;
        }
        u16::try_from(self.exceptions.len())
            .map_err(|_| "Too many exceptions")
            .map(|l| 0..l)
    }
}
/// A type representing the wire format of `Exception`. The data contained is
/// equivalently represented by [`DecodedException`].
///
/// This type is itself not used that much, most of its relevant methods live
/// on [`ExceptionULE`].
///
/// The `bits` contain supplementary data, whereas
/// `slot_presence` marks te presence of various extra data
/// in the `data` field.
///
/// The `data` field is not validated to contain all of this data,
/// this type will have GIGO behavior when constructed with invalid `data`.
///
/// The format of `data` is documented on the field
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[zerovec::make_varule(ExceptionULE)]
#[derive(PartialEq, Eq, Clone, Default, Debug)]
#[zerovec::skip_derive(Ord)]
#[cfg_attr(
    feature = "serde",
    derive(serde::Deserialize),
    zerovec::derive(Deserialize)
)]
#[cfg_attr(
    feature = "datagen",
    derive(serde::Serialize),
    zerovec::derive(Serialize)
)]
pub struct Exception<'a> {
    /// The various bit based exception data associated with this.
    ///
    /// Format: Just a u8 of bitflags, some flags unused. See [`ExceptionBits`] and its ULE type for more.
    pub bits: ExceptionBits,
    /// Which slots are present in `data`.
    ///
    /// Format: a u8 of bitflags
    pub slot_presence: SlotPresence,
    /// Format : `[char slots] [optional closure length] [ closure slot ] [ full mappings data ]`
    ///
    /// For each set [`SlotPresence`] bit, except for the two stringy slots (Closure/FullMapping),
    /// this will have one entry in the string, packed together.
    ///
    /// Note that the `simple_case` delta is stored as a [`u32`] normalized to a `char`, where [`u32`]s
    /// which are from or beyond the surrogate range `0xD800-0xDFFF` are stored as chars
    /// starting from `0xE000`. The sign is stored in `bits.negative_delta`.
    ///
    /// If both Closure/FullMapping are present, the next char will be the length of the closure slot,
    /// bisecting the rest of the data.
    /// If only one is present, the rest of the data represents that slot.
    ///
    /// The closure slot simply represents one string. The full-mappings slot represents four strings,
    /// packed in a way similar to [`VarZeroVec`], in the following format:
    /// `i1 i2 i3 [ str0 ] [ str1 ] [ str2 ] [ str3 ]`
    ///
    /// where `i1 i2 i3` are the indices of the relevant mappings string. The strings are stored in
    /// the order corresponding to the `MappingKind` enum.
    pub data: Cow<'a, str>,
}

impl ExceptionULE {
    const EMPTY_EXCEPTION: &Self = {
        static EMPTY_BYTES: &[u8] = &[0, 0];
        // Safety:
        // ExceptionULE is a packed DST with `(u8, u8, unsized)` fields. All bit patterns are valid for the two u8s
        //
        // An "empty" one can be constructed from a slice of two u8s
        unsafe {
            let slice: *const [u8] = ptr::slice_from_raw_parts(EMPTY_BYTES.as_ptr(), 0);
            &*(slice as *const Self)
        }
    };

    pub(crate) fn has_slot(&self, slot: ExceptionSlot) -> bool {
        self.slot_presence.has_slot(slot)
    }
    /// Obtain a `char` slot, if occupied. If `slot` represents a string slot,
    /// will return `None`
    pub(crate) fn get_char_slot(&self, slot: ExceptionSlot) -> Option<char> {
        if slot >= ExceptionSlot::STRING_SLOTS_START {
            return None;
        }
        let bit = 1 << (slot as u8);
        // check if slot is occupied
        if self.slot_presence.0 & bit == 0 {
            return None;
        }

        let previous_slot_mask = bit - 1;
        let previous_slots = self.slot_presence.0 & previous_slot_mask;
        let slot_num = previous_slots.count_ones() as usize;
        self.data.chars().nth(slot_num)
    }

    /// Get the `simple_case` delta (i.e. the `delta` slot), given the character
    /// this data belongs to.
    ///
    /// Normalizes the delta from char-format to u32 format
    ///
    /// Does *not* handle the sign of the delta; see `self.bits.negative_delta`
    fn get_simple_case_delta(&self) -> Option<u32> {
        let delta_ch = self.get_char_slot(ExceptionSlot::Delta)?;
        let mut delta = u32::from(delta_ch);
        // We "fill in" the surrogates range by offsetting deltas greater than it
        if delta >= SURROGATES_START {
            delta -= SURROGATES_LEN;
        }
        Some(delta)
    }

    /// Get the `simple_case` value (i.e. the `delta` slot), given the character
    /// this data belongs to.
    ///
    /// The data is stored as a delta so the character must be provided.
    ///
    /// The data cannot be stored directly as a character because the trie is more
    /// compact with adjacent characters sharing deltas.
    pub(crate) fn get_simple_case_slot_for(&self, ch: char) -> Option<char> {
        let delta = self.get_simple_case_delta()?;
        let mut delta = i32::try_from(delta).ok()?;
        if self.bits.negative_delta() {
            delta = -delta;
        }

        let new_ch = i32::try_from(u32::from(ch)).ok()? + delta;

        char::try_from(u32::try_from(new_ch).ok()?).ok()
    }

    /// Returns *all* the data in the closure/full slots, including length metadata
    fn get_stringy_data(&self) -> Option<&str> {
        const CHAR_MASK: u8 = (1 << ExceptionSlot::STRING_SLOTS_START as u8) - 1;
        let char_slot_count = (self.slot_presence.0 & CHAR_MASK).count_ones() as usize;
        let mut chars = self.data.chars();
        for _ in 0..char_slot_count {
            let res = chars.next();
            res?;
        }
        Some(chars.as_str())
    }

    /// Returns a single stringy slot, either [`ExceptionSlot::Closure`]
    /// or [`ExceptionSlot::FullMappings`].
    fn get_stringy_slot(&self, slot: ExceptionSlot) -> Option<&str> {
        debug_assert!(slot == ExceptionSlot::Closure || slot == ExceptionSlot::FullMappings);
        let other_slot = if slot == ExceptionSlot::Closure {
            ExceptionSlot::FullMappings
        } else {
            ExceptionSlot::Closure
        };
        if !self.slot_presence.has_slot(slot) {
            return None;
        }
        let stringy_data = self.get_stringy_data()?;

        if self.slot_presence.has_slot(other_slot) {
            // both stringy slots are used, we need a length
            let mut chars = stringy_data.chars();
            // GIGO: to have two strings there must be a length, if not present return None
            let length_char = chars.next()?;

            let length = usize::try_from(u32::from(length_char)).unwrap_or(0);
            // The length indexes into the string after the first char
            let remaining_slice = chars.as_str();
            // GIGO: will return none if there wasn't enough space in this slot
            if slot == ExceptionSlot::Closure {
                remaining_slice.get(0..length)
            } else {
                remaining_slice.get(length..)
            }
        } else {
            // only a single stringy slot, there is no length stored
            Some(stringy_data)
        }
    }

    /// Get the data behind the `closure` slot
    pub(crate) fn get_closure_slot(&self) -> Option<&str> {
        self.get_stringy_slot(ExceptionSlot::Closure)
    }

    /// Get all the slot data for the [`FullMappings`] slot
    ///
    /// This needs to be further segmented into four based on length metadata
    fn get_fullmappings_slot_data(&self) -> Option<&str> {
        self.get_stringy_slot(ExceptionSlot::FullMappings)
    }

    /// Get a specific [`FullMappings`] slot value
    pub(crate) fn get_fullmappings_slot_for_kind(&self, kind: MappingKind) -> Option<&str> {
        let data = self.get_fullmappings_slot_data()?;

        let mut chars = data.chars();
        // GIGO: must have three index strings, else return None
        let i1 = usize::try_from(u32::from(chars.next()?)).ok()?;
        let i2 = usize::try_from(u32::from(chars.next()?)).ok()?;
        let i3 = usize::try_from(u32::from(chars.next()?)).ok()?;
        let remaining_slice = chars.as_str();
        // GIGO: if the indices are wrong, return None
        match kind {
            MappingKind::Lower => remaining_slice.get(..i1),
            MappingKind::Fold => remaining_slice.get(i1..i2),
            MappingKind::Upper => remaining_slice.get(i2..i3),
            MappingKind::Title => remaining_slice.get(i3..),
        }
    }

    // convenience function that lets us use the ? operator
    fn get_all_fullmapping_slots(&self) -> Option<[Cow<'_, str>; 4]> {
        Some([
            self.get_fullmappings_slot_for_kind(MappingKind::Lower)?
                .into(),
            self.get_fullmappings_slot_for_kind(MappingKind::Fold)?
                .into(),
            self.get_fullmappings_slot_for_kind(MappingKind::Upper)?
                .into(),
            self.get_fullmappings_slot_for_kind(MappingKind::Title)?
                .into(),
        ])
    }

    // Given a mapping kind, returns the character for that kind, if it exists. Fold falls
    // back to Lower; Title falls back to Upper.
    #[inline]
    pub(crate) fn slot_char_for_kind(&self, kind: MappingKind) -> Option<char> {
        match kind {
            MappingKind::Lower | MappingKind::Upper => self.get_char_slot(kind.into()),
            MappingKind::Fold => self
                .get_char_slot(ExceptionSlot::Fold)
                .or_else(|| self.get_char_slot(ExceptionSlot::Lower)),
            MappingKind::Title => self
                .get_char_slot(ExceptionSlot::Title)
                .or_else(|| self.get_char_slot(ExceptionSlot::Upper)),
        }
    }

    pub(crate) fn add_full_and_closure_mappings<S: ClosureSink>(&self, set: &mut S) {
        if let Some(full) = self.get_fullmappings_slot_for_kind(MappingKind::Fold) {
            if !full.is_empty() {
                set.add_string(full);
            }
        };
        if let Some(closure) = self.get_closure_slot() {
            for c in closure.chars() {
                set.add_char(c);
            }
        };
    }

    /// Extract all the data out into a structured form
    ///
    /// Useful for serialization and debugging
    pub fn decode(&self) -> DecodedException<'_> {
        // Potential future optimization: This can
        // directly access each bit one after the other and iterate the string
        // which avoids recomputing slot offsets over and over again.
        //
        // If we're doing so we may wish to retain this older impl so that we can still roundtrip test
        let bits = self.bits;
        let lowercase = self.get_char_slot(ExceptionSlot::Lower);
        let casefold = self.get_char_slot(ExceptionSlot::Fold);
        let uppercase = self.get_char_slot(ExceptionSlot::Upper);
        let titlecase = self.get_char_slot(ExceptionSlot::Title);
        let simple_case_delta = self.get_simple_case_delta();
        let closure = self.get_closure_slot().map(Into::into);
        let full = self.get_all_fullmapping_slots();

        DecodedException {
            bits: ExceptionBits::from_unaligned(bits),
            lowercase,
            casefold,
            uppercase,
            titlecase,
            simple_case_delta,
            closure,
            full,
        }
    }

    #[cfg(any(feature = "serde", feature = "datagen"))]
    pub(crate) fn validate(&self) -> Result<(), &'static str> {
        // check that ICU4C specific fields are not set
        // check that there is enough space for all the offsets
        if self.bits.double_width_slots() {
            return Err("double-width-slots should not be used in ICU4C");
        }

        // just run all of the slot getters at once and then check
        let decoded = self.decode();

        for (slot, decoded_slot) in [
            (ExceptionSlot::Lower, &decoded.lowercase),
            (ExceptionSlot::Fold, &decoded.casefold),
            (ExceptionSlot::Upper, &decoded.uppercase),
            (ExceptionSlot::Title, &decoded.titlecase),
        ] {
            if self.has_slot(slot) && decoded_slot.is_none() {
                // decoding hit GIGO behavior, oops!
                return Err("Slot decoding failed");
            }
        }
        if self.has_slot(ExceptionSlot::Delta) && decoded.simple_case_delta.is_none() {
            // decoding hit GIGO behavior, oops!
            return Err("Slot decoding failed");
        }

        if self.has_slot(ExceptionSlot::Closure) && decoded.closure.is_none() {
            return Err("Slot decoding failed");
        }

        if self.has_slot(ExceptionSlot::FullMappings) {
            if decoded.full.is_some() {
                let data = self
                    .get_fullmappings_slot_data()
                    .ok_or("fullmappings slot doesn't parse")?;
                let mut chars = data.chars();
                let i1 = u32::from(chars.next().ok_or("fullmappings string too small")?);
                let i2 = u32::from(chars.next().ok_or("fullmappings string too small")?);
                let i3 = u32::from(chars.next().ok_or("fullmappings string too small")?);

                if i2 < i1 || i3 < i2 {
                    return Err("fullmappings string contains non-sequential indices");
                }
                let rest = chars.as_str();
                let len = u32::try_from(rest.len()).map_err(|_| "len too large for u32")?;

                if i1 > len || i2 > len || i3 > len {
                    return Err("fullmappings string contains out-of-bounds indices");
                }
            } else {
                return Err("Slot decoding failed");
            }
        }

        Ok(())
    }
}

impl fmt::Debug for ExceptionULE {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.decode().fmt(f)
    }
}

/// A decoded [`Exception`] type, with all of the data parsed out into
/// separate fields.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize))]
#[derive(Debug, Clone, PartialEq, Eq, Default)]
pub struct DecodedException<'a> {
    /// The various bit-based data associated with this exception
    pub bits: ExceptionBits,
    /// Lowercase mapping
    pub lowercase: Option<char>,
    /// Case folding
    pub casefold: Option<char>,
    /// Uppercase mapping
    pub uppercase: Option<char>,
    /// Titlecase mapping
    pub titlecase: Option<char>,
    /// The simple casefold delta. Its sign is stored in `bits.negative_delta`
    pub simple_case_delta: Option<u32>,
    /// Closure mappings
    pub closure: Option<Cow<'a, str>>,
    /// The four full-mappings strings, indexed by `MappingKind` `u8` value
    pub full: Option<[Cow<'a, str>; 4]>,
}

impl DecodedException<'_> {
    /// Convert to a wire-format encodeable (VarULE-encodeable) [`Exception`]
    pub fn encode(&self) -> Exception<'static> {
        let bits = self.bits;
        let mut slot_presence = SlotPresence(0);
        let mut data = String::new();
        if let Some(lowercase) = self.lowercase {
            slot_presence.add_slot(ExceptionSlot::Lower);
            data.push(lowercase)
        }
        if let Some(casefold) = self.casefold {
            slot_presence.add_slot(ExceptionSlot::Fold);
            data.push(casefold)
        }
        if let Some(uppercase) = self.uppercase {
            slot_presence.add_slot(ExceptionSlot::Upper);
            data.push(uppercase)
        }
        if let Some(titlecase) = self.titlecase {
            slot_presence.add_slot(ExceptionSlot::Title);
            data.push(titlecase)
        }
        if let Some(mut simple_case_delta) = self.simple_case_delta {
            slot_presence.add_slot(ExceptionSlot::Delta);

            if simple_case_delta >= SURROGATES_START {
                simple_case_delta += SURROGATES_LEN;
            }
            let simple_case_delta = char::try_from(simple_case_delta).unwrap_or('\0');
            data.push(simple_case_delta)
        }

        if let Some(ref closure) = self.closure {
            slot_presence.add_slot(ExceptionSlot::Closure);
            if self.full.is_some() {
                // GIGO: if the closure length is more than 0xD800 this will error. Plenty of space.
                debug_assert!(
                    closure.len() < 0xD800,
                    "Found overlarge closure value when encoding exception"
                );
                let len_char = u32::try_from(closure.len())
                    .ok()
                    .and_then(|c| char::try_from(c).ok())
                    .unwrap_or('\0');
                data.push(len_char);
            }
            data.push_str(closure);
        }
        if let Some(ref full) = self.full {
            slot_presence.add_slot(ExceptionSlot::FullMappings);
            let mut idx = 0;
            // iterate all elements except the last, whose length we can calculate from context
            for mapping in full.iter().take(3) {
                idx += mapping.len();
                data.push(char::try_from(u32::try_from(idx).unwrap_or(0)).unwrap_or('\0'));
            }
            for mapping in full {
                data.push_str(mapping);
            }
        }
        Exception {
            bits,
            slot_presence,
            data: data.into(),
        }
    }

    // Potential optimization: Write an `EncodeAsVarULE` that
    // directly produces an ExceptionULE
}

#[cfg(test)]
mod tests {
    use super::*;

    fn test_roundtrip_once(exception: DecodedException) {
        let encoded = exception.encode();
        let encoded = zerovec::ule::encode_varule_to_box(&encoded);
        let decoded = encoded.decode();
        assert_eq!(decoded, exception);
    }

    #[test]
    fn test_roundtrip() {
        test_roundtrip_once(DecodedException {
            lowercase: Some('ø'),
            ..Default::default()
        });
        test_roundtrip_once(DecodedException {
            titlecase: Some('X'),
            lowercase: Some('ø'),
            ..Default::default()
        });
        test_roundtrip_once(DecodedException {
            titlecase: Some('X'),
            ..Default::default()
        });
        test_roundtrip_once(DecodedException {
            titlecase: Some('X'),
            simple_case_delta: Some(0xE999),
            closure: Some("hello world".into()),
            ..Default::default()
        });
        test_roundtrip_once(DecodedException {
            simple_case_delta: Some(10),
            closure: Some("hello world".into()),
            full: Some(["你好世界".into(), "".into(), "hi".into(), "å".into()]),
            ..Default::default()
        });
        test_roundtrip_once(DecodedException {
            closure: Some("hello world".into()),
            full: Some(["aa".into(), "ț".into(), "".into(), "å".into()]),
            ..Default::default()
        });
        test_roundtrip_once(DecodedException {
            full: Some(["你好世界".into(), "".into(), "hi".into(), "å".into()]),
            ..Default::default()
        });
    }
}
