// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Functions for region-specific weekday information.

use crate::{error::RangeError, provider::*, types::Weekday};
use icu_locale_core::preferences::{define_preferences, extensions::unicode::keywords::FirstDay};
use icu_provider::prelude::*;

/// Minimum number of days in a month unit required for using this module
const MIN_UNIT_DAYS: u16 = 14;

define_preferences!(
    /// The preferences for the week information.
    [Copy]
    WeekPreferences,
    {
        /// The first day of the week
        first_weekday: FirstDay
    }
);

/// Information about the first day of the week and the weekend.
#[derive(Clone, Copy, Debug)]
#[non_exhaustive]
pub struct WeekInformation {
    /// The first day of a week.
    pub first_weekday: Weekday,
    /// The set of weekend days
    pub weekend: WeekdaySet,
}

impl WeekInformation {
    icu_provider::gen_buffer_data_constructors!(
        (prefs: WeekPreferences) -> error: DataError,
        /// Creates a new [`WeekCalculator`] from compiled data.
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new)]
    pub fn try_new_unstable<P>(provider: &P, prefs: WeekPreferences) -> Result<Self, DataError>
    where
        P: DataProvider<crate::provider::CalendarWeekV1> + ?Sized,
    {
        let locale = CalendarWeekV1::make_locale(prefs.locale_preferences);
        provider
            .load(DataRequest {
                id: DataIdentifierBorrowed::for_locale(&locale),
                ..Default::default()
            })
            .map(|response| WeekInformation {
                first_weekday: match prefs.first_weekday {
                    Some(FirstDay::Mon) => Weekday::Monday,
                    Some(FirstDay::Tue) => Weekday::Tuesday,
                    Some(FirstDay::Wed) => Weekday::Wednesday,
                    Some(FirstDay::Thu) => Weekday::Thursday,
                    Some(FirstDay::Fri) => Weekday::Friday,
                    Some(FirstDay::Sat) => Weekday::Saturday,
                    Some(FirstDay::Sun) => Weekday::Sunday,
                    _ => response.payload.get().first_weekday,
                },
                weekend: response.payload.get().weekend,
            })
    }

    /// Weekdays that are part of the 'weekend', for calendar purposes.
    /// Days may not be contiguous, and order is based off the first weekday.
    pub fn weekend(self) -> WeekdaySetIterator {
        WeekdaySetIterator::new(self.first_weekday, self.weekend)
    }
}

#[derive(Clone, Copy, Debug)]
pub(crate) struct WeekCalculator {
    first_weekday: Weekday,
    min_week_days: u8,
}

impl WeekCalculator {
    pub(crate) const ISO: Self = Self {
        first_weekday: Weekday::Monday,
        min_week_days: 4,
    };

    /// Returns the zero based index of `weekday` vs this calendar's start of week.
    fn weekday_index(self, weekday: Weekday) -> i8 {
        (7 + (weekday as i8) - (self.first_weekday as i8)) % 7
    }

    /// Computes & returns the week of given month/year according to `calendar`.
    ///
    /// # Arguments
    ///  - calendar: Calendar information used to compute the week number.
    ///  - num_days_in_previous_unit: The number of days in the preceding month/year.
    ///  - num_days_in_unit: The number of days in the month/year.
    ///  - day: 1-based day of month/year.
    ///  - week_day: The weekday of `day`..
    ///
    /// # Error
    /// If num_days_in_unit/num_days_in_previous_unit < MIN_UNIT_DAYS
    pub(crate) fn week_of(
        self,
        num_days_in_previous_unit: u16,
        num_days_in_unit: u16,
        day: u16,
        week_day: Weekday,
    ) -> Result<WeekOf, RangeError> {
        let current = UnitInfo::new(
            // The first day of this month/year is (day - 1) days from `day`.
            add_to_weekday(week_day, 1 - i32::from(day)),
            num_days_in_unit,
        )?;

        match current.relative_week(self, day) {
            RelativeWeek::LastWeekOfPreviousUnit => {
                let previous = UnitInfo::new(
                    add_to_weekday(current.first_day, -i32::from(num_days_in_previous_unit)),
                    num_days_in_previous_unit,
                )?;

                Ok(WeekOf {
                    week: previous.num_weeks(self),
                    unit: RelativeUnit::Previous,
                })
            }
            RelativeWeek::WeekOfCurrentUnit(w) => Ok(WeekOf {
                week: w,
                unit: RelativeUnit::Current,
            }),
            RelativeWeek::FirstWeekOfNextUnit => Ok(WeekOf {
                week: 1,
                unit: RelativeUnit::Next,
            }),
        }
    }
}

