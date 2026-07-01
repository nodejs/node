// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

// Provider structs must be stable
#![allow(clippy::exhaustive_structs, clippy::exhaustive_enums)]

//! 🚧 \[Unstable\] Data provider struct definitions for this ICU4X component.
//!
//! <div class="stab unstable">
//! 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
//! including in SemVer minor releases. While the serde representation of data structs is guaranteed
//! to be stable, their Rust representation might not be. Use with caution.
//! </div>
//!
//! Read more about data providers: [`icu_provider`]

use crate::provider::rules::runtime::ast::Rule;
use crate::{PluralCategory, PluralElements, PluralElementsInner, PluralOperands, PluralRules};
use alloc::borrow::{Cow, ToOwned};
use alloc::boxed::Box;
use alloc::vec::Vec;
use core::fmt;
use core::marker::PhantomData;
use icu_provider::prelude::*;
use yoke::Yokeable;
use zerofrom::ZeroFrom;
use zerovec::ule::vartuple::VarTuple;
use zerovec::ule::vartuple::VarTupleULE;
use zerovec::ule::AsULE;
use zerovec::ule::EncodeAsVarULE;
use zerovec::ule::UleError;
use zerovec::ule::VarULE;
use zerovec::ule::ULE;
use zerovec::VarZeroSlice;

pub mod rules;

#[cfg(feature = "compiled_data")]
#[derive(Debug)]
/// Baked data
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. In particular, the `DataProvider` implementations are only
/// guaranteed to match with this version's `*_unstable` providers. Use with caution.
/// </div>
pub struct Baked;

#[cfg(feature = "compiled_data")]
#[allow(unused_imports)]
const _: () = {
    use icu_plurals_data::*;
    mod icu {
        pub use crate as plurals;
        pub use icu_locale as locale;
    }

    make_provider!(Baked);
    impl_plurals_cardinal_v1!(Baked);
    impl_plurals_ordinal_v1!(Baked);
    #[cfg(feature = "unstable")]
    impl_plurals_ranges_v1!(Baked);
};

icu_provider::data_marker!(
    /// Data for cardinal classification
    PluralsCardinalV1,
    "plurals/cardinal/v1",
    PluralRulesData<'static>,
);

icu_provider::data_marker!(
    /// Data for ordinal classification
    PluralsOrdinalV1,
    "plurals/ordinal/v1",
    PluralRulesData<'static>,
);

#[cfg(feature = "unstable")]
icu_provider::data_marker!(
    /// Data for plural range formatting
    PluralsRangesV1,
    "plurals/ranges/v1",
    PluralRanges<'static>
);

#[cfg(feature = "datagen")]
/// The latest minimum set of markers required by this component.
pub const MARKERS: &[DataMarkerInfo] = &[
    PluralsCardinalV1::INFO,
    PluralsOrdinalV1::INFO,
    #[cfg(feature = "unstable")]
    PluralsRangesV1::INFO,
];

/// Plural rule strings conforming to UTS 35 syntax. Includes separate fields for five of the six
/// standard plural forms. If none of the rules match, the "other" category is assumed.
///
/// More information: <https://unicode.org/reports/tr35/tr35-numbers.html#Language_Plural_Rules>
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Default, Clone, PartialEq, Debug, Yokeable, ZeroFrom)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_plurals::provider))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct PluralRulesData<'data> {
    /// Rule that matches [`PluralCategory::Zero`], or `None` if not present.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub zero: Option<Rule<'data>>,
    /// Rule that matches [`PluralCategory::One`], or `None` if not present.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub one: Option<Rule<'data>>,
    /// Rule that matches [`PluralCategory::Two`], or `None` if not present.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub two: Option<Rule<'data>>,
    /// Rule that matches [`PluralCategory::Few`], or `None` if not present.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub few: Option<Rule<'data>>,
    /// Rule that matches [`PluralCategory::Many`], or `None` if not present.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub many: Option<Rule<'data>>,
}

icu_provider::data_struct!(
    PluralRulesData<'_>,
    #[cfg(feature = "datagen")]
);

#[cfg(feature = "unstable")]
pub use ranges::*;

#[cfg(feature = "unstable")]
mod ranges {
    use super::*;
    use zerovec::ZeroMap;

