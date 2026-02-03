// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! This module contains types and implementations for the Republic of China calendar.
//!
//! ```rust
//! use icu::calendar::{cal::Roc, Date};
//!
//! let date_iso = Date::try_new_iso(1970, 1, 2)
//!     .expect("Failed to initialize ISO Date instance.");
//! let date_roc = Date::new_from_iso(date_iso, Roc);
//!
//! assert_eq!(date_roc.era_year().year, 59);
//! assert_eq!(date_roc.month().ordinal, 1);
//! assert_eq!(date_roc.day_of_month().0, 2);
//! ```

use crate::error::year_check;
use crate::{
    cal::iso::IsoDateInner, calendar_arithmetic::ArithmeticDate, error::DateError, types, Calendar,
    Date, Iso, RangeError,
};
use calendrical_calculations::rata_die::RataDie;
use tinystr::tinystr;

/// Year of the beginning of the Taiwanese (ROC/Minguo) calendar.
/// 1912 ISO = ROC 1
const ROC_ERA_OFFSET: i32 = 1911;

/// The [Republic of China Calendar](https://en.wikipedia.org/wiki/Republic_of_China_calendar)
///
/// The ROC calendar is a solar calendar used in Taiwan and Penghu, as well as by overseas diaspora from
/// those locations. Months and days are identical to the [`Gregorian`](super::Gregorian) calendar, while years are counted
/// with 1912, the year of the establishment of the Republic of China, as year 1 of the ROC/Minguo/民国/民國 era.
///
/// The ROC calendar should not be confused with the Chinese traditional lunar calendar
/// (see [`Chinese`](crate::cal::Chinese)).
///
/// # Era codes
///
/// This calendar uses two era codes: `roc`, corresponding to years in the 民國 era (CE year 1912 and
/// after), and `broc`, corresponding to years before the 民國 era (CE year 1911 and before).
///
///
/// # Month codes
///
/// This calendar supports 12 solar month codes (`"M01" - "M12"`)
#[derive(Copy, Clone, Debug, Default)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct Roc;

/// The inner date type used for representing [`Date`]s of [`Roc`]. See [`Date`] and [`Roc`] for more info.
#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord)]
pub struct RocDateInner(IsoDateInner);

impl crate::cal::scaffold::UnstableSealed for Roc {}
impl Calendar for Roc {
    type DateInner = RocDateInner;
    type Year = types::EraYear;

    fn from_codes(
        &self,
        era: Option<&str>,
        year: i32,
        month_code: crate::types::MonthCode,
        day: u8,
    ) -> Result<Self::DateInner, DateError> {
        let year = match era {
            Some("roc") | None => ROC_ERA_OFFSET + year_check(year, 1..)?,
            Some("broc") => ROC_ERA_OFFSET + 1 - year_check(year, 1..)?,
            Some(_) => return Err(DateError::UnknownEra),
        };

        ArithmeticDate::new_from_codes(self, year, month_code, day)
            .map(IsoDateInner)
            .map(RocDateInner)
    }

    fn from_rata_die(&self, rd: RataDie) -> Self::DateInner {
        RocDateInner(Iso.from_rata_die(rd))
    }

    fn to_rata_die(&self, date: &Self::DateInner) -> RataDie {
        Iso.to_rata_die(&date.0)
    }

    fn from_iso(&self, iso: IsoDateInner) -> Self::DateInner {
        RocDateInner(iso)
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

    fn offset_date(&self, date: &mut Self::DateInner, offset: crate::DateDuration<Self>) {
        Iso.offset_date(&mut date.0, offset.cast_unit())
    }

    fn until(
        &self,
        date1: &Self::DateInner,
        date2: &Self::DateInner,
        _calendar2: &Self,
        largest_unit: crate::DateDurationUnit,
        smallest_unit: crate::DateDurationUnit,
    ) -> crate::DateDuration<Self> {
        Iso.until(&date1.0, &date2.0, &Iso, largest_unit, smallest_unit)
            .cast_unit()
    }

    fn debug_name(&self) -> &'static str {
        "ROC"
    }

    fn year_info(&self, date: &Self::DateInner) -> Self::Year {
        let extended_year = self.extended_year(date);
        if extended_year > 0 {
            types::EraYear {
                era: tinystr!(16, "roc"),
                era_index: Some(1),
                year: extended_year,
                ambiguity: types::YearAmbiguity::CenturyRequired,
            }
        } else {
            types::EraYear {
                era: tinystr!(16, "broc"),
                era_index: Some(0),
                year: 1 - extended_year,
                ambiguity: types::YearAmbiguity::EraAndCenturyRequired,
            }
        }
    }

    fn extended_year(&self, date: &Self::DateInner) -> i32 {
        Iso.extended_year(&date.0) - ROC_ERA_OFFSET
    }

    fn is_in_leap_year(&self, date: &Self::DateInner) -> bool {
        Iso.is_in_leap_year(&date.0)
    }

    fn month(&self, date: &Self::DateInner) -> crate::types::MonthInfo {
        Iso.month(&date.0)
    }

    fn day_of_month(&self, date: &Self::DateInner) -> crate::types::DayOfMonth {
        Iso.day_of_month(&date.0)
    }

    fn day_of_year(&self, date: &Self::DateInner) -> types::DayOfYear {
        Iso.day_of_year(&date.0)
    }

    fn calendar_algorithm(&self) -> Option<crate::preferences::CalendarAlgorithm> {
        Some(crate::preferences::CalendarAlgorithm::Roc)
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
    /// assert_eq!(date_roc.era_year().era, tinystr!(16, "roc"));
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
        let iso_year = year.saturating_add(ROC_ERA_OFFSET);
        Date::try_new_iso(iso_year, month, day).map(|d| Date::new_from_iso(d, Roc))
    }
}

#[cfg(test)]
mod test {

    use super::*;
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
