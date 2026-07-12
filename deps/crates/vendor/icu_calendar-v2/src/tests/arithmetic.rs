// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::options::{DateAddOptions, DateDifferenceOptions, DateDurationUnit, Overflow};
use crate::types::{DateDuration, Month};
use crate::{AsCalendar, Calendar, Date};
use core::fmt;

struct ArithmeticDateForLogging {
    pub year: i32,
    pub month: Month,
    pub day: u8,
}

impl<C> From<Date<C>> for ArithmeticDateForLogging
where
    C: Calendar,
{
    fn from(value: Date<C>) -> Self {
        Self {
            year: value.year().extended_year(),
            month: value.month().to_input(),
            day: value.day_of_month().0,
        }
    }
}

impl fmt::Display for ArithmeticDateForLogging {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{:4}.{}.{:<2}", self.year, self.month.code().0, self.day)
    }
}

struct DateDurationForLogging(DateDuration);

impl fmt::Display for DateDurationForLogging {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "{} {:2}y {:2}m {:2}w {:2}d",
            if self.0.is_negative { '-' } else { '+' },
            self.0.years,
            self.0.months,
            self.0.weeks,
            self.0.days
        )
    }
}

struct TestOutput {
    pub cal: &'static str,
    pub start: ArithmeticDateForLogging,
    pub end: ArithmeticDateForLogging,
    pub duration: DateDurationForLogging,
    pub calculated_duration: DateDurationForLogging,
    pub is_rejected: bool,
}

impl fmt::Display for TestOutput {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "{} {} = {} ({})",
            self.start, self.duration, self.end, self.cal
        )?;
        if self.duration.0 != self.calculated_duration.0 {
            write!(f, "; round-trip duration: {}", self.calculated_duration)?;
        }
        if self.is_rejected {
            write!(f, "; rejected")?;
        }
        Ok(())
    }
}

struct TestOutputs(Vec<TestOutput>);

impl fmt::Display for TestOutputs {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        for output in self.0.iter() {
            writeln!(f, "{}", output)?;
        }
        Ok(())
    }
}

