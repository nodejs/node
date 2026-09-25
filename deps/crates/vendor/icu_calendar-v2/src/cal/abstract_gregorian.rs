// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::cal::iso::{IsoDateInner, IsoEra};
use crate::calendar_arithmetic::{ArithmeticDate, DateFieldsResolver, PackWithMD};
use crate::error::{
    DateAddError, DateFromFieldsError, DateNewError, EcmaReferenceYearError, UnknownEraError,
};
use crate::options::DateFromFieldsOptions;
use crate::options::{DateAddOptions, DateDifferenceOptions};
use crate::preferences::CalendarAlgorithm;
use crate::types::EraYear;
use crate::{types, Calendar};
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

#[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub(crate) struct AbstractGregorianYear {
    iso_year: i32,
}

impl core::ops::Sub<AbstractGregorianYear> for AbstractGregorianYear {
    type Output = i32;
    #[inline]
    fn sub(self, rhs: AbstractGregorianYear) -> Self::Output {
        self.iso_year - rhs.iso_year
    }
}

impl AbstractGregorianYear {
    pub(crate) fn from_extended_year<Y: GregorianYears>(extended_year: i32) -> Self {
        Self {
            iso_year: extended_year + Y::EXTENDED_YEAR_OFFSET,
        }
    }
    #[allow(unused)] // only in ixdtf, jiff, ...
    pub(crate) fn from_iso_year(iso_year: i32) -> Self {
        Self { iso_year }
    }
    pub(crate) fn to_extended_year<Y: GregorianYears>(self) -> i32 {
        self.iso_year - Y::EXTENDED_YEAR_OFFSET
    }
    pub(crate) fn to_iso_year(self) -> i32 {
        self.iso_year
    }
}

impl PackWithMD for AbstractGregorianYear {
    type Packed = <i32 as PackWithMD>::Packed;
    #[inline]
    fn pack(self, month: u8, day: u8) -> Self::Packed {
        <i32 as PackWithMD>::pack(self.iso_year, month, day)
    }
    #[inline]
    fn unpack_day(packed: Self::Packed) -> u8 {
        <i32 as PackWithMD>::unpack_day(packed)
    }
    #[inline]
    fn unpack_month(packed: Self::Packed) -> u8 {
        <i32 as PackWithMD>::unpack_month(packed)
    }
    #[inline]
    fn unpack_year(packed: Self::Packed) -> Self {
        let iso_year = <i32 as PackWithMD>::unpack_year(packed);
        Self { iso_year }
    }
}

pub(crate) const REFERENCE_YEAR: AbstractGregorianYear = AbstractGregorianYear { iso_year: 1972 };

#[cfg(test)]
pub(crate) const LAST_DAY_OF_REFERENCE_YEAR: RataDie =
    calendrical_calculations::gregorian::day_before_year(REFERENCE_YEAR.iso_year + 1);

impl<Y: GregorianYears> DateFieldsResolver for AbstractGregorian<Y> {
    // Gregorian year
    type YearInfo = AbstractGregorianYear;

    fn days_in_provided_month(year: AbstractGregorianYear, month: u8) -> u8 {
        if month == 2 {
            28 + calendrical_calculations::gregorian::is_leap_year(year.iso_year) as u8
        } else {
            // https://www.youtube.com/watch?v=J9KijLyP-yg&t=1394s
            30 | month ^ (month >> 3)
        }
    }

    #[inline]
    fn extended_year_from_era_year_unchecked(
        &self,
        era: &[u8],
        era_year: i32,
    ) -> Result<i32, UnknownEraError> {
        self.0.extended_from_era_year(Some(era), era_year)
    }

    #[inline]
    fn year_info_from_extended(&self, extended_year: i32) -> Self::YearInfo {
        AbstractGregorianYear::from_extended_year::<Y>(extended_year)
    }

    #[inline]
    fn extended_from_year_info(&self, year_info: Self::YearInfo) -> i32 {
        year_info.to_extended_year::<Y>()
    }

    #[inline]
    fn reference_year_from_month_day(
        &self,
        _month: types::Month,
        _day: u8,
    ) -> Result<Self::YearInfo, EcmaReferenceYearError> {
        Ok(REFERENCE_YEAR)
    }

    fn to_rata_die_inner(year: Self::YearInfo, month: u8, day: u8) -> RataDie {
        calendrical_calculations::gregorian::fixed_from_gregorian(year.iso_year, month, day)
    }
}

impl<Y: GregorianYears> crate::cal::scaffold::UnstableSealed for AbstractGregorian<Y> {}

impl<Y: GregorianYears> Calendar for AbstractGregorian<Y> {
    type DateInner = ArithmeticDate<AbstractGregorian<IsoEra>>;
    type Year = EraYear;
    type DateCompatibilityError = core::convert::Infallible;

