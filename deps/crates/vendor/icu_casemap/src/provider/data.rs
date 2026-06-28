// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! The primary per-codepoint casefolding data

#[cfg(feature = "datagen")]
use alloc::collections::BTreeMap;
use core::num::TryFromIntError;
use icu_collections::codepointtrie::TrieValue;
use zerovec::ule::{AsULE, RawBytesULE, UleError, ULE};

/// The case of a Unicode character
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_casemap::provider::data))]
pub enum CaseType {
    /// Lowercase letter
    Lower = 1,
    /// Uppercase letter
    Upper = 2,
    /// Titlecase letter
    Title = 3,
}

impl CaseType {
    pub(crate) const CASE_MASK: u16 = 0x3;

    // The casetype is stored in the codepoint trie as two bits.
    // After masking them to get a value between 0 and 3, this
    // function converts to `CaseType`.
    //
    // Returns `None` for uncased
    #[inline]
    pub(crate) fn from_masked_bits(b: u16) -> Option<Self> {
        debug_assert!(b & Self::CASE_MASK == b);
        match b {
            0 => None,
            1 => Some(CaseType::Lower),
            2 => Some(CaseType::Upper),
            _ => Some(CaseType::Title),
        }
    }
}

/// The dot type of a Unicode character. This indicates how dotted
/// letters (like `i` and `j`) combine with accents placed above the
/// letter.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_casemap::provider::data))]
#[derive(Default)]
pub enum DotType {
    /// Normal characters with combining class 0
    #[default]
    NoDot = 0,
    /// Soft-dotted characters with combining class 0
    SoftDotted = 1,
    /// "Above" accents with combining class 230
    Above = 2,
    /// Other accent characters
    OtherAccent = 3,
}

impl DotType {
    pub(crate) const DOT_MASK: u16 = 0x3;

    // The dot type is stored in either the codepoint trie or the
    // exception table as two bits.  After shifting and masking them
    // to get a value between 0 and 3, this function converts to
    // DotType.
    #[inline]
    pub(crate) fn from_masked_bits(b: u16) -> Self {
        debug_assert!(b & Self::DOT_MASK == b);
        match b {
            0 => DotType::NoDot,
            1 => DotType::SoftDotted,
            2 => DotType::Above,
            _ => DotType::OtherAccent,
        }
    }
}

#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub(crate) enum MappingKind {
    Lower = 0,
    Fold = 1,
    Upper = 2,
    Title = 3,
}

/// Case mapping data associated with a single code point
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_casemap::provider::data))]
pub struct CaseMapData {
    /// Whether this is default-ignoreable
    pub ignoreable: bool,
    /// The rest of the case mapping data
    pub kind: CaseMapDataKind,
}

/// A subset of case mapping data associated with a single code point
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_casemap::provider::data))]
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub enum CaseMapDataKind {
    /// This code point is an exception. Provides the case type of its own case
    /// and the exception index stored in [`CaseMapExceptions`]
    ///
    /// [`CaseMapExceptions`]: crate::provider::exceptions::CaseMapExceptions
    Exception(Option<CaseType>, u16),
    /// This code point is uncased, and has the following extra data
    Uncased(NonExceptionData),
    /// This code point is cased. We store the extra data, its case type, and a *delta*
    /// that can be used to get its casemapped codepoint.
    Delta(NonExceptionData, CaseType, i16),
}

/// Data that is stored in [`CaseMapData`] when it is *not* an exception
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_casemap::provider::data))]
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub struct NonExceptionData {
    /// Whether or not the type is case-sensitive
    pub sensitive: bool,
    /// The "dot type"
    pub dot_type: DotType,
}

impl CaseMapData {
    #[inline]
    pub(crate) fn case_type(self) -> Option<CaseType> {
        match self.kind {
            CaseMapDataKind::Exception(case_type, ..) => case_type,
            CaseMapDataKind::Delta(_, case_type, _) => Some(case_type),
            CaseMapDataKind::Uncased(..) => None,
        }
    }

