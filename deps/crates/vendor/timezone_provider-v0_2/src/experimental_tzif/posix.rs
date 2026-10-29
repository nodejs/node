use crate::{
    common::{
        offset_range, DstTransitionInfoForYear, LocalTimeRecordResult, Mwd, MwdForTime,
        TimeZoneTransitionInfo, TransitionType,
    },
    epoch_nanoseconds::EpochNanoseconds,
    provider::{GapEntryOffsets, TimeZoneProviderResult, UtcOffsetSeconds},
    utils, TimeZoneProviderError,
};

use tinystr::TinyAsciiStr;
#[cfg(feature = "datagen")]
use zoneinfo_rs::posix::{MonthWeekDay, PosixDate, PosixDateTime, PosixTimeZone, PosixTransition};

#[zerovec::make_ule(PosixZoneULE)]
#[derive(PartialEq, Eq, Debug, Clone, Copy, PartialOrd, Ord)]
#[cfg_attr(
    feature = "datagen",
    derive(yoke::Yokeable, serde::Serialize, databake::Bake)
)]
#[cfg_attr(feature = "datagen", databake(path = timezone_provider::experimental_tzif::posix))]
pub struct PosixZone {
    pub abbr: TinyAsciiStr<5>,
    pub offset: i64,
    pub transition: Option<ZeroPosixTransition>,
}

impl PosixZone {
    pub(crate) fn resolve_for_local_seconds(
        &self,
        local_seconds: i64,
    ) -> TimeZoneProviderResult<LocalTimeRecordResult> {
        let Some(transition_info) = &self.transition else {
            // Regardless of the time, there is one variant and we can return it.
            let offset = UtcOffsetSeconds(self.offset);
            return Ok(LocalTimeRecordResult::Single(offset));
        };

        // NOTE:
        // STD -> DST == start
        // DST -> STD == end
        let (is_transition_day, mut is_dst) = cmp_seconds_to_transitions(
            &transition_info.start.date,
            &transition_info.end.date,
            local_seconds,
        )?;

        if is_transition_day {
            let time = utils::epoch_ms_to_ms_in_day(local_seconds * 1_000) as i64 / 1_000;
            let transition_time = if is_dst == TransitionType::Dst {
                transition_info.start.time
            } else {
                transition_info.end.time
            };
            // Convert to UtcOffsetSeconds so that these behave like
            // normal offsets
            let std = UtcOffsetSeconds(self.offset);
            let dst = UtcOffsetSeconds(self.offset + transition_info.savings);
            let transition_diff = if is_dst == TransitionType::Dst {
                dst.0 - std.0
            } else {
                std.0 - dst.0
            };
            let offset = offset_range(transition_time + transition_diff, transition_time);
            match offset.contains(&time) {
                true if is_dst == TransitionType::Dst => {
                    return Ok(LocalTimeRecordResult::Empty(GapEntryOffsets {
                        offset_before: std,
                        offset_after: dst,
                        transition_epoch: EpochNanoseconds::from_seconds(transition_time),
                    }));
                }
                true => {
                    // Note(nekevss, manishearth): We may need to more carefully
                    // handle inverse DST here.
                    return Ok(LocalTimeRecordResult::Ambiguous {
                        first: dst,
                        second: std,
                    });
                }
                _ => {}
            }

            // We were not contained in the transition above,
            // AND we are before it, which means we are actually in
            // the other transition!
            //
            // NOTE(Manishearth) do we need to do anything special
            // here if we end up back at the tzif transition data?
            if time < offset.start {
                is_dst.invert();
            }
        }

        match is_dst {
            TransitionType::Dst => {
                Ok(UtcOffsetSeconds(self.offset + transition_info.savings).into())
            }
            TransitionType::Std => Ok(UtcOffsetSeconds(self.offset).into()),
        }
    }

