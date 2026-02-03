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

use icu_provider::prelude::*;
use zerovec::ule::{AsULE, ULE};
use zerovec::ZeroVec;

icu_provider::data_marker!(
    /// Precomputed data for the Chinese calendar
    CalendarChineseV1,
    "calendar/chinese/v1",
    ChineseBasedCache<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// Precomputed data for the Dangi calendar
    CalendarDangiV1,
    "calendar/dangi/v1",
    ChineseBasedCache<'static>,
    is_singleton = true
);

/// Cached/precompiled data for a certain range of years for a chinese-based
/// calendar. Avoids the need to perform lunar calendar arithmetic for most calendrical
/// operations.
#[derive(Debug, PartialEq, Clone, Default, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_calendar::provider::chinese_based))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct ChineseBasedCache<'data> {
    /// The ISO year corresponding to the first data entry for this year
    pub first_related_iso_year: i32,
    /// A list of precomputed data for each year beginning with first_related_iso_year
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub data: ZeroVec<'data, PackedChineseBasedYearInfo>,
}

icu_provider::data_struct!(
    ChineseBasedCache<'_>,
    #[cfg(feature = "datagen")]
);

/// The struct containing compiled ChineseData
///
/// Bit structure (little endian: note that shifts go in the opposite direction!)
///
/// ```text
/// Bit:             0   1   2   3   4   5   6   7
/// Byte 0:          [  month lengths .............
/// Byte 1:         .. month lengths ] | [ leap month index ..
/// Byte 2:          ] | [   NY offset       ] | unused
/// ```
///
/// Where the New Year Offset is the offset from ISO Jan 21 of that year for Chinese New Year,
/// the month lengths are stored as 1 = 30, 0 = 29 for each month including the leap month.
/// The largest possible offset is 33, which requires 6 bits of storage.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord, ULE)]
#[cfg_attr(feature = "datagen", derive(databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_calendar::provider))]
#[repr(C, packed)]
pub struct PackedChineseBasedYearInfo(pub u8, pub u8, pub u8);

impl PackedChineseBasedYearInfo {
    /// The first day of the ISO year on which Chinese New Year may occur
    ///
    /// According to Reingold & Dershowitz, ch 19.6, Chinese New Year occurs on Jan 21 - Feb 21 inclusive.
    ///
    /// Chinese New Year in the year 30 AD is January 20 (30-01-20).
    ///
    /// We allow it to occur as early as January 19 which is the earliest the second new moon
    /// could occur after the Winter Solstice if the solstice is pinned to December 20.
    const FIRST_NY: i64 = 18;

    pub(crate) fn new(
        month_lengths: [bool; 13],
        leap_month_idx: Option<u8>,
        ny_offset: i64,
    ) -> Self {
        debug_assert!(
            !month_lengths[12] || leap_month_idx.is_some(),
            "Last month length should not be set for non-leap years"
        );
        let ny_offset = ny_offset - Self::FIRST_NY;
        debug_assert!(ny_offset >= 0, "Year offset too small to store");
        debug_assert!(ny_offset < 34, "Year offset too big to store");
        debug_assert!(
            leap_month_idx.map(|l| l <= 13).unwrap_or(true),
            "Leap month indices must be 1 <= i <= 13"
        );
        let mut all = 0u32; // last byte unused

        for (month, length_30) in month_lengths.iter().enumerate() {
            #[allow(clippy::indexing_slicing)]
            if *length_30 {
                all |= 1 << month as u32;
            }
        }
        let leap_month_idx = leap_month_idx.unwrap_or(0);
        all |= (leap_month_idx as u32) << (8 + 5);
        all |= (ny_offset as u32) << (16 + 1);
        let le = all.to_le_bytes();
        Self(le[0], le[1], le[2])
    }

    // Get the new year difference from the ISO new year
    pub(crate) fn ny_offset(self) -> u8 {
        Self::FIRST_NY as u8 + (self.2 >> 1)
    }

    pub(crate) fn leap_month(self) -> Option<u8> {
        let bits = (self.1 >> 5) + ((self.2 & 0b1) << 3);

        (bits != 0).then_some(bits)
    }

    // Whether a particular month has 30 days (month is 1-indexed)
    pub(crate) fn month_has_30_days(self, month: u8) -> bool {
        let months = u16::from_le_bytes([self.0, self.1]);
        months & (1 << (month - 1) as u16) != 0
    }

    #[cfg(any(test, feature = "datagen"))]
    pub(crate) fn month_lengths(self) -> [bool; 13] {
        core::array::from_fn(|i| self.month_has_30_days(i as u8 + 1))
    }

    // Which day of year is the last day of a month (month is 1-indexed)
    pub(crate) fn last_day_of_month(self, month: u8) -> u16 {
        let months = u16::from_le_bytes([self.0, self.1]);
        // month is 1-indexed, so `29 * month` includes the current month
        let mut prev_month_lengths = 29 * month as u16;
        // month is 1-indexed, so `1 << month` is a mask with all zeroes except
        // for a 1 at the bit index at the next month. Subtracting 1 from it gets us
        // a bitmask for all months up to now
        let long_month_bits = months & ((1 << month as u16) - 1);
        prev_month_lengths += long_month_bits.count_ones().try_into().unwrap_or(0);
        prev_month_lengths
    }
}

