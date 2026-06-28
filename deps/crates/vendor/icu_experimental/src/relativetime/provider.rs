// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

// Provider structs must be stable
#![allow(clippy::exhaustive_structs, clippy::exhaustive_enums)]

//! Data provider struct definitions for this ICU4X component.
//!
//! Read more about data providers: [`icu_provider`]

#[cfg(feature = "datagen")]
use core::fmt::Debug;
use icu_pattern::SinglePlaceholderPattern;
use icu_plurals::provider::PluralElementsPackedCow;
use icu_provider::prelude::*;
use zerovec::ZeroMap;

#[cfg(feature = "compiled_data")]
/// Baked data
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. In particular, the `DataProvider` implementations are only
/// guaranteed to match with this version's `*_unstable` providers. Use with caution.
/// </div>
pub use crate::provider::Baked;

icu_provider::data_marker!(
    /// `LongSecondRelativeV1`
    LongSecondRelativeV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `ShortSecondRelativeV1`
    ShortSecondRelativeV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `NarrowSecondRelativeV1`
    NarrowSecondRelativeV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `LongMinuteRelativeV1`
    LongMinuteRelativeV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `ShortMinuteRelativeV1`
    ShortMinuteRelativeV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `NarrowMinuteRelativeV1`
    NarrowMinuteRelativeV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `LongHourRelativeV1`
    LongHourRelativeV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `ShortHourRelativeV1`
    ShortHourRelativeV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `NarrowHourRelativeV1`
    NarrowHourRelativeV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `LongDayRelativeV1`
    LongDayRelativeV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `ShortDayRelativeV1`
    ShortDayRelativeV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `NarrowDayRelativeV1`
    NarrowDayRelativeV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `LongWeekRelativeV1`
    LongWeekRelativeV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `ShortWeekRelativeV1`
    ShortWeekRelativeV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `NarrowWeekRelativeV1`
    NarrowWeekRelativeV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `LongMonthRelativeV1`
    LongMonthRelativeV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `ShortMonthRelativeV1`
    ShortMonthRelativeV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `NarrowMonthRelativeV1`
    NarrowMonthRelativeV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `LongQuarterRelativeV1`
    LongQuarterRelativeV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `ShortQuarterRelativeV1`
    ShortQuarterRelativeV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `NarrowQuarterRelativeV1`
    NarrowQuarterRelativeV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `LongYearRelativeV1`
    LongYearRelativeV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `ShortYearRelativeV1`
    ShortYearRelativeV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `NarrowYearRelativeV1`
    NarrowYearRelativeV1,
    RelativeTimePatternData<'static>,
);

/// Relative time format  data struct.
#[derive(Debug, Clone, PartialEq, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_experimental::relativetime::provider))]
#[yoke(prove_covariance_manually)]
pub struct RelativeTimePatternData<'data> {
    /// Mapping for relative times with unique names.
    /// Example.
    /// In English, "-1" corresponds to "yesterday", "1" corresponds to "tomorrow".
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub relatives: ZeroMap<'data, i8, str>,
    /// How to display times in the past.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub past: PluralElementsPackedCow<'data, SinglePlaceholderPattern>,
    /// How to display times in the future.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub future: PluralElementsPackedCow<'data, SinglePlaceholderPattern>,
}

icu_provider::data_struct!(RelativeTimePatternData<'_>, #[cfg(feature = "datagen")]);
