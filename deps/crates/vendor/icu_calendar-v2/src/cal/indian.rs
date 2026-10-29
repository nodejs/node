// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::calendar_arithmetic::ArithmeticDate;
use crate::calendar_arithmetic::DateFieldsResolver;
use crate::error::{
    DateAddError, DateFromFieldsError, DateNewError, EcmaReferenceYearError, UnknownEraError,
};
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
        // months are 30 days
        30
            // except for the first 6, which are 31
            + (month <= 6) as u8
            // except for the first one in non-leap years
            - (month == 1 && !calendrical_calculations::gregorian::is_leap_year(year + YEAR_OFFSET)) as u8
    }

    #[inline]
    fn extended_year_from_era_year_unchecked(
        &self,
        era: &[u8],
        era_year: i32,
    ) -> Result<i32, UnknownEraError> {
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
    fn extended_from_year_info(&self, year_info: Self::YearInfo) -> i32 {
        year_info
    }

    #[inline]
    fn reference_year_from_month_day(
        &self,
        month: types::Month,
        day: u8,
    ) -> Result<Self::YearInfo, EcmaReferenceYearError> {
        let (ordinal_month, false) = (month.number(), month.is_leap()) else {
            return Err(EcmaReferenceYearError::MonthNotInCalendar);
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

    fn to_rata_die_inner(year: Self::YearInfo, month: u8, day: u8) -> RataDie {
        // This is implemented in terms of other manual impls, might as well reuse them
        let date = IndianDateInner(ArithmeticDate::new_unchecked(year, month, day));
        let day_of_year_indian = Indian.day_of_year(&date).0; // 1-indexed
        let days_in_year = Indian.days_in_year(&date);

        let mut year_iso = date.0.year() + YEAR_OFFSET;
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
}

impl crate::cal::scaffold::UnstableSealed for Indian {}
impl Calendar for Indian {
    type DateInner = IndianDateInner;
    type Year = types::EraYear;
    type DateCompatibilityError = core::convert::Infallible;

    fn new_date(
        &self,
        year: types::YearInput,
        month: types::Month,
        day: u8,
    ) -> Result<Self::DateInner, DateNewError> {
        ArithmeticDate::from_input_year_month_code_day(year, month, day, self).map(IndianDateInner)
    }

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

        // date is in the valid RD range
        IndianDateInner(ArithmeticDate::new_unchecked(year, month, day))
    }

    // Algorithms directly implemented in icu_calendar since they're not from the book
    fn to_rata_die(&self, date: &Self::DateInner) -> RataDie {
        date.0.to_rata_die()
    }

    fn has_cheap_iso_conversion(&self) -> bool {
        false
    }

    fn months_in_year(&self, date: &Self::DateInner) -> u8 {
        Self::months_in_provided_year(date.0.year())
    }

    fn days_in_year(&self, date: &Self::DateInner) -> u16 {
        if self.is_in_leap_year(date) {
            366
        } else {
            365
        }
    }

    fn days_in_month(&self, date: &Self::DateInner) -> u8 {
        Self::days_in_provided_month(date.0.year(), date.0.month())
    }

    fn add(
        &self,
        date: &Self::DateInner,
        duration: types::DateDuration,
        options: DateAddOptions,
    ) -> Result<Self::DateInner, DateAddError> {
        date.0.added(duration, self, options).map(IndianDateInner)
    }

    fn until(
        &self,
        date1: &Self::DateInner,
        date2: &Self::DateInner,
        options: DateDifferenceOptions,
    ) -> types::DateDuration {
        date1.0.until(&date2.0, self, options)
    }

    fn check_date_compatibility(&self, &Self: &Self) -> Result<(), Self::DateCompatibilityError> {
        Ok(())
    }

    fn year_info(&self, date: &Self::DateInner) -> Self::Year {
        let extended_year = date.0.year();
        types::EraYear {
            era_index: Some(0),
            era: tinystr!(16, "shaka"),
            year: extended_year,
            extended_year,
            ambiguity: types::YearAmbiguity::CenturyRequired,
        }
    }

    fn is_in_leap_year(&self, date: &Self::DateInner) -> bool {
        calendrical_calculations::gregorian::is_leap_year(date.0.year() + YEAR_OFFSET)
    }

    fn month(&self, date: &Self::DateInner) -> types::MonthInfo {
        types::MonthInfo::new(self, date.0)
    }

    fn day_of_month(&self, date: &Self::DateInner) -> types::DayOfMonth {
        types::DayOfMonth(date.0.day())
    }

    fn day_of_year(&self, date: &Self::DateInner) -> types::DayOfYear {
        types::DayOfYear(
            (
                // 30 day months
                30 * (date.0.month() as u16 - 1)
                // First six months are 31 days
                + if date.0.month() - 1 < 6 { date.0.month() as u16 - 1 } else { 6 }
                // Except month 1 outside a leap year
                - (date.0.month() > 1 && !calendrical_calculations::gregorian::is_leap_year(date.0.year() + YEAR_OFFSET)) as u16
            ) + date.0.day() as u16,
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
    /// Construct new Indian [`Date`].
    ///
    /// Years are arithmetic, meaning there is a year 0 preceded by negative years, with a
    /// valid range of `-9999..=9999`.
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
        ArithmeticDate::from_year_month_day(year, month, day, &Indian)
            .map(IndianDateInner)
            .map(|inner| Date::from_raw(inner, Indian))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::types::RataDie;

    #[test]
    fn roundtrip_indian() {
        // Ultimately the day of the year will always be identical regardless of it
        // being a leap year or not
        // Test dates that occur after and before Chaitra 1 (March 22/21), in all years of
        // a four-year leap cycle, to ensure that all code paths are tested
        let cases = [
            TestCase {
                rd: Date::try_new_iso(2022, 8, 29).unwrap().to_rata_die(),
                year: 1944,
                month: 6,
                day: 7,
            },
            TestCase {
                rd: Date::try_new_iso(2021, 8, 29).unwrap().to_rata_die(),
                year: 1943,
                month: 6,
                day: 7,
            },
            TestCase {
                rd: Date::try_new_iso(2020, 8, 29).unwrap().to_rata_die(),
                year: 1942,
                month: 6,
                day: 7,
            },
            TestCase {
                rd: Date::try_new_iso(2019, 8, 29).unwrap().to_rata_die(),
                year: 1941,
                month: 6,
                day: 7,
            },
            TestCase {
                rd: Date::try_new_iso(2023, 1, 27).unwrap().to_rata_die(),
                year: 1944,
                month: 11,
                day: 7,
            },
            TestCase {
                rd: Date::try_new_iso(2022, 1, 27).unwrap().to_rata_die(),
                year: 1943,
                month: 11,
                day: 7,
            },
            TestCase {
                rd: Date::try_new_iso(2021, 1, 27).unwrap().to_rata_die(),
                year: 1942,
                month: 11,
                day: 7,
            },
            TestCase {
                rd: Date::try_new_iso(2020, 1, 27).unwrap().to_rata_die(),
                year: 1941,
                month: 11,
                day: 7,
            },
        ];

        for case in cases {
            check_case(case);
        }
    }

    #[derive(Debug)]
    struct TestCase {
        rd: RataDie,
        year: i32,
        month: u8,
        day: u8,
    }

    fn check_case(case: TestCase) {
        let date = Date::from_rata_die(case.rd, Indian);

        assert_eq!(date.to_rata_die(), case.rd, "{case:?}");

        assert_eq!(date.era_year().year, case.year, "{case:?}");
        assert_eq!(date.month().ordinal, case.month, "{case:?}");
        assert_eq!(date.day_of_month().0, case.day, "{case:?}");

        assert_eq!(
            Date::try_new_indian(
                date.era_year().extended_year,
                date.month().ordinal,
                date.day_of_month().0
            ),
            Ok(date)
        );
    }

    #[test]
    fn test_cases_near_epoch_start() {
        let cases = [
            TestCase {
                rd: Date::try_new_iso(79, 3, 23).unwrap().to_rata_die(),
                year: 1,
                month: 1,
                day: 2,
            },
            TestCase {
                rd: Date::try_new_iso(79, 3, 22).unwrap().to_rata_die(),
                year: 1,
                month: 1,
                day: 1,
            },
            TestCase {
                rd: Date::try_new_iso(79, 3, 21).unwrap().to_rata_die(),
                year: 0,
                month: 12,
                day: 30,
            },
            TestCase {
                rd: Date::try_new_iso(79, 3, 20).unwrap().to_rata_die(),
                year: 0,
                month: 12,
                day: 29,
            },
            TestCase {
                rd: Date::try_new_iso(78, 3, 21).unwrap().to_rata_die(),
                year: -1,
                month: 12,
                day: 30,
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
                rd: Date::try_new_iso(1, 3, 22).unwrap().to_rata_die(),
                year: -77,
                month: 1,
                day: 1,
            },
            TestCase {
                rd: Date::try_new_iso(1, 3, 21).unwrap().to_rata_die(),
                year: -78,
                month: 12,
                day: 30,
            },
            TestCase {
                rd: Date::try_new_iso(1, 1, 1).unwrap().to_rata_die(),
                year: -78,
                month: 10,
                day: 11,
            },
            TestCase {
                rd: Date::try_new_iso(0, 3, 21).unwrap().to_rata_die(),
                year: -78,
                month: 1,
                day: 1,
            },
            TestCase {
                rd: Date::try_new_iso(0, 1, 1).unwrap().to_rata_die(),
                year: -79,
                month: 10,
                day: 11,
            },
            TestCase {
                rd: Date::try_new_iso(-1, 3, 21).unwrap().to_rata_die(),
                year: -80,
                month: 12,
                day: 30,
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
