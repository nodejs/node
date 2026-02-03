// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! This module contains types and implementations for the Indian national calendar.
//!
//! ```rust
//! use icu::calendar::{cal::Indian, Date};
//!
//! let date_iso = Date::try_new_iso(1970, 1, 2)
//!     .expect("Failed to initialize ISO Date instance.");
//! let date_indian = Date::new_from_iso(date_iso, Indian);
//!
//! assert_eq!(date_indian.era_year().year, 1891);
//! assert_eq!(date_indian.month().ordinal, 10);
//! assert_eq!(date_indian.day_of_month().0, 12);
//! ```

use crate::cal::iso::{Iso, IsoDateInner};
use crate::calendar_arithmetic::{ArithmeticDate, CalendarArithmetic};
use crate::error::DateError;
use crate::{types, Calendar, Date, DateDuration, DateDurationUnit, RangeError};
use calendrical_calculations::rata_die::RataDie;
use tinystr::tinystr;

/// The [Indian National (Śaka) Calendar](https://en.wikipedia.org/wiki/Indian_national_calendar)
///
/// The Indian National calendar is a solar calendar used by the Indian government, with twelve months.
///
/// This type can be used with [`Date`] to represent dates in this calendar.
///
/// # Era codes
///
/// This calendar uses a single era code: `shaka`, with Śaka 0 being 78 CE. Dates before this era use negative years.
///
/// # Month codes
///
/// This calendar supports 12 solar month codes (`"M01" - "M12"`)
#[derive(Copy, Clone, Debug, Hash, Default, Eq, PartialEq, PartialOrd, Ord)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct Indian;

/// The inner date type used for representing [`Date`]s of [`Indian`]. See [`Date`] and [`Indian`] for more details.
#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord)]
pub struct IndianDateInner(ArithmeticDate<Indian>);

impl CalendarArithmetic for Indian {
    type YearInfo = i32;

    fn days_in_provided_month(year: i32, month: u8) -> u8 {
        if month == 1 {
            if Self::provided_year_is_leap(year) {
                31
            } else {
                30
            }
        } else if (2..=6).contains(&month) {
            31
        } else if (7..=12).contains(&month) {
            30
        } else {
            0
        }
    }

    fn months_in_provided_year(_: i32) -> u8 {
        12
    }

    fn provided_year_is_leap(year: i32) -> bool {
        Iso::provided_year_is_leap(year + 78)
    }

    fn last_month_day_in_provided_year(_year: i32) -> (u8, u8) {
        (12, 30)
    }

    fn days_in_provided_year(year: i32) -> u16 {
        if Self::provided_year_is_leap(year) {
            366
        } else {
            365
        }
    }
}

/// The Śaka era starts on the 81st day of the Gregorian year (March 22 or 21)
/// which is an 80 day offset. This number should be subtracted from Gregorian dates
const DAY_OFFSET: u16 = 80;
/// The Śaka era is 78 years behind Gregorian. This number should be added to Gregorian dates
const YEAR_OFFSET: i32 = 78;

impl crate::cal::scaffold::UnstableSealed for Indian {}
impl Calendar for Indian {
    type DateInner = IndianDateInner;
    type Year = types::EraYear;
    fn from_codes(
        &self,
        era: Option<&str>,
        year: i32,
        month_code: types::MonthCode,
        day: u8,
    ) -> Result<Self::DateInner, DateError> {
        let year = match era {
            Some("shaka") | None => year,
            Some(_) => return Err(DateError::UnknownEra),
        };
        ArithmeticDate::new_from_codes(self, year, month_code, day).map(IndianDateInner)
    }

    fn from_rata_die(&self, rd: RataDie) -> Self::DateInner {
        self.from_iso(Iso.from_rata_die(rd))
    }

    fn to_rata_die(&self, date: &Self::DateInner) -> RataDie {
        Iso.to_rata_die(&self.to_iso(date))
    }

