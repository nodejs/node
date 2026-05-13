// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::cal::abstract_gregorian::{impl_with_abstract_gregorian, GregorianYears};
use crate::calendar_arithmetic::ArithmeticDate;
use crate::error::UnknownEraError;
use crate::preferences::CalendarAlgorithm;
use crate::{types, Date, DateError, RangeError};
use tinystr::tinystr;

/// The [Republic of China Calendar](https://en.wikipedia.org/wiki/Republic_of_China_calendar)
///
/// The ROC Calendar is a variant of the [`Gregorian`](crate::cal::Gregorian) calendar
/// created by the government of the Republic of China. It is identical to the Gregorian
/// calendar except that is uses the ROC/Minguo/民国/民國 Era (1912 CE) instead of the Common Era.
///
/// This implementation extends proleptically for dates before the calendar's creation
/// in 1 Minguo (1912 CE).
///
/// The ROC calendar should not be confused with the [`ChineseTraditional`](crate::cal::ChineseTraditional)
/// lunisolar calendar.
///
/// This corresponds to the `"roc"` [CLDR calendar](https://unicode.org/reports/tr35/#UnicodeCalendarIdentifier).
///
/// # Era codes
///
/// This calendar uses two era codes: `roc`, corresponding to years in the 民國 era (CE year 1912 and
/// after), and `broc`, corresponding to years before the 民國 era (CE year 1911 and before).
#[derive(Copy, Clone, Debug, Default)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct Roc;

impl_with_abstract_gregorian!(crate::cal::Roc, RocDateInner, RocEra, _x, RocEra);

#[derive(Copy, Clone, Debug, Default, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub(crate) struct RocEra;

impl GregorianYears for RocEra {
    const EXTENDED_YEAR_OFFSET: i32 = 1911;

    fn extended_from_era_year(
        &self,
        era: Option<&[u8]>,
        year: i32,
    ) -> Result<i32, UnknownEraError> {
        match era {
            None => Ok(year),
            Some(b"roc") => Ok(year),
            Some(b"broc") => Ok(1 - year),
            Some(_) => Err(UnknownEraError),
        }
    }

    fn era_year_from_extended(&self, extended_year: i32, _month: u8, _day: u8) -> types::EraYear {
        if extended_year > 0 {
            types::EraYear {
                era: tinystr!(16, "roc"),
                era_index: Some(1),
                year: extended_year,
                extended_year,
                ambiguity: types::YearAmbiguity::CenturyRequired,
            }
        } else {
            types::EraYear {
                era: tinystr!(16, "broc"),
                era_index: Some(0),
                year: 1 - extended_year,
                extended_year,
                ambiguity: types::YearAmbiguity::EraAndCenturyRequired,
            }
        }
    }

    fn debug_name(&self) -> &'static str {
        "ROC"
    }

    fn calendar_algorithm(&self) -> Option<CalendarAlgorithm> {
        Some(CalendarAlgorithm::Roc)
    }
}

impl Date<Roc> {
    /// Construct a new Republic of China calendar Date.
    ///
    /// Years are specified in the "roc" era. This function accepts an extended year in that era, so dates
    /// before Minguo are negative and year 0 is 1 Before Minguo. To specify dates using explicit era
    /// codes, use [`Date::try_new_from_codes()`].
    ///
    /// ```rust
    /// use icu::calendar::Date;
    /// use icu::calendar::cal::Gregorian;
    /// use tinystr::tinystr;
    ///
    /// // Create a new ROC Date
    /// let date_roc = Date::try_new_roc(1, 2, 3)
    ///     .expect("Failed to initialize ROC Date instance.");
    ///
    /// assert_eq!(date_roc.era_year().era, "roc");
    /// assert_eq!(date_roc.era_year().year, 1, "ROC year check failed!");
    /// assert_eq!(date_roc.month().ordinal, 2, "ROC month check failed!");
    /// assert_eq!(date_roc.day_of_month().0, 3, "ROC day of month check failed!");
    ///
    /// // Convert to an equivalent Gregorian date
    /// let date_gregorian = date_roc.to_calendar(Gregorian);
    ///
    /// assert_eq!(date_gregorian.era_year().year, 1912, "Gregorian from ROC year check failed!");
    /// assert_eq!(date_gregorian.month().ordinal, 2, "Gregorian from ROC month check failed!");
    /// assert_eq!(date_gregorian.day_of_month().0, 3, "Gregorian from ROC day of month check failed!");
    pub fn try_new_roc(year: i32, month: u8, day: u8) -> Result<Date<Roc>, RangeError> {
        ArithmeticDate::new_gregorian::<RocEra>(year, month, day)
            .map(RocDateInner)
            .map(|i| Date::from_raw(i, Roc))
    }
}

