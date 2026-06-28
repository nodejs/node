// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! 🚧 \[Unstable\] Data provider struct definitions for this ICU4X component.
//!
//! <div class="stab unstable">
//! 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
//! including in SemVer minor releases. While the serde representation of data structs is guaranteed
//! to be stable, their Rust representation might not be. Use with caution.
//! </div>
//!
//! Read more about data providers: [`icu_provider`]
//!
//! # Examples
//!
//! ## Get the resolved numbering system
//!
//! In a constructor call, the _last_ request for [`DecimalDigitsV1`]
//! contains the resolved numbering system as its attribute:
//!
//! ```
//! use icu::decimal::provider::DecimalDigitsV1;
//! use icu::decimal::DecimalFormatter;
//! use icu::locale::locale;
//! use icu_provider::prelude::*;
//! use std::any::TypeId;
//! use std::cell::RefCell;
//!
//! struct NumberingSystemInspectionProvider<P> {
//!     inner: P,
//!     numbering_system: RefCell<Option<Box<DataMarkerAttributes>>>,
//! }
//!
//! impl<M, P> DataProvider<M> for NumberingSystemInspectionProvider<P>
//! where
//!     M: DataMarker,
//!     P: DataProvider<M>,
//! {
//!     fn load(&self, req: DataRequest) -> Result<DataResponse<M>, DataError> {
//!         if TypeId::of::<M>() == TypeId::of::<DecimalDigitsV1>() {
//!             *self.numbering_system.try_borrow_mut().unwrap() =
//!                 Some(req.id.marker_attributes.to_owned());
//!         }
//!         self.inner.load(req)
//!     }
//! }
//!
//! let provider = NumberingSystemInspectionProvider {
//!     inner: icu::decimal::provider::Baked,
//!     numbering_system: RefCell::new(None),
//! };
//!
//! let formatter = DecimalFormatter::try_new_unstable(
//!     &provider,
//!     locale!("th").into(),
//!     Default::default(),
//! )
//! .unwrap();
//!
//! assert_eq!(
//!     provider
//!         .numbering_system
//!         .borrow()
//!         .as_ref()
//!         .map(|x| x.as_str()),
//!     Some("latn")
//! );
//!
//! let formatter = DecimalFormatter::try_new_unstable(
//!     &provider,
//!     locale!("th-u-nu-thai").into(),
//!     Default::default(),
//! )
//! .unwrap();
//!
//! assert_eq!(
//!     provider
//!         .numbering_system
//!         .borrow()
//!         .as_ref()
//!         .map(|x| x.as_str()),
//!     Some("thai")
//! );
//!
//! let formatter = DecimalFormatter::try_new_unstable(
//!     &provider,
//!     locale!("th-u-nu-adlm").into(),
//!     Default::default(),
//! )
//! .unwrap();
//!
//! assert_eq!(
//!     provider
//!         .numbering_system
//!         .borrow()
//!         .as_ref()
//!         .map(|x| x.as_str()),
//!     Some("adlm")
//! );
//! ```

// Provider structs must be stable
#![allow(clippy::exhaustive_structs)]
#![allow(clippy::exhaustive_enums)]

#[cfg(any(feature = "datagen", feature = "serde"))]
#[cfg(feature = "unstable")]
use alloc::boxed::Box;
#[cfg(feature = "datagen")]
#[cfg(feature = "unstable")]
use alloc::vec::Vec;
#[cfg(feature = "unstable")]
use icu_pattern::{Pattern, PatternBackend, SinglePlaceholder};
#[cfg(feature = "unstable")]
use icu_plurals::provider::PluralElementsPackedULE;
use icu_provider::prelude::*;
#[cfg(feature = "unstable")]
use zerovec::ule::vartuple::VarTupleULE;
use zerovec::VarZeroCow;
#[cfg(feature = "unstable")]
use zerovec::VarZeroVec;

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
    use icu_decimal_data::*;
    pub mod icu {
        pub use crate as decimal;
        pub use icu_locale as locale;
    }
    make_provider!(Baked);
    #[cfg(feature = "unstable")]
    impl_decimal_compact_long_v1!(Baked);
    #[cfg(feature = "unstable")]
    impl_decimal_compact_short_v1!(Baked);
    impl_decimal_symbols_v1!(Baked);
    impl_decimal_digits_v1!(Baked);
};

icu_provider::data_marker!(
    /// Data marker for decimal symbols
    DecimalSymbolsV1,
    "decimal/symbols/v1",
    DecimalSymbols<'static>,
);

