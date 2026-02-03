// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! This module contains types and implementations for the Julian calendar.
//!
//! ```rust
//! use icu::calendar::{cal::Julian, Date};
//!
//! let date_iso = Date::try_new_iso(1970, 1, 2)
//!     .expect("Failed to initialize ISO Date instance.");
//! let date_julian = Date::new_from_iso(date_iso, Julian);
//!
//! assert_eq!(date_julian.era_year().year, 1969);
//! assert_eq!(date_julian.month().ordinal, 12);
//! assert_eq!(date_julian.day_of_month().0, 20);
//! ```

use crate::cal::iso::{Iso, IsoDateInner};
use crate::calendar_arithmetic::{ArithmeticDate, CalendarArithmetic};
use crate::error::{year_check, DateError};
use crate::{types, Calendar, Date, DateDuration, DateDurationUnit, RangeError};
use calendrical_calculations::helpers::I32CastError;
use calendrical_calculations::rata_die::RataDie;
use tinystr::tinystr;

/// The [Julian Calendar]
///
/// The [Julian calendar] is a solar calendar that was used commonly historically, with twelve months.
///
/// This type can be used with [`Date`] to represent dates in this calendar.
///
/// [Julian calendar]: https://en.wikipedia.org/wiki/Julian_calendar
///
/// # Era codes
///
/// This calendar uses two era codes: `bce` (alias `bc`), and `ce` (alias `ad`), corresponding to the BCE and CE eras.
///
/// # Month codes
///
/// This calendar supports 12 solar month codes (`"M01" - "M12"`)
#[derive(Copy, Clone, Debug, Hash, Default, Eq, PartialEq, PartialOrd, Ord)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct Julian;

/// The inner date type used for representing [`Date`]s of [`Julian`]. See [`Date`] and [`Julian`] for more details.
#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq)]
// The inner date type used for representing Date<Julian>
pub struct JulianDateInner(pub(crate) ArithmeticDate<Julian>);

impl CalendarArithmetic for Julian {
    type YearInfo = i32;

    fn days_in_provided_month(year: i32, month: u8) -> u8 {
        match month {
            4 | 6 | 9 | 11 => 30,
            2 if Self::provided_year_is_leap(year) => 29,
            2 => 28,
            1 | 3 | 5 | 7 | 8 | 10 | 12 => 31,
            _ => 0,
        }
    }

    fn months_in_provided_year(_: i32) -> u8 {
        12
    }

    fn provided_year_is_leap(year: i32) -> bool {
        calendrical_calculations::julian::is_leap_year(year)
    }

    fn last_month_day_in_provided_year(_year: i32) -> (u8, u8) {
        (12, 31)
    }

    fn days_in_provided_year(year: i32) -> u16 {
        if Self::provided_year_is_leap(year) {
            366
        } else {
            365
        }
    }
}

impl crate::cal::scaffold::UnstableSealed for Julian {}
impl Calendar for Julian {
    type DateInner = JulianDateInner;
    type Year = types::EraYear;

    fn from_codes(
        &self,
        era: Option<&str>,
        year: i32,
        month_code: types::MonthCode,
        day: u8,
    ) -> Result<Self::DateInner, DateError> {
        let year = match era {
            Some("ce" | "ad") | None => year_check(year, 1..)?,
            Some("bce" | "bc") => 1 - year_check(year, 1..)?,
            Some(_) => return Err(DateError::UnknownEra),
        };

        ArithmeticDate::new_from_codes(self, year, month_code, day).map(JulianDateInner)
    }

    fn from_rata_die(&self, rd: RataDie) -> Self::DateInner {
        JulianDateInner(
            match calendrical_calculations::julian::julian_from_fixed(rd) {
                Err(I32CastError::BelowMin) => ArithmeticDate::min_date(),
                Err(I32CastError::AboveMax) => ArithmeticDate::max_date(),
                Ok((year, month, day)) => ArithmeticDate::new_unchecked(year, month, day),
            },
        )
    }

    fn to_rata_die(&self, date: &Self::DateInner) -> RataDie {
        calendrical_calculations::julian::fixed_from_julian(date.0.year, date.0.month, date.0.day)
    }