    /// [`PluralCategory`] but serializable as provider data.
    ///
    /// <div class="stab unstable">
    /// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
    /// including in SemVer minor releases. While the serde representation of data structs is guaranteed
    /// to be stable, their Rust representation might not be. Use with caution.
    /// </div>
    #[derive(Debug, PartialEq, Eq, Clone, Copy, Hash, Ord, PartialOrd)]
    #[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
    #[cfg_attr(feature = "datagen", databake(path = icu_plurals::provider))]
    #[cfg_attr(feature = "serde", derive(serde::Deserialize))]
    #[zerovec::make_ule(RawPluralCategoryULE)]
    #[repr(u8)]
    #[cfg_attr(
        any(feature = "datagen", feature = "serde"),
        serde(rename_all = "lowercase")
    )]
    pub enum RawPluralCategory {
        /// CLDR "other" plural category.
        Other = 0,
        /// CLDR "zero" plural category.
        Zero = 1,
        /// CLDR "one" plural category.
        One = 2,
        /// CLDR "two" plural category.
        Two = 3,
        /// CLDR "few" plural category.
        Few = 4,
        /// CLDR "many" plural category.
        Many = 5,
    }

    impl RawPluralCategory {
        /// Gets the corresponding variant string of this `RawPluralCategory`.
        #[cfg(any(feature = "datagen", feature = "serde"))]
        const fn as_str(self) -> &'static str {
            match self {
                Self::Other => "other",
                Self::Zero => "zero",
                Self::One => "one",
                Self::Two => "two",
                Self::Few => "few",
                Self::Many => "many",
            }
        }
    }

    impl From<RawPluralCategory> for PluralCategory {
        fn from(value: RawPluralCategory) -> Self {
            match value {
                RawPluralCategory::Other => PluralCategory::Other,
                RawPluralCategory::Zero => PluralCategory::Zero,
                RawPluralCategory::One => PluralCategory::One,
                RawPluralCategory::Two => PluralCategory::Two,
                RawPluralCategory::Few => PluralCategory::Few,
                RawPluralCategory::Many => PluralCategory::Many,
            }
        }
    }

    impl From<PluralCategory> for RawPluralCategory {
        fn from(value: PluralCategory) -> Self {
            match value {
                PluralCategory::Zero => RawPluralCategory::Zero,
                PluralCategory::One => RawPluralCategory::One,
                PluralCategory::Two => RawPluralCategory::Two,
                PluralCategory::Few => RawPluralCategory::Few,
                PluralCategory::Many => RawPluralCategory::Many,
                PluralCategory::Other => RawPluralCategory::Other,
            }
        }
    }

    /// An `u8` that is expected to be a plural range, but does not enforce this invariant.
    ///
    /// <div class="stab unstable">
    /// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
    /// including in SemVer minor releases. While the serde representation of data structs is guaranteed
    /// to be stable, their Rust representation might not be. Use with caution.
    /// </div>
    #[derive(Debug, PartialEq, Eq, Clone, Copy, Hash, Ord, PartialOrd)]
    #[cfg_attr(feature = "datagen", derive(databake::Bake))]
    #[cfg_attr(feature = "datagen", databake(path = icu_plurals::provider))]
    #[zerovec::make_ule(UnvalidatedPluralRangeULE)]
    pub struct UnvalidatedPluralRange(pub u8);

    impl UnvalidatedPluralRange {
        /// Creates a new `UnvalidatedPluralRange` from a category range.
        pub fn from_range(start: RawPluralCategory, end: RawPluralCategory) -> Self {
            let start = start as u8;
            let end = end as u8;

            debug_assert!(start < 16);
            debug_assert!(end < 16);

            let range = (start << 4) | end;

            Self(range)
        }
    }

    #[cfg(feature = "datagen")]
    impl serde::Serialize for UnvalidatedPluralRange {
        fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
        where
            S: serde::Serializer,
        {
            use serde::ser::Error;

            struct PrettyPrinter(RawPluralCategory, RawPluralCategory);

            impl fmt::Display for PrettyPrinter {
                fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
                    f.write_str(self.0.as_str())?;
                    f.write_str("--")?;
                    f.write_str(self.1.as_str())
                }
            }

            if serializer.is_human_readable() {
                let start = RawPluralCategory::new_from_u8(self.0 >> 4)
                    .ok_or_else(|| S::Error::custom("invalid tag in UnvalidatedPluralRange"))?;
                let end = RawPluralCategory::new_from_u8(self.0 & 0x0F)
                    .ok_or_else(|| S::Error::custom("invalid tag in UnvalidatedPluralRange"))?;
                serializer.collect_str(&PrettyPrinter(start, end))
            } else {
                self.0.serialize(serializer)
            }
        }
    }

    #[cfg(feature = "serde")]
    impl<'de> serde::Deserialize<'de> for UnvalidatedPluralRange {
        fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
        where
            D: serde::Deserializer<'de>,
        {
            use serde::de::{Error, Visitor};

            struct HumanReadableVisitor;

            impl Visitor<'_> for HumanReadableVisitor {
                type Value = UnvalidatedPluralRange;

                fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                    write!(
                        formatter,
                        "a plural range of the form <PluralCategory>-<PluralCategory>",
                    )
                }

                fn visit_str<E>(self, v: &str) -> Result<Self::Value, E>
                where
                    E: Error,
                {
                    const VARIANTS: [&str; 6] = [
                        RawPluralCategory::Other.as_str(),
                        RawPluralCategory::Zero.as_str(),
                        RawPluralCategory::One.as_str(),
                        RawPluralCategory::Two.as_str(),
                        RawPluralCategory::Few.as_str(),
                        RawPluralCategory::Many.as_str(),
                    ];

                    let (start, end) = v
                        .split_once("--")
                        .ok_or_else(|| E::custom("expected token `--` in plural range"))?;

                    let start = PluralCategory::get_for_cldr_string(start)
                        .ok_or_else(|| E::unknown_variant(start, &VARIANTS))?;
                    let end = PluralCategory::get_for_cldr_string(end)
                        .ok_or_else(|| E::unknown_variant(end, &VARIANTS))?;

                    Ok(UnvalidatedPluralRange::from_range(start.into(), end.into()))
                }
            }

            if deserializer.is_human_readable() {
                deserializer.deserialize_str(HumanReadableVisitor)
            } else {
                Ok(Self(<u8>::deserialize(deserializer)?))
            }
        }
    }

    /// Plural categories for ranges.
    ///
    /// Obtains the plural category of a range from the categories of its endpoints. It is required that
    /// the start value must be strictly less than the end value, and both values must be strictly positive.
    ///
    /// More information: <https://unicode.org/reports/tr35/tr35-numbers.html#Plural_Ranges>
    ///
    /// <div class="stab unstable">
    /// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
    /// including in SemVer minor releases. While the serde representation of data structs is guaranteed
    /// to be stable, their Rust representation might not be. Use with caution.
    /// </div>
    #[derive(Clone, PartialEq, Debug, Yokeable, ZeroFrom)]
    #[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
    #[cfg_attr(feature = "datagen", databake(path = icu_plurals::provider))]
    #[cfg_attr(feature = "serde", derive(serde::Deserialize))]
    #[yoke(prove_covariance_manually)]
    pub struct PluralRanges<'data> {
        /// Map between the categories of the endpoints of a range and its corresponding
        /// category.
        ///
        /// This is roughly equivalent to a `BTreeMap<(PluralCategory, PluralCategory), PluralCategory>`,
        /// where the key is `(start category, end category)`.
        #[cfg_attr(feature = "serde", serde(borrow))]
        pub ranges: ZeroMap<'data, UnvalidatedPluralRange, RawPluralCategory>,
    }

    icu_provider::data_struct!(
        PluralRanges<'_>,
        #[cfg(feature = "datagen")]
    );
}

