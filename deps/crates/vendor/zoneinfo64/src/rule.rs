// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use super::{Offset, Transition, EPOCH, SECONDS_IN_UTC_DAY};
use crate::UtcOffset;
use calendrical_calculations::iso;
use calendrical_calculations::rata_die::RataDie;

#[derive(Debug, Clone, Copy, PartialEq)]
pub(crate) struct Rule<'a> {
    /// The year the rule starts applying
    pub(crate) start_year: i32,
    /// The offset of standard time
    pub(crate) standard_offset_seconds: i32,
    pub(crate) inner: &'a TzRule,
}

#[derive(Debug, PartialEq)]
pub(crate) struct TzRule {
    /// The amount of seconds to add to standard_offset_seconds
    /// to get the rule offset
    additional_offset_secs: i32,
    /// The yearly start date of the rule
    start: TzRuleDate,
    /// The yearly end date of the rule
    end: TzRuleDate,
}

#[derive(Debug, PartialEq)]
struct TzRuleDate {
    /// A 1-indexed day number
    day: u8,
    /// A day of the week (0 = Sunday)
    day_of_week: u8,
    /// A 1-indexed month number
    month: u8,
    /// The time in the day (in seconds) that the transition occurs
    transition_time: u32,
    /// How to interpret transition_time
    time_mode: TimeMode,
    /// How to interpret day, day_of_week, and month
    mode: RuleMode,
}

#[derive(Debug, Copy, Clone, PartialEq)]
enum TimeMode {
    /// {transition_time} is local wall clock time in the time zone
    /// *before* the transition
    ///
    /// e.g. if the transition between LST and LDT is to happen at 02:00,
    /// the time that *would be* 02:00 LST would be the first time of LDT.
    ///
    /// This means that `{local_wall_clock_time}` may never actually be the
    /// wall clock time! The America/Los_Angeles transition occurs at Wall 02:00,
    /// however the transition from PST to PDT is
    /// `2025-03-09T01:59:59-08:00[America/Los_Angeles]` to
    /// 2025-03-09T03:00:00-07:00[America/Los_Angeles],
    /// so 2025-03-09T02:00:00 never occurs.
    ///
    /// This can be turned into Standard by subtracting the offset-from-standard
    /// of the time zone *before* this transition
    Wall = 0,
    /// {transition_time} is local standard time
    ///
    /// Will produce different results from Wall=0 for DST-to-STD transitions
    ///
    /// This can be turned into Wall by adding the offset-from-standard of the time zone
    /// *before* this transition.
    Standard = 1,
    /// {transition_time} is UTC time
    ///
    /// This is UTC time *on the UTC day* identified by this rule; which may
    /// end up on a different local day.
    ///
    /// For example, America/Santiago transitions to STD on the first Sunday after April 2.
    /// at UTC 03:00:00, which is `2025-04-06T03:00:00+00:00[UTC]`. This ends up being
    /// a transition from`2025-04-05T23:59:59-03:00[America/Santiago]` to
    /// `2025-04-05T23:00:00-04:00[America/Santiago]`).
    ///
    /// This can be turned into Standard by subtracting the standard-offset-from-UTC of the
    /// time zone. It can be turned into Wall by subtracting the offset-from-UTC of the time zone
    /// before this transition.
    Utc = 2,
}

#[derive(Debug, PartialEq, Copy, Clone)]
#[allow(non_camel_case_types, clippy::upper_case_acronyms)]
/// How to interpret `{day}` `{day_of_week}` and `{month}`
enum RuleMode {
    /// The {day}th {day_of_week} in {month}
    ///
    /// Current zoneinfo64 does not use this, instead
    /// choosing to represent this as DOW_GEQ_DOM with day = 1/8/15/22
    DOW_IN_MONTH,
    /// {month} {day}
    ///
    /// Current zoneinfo64 does not use this
    DOM,
    /// The first {day_of_week} on or after {month} {day}
    DOW_GEQ_DOM,
    /// The first {day_of_week} on or before {month} {day}
    ///
    /// Typically, this represents rules like "Last Sunday in March" (Europe/London)
    DOW_LEQ_DOM,
}

