// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::cal::coptic::{CopticDateInner, CopticYear};
use crate::cal::Coptic;
use crate::calendar_arithmetic::{ArithmeticDate, DateFieldsResolver};
use crate::error::{
    DateAddError, DateFromFieldsError, DateNewError, EcmaReferenceYearError, UnknownEraError,
};
use crate::options::DateFromFieldsOptions;
use crate::options::{DateAddOptions, DateDifferenceOptions};
use crate::types::DateFields;
use crate::{types, Calendar, Date, RangeError};
use calendrical_calculations::rata_die::RataDie;
use tinystr::tinystr;

/// The Coptic year of the Amete Mihret epoch
const AMETE_MIHRET_OFFSET: i32 = -276;

/// The Coptic year of the Amete Alem epoch
const AMETE_ALEM_OFFSET: i32 = -5776;

/// Which era style the ethiopian calendar uses
#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug, PartialOrd, Ord)]
#[non_exhaustive]
pub enum EthiopianEraStyle {
    /// Use the Anno Mundi era, anchored at the date of Creation, followed by the
    /// Incarnation era, anchored at the date of the Incarnation of Jesus
    AmeteMihret,
    /// Use the single Anno Mundi era, anchored at the date of Creation
    AmeteAlem,
}

impl EthiopianEraStyle {
    pub(crate) fn coptic_year_offset(self) -> i32 {
        match self {
            Self::AmeteMihret => AMETE_MIHRET_OFFSET,
            Self::AmeteAlem => AMETE_ALEM_OFFSET,
        }
    }
}

/// The [Ethiopian Calendar](https://en.wikipedia.org/wiki/Ethiopian_calendar)
///
/// The Ethiopian calendar is a variant of the [`Coptic`] calendar. It differs
/// from the Coptic calendar by the names of the months as well as the era.
///
/// This implementation can be constructed in two modes: using the Amete Alem era
/// scheme, or the Amete Mihret era scheme (the default), see [`EthiopianEraStyle`]
/// for more info.
///
/// This implementation extends proleptically for dates before the calendar's creation.
///
/// This corresponds to the `"ethiopic"` and `"ethioaa"` [CLDR calendars](https://unicode.org/reports/tr35/#UnicodeCalendarIdentifier),
/// with `"ethiopic"` being for [`EthiopianEraStyle::AmeteMihret`]
///
/// # Era codes
///
/// This calendar always uses the `aa` era, where 1 Amete Alem is 5493 BCE. Dates before this era use negative years.
/// Dates before that use negative year numbers.
///
/// In the Amete Mihret scheme it uses the additional `am` era, 1 Amete Mihret is 9 CE.
///
/// # Months and days
///
/// The 13 months are called Mäskäräm (`M01`, 30 days), Ṭəqəmt (`M02`, 30 days),
/// Ḫədar (`M03`, 30 days), Taḫśaś (`M04`, 30 days), Ṭərr (`M05`, 30 days), Yäkatit (`M06`, 30 days),
/// Mägabit (`M07`, 30 days), Miyazya (`M08`, 30 days), Gənbo (`M09`, 30 days),
/// Säne (`M10`, 30 days), Ḥamle (`M11`, 30 days), Nähase (`M12`, 30 days), Ṗagʷəmen (`M13`, 5 days).
///
/// In leap years (years divisible by 4), Ṗagʷəmen gains a 6th day.
///
/// Standard years thus have 365 days, and leap years 366.
///
/// # Calendar drift
///
/// The Ethiopian calendar has the same year lengths and leap year rules as the [`Coptic`] and
/// [`Julian`](crate::cal::Julian) calendars, so it experiences the same drift of 1 day in ~128
/// years with respect to the seasons.
#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord)]
pub struct Ethiopian(EthiopianEraStyle);

impl Default for Ethiopian {
    fn default() -> Self {
        Self(EthiopianEraStyle::AmeteMihret)
    }
}