/// A sized packed [`PluralElements`] suitable for use in data structs.
///
/// This type has the following limitations:
///
/// 1. It only supports `str`
/// 2. It does not implement [`VarULE`] so it can't be used in a [`VarZeroSlice`]
/// 3. It always serializes the [`FourBitMetadata`] as 0
///
/// Use [`PluralElementsPackedULE`] directly if you need these additional features.
#[derive(Debug, PartialEq, Yokeable, ZeroFrom)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_plurals::provider))]
#[cfg_attr(
    feature = "serde",
    serde(
        transparent,
        bound(
            serialize = "V: serde::Serialize + PartialEq",
            deserialize = "Box<PluralElementsPackedULE<V>>: serde::Deserialize<'de>"
        )
    )
)]
pub struct PluralElementsPackedCow<'data, V: VarULE + ?Sized> {
    /// The encoded elements.
    #[cfg_attr(
        feature = "serde",
        serde(
            borrow,
            deserialize_with = "deserialize_plural_elements_packed_cow::<_, V>"
        )
    )]
    pub elements: Cow<'data, PluralElementsPackedULE<V>>,
}

/// A bitpacked DST for [`PluralElements`].
///
/// Can be put in a [`Cow`] or a [`VarZeroSlice`].
#[derive(PartialEq, Eq)]
#[repr(transparent)]
pub struct PluralElementsPackedULE<V: VarULE + ?Sized> {
    _v: PhantomData<V>,
    /// Invariant Representation:
    ///
    /// First byte: `d...mmmm`
    /// - `d` = 0 if singleton, 1 if a map
    /// - `...` = padding, should be 0
    /// - `mmmm` = [`FourBitMetadata`] for the default value
    ///
    /// If d is 0:
    /// - Remainder: the default (plural "other") value `V`
    ///
    /// If d is 1:
    /// - Second byte: L = the length of `V`
    /// - Bytes 2..(2+L): the default (plural "other") value `V`
    /// - Remainder: [`PluralElementsTupleSliceVarULE`]
    bytes: [u8],
}

impl<V: VarULE + fmt::Debug + ?Sized> fmt::Debug for PluralElementsPackedULE<V> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let unpacked = self.as_parts();
        f.debug_struct("PluralElementsPackedULE")
            .field("parts", &unpacked)
            .field("bytes", &&self.bytes)
            .finish()
    }
}

impl<V: VarULE + ?Sized> ToOwned for PluralElementsPackedULE<V> {
    type Owned = Box<PluralElementsPackedULE<V>>;
    fn to_owned(&self) -> Self::Owned {
        self.to_boxed()
    }
}