impl TzRule {
    pub(crate) fn from_raw(value: &[i32; 11]) -> Self {
        Self {
            additional_offset_secs: value[10],
            start: TzRuleDate::new(
                value[1] as i8,
                value[2] as i8,
                value[0] as u8,
                value[3] as u32,
                value[4] as i8,
            )
            .unwrap(),
            end: TzRuleDate::new(
                value[6] as i8,
                value[7] as i8,
                value[5] as u8,
                value[8] as u32,
                value[9] as i8,
            )
            .unwrap(),
        }
    }

    fn end_before_start(&self) -> bool {
        (self.start.month, self.start.day) > (self.end.month, self.end.day)
    }
}

impl TzRuleDate {
    fn new(
        mut day: i8,
        mut day_of_week: i8,
        zero_based_month: u8,
        transition_time: u32,
        time_mode: i8,
    ) -> Option<Self> {
        let month = zero_based_month + 1;

        if day == 0 {
            return None;
        }
        if month > 12 {
            return None;
        }
        if i64::from(transition_time) > SECONDS_IN_UTC_DAY {
            return None;
        }

        let time_mode = match time_mode {
            0 => TimeMode::Wall,
            1 => TimeMode::Standard,
            2 => TimeMode::Utc,
            _ => return None,
        };

        let mode;

        if day_of_week == 0 {
            mode = RuleMode::DOM;
        } else {
            if day_of_week > 0 {
                mode = RuleMode::DOW_IN_MONTH
            } else {
                day_of_week = -day_of_week;
                if day > 0 {
                    mode = RuleMode::DOW_GEQ_DOM;
                } else {
                    day = -day;
                    mode = RuleMode::DOW_LEQ_DOM;
                }
            }
            if day_of_week > 7 {
                return None;
            }
        }

        if mode == RuleMode::DOW_IN_MONTH {
            if !(-5..=5).contains(&day) {
                return None;
            }
        } else if day < 1
            || day
                > if month == 2 {
                    29
                } else {
                    30 | month ^ (month >> 3)
                } as i8
        {
            return None;
        }

        Some(Self {
            day: u8::try_from(day).unwrap_or_default(),
            day_of_week: u8::try_from(day_of_week - 1).unwrap_or_default(),
            month: zero_based_month + 1,
            transition_time,
            time_mode,
            mode,
        })
    }

    /// Given a year, return the 1-indexed day number in that year for this transition
    fn day_in_year(&self, year: i32, day_before_year: RataDie) -> u16 {
        let days_before_month = iso::days_before_month(year, self.month);

        if let RuleMode::DOM = self.mode {
            return days_before_month + u16::from(self.day);
        }

        fn weekday(rd: RataDie) -> u8 {
            const SUNDAY: RataDie = iso::const_fixed_from_iso(0, 12, 31);
            (rd.since(SUNDAY) % 7) as u8
        }

        let weekday_before_month = weekday(day_before_year + days_before_month as i64);

        // Turn this into a zero-indexed day of week
        let day_of_month = match self.mode {
            RuleMode::DOM | // unreachable
            RuleMode::DOW_IN_MONTH => {
                // First we calculate the first {day_of_week} of the month
                let first_weekday = if self.day_of_week > weekday_before_month {
                    0
                } else {
                    7
                } + self.day_of_week - weekday_before_month;

                // Then we add additional weeks to it if desired
                first_weekday + (self.day - 1) * 7
            }
            // These two compute after/before an "anchor" day in the month
            RuleMode::DOW_GEQ_DOM => {
                let weekday_of_anchor = (weekday_before_month + self.day) % 7;
                let days_to_add = if self.day_of_week >= weekday_of_anchor {
                    0
                } else {
                    7
                } + self.day_of_week - weekday_of_anchor;
                self.day + days_to_add
            }
            RuleMode::DOW_LEQ_DOM => {
                let weekday_of_anchor = (weekday_before_month + self.day) % 7;
                let days_to_subtract = if self.day_of_week <= weekday_of_anchor {
                    0
                } else {
                    7
                } + weekday_of_anchor - self.day_of_week;
                self.day - days_to_subtract
            }
        };
        // Subtract one so we get a 0-indexed value (Jan 1 = day 0)
        days_before_month + u16::from(day_of_month)
    }