super::test_all_cals!(
    fn test_arithmetic<C: Calendar + Copy>(cal: C) {
        fn new_duration(years: i32, months: i32, weeks: i32, days: i32) -> DateDuration {
            let is_negative = years < 0 || months < 0 || weeks < 0 || days < 0;
            // Verify no mixed signs
            if is_negative {
                assert!(years <= 0 && months <= 0 && weeks <= 0 && days <= 0);
            } else {
                assert!(years >= 0 && months >= 0 && weeks >= 0 && days >= 0);
            }
            DateDuration {
                is_negative,
                years: years.unsigned_abs(),
                months: months.unsigned_abs(),
                weeks: weeks.unsigned_abs(),
                days: days.unsigned_abs(),
            }
        }

        let mut durations = Vec::new();

        // Check +/- 30 months
        for i in (-30..=30).filter(|i| *i != 0) {
            durations.push(new_duration(0, i, 0, 0));
        }

        // Check +/- 30 months with +/- 1 day
        for i in (-30i32..=30).filter(|i| *i != 0) {
            let days = i.signum();
            durations.push(new_duration(0, i, 0, days));
        }

        // Check +/- 10 years
        for i in (-10..=10).filter(|i| *i != 0) {
            durations.push(new_duration(i, 0, 0, 0));
        }

        // Check +/- 10 years with +/- 1 month
        for i in (-10i32..=10).filter(|i| *i != 0) {
            let months = i.signum();
            durations.push(new_duration(i, months, 0, 0));
        }

        // Check +/- 10 years with +/- 1 day
        for i in (-10i32..=10).filter(|i| *i != 0) {
            let days = i.signum();
            durations.push(new_duration(i, 0, 0, days));
        }

        // Check +/- 10 years with +/- 1 month and +/- 1 day
        for i in (-10i32..=10).filter(|i| *i != 0) {
            let s = i.signum();
            durations.push(new_duration(i, s, 0, s));
        }

        let start_date = Date::try_new_iso(2000, 1, 1).unwrap();
        let end_date = Date::try_new_iso(2004, 12, 31).unwrap();
        let start_rd = start_date.to_rata_die();
        let end_rd = end_date.to_rata_die();

        let add_constrain = DateAddOptions {
            overflow: Some(Overflow::Constrain),
        };
        let add_reject = DateAddOptions {
            overflow: Some(Overflow::Reject),
        };

        let mut outputs = TestOutputs(Vec::new());

        for rd_offset in 0..=(end_rd - start_rd) {
            let date = Date::from_rata_die(start_rd + rd_offset, cal);
            if date.day_of_month().0 > 2 && date.day_of_month().0 < 27 {
                // Most interesting cases are at the beginning or end of the month
                continue;
            }

            for duration in &durations {
                let mut diff_options = DateDifferenceOptions::default();
                if duration.years != 0 {
                    diff_options.largest_unit = Some(DateDurationUnit::Years);
                } else if duration.months != 0 {
                    diff_options.largest_unit = Some(DateDurationUnit::Months);
                } else {
                    diff_options.largest_unit = Some(DateDurationUnit::Days);
                }

                let added_date = date
                    .try_added_with_options(*duration, add_constrain)
                    .unwrap_or_else(|_| {
                        panic!(
                            "Failed to add duration {:?} to date {:?} in calendar {:?}",
                            duration,
                            date,
                            cal.as_calendar().debug_name()
                        )
                    });

                let calculated_duration = date
                    .try_until_with_options(&added_date, diff_options)
                    .unwrap_or_else(|_| {
                        panic!(
                            "Failed to calculate difference between {:?} and {:?} in calendar {:?}",
                            date,
                            added_date,
                            cal.as_calendar().debug_name()
                        )
                    });

                let is_rejected = date.try_added_with_options(*duration, add_reject).is_err();

                let output = TestOutput {
                    cal: cal.debug_name(),
                    start: date.into(),
                    end: added_date.into(),
                    duration: DateDurationForLogging(*duration),
                    calculated_duration: DateDurationForLogging(calculated_duration),
                    is_rejected,
                };

                // The durations should have the same sign!
                assert_eq!(
                    duration.is_negative, calculated_duration.is_negative,
                    "{output}"
                );
                assert_eq!(duration.is_negative, date > added_date, "{output}");

                // Round-trip check
                let added_back = date
                    .try_added_with_options(calculated_duration, add_constrain)
                    .unwrap();
                assert_eq!(
                    added_back, added_date,
                    "Round trip failed for {:?} + {:?} -> {:?}. Got duration {:?} which led to {:?}",
                    date, duration, added_date, calculated_duration, added_back
                );

                // Either test the duration against a known result or add it to the snapshot
                assert_eq!(duration.weeks, 0);
                if duration.months == 0 && duration.days == 0 {
                    // Years should have arithmetic behavior.
                    assert_eq!(
                        duration.add_years_to(date.year().extended_year()),
                        added_date.year().extended_year(),
                        "{output}"
                    );
                    // Month or day could constrain; add the ones that do to the snapshot.
                    if date.month().to_input() != added_date.month().to_input()
                        || date.day_of_month() != added_date.day_of_month()
                    {
                        assert!(is_rejected, "should reject: {output}");
                        outputs.0.push(output);
                    } else {
                        // Make sure we aren't skipping any normalized-duration tests
                        assert!(!is_rejected, "should NOT reject: {output}");
                        assert_eq!(*duration, calculated_duration);
                    }
                } else if duration.years == 0 && duration.days == 0 {
                    // Months should have arithmetic behavior.
                    let earlier = std::cmp::min(date, added_date);
                    let later = std::cmp::max(date, added_date);
                    let mut month_diff = 0;
                    month_diff -= earlier.month().ordinal as i32;
                    month_diff += later.month().ordinal as i32;
                    for y in earlier.year().extended_year()..later.year().extended_year() {
                        month_diff += Date::try_new(y.into(), Month::new(1), 1, cal)
                            .unwrap()
                            .months_in_year() as i32;
                    }
                    assert_eq!(duration.months, month_diff.unsigned_abs(), "{output}");
                    // Days could constrain; add the ones that do to the snapshot.
                    if date.day_of_month() != added_date.day_of_month() {
                        assert!(is_rejected, "should reject: {output}");
                        outputs.0.push(output);
                    } else {
                        // Make sure we aren't skipping any normalized-duration tests
                        assert!(!is_rejected, "should NOT reject: {output}");
                        assert_eq!(*duration, calculated_duration);
                    }
                } else if duration.years != 0 && duration.months != 0 && duration.days == 0 {
                    // These cases are some of the trickiest. Toss them all in the snapshots!
                    outputs.0.push(output);
                } else if duration.years == 0 && duration.months != 0 && duration.days == 1 {
                    // This should add months, constrain, and add a day.
                    // Month-constrain by itself is tested by other cases,
                    // which also test Overflow::Reject behavior.
                    let signed_months = duration.add_months_to(0);
                    let expected_rd = date
                        .try_added_with_options(
                            DateDuration::for_months(signed_months),
                            add_constrain,
                        )
                        .unwrap()
                        .to_rata_die()
                        + i64::from(signed_months.signum());
                    assert_eq!(added_date.to_rata_die(), expected_rd, "{output}");
                    // Only add the snapshot if the duration was normalized
                    if *duration != calculated_duration {
                        outputs.0.push(output);
                    }
                } else if duration.years != 0 && duration.months == 0 && duration.days == 1 {
                    // This should add years, constrain, and add a day.
                    // Year-constrain by itself is tested by other cases,
                    // which also test Overflow::Reject behavior.
                    let signed_years = duration.add_years_to(0);
                    let expected_rd = date
                        .try_added_with_options(
                            DateDuration::for_years(signed_years),
                            add_constrain,
                        )
                        .unwrap()
                        .to_rata_die()
                        + i64::from(signed_years.signum());
                    assert_eq!(added_date.to_rata_die(), expected_rd, "{output}");
                    // Only add the snapshot if the duration was normalized
                    if *duration != calculated_duration {
                        outputs.0.push(output);
                    }
                } else if duration.years != 0 && duration.months != 0 && duration.days == 1 {
                    // This should add years and months, constrain, and add a day.
                    // Year-month-constrain by itself is tested by other cases,
                    // which also test Overflow::Reject behavior.
                    let mut year_month_duration = *duration;
                    year_month_duration.days = 0;
                    let expected_rd = date
                        .try_added_with_options(year_month_duration, add_constrain)
                        .unwrap()
                        .to_rata_die()
                        + if duration.is_negative { -1i64 } else { 1i64 };
                    assert_eq!(added_date.to_rata_die(), expected_rd, "{output}");
                    // Only add the snapshot if the duration was normalized
                    if *duration != calculated_duration {
                        outputs.0.push(output);
                    }
                } else {
                    panic!("Not covered: {output}");
                }
            }
        }

        insta::assert_snapshot!(format!("date_arithmetic_{}", cal.debug_name()), outputs);
    }
);

