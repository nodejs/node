// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::calendar_arithmetic::{ArithmeticDate, DateFieldsResolver, PackWithMD};
use crate::error::{
    DateAddError, DateFromFieldsError, DateNewError, EcmaReferenceYearError, LunisolarDateError,
    MonthError, UnknownEraError,
};
use crate::options::{DateAddOptions, DateDifferenceOptions};
use crate::options::{DateFromFieldsOptions, Overflow};
use crate::types::{DateFields, LeapStatus, Month, MonthInfo};
use crate::RangeError;
use crate::{types, Calendar, Date};
use ::tinystr::tinystr;
use calendrical_calculations::hebrew_keviyah::{Keviyah, YearInfo};
use calendrical_calculations::rata_die::RataDie;
use core::cmp::Ordering;

/// The [Hebrew Calendar](https://en.wikipedia.org/wiki/Hebrew_calendar)
///
/// The Hebrew calendar is a lunisolar calendar used as the Jewish liturgical calendar
/// as well as an official calendar in Israel.
///
/// This implementation uses civil month numbering, where Tishrei is the first month of the year.
///
/// The precise algorithm used to calculate the Hebrew Calendar has [changed over time], with
/// the modern one being in place since about 4536 AM (776 CE). This implementation extends
/// proleptically for dates before that.
///
/// [changed over time]: https://hakirah.org/vol20AjdlerAppendices.pdf
///
/// This corresponds to the `"hebrew"` [CLDR calendar](https://unicode.org/reports/tr35/#UnicodeCalendarIdentifier).
///
/// # Era codes
///
/// This calendar uses a single era code `am`, Anno Mundi. Dates before this era use negative years.
///
/// # Months and days
///
/// The 12 months are called Tishrei (`M01`, 30 days), Ḥešvan (`M02`, 29/30 days),
/// Kīslev (`M03`, 30/29 days), Ṭevet (`M04`, 29 days), Šəvaṭ (`M05`, 30 days), ʾĂdār (`M06`, 29 days),
/// Nīsān (`M07`, 30 days), ʾĪyyar (`M08`, 29 days), Sivan (`M09`, 30 days), Tammūz (`M10`, 29 days),
/// ʾAv (`M11`, 30 days), ʾElūl (`M12`, 29 days).
///
/// Due to Rosh Hashanah postponement rules, Ḥešvan and Kislev vary in length.
///
/// In leap years (years 3, 6, 8, 11, 17, 19 in a 19-year cycle), the leap month Adar I (`M05L`, 30 days)
/// is inserted before Adar (`M06`), which is then called Adar II ([`MonthInfo::leap_status`] will be
/// [`LeapStatus::Base`] to mark this).
///
/// Standard years thus have 353-355 days, and leap years 383-385.
#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord, Default)]
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

#[derive(Copy, Clone, Debug)]
pub(crate) struct HebrewYear {
    keviyah: Keviyah,
    /// The Hebrew extended year
    value: i32,
}

impl PartialEq for HebrewYear {
    fn eq(&self, other: &Self) -> bool {
        self.value == other.value
    }
}
impl Eq for HebrewYear {}
impl core::hash::Hash for HebrewYear {
    fn hash<H: core::hash::Hasher>(&self, state: &mut H) {
        self.value.hash(state);
    }
}
impl PartialOrd for HebrewYear {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}
impl Ord for HebrewYear {
    fn cmp(&self, other: &Self) -> Ordering {
        self.value.cmp(&other.value)
    }
}

impl core::ops::Sub<HebrewYear> for HebrewYear {
    type Output = i32;
    #[inline]
    fn sub(self, rhs: HebrewYear) -> Self::Output {
        self.value - rhs.value
    }
}

impl PackWithMD for HebrewYear {
    /// The first byte is the [`Keviyah`], the remaining four the YMD as encoded by [`i32::pack`].
    type Packed = [u8; 5];

    fn pack(self, month: u8, day: u8) -> Self::Packed {
        let a = self.keviyah as u8;
        let [b, c, d, e] = self.value.pack(month, day);
        [a, b, c, d, e]
    }

    fn unpack_year([a, b, c, d, e]: Self::Packed) -> Self {
        let value = i32::unpack_year([b, c, d, e]);
        let keviyah = Keviyah::from_integer(a);
        Self { keviyah, value }
    }

    fn unpack_month([_, b, c, d, e]: Self::Packed) -> u8 {
        i32::unpack_month([b, c, d, e])
    }

    fn unpack_day([_, b, c, d, e]: Self::Packed) -> u8 {
        i32::unpack_day([b, c, d, e])
    }
}

