//! Common logic shared across TZif providers

use crate::{
    provider::{GapEntryOffsets, UtcOffsetSeconds},
    utils,
};

use core::ops::Range;

#[cfg(feature = "tzif")]
use tzif::data::{
    posix::{DstTransitionInfo, PosixTzString, TransitionDate, TransitionDay},
    tzif::LocalTimeTypeRecord,
};

// TODO: Workshop record name?
/// The `LocalTimeRecord` result represents the result of searching for a
/// time zone transition without the offset seconds applied to the
/// epoch seconds.
///
/// As a result of the search, it is possible for the resulting search to be either
/// Empty (due to an invalid time being provided that would be in the +1 tz shift)
/// or two time zones (when a time exists in the ambiguous range of a -1 shift).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum LocalTimeRecordResult {
    Empty(GapEntryOffsets),
    Single(UtcOffsetSeconds),
    Ambiguous {
        first: UtcOffsetSeconds,
        second: UtcOffsetSeconds,
    },
}

/// `TimeZoneTransitionInfo` represents information about a timezone transition.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct TimeZoneTransitionInfo {
    /// The transition time epoch at which the offset needs to be applied.
    pub transition_epoch: Option<i64>,
    /// The time zone offset in seconds.
    pub offset: UtcOffsetSeconds,
}

impl From<UtcOffsetSeconds> for LocalTimeRecordResult {
    fn from(value: UtcOffsetSeconds) -> Self {
        Self::Single(value)
    }
}

#[cfg(feature = "tzif")]
impl From<LocalTimeTypeRecord> for LocalTimeRecordResult {
    fn from(value: LocalTimeTypeRecord) -> Self {
        Self::Single(value.into())
    }
}

#[cfg(feature = "tzif")]
impl From<(LocalTimeTypeRecord, LocalTimeTypeRecord)> for LocalTimeRecordResult {
    fn from(value: (LocalTimeTypeRecord, LocalTimeTypeRecord)) -> Self {
        Self::Ambiguous {
            first: value.0.into(),
            second: value.1.into(),
        }
    }
}

/// Stores the information about DST transitions for a given year
pub(crate) struct DstTransitionInfoForYear {
    pub(crate) dst_start_seconds: i64,
    pub(crate) dst_end_seconds: i64,
    pub(crate) std_offset: UtcOffsetSeconds,
    pub(crate) dst_offset: UtcOffsetSeconds,
}

impl DstTransitionInfoForYear {
    #[cfg(feature = "tzif")]
    pub(crate) fn compute(
        posix_tz_string: &PosixTzString,
        dst_variant: &DstTransitionInfo,
        year: i32,
    ) -> Self {
        let std_offset = UtcOffsetSeconds::from(&posix_tz_string.std_info);
        let dst_offset = UtcOffsetSeconds::from(&dst_variant.variant_info);
        let dst_start_seconds =
            calculate_transition_seconds_for_year(year, dst_variant.start_date, std_offset);
        let dst_end_seconds =
            calculate_transition_seconds_for_year(year, dst_variant.end_date, dst_offset);
        Self {
            dst_start_seconds,
            dst_end_seconds,
            std_offset,
            dst_offset,
        }
    }

    // Returns the range between offsets in this year
    // This may cover DST or standard time, whichever starts first
    pub(crate) fn transition_range(&self) -> Range<i64> {
        if self.dst_start_seconds > self.dst_end_seconds {
            self.dst_end_seconds..self.dst_start_seconds
        } else {
            self.dst_start_seconds..self.dst_end_seconds
        }
    }
}