    #[inline]
    pub(crate) fn is_upper_or_title(self) -> bool {
        match self.case_type() {
            None | Some(CaseType::Lower) => false,
            Some(CaseType::Upper) | Some(CaseType::Title) => true,
        }
    }

    #[inline]
    pub(crate) fn is_relevant_to(self, kind: MappingKind) -> bool {
        match kind {
            MappingKind::Lower | MappingKind::Fold => self.is_upper_or_title(),
            MappingKind::Upper | MappingKind::Title => self.case_type() == Some(CaseType::Lower),
        }
    }

    #[inline]
    pub(crate) fn is_ignorable(self) -> bool {
        self.ignoreable
    }

    #[inline]
    pub(crate) fn has_exception(self) -> bool {
        matches!(self.kind, CaseMapDataKind::Exception(..))
    }

    // Returns true if this code point is case-sensitive.
    // only in the non-exception case
    // This is not currently exposed.
    #[inline]
    pub(crate) fn is_sensitive(self) -> bool {
        match self.kind {
            CaseMapDataKind::Exception(..) => false,
            CaseMapDataKind::Delta(ned, ..) => ned.sensitive,
            CaseMapDataKind::Uncased(ned) => ned.sensitive,
        }
    }

    #[inline]
    pub(crate) fn dot_type(self) -> DotType {
        match self.kind {
            CaseMapDataKind::Exception(..) => DotType::NoDot,
            CaseMapDataKind::Delta(ned, ..) => ned.dot_type,
            CaseMapDataKind::Uncased(ned) => ned.dot_type,
        }
    }

    // The delta between this code point and its upper/lowercase equivalent.
    // This should only be called for codepoints without exception data.
    //
    // Returns 0 for uncased types
    #[inline]
    pub(crate) fn delta(self) -> i16 {
        debug_assert!(!self.has_exception());
        match self.kind {
            CaseMapDataKind::Exception(..) => 0,
            CaseMapDataKind::Delta(.., delta) => delta,
            CaseMapDataKind::Uncased(..) => 0,
        }
    }

    // The index of the exception data for this codepoint in the exception
    // table. This should only be called for codepoints with exception data.
    #[inline]
    pub(crate) fn exception_index(self) -> u16 {
        debug_assert!(self.has_exception());
        if let CaseMapDataKind::Exception(_, i) = self.kind {
            i
        } else {
            0
        }
    }

    // CaseMapExceptionsBuilder moves the full mapping and closure
    // strings out of the exception table itself. This means that the
    // exception index for a code point in ICU4X will be different
    // from the exception index for the same codepoint in ICU4C. Given
    // a mapping from old to new, this function updates the exception
    // index if necessary.
    #[cfg(feature = "datagen")]
    pub(crate) fn with_updated_exception(self, updates: &BTreeMap<u16, u16>) -> Self {
        let kind = if let CaseMapDataKind::Exception(ty, index) = self.kind {
            if let Some(updated_exception) = updates.get(&index) {
                CaseMapDataKind::Exception(ty, *updated_exception)
            } else {
                self.kind
            }
        } else {
            self.kind
        };

        Self { kind, ..self }
    }

    /// Attempt to construct from ICU-format integer
    #[cfg(any(feature = "datagen", test))]
    pub(crate) fn try_from_icu_integer(int: u16) -> Result<Self, UleError> {
        let raw = int.to_unaligned();
        CaseMapDataULE::validate_bytes(raw.as_bytes())?;

        let this = Self::from_unaligned(CaseMapDataULE(raw));
        Ok(this)
    }
}

impl TrieValue for CaseMapData {
    type TryFromU32Error = TryFromIntError;

    fn try_from_u32(i: u32) -> Result<Self, Self::TryFromU32Error> {
        u16::try_from(i).map(|u| AsULE::from_unaligned(CaseMapDataULE(u.to_unaligned())))
    }

    fn to_u32(self) -> u32 {
        u32::from(self.to_unaligned().0.as_unsigned_int())
    }
}