    fn new_date(
        &self,
        year: types::YearInput,
        month: types::Month,
        day: u8,
    ) -> Result<Self::DateInner, DateNewError> {
        ArithmeticDate::from_input_year_month_code_day(year, month, day, self)
            .map(ArithmeticDate::cast)
    }

    fn from_fields(
        &self,
        fields: types::DateFields,
        options: DateFromFieldsOptions,
    ) -> Result<Self::DateInner, DateFromFieldsError> {
        ArithmeticDate::from_fields(fields, options, self).map(ArithmeticDate::cast)
    }

    fn from_rata_die(&self, date: RataDie) -> Self::DateInner {
        // by precondition the year cannot exceed i32, so the error case is unreachable
        let (iso_year, month, day) =
            calendrical_calculations::gregorian::gregorian_from_fixed(date).unwrap_or((1, 1, 1));

        // date is in the valid RD range
        ArithmeticDate::new_unchecked(AbstractGregorianYear { iso_year }, month, day)
    }

    fn to_rata_die(&self, date: &Self::DateInner) -> RataDie {
        date.to_rata_die()
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
        AbstractGregorian::<IsoEra>::months_in_provided_year(date.year())
    }

    fn days_in_year(&self, date: &Self::DateInner) -> u16 {
        365 + calendrical_calculations::gregorian::is_leap_year(date.year().iso_year) as u16
    }

    fn days_in_month(&self, date: &Self::DateInner) -> u8 {
        AbstractGregorian::<IsoEra>::days_in_provided_month(date.year(), date.month())
    }

    fn add(
        &self,
        date: &Self::DateInner,
        duration: types::DateDuration,
        options: DateAddOptions,
    ) -> Result<Self::DateInner, DateAddError> {
        date.added(duration, &AbstractGregorian(IsoEra), options)
    }

    fn until(
        &self,
        date1: &Self::DateInner,
        date2: &Self::DateInner,
        options: DateDifferenceOptions,
    ) -> types::DateDuration {
        date1.until(date2, &AbstractGregorian(IsoEra), options)
    }

    fn check_date_compatibility(&self, other: &Self) -> Result<(), Self::DateCompatibilityError> {
        // The GregorianYears generic only affects constructors and year_info,
        // it does not affect date identity.
        let _ignored = other;
        Ok(())
    }

    fn year_info(&self, date: &Self::DateInner) -> Self::Year {
        self.0.era_year_from_extended(
            date.year().to_extended_year::<Y>(),
            date.month(),
            date.day(),
        )
    }

    fn is_in_leap_year(&self, date: &Self::DateInner) -> bool {
        calendrical_calculations::gregorian::is_leap_year(date.year().iso_year)
    }

    fn month(&self, date: &Self::DateInner) -> types::MonthInfo {
        types::MonthInfo::new(self, date.cast())
    }

    fn day_of_month(&self, date: &Self::DateInner) -> types::DayOfMonth {
        types::DayOfMonth(date.day())
    }

    fn day_of_year(&self, date: &Self::DateInner) -> types::DayOfYear {
        types::DayOfYear(
            calendrical_calculations::gregorian::days_before_month(
                date.year().iso_year,
                date.month(),
            ) + date.day() as u16,
        )
    }

    fn debug_name(&self) -> &'static str {
        self.0.debug_name()
    }

    fn calendar_algorithm(&self) -> Option<CalendarAlgorithm> {
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
            type DateCompatibilityError = core::convert::Infallible;
            fn new_date(
                &self,
                year: types::YearInput,
                month: types::Month,
                day: u8,
            ) -> Result<Self::DateInner, crate::error::DateNewError> {
                let $self_ident = self;
                crate::cal::abstract_gregorian::AbstractGregorian($eras_expr)
                    .new_date(year, month, day)
                    .map($inner_date_ty)
            }

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

            fn add(
                &self,
                date: &Self::DateInner,
                duration: crate::types::DateDuration,
                options: crate::options::DateAddOptions,
            ) -> Result<Self::DateInner, $crate::error::DateAddError> {
                let $self_ident = self;
                crate::cal::abstract_gregorian::AbstractGregorian($eras_expr)
                    .add(&date.0, duration, options)
                    .map($inner_date_ty)
            }

            fn until(
                &self,
                date1: &Self::DateInner,
                date2: &Self::DateInner,
                options: crate::options::DateDifferenceOptions,
            ) -> crate::types::DateDuration {
                let $self_ident = self;
                crate::cal::abstract_gregorian::AbstractGregorian($eras_expr)
                    .until(&date1.0, &date2.0, options)
            }

            fn check_date_compatibility(
                &self,
                other: &Self,
            ) -> Result<(), Self::DateCompatibilityError> {
                let $self_ident = self;
                let c1 = crate::cal::abstract_gregorian::AbstractGregorian($eras_expr);
                let $self_ident = other;
                let c2 = crate::cal::abstract_gregorian::AbstractGregorian($eras_expr);
                c1.check_date_compatibility(&c2)
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