impl AsULE for PackedChineseBasedYearInfo {
    type ULE = Self;
    fn to_unaligned(self) -> Self {
        self
    }
    fn from_unaligned(other: Self) -> Self {
        other
    }
}

#[cfg(feature = "serde")]
mod serialization {
    use super::*;

    #[cfg(feature = "datagen")]
    use serde::{ser, Serialize};
    use serde::{Deserialize, Deserializer};

    #[derive(Deserialize)]
    #[cfg_attr(feature = "datagen", derive(Serialize))]
    struct SerdePackedChineseBasedYearInfo {
        ny_offset: u8,
        month_has_30_days: [bool; 13],
        leap_month_idx: Option<u8>,
    }

    impl<'de> Deserialize<'de> for PackedChineseBasedYearInfo {
        fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
        where
            D: Deserializer<'de>,
        {
            if deserializer.is_human_readable() {
                SerdePackedChineseBasedYearInfo::deserialize(deserializer).map(Into::into)
            } else {
                let data = <(u8, u8, u8)>::deserialize(deserializer)?;
                Ok(PackedChineseBasedYearInfo(data.0, data.1, data.2))
            }
        }
    }

    #[cfg(feature = "datagen")]
    impl Serialize for PackedChineseBasedYearInfo {
        fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
        where
            S: ser::Serializer,
        {
            if serializer.is_human_readable() {
                SerdePackedChineseBasedYearInfo::from(*self).serialize(serializer)
            } else {
                (self.0, self.1, self.2).serialize(serializer)
            }
        }
    }

    #[cfg(feature = "datagen")]
    impl From<PackedChineseBasedYearInfo> for SerdePackedChineseBasedYearInfo {
        fn from(other: PackedChineseBasedYearInfo) -> Self {
            Self {
                ny_offset: other.ny_offset(),
                month_has_30_days: other.month_lengths(),
                leap_month_idx: other.leap_month(),
            }
        }
    }

    impl From<SerdePackedChineseBasedYearInfo> for PackedChineseBasedYearInfo {
        fn from(other: SerdePackedChineseBasedYearInfo) -> Self {
            Self::new(
                other.month_has_30_days,
                other.leap_month_idx,
                other.ny_offset as i64,
            )
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;

    fn packed_roundtrip_single(
        mut month_lengths: [bool; 13],
        leap_month_idx: Option<u8>,
        ny_offset: i64,
    ) {
        if leap_month_idx.is_none() {
            // Avoid bad invariants
            month_lengths[12] = false;
        }
        let packed = PackedChineseBasedYearInfo::new(month_lengths, leap_month_idx, ny_offset);

        assert_eq!(
            ny_offset,
            packed.ny_offset() as i64,
            "Roundtrip with {month_lengths:?}, {leap_month_idx:?}, {ny_offset}"
        );
        assert_eq!(
            leap_month_idx,
            packed.leap_month(),
            "Roundtrip with {month_lengths:?}, {leap_month_idx:?}, {ny_offset}"
        );
        let month_lengths_roundtrip = packed.month_lengths();
        assert_eq!(
            month_lengths, month_lengths_roundtrip,
            "Roundtrip with {month_lengths:?}, {leap_month_idx:?}, {ny_offset}"
        );
    }

    #[test]
    fn test_roundtrip_packed() {
        const SHORT: [bool; 13] = [false; 13];
        const LONG: [bool; 13] = [true; 13];
        const ALTERNATING1: [bool; 13] = [
            false, true, false, true, false, true, false, true, false, true, false, true, false,
        ];
        const ALTERNATING2: [bool; 13] = [
            true, false, true, false, true, false, true, false, true, false, true, false, true,
        ];
        const RANDOM1: [bool; 13] = [
            true, true, false, false, true, true, false, true, true, true, true, false, true,
        ];
        const RANDOM2: [bool; 13] = [
            false, true, true, true, true, false, true, true, true, false, false, true, false,
        ];
        packed_roundtrip_single(SHORT, None, 18 + 5);
        packed_roundtrip_single(SHORT, None, 18 + 10);
        packed_roundtrip_single(SHORT, Some(11), 18 + 15);
        packed_roundtrip_single(LONG, Some(12), 18 + 15);
        packed_roundtrip_single(ALTERNATING1, None, 18 + 2);
        packed_roundtrip_single(ALTERNATING1, Some(3), 18 + 5);
        packed_roundtrip_single(ALTERNATING2, None, 18 + 9);
        packed_roundtrip_single(ALTERNATING2, Some(7), 18 + 26);
        packed_roundtrip_single(RANDOM1, None, 18 + 29);
        packed_roundtrip_single(RANDOM1, Some(12), 18 + 29);
        packed_roundtrip_single(RANDOM1, Some(2), 18 + 21);
        packed_roundtrip_single(RANDOM2, None, 18 + 25);
        packed_roundtrip_single(RANDOM2, Some(2), 18 + 19);
        packed_roundtrip_single(RANDOM2, Some(5), 18 + 2);
        packed_roundtrip_single(RANDOM2, Some(12), 18 + 5);
    }
}
