// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! This module contains types and implementations for the Gregorian calendar.
//!
//! ```rust
//! use icu::calendar::{cal::Gregorian, Date};
//!
//! let date_iso = Date::try_new_iso(1970, 1, 2)
//!     .expect("Failed to initialize ISO Date instance.");
//! let date_gregorian = Date::new_from_iso(date_iso, Gregorian);
//!
//! assert_eq!(date_gregorian.era_year().year, 1970);
//! assert_eq!(date_gregorian.month().ordinal, 1);
//! assert_eq!(date_gregorian.day_of_month().0, 2);
//! ```

use crate::cal::iso::{Iso, IsoDateInner};
use crate::calendar_arithmetic::ArithmeticDate;
use crate::error::{year_check, DateError};
use crate::{types, Calendar, Date, DateDuration, DateDurationUnit, RangeError};
use calendrical_calculations::rata_die::RataDie;
use tinystr::tinystr;

/// The [(proleptic) Gregorian Calendar](https://en.wikipedia.org/wiki/Proleptic_Gregorian_calendar)
///
/// The Gregorian calendar is a solar calendar used by most of the world, with twelve months.
///
/// This type can be used with [`Date`] to represent dates in this calendar.
///
/// # Era codes
///
/// This calendar uses two era codes: `bce` (alias `bc`), and `ce` (alias `ad`), corresponding to the BCE and CE eras.
///
/// # Month codes
///
/// This calendar supports 12 solar month codes (`"M01" - "M12"`)
#[derive(Copy, Clone, Debug, Default)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct Gregorian;

#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord)]
/// The inner date type used for representing [`Date`]s of [`Gregorian`]. See [`Date`] and [`Gregorian`] for more details.
pub struct GregorianDateInner(pub(crate) IsoDateInner);

impl crate::cal::scaffold::UnstableSealed for Gregorian {}
impl Calendar for Gregorian {
    type DateInner = GregorianDateInner;
    type Year = types::EraYear;
    fn from_codes(
        &self,
        era: Option<&str>,
        year: i32,
        month_code: types::MonthCode,
        day: u8,
    ) -> Result<Self::DateInner, DateError> {
        let year = match era {
            Some("bce" | "bc") => 1 - year_check(year, 1..)?,
            Some("ad" | "ce") | None => year_check(year, 1..)?,
            Some(_) => return Err(DateError::UnknownEra),
        };

        ArithmeticDate::new_from_codes(self, year, month_code, day)
            .map(IsoDateInner)
            .map(GregorianDateInner)
    }

    fn from_rata_die(&self, rd: RataDie) -> Self::DateInner {
        GregorianDateInner(Iso.from_rata_die(rd))
    }

    fn to_rata_die(&self, date: &Self::DateInner) -> RataDie {
        Iso.to_rata_die(&date.0)
    }

    fn from_iso(&self, iso: IsoDateInner) -> GregorianDateInner {
        GregorianDateInner(iso)
    }

    fn to_iso(&self, date: &Self::DateInner) -> IsoDateInner {
        date.0
    }

    fn months_in_year(&self, date: &Self::DateInner) -> u8 {
        Iso.months_in_year(&date.0)
    }

    fn days_in_year(&self, date: &Self::DateInner) -> u16 {
        Iso.days_in_year(&date.0)
    }

    fn days_in_month(&self, date: &Self::DateInner) -> u8 {
        Iso.days_in_month(&date.0)
    }

    fn offset_date(&self, date: &mut Self::DateInner, offset: DateDuration<Self>) {
        Iso.offset_date(&mut date.0, offset.cast_unit())
    }