/// Packed casemappingdata type
///
/// Data format, copied from ICU4C casepropsbuilder.cpp:
///
/// ```text
/// Trie data word:
/// Bits
/// if(exception) {
///     15..4   unsigned exception index
/// } else {
///     if(not uncased) {
///         15..7   signed delta to simple case mapping code point
///                 (add delta to input code point)
///     } else {
///         15..7   reserved, 0
///     }
///      6..5   0 normal character with cc=0
///             1 soft-dotted character
///             2 cc=230
///             3 other cc
///             The runtime code relies on these two bits to be adjacent with this encoding.
/// }
///     4   case-sensitive
///     3   exception
///     2   case-ignorable
///  1..0   0 uncased
///         1 lowercase
///         2 uppercase
///         3 titlecase
///         The runtime code relies on the case-ignorable and case type bits 2..0
///         to be the lowest bits with this encoding.
/// ```
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
#[repr(transparent)]
pub struct CaseMapDataULE(RawBytesULE<2>);

impl CaseMapDataULE {
    // 1..0 case type
    const CASE_TYPE_BITS: u16 = 0x3;
    // 2 case-ignorable
    const CASE_IGNOREABLE_BIT: u16 = 0x4;
    // 3 exception
    const EXCEPTION_BIT: u16 = 0x8;
    // 4 case-sensitive
    const CASE_SENSITIVE_BIT: u16 = 0x10;
    // 15..4 unsigned exception index
    const EXCEPTION_SHIFT: u16 = 4;
    // 15..7 signed-delta to simple case mapping code point (or reserved)
    const DELTA_SHIFT: u16 = 7;
    // 6..5 dot type
    const DOT_TYPE_BITS: u16 = 0x60;
    const DOT_SHIFT: u16 = 5;
}

/// # Safety
///
/// Safety checklist for `ULE`:
///
/// 1. The type *must not* include any uninitialized or padding bytes: repr(transparent)
///    wrapper around ULE type
/// 2. The type must have an alignment of 1 byte: repr(transparent) wrapper around ULE type
/// 3. The impl of [`ULE::validate_bytes()`] *must* return an error if the given byte slice
///    would not represent a valid slice of this type: It does
/// 4. The impl of [`ULE::validate_bytes()`] *must* return an error if the given byte slice
///    cannot be used in its entirety (if its length is not a multiple of `size_of::<Self>()`):
///    it does, due to the [`RawBytesULE`] parse call
/// 5. All other methods *must* be left with their default impl, or else implemented according to
///    their respective safety guidelines: They have been
/// 6. The equality invariant is satisfied
unsafe impl ULE for CaseMapDataULE {
    fn validate_bytes(bytes: &[u8]) -> Result<(), UleError> {
        let sixteens = RawBytesULE::<2>::parse_bytes_to_slice(bytes)?;

        for sixteen in sixteens {
            let sixteen = sixteen.as_unsigned_int();
            // The type has reserved bits in the
            // uncased + not exception case
            if sixteen & Self::EXCEPTION_BIT == 0 {
                // not an exception
                if sixteen & Self::CASE_TYPE_BITS == 0 {
                    // uncased
                    if sixteen >> Self::DELTA_SHIFT != 0 {
                        // We have some used bits in the reserved zone!
                        return Err(UleError::parse::<Self>());
                    }
                }
            }
        }
        Ok(())
    }
}

impl AsULE for CaseMapData {
    type ULE = CaseMapDataULE;

    fn from_unaligned(ule: Self::ULE) -> Self {
        let sixteen = ule.0.as_unsigned_int();

        let ignoreable = (sixteen & CaseMapDataULE::CASE_IGNOREABLE_BIT) != 0;
        let exception = (sixteen & CaseMapDataULE::EXCEPTION_BIT) != 0;

        let case_type = sixteen & CaseMapDataULE::CASE_TYPE_BITS;
        let case_type = CaseType::from_masked_bits(case_type);
        let kind = if exception {
            // No need to mask first since the exception bits start at 15
            let exception = sixteen >> CaseMapDataULE::EXCEPTION_SHIFT;
            CaseMapDataKind::Exception(case_type, exception)
        } else {
            let dot_type = (sixteen & CaseMapDataULE::DOT_TYPE_BITS) >> CaseMapDataULE::DOT_SHIFT;
            let dot_type = DotType::from_masked_bits(dot_type);
            let sensitive = (sixteen & CaseMapDataULE::CASE_SENSITIVE_BIT) != 0;
            let ned = NonExceptionData {
                dot_type,
                sensitive,
            };
            if let Some(case_type) = case_type {
                // no need to mask first since the delta bits start at 15
                // We can also cast as i16 first so we do not have to
                // sign-extend later
                let delta = (sixteen as i16) >> CaseMapDataULE::DELTA_SHIFT;
                CaseMapDataKind::Delta(ned, case_type, delta)
            } else {
                CaseMapDataKind::Uncased(ned)
            }
        };
        CaseMapData { ignoreable, kind }
    }