    fn from_iso(&self, iso: IsoDateInner) -> JulianDateInner {
        self.from_rata_die(Iso.to_rata_die(&iso))
    }

    fn to_iso(&self, date: &Self::DateInner) -> IsoDateInner {
        Iso.from_rata_die(self.to_rata_die(date))
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

    /// The calendar-specific year represented by `date`
    /// Julian has the same era scheme as Gregorian
    fn year_info(&self, date: &Self::DateInner) -> Self::Year {
        let extended_year = self.extended_year(date);
        if extended_year > 0 {
            types::EraYear {
                era: tinystr!(16, "ce"),
                era_index: Some(1),
                year: extended_year,
                ambiguity: types::YearAmbiguity::CenturyRequired,
            }
        } else {
            types::EraYear {
                era: tinystr!(16, "bce"),
                era_index: Some(0),
                year: 1_i32.saturating_sub(extended_year),
                ambiguity: types::YearAmbiguity::EraAndCenturyRequired,
            }
        }
    }

    fn extended_year(&self, date: &Self::DateInner) -> i32 {
        date.0.extended_year()
    }

    fn is_in_leap_year(&self, date: &Self::DateInner) -> bool {
        Self::provided_year_is_leap(date.0.year)
    }

    /// The calendar-specific month represented by `date`
    fn month(&self, date: &Self::DateInner) -> types::MonthInfo {
        date.0.month()
    }

    /// The calendar-specific day-of-month represented by `date`
    fn day_of_month(&self, date: &Self::DateInner) -> types::DayOfMonth {
        date.0.day_of_month()
    }

    fn day_of_year(&self, date: &Self::DateInner) -> types::DayOfYear {
        date.0.day_of_year()
    }

    fn debug_name(&self) -> &'static str {
        "Julian"
    }

    fn calendar_algorithm(&self) -> Option<crate::preferences::CalendarAlgorithm> {
        None
    }
}

impl Julian {
    /// Construct a new Julian Calendar
    pub fn new() -> Self {
        Self
    }
}

