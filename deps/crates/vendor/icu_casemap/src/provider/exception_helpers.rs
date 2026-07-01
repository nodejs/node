// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! This module contains various types for the header part of casemapping exception data
//!
//! This is both used in datagen to decode ICU4C's data, and natively in ICU4X's
//! own data model.
//!
//! [`ExceptionBits`] is the bag of bits associated with exceptions, and [`SlotPresence`]
//! marks the presence or absence of various "slots" in a given exception.
//!
//! The `exceptions_builder` module of this crate handles decoding ICU4C data using the exception
//! header, and [`crate::provider::exceptions`] handles.

use crate::provider::data::{DotType, MappingKind};
use zerovec::ule::{AsULE, ULE};

/// A bunch of bits associated with each exception.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Copy, Clone, PartialEq, Eq, Debug, Default)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize))]
pub struct ExceptionBits {
    /// Whether or not the slots are double-width.
    ///
    /// Unused in ICU4X
    pub double_width_slots: bool,
    /// There is no simple casefolding, even if there is a simple lowercase mapping
    pub no_simple_case_folding: bool,
    /// The delta stored in the `Delta` slot is negative
    pub negative_delta: bool,
    /// If the character is case sensitive
    pub is_sensitive: bool,
    /// The dot type of the character
    pub dot_type: DotType,
    /// If the character has conditional special casing
    pub has_conditional_special: bool,
    /// If the character has conditional case folding
    pub has_conditional_fold: bool,
}

impl ExceptionBits {
    /// Extract from the upper half of an ICU4C-format u16
    pub(crate) fn from_integer(int: u8) -> Self {
        let ule = ExceptionBitsULE(int);
        let double_width_slots = ule.double_width_slots();
        let no_simple_case_folding = ule.no_simple_case_folding();
        let negative_delta = ule.negative_delta();
        let is_sensitive = ule.is_sensitive();
        let has_conditional_special = ule.has_conditional_special();
        let has_conditional_fold = ule.has_conditional_fold();
        let dot_type = ule.dot_type();

        Self {
            double_width_slots,
            no_simple_case_folding,
            negative_delta,
            is_sensitive,
            dot_type,
            has_conditional_special,
            has_conditional_fold,
        }
    }

    /// Convert to an ICU4C-format upper half of u16
    pub(crate) fn to_integer(self) -> u8 {
        let mut int = 0;
        let dot_data = (self.dot_type as u8) << ExceptionBitsULE::DOT_SHIFT;
        int |= dot_data;

        if self.double_width_slots {
            int |= ExceptionBitsULE::DOUBLE_SLOTS_FLAG
        }
        if self.no_simple_case_folding {
            int |= ExceptionBitsULE::NO_SIMPLE_CASE_FOLDING_FLAG
        }
        if self.negative_delta {
            int |= ExceptionBitsULE::NEGATIVE_DELTA_FLAG
        }
        if self.is_sensitive {
            int |= ExceptionBitsULE::SENSITIVE_FLAG
        }
        if self.has_conditional_special {
            int |= ExceptionBitsULE::CONDITIONAL_SPECIAL_FLAG
        }
        if self.has_conditional_fold {
            int |= ExceptionBitsULE::CONDITIONAL_FOLD_FLAG
        }
        int
    }
}

/// Packed slot presence marker
///
/// All bits are valid, though bit 4 is unused and reserved
///
/// Bits:
///
/// ```text
///               0: Lowercase mapping (code point)
///               1: Case folding (code point)
///               2: Uppercase mapping (code point)
///               3: Titlecase mapping (code point)
///               4: Delta to simple case mapping (code point) (sign stored separately)
///               5: RESERVED
///               6: Closure mappings (string; see below)
///               7: Full mappings (strings; see below)
/// ```
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Copy, Clone, PartialEq, Eq, ULE, Debug, Default)]
#[repr(transparent)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize))]
pub struct SlotPresence(pub u8);

impl SlotPresence {
    pub(crate) fn add_slot(&mut self, slot: ExceptionSlot) {
        self.0 |= 1 << slot as u8;
    }
    pub(crate) fn has_slot(self, slot: ExceptionSlot) -> bool {
        let bit = 1 << (slot as u8);
        self.0 & bit != 0
    }
}