/// Returns the weekday that's `num_days` after `weekday`.
fn add_to_weekday(weekday: Weekday, num_days: i32) -> Weekday {
    let new_weekday = (7 + (weekday as i32) + (num_days % 7)) % 7;
    Weekday::from_days_since_sunday(new_weekday as isize)
}

/// Which year or month that a calendar assigns a week to relative to the year/month
/// the week is in.
#[derive(Clone, Copy, Debug, PartialEq)]
#[allow(clippy::enum_variant_names)]
enum RelativeWeek {
    /// A week that is assigned to the last week of the previous year/month. e.g. 2021-01-01 is week 54 of 2020 per the ISO calendar.
    LastWeekOfPreviousUnit,
    /// A week that's assigned to the current year/month. The offset is 1-based. e.g. 2021-01-11 is week 2 of 2021 per the ISO calendar so would be WeekOfCurrentUnit(2).
    WeekOfCurrentUnit(u8),
    /// A week that is assigned to the first week of the next year/month. e.g. 2019-12-31 is week 1 of 2020 per the ISO calendar.
    FirstWeekOfNextUnit,
}

/// Information about a year or month.
#[derive(Clone, Copy)]
struct UnitInfo {
    /// The weekday of this year/month's first day.
    first_day: Weekday,
    /// The number of days in this year/month.
    duration_days: u16,
}

impl UnitInfo {
    /// Creates a UnitInfo for a given year or month.
    fn new(first_day: Weekday, duration_days: u16) -> Result<UnitInfo, RangeError> {
        if duration_days < MIN_UNIT_DAYS {
            return Err(RangeError {
                field: "num_days_in_unit",
                value: duration_days as i32,
                min: MIN_UNIT_DAYS as i32,
                max: i32::MAX,
            });
        }
        Ok(UnitInfo {
            first_day,
            duration_days,
        })
    }

    /// Returns the start of this unit's first week.
    ///
    /// The returned value can be negative if this unit's first week started during the previous
    /// unit.
    fn first_week_offset(self, calendar: WeekCalculator) -> i8 {
        let first_day_index = calendar.weekday_index(self.first_day);
        if 7 - first_day_index >= calendar.min_week_days as i8 {
            -first_day_index
        } else {
            7 - first_day_index
        }
    }

    /// Returns the number of weeks in this unit according to `calendar`.
    fn num_weeks(self, calendar: WeekCalculator) -> u8 {
        let first_week_offset = self.first_week_offset(calendar);
        let num_days_including_first_week =
            (self.duration_days as i32) - (first_week_offset as i32);
        debug_assert!(
            num_days_including_first_week >= 0,
            "Unit is shorter than a week."
        );
        ((num_days_including_first_week + 7 - (calendar.min_week_days as i32)) / 7) as u8
    }

    /// Returns the week number for the given day in this unit.
    fn relative_week(self, calendar: WeekCalculator, day: u16) -> RelativeWeek {
        let days_since_first_week =
            i32::from(day) - i32::from(self.first_week_offset(calendar)) - 1;
        if days_since_first_week < 0 {
            return RelativeWeek::LastWeekOfPreviousUnit;
        }

        let week_number = (1 + days_since_first_week / 7) as u8;
        if week_number > self.num_weeks(calendar) {
            return RelativeWeek::FirstWeekOfNextUnit;
        }
        RelativeWeek::WeekOfCurrentUnit(week_number)
    }
}