super::test_all_cals!(
    fn test_day_arithmetic<C: Calendar + Copy>(cal: C) {
        let start_date = Date::try_new_iso(2000, 1, 1).unwrap();
        let end_date = Date::try_new_iso(2004, 12, 31).unwrap();
        let start_rd = start_date.to_rata_die();
        let end_rd = end_date.to_rata_die();

        let add_constrain = DateAddOptions {
            overflow: Some(Overflow::Constrain),
        };
        let add_reject = DateAddOptions {
            overflow: Some(Overflow::Reject),
        };
        let diff_options = DateDifferenceOptions {
            largest_unit: Some(DateDurationUnit::Days),
        };

        for rd_offset in 0..=(end_rd - start_rd) {
            let date = Date::from_rata_die(start_rd + rd_offset, cal);

            // Check +/- 65 days
            for i in -65..=65 {
                let duration = DateDuration::for_days(i);
                let added_date = date
                    .try_added_with_options(duration, add_constrain)
                    .unwrap();
                let is_rejected = date.try_added_with_options(duration, add_reject).is_err();
                let calculated_duration = date
                    .try_until_with_options(&added_date, diff_options)
                    .unwrap();
                assert_eq!(duration, calculated_duration);
                assert_eq!(i, (added_date.to_rata_die() - date.to_rata_die()) as i32);
                assert!(!is_rejected, "should NOT reject");
            }
        }
    }
);