// Safety (based on the safety checklist on the VarULE trait):
//  1. PluralElementsPackedULE does not include any uninitialized or padding bytes: it is transparent over a VarULE type ([u8])
//  2. PluralElementsPackedULE is aligned to 1 byte: it is transparent over a VarULE type ([u8])
//  3. The impl of `validate_bytes()` returns an error if any byte is not valid.
//  4. The impl of `validate_bytes()` returns an error if the slice cannot be used in its entirety
//  5. The impl of `from_bytes_unchecked()` returns a reference to the same data.
//  6. `parse_bytes()` is equivalent to `validate_bytes()` followed by `from_bytes_unchecked()`
//  7. byte equality is semantic equality
unsafe impl<V> VarULE for PluralElementsPackedULE<V>
where
    V: VarULE + ?Sized,
{
    fn validate_bytes(bytes: &[u8]) -> Result<(), UleError> {
        let unpacked_bytes =
            Self::unpack_bytes(bytes).ok_or_else(|| UleError::length::<Self>(bytes.len()))?;
        // The high bit of lead_byte was read in unpack_bytes.
        // Bits 0-3 are FourBitMetadata.
        // We expect bits 4-6 to be padding.
        if unpacked_bytes.lead_byte & 0x70 != 0 {
            return Err(UleError::parse::<Self>());
        }
        // Now validate the two variable-length slices.
        V::validate_bytes(unpacked_bytes.v_bytes)?;
        if let Some(specials_bytes) = unpacked_bytes.specials_bytes {
            PluralElementsTupleSliceVarULE::<V>::validate_bytes(specials_bytes)?;
        }
        Ok(())
    }

    unsafe fn from_bytes_unchecked(bytes: &[u8]) -> &Self {
        // Safety: the bytes are valid by trait invariant, and we are transparent over bytes
        &*(bytes as *const [u8] as *const Self)
    }
}

