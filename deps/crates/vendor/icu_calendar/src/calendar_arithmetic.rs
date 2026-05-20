// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::error::DateError;
use crate::types::DayOfYear;
use crate::{types, Calendar, DateDuration, DateDurationUnit, RangeError};
use core::cmp::Ordering;
use core::convert::TryInto;
use core::fmt::Debug;
use core::hash::{Hash, Hasher};
use core::marker::PhantomData;
use tinystr::tinystr;

#[derive(Debug)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub(crate) struct ArithmeticDate<C: CalendarArithmetic> {
    pub year: C::YearInfo,
    /// 1-based month of year
    pub month: u8,
    /// 1-based day of month
    pub day: u8,
    marker: PhantomData<C>,
}

// Manual impls since the derive will introduce a C: Trait bound
// and only the year value should be compared
impl<C: CalendarArithmetic> Copy for ArithmeticDate<C> {}
impl<C: CalendarArithmetic> Clone for ArithmeticDate<C> {
    fn clone(&self) -> Self {
        *self
    }
}

impl<C: CalendarArithmetic> PartialEq for ArithmeticDate<C> {
    fn eq(&self, other: &Self) -> bool {
        self.year.into() == other.year.into() && self.month == other.month && self.day == other.day
    }
}

impl<C: CalendarArithmetic> Eq for ArithmeticDate<C> {}

impl<C: CalendarArithmetic> Ord for ArithmeticDate<C> {
    fn cmp(&self, other: &Self) -> Ordering {
        self.year
            .into()
            .cmp(&other.year.into())
            .then(self.month.cmp(&other.month))
            .then(self.day.cmp(&other.day))
    }
}

impl<C: CalendarArithmetic> PartialOrd for ArithmeticDate<C> {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl<C: CalendarArithmetic> Hash for ArithmeticDate<C> {
    fn hash<H>(&self, state: &mut H)
    where
        H: Hasher,
    {
        self.year.into().hash(state);
        self.month.hash(state);
        self.day.hash(state);
    }
}

/// Maximum number of iterations when iterating through the days of a month; can be increased if necessary
#[allow(dead_code)] // TODO: Remove dead code tag after use
pub(crate) const MAX_ITERS_FOR_DAYS_OF_MONTH: u8 = 33;

pub(crate) trait CalendarArithmetic: Calendar {
    /// This stores the year as either an i32, or a type containing more
    /// useful computational information.
    type YearInfo: Copy + Debug + Into<i32>;

    // TODO(#3933): potentially make these methods take &self instead, and absorb certain y/m parameters
    // based on usage patterns (e.g month_days is only ever called with self.year)
    fn days_in_provided_month(year: Self::YearInfo, month: u8) -> u8;
    fn months_in_provided_year(year: Self::YearInfo) -> u8;
    fn provided_year_is_leap(year: Self::YearInfo) -> bool;
    fn last_month_day_in_provided_year(year: Self::YearInfo) -> (u8, u8);

    /// Calculate the days in a given year
    /// Can be overridden with simpler implementations for solar calendars
    /// (typically, 366 in leap, 365 otherwise) Leave this as the default
    /// for lunar calendars
    ///
    /// The name has `provided` in it to avoid clashes with Calendar
    fn days_in_provided_year(year: Self::YearInfo) -> u16 {
        let months_in_year = Self::months_in_provided_year(year);
        let mut days: u16 = 0;
        for month in 1..=months_in_year {
            days += Self::days_in_provided_month(year, month) as u16;
        }
        days
    }
}

pub(crate) trait PrecomputedDataSource<YearInfo> {
    /// Given a calendar year, load (or compute) the YearInfo for it
    ///
    /// In the future we may pass in an optional previous YearInfo alongside the year
    /// it matches to allow code to take shortcuts.
    fn load_or_compute_info(&self, year: i32) -> YearInfo;
}

impl PrecomputedDataSource<i32> for () {
    fn load_or_compute_info(&self, year: i32) -> i32 {
        year
    }
}

impl<C: CalendarArithmetic> ArithmeticDate<C> {
    /// Create a new `ArithmeticDate` without checking that `month` and `day` are in bounds.
    #[inline]
    pub const fn new_unchecked(year: C::YearInfo, month: u8, day: u8) -> Self {
        ArithmeticDate {
            year,
            month,
            day,
            marker: PhantomData,
        }
    }

    #[inline]
    pub fn min_date() -> Self
    where
        C: CalendarArithmetic<YearInfo = i32>,
    {
        ArithmeticDate {
            year: i32::MIN,
            month: 1,
            day: 1,
            marker: PhantomData,
        }
    }

