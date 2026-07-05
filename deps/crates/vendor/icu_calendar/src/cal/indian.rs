// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::calendar_arithmetic::ArithmeticDate;
use crate::calendar_arithmetic::DateFieldsResolver;
use crate::error::{DateError, DateFromFieldsError, EcmaReferenceYearError, UnknownEraError};
use crate::options::DateFromFieldsOptions;
use crate::options::{DateAddOptions, DateDifferenceOptions};
use crate::types::DateFields;
use crate::{types, Calendar, Date, RangeError};
use calendrical_calculations::rata_die::RataDie;
use tinystr::tinystr;

/// The [Indian National (Śaka) Calendar](https://en.wikipedia.org/wiki/Indian_national_calendar)
///
/// The Indian National calendar is a solar calendar created by the Indian government.
///
/// This implementation extends proleptically for dates before the calendar's creation
/// in 1879 Śaka (1957 CE).
///
/// This corresponds to the `"indian"` [CLDR calendar](https://unicode.org/reports/tr35/#UnicodeCalendarIdentifier).
///
/// # Era codes
///
/// This calendar uses a single era code: `shaka`, with Śaka 0 being 78 CE. Dates before this era use negative years.
///
/// # Months and days
///
/// The 12 months are called Chaitra (`M01`, 30 days), Vaisakha (`M02`, 31 days),
/// Jyaishtha (`M03`, 31 days), Ashadha (`M04`, 31 days), Sravana (`M05`, 31 days),
/// Bhadra (`M06`, 31 days), Asvina (`M07`, 30 days), Kartika (`M08`, 30 days),
/// Agrahayana or Margasirsha (`M09`, 30 days), Pausha (`M10`, 30 days), Magha (`M11`, 30 days),
/// Phalguna (`M12`, 30 days).
///
/// In leap years (years where the concurrent [`Gregorian`](crate::cal::Gregorian) year (`year + 78`) is leap),
/// Chaitra gains a 31st day.
///
/// Standard years thus have 365 days, and leap years 366.
///
/// # Calendar drift
///
/// The Indian calendar has the same year lengths and leap year rules as the Gregorian calendar,
/// so it experiences the same drift of 1 day in ~7700 years with respect to the seasons.
#[derive(Copy, Clone, Debug, Hash, Default, Eq, PartialEq, PartialOrd, Ord)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct Indian;

/// The inner date type used for representing [`Date`]s of [`Indian`]. See [`Date`] and [`Indian`] for more details.
#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord)]
pub struct IndianDateInner(ArithmeticDate<Indian>);

/// The Śaka era starts on the 81st day of the Gregorian year (March 22 or 21)
/// which is an 80 day offset. This number should be subtracted from Gregorian dates
const DAY_OFFSET: u16 = 80;
/// The Śaka era is 78 years behind Gregorian. This number should be added to Gregorian dates
const YEAR_OFFSET: i32 = 78;

impl DateFieldsResolver for Indian {
    type YearInfo = i32;

