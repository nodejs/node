// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use calendrical_calculations::rata_die::RataDie;

use crate::duration::DateDuration;
use crate::error::{
    range_check, DateAddError, DateFromFieldsError, DateNewError, EcmaReferenceYearError,
    LunisolarDateError, MonthError, UnknownEraError,
};
use crate::options::{DateAddOptions, DateDifferenceOptions, DateDurationUnit};
use crate::options::{DateFromFieldsOptions, MissingFieldsStrategy, Overflow};
use crate::types::{DateFields, Month};
use crate::{types, Calendar, DateError, RangeError};
use core::cmp::Ordering;
use core::fmt::Debug;
use core::hash::{Hash, Hasher};
use core::ops::RangeInclusive;
use core::ops::Sub;

/// This is checked by convenience constructors and `from_codes`.
/// Internally we don't care about this invariant.
pub(crate) const CONSTRUCTOR_YEAR_RANGE: RangeInclusive<i32> = -9999..=9999;

/// This is a fundamental invariant of `ArithmeticDate` and by extension all our
/// date types. Constructors that don't check [`CONSTRUCTOR_YEAR_RANGE`] check this range.
///
/// This is the range used by `Date::from_rata_die`, `Date::try_from_fields`,
/// and Date arithmetic operations.
pub(crate) const VALID_RD_RANGE: RangeInclusive<RataDie> =
    calendrical_calculations::gregorian::fixed_from_gregorian(-999999, 1, 1)
        ..=calendrical_calculations::gregorian::fixed_from_gregorian(999999, 12, 31);

/// This is a year range that exceeds the RD range for all calendars.
///
/// We *must* ensure dates are within `VALID_RD_RANGE` some point before constructing them.
///
/// However, we may need to perform a fair amount of calendar arithmetic before
/// getting to the point where we know if we are in range, and the calendar arithmetic
/// is fragile (chance of math issues, slowness, debug assertions) at high ranges.
///
/// So we try to early-check year values where possible. We use a "generous" year range
/// which is known to be wider than the valid RD range for any era in any currently
/// supported calendar.
///
/// `VALID_RD_RANGE` maps to 1031332 BH..=1030050 AH in the Islamic calendars, which have
/// the shortest years. We pick a slightly wider range so that temporary intermediate
/// values are not affected by this.
///
/// The tests in `extrema.rs` ensure that all in-range dates can be produced by the APIs that,
/// use this check, and that these year numbers map to out-of-range values for every era.
pub(crate) const GENEROUS_YEAR_RANGE: RangeInclusive<i32> = -1_040_000..=1_040_000;

/// These are for early-checking durations in `ArithmeticDate::add`
///
/// They are crate-public so they can be used in tests, but should probably not be used elsewhere.
pub(crate) const GENEROUS_MAX_YEARS: u32 =
    (*GENEROUS_YEAR_RANGE.end() - *GENEROUS_YEAR_RANGE.start()) as u32;
pub(crate) const GENEROUS_MAX_MONTHS: u32 = GENEROUS_MAX_YEARS * 13;
pub(crate) const GENEROUS_MAX_DAYS: u32 = GENEROUS_MAX_MONTHS * 31;

/// This is the year range all calendar specific computation can assume their data to be in.
///
/// It is wider than [`GENEROUS_YEAR_RANGE`]: `GENEROUS_YEAR_RANGE` is used for checking user
/// input, this is more of an internal invariant for assertions. New calendar implementations
/// must ensure they do not have overflows or panics for year (era year or extended year) within this range.
///
/// This value is computed by going past the max value producible by adding a duration,
/// which is preemptively capped at `GENEROUS_MAX_YEARS`, `GENEROUS_MAX_MONTHS`, `GENEROUS_MAX_DAYS`
/// in `added()`.
pub(crate) const SAFE_YEAR_RANGE: RangeInclusive<i32> = -8_000_000..=8_000_000;

// Invariant: VALID_RD_RANGE contains the date
#[derive(Debug)]
pub(crate) struct ArithmeticDate<C: DateFieldsResolver>(<C::YearInfo as PackWithMD>::Packed);

// Manual impls since the derive will introduce a C: Trait bound
// and only the year value should be compared
impl<C: DateFieldsResolver> Copy for ArithmeticDate<C> {}
impl<C: DateFieldsResolver> Clone for ArithmeticDate<C> {
    fn clone(&self) -> Self {
        *self
    }
}

impl<C: DateFieldsResolver> PartialEq for ArithmeticDate<C> {
    fn eq(&self, other: &Self) -> bool {
        self.year() == other.year() && self.month() == other.month() && self.day() == other.day()
    }
}

impl<C: DateFieldsResolver> Eq for ArithmeticDate<C> {}

impl<C: DateFieldsResolver> Ord for ArithmeticDate<C> {
    fn cmp(&self, other: &Self) -> Ordering {
        self.year()
            .cmp(&other.year())
            .then(self.month().cmp(&other.month()))
            .then(self.day().cmp(&other.day()))
    }
}

impl<C: DateFieldsResolver> PartialOrd for ArithmeticDate<C> {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl<C: DateFieldsResolver> Hash for ArithmeticDate<C> {
    fn hash<H>(&self, state: &mut H)
    where
        H: Hasher,
    {
        self.year().hash(state);
        self.month().hash(state);
        self.day().hash(state);
    }
}

/// Same data as [`ArithmeticDate`], but may be out of [`VALID_RD_RANGE`]
#[derive(Debug)]
struct UncheckedArithmeticDate<C: DateFieldsResolver> {
    year: C::YearInfo,
    ordinal_month: u8,
    day: u8,
}

impl<C: DateFieldsResolver> UncheckedArithmeticDate<C> {
    fn into_checked(self) -> Option<ArithmeticDate<C>> {
        let rd = C::to_rata_die_inner(self.year, self.ordinal_month, self.day);
        if !VALID_RD_RANGE.contains(&rd) {
            return None;
        }
        // Invariant checked above
        Some(ArithmeticDate::new_unchecked(
            self.year,
            self.ordinal_month,
            self.day,
        ))
    }
}

/// Maximum number of iterations when iterating through the days of a month; can be increased if necessary
#[allow(dead_code)] // TODO: Remove dead code tag after use
pub(crate) const MAX_ITERS_FOR_DAYS_OF_MONTH: u8 = 33;

pub(crate) trait PackWithMD: Copy {
    type Packed: Copy + Debug;

    fn pack(self, month: u8, day: u8) -> Self::Packed;
    fn unpack_year(packed: Self::Packed) -> Self;
    fn unpack_month(packed: Self::Packed) -> u8;
    fn unpack_day(packed: Self::Packed) -> u8;
}

impl PackWithMD for i32 {
    /// 2 bits unused, 21 bits year (`test_validity_ranges`),
    /// 4 bits month (1..13), 5 bits day (1..31)
    type Packed = [u8; 4];

    fn pack(self, month: u8, day: u8) -> Self::Packed {
        (self << 9 | i32::from(month) << 5 | i32::from(day)).to_le_bytes()
    }

    fn unpack_year(packed: Self::Packed) -> Self {
        let packed = i32::from_le_bytes(packed);
        packed >> 9
    }

    fn unpack_month(packed: Self::Packed) -> u8 {
        let packed = i32::from_le_bytes(packed);
        (packed >> 5 & 0b1111) as u8
    }