impl<V> PluralElementsPackedULE<V>
where
    V: VarULE + ?Sized,
{
    /// Casts a byte slice to a [`PluralElementsPackedULE`].
    ///
    /// # Safety
    ///
    /// The bytes must be valid according to [`PluralElementsPackedULE::validate_bytes`].
    pub const unsafe fn from_bytes_unchecked(bytes: &[u8]) -> &Self {
        // Safety: the bytes are valid by trait invariant, and we are transparent over bytes
        &*(bytes as *const [u8] as *const Self)
    }

    /// Returns a tuple with:
    /// 1. The lead byte
    /// 2. Bytes corresponding to the default V
    /// 3. Bytes corresponding to the specials slice, if present
    #[inline]
    fn unpack_bytes(bytes: &[u8]) -> Option<PluralElementsUnpackedBytes<'_>> {
        let (lead_byte, remainder) = bytes.split_first()?;
        if lead_byte & 0x80 == 0 {
            Some(PluralElementsUnpackedBytes {
                lead_byte: *lead_byte,
                v_bytes: remainder,
                specials_bytes: None,
            })
        } else {
            let (second_byte, remainder) = remainder.split_first()?;
            let (v_bytes, remainder) = remainder.split_at_checked(*second_byte as usize)?;
            Some(PluralElementsUnpackedBytes {
                lead_byte: *lead_byte,
                v_bytes,
                specials_bytes: Some(remainder),
            })
        }
    }

    /// Unpacks this structure into the default value and the optional list of specials.
    fn as_parts(&self) -> PluralElementsUnpacked<'_, V> {
        // Safety: the bytes are valid by invariant
        let unpacked_bytes = unsafe { Self::unpack_bytes(&self.bytes).unwrap_unchecked() };
        let metadata = FourBitMetadata(unpacked_bytes.lead_byte & 0x0F);
        // Safety: the bytes are valid by invariant
        let default = unsafe { V::from_bytes_unchecked(unpacked_bytes.v_bytes) };
        #[expect(clippy::manual_map)] // more explicit with the unsafe code
        let specials = if let Some(specials_bytes) = unpacked_bytes.specials_bytes {
            // Safety: the bytes are valid by invariant
            Some(unsafe {
                PluralElementsTupleSliceVarULE::<V>::from_bytes_unchecked(specials_bytes)
            })
        } else {
            None
        };
        PluralElementsUnpacked {
            default: (metadata, default),
            specials,
        }
    }

    /// Returns the value for the given [`PluralOperands`] and [`PluralRules`].
    pub fn get<'a>(&'a self, op: PluralOperands, rules: &PluralRules) -> (FourBitMetadata, &'a V) {
        let parts = self.as_parts();

        let category = rules.category_for(op);

        match parts.specials {
            Some(specials) => {
                if op.is_exactly_zero() {
                    if let Some(value) = get_special(specials, PluralElementsKeys::ExplicitZero) {
                        return value;
                    }
                }
                if op.is_exactly_one() {
                    if let Some(value) = get_special(specials, PluralElementsKeys::ExplicitOne) {
                        return value;
                    }
                }
                match category {
                    PluralCategory::Zero => Some(PluralElementsKeys::Zero),
                    PluralCategory::One => Some(PluralElementsKeys::One),
                    PluralCategory::Two => Some(PluralElementsKeys::Two),
                    PluralCategory::Few => Some(PluralElementsKeys::Few),
                    PluralCategory::Many => Some(PluralElementsKeys::Many),
                    PluralCategory::Other => None,
                }
                .and_then(|key| get_special(specials, key))
            }
            None => None,
        }
        .unwrap_or(parts.default)
    }

    /// Recovers the [`PluralElements`] corresponding to this packed structure.
    #[cfg(feature = "datagen")]
    pub fn decode(&self) -> PluralElements<(FourBitMetadata, &V)> {
        PluralElements(PluralElementsInner::from_packed(self))
    }

    /// Returns the value for the default ("other") plural variant.
    pub fn get_default(&self) -> (FourBitMetadata, &V) {
        self.as_parts().default
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[zerovec::make_ule(PluralCategoryULE)]
#[repr(u8)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
enum PluralElementsKeys {
    Zero = 0,
    One = 1,
    Two = 2,
    Few = 3,
    Many = 4,
    ExplicitZero = 5,
    ExplicitOne = 6,
}

impl<T> PluralElementsInner<T>
where
    T: PartialEq,
{
    fn get_specials_tuples(&self) -> impl Iterator<Item = (PluralElementsKeys, &T)> {
        [
            self.zero
                .as_ref()
                .filter(|&p| *p != self.other)
                .map(|s| (PluralElementsKeys::Zero, s)),
            self.one
                .as_ref()
                .filter(|&p| *p != self.other)
                .map(|s| (PluralElementsKeys::One, s)),
            self.two
                .as_ref()
                .filter(|&p| *p != self.other)
                .map(|s| (PluralElementsKeys::Two, s)),
            self.few
                .as_ref()
                .filter(|&p| *p != self.other)
                .map(|s| (PluralElementsKeys::Few, s)),
            self.many
                .as_ref()
                .filter(|&p| *p != self.other)
                .map(|s| (PluralElementsKeys::Many, s)),
            self.explicit_zero
                .as_ref()
                .filter(|&p| *p != self.other)
                .map(|s| (PluralElementsKeys::ExplicitZero, s)),
            self.explicit_one
                .as_ref()
                .filter(|&p| *p != self.other)
                .map(|s| (PluralElementsKeys::ExplicitOne, s)),
        ]
        .into_iter()
        .flatten()
    }
}

/// Four bits of metadata that are stored and retrieved with the plural elements.
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[repr(transparent)]
pub struct FourBitMetadata(u8);

impl FourBitMetadata {
    /// Creates a [`FourBitMetadata`] if the given value fits in 4 bits.
    pub fn try_from_byte(byte: u8) -> Option<Self> {
        if byte <= 0x0F {
            Some(Self(byte))
        } else {
            None
        }
    }

    /// Creates a [`FourBitMetadata`] with a zero value.
    pub const fn zero() -> Self {
        Self(0)
    }

    /// Gets the value out of a [`FourBitMetadata`].
    pub const fn get(self) -> u8 {
        self.0
    }
}

/// A pair of [`PluralElementsKeys`] and [`FourBitMetadata`].
#[derive(Debug, Copy, Clone)]
struct PluralCategoryAndMetadata {
    pub plural_category: PluralElementsKeys,
    pub metadata: FourBitMetadata,
}

struct PluralCategoryAndMetadataUnpacked {
    pub plural_category_byte: u8,
    pub metadata_byte: u8,
}

/// Bitpacked struct for [`PluralCategoryAndMetadata`].
#[derive(Debug, Copy, Clone)]
#[repr(transparent)]
struct PluralCategoryAndMetadataPackedULE(
    /// Representation: `ppppmmmm`
    /// - `pppp` are a valid [`PluralElementsKeys`]
    /// - `mmmm` are a valid [`FourBitMetadata`]
    ///
    /// The valid values are determined by their respective types.
    u8,
);

impl From<PluralCategoryAndMetadata> for PluralCategoryAndMetadataPackedULE {
    fn from(value: PluralCategoryAndMetadata) -> Self {
        let byte = ((value.plural_category as u8) << 4) | value.metadata.get();
        debug_assert!(
            PluralCategoryAndMetadata::try_from_unpacked(Self::unpack_byte(byte)).is_some()
        );
        Self(byte)
    }
}

// # Safety
//
// Safety checklist for `ULE`:
//
// 1. The type is a single byte, not padding.
// 2. The type is a single byte, so it has align(1).
// 3. `validate_bytes` checks the validity of every byte.
// 4. `validate_bytes` checks the validity of every byte.
// 5. All other methods are be left with their default impl.
// 6. The represented enums implement Eq by byte equality.
unsafe impl ULE for PluralCategoryAndMetadataPackedULE {
    fn validate_bytes(bytes: &[u8]) -> Result<(), UleError> {
        bytes
            .iter()
            .all(|byte| {
                let unpacked = Self::unpack_byte(*byte);
                PluralCategoryAndMetadata::try_from_unpacked(unpacked).is_some()
            })
            .then_some(())
            .ok_or_else(UleError::parse::<Self>)
    }
}

impl PluralCategoryAndMetadataPackedULE {
    fn unpack_byte(byte: u8) -> PluralCategoryAndMetadataUnpacked {
        let plural_category_byte = (byte & 0xF0) >> 4;
        let metadata_byte = byte & 0x0F;
        PluralCategoryAndMetadataUnpacked {
            plural_category_byte,
            metadata_byte,
        }
    }

    fn get(self) -> PluralCategoryAndMetadata {
        let unpacked = Self::unpack_byte(self.0);
        // Safety: by invariant, `self.0` contains valid values for PluralCategoryAndMetadata
        unsafe { PluralCategoryAndMetadata::try_from_unpacked(unpacked).unwrap_unchecked() }
    }
}

impl PluralCategoryAndMetadata {
    fn try_from_unpacked(unpacked: PluralCategoryAndMetadataUnpacked) -> Option<Self> {
        let plural_category = PluralElementsKeys::new_from_u8(unpacked.plural_category_byte)?;
        let metadata = FourBitMetadata::try_from_byte(unpacked.metadata_byte)?;
        Some(Self {
            plural_category,
            metadata,
        })
    }
}

impl AsULE for PluralCategoryAndMetadata {
    type ULE = PluralCategoryAndMetadataPackedULE;
    #[inline]
    fn to_unaligned(self) -> Self::ULE {
        PluralCategoryAndMetadataPackedULE::from(self)
    }
    #[inline]
    fn from_unaligned(unaligned: Self::ULE) -> Self {
        unaligned.get()
    }
}

/// The type of the special patterns list.
type PluralElementsTupleSliceVarULE<V> = VarZeroSlice<VarTupleULE<PluralCategoryAndMetadata, V>>;

/// The type of the default value.
type PluralElementWithMetadata<'a, T> = (FourBitMetadata, &'a T);

