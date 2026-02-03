// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! This module contains types and implementations for the Korean Dangi calendar.
//!
//! ```rust
//! use icu::calendar::cal::Dangi;
//! use icu::calendar::Date;
//!
//! let dangi = Dangi::new();
//! let dangi_date = Date::try_new_dangi_with_calendar(2023, 6, 6, dangi)
//!     .expect("Failed to initialize Dangi Date instance.");
//!
//! assert_eq!(dangi_date.cyclic_year().related_iso, 2023);
//! assert_eq!(dangi_date.cyclic_year().year, 40);
//! assert_eq!(dangi_date.month().ordinal, 6);
//! assert_eq!(dangi_date.day_of_month().0, 6);
//! ```

use crate::cal::chinese_based::{ChineseBasedPrecomputedData, ChineseBasedWithDataLoading};
use crate::cal::iso::{Iso, IsoDateInner};
use crate::calendar_arithmetic::PrecomputedDataSource;
use crate::calendar_arithmetic::{ArithmeticDate, CalendarArithmetic};
use crate::error::DateError;
use crate::provider::chinese_based::CalendarDangiV1;
use crate::types::CyclicYear;
use crate::AsCalendar;
use crate::{types, Calendar, Date};
use calendrical_calculations::chinese_based;
use calendrical_calculations::rata_die::RataDie;
use core::cmp::Ordering;
use icu_provider::prelude::*;

/// The [Traditional Korean (Dangi) Calendar](https://en.wikipedia.org/wiki/Korean_calendar)
///
/// The Dangi Calendar is a lunisolar calendar used traditionally in North and South Korea.
/// It is often used today to track important cultural events and holidays like Seollal
/// (Korean lunar new year). It is similar to the Chinese lunar calendar (see [`Chinese`](super::Chinese)),
/// except that observations are based in Korea (currently UTC+9) rather than China (UTC+8).
/// This can cause some differences; for example, 2012 was a leap year, but in the Dangi
/// calendar the leap month was 3, while in the Chinese calendar the leap month was 4.
///
/// This calendar is currently in a preview state: formatting for this calendar is not
/// going to be perfect.
///
/// ```rust
/// use icu::calendar::cal::{Chinese, Dangi};
/// use icu::calendar::Date;
/// use tinystr::tinystr;
///
/// let iso_a = Date::try_new_iso(2012, 4, 23).unwrap();
/// let dangi_a = iso_a.to_calendar(Dangi::new());
/// let chinese_a = iso_a.to_calendar(Chinese::new());
///
/// assert_eq!(dangi_a.month().standard_code.0, tinystr!(4, "M03L"));
/// assert_eq!(chinese_a.month().standard_code.0, tinystr!(4, "M04"));
///
/// let iso_b = Date::try_new_iso(2012, 5, 23).unwrap();
/// let dangi_b = iso_b.to_calendar(Dangi::new());
/// let chinese_b = iso_b.to_calendar(Chinese::new());
///
/// assert_eq!(dangi_b.month().standard_code.0, tinystr!(4, "M04"));
/// assert_eq!(chinese_b.month().standard_code.0, tinystr!(4, "M04L"));
/// ```
/// # Era codes
///
/// This calendar does not use era codes.
///
/// # Month codes
///
/// This calendar is a lunisolar calendar. It supports regular month codes `"M01" - "M12"` as well
/// as leap month codes `"M01L" - "M12L"`.
#[derive(Clone, Debug, Default)]
pub struct Dangi {
    data: Option<DataPayload<CalendarDangiV1>>,
}

/// The inner date type used for representing [`Date`]s of [`Dangi`]. See [`Date`] and [`Dangi`] for more detail.
#[derive(Debug, Eq, PartialEq, PartialOrd, Ord)]
pub struct DangiDateInner(ArithmeticDate<Dangi>);

// we want these impls without the `C: Copy/Clone` bounds
impl Copy for DangiDateInner {}
impl Clone for DangiDateInner {
    fn clone(&self) -> Self {
        *self
    }
}