    fn unpack_day(packed: Self::Packed) -> u8 {
        let packed = i32::from_le_bytes(packed);
        (packed & 0b11111) as u8
    }
}

/// Trait for converting from era codes, month codes, and other fields to year/month/day ordinals.
pub(crate) trait DateFieldsResolver: Calendar {
    /// This stores the year as either an i32, or a type containing more
    /// useful computational information.
    type YearInfo: Copy + Debug + Hash + Ord + Sub<Output = i32> + PackWithMD;

    fn days_in_provided_month(year: Self::YearInfo, month: u8) -> u8;

    /// Converts the era and era year to a [`Self::YearInfo`]. If the calendar does not have eras,
    /// this should always return an Err result.
    ///
    /// Precondition: `era_year` is in [`SAFE_YEAR_RANGE`].
    fn extended_year_from_era_year_unchecked(
        &self,
        era: &[u8],
        era_year: i32,
    ) -> Result<i32, UnknownEraError>;

    /// Converts an extended year to a [`Self::YearInfo`].
    ///
    /// Precondition: `extended_year` is in [`GENEROUS_YEAR_RANGE`].
    ///
    /// Some calendars with complex calculations use this precondition to
    /// ensure that the year is in a range where their arithmetic code is well-behaved.
    ///
    /// Calendar implementors who need this are encouraged to debug assert that the invariant
    /// is upheld.
    ///
    /// This should primarily be used in constructors that range-check with
    /// the very narrow [`CONSTRUCTOR_YEAR_RANGE`], after that check. If the
    /// constructor checks [`VALID_RD_RANGE`], please use `year_info_from_extended_checked`.
    fn year_info_from_extended(&self, extended_year: i32) -> Self::YearInfo;

    /// Inverse operation of [`Self::year_info_from_extended`].
    fn extended_from_year_info(&self, year_info: Self::YearInfo) -> i32;

    /// Calculates the ECMA reference year (represented as an extended year)
    /// for the month code and day, or an error if the month code and day are invalid.
    ///
    /// Note that this is called before any potential `Overflow::Constrain` application,
    /// so this should accept out-of-range day values as if they are the highest possible
    /// day for the given month.
    fn reference_year_from_month_day(
        &self,
        month: Month,
        day: u8,
    ) -> Result<Self::YearInfo, EcmaReferenceYearError>;

    /// The number of months for the given year.
    ///
    /// The default impl is for non-lunisolar calendars with 12 months!
    fn months_in_provided_year(_year: Self::YearInfo) -> u8 {
        12
    }

    /// A lower bound for the number of months in `years` years, starting from the beginning of this year.
    ///
    /// This may be equal to the number of months, but should never be larger. Ideally,
    /// implementations should be close enough to the actual answer such that the error
    /// is bounded by a constant with respect to `years`.
    ///
    /// The default impl is for non-lunisolar calendars with 12 months!
    ///
    /// `until()` will debug assert if this ever returns a value greater than the
    /// month diff betweeen two dates.
    #[inline]
    fn min_months_from(_start: Self::YearInfo, years: i32) -> i32 {
        12 * years
    }

    /// Calculates the ordinal month for the given year and month code.
    ///
    /// The default impl is for non-lunisolar calendars!
    #[inline]
    fn ordinal_from_month(
        &self,
        year: Self::YearInfo,
        month: Month,
        _overflow: Overflow,
    ) -> Result<u8, MonthError> {
        match (month.number(), month.is_leap()) {
            (month_number, false)
                if (1..=Self::months_in_provided_year(year)).contains(&month_number) =>
            {
                Ok(month_number)
            }
            _ => Err(MonthError::NotInCalendar),
        }
    }

    /// Calculates the month code from the given ordinal month and year.
    ///
    /// The caller must ensure that the ordinal is in range.
    ///
    /// The default impl is for non-lunisolar calendars!
    #[inline]
    fn month_from_ordinal(&self, _year: Self::YearInfo, ordinal_month: u8) -> Month {
        Month::new_unchecked(ordinal_month, false)
    }

    // Date-to-RD conversion
    // Used internally for implementing date arithmetic
    fn to_rata_die_inner(year: Self::YearInfo, month: u8, day: u8) -> RataDie;
}

impl<C: DateFieldsResolver> ArithmeticDate<C> {
    pub(crate) fn to_rata_die(self) -> RataDie {
        C::to_rata_die_inner(self.year(), self.month(), self.day())
    }

    pub(crate) fn year(self) -> C::YearInfo {
        C::YearInfo::unpack_year(self.0)
    }

    pub(crate) fn month(self) -> u8 {
        C::YearInfo::unpack_month(self.0)
    }

    pub(crate) fn day(self) -> u8 {
        C::YearInfo::unpack_day(self.0)
    }

    /// Precondition: the date is in `VALID_RD_RANGE`.
    ///
    /// Use `UncheckedArithmeticDate` if you wish to generate intermediate out-of-range dates.
    #[inline]
    pub(crate) fn new_unchecked(year: C::YearInfo, month: u8, day: u8) -> Self {
        ArithmeticDate(C::YearInfo::pack(year, month, day))
    }

    pub(crate) fn cast<C2: DateFieldsResolver<YearInfo = C::YearInfo>>(self) -> ArithmeticDate<C2> {
        ArithmeticDate::new_unchecked(self.year(), self.month(), self.day())
    }

    // Used by `from_codes`, checks `CONSTRUCTOR_YEAR_RANGE`
    pub(crate) fn from_input_year_month_code_day(
        year: types::YearInput,
        month: Month,
        day: u8,
        calendar: &C,
    ) -> Result<Self, DateNewError> {
        let extended_year = match year {
            types::YearInput::Extended(y) => {
                if !CONSTRUCTOR_YEAR_RANGE.contains(&y) {
                    return Err(DateNewError::InvalidYear);
                }
                y
            }
            types::YearInput::EraYear(era, y) => {
                if !CONSTRUCTOR_YEAR_RANGE.contains(&y) {
                    return Err(DateNewError::InvalidYear);
                }
                calendar
                    .extended_year_from_era_year_unchecked(era.as_bytes(), y)
                    .map_err(|_| DateNewError::InvalidEra)?
            }
        };
        let year = calendar.year_info_from_extended(extended_year);

        let month = calendar
            .ordinal_from_month(year, month, Overflow::Reject)
            .map_err(|e| match e {
                MonthError::NotInCalendar => DateNewError::MonthNotInCalendar,
                MonthError::NotInYear => DateNewError::MonthNotInYear,
            })?;

        let max_day = C::days_in_provided_month(year, month);
        if day < 1 || day > max_day {
            return Err(DateNewError::InvalidDay { max: max_day });
        }

        // date is in the valid year range, and therefore in the valid RD range
        Ok(ArithmeticDate::new_unchecked(year, month, day))
    }

    // Used by calendar-specific constructors (lunisolar), checks `CONSTRUCTOR_YEAR_RANGE`
    pub(crate) fn try_from_ymd_lunisolar(
        year: i32,
        month: Month,
        day: u8,
        calendar: &C,
    ) -> Result<Self, LunisolarDateError> {
        if !CONSTRUCTOR_YEAR_RANGE.contains(&year) {
            return Err(LunisolarDateError::InvalidYear);
        }
        let year = calendar.year_info_from_extended(year);

        let month = match calendar.ordinal_from_month(year, month, Overflow::Reject) {
            Ok(month) => month,
            Err(MonthError::NotInCalendar) => return Err(LunisolarDateError::MonthNotInCalendar),
            Err(MonthError::NotInYear) => return Err(LunisolarDateError::MonthNotInYear),
        };

        let max_day = C::days_in_provided_month(year, month);
        if !(1..=max_day).contains(&day) {
            return Err(LunisolarDateError::InvalidDay { max: max_day });
        }

        // date is in the valid year range, and therefore in the valid RD range
        Ok(Self::new_unchecked(year, month, day))
    }

