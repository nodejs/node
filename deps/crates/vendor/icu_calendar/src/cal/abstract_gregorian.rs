// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::cal::iso::{IsoDateInner, IsoEra};
use crate::calendar_arithmetic::{ArithmeticDate, DateFieldsResolver};
use crate::error::{DateError, DateFromFieldsError, EcmaReferenceYearError, UnknownEraError};
use crate::options::DateFromFieldsOptions;
use crate::options::{DateAddOptions, DateDifferenceOptions};
use crate::preferences::CalendarAlgorithm;
use crate::types::EraYear;
use crate::{types, Calendar, RangeError};
use calendrical_calculations::helpers::I32CastError;
use calendrical_calculations::rata_die::RataDie;

#[derive(Copy, Clone, Debug, Default, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub(crate) struct AbstractGregorian<Y: GregorianYears>(pub Y);

pub(crate) trait GregorianYears: Clone + core::fmt::Debug {
    // Positive if after 0 CE
    const EXTENDED_YEAR_OFFSET: i32 = 0;

    fn extended_from_era_year(&self, era: Option<&[u8]>, year: i32)
        -> Result<i32, UnknownEraError>;

    fn era_year_from_extended(&self, extended_year: i32, month: u8, day: u8) -> EraYear;

    fn calendar_algorithm(&self) -> Option<CalendarAlgorithm> {
        None
    }

    fn debug_name(&self) -> &'static str;
}

impl ArithmeticDate<AbstractGregorian<IsoEra>> {
    pub(crate) fn new_gregorian<Y: GregorianYears>(
        year: i32,
        month: u8,
        day: u8,
    ) -> Result<Self, RangeError> {
        ArithmeticDate::try_from_ymd(year + Y::EXTENDED_YEAR_OFFSET, month, day)
    }
}

pub(crate) const REFERENCE_YEAR: i32 = 1972;
#[cfg(test)]
pub(crate) const LAST_DAY_OF_REFERENCE_YEAR: RataDie =
    calendrical_calculations::gregorian::day_before_year(REFERENCE_YEAR + 1);

impl<Y: GregorianYears> DateFieldsResolver for AbstractGregorian<Y> {
    // Gregorian year
    type YearInfo = i32;

    fn days_in_provided_month(year: i32, month: u8) -> u8 {
        // https://www.youtube.com/watch?v=J9KijLyP-yg&t=1394s
        if month == 2 {
            28 + calendrical_calculations::gregorian::is_leap_year(year) as u8
        } else {
            30 | month ^ (month >> 3)
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
        Ok(self.0.extended_from_era_year(Some(era), era_year)? + Y::EXTENDED_YEAR_OFFSET)
    }

    #[inline]
    fn year_info_from_extended(&self, extended_year: i32) -> Self::YearInfo {
        extended_year + Y::EXTENDED_YEAR_OFFSET
    }

    #[inline]
    fn reference_year_from_month_day(
        &self,
        _month_code: types::ValidMonthCode,
        _day: u8,
    ) -> Result<Self::YearInfo, EcmaReferenceYearError> {
        Ok(REFERENCE_YEAR)
    }
}

impl<Y: GregorianYears> crate::cal::scaffold::UnstableSealed for AbstractGregorian<Y> {}

impl<Y: GregorianYears> Calendar for AbstractGregorian<Y> {
    type DateInner = ArithmeticDate<AbstractGregorian<IsoEra>>;
    type Year = types::EraYear;
    type DifferenceError = core::convert::Infallible;

    fn from_codes(
        &self,
        era: Option<&str>,
        year: i32,
        month_code: types::MonthCode,
        day: u8,
    ) -> Result<Self::DateInner, DateError> {
        ArithmeticDate::from_codes(era, year, month_code, day, self).map(ArithmeticDate::cast)
    }

    #[cfg(feature = "unstable")]
    fn from_fields(
        &self,
        fields: types::DateFields,
        options: DateFromFieldsOptions,
    ) -> Result<Self::DateInner, DateFromFieldsError> {
        ArithmeticDate::from_fields(fields, options, self).map(ArithmeticDate::cast)
    }

