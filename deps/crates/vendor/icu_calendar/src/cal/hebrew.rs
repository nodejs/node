// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! This module contains types and implementations for the Hebrew calendar.
//!
//! ```rust
//! use icu::calendar::Date;
//!
//! let hebrew_date = Date::try_new_hebrew(3425, 10, 11)
//!     .expect("Failed to initialize hebrew Date instance.");
//!
//! assert_eq!(hebrew_date.era_year().year, 3425);
//! assert_eq!(hebrew_date.month().ordinal, 10);
//! assert_eq!(hebrew_date.day_of_month().0, 11);
//! ```

use crate::cal::iso::{Iso, IsoDateInner};
use crate::calendar_arithmetic::PrecomputedDataSource;
use crate::calendar_arithmetic::{ArithmeticDate, CalendarArithmetic};
use crate::error::DateError;
use crate::types::MonthInfo;
use crate::RangeError;
use crate::{types, Calendar, Date, DateDuration, DateDurationUnit};
use ::tinystr::tinystr;
use calendrical_calculations::hebrew_keviyah::{Keviyah, YearInfo};
use calendrical_calculations::rata_die::RataDie;

/// The [Hebrew Calendar](https://en.wikipedia.org/wiki/Hebrew_calendar)
///
/// The Hebrew calendar is a lunisolar calendar used as the Jewish liturgical calendar
/// as well as an official calendar in Israel.
///
/// This calendar is the _civil_ Hebrew calendar, with the year starting at in the month of Tishrei.
///
/// # Era codes
///
/// This calendar uses a single era code `am`, Anno Mundi. Dates before this era use negative years.
///
/// # Month codes
///
/// This calendar is a lunisolar calendar and thus has a leap month. It supports codes `"M01"-"M12"`
/// for regular months, and the leap month Adar I being coded as `"M05L"`.
///
/// [`MonthInfo`] has slightly divergent behavior: because the regular month Adar is formatted
/// as "Adar II" in a leap year, this calendar will produce the special code `"M06L"` in any [`MonthInfo`]
/// objects it creates.
#[derive(Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord, Default)]
#[allow(clippy::exhaustive_structs)] // unit struct
pub struct Hebrew;

/// The inner date type used for representing [`Date`]s of [`Hebrew`]. See [`Date`] and [`Hebrew`] for more details.
#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord)]
pub struct HebrewDateInner(ArithmeticDate<Hebrew>);

impl Hebrew {
    /// Construct a new [`Hebrew`]
    pub fn new() -> Self {
        Hebrew
    }
}

#[derive(Copy, Clone, Debug, Eq, PartialEq, PartialOrd, Ord)]
pub(crate) struct HebrewYearInfo {
    keviyah: Keviyah,
    prev_keviyah: Keviyah,
    value: i32,
}

impl From<HebrewYearInfo> for i32 {
    fn from(value: HebrewYearInfo) -> Self {
        value.value
    }
}

impl HebrewYearInfo {
    /// Convenience method to compute for a given year. Don't use this if you actually need
    /// a YearInfo that you want to call .new_year() on.
    ///
    /// This can potentially be optimized with adjacent-year knowledge, but it's complex
    #[inline]
    fn compute(h_year: i32) -> Self {
        let keviyah = YearInfo::compute_for(h_year).keviyah;
        Self::compute_with_keviyah(keviyah, h_year)
    }
    /// Compute for a given year when the keviyah is already known
    #[inline]
    fn compute_with_keviyah(keviyah: Keviyah, h_year: i32) -> Self {
        let prev_keviyah = YearInfo::compute_for(h_year - 1).keviyah;
        Self {
            keviyah,
            prev_keviyah,
            value: h_year,
        }
    }
}
//  HEBREW CALENDAR

impl CalendarArithmetic for Hebrew {
    type YearInfo = HebrewYearInfo;

    fn days_in_provided_month(info: HebrewYearInfo, ordinal_month: u8) -> u8 {
        info.keviyah.month_len(ordinal_month)
    }

    fn months_in_provided_year(info: HebrewYearInfo) -> u8 {
        if info.keviyah.is_leap() {
            13
        } else {
            12
        }
    }

    fn days_in_provided_year(info: HebrewYearInfo) -> u16 {
        info.keviyah.year_length()
    }

    fn provided_year_is_leap(info: HebrewYearInfo) -> bool {
        info.keviyah.is_leap()
    }

    fn last_month_day_in_provided_year(info: HebrewYearInfo) -> (u8, u8) {
        info.keviyah.last_month_day_in_year()
    }
}