/// The year or month that a calendar assigns a week to relative to the year/month that it is in.
#[derive(Debug, PartialEq)]
#[allow(clippy::exhaustive_enums)] // this type is stable
pub(crate) enum RelativeUnit {
    /// A week that is assigned to previous year/month. e.g. 2021-01-01 is week 54 of 2020 per the ISO calendar.
    Previous,
    /// A week that's assigned to the current year/month. e.g. 2021-01-11 is week 2 of 2021 per the ISO calendar.
    Current,
    /// A week that is assigned to the next year/month. e.g. 2019-12-31 is week 1 of 2020 per the ISO calendar.
    Next,
}

/// The week number assigned to a given week according to a calendar.
#[derive(Debug, PartialEq)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub(crate) struct WeekOf {
    /// Week of month/year. 1 based.
    pub week: u8,
    /// The month/year that this week is in, relative to the month/year of the input date.
    pub unit: RelativeUnit,
}

/// [Iterator] that yields weekdays that are part of the weekend.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct WeekdaySetIterator {
    /// Determines the order in which we should start reading values from `weekend`.
    first_weekday: Weekday,
    /// Day being evaluated.
    current_day: Weekday,
    /// Bitset to read weekdays from.
    weekend: WeekdaySet,
}

impl WeekdaySetIterator {
    /// Creates the Iterator. Sets `current_day` to the day after `first_weekday`.
    pub(crate) fn new(first_weekday: Weekday, weekend: WeekdaySet) -> Self {
        WeekdaySetIterator {
            first_weekday,
            current_day: first_weekday,
            weekend,
        }
    }
}

impl Iterator for WeekdaySetIterator {
    type Item = Weekday;