#[allow(missing_docs)] // not actually public
#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord)]
pub struct EthiopianDateInner(CopticDateInner);

impl DateFieldsResolver for Ethiopian {
    // Coptic year
    type YearInfo = CopticYear;

    fn days_in_provided_month(year: Self::YearInfo, month: u8) -> u8 {
        Coptic::days_in_provided_month(year, month)
    }

    fn months_in_provided_year(year: Self::YearInfo) -> u8 {
        Coptic::months_in_provided_year(year)
    }

    #[inline]
    fn min_months_from(start: Self::YearInfo, years: i32) -> i32 {
        Coptic::min_months_from(start, years)
    }

    #[inline]
    fn extended_year_from_era_year_unchecked(
        &self,
        era: &[u8],
        era_year: i32,
    ) -> Result<i32, UnknownEraError> {
        match (self.era_style(), era) {
            (EthiopianEraStyle::AmeteMihret, b"am") => Ok(era_year),
            (EthiopianEraStyle::AmeteMihret, b"aa") => {
                Ok(era_year - AMETE_MIHRET_OFFSET + AMETE_ALEM_OFFSET)
            }
            (EthiopianEraStyle::AmeteAlem, b"aa") => Ok(era_year),
            (_, _) => Err(UnknownEraError),
        }
    }

    #[inline]
    fn year_info_from_extended(&self, extended_year: i32) -> Self::YearInfo {
        CopticYear::from_ethiopian_year(extended_year, self.0)
    }

    #[inline]
    fn extended_from_year_info(&self, year_info: Self::YearInfo) -> i32 {
        year_info.to_ethiopian_year(self.0)
    }

    #[inline]
    fn reference_year_from_month_day(
        &self,
        month: types::Month,
        day: u8,
    ) -> Result<Self::YearInfo, EcmaReferenceYearError> {
        Coptic::reference_year_from_month_day(month, day)
    }

    fn to_rata_die_inner(year: Self::YearInfo, month: u8, day: u8) -> RataDie {
        Coptic::to_rata_die_inner(year, month, day)
    }
}

impl crate::cal::scaffold::UnstableSealed for Ethiopian {}
impl Calendar for Ethiopian {
    type DateInner = EthiopianDateInner;
    type Year = <Coptic as Calendar>::Year;
    type DateCompatibilityError = <Coptic as Calendar>::DateCompatibilityError;

    fn new_date(
        &self,
        year: types::YearInput,
        month: types::Month,
        day: u8,
    ) -> Result<Self::DateInner, DateNewError> {
        ArithmeticDate::from_input_year_month_code_day(year, month, day, self)
            .map(ArithmeticDate::cast)
            .map(CopticDateInner)
            .map(EthiopianDateInner)
    }

    fn from_fields(
        &self,
        fields: DateFields,
        options: DateFromFieldsOptions,
    ) -> Result<Self::DateInner, DateFromFieldsError> {
        ArithmeticDate::from_fields(fields, options, self)
            .map(ArithmeticDate::cast)
            .map(CopticDateInner)
            .map(EthiopianDateInner)
    }

    fn from_rata_die(&self, rd: RataDie) -> Self::DateInner {
        EthiopianDateInner(Coptic.from_rata_die(rd))
    }

    fn to_rata_die(&self, date: &Self::DateInner) -> RataDie {
        date.0 .0.to_rata_die()
    }

    fn has_cheap_iso_conversion(&self) -> bool {
        false
    }

    fn months_in_year(&self, date: &Self::DateInner) -> u8 {
        Coptic.months_in_year(&date.0)
    }

    fn days_in_year(&self, date: &Self::DateInner) -> u16 {
        Coptic.days_in_year(&date.0)
    }

    fn days_in_month(&self, date: &Self::DateInner) -> u8 {
        Coptic.days_in_month(&date.0)
    }