    fn timestamp_for_year(
        &self,
        year: i32,
        day_before_year: RataDie,
        standard_offset_seconds: i32,
        additional_offset_seconds: i32,
    ) -> i64 {
        let day = day_before_year + self.day_in_year(year, day_before_year) as i64;
        let start_seconds =
            self.transition_time_to_utc(standard_offset_seconds, additional_offset_seconds);
        day.since(EPOCH) * SECONDS_IN_UTC_DAY + i64::from(start_seconds)
    }

    /// Converts the {transition_time} into a time in the UTC day, in seconds
    fn transition_time_to_utc(
        &self,
        standard_offset_seconds: i32,
        additional_offset_seconds: i32,
    ) -> i32 {
        let seconds_of_day = self.transition_time as i32;
        match self.time_mode {
            TimeMode::Utc => seconds_of_day,
            TimeMode::Standard => seconds_of_day - standard_offset_seconds,
            TimeMode::Wall => {
                seconds_of_day - (standard_offset_seconds + additional_offset_seconds)
            }
        }
    }
}

impl Rule<'_> {
    /// Get the first or second transition for the given year, as well as the offset that
    /// was active before the transition.
    pub(crate) fn transition(
        &self,
        year: i32,
        day_before_year: RataDie,
        second: bool,
    ) -> (Offset, Transition) {
        debug_assert!(year >= self.start_year);

        let (selected, other) = if self.inner.end_before_start() ^ second {
            (
                (&self.inner.end, 0),
                (&self.inner.start, self.inner.additional_offset_secs),
            )
        } else {
            (
                (&self.inner.start, self.inner.additional_offset_secs),
                (&self.inner.end, 0),
            )
        };

        (
            Offset {
                offset: UtcOffset(self.standard_offset_seconds + other.1),
                rule_applies: other.1 != 0,
            },
            Transition {
                since: selected.0.timestamp_for_year(
                    year,
                    day_before_year,
                    self.standard_offset_seconds,
                    other.1,
                ),
                offset: UtcOffset(self.standard_offset_seconds + selected.1),
                rule_applies: selected.1 != 0,
            },
        )
    }

    /// Get the offset for a timestamp.
    ///
    /// Returns None if `seconds_since_epoch` in UTC is before the start of the rule,
    /// or after the year `i32::MAX`.
    pub(crate) fn for_timestamp(&self, seconds_since_epoch: i64) -> Option<Offset> {
        let local_year = self.local_year_for_timestamp(seconds_since_epoch)?;
        let day_before_year = iso::day_before_year(local_year);

        let (before, first) = self.transition(local_year, day_before_year, false);

        if seconds_since_epoch < first.since {
            if local_year == self.start_year {
                return None;
            }
            return Some(before);
        }

        let (_, second) = self.transition(local_year, day_before_year, true);
        if seconds_since_epoch < second.since {
            Some(first.into())
        } else {
            Some(second.into())
        }
    }

    /// Get the transition before a timestamp.
    ///
    /// If `seconds_exact` is false, the transition at `x` is considered
    /// to be before the timestamp `x`.
    //
    // This is almost the exact same code as `for_timestamp`, just
    // that it has the `seconds_exact` flag, and that it calculates the
    // extra transition timestamp if the previous transition is in the previous year.
    pub(crate) fn prev_transition(
        &self,
        seconds_since_epoch: i64,
        seconds_exact: bool,
    ) -> Option<Transition> {
        let local_year = self.local_year_for_timestamp(seconds_since_epoch)?;
        let day_before_year = iso::day_before_year(local_year);

        let (_, first) = self.transition(local_year, day_before_year, false);

        if seconds_exact && seconds_since_epoch <= first.since
            || !seconds_exact && seconds_since_epoch < first.since
        {
            if local_year == self.start_year {
                return None;
            }
            return Some(
                self.transition(local_year - 1, iso::day_before_year(local_year - 1), true)
                    .1,
            );
        }

        let (_, second) = self.transition(local_year, day_before_year, true);
        if seconds_exact && seconds_since_epoch <= second.since
            || !seconds_exact && seconds_since_epoch < second.since
        {
            Some(first)
        } else {
            Some(second)
        }
    }

    /// Get the next transition after a timestamp.
    ///
    /// As rules continue forever into the future, this does not return
    /// an `Option`.
    ///
    /// If `seconds_since_epoch` in UTC is after the year `i32::MAX`,
    /// returns garbage.
    pub(crate) fn next_transition(&self, seconds_since_epoch: i64) -> Transition {
        let local_year = self
            .local_year_for_timestamp(seconds_since_epoch)
            .unwrap_or(self.start_year);
        let day_before_year = iso::day_before_year(local_year);

        let (_, first) = self.transition(local_year, day_before_year, false);

        if seconds_since_epoch < first.since {
            return first;
        }
        let (_, second) = self.transition(local_year, day_before_year, true);
        if seconds_since_epoch < second.since {
            second
        } else {
            self.transition(local_year + 1, iso::day_before_year(local_year + 1), false)
                .1
        }
    }

    fn local_year_for_timestamp(&self, seconds_since_epoch: i64) -> Option<i32> {
        let Ok(year) = iso::iso_year_from_fixed(EPOCH + (seconds_since_epoch / SECONDS_IN_UTC_DAY))
        else {
            // Pretend rule doesn't apply anymore after year i32::MAX
            return None;
        };

        // No transition happens in a different UTC year, this is verified
        // in `test_rule_not_at_year_boundary`
        let local_year = year;

        if local_year < self.start_year {
            return None;
        }

        Some(local_year)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::tests::TZDB;
    use crate::PossibleOffset;

    /// Tests an invariant we rely on in our code
    #[test]
    fn test_last_transition_not_in_rule_year() {
        for chrono in crate::tests::time_zones_to_test() {
            let iana = chrono.name();
            let zoneinfo64 = TZDB.get(iana).unwrap().simple();

            if let Some(rule) = zoneinfo64.final_rule(&TZDB.rules) {
                let transition = zoneinfo64.transition_offset_at(zoneinfo64.transition_count() - 1);
                let utc_year =
                    iso::iso_year_from_fixed(EPOCH + (transition.since / SECONDS_IN_UTC_DAY))
                        .unwrap();

                assert!(
                    utc_year < rule.start_year,
                    "last transition should not be in rule year: {utc_year} < {} ({iana})",
                    rule.start_year
                );
            }
        }
    }

    /// Tests an invariant we rely on in our code
    #[test]
    fn test_rule_stays_inside_year() {
        for chrono in crate::tests::time_zones_to_test() {
            let iana = chrono.name();
            let zoneinfo64 = TZDB.get(iana).unwrap().simple();

            if let Some(rule) = zoneinfo64.final_rule(&TZDB.rules) {
                let max_delta = core::cmp::max(
                    rule.standard_offset_seconds.unsigned_abs(),
                    (rule.standard_offset_seconds + rule.inner.additional_offset_secs)
                        .unsigned_abs(),
                );
                for date in [&rule.inner.start, &rule.inner.end] {
                    let seconds_of_day = date.transition_time;
                    if date.month == 0 && date.day == 1 {
                        assert!(
                            seconds_of_day > max_delta,
                            "rule at beginning should not cross year boundary {seconds_of_day} > Δ{max_delta} ({iana})"
                        );
                    }
                    if date.month == 11 && date.day == 31 {
                        assert!(
                            seconds_of_day + max_delta < SECONDS_IN_UTC_DAY as u32,
                            "rule at end of year should not cross year boundary {seconds_of_day} + Δ{max_delta} < 24h ({iana})"
                        );
                    }
                }
            }
        }
    }

    /// Tests an invariant we rely on in our code
    #[test]
    fn test_rule_offset_positive() {
        for chrono in crate::tests::time_zones_to_test() {
            let iana = chrono.name();
            let zoneinfo64 = TZDB.get(iana).unwrap().simple();

            if let Some(rule) = zoneinfo64.final_rule(&TZDB.rules) {
                assert!(
                    rule.inner.additional_offset_secs > 0,
                    "additional offset should be positive, is {} ({iana})",
                    rule.inner.additional_offset_secs,
                );
            }
        }
    }

    #[test]
    fn test_offset_before_rule_is_second_offset() {
        for chrono in crate::tests::time_zones_to_test() {
            let iana = chrono.name();
            let zoneinfo64 = TZDB.get(iana).unwrap().simple();

            if let Some(rule) = zoneinfo64.final_rule(&TZDB.rules) {
                let last_transition =
                    zoneinfo64.transition_offset_at(zoneinfo64.transition_count() - 1);

                if rule.inner.end_before_start() {
                    assert!(last_transition.rule_applies, "{iana}, {zoneinfo64:?}");

                    assert_eq!(
                        last_transition.offset,
                        UtcOffset(rule.standard_offset_seconds + rule.inner.additional_offset_secs),
                        "{iana}, {zoneinfo64:?}"
                    );
                } else {
                    assert!(!last_transition.rule_applies, "{iana}, {zoneinfo64:?}");

                    assert_eq!(
                        last_transition.offset,
                        UtcOffset(rule.standard_offset_seconds),
                        "{iana}, {zoneinfo64:?}"
                    );
                }
            }
        }
    }

    fn test_single_year(
        tz: &str,
        year: i32,
        (start_month, start_day, (start_before, start_after)): (u8, u8, (i8, i8)),
        (end_month, end_day, (end_before, end_after)): (u8, u8, (i8, i8)),
    ) {
        let zone = TZDB.get(tz).unwrap();

        // start_before doesn't actually happen
        assert!(matches!(
            zone.for_date_time(
                year,
                start_month,
                start_day - start_before.div_euclid(24).unsigned_abs(),
                start_before.rem_euclid(24).unsigned_abs(),
                0,
                0
            ),
            PossibleOffset::None { .. }
        ));

        // start_after happens exactly once
        assert!(matches!(
            zone.for_date_time(
                year,
                start_month,
                start_day - start_after.div_euclid(24).unsigned_abs(),
                start_after.rem_euclid(24).unsigned_abs(),
                0,
                0
            ),
            PossibleOffset::Single(_)
        ));

        // end_before happens exactly once
        assert!(matches!(
            zone.for_date_time(
                year,
                end_month,
                end_day - end_before.div_euclid(24).unsigned_abs(),
                end_before.rem_euclid(24).unsigned_abs(),
                0,
                0
            ),
            PossibleOffset::Single(_)
        ));

        // end_after happens again after falling back
        assert!(matches!(
            zone.for_date_time(
                year,
                end_month,
                end_day - end_after.div_euclid(24).unsigned_abs(),
                end_after.rem_euclid(24).unsigned_abs(),
                0,
                0
            ),
            PossibleOffset::Ambiguous { .. },
        ));
    }

    #[test]
    fn test_los_angeles() {
        // This is a Wall rule
        // so the transition happens at the same time in the
        // previous timezone
        test_single_year(
            "America/Los_Angeles",
            2025,
            // The transition happens at 02:00 in the previous offset
            // and 03:00/01:00 in the next
            (3, 9, (2, 3)),
            (11, 2, (2, 1)),
        );
    }

    #[test]
    fn test_london() {
        // This is a Standard rule, so the transition happens
        // at the same time in the standard timezone
        test_single_year("Europe/London", 2017, (3, 26, (1, 2)), (10, 29, (2, 1)));
    }

    #[test]
    fn test_santiago() {
        // This is a Utc rule, so the transition happens
        // at the same time in UTC
        test_single_year(
            "America/Santiago",
            2025,
            // Note: this is in the southern hemisphere,
            // the transition start is later in the year
            (9, 7, (0, 1)),
            // The transition day is April 6, but the backwards
            // transition briefly puts us back in April 5, so we get a -1
            (4, 6, (0, -1)),
        );
    }

    // DOW_IN_MONTH is not exercised by TZDB
    #[test]
    fn day_of_week_in_month() {
        // First Wednesday in August 2025
        assert_eq!(
            TzRuleDate {
                mode: RuleMode::DOW_IN_MONTH,
                day: 1,
                day_of_week: 3,
                month: 8,
                transition_time: 0,
                time_mode: TimeMode::Utc,
            }
            .day_in_year(2025, calendrical_calculations::iso::day_before_year(2025)),
            calendrical_calculations::iso::days_before_month(2025, 8) + 6
        );

        // Third Saturday in August 2025
        assert_eq!(
            TzRuleDate {
                mode: RuleMode::DOW_IN_MONTH,
                day: 3,
                day_of_week: 6,
                month: 8,
                transition_time: 0,
                time_mode: TimeMode::Utc,
            }
            .day_in_year(2025, calendrical_calculations::iso::day_before_year(2025)),
            calendrical_calculations::iso::days_before_month(2025, 8) + 16
        );
    }
}