    fn from_rata_die(&self, date: RataDie) -> Self::DateInner {
        let iso = match calendrical_calculations::gregorian::gregorian_from_fixed(date) {
            Err(I32CastError::BelowMin) => {
                ArithmeticDate::<AbstractGregorian<IsoEra>>::new_unchecked(i32::MIN, 1, 1)
            }
            Err(I32CastError::AboveMax) => ArithmeticDate::new_unchecked(i32::MAX, 12, 31),
            Ok((year, month, day)) => ArithmeticDate::new_unchecked(year, month, day),
        };

        if iso.year.checked_sub(Y::EXTENDED_YEAR_OFFSET).is_none() {
            if Y::EXTENDED_YEAR_OFFSET < 0 {
                ArithmeticDate::new_unchecked(i32::MIN - Y::EXTENDED_YEAR_OFFSET, 1, 1)
            } else {
                ArithmeticDate::new_unchecked(i32::MAX - Y::EXTENDED_YEAR_OFFSET, 12, 31)
            }
        } else {
            iso
        }
    }

    fn to_rata_die(&self, date: &Self::DateInner) -> RataDie {
        calendrical_calculations::gregorian::fixed_from_gregorian(date.year, date.month, date.day)
    }

    fn has_cheap_iso_conversion(&self) -> bool {
        true
    }

    fn from_iso(&self, iso: IsoDateInner) -> Self::DateInner {
        iso.0
    }

    fn to_iso(&self, date: &Self::DateInner) -> IsoDateInner {
        IsoDateInner(*date)
    }

    fn months_in_year(&self, date: &Self::DateInner) -> u8 {
        AbstractGregorian::<IsoEra>::months_in_provided_year(date.year)
    }

    fn days_in_year(&self, date: &Self::DateInner) -> u16 {
        365 + calendrical_calculations::gregorian::is_leap_year(date.year) as u16
    }

    fn days_in_month(&self, date: &Self::DateInner) -> u8 {
        AbstractGregorian::<IsoEra>::days_in_provided_month(date.year, date.month)
    }

    #[cfg(feature = "unstable")]
    fn add(
        &self,
        date: &Self::DateInner,
        duration: types::DateDuration,
        options: DateAddOptions,
    ) -> Result<Self::DateInner, DateError> {
        date.added(duration, &AbstractGregorian(IsoEra), options)
    }

    #[cfg(feature = "unstable")]
    fn until(
        &self,
        date1: &Self::DateInner,
        date2: &Self::DateInner,
        options: DateDifferenceOptions,
    ) -> Result<types::DateDuration, Self::DifferenceError> {
        Ok(date1.until(date2, &AbstractGregorian(IsoEra), options))
    }

    fn year_info(&self, date: &Self::DateInner) -> Self::Year {
        self.0
            .era_year_from_extended(date.year - Y::EXTENDED_YEAR_OFFSET, date.month, date.day)
    }

    fn is_in_leap_year(&self, date: &Self::DateInner) -> bool {
        calendrical_calculations::gregorian::is_leap_year(date.year)
    }

    fn month(&self, date: &Self::DateInner) -> types::MonthInfo {
        types::MonthInfo::non_lunisolar(date.month)
    }

    fn day_of_month(&self, date: &Self::DateInner) -> types::DayOfMonth {
        types::DayOfMonth(date.day)
    }

    fn day_of_year(&self, date: &Self::DateInner) -> types::DayOfYear {
        types::DayOfYear(
            calendrical_calculations::gregorian::days_before_month(date.year, date.month)
                + date.day as u16,
        )
    }

    fn debug_name(&self) -> &'static str {
        self.0.debug_name()
    }

    fn calendar_algorithm(&self) -> Option<crate::preferences::CalendarAlgorithm> {
        self.0.calendar_algorithm()
    }
}