    fn days_in_provided_month(year: i32, month: u8) -> u8 {
        if month == 1 {
            30 + calendrical_calculations::gregorian::is_leap_year(year + YEAR_OFFSET) as u8
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

    #[inline]
    fn year_info_from_era(
        &self,
        era: &[u8],
        era_year: i32,
    ) -> Result<Self::YearInfo, UnknownEraError> {
        match era {
            b"shaka" => Ok(era_year),
            _ => Err(UnknownEraError),
        }
    }

    #[inline]
    fn year_info_from_extended(&self, extended_year: i32) -> Self::YearInfo {
        extended_year
    }

    #[inline]
    fn reference_year_from_month_day(
        &self,
        month_code: types::ValidMonthCode,
        day: u8,
    ) -> Result<Self::YearInfo, EcmaReferenceYearError> {
        let (ordinal_month, false) = month_code.to_tuple() else {
            return Err(EcmaReferenceYearError::MonthCodeNotInCalendar);
        };
        // December 31, 1972 occurs on 10th month, 10th day, 1894 Shaka
        // Note: 1894 Shaka is also a leap year
        let shaka_year = if ordinal_month < 10 || (ordinal_month == 10 && day <= 10) {
            1894
        } else {
            1893
        };
        Ok(shaka_year)
    }
}

impl crate::cal::scaffold::UnstableSealed for Indian {}
impl Calendar for Indian {
    type DateInner = IndianDateInner;
    type Year = types::EraYear;
    type DifferenceError = core::convert::Infallible;

    fn from_codes(
        &self,
        era: Option<&str>,
        year: i32,
        month_code: types::MonthCode,
        day: u8,
    ) -> Result<Self::DateInner, DateError> {
        ArithmeticDate::from_codes(era, year, month_code, day, self).map(IndianDateInner)
    }

    #[cfg(feature = "unstable")]
    fn from_fields(
        &self,
        fields: DateFields,
        options: DateFromFieldsOptions,
    ) -> Result<Self::DateInner, DateFromFieldsError> {
        ArithmeticDate::from_fields(fields, options, self).map(IndianDateInner)
    }

    // Algorithms directly implemented in icu_calendar since they're not from the book
    fn from_rata_die(&self, rd: RataDie) -> Self::DateInner {
        let iso_year = calendrical_calculations::gregorian::year_from_fixed(rd)
            .unwrap_or_else(|e| e.saturate());
        // Get day number in year (1 indexed)
        let day_of_year_iso =
            (rd - calendrical_calculations::gregorian::day_before_year(iso_year)) as u16;
        // Convert to Śaka year
        let mut year = iso_year - YEAR_OFFSET;
        // This is in the previous Indian year
        let day_of_year_indian = if day_of_year_iso <= DAY_OFFSET {
            year -= 1;
            let n_days = if calendrical_calculations::gregorian::is_leap_year(year + YEAR_OFFSET) {
                366
            } else {
                365
            };

            // calculate day of year in previous year
            n_days + day_of_year_iso - DAY_OFFSET
        } else {
            day_of_year_iso - DAY_OFFSET
        };
        let mut month = 1;
        let mut day = day_of_year_indian as i32;
        while month <= 12 {
            let month_days = Self::days_in_provided_month(year, month) as i32;
            if day <= month_days {
                break;
            } else {
                day -= month_days;
                month += 1;
            }
        }

        debug_assert!(day <= Self::days_in_provided_month(year, month) as i32);
        let day = day.try_into().unwrap_or(1);

        IndianDateInner(ArithmeticDate::new_unchecked(year, month, day))
    }

    // Algorithms directly implemented in icu_calendar since they're not from the book
    fn to_rata_die(&self, date: &Self::DateInner) -> RataDie {
        let day_of_year_indian = self.day_of_year(date).0; // 1-indexed
        let days_in_year = self.days_in_year(date);

        let mut year_iso = date.0.year + YEAR_OFFSET;
        // days_in_year is a valid day of the year, so we check > not >=
        let day_of_year_iso = if day_of_year_indian + DAY_OFFSET > days_in_year {
            year_iso += 1;
            // calculate day of year in next year
            day_of_year_indian + DAY_OFFSET - days_in_year
        } else {
            day_of_year_indian + DAY_OFFSET
        };

        calendrical_calculations::gregorian::day_before_year(year_iso) + day_of_year_iso as i64
    }

    fn has_cheap_iso_conversion(&self) -> bool {
        false
    }

    fn months_in_year(&self, date: &Self::DateInner) -> u8 {
        Self::months_in_provided_year(date.0.year)
    }

    fn days_in_year(&self, date: &Self::DateInner) -> u16 {
        if self.is_in_leap_year(date) {
            366
        } else {
            365
        }
    }

    fn days_in_month(&self, date: &Self::DateInner) -> u8 {
        Self::days_in_provided_month(date.0.year, date.0.month)
    }

    #[cfg(feature = "unstable")]
    fn add(
        &self,
        date: &Self::DateInner,
        duration: types::DateDuration,
        options: DateAddOptions,
    ) -> Result<Self::DateInner, DateError> {
        date.0.added(duration, self, options).map(IndianDateInner)
    }

    #[cfg(feature = "unstable")]
    fn until(
        &self,
        date1: &Self::DateInner,
        date2: &Self::DateInner,
        options: DateDifferenceOptions,
    ) -> Result<types::DateDuration, Self::DifferenceError> {
        Ok(date1.0.until(&date2.0, self, options))
    }

    fn year_info(&self, date: &Self::DateInner) -> Self::Year {
        let extended_year = date.0.year;
        types::EraYear {
            era_index: Some(0),
            era: tinystr!(16, "shaka"),
            year: extended_year,
            extended_year,
            ambiguity: types::YearAmbiguity::CenturyRequired,
        }
    }

    fn is_in_leap_year(&self, date: &Self::DateInner) -> bool {
        calendrical_calculations::gregorian::is_leap_year(date.0.year + YEAR_OFFSET)
    }

    fn month(&self, date: &Self::DateInner) -> types::MonthInfo {
        types::MonthInfo::non_lunisolar(date.0.month)
    }

    fn day_of_month(&self, date: &Self::DateInner) -> types::DayOfMonth {
        types::DayOfMonth(date.0.day)
    }

    fn day_of_year(&self, date: &Self::DateInner) -> types::DayOfYear {
        types::DayOfYear(
            (1..date.0.month)
                .map(|m| Self::days_in_provided_month(date.0.year, m) as u16)
                .sum::<u16>()
                + date.0.day as u16,
        )
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
        ArithmeticDate::try_from_ymd(year, month, day)
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
            let result = Date::from_rata_die(initial, Indian).to_rata_die();
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
            let result = Date::from_rata_die(initial, Indian).to_rata_die();
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