#[cfg(feature = "tzif")]
pub(crate) fn calculate_transition_seconds_for_year(
    year: i32,
    transition_date: TransitionDate,
    offset: UtcOffsetSeconds,
) -> i64 {
    // Determine the year of the requested time.
    let year_epoch_seconds = i64::from(utils::epoch_days_for_year(year)) * 86400;
    let is_leap = utils::is_leap(year);

    // Calculate the days in the year for the TransitionDate
    // This value is zero-indexed so it can be added to the year's epoch seconds
    let days = match transition_date.day {
        TransitionDay::NoLeap(day) if day > 59 => day - 1 + is_leap as u16,
        TransitionDay::NoLeap(day) => day - 1,
        TransitionDay::WithLeap(day) => day,
        TransitionDay::Mwd(month, week, day) => {
            let days_to_month = utils::month_to_day((month - 1) as u8, is_leap);
            let days_in_month = u16::from(utils::iso_days_in_month(year, month as u8));

            // Month starts in the day...
            let day_offset = (u16::from(utils::epoch_seconds_to_day_of_week(year_epoch_seconds))
                + days_to_month)
                .rem_euclid(7);

            // EXAMPLE:
            //
            // 0   1   2   3   4   5   6
            // sun mon tue wed thu fri sat
            // -   -   -   0   1   2   3
            // 4   5   6   7   8   9   10
            // 11  12  13  14  15  16  17
            // 18  19  20  21  22  23  24
            // 25  26  27  28  29  30  -
            //
            // The day_offset = 3, since the month starts on a wednesday.
            //
            // We're looking for the second friday of the month. Thus, since the month started before
            // a friday, we need to start counting from week 0:
            //
            // day_of_month = (week - u16::from(day_offset <= day)) * 7 + day - day_offset = (2 - 1) * 7 + 5 - 3 = 9
            //
            // This works if the month started on a day before the day we want (day_offset <= day). However, if that's not the
            // case, we need to start counting on week 1. For example, calculate the day of the month for the third monday
            // of the month:
            //
            // day_of_month = (week - u16::from(day_offset <= day)) * 7 + day - day_offset = (3 - 0) * 7 + 1 - 3 = 19

            // Note: this day_of_month is zero-indexed!
            let mut day_of_month = (week - u16::from(day_offset <= day)) * 7 + day - day_offset;

            // Week 5 actually means "last <dayofweek> of month". The day_of_month calculation
            // above uses `week` directly; so we might end up spilling into the next month. In that
            // case, we normalize to the fourth week of the month.
            //
            // Note that this only needs to be done once; if a month will have at least four of each
            // day of the week since all months have 28 days or greater.
            //
            // We add one because day_of_month is zero_indexed
            if day_of_month + 1 > days_in_month {
                day_of_month -= 7
            }

            days_to_month + day_of_month
        }
    };

    // Transition time is on local time, so we need to add the UTC offset to get the correct UTC timestamp
    // for the transition.
    year_epoch_seconds + i64::from(days) * 86400 + transition_date.time.0 - offset.0
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub(crate) enum TransitionType {
    Dst,
    Std,
}

impl TransitionType {
    pub(crate) fn invert(&mut self) {
        *self = match *self {
            Self::Dst => Self::Std,
            Self::Std => Self::Dst,
        }
    }
}

/// The month, week of month, and day of week value built into the POSIX tz string.
///
/// For more information, see the [POSIX tz string docs](https://sourceware.org/glibc/manual/2.40/html_node/Proleptic-TZ.html)
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub(crate) struct Mwd {
    month: u8,
    week: u8,
    day: u8,
}

impl Mwd {
    #[cfg(feature = "tzif")]
    pub(crate) fn from_u16(month: u16, week: u16, day: u16) -> Self {
        Self::from_u8(
            u8::try_from(month).unwrap_or(0),
            u8::try_from(week).unwrap_or(0),
            u8::try_from(day).unwrap_or(0),
        )
    }

    pub(crate) fn from_u8(month: u8, week: u8, day: u8) -> Self {
        Self { month, week, day }
    }

    /// Given the day of the week of the 0th day in this month,
    /// normalize the week to being a week number (1 = first week, ...)
    /// rather than a weekday ordinal (1 = first friday, etc)
    pub(crate) fn normalize_to_week_number(&mut self, day_of_week_zeroth_day: u8) {
        if self.day <= day_of_week_zeroth_day {
            self.week += 1;
        }
    }
}

/// Represents an MWD for a given time
#[derive(Debug)]
pub(crate) struct MwdForTime {
    /// This will never have day = 5
    pub(crate) mwd: Mwd,
    /// The day of the week of the 0th day (the day before the month starts)
    pub(crate) day_of_week_zeroth_day: u8,
    /// This is the day of week of the 29th and the last day of the month,
    /// if the month has more than 28 days.
    /// Basically, this is the start and end of the "fifth $weekday of the month" period
    pub(crate) extra_days: Option<(u8, u8)>,
}

impl MwdForTime {
    #[cfg(any(feature = "tzif", feature = "experimental_tzif"))]
    pub(crate) fn from_seconds(seconds: i64) -> Self {
        let (year, month, day_of_month) = utils::ymd_from_epoch_milliseconds(seconds * 1_000);
        let week_of_month = day_of_month / 7 + 1;
        let day_of_week = utils::epoch_seconds_to_day_of_week(seconds);
        let mut mwd = Mwd::from_u8(month, week_of_month, day_of_week);
        let days_in_month = utils::iso_days_in_month(year, month);
        let day_of_week_zeroth_day =
            (i16::from(day_of_week) - i16::from(day_of_month)).rem_euclid(7) as u8;
        mwd.normalize_to_week_number(day_of_week_zeroth_day);
        if day_of_month > 28 {
            let day_of_week_day_29 = (day_of_week_zeroth_day + 29).rem_euclid(7);
            let day_of_week_last_day = (day_of_week_zeroth_day + days_in_month).rem_euclid(7);
            Self {
                mwd,
                day_of_week_zeroth_day,
                extra_days: Some((day_of_week_day_29, day_of_week_last_day)),
            }
        } else {
            // No day 5
            Self {
                mwd,
                day_of_week_zeroth_day,
                extra_days: None,
            }
        }
    }

    /// MWDs from Posix data can contain `w=5`, which means the *last* $weekday of the month,
    /// not the 5th. For MWDs in the same month, this normalizes the 5 to the actual number of the
    /// last weekday of the month (5 or 4)
    ///
    /// Furthermore, this turns the week number into a true week number: the "second friday in March"
    /// will be turned into "the friday in the first week of March" or "the Friday in the second week of March"
    /// depending on when March starts.
    ///
    /// This normalization *only* applies to MWDs in the same month. For other MWDs, such normalization is irrelevant.
    pub(crate) fn normalize_mwd(&self, other: &mut Mwd) {
        // If we're in the same month, normalization will actually have a useful effect
        if self.mwd.month == other.month {
            // First normalize MWDs that are like "the last $weekday in the month"
            // the last $weekday in the month, we need special handling
            if other.week == 5 {
                if let Some((day_29, last_day)) = self.extra_days {
                    if day_29 < last_day {
                        if other.day < day_29 || other.day > last_day {
                            // This day isn't found in the last week. Subtract one.
                            other.week = 4;
                        }
                    } else {
                        // The extra part of the month crosses Sunday
                        if other.day < day_29 && other.day > last_day {
                            // This day isn't found in the last week. Subtract one.
                            other.week = 4;
                        }
                    }
                } else {
                    // There is no week 5 in this month, normalize to 4
                    other.week = 4;
                }
            }

            other.normalize_to_week_number(self.day_of_week_zeroth_day);
        }
    }
}

pub(crate) fn offset_range(offset_one: i64, offset_two: i64) -> core::ops::Range<i64> {
    if offset_one < offset_two {
        return offset_one..offset_two;
    }
    offset_two..offset_one
}

#[derive(Debug)]
pub(crate) enum TransitionKind {
    // The offsets didn't change (happens when abbreviations/savings values change)
    Smooth,
    // The offsets changed in a way that leaves a gap
    Gap,
    // The offsets changed in a way that produces overlapping time.
    Overlap,
}