#[cfg(test)]
mod test {

    use super::*;
    use crate::cal::Iso;
    use calendrical_calculations::rata_die::RataDie;

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
        let roc_from_rd = Date::from_rata_die(case.rd, Roc);
        assert_eq!(
            roc_from_rd.era_year().year,
            case.expected_year,
            "Failed year check from RD: {case:?}\nISO: {iso_from_rd:?}\nROC: {roc_from_rd:?}"
        );
        assert_eq!(
            roc_from_rd.era_year().era,
            case.expected_era,
            "Failed era check from RD: {case:?}\nISO: {iso_from_rd:?}\nROC: {roc_from_rd:?}"
        );
        assert_eq!(
            roc_from_rd.extended_year(),
            if case.expected_era == "roc" {
                case.expected_year
            } else {
                1 - case.expected_year
            },
            "Failed year check from RD: {case:?}\nISO: {iso_from_rd:?}\nROC: {roc_from_rd:?}"
        );
        assert_eq!(
            roc_from_rd.month().ordinal,
            case.expected_month,
            "Failed month check from RD: {case:?}\nISO: {iso_from_rd:?}\nROC: {roc_from_rd:?}"
        );
        assert_eq!(roc_from_rd.day_of_month().0, case.expected_day,
            "Failed day_of_month check from RD: {case:?}\nISO: {iso_from_rd:?}\nROC: {roc_from_rd:?}");