// These impls just make custom derives on types containing C
// work. They're basically no-ops
impl PartialEq for Dangi {
    fn eq(&self, _: &Self) -> bool {
        true
    }
}
impl Eq for Dangi {}
#[allow(clippy::non_canonical_partial_ord_impl)] // this is intentional
impl PartialOrd for Dangi {
    fn partial_cmp(&self, _: &Self) -> Option<Ordering> {
        Some(Ordering::Equal)
    }
}

impl Ord for Dangi {
    fn cmp(&self, _: &Self) -> Ordering {
        Ordering::Equal
    }
}

impl Dangi {
    /// Creates a new [`Dangi`] with some precomputed calendrical calculations.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub const fn new() -> Self {
        Self {
            data: Some(DataPayload::from_static_ref(
                crate::provider::Baked::SINGLETON_CALENDAR_DANGI_V1,
            )),
        }
    }

    icu_provider::gen_buffer_data_constructors!(() -> error: DataError,
        functions: [
            new: skip,
            try_new_with_buffer_provider,
            try_new_unstable,
            Self,
    ]);

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::new)]
    pub fn try_new_unstable<D: DataProvider<CalendarDangiV1> + ?Sized>(
        provider: &D,
    ) -> Result<Self, DataError> {
        Ok(Self {
            data: Some(provider.load(Default::default())?.payload),
        })
    }

    /// Construct a new [`Dangi`] without any precomputed calendrical calculations.
    pub fn new_always_calculating() -> Self {
        Dangi { data: None }
    }

    pub(crate) const DEBUG_NAME: &'static str = "Dangi";
}

impl crate::cal::scaffold::UnstableSealed for Dangi {}
impl Calendar for Dangi {
    type DateInner = DangiDateInner;
    type Year = CyclicYear;

    fn from_codes(
        &self,
        era: Option<&str>,
        year: i32,
        month_code: crate::types::MonthCode,
        day: u8,
    ) -> Result<Self::DateInner, DateError> {
        match era {
            None => {}
            _ => return Err(DateError::UnknownEra),
        }

        let year = self.get_precomputed_data().load_or_compute_info(year);

        let Some(month) = year.parse_month_code(month_code) else {
            return Err(DateError::UnknownMonthCode(month_code));
        };

        year.validate_md(month, day)?;

        Ok(DangiDateInner(ArithmeticDate::new_unchecked(
            year, month, day,
        )))
    }

    fn from_rata_die(&self, rd: RataDie) -> Self::DateInner {
        let iso = Iso.from_rata_die(rd);
        let y = self
            .get_precomputed_data()
            .load_or_compute_info_for_rd(rd, iso.0);
        let (m, d) = y.md_from_rd(rd);
        DangiDateInner(ArithmeticDate::new_unchecked(y, m, d))
    }

    fn to_rata_die(&self, date: &Self::DateInner) -> RataDie {
        date.0.year.rd_from_md(date.0.month, date.0.day)
    }

    fn from_iso(&self, iso: IsoDateInner) -> Self::DateInner {
        let rd = Iso.to_rata_die(&iso);
        let y = self
            .get_precomputed_data()
            .load_or_compute_info_for_rd(rd, iso.0);
        let (m, d) = y.md_from_rd(rd);
        DangiDateInner(ArithmeticDate::new_unchecked(y, m, d))
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

    fn offset_date(&self, date: &mut Self::DateInner, offset: crate::DateDuration<Self>) {
        date.0.offset_date(offset, &self.get_precomputed_data());
    }

    fn until(
        &self,
        date1: &Self::DateInner,
        date2: &Self::DateInner,
        _calendar2: &Self,
        largest_unit: crate::DateDurationUnit,
        smallest_unit: crate::DateDurationUnit,
    ) -> crate::DateDuration<Self> {
        date1.0.until(date2.0, largest_unit, smallest_unit)
    }

    fn debug_name(&self) -> &'static str {
        Self::DEBUG_NAME
    }

