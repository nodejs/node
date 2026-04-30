// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! This module contains types and implementations for the Ethiopian calendar.
//!
//! ```rust
//! use icu::calendar::{cal::Ethiopian, Date};
//!
//! let date_iso = Date::try_new_iso(1970, 1, 2)
//!     .expect("Failed to initialize ISO Date instance.");
//! let date_ethiopian = Date::new_from_iso(date_iso, Ethiopian::new());
//!
//! assert_eq!(date_ethiopian.era_year().year, 1962);
//! assert_eq!(date_ethiopian.month().ordinal, 4);
//! assert_eq!(date_ethiopian.day_of_month().0, 24);
//! ```

use crate::cal::iso::{Iso, IsoDateInner};
use crate::calendar_arithmetic::{ArithmeticDate, CalendarArithmetic};
use crate::error::{year_check, DateError};
use crate::{types, Calendar, Date, DateDuration, DateDurationUnit, RangeError};
use calendrical_calculations::helpers::I32CastError;
use calendrical_calculations::rata_die::RataDie;
use tinystr::tinystr;

/// The number of years the Amete Alem epoch precedes the Amete Mihret epoch
const INCARNATION_OFFSET: i32 = 5500;

/// Which era style the ethiopian calendar uses
#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
#[non_exhaustive]
pub enum EthiopianEraStyle {
    /// Use the Anno Mundi era, anchored at the date of Creation, followed by the
    /// Incarnation era, anchored at the date of the Incarnation of Jesus
    AmeteMihret,
    /// Use the single Anno Mundi era, anchored at the date of Creation
    AmeteAlem,
}

/// The [Ethiopian Calendar]
///
/// The [Ethiopian calendar] is a solar calendar used by the Coptic Orthodox Church, with twelve normal months
/// and a thirteenth small epagomenal month.
///
/// This type can be used with [`Date`] to represent dates in this calendar.
///
/// It can be constructed in two modes: using the Amete Alem era scheme, or the Amete Mihret era scheme (the default),
/// see [`EthiopianEraStyle`] for more info.
///
/// [Ethiopian calendar]: https://en.wikipedia.org/wiki/Ethiopian_calendar
///
/// # Era codes
///
/// This calendar always uses the `aa` era, where 1 Amete Alem is 5493 BCE. Dates before this era use negative years.
/// Dates before that use negative year numbers.
/// In the Amete Mihret scheme it uses the additional `am` era, 1 Amete Mihret is 9 CE.
///
/// # Month codes
///
/// This calendar supports 13 solar month codes (`"M01" - "M13"`), with `"M13"` being used for the short epagomenal month
/// at the end of the year.
// The bool specifies whether dates should be in the Amete Alem era scheme
#[derive(Copy, Clone, Debug, Hash, Default, Eq, PartialEq, PartialOrd, Ord)]
pub struct Ethiopian(pub(crate) bool);

/// The inner date type used for representing [`Date`]s of [`Ethiopian`]. See [`Date`] and [`Ethiopian`] for more details.
#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord)]
pub struct EthiopianDateInner(ArithmeticDate<Ethiopian>);

impl CalendarArithmetic for Ethiopian {
    type YearInfo = i32;

    fn days_in_provided_month(year: i32, month: u8) -> u8 {
        if (1..=12).contains(&month) {
            30
        } else if month == 13 {
            if Self::provided_year_is_leap(year) {
                6
            } else {
                5
            }
        } else {
            0
        }
    }

    fn months_in_provided_year(_: i32) -> u8 {
        13
    }

    fn provided_year_is_leap(year: i32) -> bool {
        year.rem_euclid(4) == 3
    }

    fn last_month_day_in_provided_year(year: i32) -> (u8, u8) {
        if Self::provided_year_is_leap(year) {
            (13, 6)
        } else {
            (13, 5)
        }
    }

    fn days_in_provided_year(year: i32) -> u16 {
        if Self::provided_year_is_leap(year) {
            366
        } else {
            365
        }
    }
}