    #[inline]
    pub fn max_date() -> Self
    where
        C: CalendarArithmetic<YearInfo = i32>,
    {
        let year = i32::MAX;
        let (month, day) = C::last_month_day_in_provided_year(year);
        ArithmeticDate {
            year: i32::MAX,
            month,
            day,
            marker: PhantomData,
        }
    }

    #[inline]
    fn offset_days(&mut self, mut day_offset: i32, data: &impl PrecomputedDataSource<C::YearInfo>) {
        while day_offset != 0 {
            let month_days = C::days_in_provided_month(self.year, self.month);
            if self.day as i32 + day_offset > month_days as i32 {
                self.offset_months(1, data);
                day_offset -= month_days as i32;
            } else if self.day as i32 + day_offset < 1 {
                self.offset_months(-1, data);
                day_offset += C::days_in_provided_month(self.year, self.month) as i32;
            } else {
                self.day = (self.day as i32 + day_offset) as u8;
                day_offset = 0;
            }
        }
    }

    #[inline]
    fn offset_months(
        &mut self,
        mut month_offset: i32,
        data: &impl PrecomputedDataSource<C::YearInfo>,
    ) {
        while month_offset != 0 {
            let year_months = C::months_in_provided_year(self.year);
            if self.month as i32 + month_offset > year_months as i32 {
                self.year = data.load_or_compute_info(self.year.into() + 1);
                month_offset -= year_months as i32;
            } else if self.month as i32 + month_offset < 1 {
                self.year = data.load_or_compute_info(self.year.into() - 1);
                month_offset += C::months_in_provided_year(self.year) as i32;
            } else {
                self.month = (self.month as i32 + month_offset) as u8;
                month_offset = 0
            }
        }
    }

    #[inline]
    pub fn offset_date(
        &mut self,
        offset: DateDuration<C>,
        data: &impl PrecomputedDataSource<C::YearInfo>,
    ) {
        if offset.years != 0 {
            // For offset_date to work with lunar calendars, need to handle an edge case where the original month is not valid in the future year.
            self.year = data.load_or_compute_info(self.year.into() + offset.years);
        }

        self.offset_months(offset.months, data);

        let day_offset = offset.days + offset.weeks * 7 + self.day as i32 - 1;
        self.day = 1;
        self.offset_days(day_offset, data);
    }

    #[inline]
    pub fn until(
        &self,
        date2: ArithmeticDate<C>,
        _largest_unit: DateDurationUnit,
        _smaller_unit: DateDurationUnit,
    ) -> DateDuration<C> {
        // This simple implementation does not need C::PrecomputedDataSource right now, but it
        // likely will once we've written a proper implementation
        DateDuration::new(
            self.year.into() - date2.year.into(),
            self.month as i32 - date2.month as i32,
            0,
            self.day as i32 - date2.day as i32,
        )
    }

    #[inline]
    pub fn days_in_year(&self) -> u16 {
        C::days_in_provided_year(self.year)
    }

    #[inline]
    pub fn months_in_year(&self) -> u8 {
        C::months_in_provided_year(self.year)
    }

    #[inline]
    pub fn days_in_month(&self) -> u8 {
        C::days_in_provided_month(self.year, self.month)
    }

    #[inline]
    pub fn date_from_year_day(year: i32, year_day: u32) -> ArithmeticDate<C>
    where
        C: CalendarArithmetic<YearInfo = i32>,
    {
        let mut month = 1;
        let mut day = year_day as i32;
        while month <= C::months_in_provided_year(year) {
            let month_days = C::days_in_provided_month(year, month) as i32;
            if day <= month_days {
                break;
            } else {
                day -= month_days;
                month += 1;
            }
        }

        debug_assert!(day <= C::days_in_provided_month(year, month) as i32);
        #[allow(clippy::unwrap_used)]
        // The day is expected to be within the range of month_days of C
        ArithmeticDate {
            year,
            month,
            day: day.try_into().unwrap_or(1),
            marker: PhantomData,
        }
    }

    #[inline]
    pub fn day_of_month(&self) -> types::DayOfMonth {
        types::DayOfMonth(self.day)
    }

    #[inline]
    pub fn day_of_year(&self) -> DayOfYear {
        let mut day_of_year = 0;
        for month in 1..self.month {
            day_of_year += C::days_in_provided_month(self.year, month) as u16;
        }
        DayOfYear(day_of_year + (self.day as u16))
    }

    pub fn extended_year(&self) -> i32 {
        self.year.into()
    }