    // Algorithms directly implemented in icu_calendar since they're not from the book
    fn from_iso(&self, iso: IsoDateInner) -> IndianDateInner {
        // Get day number in year (1 indexed)
        let day_of_year_iso = Iso::day_of_year(iso);
        // Convert to Śaka year
        let mut year = iso.0.year - YEAR_OFFSET;
        // This is in the previous Indian year
        let day_of_year_indian = if day_of_year_iso <= DAY_OFFSET {
            year -= 1;
            let n_days = Self::days_in_provided_year(year);

            // calculate day of year in previous year
            n_days + day_of_year_iso - DAY_OFFSET
        } else {
            day_of_year_iso - DAY_OFFSET
        };
        IndianDateInner(ArithmeticDate::date_from_year_day(
            year,
            day_of_year_indian as u32,
        ))
    }

    // Algorithms directly implemented in icu_calendar since they're not from the book
    fn to_iso(&self, date: &Self::DateInner) -> IsoDateInner {
        let day_of_year_indian = date.0.day_of_year().0; // 1-indexed
        let days_in_year = date.0.days_in_year();

        let mut year = date.0.year + YEAR_OFFSET;
        // days_in_year is a valid day of the year, so we check > not >=
        let day_of_year_iso = if day_of_year_indian + DAY_OFFSET > days_in_year {
            year += 1;
            // calculate day of year in next year
            day_of_year_indian + DAY_OFFSET - days_in_year
        } else {
            day_of_year_indian + DAY_OFFSET
        };
        Iso::iso_from_year_day(year, day_of_year_iso)
    }

    fn months_in_year(&self, date: &Self::DateInner) -> u8 {
        date.0.months_in_year()
    }

    fn days_in_year(&self, date: &Self::DateInner) -> u16 {
        date.0.days_in_year()
    }

    fn days_in_month(&self, date: &Self::DateInner) -> u8 {
        date.0.days_in_month()
    }

    fn offset_date(&self, date: &mut Self::DateInner, offset: DateDuration<Self>) {
        date.0.offset_date(offset, &());
    }

    #[allow(clippy::field_reassign_with_default)]
    fn until(
        &self,
        date1: &Self::DateInner,
        date2: &Self::DateInner,
        _calendar2: &Self,
        _largest_unit: DateDurationUnit,
        _smallest_unit: DateDurationUnit,
    ) -> DateDuration<Self> {
        date1.0.until(date2.0, _largest_unit, _smallest_unit)
    }

    fn year_info(&self, date: &Self::DateInner) -> Self::Year {
        types::EraYear {
            era_index: Some(0),
            era: tinystr!(16, "shaka"),
            year: self.extended_year(date),
            ambiguity: types::YearAmbiguity::CenturyRequired,
        }
    }

    fn extended_year(&self, date: &Self::DateInner) -> i32 {
        date.0.extended_year()
    }

    fn is_in_leap_year(&self, date: &Self::DateInner) -> bool {
        Self::provided_year_is_leap(date.0.year)
    }

    fn month(&self, date: &Self::DateInner) -> types::MonthInfo {
        date.0.month()
    }

    fn day_of_month(&self, date: &Self::DateInner) -> types::DayOfMonth {
        date.0.day_of_month()
    }

    fn day_of_year(&self, date: &Self::DateInner) -> types::DayOfYear {
        date.0.day_of_year()
    }

    fn debug_name(&self) -> &'static str {
        "Indian"
    }

    fn calendar_algorithm(&self) -> Option<crate::preferences::CalendarAlgorithm> {
        Some(crate::preferences::CalendarAlgorithm::Indian)
    }
}

impl Indian {
    /// Construct a new Indian Calendar
    pub fn new() -> Self {
        Self
    }
}