    fn add(
        &self,
        date: &Self::DateInner,
        duration: types::DateDuration,
        options: DateAddOptions,
    ) -> Result<Self::DateInner, DateAddError> {
        Coptic
            .add(&date.0, duration, options)
            .map(EthiopianDateInner)
    }

    fn until(
        &self,
        date1: &Self::DateInner,
        date2: &Self::DateInner,
        options: DateDifferenceOptions,
    ) -> types::DateDuration {
        Coptic.until(&date1.0, &date2.0, options)
    }

    fn check_date_compatibility(&self, other: &Self) -> Result<(), Self::DateCompatibilityError> {
        let _does_not_affect_semantics = other.era_style();
        Ok(())
    }

    fn year_info(&self, date: &Self::DateInner) -> Self::Year {
        let coptic_year = date.0 .0.year();
        let extended_year = coptic_year.to_ethiopian_year(self.0);

        if self.0 == EthiopianEraStyle::AmeteAlem || extended_year <= 0 {
            types::EraYear {
                era: tinystr!(16, "aa"),
                era_index: Some(0),
                year: coptic_year.to_ethiopian_year(EthiopianEraStyle::AmeteAlem),
                extended_year,
                ambiguity: types::YearAmbiguity::CenturyRequired,
            }
        } else {
            types::EraYear {
                era: tinystr!(16, "am"),
                era_index: Some(1),
                year: coptic_year.to_ethiopian_year(EthiopianEraStyle::AmeteMihret),
                extended_year,
                ambiguity: types::YearAmbiguity::CenturyRequired,
            }
        }
    }

    fn is_in_leap_year(&self, date: &Self::DateInner) -> bool {
        Coptic.is_in_leap_year(&date.0)
    }

    fn month(&self, date: &Self::DateInner) -> types::MonthInfo {
        Coptic.month(&date.0)
    }

    fn day_of_month(&self, date: &Self::DateInner) -> types::DayOfMonth {
        Coptic.day_of_month(&date.0)
    }

    fn day_of_year(&self, date: &Self::DateInner) -> types::DayOfYear {
        Coptic.day_of_year(&date.0)
    }

    fn debug_name(&self) -> &'static str {
        match self.0 {
            EthiopianEraStyle::AmeteMihret => "Ethiopian",
            EthiopianEraStyle::AmeteAlem => "Ethiopian (Amete Alem)",
        }
    }

    fn calendar_algorithm(&self) -> Option<crate::preferences::CalendarAlgorithm> {
        match self.0 {
            EthiopianEraStyle::AmeteMihret => Some(crate::preferences::CalendarAlgorithm::Ethiopic),
            EthiopianEraStyle::AmeteAlem => Some(crate::preferences::CalendarAlgorithm::Ethioaa),
        }
    }
}

impl Ethiopian {
    /// Construct a new Ethiopian Calendar for the Amete Mihret era naming scheme
    pub const fn new() -> Self {
        Self(EthiopianEraStyle::AmeteMihret)
    }

    /// Construct a new Ethiopian Calendar with an explicit [`EthiopianEraStyle`].
    pub const fn new_with_era_style(era_style: EthiopianEraStyle) -> Self {
        Self(era_style)
    }

    /// Returns the [`EthiopianEraStyle`] used by this calendar.
    pub fn era_style(&self) -> EthiopianEraStyle {
        self.0
    }
}

