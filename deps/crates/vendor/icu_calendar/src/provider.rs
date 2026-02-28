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

// Provider structs must be stable
#![allow(clippy::exhaustive_structs, clippy::exhaustive_enums)]

pub mod chinese_based;
pub mod hijri;
pub use chinese_based::{CalendarChineseV1, CalendarDangiV1};
pub use hijri::CalendarHijriSimulatedMeccaV1;

use crate::types::Weekday;
use icu_provider::fallback::{LocaleFallbackConfig, LocaleFallbackPriority};
use icu_provider::prelude::*;
use tinystr::TinyStr16;
use zerovec::ZeroVec;

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
    use icu_calendar_data::*;
    pub mod icu {
        pub use crate as calendar;
        pub use icu_locale as locale;
    }
    make_provider!(Baked);
    impl_calendar_chinese_v1!(Baked);
    impl_calendar_dangi_v1!(Baked);
    impl_calendar_hijri_simulated_mecca_v1!(Baked);
    impl_calendar_japanese_modern_v1!(Baked);
    impl_calendar_japanese_extended_v1!(Baked);
    impl_calendar_week_v1!(Baked);
};

icu_provider::data_marker!(
    /// Modern Japanese era names
    CalendarJapaneseModernV1,
    "calendar/japanese/modern/v1",
    JapaneseEras<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// Full Japanese era names
    CalendarJapaneseExtendedV1,
    "calendar/japanese/extended/v1",
    JapaneseEras<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// Week information
    CalendarWeekV1,
    "calendar/week/v1",
    WeekData,
    fallback_config = {
        let mut config = LocaleFallbackConfig::default();
        config.priority = LocaleFallbackPriority::Region;
        config
    },
);

#[cfg(feature = "datagen")]
/// The latest minimum set of markers required by this component.
pub const MARKERS: &[DataMarkerInfo] = &[
    CalendarChineseV1::INFO,
    CalendarDangiV1::INFO,
    CalendarHijriSimulatedMeccaV1::INFO,
    CalendarJapaneseModernV1::INFO,
    CalendarJapaneseExtendedV1::INFO,
    CalendarWeekV1::INFO,
];

/// The date at which an era started
///
/// The order of fields in this struct is important!
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[zerovec::make_ule(EraStartDateULE)]
#[derive(
    Copy, Clone, PartialEq, PartialOrd, Eq, Ord, Hash, Debug, yoke::Yokeable, zerofrom::ZeroFrom,
)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_calendar::provider))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct EraStartDate {
    /// The year the era started in
    pub year: i32,
    /// The month the era started in
    pub month: u8,
    /// The day the era started in
    pub day: u8,
}

/// A data structure containing the necessary era data for constructing a
/// [`Japanese`](crate::cal::Japanese) calendar object
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Debug, PartialEq, Clone, Default, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_calendar::provider))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct JapaneseEras<'data> {
    /// A map from era start dates to their era codes
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub dates_to_eras: ZeroVec<'data, (EraStartDate, TinyStr16)>,
}

icu_provider::data_struct!(
    JapaneseEras<'_>,
    #[cfg(feature = "datagen")]
);

/// An ICU4X mapping to a subset of CLDR weekData.
/// See CLDR-JSON's weekData.json for more context.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Clone, Copy, Debug, PartialEq, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_calendar::provider))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[allow(clippy::exhaustive_structs)] // used in data provider
pub struct WeekData {
    /// The first day of a week.
    pub first_weekday: Weekday,
    /// Bitset representing weekdays that are part of the 'weekend', for calendar purposes.
    /// The number of days can be different between locales, and may not be contiguous.
    pub weekend: WeekdaySet,
}

icu_provider::data_struct!(
    WeekData,
    #[cfg(feature = "datagen")]
);

/// Bitset representing weekdays.
//
// This Bitset uses an [u8] to represent the weekend, thus leaving one bit free.
// Each bit represents a day in the following order:
//
//   ┌▷Mon
//   │┌▷Tue
//   ││┌▷Wed
//   │││┌▷Thu
//   ││││ ┌▷Fri
//   ││││ │┌▷Sat
//   ││││ ││┌▷Sun
//   ││││ │││
// 0b0000_1010
//
// Please note that this is not a range, this are the discrete days representing a weekend. Other examples:
// 0b0101_1000 -> Tue, Thu, Fri
// 0b0000_0110 -> Sat, Sun
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct WeekdaySet(u8);

impl WeekdaySet {
    /// Returns whether the set contains the day.
    pub const fn contains(&self, day: Weekday) -> bool {
        self.0 & day.bit_value() != 0
    }
}

impl WeekdaySet {
    /// Creates a new [WeekdaySet] using the provided days.
    pub const fn new(days: &[Weekday]) -> Self {
        let mut i = 0;
        let mut w = 0;
        #[allow(clippy::indexing_slicing)]
        while i < days.len() {
            w |= days[i].bit_value();
            i += 1;
        }
        Self(w)
    }
}