/// Internal intermediate type that can be converted into a [`PluralElementsPackedULE`].
struct PluralElementsPackedBuilder<'a, T> {
    pub default: PluralElementWithMetadata<'a, T>,
    pub specials: Option<Vec<VarTuple<PluralCategoryAndMetadata, &'a T>>>,
}

/// Internal unpacked and deserialized values from a [`PluralElementsPackedULE`].
#[derive(Debug)]
struct PluralElementsUnpacked<'a, V: VarULE + ?Sized> {
    pub default: PluralElementWithMetadata<'a, V>,
    pub specials: Option<&'a PluralElementsTupleSliceVarULE<V>>,
}

/// Internal unpacked bytes from a [`PluralElementsPackedULE`].
struct PluralElementsUnpackedBytes<'a> {
    pub lead_byte: u8,
    pub v_bytes: &'a [u8],
    pub specials_bytes: Option<&'a [u8]>,
}

/// Helper function to access a value from [`PluralElementsTupleSliceVarULE`]
fn get_special<V: VarULE + ?Sized>(
    data: &PluralElementsTupleSliceVarULE<V>,
    key: PluralElementsKeys,
) -> Option<(FourBitMetadata, &V)> {
    data.iter()
        .filter_map(|ule| {
            let PluralCategoryAndMetadata {
                plural_category,
                metadata,
            } = ule.sized.get();
            (plural_category == key).then_some((metadata, &ule.variable))
        })
        .next()
}

impl<T> PluralElementsInner<(FourBitMetadata, T)>
where
    T: PartialEq,
{
    fn to_packed_builder<'a, V>(&'a self) -> PluralElementsPackedBuilder<'a, T>
    where
        &'a T: EncodeAsVarULE<V>,
        V: VarULE + ?Sized,
    {
        let specials = self
            .get_specials_tuples()
            .map(|(plural_category, (metadata, t))| VarTuple {
                sized: PluralCategoryAndMetadata {
                    plural_category,
                    metadata: *metadata,
                },
                variable: t,
            })
            .collect::<Vec<_>>();
        PluralElementsPackedBuilder {
            default: (self.other.0, &self.other.1),
            specials: if specials.is_empty() {
                None
            } else {
                Some(specials)
            },
        }
    }
}

unsafe impl<T, V> EncodeAsVarULE<PluralElementsPackedULE<V>>
    for PluralElements<(FourBitMetadata, T)>
where
    T: PartialEq + fmt::Debug,
    for<'a> &'a T: EncodeAsVarULE<V>,
    V: VarULE + ?Sized,
{
    fn encode_var_ule_as_slices<R>(&self, _cb: impl FnOnce(&[&[u8]]) -> R) -> R {
        // unnecessary if the other two are implemented
        unreachable!()
    }

    fn encode_var_ule_len(&self) -> usize {
        let builder = self.0.to_packed_builder();
        1 + builder.default.1.encode_var_ule_len()
            + match builder.specials {
                Some(specials) => {
                    1 + EncodeAsVarULE::<PluralElementsTupleSliceVarULE<V>>::encode_var_ule_len(
                        &specials,
                    )
                }
                None => 0,
            }
    }

    fn encode_var_ule_write(&self, dst: &mut [u8]) {
        let builder = self.0.to_packed_builder();
        #[expect(clippy::unwrap_used)] // by trait invariant
        let (lead_byte, remainder) = dst.split_first_mut().unwrap();
        *lead_byte = builder.default.0.get();
        if let Some(specials) = builder.specials {
            *lead_byte |= 0x80;
            #[expect(clippy::unwrap_used)] // by trait invariant
            let (second_byte, remainder) = remainder.split_first_mut().unwrap();
            *second_byte = match u8::try_from(builder.default.1.encode_var_ule_len()) {
                Ok(x) => x,
                // TODO: Inform the user more nicely that their data doesn't fit in our packed structure
                #[expect(clippy::panic)] // for now okay since it is mostly only during datagen
                Err(_) => {
                    panic!("other value too long to be packed: {self:?}")
                }
            };
            #[expect(clippy::unwrap_used)] // for now okay since it is mostly only during datagen
            let (v_bytes, specials_bytes) = remainder
                .split_at_mut_checked(*second_byte as usize)
                .unwrap();
            builder.default.1.encode_var_ule_write(v_bytes);
            EncodeAsVarULE::<PluralElementsTupleSliceVarULE<V>>::encode_var_ule_write(
                &specials,
                specials_bytes,
            );
        } else {
            builder.default.1.encode_var_ule_write(remainder)
        };
    }
}