/// The bitflags on an exception header.
///
/// Format from icu4c, documented in casepropsbuilder.cpp, shifted 8 bits since ICU4C has this packed
/// alongside a [`SlotPresence`]
///
/// ```text
///            0  Double-width slots. If set, then each optional slot is stored as two
///               elements of the array (high and low halves of 32-bit values) instead of
///               a single element.
///            1  Has no simple case folding, even if there is a simple lowercase mapping
///           2  The value in the delta slot is negative
///           3  Is case-sensitive (not exposed)
///       4..5  Dot type
///           6  Has conditional special casing
///           7  Has conditional case folding
/// ```
///
/// All bits are valid, though in ICU4X data bits 0 and 2 are not used
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Copy, Clone, PartialEq, Eq, ULE, Debug)]
#[repr(transparent)]
pub struct ExceptionBitsULE(pub u8);

impl ExceptionBitsULE {
    const DOUBLE_SLOTS_FLAG: u8 = 0x1;

    const NO_SIMPLE_CASE_FOLDING_FLAG: u8 = 0x2;
    const NEGATIVE_DELTA_FLAG: u8 = 0x4;
    const SENSITIVE_FLAG: u8 = 0x8;

    const DOT_SHIFT: u8 = 4;

    const CONDITIONAL_SPECIAL_FLAG: u8 = 0x40;
    const CONDITIONAL_FOLD_FLAG: u8 = 0x80;
}

impl ExceptionBitsULE {
    /// Whether or not the slots are double-width.
    ///
    /// Unused in ICU4X
    pub fn double_width_slots(self) -> bool {
        self.0 & Self::DOUBLE_SLOTS_FLAG != 0
    }

    /// There is no simple casefolding, even if there is a simple lowercase mapping
    pub fn no_simple_case_folding(self) -> bool {
        self.0 & Self::NO_SIMPLE_CASE_FOLDING_FLAG != 0
    }

    /// The delta stored in the `Delta` slot is negative
    pub fn negative_delta(self) -> bool {
        self.0 & Self::NEGATIVE_DELTA_FLAG != 0
    }

    /// If the character is case sensitive
    pub fn is_sensitive(self) -> bool {
        self.0 & Self::SENSITIVE_FLAG != 0
    }

    /// If the character has conditional special casing
    pub fn has_conditional_special(self) -> bool {
        self.0 & Self::CONDITIONAL_SPECIAL_FLAG != 0
    }

    /// If the character has conditional case folding
    pub fn has_conditional_fold(self) -> bool {
        self.0 & Self::CONDITIONAL_FOLD_FLAG != 0
    }

    /// The dot type of the character
    pub fn dot_type(self) -> DotType {
        DotType::from_masked_bits((u16::from(self.0 >> Self::DOT_SHIFT)) & DotType::DOT_MASK)
    }
}

impl AsULE for ExceptionBits {
    type ULE = ExceptionBitsULE;
    fn from_unaligned(u: ExceptionBitsULE) -> Self {
        ExceptionBits::from_integer(u.0)
    }

    fn to_unaligned(self) -> ExceptionBitsULE {
        ExceptionBitsULE(self.to_integer())
    }
}

impl AsULE for SlotPresence {
    type ULE = SlotPresence;
    fn from_unaligned(u: Self) -> Self {
        u
    }

    fn to_unaligned(self) -> Self {
        self
    }
}

/// The different slots that may be present in slot-based exception data
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Copy, Clone, Debug, PartialOrd, Ord, PartialEq, Eq)]
pub(crate) enum ExceptionSlot {
    /// Lowercase mapping
    Lower = 0,
    /// Case folding
    Fold = 1,
    /// Uppercase mapping
    Upper = 2,
    /// Titlecase mapping
    Title = 3,
    /// The delta to the simple case folding
    Delta = 4,
    // Slot 5 is reserved
    /// The closure set
    Closure = 6,
    /// The four full-mappings
    FullMappings = 7,
}

impl ExceptionSlot {
    /// Where the string slots begin
    pub(crate) const STRING_SLOTS_START: Self = Self::Closure;
}

impl From<MappingKind> for ExceptionSlot {
    fn from(full: MappingKind) -> Self {
        match full {
            MappingKind::Lower => Self::Lower,
            MappingKind::Fold => Self::Fold,
            MappingKind::Upper => Self::Upper,
            MappingKind::Title => Self::Title,
        }
    }
}