    fn year_info(&self, date: &Self::DateInner) -> Self::Year {
        let year = date.0.year;
        CyclicYear {
            year: (year.related_iso as i64 - 4).rem_euclid(60) as u8 + 1,
            related_iso: year.related_iso,
        }
    }

    fn extended_year(&self, date: &Self::DateInner) -> i32 {
        chinese_based::extended_from_iso::<chinese_based::Dangi>(date.0.year.related_iso)
    }

    fn is_in_leap_year(&self, date: &Self::DateInner) -> bool {
        Self::provided_year_is_leap(date.0.year)
    }

    fn month(&self, date: &Self::DateInner) -> crate::types::MonthInfo {
        date.0.year.month(date.0.month)
    }

    fn day_of_month(&self, date: &Self::DateInner) -> crate::types::DayOfMonth {
        date.0.day_of_month()
    }

    fn day_of_year(&self, date: &Self::DateInner) -> crate::types::DayOfYear {
        types::DayOfYear(date.0.year.day_of_year(date.0.month, date.0.day))
    }

    fn calendar_algorithm(&self) -> Option<crate::preferences::CalendarAlgorithm> {
        Some(crate::preferences::CalendarAlgorithm::Dangi)
    }
}

impl<A: AsCalendar<Calendar = Dangi>> Date<A> {
    /// Construct a new Dangi date from a `year`, `month`, and `day`.
    /// `year` represents the [ISO](crate::Iso) year that roughly matches the Dangi year;
    /// `month` represents the month of the year ordinally (ex. if it is a leap year, the last month will be 13, not 12);
    /// `day` indicates day of month.
    ///
    /// This date will not use any precomputed calendrical calculations,
    /// one that loads such data from a provider will be added in the future (#3933)
    ///
    /// ```rust
    /// use icu::calendar::cal::Dangi;
    /// use icu::calendar::Date;
    ///
    /// let dangi = Dangi::new();
    ///
    /// let date_dangi = Date::try_new_dangi_with_calendar(2023, 6, 18, dangi)
    ///     .expect("Failed to initialize Dangi Date instance.");
    ///
    /// assert_eq!(date_dangi.cyclic_year().related_iso, 2023);
    /// assert_eq!(date_dangi.cyclic_year().year, 40);
    /// assert_eq!(date_dangi.month().ordinal, 6);
    /// assert_eq!(date_dangi.day_of_month().0, 18);
    /// ```
    pub fn try_new_dangi_with_calendar(
        related_iso_year: i32,
        month: u8,
        day: u8,
        calendar: A,
    ) -> Result<Date<A>, DateError> {
        let year = calendar
            .as_calendar()
            .get_precomputed_data()
            .load_or_compute_info(related_iso_year);
        year.validate_md(month, day)?;
        Ok(Date::from_raw(
            DangiDateInner(ArithmeticDate::new_unchecked(year, month, day)),
            calendar,
        ))
    }
}

impl ChineseBasedWithDataLoading for Dangi {
    type CB = calendrical_calculations::chinese_based::Dangi;
    fn get_precomputed_data(&self) -> ChineseBasedPrecomputedData<Self::CB> {
        ChineseBasedPrecomputedData::new(self.data.as_ref().map(|d| d.get()))
    }
}

#[cfg(test)]
mod test {

    use super::*;
    use crate::cal::Chinese;
    use calendrical_calculations::rata_die::RataDie;