    fn next(&mut self) -> Option<Self::Item> {
        // Check each bit until we find one that is ON or until we are back to the start of the week.
        while self.current_day.next_day() != self.first_weekday {
            if self.weekend.contains(self.current_day) {
                let result = self.current_day;
                self.current_day = self.current_day.next_day();
                return Some(result);
            } else {
                self.current_day = self.current_day.next_day();
            }
        }

        if self.weekend.contains(self.current_day) {
            // Clear weekend, we've seen all bits.
            // Breaks the loop next time `next()` is called
            self.weekend = WeekdaySet::new(&[]);
            return Some(self.current_day);
        }

        Option::None
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{types::Weekday, Date, DateDuration, RangeError};

    static ISO_CALENDAR: WeekCalculator = WeekCalculator {
        first_weekday: Weekday::Monday,
        min_week_days: 4,
    };

    static AE_CALENDAR: WeekCalculator = WeekCalculator {
        first_weekday: Weekday::Saturday,
        min_week_days: 4,
    };

    static US_CALENDAR: WeekCalculator = WeekCalculator {
        first_weekday: Weekday::Sunday,
        min_week_days: 1,
    };

    #[test]
    fn test_weekday_index() {
        assert_eq!(ISO_CALENDAR.weekday_index(Weekday::Monday), 0);
        assert_eq!(ISO_CALENDAR.weekday_index(Weekday::Sunday), 6);

        assert_eq!(AE_CALENDAR.weekday_index(Weekday::Saturday), 0);
        assert_eq!(AE_CALENDAR.weekday_index(Weekday::Friday), 6);
    }

    #[test]
    fn test_first_week_offset() {
        let first_week_offset =
            |calendar, day| UnitInfo::new(day, 30).unwrap().first_week_offset(calendar);
        assert_eq!(first_week_offset(ISO_CALENDAR, Weekday::Monday), 0);
        assert_eq!(first_week_offset(ISO_CALENDAR, Weekday::Tuesday), -1);
        assert_eq!(first_week_offset(ISO_CALENDAR, Weekday::Wednesday), -2);
        assert_eq!(first_week_offset(ISO_CALENDAR, Weekday::Thursday), -3);
        assert_eq!(first_week_offset(ISO_CALENDAR, Weekday::Friday), 3);
        assert_eq!(first_week_offset(ISO_CALENDAR, Weekday::Saturday), 2);
        assert_eq!(first_week_offset(ISO_CALENDAR, Weekday::Sunday), 1);

        assert_eq!(first_week_offset(AE_CALENDAR, Weekday::Saturday), 0);
        assert_eq!(first_week_offset(AE_CALENDAR, Weekday::Sunday), -1);
        assert_eq!(first_week_offset(AE_CALENDAR, Weekday::Monday), -2);
        assert_eq!(first_week_offset(AE_CALENDAR, Weekday::Tuesday), -3);
        assert_eq!(first_week_offset(AE_CALENDAR, Weekday::Wednesday), 3);
        assert_eq!(first_week_offset(AE_CALENDAR, Weekday::Thursday), 2);
        assert_eq!(first_week_offset(AE_CALENDAR, Weekday::Friday), 1);

        assert_eq!(first_week_offset(US_CALENDAR, Weekday::Sunday), 0);
        assert_eq!(first_week_offset(US_CALENDAR, Weekday::Monday), -1);
        assert_eq!(first_week_offset(US_CALENDAR, Weekday::Tuesday), -2);
        assert_eq!(first_week_offset(US_CALENDAR, Weekday::Wednesday), -3);
        assert_eq!(first_week_offset(US_CALENDAR, Weekday::Thursday), -4);
        assert_eq!(first_week_offset(US_CALENDAR, Weekday::Friday), -5);
        assert_eq!(first_week_offset(US_CALENDAR, Weekday::Saturday), -6);
    }

    #[test]
    fn test_num_weeks() {
        // 4 days in first & last week.
        assert_eq!(
            UnitInfo::new(Weekday::Thursday, 4 + 2 * 7 + 4)
                .unwrap()
                .num_weeks(ISO_CALENDAR),
            4
        );
        // 3 days in first week, 4 in last week.
        assert_eq!(
            UnitInfo::new(Weekday::Friday, 3 + 2 * 7 + 4)
                .unwrap()
                .num_weeks(ISO_CALENDAR),
            3
        );
        // 3 days in first & last week.
        assert_eq!(
            UnitInfo::new(Weekday::Friday, 3 + 2 * 7 + 3)
                .unwrap()
                .num_weeks(ISO_CALENDAR),
            2
        );

        // 1 day in first & last week.
        assert_eq!(
            UnitInfo::new(Weekday::Saturday, 1 + 2 * 7 + 1)
                .unwrap()
                .num_weeks(US_CALENDAR),
            4
        );
    }

    /// Uses enumeration & bucketing to assign each day of a month or year `unit` to a week.
    ///
    /// This alternative implementation serves as an exhaustive safety check
    /// of relative_week() (in addition to the manual test points used
    /// for testing week_of()).
    fn classify_days_of_unit(calendar: WeekCalculator, unit: &UnitInfo) -> Vec<RelativeWeek> {
        let mut weeks: Vec<Vec<Weekday>> = Vec::new();
        for day_index in 0..unit.duration_days {
            let day = super::add_to_weekday(unit.first_day, i32::from(day_index));
            if day == calendar.first_weekday || weeks.is_empty() {
                weeks.push(Vec::new());
            }
            weeks.last_mut().unwrap().push(day);
        }

        let mut day_week_of_units = Vec::new();
        let mut weeks_in_unit = 0;
        for (index, week) in weeks.iter().enumerate() {
            let week_of_unit = if week.len() < usize::from(calendar.min_week_days) {
                match index {
                    0 => RelativeWeek::LastWeekOfPreviousUnit,
                    x if x == weeks.len() - 1 => RelativeWeek::FirstWeekOfNextUnit,
                    _ => panic!(),
                }
            } else {
                weeks_in_unit += 1;
                RelativeWeek::WeekOfCurrentUnit(weeks_in_unit)
            };

            day_week_of_units.append(&mut [week_of_unit].repeat(week.len()));
        }
        day_week_of_units
    }

    #[test]
    fn test_relative_week_of_month() {
        for min_week_days in 1..7 {
            for start_of_week in 1..7 {
                let calendar = WeekCalculator {
                    first_weekday: Weekday::from_days_since_sunday(start_of_week),
                    min_week_days,
                };
                for unit_duration in super::MIN_UNIT_DAYS..400 {
                    for start_of_unit in 1..7 {
                        let unit = UnitInfo::new(
                            Weekday::from_days_since_sunday(start_of_unit),
                            unit_duration,
                        )
                        .unwrap();
                        let expected = classify_days_of_unit(calendar, &unit);
                        for (index, expected_week_of) in expected.iter().enumerate() {
                            let day = index + 1;
                            assert_eq!(
                                unit.relative_week(calendar, day as u16),
                                *expected_week_of,
                                "For the {day}/{unit_duration} starting on Weekday \
                        {start_of_unit} using start_of_week {start_of_week} \
                        & min_week_days {min_week_days}"
                            );
                        }
                    }
                }
            }
        }
    }

    fn week_of_month_from_iso_date(
        calendar: WeekCalculator,
        yyyymmdd: u32,
    ) -> Result<WeekOf, RangeError> {
        let year = (yyyymmdd / 10000) as i32;
        let month = ((yyyymmdd / 100) % 100) as u8;
        let day = (yyyymmdd % 100) as u8;

        let date = Date::try_new_iso(year, month, day)?;
        let previous_month = date.added(DateDuration::new(0, -1, 0, 0));

        calendar.week_of(
            u16::from(previous_month.days_in_month()),
            u16::from(date.days_in_month()),
            u16::from(day),
            date.day_of_week(),
        )
    }

    #[test]
    fn test_week_of_month_using_dates() {
        assert_eq!(
            week_of_month_from_iso_date(ISO_CALENDAR, 20210418).unwrap(),
            WeekOf {
                week: 3,
                unit: RelativeUnit::Current,
            }
        );
        assert_eq!(
            week_of_month_from_iso_date(ISO_CALENDAR, 20210419).unwrap(),
            WeekOf {
                week: 4,
                unit: RelativeUnit::Current,
            }
        );

        // First day of year is a Thursday.
        assert_eq!(
            week_of_month_from_iso_date(ISO_CALENDAR, 20180101).unwrap(),
            WeekOf {
                week: 1,
                unit: RelativeUnit::Current,
            }
        );
        // First day of year is a Friday.
        assert_eq!(
            week_of_month_from_iso_date(ISO_CALENDAR, 20210101).unwrap(),
            WeekOf {
                week: 5,
                unit: RelativeUnit::Previous,
            }
        );

        // The month ends on a Wednesday.
        assert_eq!(
            week_of_month_from_iso_date(ISO_CALENDAR, 20200930).unwrap(),
            WeekOf {
                week: 1,
                unit: RelativeUnit::Next,
            }
        );
        // The month ends on a Thursday.
        assert_eq!(
            week_of_month_from_iso_date(ISO_CALENDAR, 20201231).unwrap(),
            WeekOf {
                week: 5,
                unit: RelativeUnit::Current,
            }
        );

        // US calendar always assigns the week to the current month. 2020-12-31 is a Thursday.
        assert_eq!(
            week_of_month_from_iso_date(US_CALENDAR, 20201231).unwrap(),
            WeekOf {
                week: 5,
                unit: RelativeUnit::Current,
            }
        );
        assert_eq!(
            week_of_month_from_iso_date(US_CALENDAR, 20210101).unwrap(),
            WeekOf {
                week: 1,
                unit: RelativeUnit::Current,
            }
        );
    }
}

#[test]
fn test_first_day() {
    use icu_locale_core::locale;

    assert_eq!(
        WeekInformation::try_new(locale!("und-US").into())
            .unwrap()
            .first_weekday,
        Weekday::Sunday,
    );

    assert_eq!(
        WeekInformation::try_new(locale!("und-FR").into())
            .unwrap()
            .first_weekday,
        Weekday::Monday,
    );

    assert_eq!(
        WeekInformation::try_new(locale!("und-FR-u-fw-tue").into())
            .unwrap()
            .first_weekday,
        Weekday::Tuesday,
    );
}

#[test]
fn test_weekend() {
    use icu_locale_core::locale;

    assert_eq!(
        WeekInformation::try_new(locale!("und").into())
            .unwrap()
            .weekend()
            .collect::<Vec<_>>(),
        vec![Weekday::Saturday, Weekday::Sunday],
    );

    assert_eq!(
        WeekInformation::try_new(locale!("und-FR").into())
            .unwrap()
            .weekend()
            .collect::<Vec<_>>(),
        vec![Weekday::Saturday, Weekday::Sunday],
    );

    assert_eq!(
        WeekInformation::try_new(locale!("und-IQ").into())
            .unwrap()
            .weekend()
            .collect::<Vec<_>>(),
        vec![Weekday::Saturday, Weekday::Friday],
    );

    assert_eq!(
        WeekInformation::try_new(locale!("und-IR").into())
            .unwrap()
            .weekend()
            .collect::<Vec<_>>(),
        vec![Weekday::Friday],
    );
}

#[test]
fn test_weekdays_iter() {
    use Weekday::*;

    // Weekend ends one day before week starts
    let default_weekend = WeekdaySetIterator::new(Monday, WeekdaySet::new(&[Saturday, Sunday]));
    assert_eq!(vec![Saturday, Sunday], default_weekend.collect::<Vec<_>>());

    // Non-contiguous weekend
    let fri_sun_weekend = WeekdaySetIterator::new(Monday, WeekdaySet::new(&[Friday, Sunday]));
    assert_eq!(vec![Friday, Sunday], fri_sun_weekend.collect::<Vec<_>>());

    let multiple_contiguous_days = WeekdaySetIterator::new(
        Monday,
        WeekdaySet::new(&[
            Weekday::Tuesday,
            Weekday::Wednesday,
            Weekday::Thursday,
            Weekday::Friday,
        ]),
    );
    assert_eq!(
        vec![Tuesday, Wednesday, Thursday, Friday],
        multiple_contiguous_days.collect::<Vec<_>>()
    );

    // Non-contiguous days and iterator yielding elements based off first_weekday
    let multiple_non_contiguous_days = WeekdaySetIterator::new(
        Wednesday,
        WeekdaySet::new(&[
            Weekday::Tuesday,
            Weekday::Thursday,
            Weekday::Friday,
            Weekday::Sunday,
        ]),
    );
    assert_eq!(
        vec![Thursday, Friday, Sunday, Tuesday],
        multiple_non_contiguous_days.collect::<Vec<_>>()
    );
}

#[test]
fn test_iso_weeks() {
    use crate::types::IsoWeekOfYear;
    use crate::Date;

    #[allow(clippy::zero_prefixed_literal)]
    for ((y, m, d), (iso_year, week_number)) in [
        // 2010 starts on a Thursday, so 2009 has 53 ISO weeks
        ((2009, 12, 30), (2009, 53)),
        ((2009, 12, 31), (2009, 53)),
        ((2010, 01, 01), (2009, 53)),
        ((2010, 01, 02), (2009, 53)),
        ((2010, 01, 03), (2009, 53)),
        ((2010, 01, 04), (2010, 1)),
        ((2010, 01, 05), (2010, 1)),
        // 2030 starts on a Monday
        ((2029, 12, 29), (2029, 52)),
        ((2029, 12, 30), (2029, 52)),
        ((2029, 12, 31), (2030, 1)),
        ((2030, 01, 01), (2030, 1)),
        ((2030, 01, 02), (2030, 1)),
        ((2030, 01, 03), (2030, 1)),
        ((2030, 01, 04), (2030, 1)),
    ] {
        assert_eq!(
            Date::try_new_iso(y, m, d).unwrap().week_of_year(),
            IsoWeekOfYear {
                iso_year,
                week_number
            }
        );
    }
}