    /// The [`types::MonthInfo`] for the current month (with month code) for a solar calendar
    /// Lunar calendars should not use this method and instead manually implement a month code
    /// resolver.
    /// Originally "solar_month" but renamed because it can be used for some lunar calendars
    ///
    /// Returns "und" if run with months that are out of bounds for the current
    /// calendar.
    #[inline]
    pub fn month(&self) -> types::MonthInfo {
        let code = match self.month {
            a if a > C::months_in_provided_year(self.year) => tinystr!(4, "und"),
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
            13 => tinystr!(4, "M13"),
            _ => tinystr!(4, "und"),
        };
        types::MonthInfo {
            ordinal: self.month,
            standard_code: types::MonthCode(code),
            formatting_code: types::MonthCode(code),
        }
    }

    /// Construct a new arithmetic date from a year, month code, and day, bounds checking
    /// the month and day
    /// Originally (new_from_solar_codes) but renamed because it works for some lunar calendars
    pub fn new_from_codes<C2: Calendar>(
        // Separate type since the debug_name() impl may differ when DateInner types
        // are nested (e.g. in GregorianDateInner)
        _cal: &C2,
        year: i32,
        month_code: types::MonthCode,
        day: u8,
    ) -> Result<Self, DateError>
    where
        C: CalendarArithmetic<YearInfo = i32>,
    {
        let month = if let Some((ordinal, false)) = month_code.parsed() {
            ordinal
        } else {
            return Err(DateError::UnknownMonthCode(month_code));
        };

        if month > C::months_in_provided_year(year) {
            return Err(DateError::UnknownMonthCode(month_code));
        }

        let max_day = C::days_in_provided_month(year, month);
        if day == 0 || day > max_day {
            return Err(DateError::Range {
                field: "day",
                value: day as i32,
                min: 1,
                max: max_day as i32,
            });
        }

        Ok(Self::new_unchecked(year, month, day))
    }

    /// Construct a new arithmetic date from a year, month ordinal, and day, bounds checking
    /// the month and day
    pub fn new_from_ordinals(year: C::YearInfo, month: u8, day: u8) -> Result<Self, RangeError> {
        let max_month = C::months_in_provided_year(year);
        if month == 0 || month > max_month {
            return Err(RangeError {
                field: "month",
                value: month as i32,
                min: 1,
                max: max_month as i32,
            });
        }
        let max_day = C::days_in_provided_month(year, month);
        if day == 0 || day > max_day {
            return Err(RangeError {
                field: "day",
                value: day as i32,
                min: 1,
                max: max_day as i32,
            });
        }

        Ok(Self::new_unchecked(year, month, day))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::cal::Iso;

    #[test]
    fn test_ord() {
        let dates_in_order = [
            ArithmeticDate::<Iso>::new_unchecked(-10, 1, 1),
            ArithmeticDate::<Iso>::new_unchecked(-10, 1, 2),
            ArithmeticDate::<Iso>::new_unchecked(-10, 2, 1),
            ArithmeticDate::<Iso>::new_unchecked(-1, 1, 1),
            ArithmeticDate::<Iso>::new_unchecked(-1, 1, 2),
            ArithmeticDate::<Iso>::new_unchecked(-1, 2, 1),
            ArithmeticDate::<Iso>::new_unchecked(0, 1, 1),
            ArithmeticDate::<Iso>::new_unchecked(0, 1, 2),
            ArithmeticDate::<Iso>::new_unchecked(0, 2, 1),
            ArithmeticDate::<Iso>::new_unchecked(1, 1, 1),
            ArithmeticDate::<Iso>::new_unchecked(1, 1, 2),
            ArithmeticDate::<Iso>::new_unchecked(1, 2, 1),
            ArithmeticDate::<Iso>::new_unchecked(10, 1, 1),
            ArithmeticDate::<Iso>::new_unchecked(10, 1, 2),
            ArithmeticDate::<Iso>::new_unchecked(10, 2, 1),
        ];
        for (i, i_date) in dates_in_order.iter().enumerate() {
            for (j, j_date) in dates_in_order.iter().enumerate() {
                let result1 = i_date.cmp(j_date);
                let result2 = j_date.cmp(i_date);
                assert_eq!(result1.reverse(), result2);
                assert_eq!(i.cmp(&j), i_date.cmp(j_date));
            }
        }
    }

    #[test]
    pub fn zero() {
        use crate::Date;
        Date::try_new_iso(2024, 0, 1).unwrap_err();
        Date::try_new_iso(2024, 1, 0).unwrap_err();
        Date::try_new_iso(2024, 0, 0).unwrap_err();
    }
}