icu_provider::data_marker!(
    /// The digits for a given numbering system. This data ought to be stored in the `und` locale with a marker attribute
    /// set to the numbering system code.
    DecimalDigitsV1,
    "decimal/digits/v1",
    [char; 10],
    #[cfg(feature = "datagen")]
    attributes_domain = "numbering_system"
);

#[cfg(feature = "datagen")]
/// The latest minimum set of markers required by this component.
#[cfg(feature = "unstable")]
pub const MARKERS: &[DataMarkerInfo] = &[
    DecimalSymbolsV1::INFO,
    DecimalDigitsV1::INFO,
    DecimalCompactLongV1::INFO,
    DecimalCompactShortV1::INFO,
];

#[cfg(feature = "datagen")]
/// The latest minimum set of markers required by this component.
#[cfg(not(feature = "unstable"))]
pub const MARKERS: &[DataMarkerInfo] = &[DecimalSymbolsV1::INFO, DecimalDigitsV1::INFO];

/// A collection of settings expressing where to put grouping separators in a decimal number.
/// For example, `1,000,000` has two grouping separators, positioned along every 3 digits.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Debug, PartialEq, Clone, yoke::Yokeable, Copy, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_decimal::provider))]
pub struct GroupingSizes {
    /// The size of the first (lowest-magnitude) group.
    ///
    /// If 0, grouping separators will never be shown.
    pub primary: u8,

    /// The size of groups after the first group.
    ///
    /// If 0, defaults to be the same as `primary`.
    pub secondary: u8,

    /// The minimum number of digits required before the first group. For example, if `primary=3`
    /// and `min_grouping=2`, grouping separators will be present on 10,000 and above.
    pub min_grouping: u8,
}

/// A stack representation of the strings used in [`DecimalSymbols`], i.e. a builder type
/// for [`DecimalSymbolsStrs`]. This type can be obtained from a [`DecimalSymbolsStrs`]
/// the `From`/`Into` traits.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Debug, PartialEq, Clone, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize))]
#[zerovec::make_varule(DecimalSymbolsStrs)]
#[zerovec::derive(Debug)]
#[zerovec::skip_derive(Ord)]
#[cfg_attr(not(feature = "alloc"), zerovec::skip_derive(ZeroMapKV, ToOwned))]
#[cfg_attr(feature = "serde", zerovec::derive(Deserialize))]
#[cfg_attr(feature = "datagen", zerovec::derive(Serialize))]
// Each affix/separator is at most three characters, which tends to be around 3-12 bytes each
// and the numbering system is at most 8 ascii bytes, All put together the indexing is extremely
// unlikely to have to go past 256.
#[zerovec::format(zerovec::vecs::Index8)]
pub struct DecimalSymbolStrsBuilder<'data> {
    /// Prefix to apply when a negative sign is needed.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub minus_sign_prefix: VarZeroCow<'data, str>,
    /// Suffix to apply when a negative sign is needed.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub minus_sign_suffix: VarZeroCow<'data, str>,

    /// Prefix to apply when a positive sign is needed.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub plus_sign_prefix: VarZeroCow<'data, str>,
    /// Suffix to apply when a positive sign is needed.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub plus_sign_suffix: VarZeroCow<'data, str>,

    /// Character used to separate the integer and fraction parts of the number.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub decimal_separator: VarZeroCow<'data, str>,

    /// Character used to separate groups in the integer part of the number.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub grouping_separator: VarZeroCow<'data, str>,

    /// The numbering system to use.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub numsys: VarZeroCow<'data, str>,
}

#[cfg(feature = "alloc")]
impl DecimalSymbolStrsBuilder<'_> {
    /// Build a [`DecimalSymbolsStrs`]
    ///
    /// ✨ *Enabled with the `alloc` Cargo feature.*
    pub fn build(&self) -> VarZeroCow<'static, DecimalSymbolsStrs> {
        VarZeroCow::from_encodeable(self)
    }
}

/// Symbols and metadata required for formatting a [`Decimal`](crate::input::Decimal).
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Debug, PartialEq, Clone, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_decimal::provider))]
pub struct DecimalSymbols<'data> {
    /// String data for the symbols: +/- affixes and separators
    #[cfg_attr(feature = "serde", serde(borrow))]
    // We use a VarZeroCow here to reduce the stack size of DecimalSymbols: instead of serializing multiple strs,
    // this type will now serialize as a single u8 buffer with optimized indexing that packs all the data together
    pub strings: VarZeroCow<'data, DecimalSymbolsStrs>,

    /// Settings used to determine where to place groups in the integer part of the number.
    pub grouping_sizes: GroupingSizes,
}