    #[allow(clippy::field_reassign_with_default)] // it's more clear this way
    fn until(
        &self,
        date1: &Self::DateInner,
        date2: &Self::DateInner,
        _calendar2: &Self,
        largest_unit: DateDurationUnit,
        smallest_unit: DateDurationUnit,
    ) -> DateDuration<Self> {
        Iso.until(&date1.0, &date2.0, &Iso, largest_unit, smallest_unit)
            .cast_unit()
    }
    /// The calendar-specific year represented by `date`
    fn year_info(&self, date: &Self::DateInner) -> Self::Year {
        let extended_year = self.extended_year(date);
        if extended_year > 0 {
            types::EraYear {
                era: tinystr!(16, "ce"),
                era_index: Some(1),
                year: extended_year,
                ambiguity: match extended_year {
                    ..=999 => types::YearAmbiguity::EraAndCenturyRequired,
                    1000..=1949 => types::YearAmbiguity::CenturyRequired,
                    1950..=2049 => types::YearAmbiguity::Unambiguous,
                    2050.. => types::YearAmbiguity::CenturyRequired,
                },
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
        Iso.extended_year(&date.0)
    }

    fn is_in_leap_year(&self, date: &Self::DateInner) -> bool {
        Iso.is_in_leap_year(&date.0)
    }

    /// The calendar-specific month represented by `date`
    fn month(&self, date: &Self::DateInner) -> types::MonthInfo {
        Iso.month(&date.0)
    }

    /// The calendar-specific day-of-month represented by `date`
    fn day_of_month(&self, date: &Self::DateInner) -> types::DayOfMonth {
        Iso.day_of_month(&date.0)
    }

    /// Information of the day of the year
    fn day_of_year(&self, date: &Self::DateInner) -> types::DayOfYear {
        date.0 .0.day_of_year()
    }

    fn debug_name(&self) -> &'static str {
        "Gregorian"
    }

    fn calendar_algorithm(&self) -> Option<crate::preferences::CalendarAlgorithm> {
        Some(crate::preferences::CalendarAlgorithm::Gregory)
    }
}

impl Date<Gregorian> {
    /// Construct a new Gregorian Date.
    ///
    /// Years are specified as ISO years.
    ///
    /// ```rust
    /// use icu::calendar::Date;
    ///
    /// // Conversion from ISO to Gregorian
    /// let date_gregorian = Date::try_new_gregorian(1970, 1, 2)
    ///     .expect("Failed to initialize Gregorian Date instance.");
    ///
    /// assert_eq!(date_gregorian.era_year().year, 1970);
    /// assert_eq!(date_gregorian.month().ordinal, 1);
    /// assert_eq!(date_gregorian.day_of_month().0, 2);
    /// ```
    pub fn try_new_gregorian(year: i32, month: u8, day: u8) -> Result<Date<Gregorian>, RangeError> {
        Date::try_new_iso(year, month, day).map(|d| Date::new_from_iso(d, Gregorian))
    }
}

#[cfg(test)]
mod test {
    use calendrical_calculations::rata_die::RataDie;

    use super::*;

    #[derive(Debug)]
    struct TestCase {
        rd: RataDie,
        iso_year: i32,
        iso_month: u8,
        iso_day: u8,
        expected_year: i32,
        expected_era: &'static str,
        expected_month: u8,
        expected_day: u8,
    }

    fn check_test_case(case: TestCase) {
        let iso_from_rd = Date::from_rata_die(case.rd, Iso);
        let greg_date_from_rd = Date::from_rata_die(case.rd, Gregorian);
        assert_eq!(greg_date_from_rd.era_year().year, case.expected_year,
            "Failed year check from RD: {case:?}\nISO: {iso_from_rd:?}\nGreg: {greg_date_from_rd:?}");
        assert_eq!(
            greg_date_from_rd.era_year().era,
            case.expected_era,
            "Failed era check from RD: {case:?}\nISO: {iso_from_rd:?}\nGreg: {greg_date_from_rd:?}"
        );
        assert_eq!(greg_date_from_rd.month().ordinal, case.expected_month,
            "Failed month check from RD: {case:?}\nISO: {iso_from_rd:?}\nGreg: {greg_date_from_rd:?}");
        assert_eq!(
            greg_date_from_rd.day_of_month().0,
            case.expected_day,
            "Failed day check from RD: {case:?}\nISO: {iso_from_rd:?}\nGreg: {greg_date_from_rd:?}"
        );

        let iso_date_man: Date<Iso> =
            Date::try_new_iso(case.iso_year, case.iso_month, case.iso_day)
                .expect("Failed to initialize ISO date for {case:?}");
        let greg_date_man: Date<Gregorian> = Date::new_from_iso(iso_date_man, Gregorian);
        assert_eq!(iso_from_rd, iso_date_man,
            "ISO from RD not equal to ISO generated from manually-input ymd\nCase: {case:?}\nRD: {iso_from_rd:?}\nMan: {iso_date_man:?}");
        assert_eq!(greg_date_from_rd, greg_date_man,
            "Greg. date from RD not equal to Greg. generated from manually-input ymd\nCase: {case:?}\nRD: {greg_date_from_rd:?}\nMan: {greg_date_man:?}");
    }