impl Weekday {
    /// Defines the bit order used for encoding and reading weekend days.
    const fn bit_value(self) -> u8 {
        match self {
            Weekday::Monday => 1 << 6,
            Weekday::Tuesday => 1 << 5,
            Weekday::Wednesday => 1 << 4,
            Weekday::Thursday => 1 << 3,
            Weekday::Friday => 1 << 2,
            Weekday::Saturday => 1 << 1,
            Weekday::Sunday => 1 << 0,
        }
    }
}

#[cfg(feature = "datagen")]
impl databake::Bake for WeekdaySet {
    fn bake(&self, ctx: &databake::CrateEnv) -> databake::TokenStream {
        ctx.insert("icu_calendar");
        let days =
            crate::week::WeekdaySetIterator::new(Weekday::Monday, *self).map(|d| d.bake(ctx));
        databake::quote! {
            icu_calendar::provider::WeekdaySet::new(&[#(#days),*])
        }
    }
}

#[cfg(feature = "datagen")]
impl databake::BakeSize for WeekdaySet {
    fn borrows_size(&self) -> usize {
        0
    }
}

#[cfg(feature = "datagen")]
impl serde::Serialize for WeekdaySet {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        if serializer.is_human_readable() {
            use serde::ser::SerializeSeq;

            let mut seq = serializer.serialize_seq(None)?;
            for day in crate::week::WeekdaySetIterator::new(Weekday::Monday, *self) {
                seq.serialize_element(&day)?;
            }
            seq.end()
        } else {
            self.0.serialize(serializer)
        }
    }
}

#[cfg(feature = "serde")]
impl<'de> serde::Deserialize<'de> for WeekdaySet {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        if deserializer.is_human_readable() {
            use core::marker::PhantomData;

            struct Visitor<'de>(PhantomData<&'de ()>);
            impl<'de> serde::de::Visitor<'de> for Visitor<'de> {
                type Value = WeekdaySet;
                fn expecting(&self, f: &mut core::fmt::Formatter) -> core::fmt::Result {
                    core::write!(f, "a sequence of Weekdays")
                }
                fn visit_seq<A: serde::de::SeqAccess<'de>>(
                    self,
                    mut seq: A,
                ) -> Result<Self::Value, A::Error> {
                    let mut set = WeekdaySet::new(&[]);
                    while let Some(day) = seq.next_element::<Weekday>()? {
                        set.0 |= day.bit_value();
                    }
                    Ok(set)
                }
            }
            deserializer.deserialize_seq(Visitor(PhantomData))
        } else {
            u8::deserialize(deserializer).map(Self)
        }
    }
}

#[test]
fn test_weekdayset_bake() {
    databake::test_bake!(
        WeekdaySet,
        const,
        crate::provider::WeekdaySet::new(&[
            crate::types::Weekday::Monday,
            crate::types::Weekday::Wednesday,
            crate::types::Weekday::Friday
        ]),
        icu_calendar
    );
}

#[test]
fn test_weekdayset_new() {
    use Weekday::*;

    let sat_sun_bitmap = Saturday.bit_value() | Sunday.bit_value();
    let sat_sun_weekend = WeekdaySet::new(&[Saturday, Sunday]);
    assert_eq!(sat_sun_bitmap, sat_sun_weekend.0);

    let fri_sat_bitmap = Friday.bit_value() | Saturday.bit_value();
    let fri_sat_weekend = WeekdaySet::new(&[Friday, Saturday]);
    assert_eq!(fri_sat_bitmap, fri_sat_weekend.0);

    let fri_sun_bitmap = Friday.bit_value() | Sunday.bit_value();
    let fri_sun_weekend = WeekdaySet::new(&[Friday, Sunday]);
    assert_eq!(fri_sun_bitmap, fri_sun_weekend.0);

    let fri_bitmap = Friday.bit_value();
    let fri_weekend = WeekdaySet::new(&[Friday, Friday]);
    assert_eq!(fri_bitmap, fri_weekend.0);

    let sun_mon_bitmap = Sunday.bit_value() | Monday.bit_value();
    let sun_mon_weekend = WeekdaySet::new(&[Sunday, Monday]);
    assert_eq!(sun_mon_bitmap, sun_mon_weekend.0);

    let mon_sun_bitmap = Monday.bit_value() | Sunday.bit_value();
    let mon_sun_weekend = WeekdaySet::new(&[Monday, Sunday]);
    assert_eq!(mon_sun_bitmap, mon_sun_weekend.0);

    let mon_bitmap = Monday.bit_value();
    let mon_weekend = WeekdaySet::new(&[Monday]);
    assert_eq!(mon_bitmap, mon_weekend.0);
}