    pub(crate) fn from_fields(
        fields: DateFields,
        options: DateFromFieldsOptions,
        calendar: &C,
    ) -> Result<Self, DateFromFieldsError> {
        let missing_fields_strategy = options.missing_fields_strategy.unwrap_or_default();

        // DateFromFieldsOptions documents the default as reject
        let overflow = options.overflow.unwrap_or(Overflow::Reject);

        let day = match fields.day {
            Some(day) => day,
            None => match missing_fields_strategy {
                MissingFieldsStrategy::Reject => return Err(DateFromFieldsError::NotEnoughFields),
                MissingFieldsStrategy::Ecma => {
                    if fields.extended_year.is_some() || fields.era_year.is_some() {
                        // The ECMAScript strategy is to pick day 1, always, regardless of whether
                        // that day exists for the month/year combo
                        1
                    } else {
                        return Err(DateFromFieldsError::NotEnoughFields);
                    }
                }
            },
        };

        // We're returning these error early so that we return structural type
        // errors before range errors, see comment in the year code below.
        if fields.month_code.is_none() && fields.ordinal_month.is_none() && fields.month.is_none() {
            return Err(DateFromFieldsError::NotEnoughFields);
        }
        if fields.month_code.is_some() && fields.month.is_some() {
            return Err(DateFromFieldsError::TooManyFields);
        }

        let mut valid_month = None;

        // To better match the Temporal specification's order of operations, we try
        // to return structural type errors (`NotEnoughFields`) before checking for range errors.
        // This isn't behavior we *must* have, but it is not much additional work to maintain
        // so we make an attempt.
        let year = match (fields.era, fields.era_year) {
            (None, None) => match fields.extended_year {
                Some(extended_year) => {
                    if !GENEROUS_YEAR_RANGE.contains(&extended_year) {
                        return Err(DateFromFieldsError::Overflow);
                    }
                    calendar.year_info_from_extended(extended_year)
                }
                None => match missing_fields_strategy {
                    MissingFieldsStrategy::Reject => {
                        return Err(DateFromFieldsError::NotEnoughFields)
                    }
                    MissingFieldsStrategy::Ecma => {
                        let (m, d) = match (fields.month, fields.month_code, fields.ordinal_month) {
                            (Some(month), _, None) => (month, day),
                            (_, Some(month_code), None) => {
                                let validated = Month::try_from_utf8(month_code)?;
                                valid_month = Some(validated);
                                (validated, day)
                            }
                            _ => return Err(DateFromFieldsError::NotEnoughFields),
                        };
                        let ref_year = calendar.reference_year_from_month_day(m, d);
                        if ref_year.err() == Some(EcmaReferenceYearError::UseRegularIfConstrain)
                            && overflow == Overflow::Constrain
                        {
                            let new_valid_month = Month::new(m.number());
                            valid_month = Some(new_valid_month);
                            calendar.reference_year_from_month_day(new_valid_month, d)?
                        } else {
                            ref_year?
                        }
                    }
                },
            },
            (Some(era), Some(era_year)) => {
                if !GENEROUS_YEAR_RANGE.contains(&era_year) {
                    return Err(DateFromFieldsError::Overflow);
                }
                let extended_year =
                    calendar.extended_year_from_era_year_unchecked(era, era_year)?;
                let year = calendar.year_info_from_extended(extended_year);
                if let Some(extended_year) = fields.extended_year {
                    if calendar.extended_from_year_info(year) != extended_year {
                        return Err(DateFromFieldsError::InconsistentYear);
                    }
                }
                year
            }
            // Era and Era Year must be both or neither
            (Some(_), None) | (None, Some(_)) => return Err(DateFromFieldsError::NotEnoughFields),
        };

        let month = match (fields.month_code, fields.month) {
            (_, Some(month)) => {
                let computed_month = calendar.ordinal_from_month(year, month, overflow)?;
                if let Some(ordinal_month) = fields.ordinal_month {
                    if computed_month != ordinal_month {
                        return Err(DateFromFieldsError::InconsistentMonth);
                    }
                }
                computed_month
            }
            (Some(month_code), _) => {
                let validated = match valid_month {
                    Some(validated) => validated,
                    None => Month::try_from_utf8(month_code)?,
                };
                let computed_month = calendar.ordinal_from_month(year, validated, overflow)?;
                if let Some(ordinal_month) = fields.ordinal_month {
                    if computed_month != ordinal_month {
                        return Err(DateFromFieldsError::InconsistentMonth);
                    }
                }
                computed_month
            }
            (None, None) => match fields.ordinal_month {
                Some(month) => month,
                None => {
                    debug_assert!(false, "Already checked above");
                    return Err(DateFromFieldsError::NotEnoughFields);
                }
            },
        };

        let max_month = C::months_in_provided_year(year);
        let month = if matches!(overflow, Overflow::Constrain) {
            month.clamp(1, max_month)
        } else if (1..=max_month).contains(&month) {
            month
        } else {
            return Err(DateFromFieldsError::InvalidOrdinalMonth { max: max_month });
        };

        let max_day = C::days_in_provided_month(year, month);
        let day = if matches!(overflow, Overflow::Constrain) {
            day.clamp(1, max_day)
        } else if (1..=max_day).contains(&day) {
            day
        } else {
            return Err(DateFromFieldsError::InvalidDay { max: max_day });
        };
        let rd = C::to_rata_die_inner(year, month, day);

        // We early checked for a generous range of years, now we must check
        // to ensure we are actually in range for our core invariant.
        if !VALID_RD_RANGE.contains(&rd) {
            return Err(DateFromFieldsError::Overflow);
        }
        // We just checked the RD range above
        Ok(Self::new_unchecked(year, month, day))
    }

    // Used by calendar-specific constructors (non-lunisolar), checks `CONSTRUCTOR_YEAR_RANGE`
    pub(crate) fn from_year_month_day(
        year: i32,
        month: u8,
        day: u8,
        cal: &C,
    ) -> Result<Self, RangeError> {
        range_check(year, "year", CONSTRUCTOR_YEAR_RANGE)?;
        let year_info = cal.year_info_from_extended(year);
        range_check(month, "month", 1..=C::months_in_provided_year(year_info))?;
        range_check(day, "day", 1..=C::days_in_provided_month(year_info, month))?;
        // date is in the valid year range, and therefore in the valid RD range
        Ok(ArithmeticDate::new_unchecked(year_info, month, day))
    }

    // Used by calendar-specific constructors (Japanese), checks `CONSTRUCTOR_YEAR_RANGE`
    pub(crate) fn from_era_year_month_day(
        era: &str,
        year: i32,
        month: u8,
        day: u8,
        cal: &C,
    ) -> Result<Self, DateError> {
        range_check(year, "year", CONSTRUCTOR_YEAR_RANGE)?;
        let extended_year = cal.extended_year_from_era_year_unchecked(era.as_bytes(), year)?;
        let year_info = cal.year_info_from_extended(extended_year);
        range_check(month, "month", 1..=C::months_in_provided_year(year_info))?;
        range_check(day, "day", 1..=C::days_in_provided_month(year_info, month))?;
        // date is in the valid year range, and therefore in the valid RD range
        Ok(ArithmeticDate::new_unchecked(year_info, month, day))
    }