#[cfg(feature = "datagen")]
impl<'a, V> PluralElementsInner<(FourBitMetadata, &'a V)>
where
    V: VarULE + ?Sized,
{
    fn from_packed(packed: &'a PluralElementsPackedULE<V>) -> Self {
        let parts = packed.as_parts();
        PluralElementsInner {
            other: parts.default,
            zero: parts
                .specials
                .and_then(|specials| get_special(specials, PluralElementsKeys::Zero)),
            one: parts
                .specials
                .and_then(|specials| get_special(specials, PluralElementsKeys::One)),
            two: parts
                .specials
                .and_then(|specials| get_special(specials, PluralElementsKeys::Two)),
            few: parts
                .specials
                .and_then(|specials| get_special(specials, PluralElementsKeys::Few)),
            many: parts
                .specials
                .and_then(|specials| get_special(specials, PluralElementsKeys::Many)),
            explicit_zero: parts
                .specials
                .and_then(|specials| get_special(specials, PluralElementsKeys::ExplicitZero)),
            explicit_one: parts
                .specials
                .and_then(|specials| get_special(specials, PluralElementsKeys::ExplicitOne)),
        }
    }
}

#[cfg(feature = "serde")]
impl<T> PluralElementsInner<(FourBitMetadata, T)> {
    fn into_packed<V>(self) -> Box<PluralElementsPackedULE<V>>
    where
        T: PartialEq + fmt::Debug,
        for<'a> &'a T: EncodeAsVarULE<V>,
        V: VarULE + ?Sized,
    {
        zerovec::ule::encode_varule_to_box(&PluralElements(self))
    }
}

#[cfg(feature = "serde")]
impl<'de, 'data, V> serde::Deserialize<'de> for &'data PluralElementsPackedULE<V>
where
    'de: 'data,
    V: VarULE + ?Sized,
{
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        if deserializer.is_human_readable() {
            Err(serde::de::Error::custom(
                "&PluralElementsPackedULE cannot be deserialized from human-readable formats",
            ))
        } else {
            let bytes = <&[u8]>::deserialize(deserializer)?;
            PluralElementsPackedULE::<V>::parse_bytes(bytes).map_err(serde::de::Error::custom)
        }
    }
}

#[cfg(feature = "serde")]
impl<'de, V> serde::Deserialize<'de> for Box<PluralElementsPackedULE<V>>
where
    V: VarULE + ?Sized,
    Box<V>: serde::Deserialize<'de> + PartialEq + fmt::Debug,
{
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        if deserializer.is_human_readable() {
            let plural_elements: PluralElementsInner<(FourBitMetadata, Box<V>)> =
                PluralElementsInner::deserialize(deserializer)?;
            Ok(plural_elements.into_packed())
        } else {
            let bytes = <&[u8]>::deserialize(deserializer)?;
            PluralElementsPackedULE::<V>::parse_bytes(bytes)
                .map(|ule| ule.to_owned())
                .map_err(serde::de::Error::custom)
        }
    }
}

#[cfg(feature = "datagen")]
impl<V> serde::Serialize for PluralElementsPackedULE<V>
where
    V: PartialEq + serde::Serialize + VarULE + ?Sized,
{
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        if serializer.is_human_readable() {
            let plural_elements: PluralElementsInner<(FourBitMetadata, &V)> =
                PluralElementsInner::from_packed(self);
            plural_elements.serialize(serializer)
        } else {
            serializer.serialize_bytes(self.as_bytes())
        }
    }
}