    pub(crate) fn resolve_for_epoch_seconds(
        &self,
        epoch_seconds: i64,
    ) -> TimeZoneProviderResult<TimeZoneTransitionInfo> {
        let Some(dst_transition_info) = &self.transition else {
            // Regardless of the time, there is one variant and we can return it.
            return Ok(TimeZoneTransitionInfo {
                transition_epoch: None,
                offset: UtcOffsetSeconds(self.offset),
            });
        };

        let year = utils::epoch_time_to_iso_year(epoch_seconds * 1000);

        let transition_info = DstTransitionInfoForYear::compute_zero_transition(
            self.offset,
            dst_transition_info,
            year,
        );
        let dst_start_seconds = transition_info.dst_start_seconds;
        let dst_end_seconds = transition_info.dst_end_seconds;

        // Need to determine if the range being tested is standard or savings time.
        let dst_is_inversed = dst_end_seconds < dst_start_seconds;

        // We have potentially to different variations of the DST start and end time.
        //
        // Northern hemisphere: dst_start -> dst_end
        // Southern hemisphere: dst_end -> dst_start
        //
        // This is primarily due to the summer / winter months of those areas.
        //
        // For the northern hemispere, we can check if the range contains the seconds. For the
        // southern hemisphere, we check if the range does no contain the value.
        let should_return_dst = (!dst_is_inversed
            && (dst_start_seconds..dst_end_seconds).contains(&epoch_seconds))
            || (dst_is_inversed && !(dst_end_seconds..dst_start_seconds).contains(&epoch_seconds));

        // Expanding on the above, the state of time zones in the year are:
        //
        // Northern hemisphere: STD -> DST -> STD
        // Southern hemisphere: DST -> STD -> DST
        //
        // This is simple for the returning the offsets, but if the seconds value falls into the first
        // available rule. However, the northern hemisphere's first STD rule and the Southern hemisphere's
        // first DST rule will have different transition times that are based in the year prior, so if the
        // requested seconds falls in that range, we calculate the transition time for the prior year.
        let (new_offset, transition_epoch) = if should_return_dst {
            let transition_epoch = if dst_is_inversed && epoch_seconds < dst_end_seconds {
                Some(calculate_transition_seconds_for_year(
                    year - 1,
                    dst_transition_info.start,
                    transition_info.dst_offset,
                ))
            } else {
                Some(dst_start_seconds)
            };
            (transition_info.dst_offset, transition_epoch)
        } else {
            let transition_epoch = if !dst_is_inversed && epoch_seconds < dst_start_seconds {
                Some(calculate_transition_seconds_for_year(
                    year - 1,
                    dst_transition_info.end,
                    transition_info.std_offset,
                ))
            } else {
                Some(dst_end_seconds)
            };
            (transition_info.std_offset, transition_epoch)
        };
        Ok(TimeZoneTransitionInfo {
            offset: new_offset,
            transition_epoch,
        })
    }
}

impl DstTransitionInfoForYear {
    pub(crate) fn compute_zero_transition(
        std_offset_seconds: i64,
        dst_transition: &ZeroPosixTransition,
        year: i32,
    ) -> Self {
        let std_offset = UtcOffsetSeconds(std_offset_seconds);
        let dst_offset = UtcOffsetSeconds(std_offset_seconds + dst_transition.savings);
        let dst_start_seconds =
            calculate_transition_seconds_for_year(year, dst_transition.start, std_offset);
        let dst_end_seconds =
            calculate_transition_seconds_for_year(year, dst_transition.end, dst_offset);
        Self {
            dst_start_seconds,
            dst_end_seconds,
            std_offset,
            dst_offset,
        }
    }
}

fn calculate_transition_seconds_for_year(
    year: i32,
    transition_dt: ZeroTransitionDateTime,
    offset: UtcOffsetSeconds,
) -> i64 {
    // Determine the year of the requested time.
    let year_epoch_seconds = i64::from(utils::epoch_days_for_year(year)) * 86400;
    let is_leap = utils::is_leap(year);

    // Calculate the days in the year for the TransitionDate
    // This value is zero-indexed so it can be added to the year's epoch seconds
    let days = match transition_dt.date {
        ZeroTransitionDate {
            kind: DateKind::JulianNoLeap,
            day: Some(day),
            ..
        } => day,
        ZeroTransitionDate {
            kind: DateKind::Julian,
            day: Some(day),
            ..
        } => day + 1,
        ZeroTransitionDate {
            kind: DateKind::MonthWeekDay,
            mwd: Some((month, week, day)),
            ..
        } => {
            let days_to_month = utils::month_to_day(month - 1, is_leap);
            let days_in_month = utils::iso_days_in_month(year, month);

            // Month starts in the day...
            let day_offset = (u16::from(utils::epoch_seconds_to_day_of_week(year_epoch_seconds))
                + days_to_month)
                .rem_euclid(7) as u8;

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
            let mut day_of_month = (week - u8::from(day_offset <= day)) * 7 + day - day_offset;

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

            days_to_month + day_of_month as u16
        }
        _ => panic!("Invalid TransitionDate found."),
    };

    // Transition time is on local time, so we need to add the UTC offset to get the correct UTC timestamp
    // for the transition.
    year_epoch_seconds + i64::from(days) * 86400 + transition_dt.time - offset.0
}