        let iso_from_case = Date::try_new_iso(case.iso_year, case.iso_month, case.iso_day)
            .expect("Failed to initialize ISO date for {case:?}");
        let roc_from_case = Date::new_from_iso(iso_from_case, Roc);
        assert_eq!(iso_from_rd, iso_from_case,
            "ISO from RD not equal to ISO generated from manually-input ymd\nCase: {case:?}\nRD: {iso_from_rd:?}\nManual: {iso_from_case:?}");
        assert_eq!(roc_from_rd, roc_from_case,
            "ROC date from RD not equal to ROC generated from manually-input ymd\nCase: {case:?}\nRD: {roc_from_rd:?}\nManual: {roc_from_case:?}");
    }

    #[test]
    fn test_roc_current_era() {
        // Tests that the ROC calendar gives the correct expected day, month, and year for years >= 1912
        // (years in the ROC/minguo era)
        //
        // Jan 1. 1912 CE = RD 697978

        let cases = [
            TestCase {
                rd: RataDie::new(697978),
                iso_year: 1912,
                iso_month: 1,
                iso_day: 1,
                expected_year: 1,
                expected_era: "roc",
                expected_month: 1,
                expected_day: 1,
            },
            TestCase {
                rd: RataDie::new(698037),
                iso_year: 1912,
                iso_month: 2,
                iso_day: 29,
                expected_year: 1,
                expected_era: "roc",
                expected_month: 2,
                expected_day: 29,
            },
            TestCase {
                rd: RataDie::new(698524),
                iso_year: 1913,
                iso_month: 6,
                iso_day: 30,
                expected_year: 2,
                expected_era: "roc",
                expected_month: 6,
                expected_day: 30,
            },
            TestCase {
                rd: RataDie::new(738714),
                iso_year: 2023,
                iso_month: 7,
                iso_day: 13,
                expected_year: 112,
                expected_era: "roc",
                expected_month: 7,
                expected_day: 13,
            },
        ];

        for case in cases {
            check_test_case(case);
        }
    }

    #[test]
    fn test_roc_prior_era() {
        // Tests that the ROC calendar gives the correct expected day, month, and year for years <= 1911
        // (years in the ROC/minguo era)
        //
        // Jan 1. 1912 CE = RD 697978
        let cases = [
            TestCase {
                rd: RataDie::new(697977),
                iso_year: 1911,
                iso_month: 12,
                iso_day: 31,
                expected_year: 1,
                expected_era: "broc",
                expected_month: 12,
                expected_day: 31,
            },
            TestCase {
                rd: RataDie::new(697613),
                iso_year: 1911,
                iso_month: 1,
                iso_day: 1,
                expected_year: 1,
                expected_era: "broc",
                expected_month: 1,
                expected_day: 1,
            },
            TestCase {
                rd: RataDie::new(697612),
                iso_year: 1910,
                iso_month: 12,
                iso_day: 31,
                expected_year: 2,
                expected_era: "broc",
                expected_month: 12,
                expected_day: 31,
            },
            TestCase {
                rd: RataDie::new(696576),
                iso_year: 1908,
                iso_month: 2,
                iso_day: 29,
                expected_year: 4,
                expected_era: "broc",
                expected_month: 2,
                expected_day: 29,
            },
            TestCase {
                rd: RataDie::new(1),
                iso_year: 1,
                iso_month: 1,
                iso_day: 1,
                expected_year: 1911,
                expected_era: "broc",
                expected_month: 1,
                expected_day: 1,
            },
            TestCase {
                rd: RataDie::new(0),
                iso_year: 0,
                iso_month: 12,
                iso_day: 31,
                expected_year: 1912,
                expected_era: "broc",
                expected_month: 12,
                expected_day: 31,
            },
        ];

        for case in cases {
            check_test_case(case);
        }
    }

    #[test]
    fn test_roc_directionality_near_epoch() {
        // Tests that for a large range of RDs near the beginning of the minguo era (CE 1912),
        // the comparison between those two RDs should be equal to the comparison between their
        // corresponding YMD.
        let rd_epoch_start = 697978;
        for i in (rd_epoch_start - 100)..=(rd_epoch_start + 100) {
            for j in (rd_epoch_start - 100)..=(rd_epoch_start + 100) {
                let iso_i = Date::from_rata_die(RataDie::new(i), Iso);
                let iso_j = Date::from_rata_die(RataDie::new(j), Iso);

                let roc_i = Date::from_rata_die(RataDie::new(i), Roc);
                let roc_j = Date::from_rata_die(RataDie::new(j), Roc);

                assert_eq!(
                    i.cmp(&j),
                    iso_i.cmp(&iso_j),
                    "ISO directionality inconsistent with directionality for i: {i}, j: {j}"
                );
                assert_eq!(
                    i.cmp(&j),
                    roc_i.cmp(&roc_j),
                    "ROC directionality inconsistent with directionality for i: {i}, j: {j}"
                );
            }
        }
    }

    #[test]
    fn test_roc_directionality_near_rd_zero() {
        // Same as `test_directionality_near_epoch`, but with a focus around RD 0
        for i in -100..=100 {
            for j in -100..100 {
                let iso_i = Date::from_rata_die(RataDie::new(i), Iso);
                let iso_j = Date::from_rata_die(RataDie::new(j), Iso);

                let roc_i = Date::from_rata_die(RataDie::new(i), Roc);
                let roc_j = Date::from_rata_die(RataDie::new(j), Roc);

                assert_eq!(
                    i.cmp(&j),
                    iso_i.cmp(&iso_j),
                    "ISO directionality inconsistent with directionality for i: {i}, j: {j}"
                );
                assert_eq!(
                    i.cmp(&j),
                    roc_i.cmp(&roc_j),
                    "ROC directionality inconsistent with directionality for i: {i}, j: {j}"
                );
            }
        }
    }
}