icu_provider::data_struct!(
    DecimalSymbols<'_>,
    #[cfg(feature = "datagen")]
);

impl<'a> core::ops::Deref for DecimalSymbols<'a> {
    type Target = VarZeroCow<'a, DecimalSymbolsStrs>;

    fn deref(&self) -> &Self::Target {
        &self.strings
    }
}

impl DecimalSymbols<'static> {
    /// Create a new en-US format for use in testing
    #[cfg(feature = "datagen")]
    pub fn new_en_for_testing() -> Self {
        let strings = DecimalSymbolStrsBuilder {
            minus_sign_prefix: VarZeroCow::new_borrowed("-"),
            minus_sign_suffix: VarZeroCow::new_borrowed(""),
            plus_sign_prefix: VarZeroCow::new_borrowed("+"),
            plus_sign_suffix: VarZeroCow::new_borrowed(""),
            decimal_separator: VarZeroCow::new_borrowed("."),
            grouping_separator: VarZeroCow::new_borrowed(","),
            numsys: VarZeroCow::new_borrowed("latn"),
        };
        Self {
            strings: VarZeroCow::from_encodeable(&strings),
            grouping_sizes: GroupingSizes {
                primary: 3,
                secondary: 3,
                min_grouping: 1,
            },
        }
    }
}

#[cfg(feature = "unstable")]
icu_provider::data_marker!(
    /// `DecimalCompactLongV1`
    DecimalCompactLongV1,
    CompactPatterns<'static, SinglePlaceholder>,
);
#[cfg(feature = "unstable")]
icu_provider::data_marker!(
    /// `DecimalCompactShortV1`
    DecimalCompactShortV1,
    CompactPatterns<'static, SinglePlaceholder>,
);

/// Compact pattern data struct.
///
/// As in CLDR, this is a mapping from type (a power of ten, corresponding to
/// the magnitude of the number being formatted) to a plural pattern.
///
/// If all plural patterns are compatible across consecutive types, the
/// larger types are omitted, thus given
/// > (3, other) ↦ 0K, (4, other) ↦ 00K, (5, other) ↦ 000K
///
/// only
/// > (3, other) ↦ 0K
///
/// is stored.
///
/// Finally, the pattern indicating noncompact notation for the first few powers
/// of ten might be omitted; that is, there is an implicit (0, other) ↦ 0.
///
/// The plural patterns are stored with the 4-bit metadata representing the exponent
/// shift (number of zeros in the pattern minus 1).
#[derive(Debug, Clone, PartialEq, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "datagen", derive(databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_decimal::provider))]
#[cfg(feature = "unstable")]
pub struct CompactPatterns<'a, P: PatternBackend>(
    pub VarZeroVec<'a, VarTupleULE<u8, PluralElementsPackedULE<Pattern<P>>>>,
);

#[cfg(feature = "datagen")]
#[cfg(feature = "unstable")]
impl<'data, P: PatternBackend> serde::Serialize for CompactPatterns<'data, P>
where
    Pattern<P>: serde::Serialize,
{
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        self.0.serialize(serializer)
    }
}

#[cfg(feature = "serde")]
#[cfg(feature = "unstable")]
impl<'de, 'data, P: PatternBackend> serde::Deserialize<'de> for CompactPatterns<'data, P>
where
    'de: 'data,
    Box<Pattern<P>>: serde::Deserialize<'de>,
{
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        VarZeroVec::<VarTupleULE<u8, PluralElementsPackedULE<Pattern<P>>>>::deserialize(
            deserializer,
        )
        .map(Self)
    }
}

#[cfg(feature = "unstable")]
impl<P: PatternBackend> icu_provider::ule::MaybeAsVarULE for CompactPatterns<'_, P> {
    type EncodedStruct = [()];
}

#[cfg(feature = "datagen")]
#[cfg(feature = "unstable")]
impl<P: PatternBackend> icu_provider::ule::MaybeEncodeAsVarULE for CompactPatterns<'_, P> {
    type EncodeableStruct<'b>
        = &'b [()]
    where
        Self: 'b;
    fn maybe_as_encodeable<'b>(&'b self) -> Option<Self::EncodeableStruct<'b>> {
        None
    }
}