    /// Implements the Temporal abstract operation `BalanceNonISODate`.
    ///
    /// This takes a year, month, and day, where the month and day might be out of range, then
    /// balances excess months into the year field and excess days into the month field.
    ///
    /// In addition to specced behavior, this guarantees that it will produce an in-generous-year-range date.
    ///
    /// This does *not* necessarily produce something within [`VALID_RD_RANGE`], but it will ensure things
    /// are within [`GENEROUS_YEAR_RANGE`].
    fn new_balanced(
        year: C::YearInfo,
        ordinal_month: i32,
        day: i32,
        cal: &C,
    ) -> UncheckedArithmeticDate<C> {
        // 1. Let _resolvedYear_ be _arithmeticYear_.
        // 1. Let _resolvedMonth_ be _ordinalMonth_.
        let mut resolved_year = year;
        let mut resolved_month = ordinal_month;
        // 1. Let _monthsInYear_ be CalendarMonthsInYear(_calendar_, _resolvedYear_).
        let mut months_in_year = C::months_in_provided_year(resolved_year);
        // 1. Repeat, while _resolvedMonth_ &le; 0,
        //   1. Set _resolvedYear_ to _resolvedYear_ - 1.
        //   1. Set _monthsInYear_ to CalendarMonthsInYear(_calendar_, _resolvedYear_).
        //   1. Set _resolvedMonth_ to _resolvedMonth_ + _monthsInYear_.
        while resolved_month <= 0 {
            resolved_year =
                cal.year_info_from_extended(cal.extended_from_year_info(resolved_year) - 1);
            months_in_year = C::months_in_provided_year(resolved_year);
            resolved_month += i32::from(months_in_year);
        }
        // 1. Repeat, while _resolvedMonth_ &gt; _monthsInYear_,
        //   1. Set _resolvedMonth_ to _resolvedMonth_ - _monthsInYear_.
        //   1. Set _resolvedYear_ to _resolvedYear_ + 1.
        //   1. Set _monthsInYear_ to CalendarMonthsInYear(_calendar_, _resolvedYear_).
        while resolved_month > i32::from(months_in_year) {
            resolved_month -= i32::from(months_in_year);
            resolved_year =
                cal.year_info_from_extended(cal.extended_from_year_info(resolved_year) + 1);
            months_in_year = C::months_in_provided_year(resolved_year);
        }
        debug_assert!(u8::try_from(resolved_month).is_ok());
        let mut resolved_month = resolved_month as u8;
        // 1. Let _resolvedDay_ be _day_.
        let mut resolved_day = day;
        // 1. Let _daysInMonth_ be CalendarDaysInMonth(_calendar_, _resolvedYear_, _resolvedMonth_).
        let mut days_in_month = C::days_in_provided_month(resolved_year, resolved_month);
        // 1. Repeat, while _resolvedDay_ &le; 0,
        while resolved_day <= 0 {
            //   1. Set _resolvedMonth_ to _resolvedMonth_ - 1.
            //   1. If _resolvedMonth_ is 0, then
            resolved_month -= 1;
            if resolved_month == 0 {
                //     1. Set _resolvedYear_ to _resolvedYear_ - 1.
                //     1. Set _monthsInYear_ to CalendarMonthsInYear(_calendar_, _resolvedYear_).
                //     1. Set _resolvedMonth_ to _monthsInYear_.
                resolved_year =
                    cal.year_info_from_extended(cal.extended_from_year_info(resolved_year) - 1);
                months_in_year = C::months_in_provided_year(resolved_year);
                resolved_month = months_in_year;
            }
            //   1. Set _daysInMonth_ to CalendarDaysInMonth(_calendar_, _resolvedYear_, _resolvedMonth_).
            //   1. Set _resolvedDay_ to _resolvedDay_ + _daysInMonth_.
            days_in_month = C::days_in_provided_month(resolved_year, resolved_month);
            resolved_day += i32::from(days_in_month);
        }
        // 1. Repeat, while _resolvedDay_ &gt; _daysInMonth_,
        while resolved_day > i32::from(days_in_month) {
            //   1. Set _resolvedDay_ to _resolvedDay_ - _daysInMonth_.
            //   1. Set _resolvedMonth_ to _resolvedMonth_ + 1.
            //   1. If _resolvedMonth_ &gt; _monthsInYear_, then
            resolved_day -= i32::from(days_in_month);
            resolved_month += 1;
            if resolved_month > months_in_year {
                //     1. Set _resolvedYear_ to _resolvedYear_ + 1.
                //     1. Set _monthsInYear_ to CalendarMonthsInYear(_calendar_, _resolvedYear_).
                //     1. Set _resolvedMonth_ to 1.
                resolved_year =
                    cal.year_info_from_extended(cal.extended_from_year_info(resolved_year) + 1);
                months_in_year = C::months_in_provided_year(resolved_year);
                resolved_month = 1;
            }
            //   1. Set _daysInMonth_ to CalendarDaysInMonth(_calendar_, _resolvedYear_, _resolvedMonth_).
            days_in_month = C::days_in_provided_month(resolved_year, resolved_month);
        }
        debug_assert!(u8::try_from(resolved_day).is_ok());
        let resolved_day = resolved_day as u8;
        // 1. Return the Record { [[Year]]: _resolvedYear_, [[Month]]: _resolvedMonth_, [[Day]]: _resolvedDay_ }.
        UncheckedArithmeticDate {
            year: resolved_year,
            ordinal_month: resolved_month,
            day: resolved_day,
        }
    }

    /// Implements the Temporal abstract operation `CompareSurpasses` based on month code
    #[allow(clippy::collapsible_if, clippy::collapsible_else_if)] // to match the spec
    fn compare_surpasses_lexicographic(
        sign: i32,
        year: C::YearInfo,
        month: Month,
        day: u8,
        target: &Self,
        cal: &C,
    ) -> bool {
        // 1. If _year_ ≠ _target_.[[Year]], then
        //   1. If _sign_ × (_year_ - _target_.[[Year]]) > 0, return *true*.
        // 1. Else if _monthCode_ ≠ _target_.[[MonthCode]], then
        //   1. If _sign > 0, then
        //     1. If _monthCode_ is lexicographically greater than _target_.[[MonthCode]], return *true*.
        //   1. Else,
        //     1. If _target_.[[MonthCode]] is lexicographically greater than _monthCode_, return *true*.
        // (note: integer steps omitted)
        // 1. Else if _day_ ≠ _target_.[[Day]], then
        //   1. If _sign_ × (_day_ - _target_.[[Day]]) > 0, return *true*.
        if year != target.year() {
            if sign * (year - target.year()) > 0 {
                return true;
            }
        } else {
            let target_month = cal.month_from_ordinal(target.year(), target.month());
            if month != target_month {
                if sign > 0 {
                    if month > target_month {
                        return true;
                    }
                } else {
                    if month <= target_month {
                        return true;
                    }
                }
            } else if day != target.day() {
                if sign * (i32::from(day) - i32::from(target.day())) > 0 {
                    return true;
                }
            }
        }
        false
    }