    fn to_unaligned(self) -> Self::ULE {
        let mut sixteen = 0;
        if self.ignoreable {
            sixteen |= CaseMapDataULE::CASE_IGNOREABLE_BIT;
        }
        match self.kind {
            CaseMapDataKind::Exception(case_type, e) => {
                sixteen |= CaseMapDataULE::EXCEPTION_BIT;
                sixteen |= e << CaseMapDataULE::EXCEPTION_SHIFT;
                sixteen |= case_type.map(|c| c as u16).unwrap_or(0);
            }
            CaseMapDataKind::Uncased(ned) => {
                sixteen |= (ned.dot_type as u16) << CaseMapDataULE::DOT_SHIFT;
                if ned.sensitive {
                    sixteen |= CaseMapDataULE::CASE_SENSITIVE_BIT;
                }
                // Remaining bytes are left at zero
                // case_type is Uncased (0)
            }
            CaseMapDataKind::Delta(ned, case_type, delta) => {
                // First shift (which keeps the signedness), then cast to the
                // right type
                sixteen |= (delta << CaseMapDataULE::DELTA_SHIFT) as u16;
                sixteen |= (ned.dot_type as u16) << CaseMapDataULE::DOT_SHIFT;
                if ned.sensitive {
                    sixteen |= CaseMapDataULE::CASE_SENSITIVE_BIT;
                }
                sixteen |= case_type as u16;
            }
        }
        CaseMapDataULE(sixteen.to_unaligned())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_roundtrip() {
        const TESTCASES: &[CaseMapData] = &[
            CaseMapData {
                ignoreable: true,
                kind: CaseMapDataKind::Exception(Some(CaseType::Title), 923),
            },
            CaseMapData {
                ignoreable: false,
                kind: CaseMapDataKind::Exception(None, 923),
            },
            CaseMapData {
                ignoreable: true,
                kind: CaseMapDataKind::Delta(
                    NonExceptionData {
                        sensitive: true,
                        dot_type: DotType::SoftDotted,
                    },
                    CaseType::Upper,
                    50,
                ),
            },
            CaseMapData {
                ignoreable: false,
                kind: CaseMapDataKind::Delta(
                    NonExceptionData {
                        sensitive: true,
                        dot_type: DotType::SoftDotted,
                    },
                    CaseType::Upper,
                    -50,
                ),
            },
            CaseMapData {
                ignoreable: false,
                kind: CaseMapDataKind::Uncased(NonExceptionData {
                    sensitive: false,
                    dot_type: DotType::SoftDotted,
                }),
            },
        ];

        for case in TESTCASES {
            let ule = case.to_unaligned();
            let roundtrip = CaseMapData::from_unaligned(ule);
            assert_eq!(*case, roundtrip);
            let integer = ule.0.as_unsigned_int();
            let roundtrip2 = CaseMapData::try_from_icu_integer(integer).unwrap();
            assert_eq!(*case, roundtrip2);
        }
    }
    #[test]
    fn test_integer_roundtrip() {
        // Buggy roundtrip cases go here
        fn test_single_integer(int: u16) {
            let cmd = CaseMapData::try_from_icu_integer(int).unwrap();
            assert_eq!(int, cmd.to_unaligned().0.as_unsigned_int())
        }

        test_single_integer(84);
        test_single_integer(2503);
    }
}