impl crate::cal::scaffold::UnstableSealed for Ethiopian {}
impl Calendar for Ethiopian {
    type DateInner = EthiopianDateInner;
    type Year = types::EraYear;
    fn from_codes(
        &self,
        era: Option<&str>,
        year: i32,
        month_code: types::MonthCode,
        day: u8,
    ) -> Result<Self::DateInner, DateError> {
        let year = match (self.era_style(), era) {
            (EthiopianEraStyle::AmeteMihret, Some("am") | None) => {
                year_check(year, 1..)? + INCARNATION_OFFSET
            }
            (EthiopianEraStyle::AmeteMihret, Some("aa")) => {
                year_check(year, ..=INCARNATION_OFFSET)?
            }
            (EthiopianEraStyle::AmeteAlem, Some("aa") | None) => year,
            (_, Some(_)) => {
                return Err(DateError::UnknownEra);
            }
        };
        ArithmeticDate::new_from_codes(self, year, month_code, day).map(EthiopianDateInner)
    }

    fn from_rata_die(&self, rd: RataDie) -> Self::DateInner {
        EthiopianDateInner(
            match calendrical_calculations::ethiopian::ethiopian_from_fixed(rd) {
                Err(I32CastError::BelowMin) => ArithmeticDate::min_date(),
                Err(I32CastError::AboveMax) => ArithmeticDate::max_date(),
                Ok((year, month, day)) => ArithmeticDate::new_unchecked(
                    // calendrical calculations returns years in the Incarnation era
                    year + INCARNATION_OFFSET,
                    month,
                    day,
                ),
            },
        )
    }

    fn to_rata_die(&self, date: &Self::DateInner) -> RataDie {
        // calendrical calculations expects years in the Incarnation era
        calendrical_calculations::ethiopian::fixed_from_ethiopian(
            date.0.year - INCARNATION_OFFSET,
            date.0.month,
            date.0.day,
        )
    }

    fn from_iso(&self, iso: IsoDateInner) -> EthiopianDateInner {
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
        date.0.offset_date(offset, &());
    }

    #[allow(clippy::field_reassign_with_default)]
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

    fn year_info(&self, date: &Self::DateInner) -> Self::Year {
        let year = date.0.year;
        if self.0 || year <= INCARNATION_OFFSET {
            types::EraYear {
                era: tinystr!(16, "aa"),
                era_index: Some(0),
                year,
                ambiguity: types::YearAmbiguity::CenturyRequired,
            }
        } else {
            types::EraYear {
                era: tinystr!(16, "am"),
                era_index: Some(1),
                year: year - INCARNATION_OFFSET,
                ambiguity: types::YearAmbiguity::CenturyRequired,
            }
        }
    }

    fn extended_year(&self, date: &Self::DateInner) -> i32 {
        let year = date.0.extended_year();
        if self.0 {
            year
        } else {
            year - INCARNATION_OFFSET
        }
    }

    fn is_in_leap_year(&self, date: &Self::DateInner) -> bool {
        Self::provided_year_is_leap(date.0.year)
    }

    fn month(&self, date: &Self::DateInner) -> types::MonthInfo {
        date.0.month()
    }

    fn day_of_month(&self, date: &Self::DateInner) -> types::DayOfMonth {
        date.0.day_of_month()
    }

    fn day_of_year(&self, date: &Self::DateInner) -> types::DayOfYear {
        date.0.day_of_year()
    }

    fn debug_name(&self) -> &'static str {
        "Ethiopian"
    }

    fn calendar_algorithm(&self) -> Option<crate::preferences::CalendarAlgorithm> {
        Some(crate::preferences::CalendarAlgorithm::Ethiopic)
    }
}

impl Ethiopian {
    /// Construct a new Ethiopian Calendar for the Amete Mihret era naming scheme
    pub const fn new() -> Self {
        Self(false)
    }
    /// Construct a new Ethiopian Calendar with a value specifying whether or not it is Amete Alem
    pub const fn new_with_era_style(era_style: EthiopianEraStyle) -> Self {
        Self(matches!(era_style, EthiopianEraStyle::AmeteAlem))
    }