    /// Run a test twice, with two calendars
    fn do_twice(
        dangi_calculating: &Dangi,
        dangi_cached: &Dangi,
        test: impl Fn(crate::Ref<Dangi>, &'static str),
    ) {
        test(crate::Ref(dangi_calculating), "calculating");
        test(crate::Ref(dangi_cached), "cached");
    }

    fn check_cyclic_and_rel_iso(year: i32) {
        let iso = Date::try_new_iso(year, 6, 6).unwrap();
        let chinese = iso.to_calendar(Chinese::new_always_calculating());
        let dangi = iso.to_calendar(Dangi::new_always_calculating());
        let chinese_year = chinese.cyclic_year();
        let korean_year = dangi.cyclic_year();
        assert_eq!(
            chinese_year, korean_year,
            "Cyclic year failed for year: {year}"
        );
        let chinese_rel_iso = chinese_year.related_iso;
        let korean_rel_iso = korean_year.related_iso;
        assert_eq!(
            chinese_rel_iso, korean_rel_iso,
            "Rel. ISO year equality failed for year: {year}"
        );
        assert_eq!(korean_rel_iso, year, "Dangi Rel. ISO failed!");
    }

    #[test]
    fn test_cyclic_same_as_chinese_near_present_day() {
        for year in 1923..=2123 {
            check_cyclic_and_rel_iso(year);
        }
    }

    #[test]
    fn test_cyclic_same_as_chinese_near_rd_zero() {
        for year in -100..=100 {
            check_cyclic_and_rel_iso(year);
        }
    }

    #[test]
    fn test_iso_to_dangi_roundtrip() {
        let mut rd = -1963020;
        let max_rd = 1963020;
        let mut iters = 0;
        let max_iters = 560;
        let dangi_calculating = Dangi::new_always_calculating();
        let dangi_cached = Dangi::new();
        while rd < max_rd && iters < max_iters {
            let rata_die = RataDie::new(rd);
            let iso = Date::from_rata_die(rata_die, Iso);
            do_twice(&dangi_calculating, &dangi_cached, |dangi, calendar_type| {
                let korean = iso.to_calendar(dangi);
                let result = korean.to_calendar(Iso);
                assert_eq!(
                    iso, result,
                    "[{calendar_type}] Failed roundtrip ISO -> Dangi -> ISO for RD: {rd}"
                );
            });

            rd += 7043;
            iters += 1;
        }
    }

    #[test]
    fn test_dangi_consistent_with_icu() {
        // Test cases for this test are derived from existing ICU Intl.DateTimeFormat. If there is a bug in ICU,
        // these test cases may be affected, and this calendar's output may not be entirely valid.

        // There are a number of test cases which do not match ICU for dates very far in the past or future,
        // see #3709.

        #[derive(Debug)]
        struct TestCase {
            iso_year: i32,
            iso_month: u8,
            iso_day: u8,
            expected_rel_iso: i32,
            expected_cyclic: u8,
            expected_month: u8,
            expected_day: u8,
        }

        let cases = [
            TestCase {
                // #3709: This test case fails to match ICU
                iso_year: 4321,
                iso_month: 1,
                iso_day: 23,
                expected_rel_iso: 4320,
                expected_cyclic: 57,
                expected_month: 13,
                expected_day: 12,
            },
            TestCase {
                iso_year: 3649,
                iso_month: 9,
                iso_day: 20,
                expected_rel_iso: 3649,
                expected_cyclic: 46,
                expected_month: 9,
                expected_day: 1,
            },
            TestCase {
                iso_year: 3333,
                iso_month: 3,
                iso_day: 3,
                expected_rel_iso: 3333,
                expected_cyclic: 30,
                expected_month: 1,
                expected_day: 25,
            },
            TestCase {
                iso_year: 3000,
                iso_month: 3,
                iso_day: 30,
                expected_rel_iso: 3000,
                expected_cyclic: 57,
                expected_month: 3,
                expected_day: 3,
            },
            TestCase {
                iso_year: 2772,
                iso_month: 7,
                iso_day: 27,
                expected_rel_iso: 2772,
                expected_cyclic: 9,
                expected_month: 7,
                expected_day: 5,
            },
            TestCase {
                iso_year: 2525,
                iso_month: 2,
                iso_day: 25,
                expected_rel_iso: 2525,
                expected_cyclic: 2,
                expected_month: 2,
                expected_day: 3,
            },
            TestCase {
                iso_year: 2345,
                iso_month: 3,
                iso_day: 21,
                expected_rel_iso: 2345,
                expected_cyclic: 2,
                expected_month: 2,
                expected_day: 17,
            },
            TestCase {
                iso_year: 2222,
                iso_month: 2,
                iso_day: 22,
                expected_rel_iso: 2222,
                expected_cyclic: 59,
                expected_month: 1,
                expected_day: 11,
            },
            TestCase {
                iso_year: 2167,
                iso_month: 6,
                iso_day: 22,
                expected_rel_iso: 2167,
                expected_cyclic: 4,
                expected_month: 5,
                expected_day: 6,
            },
            TestCase {
                iso_year: 2121,
                iso_month: 2,
                iso_day: 12,
                expected_rel_iso: 2120,
                expected_cyclic: 17,
                expected_month: 13,
                expected_day: 25,
            },
            TestCase {
                iso_year: 2080,
                iso_month: 12,
                iso_day: 31,
                expected_rel_iso: 2080,
                expected_cyclic: 37,
                expected_month: 12,
                expected_day: 21,
            },
            TestCase {
                iso_year: 2030,
                iso_month: 3,
                iso_day: 20,
                expected_rel_iso: 2030,
                expected_cyclic: 47,
                expected_month: 2,
                expected_day: 17,
            },
            TestCase {
                iso_year: 2027,
                iso_month: 2,
                iso_day: 7,
                expected_rel_iso: 2027,
                expected_cyclic: 44,
                expected_month: 1,
                expected_day: 1,
            },
            TestCase {
                iso_year: 2023,
                iso_month: 7,
                iso_day: 1,
                expected_rel_iso: 2023,
                expected_cyclic: 40,
                expected_month: 6,
                expected_day: 14,
            },
            TestCase {
                iso_year: 2022,
                iso_month: 3,
                iso_day: 1,
                expected_rel_iso: 2022,
                expected_cyclic: 39,
                expected_month: 1,
                expected_day: 29,
            },
            TestCase {
                iso_year: 2021,
                iso_month: 2,
                iso_day: 1,
                expected_rel_iso: 2020,
                expected_cyclic: 37,
                expected_month: 13,
                expected_day: 20,
            },
            TestCase {
                iso_year: 2016,
                iso_month: 3,
                iso_day: 30,
                expected_rel_iso: 2016,
                expected_cyclic: 33,
                expected_month: 2,
                expected_day: 22,
            },
            TestCase {
                iso_year: 2016,
                iso_month: 7,
                iso_day: 30,
                expected_rel_iso: 2016,
                expected_cyclic: 33,
                expected_month: 6,
                expected_day: 27,
            },
            TestCase {
                iso_year: 2015,
                iso_month: 9,
                iso_day: 22,
                expected_rel_iso: 2015,
                expected_cyclic: 32,
                expected_month: 8,
                expected_day: 10,
            },
            TestCase {
                iso_year: 2013,
                iso_month: 10,
                iso_day: 1,
                expected_rel_iso: 2013,
                expected_cyclic: 30,
                expected_month: 8,
                expected_day: 27,
            },
            TestCase {
                iso_year: 2010,
                iso_month: 2,
                iso_day: 1,
                expected_rel_iso: 2009,
                expected_cyclic: 26,
                expected_month: 13,
                expected_day: 18,
            },
            TestCase {
                iso_year: 2000,
                iso_month: 8,
                iso_day: 30,
                expected_rel_iso: 2000,
                expected_cyclic: 17,
                expected_month: 8,
                expected_day: 2,
            },
            TestCase {
                iso_year: 1990,
                iso_month: 11,
                iso_day: 11,
                expected_rel_iso: 1990,
                expected_cyclic: 7,
                expected_month: 10,
                expected_day: 24,
            },
            TestCase {
                iso_year: 1970,
                iso_month: 6,
                iso_day: 10,
                expected_rel_iso: 1970,
                expected_cyclic: 47,
                expected_month: 5,
                expected_day: 7,
            },
            TestCase {
                iso_year: 1970,
                iso_month: 1,
                iso_day: 1,
                expected_rel_iso: 1969,
                expected_cyclic: 46,
                expected_month: 11,
                expected_day: 24,
            },
            TestCase {
                iso_year: 1941,
                iso_month: 12,
                iso_day: 7,
                expected_rel_iso: 1941,
                expected_cyclic: 18,
                expected_month: 11,
                expected_day: 19,
            },
            TestCase {
                iso_year: 1812,
                iso_month: 5,
                iso_day: 4,
                expected_rel_iso: 1812,
                expected_cyclic: 9,
                expected_month: 3,
                expected_day: 24,
            },
            TestCase {
                iso_year: 1655,
                iso_month: 6,
                iso_day: 15,
                expected_rel_iso: 1655,
                expected_cyclic: 32,
                expected_month: 5,
                expected_day: 12,
            },
            TestCase {
                iso_year: 1333,
                iso_month: 3,
                iso_day: 10,
                expected_rel_iso: 1333,
                expected_cyclic: 10,
                expected_month: 2,
                expected_day: 16,
            },
            TestCase {
                iso_year: 1000,
                iso_month: 10,
                iso_day: 10,
                expected_rel_iso: 1000,
                expected_cyclic: 37,
                expected_month: 9,
                expected_day: 5,
            },
            TestCase {
                iso_year: 842,
                iso_month: 2,
                iso_day: 15,
                expected_rel_iso: 841,
                expected_cyclic: 58,
                expected_month: 13,
                expected_day: 28,
            },
            TestCase {
                iso_year: 101,
                iso_month: 1,
                iso_day: 10,
                expected_rel_iso: 100,
                expected_cyclic: 37,
                expected_month: 12,
                expected_day: 24,
            },
            TestCase {
                iso_year: -1,
                iso_month: 3,
                iso_day: 28,
                expected_rel_iso: -1,
                expected_cyclic: 56,
                expected_month: 2,
                expected_day: 25,
            },
            TestCase {
                iso_year: -3,
                iso_month: 2,
                iso_day: 28,
                expected_rel_iso: -3,
                expected_cyclic: 54,
                expected_month: 2,
                expected_day: 5,
            },
            TestCase {
                iso_year: -365,
                iso_month: 7,
                iso_day: 24,
                expected_rel_iso: -365,
                expected_cyclic: 52,
                expected_month: 6,
                expected_day: 24,
            },
            TestCase {
                iso_year: -999,
                iso_month: 9,
                iso_day: 9,
                expected_rel_iso: -999,
                expected_cyclic: 18,
                expected_month: 7,
                expected_day: 27,
            },
            TestCase {
                iso_year: -1500,
                iso_month: 1,
                iso_day: 5,
                expected_rel_iso: -1501,
                expected_cyclic: 56,
                expected_month: 12,
                expected_day: 2,
            },
            TestCase {
                iso_year: -2332,
                iso_month: 3,
                iso_day: 1,
                expected_rel_iso: -2332,
                expected_cyclic: 5,
                expected_month: 1,
                expected_day: 16,
            },
            TestCase {
                iso_year: -2332,
                iso_month: 2,
                iso_day: 15,
                expected_rel_iso: -2332,
                expected_cyclic: 5,
                expected_month: 1,
                expected_day: 1,
            },
            TestCase {
                // #3709: This test case fails to match ICU
                iso_year: -2332,
                iso_month: 2,
                iso_day: 14,
                expected_rel_iso: -2333,
                expected_cyclic: 4,
                expected_month: 13,
                expected_day: 30,
            },
            TestCase {
                // #3709: This test case fails to match ICU
                iso_year: -2332,
                iso_month: 1,
                iso_day: 17,
                expected_rel_iso: -2333,
                expected_cyclic: 4,
                expected_month: 13,
                expected_day: 2,
            },
            TestCase {
                // #3709: This test case fails to match ICU
                iso_year: -2332,
                iso_month: 1,
                iso_day: 16,
                expected_rel_iso: -2333,
                expected_cyclic: 4,
                expected_month: 13,
                expected_day: 1,
            },
            TestCase {
                iso_year: -2332,
                iso_month: 1,
                iso_day: 15,
                expected_rel_iso: -2333,
                expected_cyclic: 4,
                expected_month: 12,
                expected_day: 29,
            },
            TestCase {
                iso_year: -2332,
                iso_month: 1,
                iso_day: 1,
                expected_rel_iso: -2333,
                expected_cyclic: 4,
                expected_month: 12,
                expected_day: 15,
            },
            TestCase {
                iso_year: -2333,
                iso_month: 1,
                iso_day: 16,
                expected_rel_iso: -2334,
                expected_cyclic: 3,
                expected_month: 12,
                expected_day: 19,
            },
            TestCase {
                iso_year: -2333,
                iso_month: 1,
                iso_day: 27,
                expected_rel_iso: -2333,
                expected_cyclic: 4,
                expected_month: 1,
                expected_day: 1,
            },
            TestCase {
                iso_year: -2333,
                iso_month: 1,
                iso_day: 26,
                expected_rel_iso: -2334,
                expected_cyclic: 3,
                expected_month: 12,
                expected_day: 29,
            },
            TestCase {
                iso_year: -2600,
                iso_month: 9,
                iso_day: 16,
                expected_rel_iso: -2600,
                expected_cyclic: 37,
                expected_month: 8,
                expected_day: 16,
            },
            TestCase {
                iso_year: -2855,
                iso_month: 2,
                iso_day: 3,
                expected_rel_iso: -2856,
                expected_cyclic: 21,
                expected_month: 12,
                expected_day: 30,
            },
            TestCase {
                // #3709: This test case fails to match ICU
                iso_year: -3000,
                iso_month: 5,
                iso_day: 15,
                expected_rel_iso: -3000,
                expected_cyclic: 57,
                expected_month: 4,
                expected_day: 1,
            },
            TestCase {
                // #3709: This test case fails to match ICU
                iso_year: -3649,
                iso_month: 9,
                iso_day: 20,
                expected_rel_iso: -3649,
                expected_cyclic: 8,
                expected_month: 8,
                expected_day: 10,
            },
            TestCase {
                // #3709: This test case fails to match ICU
                iso_year: -3649,
                iso_month: 3,
                iso_day: 30,
                expected_rel_iso: -3649,
                expected_cyclic: 8,
                expected_month: 2,
                expected_day: 14,
            },
            TestCase {
                // #3709: This test case fails to match ICU
                iso_year: -3650,
                iso_month: 3,
                iso_day: 30,
                expected_rel_iso: -3650,
                expected_cyclic: 7,
                expected_month: 3,
                expected_day: 3,
            },
        ];

        let dangi_calculating = Dangi::new_always_calculating();
        let dangi_cached = Dangi::new();

        for case in cases {
            let iso = Date::try_new_iso(case.iso_year, case.iso_month, case.iso_day).unwrap();
            do_twice(&dangi_calculating, &dangi_cached, |dangi, calendar_type| {
                let dangi = iso.to_calendar(dangi);
                let dangi_cyclic = dangi.cyclic_year();
                let dangi_month = dangi.month().ordinal;
                let dangi_day = dangi.day_of_month().0;

                assert_eq!(
                    dangi_cyclic.related_iso, case.expected_rel_iso,
                    "[{calendar_type}] Related ISO failed for test case: {case:?}"
                );
                assert_eq!(
                    dangi_cyclic.year, case.expected_cyclic,
                    "[{calendar_type}] Cyclic year failed for test case: {case:?}"
                );
                assert_eq!(
                    dangi_month, case.expected_month,
                    "[{calendar_type}] Month failed for test case: {case:?}"
                );
                assert_eq!(
                    dangi_day, case.expected_day,
                    "[{calendar_type}] Day failed for test case: {case:?}"
                );
            });
        }
    }
}