impl HebrewYear {
    /// Convenience method to compute for a given year. Don't use this if you actually need
    /// a [`YearInfo`] that you want to call `.new_year()` on.
    fn compute(value: i32) -> Self {
        Self {
            keviyah: YearInfo::compute_for(value).keviyah,
            value,
        }
    }

    fn for_rd(rd: RataDie) -> Self {
        let (year, value) = YearInfo::year_containing_rd(rd);
        Self {
            keviyah: year.keviyah,
            value,
        }
    }

    fn new_year(self) -> RataDie {
        self.keviyah.year_info(self.value).new_year()
    }
}

impl DateFieldsResolver for Hebrew {
    type YearInfo = HebrewYear;
    fn days_in_provided_month(year: HebrewYear, ordinal_month: u8) -> u8 {
        year.keviyah.month_len(ordinal_month)
    }

    fn months_in_provided_year(year: HebrewYear) -> u8 {
        12 + year.keviyah.is_leap() as u8
    }

    #[inline]
    fn min_months_from(_start: HebrewYear, years: i32) -> i32 {
        // The Hebrew Metonic cycle is 7 leap years every 19 years,
        // which comes out to 235 months per 19 years.
        //
        // We need to ensure that this is always *lower or equal to* the number of
        // months in a given year span.
        //
        // Firstly, note that this math will produce exactly the number of months in any given period
        // that spans a whole number of cycles. Note that we are only performing integer
        // ops here, and our SAFE_YEAR_RANGE is well within the range of allowed values
        // for multiplying by 235.
        //
        // So we only need to verify that this math produces the right results within a single cycle.
        //
        // The Hebrew Metonic cycle has leap years in year 3, 6, 8, 11, 14, 17, and 19 (starting counting at year 1),
        // i.e., leap year gaps of +3, +3, +2, +3, +3, +3, +2.
        //
        // 235 / 19 is ≈12 7/19 months per year, which leads to one leap month every three years plus 2/19
        // months left over. So this is correct as long as it does not predict a leap month in 2 years
        // where the Hebrew calendar expects one in 3.
        //
        // The longest sequence of "three year leap months" in the Hebrew calendar
        // is 4: year 8->11->14->17. In that time the error will accumulate to 6/19, which is not
        // enough to create a "two year leap month" in our calculation. So this calculation cannot go past
        // the actual cycle of the Hebrew calendar.
        235 * years / 19
    }

    #[inline]
    fn extended_year_from_era_year_unchecked(
        &self,
        era: &[u8],
        era_year: i32,
    ) -> Result<i32, UnknownEraError> {
        match era {
            b"am" => Ok(era_year),
            _ => Err(UnknownEraError),
        }
    }
    #[inline]
    fn year_info_from_extended(&self, extended_year: i32) -> Self::YearInfo {
        HebrewYear::compute(extended_year)
    }

    #[inline]
    fn extended_from_year_info(&self, year_info: Self::YearInfo) -> i32 {
        year_info.value
    }

    fn reference_year_from_month_day(
        &self,
        month: Month,
        day: u8,
    ) -> Result<Self::YearInfo, EcmaReferenceYearError> {
        // December 31, 1972 occurs on 4th month, 26th day, 5733 AM
        let hebrew_year = match (month.number(), month.is_leap()) {
            (1, false) => 5733,
            (2, false) => match day {
                // There is no day 30 in 5733 (there is in 5732)
                ..=29 => 5733,
                // Note (here and below): this must be > 29, not just == 30,
                // since we have not yet applied a potential Overflow::Constrain.
                _ => 5732,
            },
            (3, false) => match day {
                // There is no day 30 in 5733 (there is in 5732)
                ..=29 => 5733,
                _ => 5732,
            },
            (4, false) => match day {
                ..=26 => 5733,
                _ => 5732,
            },
            (5..=12, false) => 5732,
            // Neither 5731 nor 5732 is a leap year
            (5, true) => 5730,
            _ => {
                return Err(EcmaReferenceYearError::MonthNotInCalendar);
            }
        };
        Ok(HebrewYear::compute(hebrew_year))
    }

    fn ordinal_from_month(
        &self,
        year: Self::YearInfo,
        month: Month,
        overflow: Overflow,
    ) -> Result<u8, MonthError> {
        let is_leap_year = year.keviyah.is_leap();
        let ordinal_month = match (month.number(), month.is_leap()) {
            (n @ 1..=12, false) => n + (n >= 6 && is_leap_year) as u8,
            (5, true) => {
                if is_leap_year {
                    6
                } else {
                    // Requesting Adar 1 in non-leap year, handle constrain/reject behavior
                    match overflow {
                        Overflow::Constrain => 6,
                        Overflow::Reject => return Err(MonthError::NotInYear),
                    }
                }
            }
            _ => return Err(MonthError::NotInCalendar),
        };
        Ok(ordinal_month)
    }