macro_rules! impl_with_abstract_gregorian {
    ($cal_ty:ty, $inner_date_ty:ident, $eras_ty:ty, $self_ident:ident, $eras_expr:expr) => {
        #[derive(Copy, Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord)]
        pub struct $inner_date_ty(
            pub(crate)  ArithmeticDate<
                crate::cal::abstract_gregorian::AbstractGregorian<crate::cal::iso::IsoEra>,
            >,
        );

        impl crate::cal::scaffold::UnstableSealed for $cal_ty {}
        impl crate::Calendar for $cal_ty {
            type DateInner = $inner_date_ty;
            type Year = types::EraYear;
            type DifferenceError = core::convert::Infallible;

            fn from_codes(
                &self,
                era: Option<&str>,
                year: i32,
                month_code: types::MonthCode,
                day: u8,
            ) -> Result<Self::DateInner, crate::error::DateError> {
                let $self_ident = self;
                crate::cal::abstract_gregorian::AbstractGregorian($eras_expr)
                    .from_codes(era, year, month_code, day)
                    .map($inner_date_ty)
            }

            #[cfg(feature = "unstable")]
            fn from_fields(
                &self,
                fields: crate::types::DateFields,
                options: crate::options::DateFromFieldsOptions,
            ) -> Result<Self::DateInner, crate::error::DateFromFieldsError> {
                let $self_ident = self;
                crate::cal::abstract_gregorian::AbstractGregorian($eras_expr)
                    .from_fields(fields, options)
                    .map($inner_date_ty)
            }

            fn from_rata_die(&self, rd: crate::types::RataDie) -> Self::DateInner {
                let $self_ident = self;
                $inner_date_ty(
                    crate::cal::abstract_gregorian::AbstractGregorian($eras_expr).from_rata_die(rd),
                )
            }

            fn to_rata_die(&self, date: &Self::DateInner) -> crate::types::RataDie {
                let $self_ident = self;
                crate::cal::abstract_gregorian::AbstractGregorian($eras_expr).to_rata_die(&date.0)
            }

            fn has_cheap_iso_conversion(&self) -> bool {
                let $self_ident = self;
                crate::cal::abstract_gregorian::AbstractGregorian($eras_expr)
                    .has_cheap_iso_conversion()
            }

            fn from_iso(&self, iso: crate::cal::iso::IsoDateInner) -> Self::DateInner {
                let $self_ident = self;
                $inner_date_ty(
                    crate::cal::abstract_gregorian::AbstractGregorian($eras_expr).from_iso(iso),
                )
            }

            fn to_iso(&self, date: &Self::DateInner) -> crate::cal::iso::IsoDateInner {
                let $self_ident = self;
                crate::cal::abstract_gregorian::AbstractGregorian($eras_expr).to_iso(&date.0)
            }

            fn months_in_year(&self, date: &Self::DateInner) -> u8 {
                let $self_ident = self;
                crate::cal::abstract_gregorian::AbstractGregorian($eras_expr)
                    .months_in_year(&date.0)
            }

            fn days_in_year(&self, date: &Self::DateInner) -> u16 {
                let $self_ident = self;
                crate::cal::abstract_gregorian::AbstractGregorian($eras_expr).days_in_year(&date.0)
            }

            fn days_in_month(&self, date: &Self::DateInner) -> u8 {
                let $self_ident = self;
                crate::cal::abstract_gregorian::AbstractGregorian($eras_expr).days_in_month(&date.0)
            }

            #[cfg(feature = "unstable")]
            fn add(
                &self,
                date: &Self::DateInner,
                duration: crate::types::DateDuration,
                options: crate::options::DateAddOptions,
            ) -> Result<Self::DateInner, DateError> {
                let $self_ident = self;
                crate::cal::abstract_gregorian::AbstractGregorian($eras_expr)
                    .add(&date.0, duration, options)
                    .map($inner_date_ty)
            }

            #[cfg(feature = "unstable")]
            fn until(
                &self,
                date1: &Self::DateInner,
                date2: &Self::DateInner,
                options: crate::options::DateDifferenceOptions,
            ) -> Result<crate::types::DateDuration, Self::DifferenceError> {
                let $self_ident = self;
                crate::cal::abstract_gregorian::AbstractGregorian($eras_expr)
                    .until(&date1.0, &date2.0, options)
            }

            fn year_info(&self, date: &Self::DateInner) -> Self::Year {
                let $self_ident = self;
                crate::cal::abstract_gregorian::AbstractGregorian($eras_expr).year_info(&date.0)
            }

            fn is_in_leap_year(&self, date: &Self::DateInner) -> bool {
                let $self_ident = self;
                crate::cal::abstract_gregorian::AbstractGregorian($eras_expr)
                    .is_in_leap_year(&date.0)
            }

            fn month(&self, date: &Self::DateInner) -> types::MonthInfo {
                let $self_ident = self;
                crate::cal::abstract_gregorian::AbstractGregorian($eras_expr).month(&date.0)
            }

            fn day_of_month(&self, date: &Self::DateInner) -> types::DayOfMonth {
                let $self_ident = self;
                crate::cal::abstract_gregorian::AbstractGregorian($eras_expr).day_of_month(&date.0)
            }

            fn day_of_year(&self, date: &Self::DateInner) -> types::DayOfYear {
                let $self_ident = self;
                crate::cal::abstract_gregorian::AbstractGregorian($eras_expr).day_of_year(&date.0)
            }

            fn debug_name(&self) -> &'static str {
                let $self_ident = self;
                crate::cal::abstract_gregorian::AbstractGregorian($eras_expr).debug_name()
            }

            fn calendar_algorithm(&self) -> Option<crate::preferences::CalendarAlgorithm> {
                let $self_ident = self;
                crate::cal::abstract_gregorian::AbstractGregorian($eras_expr).calendar_algorithm()
            }
        }
    };
}
pub(crate) use impl_with_abstract_gregorian;