    /// Returns whether this has the Amete Alem era
    pub fn era_style(&self) -> EthiopianEraStyle {
        if self.0 {
            EthiopianEraStyle::AmeteAlem
        } else {
            EthiopianEraStyle::AmeteMihret
        }
    }
}

impl Date<Ethiopian> {
    /// Construct new Ethiopian Date.
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
        mut year: i32,
        month: u8,
        day: u8,
    ) -> Result<Date<Ethiopian>, RangeError> {
        if era_style == EthiopianEraStyle::AmeteAlem {
            year -= INCARNATION_OFFSET;
        }
        ArithmeticDate::new_from_ordinals(year, month, day)
            .map(EthiopianDateInner)
            .map(|inner| Date::from_raw(inner, Ethiopian::new_with_era_style(era_style)))
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_leap_year() {
        // 11th September 2023 in gregorian is 6/13/2015 in ethiopian
        let iso_date = Date::try_new_iso(2023, 9, 11).unwrap();
        let date_ethiopian = Date::new_from_iso(iso_date, Ethiopian::new());
        assert_eq!(date_ethiopian.extended_year(), 2015);
        assert_eq!(date_ethiopian.month().ordinal, 13);
        assert_eq!(date_ethiopian.day_of_month().0, 6);
    }

    #[test]
    fn test_iso_to_ethiopian_conversion_and_back() {
        let iso_date = Date::try_new_iso(1970, 1, 2).unwrap();
        let date_ethiopian = Date::new_from_iso(iso_date, Ethiopian::new());

        assert_eq!(date_ethiopian.extended_year(), 1962);
        assert_eq!(date_ethiopian.month().ordinal, 4);
        assert_eq!(date_ethiopian.day_of_month().0, 24);

        assert_eq!(
            date_ethiopian.to_iso(),
            Date::try_new_iso(1970, 1, 2).unwrap()
        );
    }

    #[test]
    fn test_iso_to_ethiopian_aa_conversion_and_back() {
        let iso_date = Date::try_new_iso(1970, 1, 2).unwrap();
        let date_ethiopian = Date::new_from_iso(
            iso_date,
            Ethiopian::new_with_era_style(EthiopianEraStyle::AmeteAlem),
        );

        assert_eq!(date_ethiopian.extended_year(), 7462);
        assert_eq!(date_ethiopian.month().ordinal, 4);
        assert_eq!(date_ethiopian.day_of_month().0, 24);

        assert_eq!(
            date_ethiopian.to_iso(),
            Date::try_new_iso(1970, 1, 2).unwrap()
        );
    }

    #[test]
    fn test_roundtrip_negative() {
        // https://github.com/unicode-org/icu4x/issues/2254
        let iso_date = Date::try_new_iso(-1000, 3, 3).unwrap();
        let ethiopian = iso_date.to_calendar(Ethiopian::new());
        let recovered_iso = ethiopian.to_iso();
        assert_eq!(iso_date, recovered_iso);
    }

    #[test]
    fn extended_year() {
        assert_eq!(
            Date::new_from_iso(
                Date::try_new_iso(-5500 + 9, 1, 1).unwrap(),
                Ethiopian::new_with_era_style(EthiopianEraStyle::AmeteAlem)
            )
            .extended_year(),
            1
        );
        assert_eq!(
            Date::new_from_iso(
                Date::try_new_iso(9, 1, 1).unwrap(),
                Ethiopian::new_with_era_style(EthiopianEraStyle::AmeteAlem)
            )
            .extended_year(),
            5501
        );

        assert_eq!(
            Date::new_from_iso(
                Date::try_new_iso(-5500 + 9, 1, 1).unwrap(),
                Ethiopian::new_with_era_style(EthiopianEraStyle::AmeteMihret)
            )
            .extended_year(),
            -5499
        );
        assert_eq!(
            Date::new_from_iso(
                Date::try_new_iso(9, 1, 1).unwrap(),
                Ethiopian::new_with_era_style(EthiopianEraStyle::AmeteMihret)
            )
            .extended_year(),
            1
        );
    }
}