fn cmp_seconds_to_transitions(
    start: &ZeroTransitionDate,
    end: &ZeroTransitionDate,
    seconds: i64,
) -> TimeZoneProviderResult<(bool, TransitionType)> {
    // Assert the kinds are equal
    assert_eq!(start.kind, end.kind);

    let cmp_result = match (start.to_enum(), end.to_enum()) {
        (
            TransitionDate::Mwd((start_month, start_week, start_day)),
            TransitionDate::Mwd((end_month, end_week, end_day)),
        ) => {
            let mwd = MwdForTime::from_seconds(seconds);
            let mut start = Mwd::from_u8(start_month, start_week, start_day);
            let mut end = Mwd::from_u8(end_month, end_week, end_day);

            mwd.normalize_mwd(&mut start);
            mwd.normalize_mwd(&mut end);

            let is_transition = start == mwd.mwd || end == mwd.mwd;
            let is_dst = if start > end {
                mwd.mwd < end || start <= mwd.mwd
            } else {
                start <= mwd.mwd && mwd.mwd < end
            };

            (is_transition, is_dst)
        }
        (TransitionDate::Julian(start), TransitionDate::Julian(end)) => {
            let day_in_year = utils::epoch_time_to_day_in_year(seconds * 1_000) as u16;
            let is_transition = start == day_in_year || end == day_in_year;
            let is_dst = if start > end {
                day_in_year < end || start <= day_in_year
            } else {
                start <= day_in_year && day_in_year < end
            };
            (is_transition, is_dst)
        }
        // TODO: do we need to modify the logic for leap years?
        (TransitionDate::JulianNoLeap(start), TransitionDate::JulianNoLeap(end)) => {
            let day_in_year = utils::epoch_time_to_day_in_year(seconds * 1_000) as u16;
            let is_transition = start == day_in_year || end == day_in_year;
            let is_dst = if start > end {
                day_in_year < end || start <= day_in_year
            } else {
                start <= day_in_year && day_in_year < end
            };
            (is_transition, is_dst)
        }
        // NOTE: The assumption here is that mismatched day types on
        // a POSIX string is an illformed string.
        _ => {
            return Err(TimeZoneProviderError::Assert(
                "Mismatched day types on a POSIX string.",
            ))
        }
    };

    match cmp_result {
        (true, dst) if dst => Ok((true, TransitionType::Dst)),
        (true, _) => Ok((true, TransitionType::Std)),
        (false, dst) if dst => Ok((false, TransitionType::Dst)),
        (false, _) => Ok((false, TransitionType::Std)),
    }
}

#[cfg(feature = "datagen")]
#[allow(clippy::unwrap_used, reason = "Datagen only")]
impl From<&PosixTimeZone> for PosixZone {
    fn from(value: &PosixTimeZone) -> Self {
        let abbr = TinyAsciiStr::<5>::try_from_str(&value.abbr.formatted).unwrap();
        let offset = value.offset.as_secs();
        let transition = value
            .transition_info
            .as_ref()
            .map(ZeroPosixTransition::from);

        Self {
            abbr,
            offset,
            transition,
        }
    }
}

#[zerovec::make_ule(PosixTransitionULE)]
#[derive(PartialEq, Eq, Debug, Clone, Copy, PartialOrd, Ord)]
#[cfg_attr(
    feature = "datagen",
    derive(yoke::Yokeable, serde::Serialize, databake::Bake)
)]
#[cfg_attr(feature = "datagen", databake(path = timezone_provider::experimental_tzif::posix))]
pub struct ZeroPosixTransition {
    pub abbr: TinyAsciiStr<5>,
    pub savings: i64,
    pub start: ZeroTransitionDateTime,
    pub end: ZeroTransitionDateTime,
}

