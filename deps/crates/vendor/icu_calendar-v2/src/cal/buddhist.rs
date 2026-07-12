// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::cal::abstract_gregorian::AbstractGregorian;
use crate::error::UnknownEraError;
use crate::preferences::CalendarAlgorithm;
use crate::{
    cal::abstract_gregorian::{impl_with_abstract_gregorian, GregorianYears},
    calendar_arithmetic::ArithmeticDate,
    types, Date, RangeError,
};
use tinystr::tinystr;

#[derive(Copy, Clone, Debug, Default)]
/// The [Thai Solar Buddhist Calendar](https://en.wikipedia.org/wiki/Thai_solar_calendar)
///
/// The Thai Solar Buddhist Calendar is a variant of the [`Gregorian`](crate::cal::Gregorian) calendar
/// created by the Thai government. It is identical to the Gregorian calendar except that is uses
/// the Buddhist Era (-543 CE) instead of the Common Era.
///
/// This implementation extends proleptically for dates before the calendar's creation
/// in 2484 BE (1941 CE).
///
/// This corresponds to the `"buddhist"` [CLDR calendar](https://unicode.org/reports/tr35/#UnicodeCalendarIdentifier).
///
/// # Era codes
///
/// This calendar uses a single era code `be`, with 1 Buddhist Era being 543 BCE. Dates before this era use negative years.
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct Buddhist;

impl_with_abstract_gregorian!(Buddhist, BuddhistDateInner, BuddhistEra, _x, BuddhistEra);

#[derive(Copy, Clone, Debug, Default, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub(crate) struct BuddhistEra;

impl GregorianYears for BuddhistEra {
    const EXTENDED_YEAR_OFFSET: i32 = -543;

    fn extended_from_era_year(
        &self,
        era: Option<&[u8]>,
        year: i32,
    ) -> Result<i32, UnknownEraError> {
        match era {
            Some(b"be") | None => Ok(year),
            _ => Err(UnknownEraError),
        }
    }

    fn era_year_from_extended(&self, extended_year: i32, _month: u8, _day: u8) -> types::EraYear {
        types::EraYear {
            era: tinystr!(16, "be"),
            era_index: Some(0),
            year: extended_year,
            extended_year,
            ambiguity: types::YearAmbiguity::CenturyRequired,
        }
    }

    fn debug_name(&self) -> &'static str {
        "Buddhist"
    }

    fn calendar_algorithm(&self) -> Option<CalendarAlgorithm> {
        Some(CalendarAlgorithm::Buddhist)
    }
}

impl Date<Buddhist> {
    /// Construct a new Buddhist [`Date`].
    ///
    /// Years are arithmetic, meaning there is a year 0 preceded by negative years, with a
    /// valid range of `-9999..=9999`.
    ///
    /// ```rust
    /// use icu::calendar::Date;
    ///
    /// let date_buddhist = Date::try_new_buddhist(1970, 1, 2)
    ///     .expect("Failed to initialize Buddhist Date instance.");
    ///
    /// assert_eq!(date_buddhist.era_year().year, 1970);
    /// assert_eq!(date_buddhist.month().ordinal, 1);
    /// assert_eq!(date_buddhist.day_of_month().0, 2);
    /// ```
    pub fn try_new_buddhist(year: i32, month: u8, day: u8) -> Result<Date<Buddhist>, RangeError> {
        ArithmeticDate::from_year_month_day(year, month, day, &AbstractGregorian(BuddhistEra))
            .map(ArithmeticDate::cast)
            .map(BuddhistDateInner)
            .map(|i| Date::from_raw(i, Buddhist))
    }
}

#[cfg(test)]
mod test {
    use calendrical_calculations::rata_die::RataDie;

    use super::*;

    #[test]
    fn test_buddhist_roundtrip_near_rd_zero() {
        for i in -10000..=10000 {
            let rd = RataDie::new(i);
            let buddhist = Date::from_rata_die(rd, Buddhist);
            let result = buddhist.to_rata_die();
            assert_eq!(rd, result);
        }
    }

    #[test]
    fn test_buddhist_roundtrip_near_epoch() {
        // Buddhist epoch start RD: -198326
        for i in -208326..=-188326 {
            let rd = RataDie::new(i);
            let buddhist = Date::from_rata_die(rd, Buddhist);
            let result = buddhist.to_rata_die();
            assert_eq!(rd, result);
        }
    }