    #[test]
    fn test_gregorian_ce() {
        // Tests that the Gregorian calendar gives the correct expected
        // day, month, and year for positive years (AD/CE/gregory era)

        let cases = [
            TestCase {
                rd: RataDie::new(1),
                iso_year: 1,
                iso_month: 1,
                iso_day: 1,
                expected_year: 1,
                expected_era: "ce",
                expected_month: 1,
                expected_day: 1,
            },
            TestCase {
                rd: RataDie::new(181),
                iso_year: 1,
                iso_month: 6,
                iso_day: 30,
                expected_year: 1,
                expected_era: "ce",
                expected_month: 6,
                expected_day: 30,
            },
            TestCase {
                rd: RataDie::new(1155),
                iso_year: 4,
                iso_month: 2,
                iso_day: 29,
                expected_year: 4,
                expected_era: "ce",
                expected_month: 2,
                expected_day: 29,
            },
            TestCase {
                rd: RataDie::new(1344),
                iso_year: 4,
                iso_month: 9,
                iso_day: 5,
                expected_year: 4,
                expected_era: "ce",
                expected_month: 9,
                expected_day: 5,
            },
            TestCase {
                rd: RataDie::new(36219),
                iso_year: 100,
                iso_month: 3,
                iso_day: 1,
                expected_year: 100,
                expected_era: "ce",
                expected_month: 3,
                expected_day: 1,
            },
        ];

        for case in cases {
            check_test_case(case);
        }
    }

    #[test]
    fn test_gregorian_bce() {
        // Tests that the Gregorian calendar gives the correct expected
        // day, month, and year for negative years (BC/BCE era)

        let cases = [
            TestCase {
                rd: RataDie::new(0),
                iso_year: 0,
                iso_month: 12,
                iso_day: 31,
                expected_year: 1,
                expected_era: "bce",
                expected_month: 12,
                expected_day: 31,
            },
            TestCase {
                rd: RataDie::new(-365), // This is a leap year
                iso_year: 0,
                iso_month: 1,
                iso_day: 1,
                expected_year: 1,
                expected_era: "bce",
                expected_month: 1,
                expected_day: 1,
            },
            TestCase {
                rd: RataDie::new(-366),
                iso_year: -1,
                iso_month: 12,
                iso_day: 31,
                expected_year: 2,
                expected_era: "bce",
                expected_month: 12,
                expected_day: 31,
            },
            TestCase {
                rd: RataDie::new(-1461),
                iso_year: -4,
                iso_month: 12,
                iso_day: 31,
                expected_year: 5,
                expected_era: "bce",
                expected_month: 12,
                expected_day: 31,
            },
            TestCase {
                rd: RataDie::new(-1826),
                iso_year: -4,
                iso_month: 1,
                iso_day: 1,
                expected_year: 5,
                expected_era: "bce",
                expected_month: 1,
                expected_day: 1,
            },
        ];

        for case in cases {
            check_test_case(case);
        }
    }

    #[test]
    fn check_gregorian_directionality() {
        // Tests that for a large range of RDs, if a RD
        // is less than another, the corresponding YMD should also be less
        // than the other, without exception.
        for i in -100..100 {
            for j in -100..100 {
                let iso_i = Date::from_rata_die(RataDie::new(i), Iso);
                let iso_j = Date::from_rata_die(RataDie::new(j), Iso);

                let greg_i = iso_i.to_calendar(Gregorian);
                let greg_j = iso_j.to_calendar(Gregorian);

                assert_eq!(
                    i.cmp(&j),
                    iso_i.cmp(&iso_j),
                    "ISO directionality inconsistent with directionality for i: {i}, j: {j}"
                );
                assert_eq!(
                    i.cmp(&j),
                    greg_i.cmp(&greg_j),
                    "Gregorian directionality inconsistent with directionality for i: {i}, j: {j}"
                );
            }
        }
    }
}