#[cfg(feature = "datagen")]
impl<'a, V> databake::Bake for &'a PluralElementsPackedULE<V>
where
    &'a V: databake::Bake,
    V: VarULE + ?Sized,
{
    fn bake(&self, ctx: &databake::CrateEnv) -> databake::TokenStream {
        ctx.insert("icu_plurals");
        let bytes = (&self.bytes).bake(ctx);
        databake::quote! {
            // Safety: the bytes came directly from self.bytes on the previous line.
            unsafe { icu_plurals::provider::PluralElementsPackedULE::from_bytes_unchecked(#bytes) }
        }
    }
}

#[cfg(feature = "datagen")]
impl<'a, V> databake::BakeSize for &'a PluralElementsPackedULE<V>
where
    &'a V: databake::Bake,
    V: VarULE + ?Sized,
{
    fn borrows_size(&self) -> usize {
        self.bytes.len()
    }
}

/// Helper function to properly deserialize a `Cow<PluralElementsPackedULE<V>>`
///
/// Due to <https://github.com/rust-lang/rust/issues/130180>, you may need to qualify
/// `V` when invoking this, like so:
///
/// `#[serde(deserialize_with = "deserialize_plural_elements_packed_cow::<_, str>")]`
///
/// See <https://github.com/unicode-org/icu4x/pull/1556>
#[cfg(feature = "serde")]
fn deserialize_plural_elements_packed_cow<'de, 'data, D, V>(
    deserializer: D,
) -> Result<Cow<'data, PluralElementsPackedULE<V>>, D::Error>
where
    'de: 'data,
    D: serde::Deserializer<'de>,
    V: VarULE + ?Sized,
    Box<PluralElementsPackedULE<V>>: serde::Deserialize<'de>,
{
    use serde::Deserialize;
    if deserializer.is_human_readable() {
        let value = Box::<PluralElementsPackedULE<V>>::deserialize(deserializer)?;
        Ok(Cow::Owned(value))
    } else {
        let value = <&'de PluralElementsPackedULE<V>>::deserialize(deserializer)?;
        Ok(Cow::Borrowed(value))
    }
}

// Need a manual impl because the derive(Clone) impl bounds are wrong
impl<V> Clone for PluralElementsPackedCow<'_, V>
where
    V: VarULE + ?Sized,
{
    fn clone(&self) -> Self {
        Self {
            elements: self.elements.clone(),
        }
    }
}

impl<T, V> From<PluralElements<T>> for PluralElementsPackedCow<'static, V>
where
    V: VarULE + ?Sized,
    T: PartialEq + fmt::Debug,
    for<'a> &'a T: EncodeAsVarULE<V>,
{
    fn from(value: PluralElements<T>) -> Self {
        let elements =
            zerovec::ule::encode_varule_to_box(&value.map(|s| (FourBitMetadata::zero(), s)));
        Self {
            elements: Cow::Owned(elements),
        }
    }
}

impl<V> PluralElementsPackedCow<'_, V>
where
    V: VarULE + ?Sized,
{
    /// Returns the value for the given [`PluralOperands`] and [`PluralRules`].
    pub fn get<'a>(&'a self, op: PluralOperands, rules: &PluralRules) -> &'a V {
        self.elements.get(op, rules).1
    }
}

#[test]
fn test_serde_singleton_roundtrip() {
    let plural_elements = PluralElements::new((FourBitMetadata::zero(), "abc"));
    let ule = zerovec::ule::encode_varule_to_box(&plural_elements);

    let postcard_bytes = postcard::to_allocvec(&ule).unwrap();
    assert_eq!(
        postcard_bytes,
        &[
            4,    // Postcard header
            0x00, // Discriminant
            b'a', b'b', b'c', // String
        ]
    );

    let postcard_ule: Box<PluralElementsPackedULE<str>> =
        postcard::from_bytes(&postcard_bytes).unwrap();
    assert_eq!(ule, postcard_ule);

    let postcard_borrowed: &PluralElementsPackedULE<str> =
        postcard::from_bytes(&postcard_bytes).unwrap();
    assert_eq!(&*ule, postcard_borrowed);

    let postcard_cow: PluralElementsPackedCow<str> = postcard::from_bytes(&postcard_bytes).unwrap();
    assert_eq!(&*ule, &*postcard_cow.elements);
    assert!(matches!(postcard_cow.elements, Cow::Borrowed(_)));

    let json_str = serde_json::to_string(&ule).unwrap();
    let json_ule: Box<PluralElementsPackedULE<str>> = serde_json::from_str(&json_str).unwrap();
    assert_eq!(ule, json_ule);
}

#[test]
fn test_serde_nonsingleton_roundtrip() {
    let plural_elements = PluralElements::new((FourBitMetadata::zero(), "abc"))
        .with_one_value(Some((FourBitMetadata::zero(), "defg")));
    let ule = zerovec::ule::encode_varule_to_box(&plural_elements);

    let postcard_bytes = postcard::to_allocvec(&ule).unwrap();
    assert_eq!(
        postcard_bytes,
        &[
            12,   // Postcard header
            0x80, // Discriminant
            3, b'a', b'b', b'c', // String of length 3
            1, 0, // VarZeroVec of length 1
            0x10, b'd', b'e', b'f', b'g' // Plural category 1 and string "defg"
        ]
    );

    let postcard_ule: Box<PluralElementsPackedULE<str>> =
        postcard::from_bytes(&postcard_bytes).unwrap();
    assert_eq!(ule, postcard_ule);

    let postcard_borrowed: &PluralElementsPackedULE<str> =
        postcard::from_bytes(&postcard_bytes).unwrap();
    assert_eq!(&*ule, postcard_borrowed);

    let json_str = serde_json::to_string(&ule).unwrap();
    let json_ule: Box<PluralElementsPackedULE<str>> = serde_json::from_str(&json_str).unwrap();
    assert_eq!(ule, json_ule);
}