    #[test]
    fn test_buddhist_directionality_near_rd_zero() {
        for i in -100..=100 {
            for j in -100..=100 {
                let buddhist_i = Date::from_rata_die(RataDie::new(i), Buddhist);
                let buddhist_j = Date::from_rata_die(RataDie::new(j), Buddhist);

                assert_eq!(
                    i.cmp(&j),
                    buddhist_i.cmp(&buddhist_j),
                    "Buddhist directionality inconsistent with directionality for i: {i}, j: {j}"
                );
            }
        }
    }

    #[test]
    fn test_buddhist_directionality_near_epoch() {
        // Buddhist epoch start RD: -198326
        for i in -198426..=-198226 {
            for j in -198426..=-198226 {
                let buddhist_i = Date::from_rata_die(RataDie::new(i), Buddhist);
                let buddhist_j = Date::from_rata_die(RataDie::new(j), Buddhist);

                assert_eq!(
                    i.cmp(&j),
                    buddhist_i.cmp(&buddhist_j),
                    "Buddhist directionality inconsistent with directionality for i: {i}, j: {j}"
                );
            }
        }
    }

    #[derive(Debug)]
    struct TestCase {
        rd: RataDie,
        year: i32,
        month: u8,
        day: u8,
    }

    fn check_test_case(case: TestCase) {
        let date = Date::from_rata_die(case.rd, Buddhist);

        assert_eq!(date.to_rata_die(), case.rd, "{case:?}");

        assert_eq!(date.era_year().year, case.year, "{case:?}");
        assert_eq!(date.month().ordinal, case.month, "{case:?}");
        assert_eq!(date.day_of_month().0, case.day, "{case:?}");

        assert_eq!(
            Date::try_new_buddhist(
                date.era_year().extended_year,
                date.month().ordinal,
                date.day_of_month().0
            ),
            Ok(date),
            "{case:?}"
        );
    }

    #[test]
    fn test_buddhist_cases_near_rd_zero() {
        let cases = [
            TestCase {
                rd: Date::try_new_iso(-100, 2, 15).unwrap().to_rata_die(),
                year: 443,
                month: 2,
                day: 15,
            },
            TestCase {
                rd: Date::try_new_iso(-3, 10, 29).unwrap().to_rata_die(),
                year: 540,
                month: 10,
                day: 29,
            },
            TestCase {
                rd: Date::try_new_iso(0, 12, 31).unwrap().to_rata_die(),
                year: 543,
                month: 12,
                day: 31,
            },
            TestCase {
                rd: Date::try_new_iso(1, 1, 1).unwrap().to_rata_die(),
                year: 544,
                month: 1,
                day: 1,
            },
            TestCase {
                rd: Date::try_new_iso(4, 2, 29).unwrap().to_rata_die(),
                year: 547,
                month: 2,
                day: 29,
            },
        ];

        for case in cases {
            check_test_case(case);
        }
    }

    #[test]
    fn test_buddhist_cases_near_epoch() {
        // 1 BE = 543 BCE = -542 ISO
        let cases = [
            TestCase {
                rd: Date::try_new_iso(-554, 12, 31).unwrap().to_rata_die(),
                year: -11,
                month: 12,
                day: 31,
            },
            TestCase {
                rd: Date::try_new_iso(-553, 1, 1).unwrap().to_rata_die(),
                year: -10,
                month: 1,
                day: 1,
            },
            TestCase {
                rd: Date::try_new_iso(-544, 8, 31).unwrap().to_rata_die(),
                year: -1,
                month: 8,
                day: 31,
            },
            TestCase {
                rd: Date::try_new_iso(-543, 5, 12).unwrap().to_rata_die(),
                year: 0,
                month: 5,
                day: 12,
            },
            TestCase {
                rd: Date::try_new_iso(-543, 12, 31).unwrap().to_rata_die(),
                year: 0,
                month: 12,
                day: 31,
            },
            TestCase {
                rd: Date::try_new_iso(-542, 1, 1).unwrap().to_rata_die(),
                year: 1,
                month: 1,
                day: 1,
            },
            TestCase {
                rd: Date::try_new_iso(-541, 7, 9).unwrap().to_rata_die(),
                year: 2,
                month: 7,
                day: 9,
            },
        ];

        for case in cases {
            check_test_case(case);
        }
    }
}