impl Date<Indian> {
    /// Construct new Indian Date, with year provided in the Śaka era.
    ///
    /// ```rust
    /// use icu::calendar::Date;
    ///
    /// let date_indian = Date::try_new_indian(1891, 10, 12)
    ///     .expect("Failed to initialize Indian Date instance.");
    ///
    /// assert_eq!(date_indian.era_year().year, 1891);
    /// assert_eq!(date_indian.month().ordinal, 10);
    /// assert_eq!(date_indian.day_of_month().0, 12);
    /// ```
    pub fn try_new_indian(year: i32, month: u8, day: u8) -> Result<Date<Indian>, RangeError> {
        ArithmeticDate::new_from_ordinals(year, month, day)
            .map(IndianDateInner)
            .map(|inner| Date::from_raw(inner, Indian))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use calendrical_calculations::rata_die::RataDie;
    fn assert_roundtrip(y: i32, m: u8, d: u8, iso_y: i32, iso_m: u8, iso_d: u8) {
        let indian =
            Date::try_new_indian(y, m, d).expect("Indian date should construct successfully");
        let iso = indian.to_iso();

        assert_eq!(
            iso.era_year().year,
            iso_y,
            "{y}-{m}-{d}: ISO year did not match"
        );
        assert_eq!(
            iso.month().ordinal,
            iso_m,
            "{y}-{m}-{d}: ISO month did not match"
        );
        assert_eq!(
            iso.day_of_month().0,
            iso_d,
            "{y}-{m}-{d}: ISO day did not match"
        );

        let roundtrip = iso.to_calendar(Indian);

        assert_eq!(
            roundtrip.era_year().year,
            indian.era_year().year,
            "{y}-{m}-{d}: roundtrip year did not match"
        );
        assert_eq!(
            roundtrip.month().ordinal,
            indian.month().ordinal,
            "{y}-{m}-{d}: roundtrip month did not match"
        );
        assert_eq!(
            roundtrip.day_of_month(),
            indian.day_of_month(),
            "{y}-{m}-{d}: roundtrip day did not match"
        );
    }

    #[test]
    fn roundtrip_indian() {
        // Ultimately the day of the year will always be identical regardless of it
        // being a leap year or not
        // Test dates that occur after and before Chaitra 1 (March 22/21), in all years of
        // a four-year leap cycle, to ensure that all code paths are tested
        assert_roundtrip(1944, 6, 7, 2022, 8, 29);
        assert_roundtrip(1943, 6, 7, 2021, 8, 29);
        assert_roundtrip(1942, 6, 7, 2020, 8, 29);
        assert_roundtrip(1941, 6, 7, 2019, 8, 29);
        assert_roundtrip(1944, 11, 7, 2023, 1, 27);
        assert_roundtrip(1943, 11, 7, 2022, 1, 27);
        assert_roundtrip(1942, 11, 7, 2021, 1, 27);
        assert_roundtrip(1941, 11, 7, 2020, 1, 27);
    }

    #[derive(Debug)]
    struct TestCase {
        iso_year: i32,
        iso_month: u8,
        iso_day: u8,
        expected_year: i32,
        expected_month: u8,
        expected_day: u8,
    }

    fn check_case(case: TestCase) {
        let iso = Date::try_new_iso(case.iso_year, case.iso_month, case.iso_day).unwrap();
        let indian = iso.to_calendar(Indian);
        assert_eq!(
            indian.era_year().year,
            case.expected_year,
            "Year check failed for case: {case:?}"
        );
        assert_eq!(
            indian.month().ordinal,
            case.expected_month,
            "Month check failed for case: {case:?}"
        );
        assert_eq!(
            indian.day_of_month().0,
            case.expected_day,
            "Day check failed for case: {case:?}"
        );
    }

    #[test]
    fn test_cases_near_epoch_start() {
        let cases = [
            TestCase {
                iso_year: 79,
                iso_month: 3,
                iso_day: 23,
                expected_year: 1,
                expected_month: 1,
                expected_day: 2,
            },
            TestCase {
                iso_year: 79,
                iso_month: 3,
                iso_day: 22,
                expected_year: 1,
                expected_month: 1,
                expected_day: 1,
            },
            TestCase {
                iso_year: 79,
                iso_month: 3,
                iso_day: 21,
                expected_year: 0,
                expected_month: 12,
                expected_day: 30,
            },
            TestCase {
                iso_year: 79,
                iso_month: 3,
                iso_day: 20,
                expected_year: 0,
                expected_month: 12,
                expected_day: 29,
            },
            TestCase {
                iso_year: 78,
                iso_month: 3,
                iso_day: 21,
                expected_year: -1,
                expected_month: 12,
                expected_day: 30,
            },
        ];

        for case in cases {
            check_case(case);
        }
    }

    #[test]
    fn test_cases_near_rd_zero() {
        let cases = [
            TestCase {
                iso_year: 1,
                iso_month: 3,
                iso_day: 22,
                expected_year: -77,
                expected_month: 1,
                expected_day: 1,
            },
            TestCase {
                iso_year: 1,
                iso_month: 3,
                iso_day: 21,
                expected_year: -78,
                expected_month: 12,
                expected_day: 30,
            },
            TestCase {
                iso_year: 1,
                iso_month: 1,
                iso_day: 1,
                expected_year: -78,
                expected_month: 10,
                expected_day: 11,
            },
            TestCase {
                iso_year: 0,
                iso_month: 3,
                iso_day: 21,
                expected_year: -78,
                expected_month: 1,
                expected_day: 1,
            },
            TestCase {
                iso_year: 0,
                iso_month: 1,
                iso_day: 1,
                expected_year: -79,
                expected_month: 10,
                expected_day: 11,
            },
            TestCase {
                iso_year: -1,
                iso_month: 3,
                iso_day: 21,
                expected_year: -80,
                expected_month: 12,
                expected_day: 30,
            },
        ];

        for case in cases {
            check_case(case);
        }
    }

    #[test]
    fn test_roundtrip_near_rd_zero() {
        for i in -1000..=1000 {
            let initial = RataDie::new(i);
            let result = Date::from_rata_die(initial, Iso)
                .to_calendar(Indian)
                .to_iso()
                .to_rata_die();
            assert_eq!(
                initial, result,
                "Roundtrip failed for initial: {initial:?}, result: {result:?}"
            );
        }
    }

    #[test]
    fn test_roundtrip_near_epoch_start() {
        // Epoch start: RD 28570
        for i in 27570..=29570 {
            let initial = RataDie::new(i);
            let result = Date::from_rata_die(initial, Iso)
                .to_calendar(Indian)
                .to_iso()
                .to_rata_die();
            assert_eq!(
                initial, result,
                "Roundtrip failed for initial: {initial:?}, result: {result:?}"
            );
        }
    }

    #[test]
    fn test_directionality_near_rd_zero() {
        for i in -100..=100 {
            for j in -100..=100 {
                let rd_i = RataDie::new(i);
                let rd_j = RataDie::new(j);

                let indian_i = Date::from_rata_die(rd_i, Indian);
                let indian_j = Date::from_rata_die(rd_j, Indian);

                assert_eq!(i.cmp(&j), indian_i.cmp(&indian_j), "Directionality test failed for i: {i}, j: {j}, indian_i: {indian_i:?}, indian_j: {indian_j:?}");
            }
        }
    }

    #[test]
    fn test_directionality_near_epoch_start() {
        // Epoch start: RD 28570
        for i in 28470..=28670 {
            for j in 28470..=28670 {
                let indian_i = Date::from_rata_die(RataDie::new(i), Indian);
                let indian_j = Date::from_rata_die(RataDie::new(j), Indian);

                assert_eq!(i.cmp(&j), indian_i.cmp(&indian_j), "Directionality test failed for i: {i}, j: {j}, indian_i: {indian_i:?}, indian_j: {indian_j:?}");
            }
        }
    }
}