impl PrecomputedDataSource<HebrewYearInfo> for () {
    fn load_or_compute_info(&self, h_year: i32) -> HebrewYearInfo {
        HebrewYearInfo::compute(h_year)
    }
}

impl crate::cal::scaffold::UnstableSealed for Hebrew {}
impl Calendar for Hebrew {
    type DateInner = HebrewDateInner;
    type Year = types::EraYear;

    fn from_codes(
        &self,
        era: Option<&str>,
        year: i32,
        month_code: types::MonthCode,
        day: u8,
    ) -> Result<Self::DateInner, DateError> {
        match era {
            Some("am") | None => {}
            _ => return Err(DateError::UnknownEra),
        }

        let year = HebrewYearInfo::compute(year);

        let is_leap_year = year.keviyah.is_leap();

        let month_code_str = month_code.0.as_str();

        let month_ordinal = if is_leap_year {
            match month_code_str {
                "M01" => 1,
                "M02" => 2,
                "M03" => 3,
                "M04" => 4,
                "M05" => 5,
                "M05L" => 6,
                // M06L is the formatting era code used for Adar II
                "M06" | "M06L" => 7,
                "M07" => 8,
                "M08" => 9,
                "M09" => 10,
                "M10" => 11,
                "M11" => 12,
                "M12" => 13,
                _ => {
                    return Err(DateError::UnknownMonthCode(month_code));
                }
            }
        } else {
            match month_code_str {
                "M01" => 1,
                "M02" => 2,
                "M03" => 3,
                "M04" => 4,
                "M05" => 5,
                "M06" => 6,
                "M07" => 7,
                "M08" => 8,
                "M09" => 9,
                "M10" => 10,
                "M11" => 11,
                "M12" => 12,
                _ => {
                    return Err(DateError::UnknownMonthCode(month_code));
                }
            }
        };

        Ok(HebrewDateInner(ArithmeticDate::new_from_ordinals(
            year,
            month_ordinal,
            day,
        )?))
    }

    fn from_rata_die(&self, rd: RataDie) -> Self::DateInner {
        let (year, h_year) = YearInfo::year_containing_rd(rd);
        // Obtaining a 1-indexed day-in-year value
        let day = rd - year.new_year() + 1;
        let day = u16::try_from(day).unwrap_or(u16::MAX);

        let year = HebrewYearInfo::compute_with_keviyah(year.keviyah, h_year);
        let (month, day) = year.keviyah.month_day_for(day);
        HebrewDateInner(ArithmeticDate::new_unchecked(year, month, day))
    }

    fn to_rata_die(&self, date: &Self::DateInner) -> RataDie {
        let year = date.0.year.keviyah.year_info(date.0.year.value);

        let ny = year.new_year();
        let days_preceding = year.keviyah.days_preceding(date.0.month);

        // Need to subtract 1 since the new year is itself in this year
        ny + i64::from(days_preceding) + i64::from(date.0.day) - 1
    }

    fn from_iso(&self, iso: IsoDateInner) -> Self::DateInner {
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
        date.0.offset_date(offset, &())
    }

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