impl Date<Julian> {
    /// Construct new Julian Date.
    ///
    /// Years are arithmetic, meaning there is a year 0. Zero and negative years are in BC, with year 0 = 1 BC
    ///
    /// ```rust
    /// use icu::calendar::Date;
    ///
    /// let date_julian = Date::try_new_julian(1969, 12, 20)
    ///     .expect("Failed to initialize Julian Date instance.");
    ///
    /// assert_eq!(date_julian.era_year().year, 1969);
    /// assert_eq!(date_julian.month().ordinal, 12);
    /// assert_eq!(date_julian.day_of_month().0, 20);
    /// ```
    pub fn try_new_julian(year: i32, month: u8, day: u8) -> Result<Date<Julian>, RangeError> {
        ArithmeticDate::new_from_ordinals(year, month, day)
            .map(JulianDateInner)
            .map(|inner| Date::from_raw(inner, Julian))
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_day_iso_to_julian() {
        // March 1st 200 is same on both calendars
        let iso_date = Date::try_new_iso(200, 3, 1).unwrap();
        let julian_date = Date::new_from_iso(iso_date, Julian).inner;
        assert_eq!(julian_date.0.year, 200);
        assert_eq!(julian_date.0.month, 3);
        assert_eq!(julian_date.0.day, 1);

        // Feb 28th, 200 (iso) = Feb 29th, 200 (julian)
        let iso_date = Date::try_new_iso(200, 2, 28).unwrap();
        let julian_date = Date::new_from_iso(iso_date, Julian).inner;
        assert_eq!(julian_date.0.year, 200);
        assert_eq!(julian_date.0.month, 2);
        assert_eq!(julian_date.0.day, 29);

        // March 1st 400 (iso) = Feb 29th, 400 (julian)
        let iso_date = Date::try_new_iso(400, 3, 1).unwrap();
        let julian_date = Date::new_from_iso(iso_date, Julian).inner;
        assert_eq!(julian_date.0.year, 400);
        assert_eq!(julian_date.0.month, 2);
        assert_eq!(julian_date.0.day, 29);

        // Jan 1st, 2022 (iso) = Dec 19, 2021 (julian)
        let iso_date = Date::try_new_iso(2022, 1, 1).unwrap();
        let julian_date = Date::new_from_iso(iso_date, Julian).inner;
        assert_eq!(julian_date.0.year, 2021);
        assert_eq!(julian_date.0.month, 12);
        assert_eq!(julian_date.0.day, 19);
    }

    #[test]
    fn test_day_julian_to_iso() {
        // March 1st 200 is same on both calendars
        let julian_date = Date::try_new_julian(200, 3, 1).unwrap();
        let iso_date = julian_date.to_iso();
        let iso_expected_date = Date::try_new_iso(200, 3, 1).unwrap();
        assert_eq!(iso_date, iso_expected_date);

        // Feb 28th, 200 (iso) = Feb 29th, 200 (julian)
        let julian_date = Date::try_new_julian(200, 2, 29).unwrap();
        let iso_date = julian_date.to_iso();
        let iso_expected_date = Date::try_new_iso(200, 2, 28).unwrap();
        assert_eq!(iso_date, iso_expected_date);

        // March 1st 400 (iso) = Feb 29th, 400 (julian)
        let julian_date = Date::try_new_julian(400, 2, 29).unwrap();
        let iso_date = julian_date.to_iso();
        let iso_expected_date = Date::try_new_iso(400, 3, 1).unwrap();
        assert_eq!(iso_date, iso_expected_date);

        // Jan 1st, 2022 (iso) = Dec 19, 2021 (julian)
        let julian_date = Date::try_new_julian(2021, 12, 19).unwrap();
        let iso_date = julian_date.to_iso();
        let iso_expected_date = Date::try_new_iso(2022, 1, 1).unwrap();
        assert_eq!(iso_date, iso_expected_date);

        // March 1st, 2022 (iso) = Feb 16, 2022 (julian)
        let julian_date = Date::try_new_julian(2022, 2, 16).unwrap();
        let iso_date = julian_date.to_iso();
        let iso_expected_date = Date::try_new_iso(2022, 3, 1).unwrap();
        assert_eq!(iso_date, iso_expected_date);
    }

    #[test]
    fn test_roundtrip_negative() {
        // https://github.com/unicode-org/icu4x/issues/2254
        let iso_date = Date::try_new_iso(-1000, 3, 3).unwrap();
        let julian = iso_date.to_calendar(Julian::new());
        let recovered_iso = julian.to_iso();
        assert_eq!(iso_date, recovered_iso);
    }

    #[test]
    fn test_julian_near_era_change() {
        // Tests that the Julian calendar gives the correct expected
        // day, month, and year for positive years (CE)

        #[derive(Debug)]
        struct TestCase {
            rd: i64,
            iso_year: i32,
            iso_month: u8,
            iso_day: u8,
            expected_year: i32,
            expected_era: &'static str,
            expected_month: u8,
            expected_day: u8,
        }

        let cases = [
            TestCase {
                rd: 1,
                iso_year: 1,
                iso_month: 1,
                iso_day: 1,
                expected_year: 1,
                expected_era: "ce",
                expected_month: 1,
                expected_day: 3,
            },
            TestCase {
                rd: 0,
                iso_year: 0,
                iso_month: 12,
                iso_day: 31,
                expected_year: 1,
                expected_era: "ce",
                expected_month: 1,
                expected_day: 2,
            },
            TestCase {
                rd: -1,
                iso_year: 0,
                iso_month: 12,
                iso_day: 30,
                expected_year: 1,
                expected_era: "ce",
                expected_month: 1,
                expected_day: 1,
            },
            TestCase {
                rd: -2,
                iso_year: 0,
                iso_month: 12,
                iso_day: 29,
                expected_year: 1,
                expected_era: "bce",
                expected_month: 12,
                expected_day: 31,
            },
            TestCase {
                rd: -3,
                iso_year: 0,
                iso_month: 12,
                iso_day: 28,
                expected_year: 1,
                expected_era: "bce",
                expected_month: 12,
                expected_day: 30,
            },
            TestCase {
                rd: -367,
                iso_year: -1,
                iso_month: 12,
                iso_day: 30,
                expected_year: 1,
                expected_era: "bce",
                expected_month: 1,
                expected_day: 1,
            },
            TestCase {
                rd: -368,
                iso_year: -1,
                iso_month: 12,
                iso_day: 29,
                expected_year: 2,
                expected_era: "bce",
                expected_month: 12,
                expected_day: 31,
            },
            TestCase {
                rd: -1462,
                iso_year: -4,
                iso_month: 12,
                iso_day: 30,
                expected_year: 4,
                expected_era: "bce",
                expected_month: 1,
                expected_day: 1,
            },
            TestCase {
                rd: -1463,
                iso_year: -4,
                iso_month: 12,
                iso_day: 29,
                expected_year: 5,
                expected_era: "bce",
                expected_month: 12,
                expected_day: 31,
            },
        ];

        for case in cases {
            let iso_from_rd = Date::from_rata_die(RataDie::new(case.rd), crate::Iso);
            let julian_from_rd = Date::from_rata_die(RataDie::new(case.rd), Julian);
            assert_eq!(julian_from_rd.era_year().year, case.expected_year,
                "Failed year check from RD: {case:?}\nISO: {iso_from_rd:?}\nJulian: {julian_from_rd:?}");
            assert_eq!(julian_from_rd.era_year().era, case.expected_era,
                "Failed era check from RD: {case:?}\nISO: {iso_from_rd:?}\nJulian: {julian_from_rd:?}");
            assert_eq!(julian_from_rd.month().ordinal, case.expected_month,
                "Failed month check from RD: {case:?}\nISO: {iso_from_rd:?}\nJulian: {julian_from_rd:?}");
            assert_eq!(julian_from_rd.day_of_month().0, case.expected_day,
                "Failed day check from RD: {case:?}\nISO: {iso_from_rd:?}\nJulian: {julian_from_rd:?}");

            let iso_date_man = Date::try_new_iso(case.iso_year, case.iso_month, case.iso_day)
                .expect("Failed to initialize ISO date for {case:?}");
            let julian_date_man = Date::new_from_iso(iso_date_man, Julian);
            assert_eq!(iso_from_rd, iso_date_man,
                "ISO from RD not equal to ISO generated from manually-input ymd\nCase: {case:?}\nRD: {iso_from_rd:?}\nMan: {iso_date_man:?}");
            assert_eq!(julian_from_rd, julian_date_man,
                "Julian from RD not equal to Julian generated from manually-input ymd\nCase: {case:?}\nRD: {julian_from_rd:?}\nMan: {julian_date_man:?}");
        }
    }

    #[test]
    fn test_julian_rd_date_conversion() {
        // Tests that converting from RD to Julian then
        // back to RD yields the same RD
        for i in -10000..=10000 {
            let rd = RataDie::new(i);
            let julian = Date::from_rata_die(rd, Julian);
            let new_rd = julian.to_rata_die();
            assert_eq!(rd, new_rd);
        }
    }

    #[test]
    fn test_julian_directionality() {
        // Tests that for a large range of RDs, if a RD
        // is less than another, the corresponding YMD should also be less
        // than the other, without exception.
        for i in -100..=100 {
            for j in -100..=100 {
                let julian_i = Date::from_rata_die(RataDie::new(i), Julian);
                let julian_j = Date::from_rata_die(RataDie::new(j), Julian);

                assert_eq!(
                    i.cmp(&j),
                    julian_i.inner.0.cmp(&julian_j.inner.0),
                    "Julian directionality inconsistent with directionality for i: {i}, j: {j}"
                );
            }
        }
    }

    #[test]
    fn test_hebrew_epoch() {
        assert_eq!(
            calendrical_calculations::julian::fixed_from_julian_book_version(-3761, 10, 7),
            RataDie::new(-1373427)
        );
    }

    #[test]
    fn test_julian_leap_years() {
        assert!(Julian::provided_year_is_leap(4));
        assert!(Julian::provided_year_is_leap(0));
        assert!(Julian::provided_year_is_leap(-4));

        Date::try_new_julian(2020, 2, 29).unwrap();
    }
}
