// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! 🚧 \[Unstable\] Data provider struct definitions for chinese-based calendars.
//!
//! <div class="stab unstable">
//! 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
//! including in SemVer minor releases. While the serde representation of data structs is guaranteed
//! to be stable, their Rust representation might not be. Use with caution.
//! </div>
//!
//! Read more about data providers: [`icu_provider`]

use calendrical_calculations::rata_die::RataDie;
use icu_provider::prelude::*;
use zerovec::ule::AsULE;
use zerovec::ZeroVec;

icu_provider::data_marker!(
    /// Precomputed data for the Hijri obsevational calendar
    CalendarHijriSimulatedMeccaV1,
    "calendar/hijri/simulated/mecca/v1",
    HijriData<'static>,
    is_singleton = true,
);

/// Cached/precompiled data for a certain range of years for a chinese-based
/// calendar. Avoids the need to perform lunar calendar arithmetic for most calendrical
/// operations.
#[derive(Debug, PartialEq, Clone, Default, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_calendar::provider::hijri))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct HijriData<'data> {
    /// The extended year corresponding to the first data entry for this year
    pub first_extended_year: i32,
    /// A list of precomputed data for each year beginning with first_extended_year
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub data: ZeroVec<'data, PackedHijriYearInfo>,
}

icu_provider::data_struct!(
    HijriData<'_>,
    #[cfg(feature = "datagen")]
);

/// The struct containing compiled Hijri YearInfo
///
/// Bit structure
///
/// ```text
/// Bit:              F.........C  B.............0
/// Value:           [ start day ][ month lengths ]
/// ```
///
/// The start day is encoded as a signed offset from `Self::mean_synodic_start_day`. This number does not
/// appear to be less than 2, however we use all remaining bits for it in case of drift in the math.
/// The month lengths are stored as 1 = 30, 0 = 29 for each month including the leap month.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Copy, Clone, Hash, PartialEq, Eq, PartialOrd, Ord, Debug)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_calendar::provider))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct PackedHijriYearInfo(pub u16);

impl PackedHijriYearInfo {
    pub(crate) const fn new(
        extended_year: i32,
        month_lengths: [bool; 12],
        start_day: RataDie,
    ) -> Self {
        let start_offset = start_day.since(Self::mean_synodic_start_day(extended_year));

        debug_assert!(
            -8 < start_offset && start_offset < 8,
            "Year offset too big to store"
        );
        let start_offset = start_offset as i8;

        let mut all = 0u16; // last byte unused

        let mut i = 0;
        while i < 12 {
            #[allow(clippy::indexing_slicing)]
            if month_lengths[i] {
                all |= 1 << i;
            }
            i += 1;
        }

        if start_offset < 0 {
            all |= 1 << 12;
        }
        all |= (start_offset.unsigned_abs() as u16) << 13;
        Self(all)
    }

    pub(crate) fn unpack(self, extended_year: i32) -> ([bool; 12], RataDie) {
        let month_lengths = core::array::from_fn(|i| self.0 & (1 << (i as u8) as u16) != 0);
        let start_offset = if (self.0 & 0b1_0000_0000_0000) != 0 {
            -((self.0 >> 13) as i64)
        } else {
            (self.0 >> 13) as i64
        };
        (
            month_lengths,
            Self::mean_synodic_start_day(extended_year) + start_offset,
        )
    }

    const fn mean_synodic_start_day(extended_year: i32) -> RataDie {
        // -1 because the epoch is new year of year 1
        // truncating instead of flooring does not matter, as this is used for positive years only
        calendrical_calculations::islamic::ISLAMIC_EPOCH_FRIDAY.add(
            ((extended_year - 1) as f64 * calendrical_calculations::islamic::MEAN_YEAR_LENGTH)
                as i64,
        )
    }
}

impl AsULE for PackedHijriYearInfo {
    type ULE = <u16 as AsULE>::ULE;
    fn from_unaligned(unaligned: Self::ULE) -> Self {
        Self(<u16 as AsULE>::from_unaligned(unaligned))
    }
    fn to_unaligned(self) -> Self::ULE {
        <u16 as AsULE>::to_unaligned(self.0)
    }
}

#[test]
fn test_hijri_packed_roundtrip() {
    fn single_roundtrip(month_lengths: [bool; 12], year_start: RataDie) {
        let packed = PackedHijriYearInfo::new(1600, month_lengths, year_start);
        let (month_lengths2, year_start2) = packed.unpack(1600);
        assert_eq!(month_lengths, month_lengths2, "Month lengths must match for testcase {month_lengths:?} / {year_start:?}, with packed repr: {packed:?}");
        assert_eq!(year_start, year_start2, "Month lengths must match for testcase {month_lengths:?} / {year_start:?}, with packed repr: {packed:?}");
    }

    let l = true;
    let s = false;
    let all_short = [s; 12];
    let all_long = [l; 12];
    let mixed1 = [l, s, l, s, l, s, l, s, l, s, l, s];
    let mixed2 = [s, s, l, l, l, s, l, s, s, s, l, l];

    let start_1600 = PackedHijriYearInfo::mean_synodic_start_day(1600);
    single_roundtrip(all_short, start_1600);
    single_roundtrip(all_long, start_1600);
    single_roundtrip(mixed1, start_1600);
    single_roundtrip(mixed2, start_1600);

    single_roundtrip(mixed1, start_1600 - 7);
    single_roundtrip(mixed2, start_1600 + 7);
    single_roundtrip(mixed2, start_1600 + 4);
    single_roundtrip(mixed2, start_1600 + 1);
    single_roundtrip(mixed2, start_1600 - 1);
    single_roundtrip(mixed2, start_1600 - 4);
}