impl Date<Ethiopian> {
    /// Construct new Ethiopian [`Date`].
    ///
    /// Years are arithmetic, meaning there is a year 0 preceded by negative years, with a
    /// valid range of `-9999..=9999`.
    ///
    /// Years are interpreted according to the provided `era_style`.
    ///
    /// ```rust
    /// use icu::calendar::cal::EthiopianEraStyle;
    /// use icu::calendar::Date;
    ///
    /// let date_ethiopian =
    ///     Date::try_new_ethiopian(EthiopianEraStyle::AmeteMihret, 2014, 8, 25)
    ///         .expect("Failed to initialize Ethopic Date instance.");
    ///
    /// assert_eq!(date_ethiopian.era_year().year, 2014);
    /// assert_eq!(date_ethiopian.month().ordinal, 8);
    /// assert_eq!(date_ethiopian.day_of_month().0, 25);
    /// ```
    pub fn try_new_ethiopian(
        era_style: EthiopianEraStyle,
        year: i32,
        month: u8,
        day: u8,
    ) -> Result<Date<Ethiopian>, RangeError> {
        ArithmeticDate::from_year_month_day(year, month, day, &Ethiopian(era_style))
            .map(ArithmeticDate::cast)
            .map(CopticDateInner)
            .map(EthiopianDateInner)
            .map(|inner| Date::from_raw(inner, Ethiopian(era_style)))
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_leap_year() {
        // 2023-09-11 ISO is 2025-13-06 Ethiopian
        let rd = Date::try_new_iso(2023, 9, 11).unwrap().to_rata_die();
        let date_ethiopian = Date::from_rata_die(rd, Ethiopian::new());
        assert_eq!(date_ethiopian.year().extended_year(), 2015);
        assert_eq!(date_ethiopian.month().ordinal, 13);
        assert_eq!(date_ethiopian.day_of_month().0, 6);
    }

    #[test]
    fn test_ethiopian_conversion_and_back() {
        let rd = Date::try_new_iso(1970, 1, 2).unwrap().to_rata_die();
        let date_ethiopian = Date::from_rata_die(rd, Ethiopian::new());

        assert_eq!(date_ethiopian.year().extended_year(), 1962);
        assert_eq!(date_ethiopian.month().ordinal, 4);
        assert_eq!(date_ethiopian.day_of_month().0, 24);

        assert_eq!(date_ethiopian.to_rata_die(), rd);
    }

    #[test]
    fn test_ethiopian_aa_conversion_and_back() {
        let rd = Date::try_new_iso(1970, 1, 2).unwrap().to_rata_die();
        let date_ethiopian = Date::from_rata_die(rd, Ethiopian(EthiopianEraStyle::AmeteAlem));

        assert_eq!(date_ethiopian.year().extended_year(), 7462);
        assert_eq!(date_ethiopian.month().ordinal, 4);
        assert_eq!(date_ethiopian.day_of_month().0, 24);

        assert_eq!(date_ethiopian.to_rata_die(), rd);
    }

    #[test]
    fn test_roundtrip_negative() {
        // https://github.com/unicode-org/icu4x/issues/2254
        let rd = Date::try_new_iso(-1000, 3, 3).unwrap().to_rata_die();
        let date_ethiopian = Date::from_rata_die(rd, Ethiopian::new());
        assert_eq!(date_ethiopian.to_rata_die(), rd);
    }

    #[test]
    fn extended_year() {
        assert_eq!(
            Date::try_new_iso(-5500 + 9, 1, 1)
                .unwrap()
                .to_calendar(Ethiopian(EthiopianEraStyle::AmeteAlem))
                .year()
                .extended_year(),
            1
        );
        assert_eq!(
            Date::try_new_iso(9, 1, 1)
                .unwrap()
                .to_calendar(Ethiopian(EthiopianEraStyle::AmeteAlem))
                .year()
                .extended_year(),
            5501
        );

        assert_eq!(
            Date::try_new_iso(-5500 + 9, 1, 1)
                .unwrap()
                .to_calendar(Ethiopian(EthiopianEraStyle::AmeteMihret))
                .year()
                .extended_year(),
            -5499
        );
        assert_eq!(
            Date::try_new_iso(9, 1, 1)
                .unwrap()
                .to_calendar(Ethiopian(EthiopianEraStyle::AmeteMihret))
                .year()
                .extended_year(),
            1
        );
    }
}