    fn debug_name(&self) -> &'static str {
        "Hebrew"
    }

    fn year_info(&self, date: &Self::DateInner) -> Self::Year {
        types::EraYear {
            era_index: Some(0),
            era: tinystr!(16, "am"),
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

    fn month(&self, date: &Self::DateInner) -> MonthInfo {
        let mut ordinal = date.0.month;
        let is_leap_year = Self::provided_year_is_leap(date.0.year);

        if is_leap_year {
            if ordinal == 6 {
                return types::MonthInfo {
                    ordinal,
                    standard_code: types::MonthCode(tinystr!(4, "M05L")),
                    formatting_code: types::MonthCode(tinystr!(4, "M05L")),
                };
            } else if ordinal == 7 {
                return types::MonthInfo {
                    ordinal,
                    // Adar II is the same as Adar and has the same code
                    standard_code: types::MonthCode(tinystr!(4, "M06")),
                    formatting_code: types::MonthCode(tinystr!(4, "M06L")),
                };
            }
        }

        if is_leap_year && ordinal > 6 {
            ordinal -= 1;
        }

        let code = match ordinal {
            1 => tinystr!(4, "M01"),
            2 => tinystr!(4, "M02"),
            3 => tinystr!(4, "M03"),
            4 => tinystr!(4, "M04"),
            5 => tinystr!(4, "M05"),
            6 => tinystr!(4, "M06"),
            7 => tinystr!(4, "M07"),
            8 => tinystr!(4, "M08"),
            9 => tinystr!(4, "M09"),
            10 => tinystr!(4, "M10"),
            11 => tinystr!(4, "M11"),
            12 => tinystr!(4, "M12"),
            _ => tinystr!(4, "und"),
        };

        types::MonthInfo {
            ordinal: date.0.month,
            standard_code: types::MonthCode(code),
            formatting_code: types::MonthCode(code),
        }
    }

    fn day_of_month(&self, date: &Self::DateInner) -> types::DayOfMonth {
        date.0.day_of_month()
    }

    fn day_of_year(&self, date: &Self::DateInner) -> types::DayOfYear {
        date.0.day_of_year()
    }

    fn calendar_algorithm(&self) -> Option<crate::preferences::CalendarAlgorithm> {
        Some(crate::preferences::CalendarAlgorithm::Hebrew)
    }
}

impl Date<Hebrew> {
    /// Construct new Hebrew Date.
    ///
    /// This date will not use any precomputed calendrical calculations,
    /// one that loads such data from a provider will be added in the future (#3933)
    ///
    ///
    /// ```rust
    /// use icu::calendar::Date;
    ///
    /// let date_hebrew = Date::try_new_hebrew(3425, 4, 25)
    ///     .expect("Failed to initialize Hebrew Date instance.");
    ///
    /// assert_eq!(date_hebrew.era_year().year, 3425);
    /// assert_eq!(date_hebrew.month().ordinal, 4);
    /// assert_eq!(date_hebrew.day_of_month().0, 25);
    /// ```
    pub fn try_new_hebrew(year: i32, month: u8, day: u8) -> Result<Date<Hebrew>, RangeError> {
        let year = HebrewYearInfo::compute(year);

        ArithmeticDate::new_from_ordinals(year, month, day)
            .map(HebrewDateInner)
            .map(|inner| Date::from_raw(inner, Hebrew))
    }
}

#[cfg(test)]
mod tests {

    use super::*;
    use crate::types::MonthCode;
    use calendrical_calculations::hebrew_keviyah::*;

    // Sentinel value for Adar I
    // We're using normalized month values here so that we can use constants. These do not
    // distinguish between the different Adars. We add an out-of-range sentinel value of 13 to
    // specifically talk about Adar I in a leap year
    const ADARI: u8 = 13;

    /// The leap years used in the tests below
    const LEAP_YEARS_IN_TESTS: [i32; 1] = [5782];
    /// (iso, hebrew) pairs of testcases. If any of the years here
    /// are leap years please add them to LEAP_YEARS_IN_TESTS (we have this manually
    /// so we don't end up exercising potentially buggy codepaths to test this)
    #[allow(clippy::type_complexity)]
    const ISO_HEBREW_DATE_PAIRS: [((i32, u8, u8), (i32, u8, u8)); 48] = [
        ((2021, 1, 10), (5781, TEVET, 26)),
        ((2021, 1, 25), (5781, SHEVAT, 12)),
        ((2021, 2, 10), (5781, SHEVAT, 28)),
        ((2021, 2, 25), (5781, ADAR, 13)),
        ((2021, 3, 10), (5781, ADAR, 26)),
        ((2021, 3, 25), (5781, NISAN, 12)),
        ((2021, 4, 10), (5781, NISAN, 28)),
        ((2021, 4, 25), (5781, IYYAR, 13)),
        ((2021, 5, 10), (5781, IYYAR, 28)),
        ((2021, 5, 25), (5781, SIVAN, 14)),
        ((2021, 6, 10), (5781, SIVAN, 30)),
        ((2021, 6, 25), (5781, TAMMUZ, 15)),
        ((2021, 7, 10), (5781, AV, 1)),
        ((2021, 7, 25), (5781, AV, 16)),
        ((2021, 8, 10), (5781, ELUL, 2)),
        ((2021, 8, 25), (5781, ELUL, 17)),
        ((2021, 9, 10), (5782, TISHREI, 4)),
        ((2021, 9, 25), (5782, TISHREI, 19)),
        ((2021, 10, 10), (5782, ḤESHVAN, 4)),
        ((2021, 10, 25), (5782, ḤESHVAN, 19)),
        ((2021, 11, 10), (5782, KISLEV, 6)),
        ((2021, 11, 25), (5782, KISLEV, 21)),
        ((2021, 12, 10), (5782, TEVET, 6)),
        ((2021, 12, 25), (5782, TEVET, 21)),
        ((2022, 1, 10), (5782, SHEVAT, 8)),
        ((2022, 1, 25), (5782, SHEVAT, 23)),
        ((2022, 2, 10), (5782, ADARI, 9)),
        ((2022, 2, 25), (5782, ADARI, 24)),
        ((2022, 3, 10), (5782, ADAR, 7)),
        ((2022, 3, 25), (5782, ADAR, 22)),
        ((2022, 4, 10), (5782, NISAN, 9)),
        ((2022, 4, 25), (5782, NISAN, 24)),
        ((2022, 5, 10), (5782, IYYAR, 9)),
        ((2022, 5, 25), (5782, IYYAR, 24)),
        ((2022, 6, 10), (5782, SIVAN, 11)),
        ((2022, 6, 25), (5782, SIVAN, 26)),
        ((2022, 7, 10), (5782, TAMMUZ, 11)),
        ((2022, 7, 25), (5782, TAMMUZ, 26)),
        ((2022, 8, 10), (5782, AV, 13)),
        ((2022, 8, 25), (5782, AV, 28)),
        ((2022, 9, 10), (5782, ELUL, 14)),
        ((2022, 9, 25), (5782, ELUL, 29)),
        ((2022, 10, 10), (5783, TISHREI, 15)),
        ((2022, 10, 25), (5783, TISHREI, 30)),
        ((2022, 11, 10), (5783, ḤESHVAN, 16)),
        ((2022, 11, 25), (5783, KISLEV, 1)),
        ((2022, 12, 10), (5783, KISLEV, 16)),
        ((2022, 12, 25), (5783, TEVET, 1)),
    ];

    #[test]
    fn test_conversions() {
        for ((iso_y, iso_m, iso_d), (y, m, d)) in ISO_HEBREW_DATE_PAIRS.into_iter() {
            let iso_date = Date::try_new_iso(iso_y, iso_m, iso_d).unwrap();
            let month_code = if m == ADARI {
                MonthCode(tinystr!(4, "M05L"))
            } else {
                MonthCode::new_normal(m).unwrap()
            };
            let hebrew_date = Date::try_new_from_codes(Some("am"), y, month_code, d, Hebrew)
                .expect("Date should parse");

            let iso_to_hebrew = iso_date.to_calendar(Hebrew);

            let hebrew_to_iso = hebrew_date.to_calendar(Iso);

            assert_eq!(
                hebrew_to_iso, iso_date,
                "Failed comparing to-ISO value for {hebrew_date:?} => {iso_date:?}"
            );
            assert_eq!(
                iso_to_hebrew, hebrew_date,
                "Failed comparing to-hebrew value for {iso_date:?} => {hebrew_date:?}"
            );

            let ordinal_month = if LEAP_YEARS_IN_TESTS.contains(&y) {
                if m == ADARI {
                    ADAR
                } else if m >= ADAR {
                    m + 1
                } else {
                    m
                }
            } else {
                assert!(m != ADARI);
                m
            };

            let ordinal_hebrew_date = Date::try_new_hebrew(y, ordinal_month, d)
                .expect("Construction of date must succeed");

            assert_eq!(ordinal_hebrew_date, hebrew_date, "Hebrew date construction from codes and ordinals should work the same for {hebrew_date:?}");
        }
    }

    #[test]
    fn test_icu_bug_22441() {
        let yi = YearInfo::compute_for(88369);
        assert_eq!(yi.keviyah.year_length(), 383);
    }

    #[test]
    fn test_negative_era_years() {
        let greg_date = Date::try_new_gregorian(-5000, 1, 1).unwrap();
        let greg_year = greg_date.era_year();
        assert_eq!(greg_date.inner.0 .0.year, -5000);
        assert_eq!(greg_year.era, "bce");
        // In Gregorian, era year is 1 - extended year
        assert_eq!(greg_year.year, 5001);
        let hebr_date = greg_date.to_calendar(Hebrew);
        let hebr_year = hebr_date.era_year();
        assert_eq!(hebr_date.inner.0.year.value, -1240);
        assert_eq!(hebr_year.era, "am");
        // In Hebrew, there is no inverse era, so negative extended years are negative era years
        assert_eq!(hebr_year.year, -1240);
    }

    #[test]
    fn test_weekdays() {
        // https://github.com/unicode-org/icu4x/issues/4893
        let cal = Hebrew::new();
        let era = "am";
        let month_code = MonthCode(tinystr!(4, "M01"));
        let dt = Date::try_new_from_codes(Some(era), 3760, month_code, 1, cal).unwrap();

        // Should be Saturday per:
        // https://www.hebcal.com/converter?hd=1&hm=Tishrei&hy=3760&h2g=1
        assert_eq!(6, dt.day_of_week() as usize);
    }
}