    fn month_from_ordinal(&self, year: Self::YearInfo, ordinal_month: u8) -> Month {
        let is_leap = year.keviyah.is_leap();
        Month::new_unchecked(
            ordinal_month - (is_leap && ordinal_month >= 6) as u8,
            ordinal_month == 6 && is_leap,
        )
    }

    fn to_rata_die_inner(year: Self::YearInfo, month: u8, day: u8) -> RataDie {
        year.new_year() + year.keviyah.days_preceding(month) as i64 + (day - 1) as i64
    }
}

impl crate::cal::scaffold::UnstableSealed for Hebrew {}
impl Calendar for Hebrew {
    type DateInner = HebrewDateInner;
    type Year = types::EraYear;
    type DateCompatibilityError = core::convert::Infallible;

    fn new_date(
        &self,
        year: types::YearInput,
        month: Month,
        day: u8,
    ) -> Result<Self::DateInner, DateNewError> {
        ArithmeticDate::from_input_year_month_code_day(year, month, day, self).map(HebrewDateInner)
    }

    fn from_fields(
        &self,
        fields: DateFields,
        options: DateFromFieldsOptions,
    ) -> Result<Self::DateInner, DateFromFieldsError> {
        ArithmeticDate::from_fields(fields, options, self).map(HebrewDateInner)
    }

    fn from_rata_die(&self, rd: RataDie) -> Self::DateInner {
        let year = HebrewYear::for_rd(rd);

        // Clamp the RD to our year
        let rd = rd.clamp(
            year.new_year(),
            year.new_year() + year.keviyah.year_length() as i64,
        );

        let (month, day) = year
            .keviyah
            .month_day_for((rd - year.new_year()) as u16 + 1);

        // date is in the valid RD range
        HebrewDateInner(ArithmeticDate::new_unchecked(year, month, day))
    }

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
        date.0.year().keviyah.year_length()
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
        date.0.added(duration, self, options).map(HebrewDateInner)
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

    fn debug_name(&self) -> &'static str {
        "Hebrew"
    }

    fn year_info(&self, date: &Self::DateInner) -> Self::Year {
        let extended_year = date.0.year().value;
        types::EraYear {
            era_index: Some(0),
            era: tinystr!(16, "am"),
            year: extended_year,
            extended_year,
            ambiguity: types::YearAmbiguity::CenturyRequired,
        }
    }

    fn is_in_leap_year(&self, date: &Self::DateInner) -> bool {
        date.0.year().keviyah.is_leap()
    }

    fn month(&self, date: &Self::DateInner) -> MonthInfo {
        let mut m = MonthInfo::new(self, date.0);
        // Even though the leap month is modeled as M05L,
        // the actual leap base is M06.
        if m.number() == 6 && m.ordinal == 7 {
            m.leap_status = LeapStatus::Base;
            #[allow(deprecated)]
            {
                // This is an ICU4X invention, it's not needed by
                // formatting anymore, but we keep producing it
                // for now.
                m.formatting_code = Month::leap(6).code();
            }
        }
        m
    }

    fn day_of_month(&self, date: &Self::DateInner) -> types::DayOfMonth {
        types::DayOfMonth(date.0.day())
    }

    fn day_of_year(&self, date: &Self::DateInner) -> types::DayOfYear {
        types::DayOfYear(date.0.year().keviyah.days_preceding(date.0.month()) + date.0.day() as u16)
    }

    fn calendar_algorithm(&self) -> Option<crate::preferences::CalendarAlgorithm> {
        Some(crate::preferences::CalendarAlgorithm::Hebrew)
    }
}

impl Date<Hebrew> {
    /// Construct a new Hebrew [`Date`].
    ///
    /// Years are arithmetic, meaning there is a year 0 preceded by negative years, with a
    /// valid range of `-9999..=9999`.
    ///
    /// ```rust
    /// use icu::calendar::types::Month;
    /// use icu::calendar::Date;
    ///
    /// let date = Date::try_new_hebrew_v2(5782, Month::leap(5), 7)
    ///     .expect("Failed to initialize Date instance.");
    ///
    /// assert_eq!(date.era_year().year, 5782);
    /// // Adar I
    /// assert_eq!(date.month().number(), 5);
    /// assert!(date.month().to_input().is_leap());
    /// assert_eq!(date.day_of_month().0, 7);
    /// ```
    pub fn try_new_hebrew_v2(
        year: i32,
        month: Month,
        day: u8,
    ) -> Result<Date<Hebrew>, LunisolarDateError> {
        ArithmeticDate::try_from_ymd_lunisolar(year, month, day, &Hebrew)
            .map(HebrewDateInner)
            .map(|inner| Date::from_raw(inner, Hebrew))
    }