#[cfg(feature = "datagen")]
#[cfg(feature = "unstable")]
impl<P: PatternBackend> CompactPatterns<'static, P> {
    /// Creates a new [`CompactPatterns`] from a map of patterns.
    /// The values contains an additional `u8` that contains the
    /// magnitude of the pattern, which can be different from the
    /// magnitude key (e.g. for the maginute 5 there might be a
    /// magnitude 3 pattern).
    #[allow(clippy::type_complexity)]
    pub fn new(
        patterns: alloc::collections::BTreeMap<
            u8,
            (u8, icu_plurals::PluralElements<Box<Pattern<P>>>),
        >,
        zero_magnitude: Option<&icu_plurals::PluralElements<&Pattern<P>>>,
    ) -> Result<Self, DataError> {
        use icu_plurals::provider::FourBitMetadata;
        use icu_plurals::PluralElements;
        use zerovec::ule::encode_varule_to_box;
        use zerovec::ule::vartuple::VarTuple;
        use zerovec::vecs::VarZeroVecOwned;

        if !patterns
            .values()
            .zip(patterns.values().skip(1))
            .all(|(low, high)| low.0 <= high.0)
        {
            Err(
                DataError::custom("Compact exponents should be nondecreasing").with_debug_context(
                    &patterns
                        .values()
                        .map(|(exponent, _)| exponent)
                        .collect::<Vec<_>>(),
                ),
            )?;
        }

        let mut deduplicated_patterns: Vec<(
            u8,
            PluralElements<(FourBitMetadata, Box<Pattern<P>>)>,
        )> = Vec::new();

        // Deduplicate sequences of types that have the same plural map, keeping the lowest type.
        for (log10_type, (exponent, map)) in patterns
            .into_iter()
            // Skip leading 0 patterns
            .skip_while(|(_, (_, pattern))| Some(&pattern.as_ref().map(|p| &**p)) == zero_magnitude)
        {
            if let Some(prev) = deduplicated_patterns.last() {
                // The higher pattern can never be exactly one of the low pattern, so we can ignore that value
                if prev
                    .1
                    .as_ref()
                    .with_explicit_one_value(None)
                    .map(|(_, p)| p)
                    == map.as_ref()
                {
                    continue;
                }
            }

            // Store the exponent as a difference from the log10_type, i.e. the number of zeros
            // in the pattern, minus 1. No pattern should have more than 16 zeros.
            let Some(metadata) = FourBitMetadata::try_from_byte(log10_type - exponent) else {
                return Err(DataError::custom("Pattern has too many zeros")
                    .with_debug_context(&(log10_type - exponent)));
            };

            deduplicated_patterns.push((log10_type, map.map(|p| (metadata, p))))
        }

        #[allow(clippy::unwrap_used)] // keyed by u8, so it cannot exceed usize/2
        Ok(Self(
            VarZeroVecOwned::try_from_elements(
                &deduplicated_patterns
                    .into_iter()
                    .map(|(log10_type, plural_map)| {
                        encode_varule_to_box(&VarTuple {
                            sized: log10_type,
                            variable: plural_map,
                        })
                    })
                    .collect::<Vec<Box<VarTupleULE<u8, PluralElementsPackedULE<Pattern<P>>>>>>(),
            )
            .unwrap()
            .into(),
        ))
    }
}

pub(crate) fn load_with_fallback<'a, M: DataMarker>(
    provider: &(impl DataProvider<M> + ?Sized),
    ids: impl Iterator<Item = DataIdentifierBorrowed<'a>>,
) -> Result<DataResponse<M>, DataError> {
    let mut ids = ids.peekable();

    while let Some(id) = ids.next() {
        if ids.peek().is_some() {
            if let Some(r) = provider
                .load(DataRequest {
                    id,
                    metadata: {
                        let mut m = DataRequestMetadata::default();
                        m.silent = true;
                        m
                    },
                })
                .allow_identifier_not_found()?
            {
                return Ok(r);
            }
        } else {
            return provider.load(DataRequest {
                id,
                metadata: DataRequestMetadata::default(),
            });
        }
    }

    Err(DataErrorKind::InvalidRequest.into_error())
}

impl crate::DecimalFormatterPreferences {
    pub(crate) fn nu_id<'a>(
        &'a self,
        locale: &'a DataLocale,
    ) -> Option<DataIdentifierBorrowed<'a>> {
        self.numbering_system
            .as_ref()
            .map(|s| s.as_str())
            .map(|nu| {
                DataIdentifierBorrowed::for_marker_attributes_and_locale(
                    DataMarkerAttributes::from_str_or_panic(nu),
                    locale,
                )
            })
    }
}