    /// Implements the Temporal abstract operation `CompareSurpasses` based on month ordinal
    #[allow(clippy::collapsible_if, clippy::collapsible_else_if)] // to match the spec
    fn compare_surpasses_ordinal(
        sign: i32,
        year: C::YearInfo,
        month: u8,
        day: u8,
        target: &Self,
    ) -> bool {
        // 1. If _year_ ≠ _target_.[[Year]], then
        //   1. If _sign_ × (_year_ - _target_.[[Year]]) > 0, return *true*.
        // (note: month code steps omitted)
        // 1. Else if _month_ ≠ _target_.[[Month]], then
        //   1. If _sign_ × (_month_ - _target_.[[Month]]) > 0, return *true*.
        // 1. Else if _day_ ≠ _target_.[[Day]], then
        //   1. If _sign_ × (_day_ - _target_.[[Day]]) > 0, return *true*.
        if year != target.year() {
            if sign * (year - target.year()) > 0 {
                return true;
            }
        } else if month != target.month() {
            if sign * (i32::from(month) - i32::from(target.month())) > 0 {
                return true;
            }
        } else if day != target.day() {
            if sign * (i32::from(day) - i32::from(target.day())) > 0 {
                return true;
            }
        }
        false
    }

    /// Implements the Temporal abstract operation `NonISODateSurpasses`.
    ///
    /// This takes two dates (`self` and `other`), `duration`, and `sign` (either -1 or 1), then
    /// returns whether adding the duration to `self` results in a year/month/day that exceeds
    /// `other` in the direction indicated by `sign`, constraining the month but not the day.
    ///
    /// Note: This function is no longer used, but the code is retained for
    /// reference. The `until()` implementation uses `SurpassesChecker`, which
    /// implements the same spec, but with better performance.
    #[allow(dead_code)] // TODO: remove surpasses() method when no longer needed
    pub(crate) fn surpasses(
        &self,
        other: &Self,
        duration: DateDuration,
        sign: i32,
        cal: &C,
    ) -> bool {
        // 1. Let _parts_ be CalendarISOToDate(_calendar_, _fromIsoDate_).
        let parts = self;
        // 1. Let _calDate2_ be CalendarISOToDate(_calendar_, _toIsoDate_).
        let cal_date_2 = other;
        // 1. Let _y0_ be _parts_.[[Year]] + _years_.
        let y0 = cal.year_info_from_extended(
            duration.add_years_to(cal.extended_from_year_info(parts.year())),
        );
        // 1. If CompareSurpasses(_sign_, _y0_, _parts_.[[MonthCode]], _parts_.[[Day]], _calDate2_) is *true*, return *true*.
        let base_month = cal.month_from_ordinal(parts.year(), parts.month());
        if Self::compare_surpasses_lexicographic(sign, y0, base_month, parts.day(), cal_date_2, cal)
        {
            return true;
        }
        // 1. Let _m0_ be MonthCodeToOrdinal(_calendar_, _y0_, ! ConstrainMonthCode(_calendar_, _y0_, _parts_.[[MonthCode]], ~constrain~)).
        let m0_result = cal.ordinal_from_month(y0, base_month, Overflow::Constrain);
        let m0 = match m0_result {
            Ok(m0) => m0,
            Err(_) => {
                debug_assert!(
                    false,
                    "valid month code for calendar, and constrained to the year"
                );
                1
            }
        };
        // 1. Let _monthsAdded_ be BalanceNonISODate(_calendar_, _y0_, _m0_ + _months_, 1).
        let months_added = Self::new_balanced(y0, duration.add_months_to(m0), 1, cal);

        // 1. If CompareSurpasses(_sign_, _monthsAdded_.[[Year]], _monthsAdded_.[[Month]], _parts_.[[Day]], _calDate2_) is *true*, return *true*.
        if Self::compare_surpasses_ordinal(
            sign,
            months_added.year,
            months_added.ordinal_month,
            parts.day(),
            cal_date_2,
        ) {
            return true;
        }
        // 1. If _weeks_ = 0 and _days_ = 0, return *false*.
        if duration.weeks == 0 && duration.days == 0 {
            return false;
        }
        // 1. Let _endOfMonth_ be BalanceNonISODate(_calendar_, _monthsAdded_.[[Year]], _monthsAdded_.[[Month]] + 1, 0).
        let end_of_month = Self::new_balanced(
            months_added.year,
            i32::from(months_added.ordinal_month + 1),
            0,
            cal,
        );
        // 1. Let _baseDay_ be _parts_.[[Day]].
        let base_day = parts.day();
        // 1. If _baseDay_ &le; _endOfMonth_.[[Day]], then
        //   1. Let _regulatedDay_ be _baseDay_.
        // 1. Else,
        //   1. Let _regulatedDay_ be _endOfMonth_.[[Day]].
        let regulated_day = if base_day < end_of_month.day {
            base_day
        } else {
            end_of_month.day
        };
        // 1. Let _daysInWeek_ be 7 (the number of days in a week for all supported calendars).
        // 1. Let _balancedDate_ be BalanceNonISODate(_calendar_, _endOfMonth_.[[Year]], _endOfMonth_.[[Month]], _regulatedDay_ + _daysInWeek_ * _weeks_ + _days_).
        // 1. Return CompareSurpasses(_sign_, _balancedDate_.[[Year]], _balancedDate_.[[Month]], _balancedDate_.[[Day]], _calDate2_).
        let balanced_date = Self::new_balanced(
            end_of_month.year,
            i32::from(end_of_month.ordinal_month),
            duration.add_weeks_and_days_to(regulated_day),
            cal,
        );

        Self::compare_surpasses_ordinal(
            sign,
            balanced_date.year,
            balanced_date.ordinal_month,
            balanced_date.day,
            cal_date_2,
        )
    }