#[cfg(feature = "datagen")]
#[allow(clippy::unwrap_used, reason = "Datagen only")]
impl From<&PosixTransition> for ZeroPosixTransition {
    fn from(value: &PosixTransition) -> Self {
        let abbr = TinyAsciiStr::<5>::try_from_str(&value.abbr.formatted).unwrap();
        let savings = value.savings.as_secs();
        let start = ZeroTransitionDateTime::from(&value.start);
        let end = ZeroTransitionDateTime::from(&value.end);
        Self {
            abbr,
            savings,
            start,
            end,
        }
    }
}

#[zerovec::make_ule(TransitionDateTimeULE)]
#[derive(PartialEq, Eq, Debug, Clone, Copy, PartialOrd, Ord)]
#[cfg_attr(
    feature = "datagen",
    derive(yoke::Yokeable, serde::Serialize, databake::Bake)
)]
#[cfg_attr(feature = "datagen", databake(path = timezone_provider::experimental_tzif::posix))]
pub struct ZeroTransitionDateTime {
    /// The date at which a transition should occur.
    pub date: ZeroTransitionDate,
    /// The time of day in seconds.
    pub time: i64,
}

#[cfg(feature = "datagen")]
impl From<&PosixDateTime> for ZeroTransitionDateTime {
    fn from(value: &PosixDateTime) -> Self {
        Self {
            date: value.date.into(),
            time: value.time.as_secs(),
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub(crate) enum TransitionDate {
    Julian(u16),
    JulianNoLeap(u16),
    Mwd((u8, u8, u8)),
}

#[zerovec::make_ule(DateULE)]
#[derive(PartialEq, Eq, Debug, Clone, Copy, PartialOrd, Ord)]
#[cfg_attr(
    feature = "datagen",
    derive(yoke::Yokeable, serde::Serialize, databake::Bake)
)]
#[cfg_attr(feature = "datagen", databake(path = timezone_provider::experimental_tzif::posix))]
pub struct ZeroTransitionDate {
    pub kind: DateKind,
    pub day: Option<u16>,
    pub mwd: Option<(u8, u8, u8)>,
}

impl ZeroTransitionDate {
    pub(crate) fn to_enum(self) -> TransitionDate {
        match self {
            ZeroTransitionDate {
                kind: DateKind::JulianNoLeap,
                day: Some(day),
                ..
            } => TransitionDate::JulianNoLeap(day),
            ZeroTransitionDate {
                kind: DateKind::Julian,
                day: Some(day),
                ..
            } => TransitionDate::Julian(day),
            ZeroTransitionDate {
                kind: DateKind::MonthWeekDay,
                mwd: Some(mwd),
                ..
            } => TransitionDate::Mwd(mwd),
            _ => panic!("Invalid ZeroTransitionDate"),
        }
    }
}

#[cfg(feature = "datagen")]
impl From<PosixDate> for ZeroTransitionDate {
    fn from(value: PosixDate) -> Self {
        let (kind, day, mwd) = match value {
            PosixDate::JulianLeap(day) => (DateKind::Julian, Some(day), None),
            PosixDate::JulianNoLeap(day) => (DateKind::JulianNoLeap, Some(day), None),
            PosixDate::MonthWeekDay(MonthWeekDay(month, week, day)) => (
                DateKind::MonthWeekDay,
                None,
                Some((month as u8, week, day as u8)),
            ),
        };
        Self { kind, day, mwd }
    }
}
#[zerovec::make_ule(DateKindULE)]
#[derive(PartialEq, Eq, Debug, Clone, Copy, PartialOrd, Ord)]
#[cfg_attr(
    feature = "datagen",
    derive(yoke::Yokeable, serde::Serialize, databake::Bake)
)]
#[cfg_attr(feature = "datagen", databake(path = timezone_provider::experimental_tzif::posix))]
#[repr(u8)]
pub enum DateKind {
    JulianNoLeap = 0,
    Julian = 1,
    MonthWeekDay = 2,
}