    /// This method uses an ordinal month, which is probably not what you want.
    ///
    /// Use [`Date::try_new_hebrew_v2`]
    #[deprecated(since = "2.1.0", note = "use `Date::try_new_hebrew_v2`")]
    pub fn try_new_hebrew(
        year: i32,
        ordinal_month: u8,
        day: u8,
    ) -> Result<Date<Hebrew>, RangeError> {
        ArithmeticDate::from_year_month_day(year, ordinal_month, day, &Hebrew)
            .map(HebrewDateInner)
            .map(|inner| Date::from_raw(inner, Hebrew))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::types::Weekday;

    pub const TISHREI: Month = Month::new(1);
    pub const ḤESHVAN: Month = Month::new(2);
    pub const KISLEV: Month = Month::new(3);
    pub const TEVET: Month = Month::new(4);
    pub const SHEVAT: Month = Month::new(5);
    pub const ADARI: Month = Month::leap(5);
    pub const ADAR: Month = Month::new(6);
    pub const NISAN: Month = Month::new(7);
    pub const IYYAR: Month = Month::new(8);
    pub const SIVAN: Month = Month::new(9);
    pub const TAMMUZ: Month = Month::new(10);
    pub const AV: Month = Month::new(11);
    pub const ELUL: Month = Month::new(12);

    const LEAP_YEARS_IN_TESTS: [i32; 1] = [5782];
    // If any of the years here are leap years, add them to
    // [`LEAP_YEARS_IN_TESTS`] (we have this manually so we don't
    // end up exercising potentially buggy codepaths to test this)
    #[expect(clippy::type_complexity)]
    const ISO_HEBREW_DATE_PAIRS: [((i32, u8, u8), (i32, Month, u8)); 48] = [
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
            let rd = Date::try_new_iso(iso_y, iso_m, iso_d)
                .unwrap()
                .to_rata_die();

            let date = Date::from_rata_die(rd, Hebrew);

            assert_eq!(date.to_rata_die(), rd, "{date:?}");

            assert_eq!(date.era_year().year, y, "{date:?}");
            assert_eq!(
                date.month().ordinal,
                if (m == ADARI || m.number() >= ADAR.number()) && LEAP_YEARS_IN_TESTS.contains(&y) {
                    m.number() + 1
                } else {
                    assert!(m != ADARI);
                    m.number()
                },
                "{date:?}"
            );
            assert_eq!(date.day_of_month().0, d, "{date:?}");

            assert_eq!(
                Date::try_new(
                    types::YearInput::EraYear(&date.era_year().era, date.era_year().year),
                    date.month().to_input(),
                    date.day_of_month().0,
                    Hebrew
                ),
                Ok(date)
            );

            assert_eq!(
                Date::try_new_hebrew_v2(
                    date.era_year().year,
                    date.month().to_input(),
                    date.day_of_month().0,
                ),
                Ok(date)
            );

            #[allow(deprecated)] // should still test
            {
                assert_eq!(
                    Date::try_new_hebrew(
                        date.era_year().extended_year,
                        date.month().ordinal,
                        date.day_of_month().0
                    ),
                    Ok(date)
                );
            }
        }
    }

    #[test]
    fn test_icu_bug_22441() {
        let yi = HebrewYear::compute(88369);
        assert_eq!(yi.keviyah.year_length(), 383);
    }

    #[test]
    fn test_negative_era_years() {
        let greg_date = Date::try_new_gregorian(-5000, 1, 1).unwrap();
        let greg_year = greg_date.era_year();
        assert_eq!(greg_year.extended_year, -5000);
        assert_eq!(greg_year.era, "bce");
        // In Gregorian, era year is 1 - extended year
        assert_eq!(greg_year.year, 5001);
        let hebr_date = greg_date.to_calendar(Hebrew);
        let hebr_year = hebr_date.era_year();
        assert_eq!(hebr_year.extended_year, -1240);
        assert_eq!(hebr_year.era, "am");
        // In Hebrew, there is no inverse era, so negative extended years are negative era years
        assert_eq!(hebr_year.year, -1240);
    }

    #[test]
    fn test_weekdays() {
        // https://github.com/unicode-org/icu4x/issues/4893
        let dt = Date::try_new_hebrew_v2(3760, Month::new(1), 1).unwrap();

        // Should be Saturday per:
        // https://www.hebcal.com/converter?hd=1&hm=Tishrei&hy=3760&h2g=1
        assert_eq!(dt.weekday(), Weekday::Saturday);
    }
}