    /// Implements the Temporal abstract operation `NonISODateAdd`.
    ///
    /// This takes a date (`self`) and `duration`, then returns a new date resulting from
    /// adding `duration` to `self`, constrained according to `options`.
    pub(crate) fn added(
        &self,
        duration: DateDuration,
        cal: &C,
        options: DateAddOptions,
    ) -> Result<Self, DateAddError> {
        // We preemptively protect ourselves from overly-large values
        //
        // This is mostly needed for avoiding overflows on Duration arithmetic,
        // this could also be handled by updating DateDuration::add_foo_to()
        // to return an error. However, that would lead to more error handling cluttering
        // surpasses().

        if duration.years > GENEROUS_MAX_YEARS
            || duration.months > GENEROUS_MAX_MONTHS
            || duration
                .weeks
                .saturating_mul(7)
                .saturating_add(duration.days)
                > GENEROUS_MAX_DAYS
        {
            return Err(DateAddError::Overflow);
        }

        // DateAddOptions documents the default as constrain
        let overflow = options.overflow.unwrap_or(Overflow::Constrain);

        // 1. Let _parts_ be CalendarISOToDate(_calendar_, _isoDate_).
        // 1. Let _y0_ be _parts_.[[Year]] + _duration_.[[Years]].
        let extended_year = duration.add_years_to(cal.extended_from_year_info(self.year()));
        if !GENEROUS_YEAR_RANGE.contains(&extended_year) {
            return Err(DateAddError::Overflow);
        }
        let y0 = cal.year_info_from_extended(extended_year);
        // 1. Let _m0_ be MonthCodeToOrdinal(_calendar_, _y0_, ! ConstrainMonthCode(_calendar_, _y0_, _parts_.[[MonthCode]], _overflow_)).
        let base_month = cal.month_from_ordinal(self.year(), self.month());
        let m0_result = cal.ordinal_from_month(y0, base_month, overflow);
        let m0 = match m0_result {
            Ok(m0) => m0,
            Err(MonthError::NotInCalendar) => {
                debug_assert!(
                    false,
                    "Should never get NotInCalendar when performing arithmetic"
                );
                return Err(DateAddError::MonthNotInYear);
            }
            Err(MonthError::NotInYear) => return Err(DateAddError::MonthNotInYear),
        };
        // 1. Let _endOfMonth_ be BalanceNonISODate(_calendar_, _y0_, _m0_ + _duration_.[[Months]] + 1, 0).
        let end_of_month = Self::new_balanced(y0, duration.add_months_to(m0) + 1, 0, cal);
        // 1. Let _baseDay_ be _parts_.[[Day]].
        let base_day = self.day();
        // 1. If _baseDay_ &le; _endOfMonth_.[[Day]], then
        //   1. Let _regulatedDay_ be _baseDay_.
        let regulated_day = if base_day <= end_of_month.day {
            base_day
        } else {
            // 1. Else,
            //   1. If _overflow_ is ~reject~, throw a *RangeError* exception.
            // Note: ICU4X default is constrain here
            if overflow == Overflow::Reject {
                return Err(DateAddError::InvalidDay {
                    max: end_of_month.day,
                });
            }
            end_of_month.day
        };

        // 1. Let _balancedDate_ be BalanceNonISODate(_calendar_, _endOfMonth_.[[Year]], _endOfMonth_.[[Month]], _regulatedDay_ + 7 * _duration_.[[Weeks]] + _duration_.[[Days]]).
        // 1. Let _result_ be ? CalendarIntegersToISO(_calendar_, _balancedDate_.[[Year]], _balancedDate_.[[Month]], _balancedDate_.[[Day]]).
        // 1. Return _result_.
        let balanced = Self::new_balanced(
            end_of_month.year,
            i32::from(end_of_month.ordinal_month),
            duration.add_weeks_and_days_to(regulated_day),
            cal,
        );

        balanced.into_checked().ok_or(DateAddError::Overflow)
    }

    /// Implements the Temporal abstract operation `NonISODateUntil`.
    ///
    /// This takes two dates (`self` and `other`), then returns a duration that, when
    /// added to `self`, results in `other`, with largest unit according to `options`.
    pub(crate) fn until(
        &self,
        other: &Self,
        cal: &C,
        options: DateDifferenceOptions,
    ) -> DateDuration {
        // Non-spec optimization: fast path for day/week diffs
        // Avoids quadratic behavior in surpasses() for days/weeks
        if matches!(
            options.largest_unit,
            Some(DateDurationUnit::Days) | Some(DateDurationUnit::Weeks)
        ) {
            let from = self.to_rata_die();
            let to = other.to_rata_die();
            let diff = to - from;
            let diff = match i32::try_from(diff) {
                Ok(d) => d,
                Err(_) => {
                    debug_assert!(false, "rata die diff out of i32 range");
                    if diff > 0 {
                        i32::MAX
                    } else {
                        i32::MIN
                    }
                }
            };
            if matches!(options.largest_unit, Some(DateDurationUnit::Weeks)) {
                return DateDuration::for_weeks_and_days(diff);
            } else {
                return DateDuration::for_days(diff);
            }
        }

        // 1. Let sign be -1 × CompareISODate(one, two).
        // 2. If sign = 0, return ZeroDateDuration().
        let sign = match other.cmp(self) {
            Ordering::Greater => 1,
            Ordering::Equal => return DateDuration::default(),
            Ordering::Less => -1,
        };

        // Non-spec optimization: Instead of calling a stateless
        // NonISODateSurpasses implementation that repeatedly recomputes many
        // values, we use a stateful implementation. It caches intermediate
        // produced by NonISODateSurpasses and reuses them in subsequent calls,
        // to avoid unnecessary recalculations.
        let mut surpasses_checker: SurpassesChecker<'_, C> =
            SurpassesChecker::new(self, other, sign, cal);

        // Preparation for non-specced optimization:
        // We don't want to spend time incrementally bumping it up one year
        // at a time, so let's pre-guess a year delta that is guaranteed to not
        // surpass.
        let year_diff = other.year() - self.year();
        let min_years = if year_diff == 0 { 0 } else { year_diff - sign };

        // clippy rejects: debug_assert!(!surpasses_checker.surpasses_months(min_months));
        #[cfg(debug_assertions)]
        {
            let mut debug_checker = SurpassesChecker::new(self, other, sign, cal);
            let min_years_valid = !debug_checker.surpasses_years(min_years);
            debug_assert!(min_years_valid);
        }

        // 3. Let years be 0.
        // 4. If largestUnit is year, then
        //   a. Let candidateYears be sign.
        //   b. Repeat, while NonISODateSurpasses(calendar, sign, one, two, candidateYears, 0, 0, 0) is false,
        //     i. Set years to candidateYears.
        //     ii. Set candidateYears to candidateYears + sign.
        let mut years = 0;
        if matches!(options.largest_unit, Some(DateDurationUnit::Years)) {
            let mut candidate_years = sign;

            // Non-spec optimization: try to fast-forward candidate_years to
            // a year value that does not surpass
            if min_years != 0 {
                candidate_years = min_years
            };

            while !surpasses_checker.surpasses_years(candidate_years) {
                years = candidate_years;
                candidate_years += sign;
            }
        }
        // Freeze the checker's Years field state.
        surpasses_checker.set_years(years);

        // 5. Let months be 0.
        // 6. If largestUnit is year or largestUnit is month, then
        //   a. Let candidateMonths be sign.
        //   b. Repeat, while NonISODateSurpasses(calendar, sign, one, two, years, candidateMonths, 0, 0) is false,
        //     i. Set months to candidateMonths.
        //     ii. Set candidateMonths to candidateMonths + sign.
        let mut months = 0;
        if matches!(
            options.largest_unit,
            Some(DateDurationUnit::Years) | Some(DateDurationUnit::Months)
        ) {
            let mut candidate_months = sign;

            // Non-spec optimization: try to fast-forward candidate_months to
            // a month value that does not surpass
            if options.largest_unit == Some(DateDurationUnit::Months) && min_years != 0 {
                // If largest_unit = Months, then compute the calendar-specific minimum number of
                // months corresponding to min_years.
                let min_months = C::min_months_from(self.year(), min_years);
                // clippy rejects: debug_assert!(!surpasses_checker.surpasses_months(min_months));
                #[cfg(debug_assertions)]
                {
                    let mut debug_checker = SurpassesChecker::new(self, other, sign, cal);
                    debug_checker.surpasses_years(years);
                    let min_months_valid = !debug_checker.surpasses_months(min_months);
                    debug_assert!(min_months_valid);
                }
                candidate_months = min_months
            }

            while !surpasses_checker.surpasses_months(candidate_months) {
                months = candidate_months;
                candidate_months += sign;
            }
        }
        // Freeze the checker's Months field state.
        surpasses_checker.set_months(months);

        // 7. Let weeks be 0.
        // 8. If largestUnit is week, then
        //   a. Let candidateWeeks be sign.
        //   b. Repeat, while NonISODateSurpasses(calendar, sign, one, two, years, months, candidateWeeks, 0) is false,
        //     i. Set weeks to candidateWeeks.
        //     ii. Set candidateWeeks to candidateWeeks + sign.
        let mut weeks = 0;
        if matches!(options.largest_unit, Some(DateDurationUnit::Weeks)) {
            let mut candidate_weeks = sign;
            while !surpasses_checker.surpasses_weeks(candidate_weeks) {
                weeks = candidate_weeks;
                candidate_weeks += sign;
            }
        }
        // Freeze the checker's Weeks field state.
        surpasses_checker.set_weeks(weeks);

        // 9. Let days be 0.
        // 10. Let candidateDays be sign.
        // 11. Repeat, while NonISODateSurpasses(calendar, sign, one, two, years, months, weeks, candidateDays) is false,
        //   a. Set days to candidateDays.
        //   b. Set candidateDays to candidateDays + sign.
        let mut days = 0;
        // There is no pressing need to optimize candidate_days here: the early-return RD arithmetic
        // optimization will be hit if the largest_unit is weeks/days, and if it is months or years we will
        // arrive here with a candidate date that is at most 31 days off. We can run this loop 31 times.
        let mut candidate_days = sign;
        while !surpasses_checker.surpasses_days(candidate_days) {
            days = candidate_days;
            candidate_days += sign;
        }

        // 12. Return ! CreateDateDurationRecord(years, months, weeks, days).
        DateDuration::from_signed_ymwd(years, months, weeks, days)
    }
}

/// Stateful checker for iterative per-field `surpasses()` checks
///
/// This checker for `surpasses()` is designed to iteratively evaluate one field
/// at a time, from largest to smallest. After each field value is determined,
/// the caller must cache the computed state of the field's value by calling
/// `set_<field>(<field_value>)` before moving on to the next smaller field.
struct SurpassesChecker<'a, C: DateFieldsResolver> {
    parts: &'a ArithmeticDate<C>,
    cal_date_2: &'a ArithmeticDate<C>,
    sign: i32,
    cal: &'a C,
    y0: <C as DateFieldsResolver>::YearInfo,
    m0: u8,
    end_of_month: UncheckedArithmeticDate<C>,
    regulated_day: u8,
    weeks: i32,
}

impl<'a, C: DateFieldsResolver> SurpassesChecker<'a, C> {
    fn new(
        parts: &'a ArithmeticDate<C>,
        cal_date_2: &'a ArithmeticDate<C>,
        sign: i32,
        cal: &'a C,
    ) -> Self {
        Self {
            parts,
            cal_date_2,
            sign,
            cal,
            y0: cal.year_info_from_extended(0),
            m0: 0,
            end_of_month: UncheckedArithmeticDate {
                year: cal.year_info_from_extended(0),
                ordinal_month: 0,
                day: 0,
            },
            regulated_day: 0,
            weeks: 0,
        }
    }

    // NOTE: Numbered comments refer to the Temporal `NonISODateSurpasses`
    // spec. A design goal of this code is to mirror the spec as closely as
    // possible. This dictates the names of variables and the sequence of
    // operations.

    fn surpasses_years(&mut self, years: i32) -> bool {
        // 1. Let parts be CalendarISOToDate(calendar, fromIsoDate).
        // 2. Let calDate2 be CalendarISOToDate(calendar, toIsoDate).
        // 3. Let y0 be parts.[[Year]] + years.
        self.y0 = self
            .cal
            .year_info_from_extended(self.cal.extended_from_year_info(self.parts.year()) + years);
        // 4. If CompareSurpasses(sign, y0, parts.[[MonthCode]], parts.[[Day]], calDate2) is true, return true.
        let base_month = self
            .cal
            .month_from_ordinal(self.parts.year(), self.parts.month());
        let surpasses_years = ArithmeticDate::<C>::compare_surpasses_lexicographic(
            self.sign,
            self.y0,
            base_month,
            self.parts.day(),
            self.cal_date_2,
            self.cal,
        );
        // 5. Let m0 be MonthCodeToOrdinal(calendar, y0, ! ConstrainMonthCode(calendar, y0, parts.[[MonthCode]], constrain)).

        let m0_result = self
            .cal
            .ordinal_from_month(self.y0, base_month, Overflow::Constrain);
        self.m0 = match m0_result {
            Ok(m0) => m0,
            Err(_) => {
                debug_assert!(
                    false,
                    "valid month code for calendar, and constrained to the year"
                );
                1
            }
        };

        // If the lexicographic check returns false, the spec continues to the
        // ordinal check, which is implemented in `surpasses_months`.
        surpasses_years || self.surpasses_months(0)
    }

    #[inline]
    fn set_years(&mut self, years: i32) {
        self.surpasses_years(years);
    }

    fn surpasses_months(&mut self, months: i32) -> bool {
        // 6. Let monthsAdded be BalanceNonISODate(calendar, y0, m0 + months, 1).
        let months_added =
            ArithmeticDate::<C>::new_balanced(self.y0, months + i32::from(self.m0), 1, self.cal);

        // 7. If CompareSurpasses(sign, monthsAdded.[[Year]], monthsAdded.[[Month]], parts.[[Day]], calDate2) is true, return true.
        // We do not need to perform any other checks because step 8 of the spec
        // guarantees an early return if weeks and days are 0.
        ArithmeticDate::<C>::compare_surpasses_ordinal(
            self.sign,
            months_added.year,
            months_added.ordinal_month,
            self.parts.day(),
            self.cal_date_2,
        )
    }

    fn set_months(&mut self, months: i32) {
        // 6. Let monthsAdded be BalanceNonISODate(calendar, y0, m0 + months, 1).
        let months_added =
            ArithmeticDate::<C>::new_balanced(self.y0, months + i32::from(self.m0), 1, self.cal);

        // 9. Let endOfMonth be BalanceNonISODate(calendar, monthsAdded.[[Year]], monthsAdded.[[Month]] + 1, 0).
        self.end_of_month = ArithmeticDate::<C>::new_balanced(
            months_added.year,
            i32::from(months_added.ordinal_month) + 1,
            0,
            self.cal,
        );
        // 10. Let baseDay be parts.[[Day]].
        let base_day = self.parts.day();
        // 11. If baseDay ≤ endOfMonth.[[Day]], then
        //     a. Let regulatedDay be baseDay.
        // 12. Else,
        //     a. Let regulatedDay be endOfMonth.[[Day]].
        self.regulated_day = if base_day < self.end_of_month.day {
            base_day
        } else {
            self.end_of_month.day
        };
    }

    fn surpasses_weeks(&mut self, weeks: i32) -> bool {
        // 8. If weeks = 0 (*and days = 0*), return false.
        if weeks == 0 {
            return false;
        }

        // 13. Let daysInWeek be 7 (the number of days in a week for all supported calendars).
        // 14. Let balancedDate be BalanceNonISODate(calendar, endOfMonth.[[Year]], endOfMonth.[[Month]], regulatedDay + daysInWeek * weeks + days).
        // 15. Return CompareSurpasses(sign, balancedDate.[[Year]], balancedDate.[[Month]], balancedDate.[[Day]], calDate2).
        let balanced_date = ArithmeticDate::<C>::new_balanced(
            self.end_of_month.year,
            i32::from(self.end_of_month.ordinal_month),
            7 * weeks + i32::from(self.regulated_day),
            self.cal,
        );

        self.weeks = weeks;

        ArithmeticDate::<C>::compare_surpasses_ordinal(
            self.sign,
            balanced_date.year,
            balanced_date.ordinal_month,
            balanced_date.day,
            self.cal_date_2,
        )
    }

    #[inline]
    fn set_weeks(&mut self, weeks: i32) {
        self.surpasses_weeks(weeks);
    }

    fn surpasses_days(&mut self, days: i32) -> bool {
        // 8. If weeks = 0 and days = 0, return false.
        if self.weeks == 0 && days == 0 {
            return false;
        }

        // 13. Let daysInWeek be 7 (the number of days in a week for all supported calendars).
        // 14. Let balancedDate be BalanceNonISODate(calendar, endOfMonth.[[Year]], endOfMonth.[[Month]], regulatedDay + daysInWeek * weeks + days).
        // 15. Return CompareSurpasses(sign, balancedDate.[[Year]], balancedDate.[[Month]], balancedDate.[[Day]], calDate2).
        let balanced_date = ArithmeticDate::<C>::new_balanced(
            self.end_of_month.year,
            i32::from(self.end_of_month.ordinal_month),
            7 * self.weeks + days + i32::from(self.regulated_day),
            self.cal,
        );

        ArithmeticDate::<C>::compare_surpasses_ordinal(
            self.sign,
            balanced_date.year,
            balanced_date.ordinal_month,
            balanced_date.day,
            self.cal_date_2,
        )
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{
        cal::{coptic::CopticYear, *},
        Date,
    };

    #[test]
    fn test_ord() {
        let new_coptic = |coptic_year, month, day| {
            ArithmeticDate::<Coptic>::new_unchecked(
                CopticYear::from_coptic_anno_martyrum_year(coptic_year),
                month,
                day,
            )
        };
        let dates_in_order = [
            new_coptic(-10, 1, 1),
            new_coptic(-10, 1, 2),
            new_coptic(-10, 2, 1),
            new_coptic(-1, 1, 1),
            new_coptic(-1, 1, 2),
            new_coptic(-1, 2, 1),
            new_coptic(0, 1, 1),
            new_coptic(0, 1, 2),
            new_coptic(0, 2, 1),
            new_coptic(1, 1, 1),
            new_coptic(1, 1, 2),
            new_coptic(1, 2, 1),
            new_coptic(10, 1, 1),
            new_coptic(10, 1, 2),
            new_coptic(10, 2, 1),
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

    #[test]
    fn test_validity_ranges() {
        #[rustfmt::skip]
        let lowest_years = [
            Date::from_rata_die(*VALID_RD_RANGE.start(), Buddhist).year().extended_year(),
            Date::from_rata_die(*VALID_RD_RANGE.start(), ChineseTraditional::new()).year().extended_year(),
            Date::from_rata_die(*VALID_RD_RANGE.start(), Coptic).year().extended_year(),
            Date::from_rata_die(*VALID_RD_RANGE.start(), Ethiopian::new_with_era_style(EthiopianEraStyle::AmeteAlem)).year().extended_year(),
            Date::from_rata_die(*VALID_RD_RANGE.start(), Ethiopian::new_with_era_style(EthiopianEraStyle::AmeteMihret)).year().extended_year(),
            Date::from_rata_die(*VALID_RD_RANGE.start(), Gregorian).year().extended_year(),
            Date::from_rata_die(*VALID_RD_RANGE.start(), Hebrew).year().extended_year(),
            Date::from_rata_die(*VALID_RD_RANGE.start(), Hijri::new_umm_al_qura()).year().extended_year(),
            Date::from_rata_die(*VALID_RD_RANGE.start(), Hijri::new_tabular(HijriTabularLeapYears::TypeII, HijriTabularEpoch::Thursday)).year().extended_year(),
            Date::from_rata_die(*VALID_RD_RANGE.start(), Indian).year().extended_year(),
            Date::from_rata_die(*VALID_RD_RANGE.start(), Iso).year().extended_year(),
            Date::from_rata_die(*VALID_RD_RANGE.start(), Japanese::new()).year().extended_year(),
            Date::from_rata_die(*VALID_RD_RANGE.start(), Julian).year().extended_year(),
            Date::from_rata_die(*VALID_RD_RANGE.start(), KoreanTraditional::new()).year().extended_year(),
            Date::from_rata_die(*VALID_RD_RANGE.start(), Persian).year().extended_year(),
            Date::from_rata_die(*VALID_RD_RANGE.start(), Roc).year().extended_year(),
        ];

        #[rustfmt::skip]
        let highest_years = [
            Date::from_rata_die(*VALID_RD_RANGE.end(), Buddhist).year().extended_year(),
            Date::from_rata_die(*VALID_RD_RANGE.end(), ChineseTraditional::new()).year().extended_year(),
            Date::from_rata_die(*VALID_RD_RANGE.end(), Coptic).year().extended_year(),
            Date::from_rata_die(*VALID_RD_RANGE.end(), Ethiopian::new_with_era_style(EthiopianEraStyle::AmeteAlem)).year().extended_year(),
            Date::from_rata_die(*VALID_RD_RANGE.end(), Ethiopian::new_with_era_style(EthiopianEraStyle::AmeteMihret)).year().extended_year(),
            Date::from_rata_die(*VALID_RD_RANGE.end(), Gregorian).year().extended_year(),
            Date::from_rata_die(*VALID_RD_RANGE.end(), Hebrew).year().extended_year(),
            Date::from_rata_die(*VALID_RD_RANGE.end(), Hijri::new_umm_al_qura()).year().extended_year(),
            Date::from_rata_die(*VALID_RD_RANGE.end(), Hijri::new_tabular(HijriTabularLeapYears::TypeII, HijriTabularEpoch::Thursday)).year().extended_year(),
            Date::from_rata_die(*VALID_RD_RANGE.end(), Indian).year().extended_year(),
            Date::from_rata_die(*VALID_RD_RANGE.end(), Iso).year().extended_year(),
            Date::from_rata_die(*VALID_RD_RANGE.end(), Japanese::new()).year().extended_year(),
            Date::from_rata_die(*VALID_RD_RANGE.end(), Julian).year().extended_year(),
            Date::from_rata_die(*VALID_RD_RANGE.end(), KoreanTraditional::new()).year().extended_year(),
            Date::from_rata_die(*VALID_RD_RANGE.end(), Persian).year().extended_year(),
            Date::from_rata_die(*VALID_RD_RANGE.end(), Roc).year().extended_year(),
        ];

        // Valid RDs can represent all valid years
        assert!(lowest_years
            .iter()
            .all(|y| y <= CONSTRUCTOR_YEAR_RANGE.start()));
        assert!(highest_years
            .iter()
            .all(|y| y >= CONSTRUCTOR_YEAR_RANGE.end()));

        // All years are 21-bits
        assert!(-lowest_years.iter().copied().min().unwrap() < 1 << 20);
        assert!(highest_years.iter().copied().max().unwrap() < 1 << 20);
    }

    #[test]
    fn test_from_fields_consistent_years() {
        let fields = DateFields {
            extended_year: Some(0),
            era_year: Some(0),
            era: Some(b"be"),
            ordinal_month: Some(1),
            day: Some(1),
            ..Default::default()
        };

        let date = Date::try_from_fields(fields, Default::default(), Buddhist).unwrap();
        assert_eq!(date.year().extended_year(), 0);
    }
}
