// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! This module contains types and implementations for the Hijri calendars.
//!
//! ```rust
//! use icu::calendar::cal::HijriSimulated;
//! use icu::calendar::Date;
//!
//! let hijri = HijriSimulated::new_mecca_always_calculating();
//! let hijri_date =
//!     Date::try_new_simulated_hijri_with_calendar(1348, 10, 11, hijri)
//!         .expect("Failed to initialize Hijri Date instance.");
//!
//! assert_eq!(hijri_date.era_year().year, 1348);
//! assert_eq!(hijri_date.month().ordinal, 10);
//! assert_eq!(hijri_date.day_of_month().0, 11);
//! ```

use crate::cal::iso::{Iso, IsoDateInner};
use crate::calendar_arithmetic::PrecomputedDataSource;
use crate::calendar_arithmetic::{ArithmeticDate, CalendarArithmetic};
use crate::error::{year_check, DateError};
use crate::provider::hijri::PackedHijriYearInfo;
use crate::provider::hijri::{CalendarHijriSimulatedMeccaV1, HijriData};
use crate::types::EraYear;
use crate::{types, Calendar, Date, DateDuration, DateDurationUnit};
use crate::{AsCalendar, RangeError};
use calendrical_calculations::islamic::{ISLAMIC_EPOCH_FRIDAY, ISLAMIC_EPOCH_THURSDAY};
use calendrical_calculations::rata_die::RataDie;
use icu_provider::marker::ErasedMarker;
use icu_provider::prelude::*;
use tinystr::tinystr;
use ummalqura_data::{UMMALQURA_DATA, UMMALQURA_DATA_STARTING_YEAR};

mod ummalqura_data;

fn era_year(year: i32) -> EraYear {
    if year > 0 {
        types::EraYear {
            era: tinystr!(16, "ah"),
            era_index: Some(0),
            year,
            ambiguity: types::YearAmbiguity::CenturyRequired,
        }
    } else {
        types::EraYear {
            era: tinystr!(16, "bh"),
            era_index: Some(1),
            year: 1 - year,
            ambiguity: types::YearAmbiguity::CenturyRequired,
        }
    }
}

/// The [simulated Hijri Calendar](https://en.wikipedia.org/wiki/Islamic_calendar)
///
/// # Era codes
///
/// This calendar uses two era codes: `ah`, and `bh`, corresponding to the Anno Hegirae and Before Hijrah eras
///
/// # Month codes
///
/// This calendar is a pure lunar calendar with no leap months. It uses month codes
/// `"M01" - "M12"`.
#[derive(Clone, Debug)]
pub struct HijriSimulated {
    pub(crate) location: HijriSimulatedLocation,
    data: Option<DataPayload<ErasedMarker<HijriData<'static>>>>,
}

#[derive(Clone, Debug, Copy, PartialEq)]
pub(crate) enum HijriSimulatedLocation {
    Mecca,
}

impl HijriSimulatedLocation {
    fn location(self) -> calendrical_calculations::islamic::Location {
        match self {
            Self::Mecca => calendrical_calculations::islamic::MECCA,
        }
    }
}

/// The [Umm al-Qura Hijri Calendar](https://en.wikipedia.org/wiki/Islamic_calendar#Saudi_Arabia's_Umm_al-Qura_calendar)
///
/// This calendar is the official calendar in Saudi Arabia.
///
/// # Era codes
///
/// This calendar uses two era codes: `ah`, and `bh`, corresponding to the Anno Hegirae and Before Hijrah eras
///
/// # Month codes
///
/// This calendar is a pure lunar calendar with no leap months. It uses month codes
/// `"M01" - "M12"`.
#[derive(Clone, Debug, Default)]
#[non_exhaustive]
pub struct HijriUmmAlQura;

/// The [tabular Hijri Calendar](https://en.wikipedia.org/wiki/Tabular_Islamic_calendar).
///
/// See [`HijriTabularEpoch`] and [`HijriTabularLeapYears`] for customization.
///
/// The most common version of this calendar uses [`HijriTabularEpoch::Friday`] and [`HijriTabularLeapYears::TypeII`].
///
/// # Era codes
///
/// This calendar uses two era codes: `ah`, and `bh`, corresponding to the Anno Hegirae and Before Hijrah eras
///
/// # Month codes
///
/// This calendar is a pure lunar calendar with no leap months. It uses month codes
/// `"M01" - "M12"`.
#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord)]
pub struct HijriTabular {
    pub(crate) leap_years: HijriTabularLeapYears,
    pub(crate) epoch: HijriTabularEpoch,
}

impl HijriSimulated {
    /// Creates a new [`HijriSimulated`] for reference location Mecca, with some compiled data containing precomputed calendrical calculations.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub const fn new_mecca() -> Self {
        Self {
            location: HijriSimulatedLocation::Mecca,
            data: Some(DataPayload::from_static_ref(
                crate::provider::Baked::SINGLETON_CALENDAR_HIJRI_SIMULATED_MECCA_V1,
            )),
        }
    }

    icu_provider::gen_buffer_data_constructors!(() -> error: DataError,
        functions: [
            new: skip,
            try_new_mecca_with_buffer_provider,
            try_new_mecca_unstable,
            Self,
    ]);

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::new_mecca)]
    pub fn try_new_mecca_unstable<D: DataProvider<CalendarHijriSimulatedMeccaV1> + ?Sized>(
        provider: &D,
    ) -> Result<Self, DataError> {
        Ok(Self {
            location: HijriSimulatedLocation::Mecca,
            data: Some(provider.load(Default::default())?.payload.cast()),
        })
    }

    /// Construct a new [`HijriSimulated`] for reference location Mecca, without any precomputed calendrical calculations.
    pub const fn new_mecca_always_calculating() -> Self {
        Self {
            location: HijriSimulatedLocation::Mecca,
            data: None,
        }
    }

    /// Compute a cache for this calendar
    #[cfg(feature = "datagen")]
    pub fn build_cache(&self, extended_years: core::ops::Range<i32>) -> HijriData<'static> {
        let data = extended_years
            .clone()
            .map(|year| self.location.compute_year_info(year).pack())
            .collect();
        HijriData {
            first_extended_year: extended_years.start,
            data,
        }
    }
}

impl HijriUmmAlQura {
    /// Creates a new [`HijriUmmAlQura`].
    pub const fn new() -> Self {
        Self
    }
}

/// The epoch for the [`HijriTabular`] calendar.
#[non_exhaustive]
#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord)]
pub enum HijriTabularEpoch {
    /// Thusday July 15, 622 AD (0622-07-18 ISO)
    Thursday,
    /// Friday July 16, 622 AD (0622-07-19 ISO)
    Friday,
}

impl HijriTabularEpoch {
    fn rata_die(self) -> RataDie {
        match self {
            Self::Thursday => ISLAMIC_EPOCH_THURSDAY,
            Self::Friday => ISLAMIC_EPOCH_FRIDAY,
        }
    }
}

/// The leap year rule for the [`HijriTabular`] calendar.
///
/// This specifies which years of a 30-year cycle have an additional day at
/// the end of the year.
#[non_exhaustive]
#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord)]
pub enum HijriTabularLeapYears {
    /// Leap years 2, 5, 7, 10, 13, 16, 18, 21, 24, 26, 29
    TypeII,
}

impl HijriTabular {
    /// Construct a new [`HijriTabular`] with the given leap year rule and epoch.
    pub const fn new(leap_years: HijriTabularLeapYears, epoch: HijriTabularEpoch) -> Self {
        Self { epoch, leap_years }
    }
}

#[derive(Copy, Clone, Debug, Hash, PartialEq, Eq, PartialOrd, Ord)]
pub(crate) struct HijriYearInfo {
    month_lengths: [bool; 12],
    start_day: RataDie,
    value: i32,
}

impl From<HijriYearInfo> for i32 {
    fn from(value: HijriYearInfo) -> Self {
        value.value
    }
}

impl HijriData<'_> {
    /// Get the cached data for a given extended year
    fn get(&self, extended_year: i32) -> Option<HijriYearInfo> {
        Some(HijriYearInfo::unpack(
            extended_year,
            self.data
                .get(usize::try_from(extended_year - self.first_extended_year).ok()?)?,
        ))
    }
}

const LONG_YEAR_LEN: u16 = 355;
const SHORT_YEAR_LEN: u16 = 354;

impl HijriYearInfo {
    #[cfg(feature = "datagen")]
    fn pack(&self) -> PackedHijriYearInfo {
        PackedHijriYearInfo::new(self.value, self.month_lengths, self.start_day)
    }

    fn unpack(extended_year: i32, packed: PackedHijriYearInfo) -> Self {
        let (month_lengths, start_day) = packed.unpack(extended_year);

        HijriYearInfo {
            month_lengths,
            start_day,
            value: extended_year,
        }
    }

    /// The number of days in a given 1-indexed month
    fn days_in_month(self, month: u8) -> u8 {
        let Some(zero_month) = month.checked_sub(1) else {
            return 29;
        };

        if self.month_lengths.get(zero_month as usize) == Some(&true) {
            30
        } else {
            29
        }
    }

    fn days_in_year(self) -> u16 {
        self.last_day_of_month(12)
    }

    /// Get the date's R.D. given (m, d) in this info's year
    fn md_to_rd(self, month: u8, day: u8) -> RataDie {
        let month_offset = if month == 1 {
            0
        } else {
            self.last_day_of_month(month - 1)
        };
        self.start_day + month_offset as i64 + (day - 1) as i64
    }

    fn md_from_rd(self, rd: RataDie) -> (u8, u8) {
        let day_of_year = (rd - self.start_day) as u16;
        debug_assert!(day_of_year < 360);
        // We divide by 30, not 29, to account for the case where all months before this
        // were length 30 (possible near the beginning of the year)
        let mut month = (day_of_year / 30) as u8 + 1;

        let day_of_year = day_of_year + 1;
        let mut last_day_of_month = self.last_day_of_month(month);
        let mut last_day_of_prev_month = if month == 1 {
            0
        } else {
            self.last_day_of_month(month - 1)
        };

        while day_of_year > last_day_of_month && month <= 12 {
            month += 1;
            last_day_of_prev_month = last_day_of_month;
            last_day_of_month = self.last_day_of_month(month);
        }
        debug_assert!(
            day_of_year - last_day_of_prev_month <= 30,
            "Found day {} that doesn't fit in month!",
            day_of_year - last_day_of_prev_month
        );
        let day = (day_of_year - last_day_of_prev_month) as u8;
        (month, day)
    }

    // Which day of year is the last day of a month (month is 1-indexed)
    fn last_day_of_month(self, month: u8) -> u16 {
        29 * month as u16
            + self
                .month_lengths
                .get(..month as usize)
                .unwrap_or_default()
                .iter()
                .filter(|&&x| x)
                .count() as u16
    }
}

impl PrecomputedDataSource<HijriYearInfo> for HijriSimulated {
    fn load_or_compute_info(&self, extended_year: i32) -> HijriYearInfo {
        self.data
            .as_ref()
            .and_then(|d| d.get().get(extended_year))
            .unwrap_or_else(|| self.location.compute_year_info(extended_year))
    }
}

/// The inner date type used for representing [`Date`]s of [`HijriSimulated`]. See [`Date`] and [`HijriSimulated`] for more details.

#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord)]
pub struct HijriSimulatedDateInner(ArithmeticDate<HijriSimulated>);

impl CalendarArithmetic for HijriSimulated {
    type YearInfo = HijriYearInfo;

    fn days_in_provided_month(year: Self::YearInfo, month: u8) -> u8 {
        year.days_in_month(month)
    }

    fn months_in_provided_year(_year: Self::YearInfo) -> u8 {
        12
    }

    fn days_in_provided_year(year: Self::YearInfo) -> u16 {
        year.days_in_year()
    }

    // As an true lunar calendar, it does not have leap years.
    fn provided_year_is_leap(year: Self::YearInfo) -> bool {
        year.days_in_year() != SHORT_YEAR_LEN
    }

    fn last_month_day_in_provided_year(year: Self::YearInfo) -> (u8, u8) {
        let days = Self::days_in_provided_month(year, 12);

        (12, days)
    }
}

impl crate::cal::scaffold::UnstableSealed for HijriSimulated {}
impl Calendar for HijriSimulated {
    type DateInner = HijriSimulatedDateInner;
    type Year = types::EraYear;
    fn from_codes(
        &self,
        era: Option<&str>,
        year: i32,
        month_code: types::MonthCode,
        day: u8,
    ) -> Result<Self::DateInner, DateError> {
        let year = match era {
            Some("ah") | None => year_check(year, 1..)?,
            Some("bh") => 1 - year_check(year, 1..)?,
            Some(_) => return Err(DateError::UnknownEra),
        };
        let Some((month, false)) = month_code.parsed() else {
            return Err(DateError::UnknownMonthCode(month_code));
        };
        Ok(HijriSimulatedDateInner(ArithmeticDate::new_from_ordinals(
            self.load_or_compute_info(year),
            month,
            day,
        )?))
    }

    fn from_rata_die(&self, rd: RataDie) -> Self::DateInner {
        // +1 because the epoch is new year of year 1
        // truncating instead of flooring does not matter, as this is well-defined for
        // positive years only
        let extended_year = ((rd - calendrical_calculations::islamic::ISLAMIC_EPOCH_FRIDAY) as f64
            / calendrical_calculations::islamic::MEAN_YEAR_LENGTH)
            as i32
            + 1;

        let year = self.load_or_compute_info(extended_year);

        let y = if rd < year.start_day {
            self.load_or_compute_info(extended_year - 1)
        } else {
            let next_year = self.load_or_compute_info(extended_year + 1);
            if rd < next_year.start_day {
                year
            } else {
                next_year
            }
        };
        let (m, d) = y.md_from_rd(rd);
        HijriSimulatedDateInner(ArithmeticDate::new_unchecked(y, m, d))
    }

    fn to_rata_die(&self, date: &Self::DateInner) -> RataDie {
        date.0.year.md_to_rd(date.0.month, date.0.day)
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
        date.0.offset_date(offset, self)
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
        Self::DEBUG_NAME
    }

    fn year_info(&self, date: &Self::DateInner) -> Self::Year {
        era_year(self.extended_year(date))
    }

    fn extended_year(&self, date: &Self::DateInner) -> i32 {
        date.0.extended_year()
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

    fn calendar_algorithm(&self) -> Option<crate::preferences::CalendarAlgorithm> {
        Some(match self.location {
            crate::cal::hijri::HijriSimulatedLocation::Mecca => {
                crate::preferences::CalendarAlgorithm::Hijri(Some(
                    crate::preferences::HijriCalendarAlgorithm::Rgsa,
                ))
            }
        })
    }
}

impl HijriSimulatedLocation {
    fn compute_year_info(self, extended_year: i32) -> HijriYearInfo {
        let start_day = calendrical_calculations::islamic::fixed_from_observational_islamic(
            extended_year,
            1,
            1,
            self.location(),
        );
        let next_start_day = calendrical_calculations::islamic::fixed_from_observational_islamic(
            extended_year + 1,
            1,
            1,
            self.location(),
        );
        match (next_start_day - start_day) as u16 {
            LONG_YEAR_LEN | SHORT_YEAR_LEN => (),
            353 => {
                icu_provider::log::trace!(
                    "({}) Found year {extended_year} AH with length {}. See <https://github.com/unicode-org/icu4x/issues/4930>",
                    HijriSimulated::DEBUG_NAME,
                    next_start_day - start_day
                );
            }
            other => {
                debug_assert!(
                    false,
                    "({}) Found year {extended_year} AH with length {}!",
                    HijriSimulated::DEBUG_NAME,
                    other
                )
            }
        }

        let month_lengths = {
            let mut excess_days = 0;
            let mut month_lengths = core::array::from_fn(|month_idx| {
                let days_in_month =
                    calendrical_calculations::islamic::observational_islamic_month_days(
                        extended_year,
                        month_idx as u8 + 1,
                        self.location(),
                    );
                match days_in_month {
                    29 => false,
                    30 => true,
                    31 => {
                        icu_provider::log::trace!(
                            "({}) Found year {extended_year} AH with month length {days_in_month} for month {}.",
                            HijriSimulated::DEBUG_NAME,
                            month_idx + 1
                        );
                        excess_days += 1;
                        true
                    }
                    _ => {
                        debug_assert!(
                            false,
                            "({}) Found year {extended_year} AH with month length {days_in_month} for month {}!",
                            HijriSimulated::DEBUG_NAME,
                            month_idx + 1
                        );
                        false
                    }
                }
            });
            // To maintain invariants for calendar arithmetic, if astronomy finds
            // a 31-day month, "move" the day to the first 29-day month in the
            // same year to maintain all months at 29 or 30 days.
            if excess_days != 0 {
                debug_assert_eq!(
                    excess_days,
                    1,
                    "({}) Found year {extended_year} AH with more than one excess day!",
                    HijriSimulated::DEBUG_NAME
                );
                if let Some(l) = month_lengths.iter_mut().find(|l| !(**l)) {
                    *l = true;
                }
            }
            month_lengths
        };
        HijriYearInfo {
            month_lengths,
            start_day,
            value: extended_year,
        }
    }
}

impl HijriSimulated {
    pub(crate) const DEBUG_NAME: &'static str = "Hijri (simulated)";
}

impl<A: AsCalendar<Calendar = HijriSimulated>> Date<A> {
    /// Construct new simulated Hijri Date.
    ///
    /// ```rust
    /// use icu::calendar::cal::HijriSimulated;
    /// use icu::calendar::Date;
    ///
    /// let hijri = HijriSimulated::new_mecca_always_calculating();
    ///
    /// let date_hijri =
    ///     Date::try_new_simulated_hijri_with_calendar(1392, 4, 25, hijri)
    ///         .expect("Failed to initialize Hijri Date instance.");
    ///
    /// assert_eq!(date_hijri.era_year().year, 1392);
    /// assert_eq!(date_hijri.month().ordinal, 4);
    /// assert_eq!(date_hijri.day_of_month().0, 25);
    /// ```
    pub fn try_new_simulated_hijri_with_calendar(
        year: i32,
        month: u8,
        day: u8,
        calendar: A,
    ) -> Result<Date<A>, RangeError> {
        let y = calendar.as_calendar().load_or_compute_info(year);
        ArithmeticDate::new_from_ordinals(y, month, day)
            .map(HijriSimulatedDateInner)
            .map(|inner| Date::from_raw(inner, calendar))
    }
}

#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord)]
/// The inner date type used for representing [`Date`]s of [`HijriUmmAlQura`]. See [`Date`] and [`HijriUmmAlQura`] for more details.
pub struct HijriUmmAlQuraDateInner(ArithmeticDate<HijriUmmAlQura>);

impl CalendarArithmetic for HijriUmmAlQura {
    type YearInfo = HijriYearInfo;

    fn days_in_provided_month(year: Self::YearInfo, month: u8) -> u8 {
        year.days_in_month(month)
    }

    fn months_in_provided_year(_year: HijriYearInfo) -> u8 {
        12
    }

    fn days_in_provided_year(year: Self::YearInfo) -> u16 {
        year.days_in_year()
    }

    // As an true lunar calendar, it does not have leap years.
    fn provided_year_is_leap(year: Self::YearInfo) -> bool {
        year.days_in_year() != SHORT_YEAR_LEN
    }

    fn last_month_day_in_provided_year(year: HijriYearInfo) -> (u8, u8) {
        let days = Self::days_in_provided_month(year, 12);

        (12, days)
    }
}

impl crate::cal::scaffold::UnstableSealed for HijriUmmAlQura {}
impl Calendar for HijriUmmAlQura {
    type DateInner = HijriUmmAlQuraDateInner;
    type Year = types::EraYear;
    fn from_codes(
        &self,
        era: Option<&str>,
        year: i32,
        month_code: types::MonthCode,
        day: u8,
    ) -> Result<Self::DateInner, DateError> {
        let year = match era {
            Some("ah") | None => year_check(year, 1..)?,
            Some("bh") => 1 - year_check(year, 1..)?,
            Some(_) => return Err(DateError::UnknownEra),
        };
        let Some((month, false)) = month_code.parsed() else {
            return Err(DateError::UnknownMonthCode(month_code));
        };
        Ok(HijriUmmAlQuraDateInner(ArithmeticDate::new_from_ordinals(
            self.load_or_compute_info(year),
            month,
            day,
        )?))
    }

    fn from_rata_die(&self, rd: RataDie) -> Self::DateInner {
        // +1 because the epoch is new year of year 1
        // truncating instead of flooring does not matter, as this is well-defined for
        // positive years only
        let extended_year = ((rd - calendrical_calculations::islamic::ISLAMIC_EPOCH_FRIDAY) as f64
            / calendrical_calculations::islamic::MEAN_YEAR_LENGTH)
            as i32
            + 1;

        let year = self.load_or_compute_info(extended_year);

        let y = if rd < year.start_day {
            self.load_or_compute_info(extended_year - 1)
        } else {
            let next_year = self.load_or_compute_info(extended_year + 1);
            if rd < next_year.start_day {
                year
            } else {
                next_year
            }
        };
        let (m, d) = y.md_from_rd(rd);
        HijriUmmAlQuraDateInner(ArithmeticDate::new_unchecked(y, m, d))
    }

    fn to_rata_die(&self, date: &Self::DateInner) -> RataDie {
        date.0.year.md_to_rd(date.0.month, date.0.day)
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
        date.0.offset_date(offset, &HijriUmmAlQura)
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
        Self::DEBUG_NAME
    }

    fn year_info(&self, date: &Self::DateInner) -> Self::Year {
        era_year(self.extended_year(date))
    }

    fn extended_year(&self, date: &Self::DateInner) -> i32 {
        date.0.extended_year()
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

    fn calendar_algorithm(&self) -> Option<crate::preferences::CalendarAlgorithm> {
        let expected_calendar = crate::preferences::CalendarAlgorithm::Hijri(Some(
            crate::preferences::HijriCalendarAlgorithm::Umalqura,
        ));
        Some(expected_calendar)
    }
}

impl PrecomputedDataSource<HijriYearInfo> for HijriUmmAlQura {
    fn load_or_compute_info(&self, year: i32) -> HijriYearInfo {
        if let Some(&packed) = usize::try_from(year - UMMALQURA_DATA_STARTING_YEAR)
            .ok()
            .and_then(|i| UMMALQURA_DATA.get(i))
        {
            HijriYearInfo::unpack(year, packed)
        } else {
            HijriYearInfo {
                value: year,
                month_lengths: core::array::from_fn(|i| {
                    HijriTabular::days_in_provided_month(year, i as u8 + 1) == 30
                }),
                start_day: calendrical_calculations::islamic::fixed_from_tabular_islamic(
                    year,
                    1,
                    1,
                    ISLAMIC_EPOCH_FRIDAY,
                ),
            }
        }
    }
}

impl HijriUmmAlQura {
    pub(crate) const DEBUG_NAME: &'static str = "Hijri (Umm al-Qura)";
}

impl Date<HijriUmmAlQura> {
    /// Construct new Hijri Umm al-Qura Date.
    ///
    /// ```rust
    /// use icu::calendar::cal::HijriUmmAlQura;
    /// use icu::calendar::Date;
    ///
    /// let date_hijri = Date::try_new_ummalqura(1392, 4, 25)
    ///     .expect("Failed to initialize Hijri Date instance.");
    ///
    /// assert_eq!(date_hijri.era_year().year, 1392);
    /// assert_eq!(date_hijri.month().ordinal, 4);
    /// assert_eq!(date_hijri.day_of_month().0, 25);
    /// ```
    pub fn try_new_ummalqura(
        year: i32,
        month: u8,
        day: u8,
    ) -> Result<Date<HijriUmmAlQura>, RangeError> {
        let y = HijriUmmAlQura.load_or_compute_info(year);
        Ok(Date::from_raw(
            HijriUmmAlQuraDateInner(ArithmeticDate::new_from_ordinals(y, month, day)?),
            HijriUmmAlQura,
        ))
    }
}

/// The inner date type used for representing [`Date`]s of [`HijriTabular`]. See [`Date`] and [`HijriTabular`] for more details.

#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord)]
pub struct HijriTabularDateInner(ArithmeticDate<HijriTabular>);

impl CalendarArithmetic for HijriTabular {
    type YearInfo = i32;

    fn days_in_provided_month(year: i32, month: u8) -> u8 {
        match month {
            1 | 3 | 5 | 7 | 9 | 11 => 30,
            2 | 4 | 6 | 8 | 10 => 29,
            12 if Self::provided_year_is_leap(year) => 30,
            12 => 29,
            _ => 0,
        }
    }

    fn months_in_provided_year(_year: Self::YearInfo) -> u8 {
        12
    }

    fn days_in_provided_year(year: i32) -> u16 {
        if Self::provided_year_is_leap(year) {
            LONG_YEAR_LEN
        } else {
            SHORT_YEAR_LEN
        }
    }

    fn provided_year_is_leap(year: i32) -> bool {
        (14 + 11 * year).rem_euclid(30) < 11
    }

    fn last_month_day_in_provided_year(year: i32) -> (u8, u8) {
        if Self::provided_year_is_leap(year) {
            (12, 30)
        } else {
            (12, 29)
        }
    }
}

impl crate::cal::scaffold::UnstableSealed for HijriTabular {}
impl Calendar for HijriTabular {
    type DateInner = HijriTabularDateInner;
    type Year = types::EraYear;

    fn from_codes(
        &self,
        era: Option<&str>,
        year: i32,
        month_code: types::MonthCode,
        day: u8,
    ) -> Result<Self::DateInner, DateError> {
        let year = match era {
            Some("ah") | None => year_check(year, 1..)?,
            Some("bh") => 1 - year_check(year, 1..)?,
            Some(_) => return Err(DateError::UnknownEra),
        };

        ArithmeticDate::new_from_codes(self, year, month_code, day).map(HijriTabularDateInner)
    }

    fn from_rata_die(&self, rd: RataDie) -> Self::DateInner {
        let (y, m, d) = match self.leap_years {
            HijriTabularLeapYears::TypeII => {
                calendrical_calculations::islamic::tabular_islamic_from_fixed(
                    rd,
                    self.epoch.rata_die(),
                )
            }
        };

        debug_assert!(Date::try_new_hijri_tabular_with_calendar(y, m, d, crate::Ref(self)).is_ok());
        HijriTabularDateInner(ArithmeticDate::new_unchecked(y, m, d))
    }

    fn to_rata_die(&self, date: &Self::DateInner) -> RataDie {
        match self.leap_years {
            HijriTabularLeapYears::TypeII => {
                calendrical_calculations::islamic::fixed_from_tabular_islamic(
                    date.0.year,
                    date.0.month,
                    date.0.day,
                    self.epoch.rata_die(),
                )
            }
        }
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
        match self.epoch {
            HijriTabularEpoch::Friday => "Hijri (civil)",
            HijriTabularEpoch::Thursday => "Hijri (astronomical)",
        }
    }

    fn year_info(&self, date: &Self::DateInner) -> Self::Year {
        era_year(self.extended_year(date))
    }

    fn extended_year(&self, date: &Self::DateInner) -> i32 {
        date.0.extended_year()
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

    fn calendar_algorithm(&self) -> Option<crate::preferences::CalendarAlgorithm> {
        let expected_calendar = match (self.epoch, self.leap_years) {
            (crate::cal::HijriTabularEpoch::Friday, crate::cal::HijriTabularLeapYears::TypeII) => {
                crate::preferences::CalendarAlgorithm::Hijri(Some(
                    crate::preferences::HijriCalendarAlgorithm::Civil,
                ))
            }
            (
                crate::cal::HijriTabularEpoch::Thursday,
                crate::cal::HijriTabularLeapYears::TypeII,
            ) => crate::preferences::CalendarAlgorithm::Hijri(Some(
                crate::preferences::HijriCalendarAlgorithm::Tbla,
            )),
        };
        Some(expected_calendar)
    }
}

impl<A: AsCalendar<Calendar = HijriTabular>> Date<A> {
    /// Construct new Tabular Hijri Date.
    ///
    /// ```rust
    /// use icu::calendar::cal::{
    ///     HijriTabular, HijriTabularEpoch, HijriTabularLeapYears,
    /// };
    /// use icu::calendar::Date;
    ///
    /// let hijri = HijriTabular::new(
    ///     HijriTabularLeapYears::TypeII,
    ///     HijriTabularEpoch::Thursday,
    /// );
    ///
    /// let date_hijri =
    ///     Date::try_new_hijri_tabular_with_calendar(1392, 4, 25, hijri)
    ///         .expect("Failed to initialize Hijri Date instance.");
    ///
    /// assert_eq!(date_hijri.era_year().year, 1392);
    /// assert_eq!(date_hijri.month().ordinal, 4);
    /// assert_eq!(date_hijri.day_of_month().0, 25);
    /// ```
    pub fn try_new_hijri_tabular_with_calendar(
        year: i32,
        month: u8,
        day: u8,
        calendar: A,
    ) -> Result<Date<A>, RangeError> {
        ArithmeticDate::new_from_ordinals(year, month, day)
            .map(HijriTabularDateInner)
            .map(|inner| Date::from_raw(inner, calendar))
    }
}

#[cfg(test)]
mod test {
    use types::MonthCode;

    use super::*;
    use crate::Ref;

    const START_YEAR: i32 = -1245;
    const END_YEAR: i32 = 1518;

    #[derive(Debug)]
    struct DateCase {
        year: i32,
        month: u8,
        day: u8,
    }

    static TEST_RD: [i64; 33] = [
        -214193, -61387, 25469, 49217, 171307, 210155, 253427, 369740, 400085, 434355, 452605,
        470160, 473837, 507850, 524156, 544676, 567118, 569477, 601716, 613424, 626596, 645554,
        664224, 671401, 694799, 704424, 708842, 709409, 709580, 727274, 728714, 744313, 764652,
    ];

    static UMMALQURA_CASES: [DateCase; 33] = [
        DateCase {
            year: -1245,
            month: 12,
            day: 9,
        },
        DateCase {
            year: -813,
            month: 2,
            day: 23,
        },
        DateCase {
            year: -568,
            month: 4,
            day: 1,
        },
        DateCase {
            year: -501,
            month: 4,
            day: 6,
        },
        DateCase {
            year: -157,
            month: 10,
            day: 17,
        },
        DateCase {
            year: -47,
            month: 6,
            day: 3,
        },
        DateCase {
            year: 75,
            month: 7,
            day: 13,
        },
        DateCase {
            year: 403,
            month: 10,
            day: 5,
        },
        DateCase {
            year: 489,
            month: 5,
            day: 22,
        },
        DateCase {
            year: 586,
            month: 2,
            day: 7,
        },
        DateCase {
            year: 637,
            month: 8,
            day: 7,
        },
        DateCase {
            year: 687,
            month: 2,
            day: 20,
        },
        DateCase {
            year: 697,
            month: 7,
            day: 7,
        },
        DateCase {
            year: 793,
            month: 7,
            day: 1,
        },
        DateCase {
            year: 839,
            month: 7,
            day: 6,
        },
        DateCase {
            year: 897,
            month: 6,
            day: 1,
        },
        DateCase {
            year: 960,
            month: 9,
            day: 30,
        },
        DateCase {
            year: 967,
            month: 5,
            day: 27,
        },
        DateCase {
            year: 1058,
            month: 5,
            day: 18,
        },
        DateCase {
            year: 1091,
            month: 6,
            day: 2,
        },
        DateCase {
            year: 1128,
            month: 8,
            day: 4,
        },
        DateCase {
            year: 1182,
            month: 2,
            day: 3,
        },
        DateCase {
            year: 1234,
            month: 10,
            day: 10,
        },
        DateCase {
            year: 1255,
            month: 1,
            day: 11,
        },
        DateCase {
            year: 1321,
            month: 1,
            day: 21,
        },
        DateCase {
            year: 1348,
            month: 3,
            day: 20,
        },
        DateCase {
            year: 1360,
            month: 9,
            day: 7,
        },
        DateCase {
            year: 1362,
            month: 4,
            day: 14,
        },
        DateCase {
            year: 1362,
            month: 10,
            day: 7,
        },
        DateCase {
            year: 1412,
            month: 9,
            day: 12,
        },
        DateCase {
            year: 1416,
            month: 10,
            day: 6,
        },
        DateCase {
            year: 1460,
            month: 10,
            day: 13,
        },
        DateCase {
            year: 1518,
            month: 3,
            day: 5,
        },
    ];

    static SIMULATED_CASES: [DateCase; 33] = [
        DateCase {
            year: -1245,
            month: 12,
            day: 10,
        },
        DateCase {
            year: -813,
            month: 2,
            day: 25,
        },
        DateCase {
            year: -568,
            month: 4,
            day: 2,
        },
        DateCase {
            year: -501,
            month: 4,
            day: 7,
        },
        DateCase {
            year: -157,
            month: 10,
            day: 18,
        },
        DateCase {
            year: -47,
            month: 6,
            day: 3,
        },
        DateCase {
            year: 75,
            month: 7,
            day: 13,
        },
        DateCase {
            year: 403,
            month: 10,
            day: 5,
        },
        DateCase {
            year: 489,
            month: 5,
            day: 22,
        },
        DateCase {
            year: 586,
            month: 2,
            day: 7,
        },
        DateCase {
            year: 637,
            month: 8,
            day: 7,
        },
        DateCase {
            year: 687,
            month: 2,
            day: 21,
        },
        DateCase {
            year: 697,
            month: 7,
            day: 7,
        },
        DateCase {
            year: 793,
            month: 6,
            day: 29,
        },
        DateCase {
            year: 839,
            month: 7,
            day: 6,
        },
        DateCase {
            year: 897,
            month: 6,
            day: 2,
        },
        DateCase {
            year: 960,
            month: 9,
            day: 30,
        },
        DateCase {
            year: 967,
            month: 5,
            day: 27,
        },
        DateCase {
            year: 1058,
            month: 5,
            day: 18,
        },
        DateCase {
            year: 1091,
            month: 6,
            day: 3,
        },
        DateCase {
            year: 1128,
            month: 8,
            day: 4,
        },
        DateCase {
            year: 1182,
            month: 2,
            day: 4,
        },
        DateCase {
            year: 1234,
            month: 10,
            day: 10,
        },
        DateCase {
            year: 1255,
            month: 1,
            day: 11,
        },
        DateCase {
            year: 1321,
            month: 1,
            day: 20,
        },
        DateCase {
            year: 1348,
            month: 3,
            day: 19,
        },
        DateCase {
            year: 1360,
            month: 9,
            day: 7,
        },
        DateCase {
            year: 1362,
            month: 4,
            day: 13,
        },
        DateCase {
            year: 1362,
            month: 10,
            day: 7,
        },
        DateCase {
            year: 1412,
            month: 9,
            day: 12,
        },
        DateCase {
            year: 1416,
            month: 10,
            day: 5,
        },
        DateCase {
            year: 1460,
            month: 10,
            day: 12,
        },
        DateCase {
            year: 1518,
            month: 3,
            day: 5,
        },
    ];

    static ARITHMETIC_CASES: [DateCase; 33] = [
        DateCase {
            year: -1245,
            month: 12,
            day: 9,
        },
        DateCase {
            year: -813,
            month: 2,
            day: 23,
        },
        DateCase {
            year: -568,
            month: 4,
            day: 1,
        },
        DateCase {
            year: -501,
            month: 4,
            day: 6,
        },
        DateCase {
            year: -157,
            month: 10,
            day: 17,
        },
        DateCase {
            year: -47,
            month: 6,
            day: 3,
        },
        DateCase {
            year: 75,
            month: 7,
            day: 13,
        },
        DateCase {
            year: 403,
            month: 10,
            day: 5,
        },
        DateCase {
            year: 489,
            month: 5,
            day: 22,
        },
        DateCase {
            year: 586,
            month: 2,
            day: 7,
        },
        DateCase {
            year: 637,
            month: 8,
            day: 7,
        },
        DateCase {
            year: 687,
            month: 2,
            day: 20,
        },
        DateCase {
            year: 697,
            month: 7,
            day: 7,
        },
        DateCase {
            year: 793,
            month: 7,
            day: 1,
        },
        DateCase {
            year: 839,
            month: 7,
            day: 6,
        },
        DateCase {
            year: 897,
            month: 6,
            day: 1,
        },
        DateCase {
            year: 960,
            month: 9,
            day: 30,
        },
        DateCase {
            year: 967,
            month: 5,
            day: 27,
        },
        DateCase {
            year: 1058,
            month: 5,
            day: 18,
        },
        DateCase {
            year: 1091,
            month: 6,
            day: 2,
        },
        DateCase {
            year: 1128,
            month: 8,
            day: 4,
        },
        DateCase {
            year: 1182,
            month: 2,
            day: 3,
        },
        DateCase {
            year: 1234,
            month: 10,
            day: 10,
        },
        DateCase {
            year: 1255,
            month: 1,
            day: 11,
        },
        DateCase {
            year: 1321,
            month: 1,
            day: 21,
        },
        DateCase {
            year: 1348,
            month: 3,
            day: 19,
        },
        DateCase {
            year: 1360,
            month: 9,
            day: 8,
        },
        DateCase {
            year: 1362,
            month: 4,
            day: 13,
        },
        DateCase {
            year: 1362,
            month: 10,
            day: 7,
        },
        DateCase {
            year: 1412,
            month: 9,
            day: 13,
        },
        DateCase {
            year: 1416,
            month: 10,
            day: 5,
        },
        DateCase {
            year: 1460,
            month: 10,
            day: 12,
        },
        DateCase {
            year: 1518,
            month: 3,
            day: 5,
        },
    ];

    static ASTRONOMICAL_CASES: [DateCase; 33] = [
        DateCase {
            year: -1245,
            month: 12,
            day: 10,
        },
        DateCase {
            year: -813,
            month: 2,
            day: 24,
        },
        DateCase {
            year: -568,
            month: 4,
            day: 2,
        },
        DateCase {
            year: -501,
            month: 4,
            day: 7,
        },
        DateCase {
            year: -157,
            month: 10,
            day: 18,
        },
        DateCase {
            year: -47,
            month: 6,
            day: 4,
        },
        DateCase {
            year: 75,
            month: 7,
            day: 14,
        },
        DateCase {
            year: 403,
            month: 10,
            day: 6,
        },
        DateCase {
            year: 489,
            month: 5,
            day: 23,
        },
        DateCase {
            year: 586,
            month: 2,
            day: 8,
        },
        DateCase {
            year: 637,
            month: 8,
            day: 8,
        },
        DateCase {
            year: 687,
            month: 2,
            day: 21,
        },
        DateCase {
            year: 697,
            month: 7,
            day: 8,
        },
        DateCase {
            year: 793,
            month: 7,
            day: 2,
        },
        DateCase {
            year: 839,
            month: 7,
            day: 7,
        },
        DateCase {
            year: 897,
            month: 6,
            day: 2,
        },
        DateCase {
            year: 960,
            month: 10,
            day: 1,
        },
        DateCase {
            year: 967,
            month: 5,
            day: 28,
        },
        DateCase {
            year: 1058,
            month: 5,
            day: 19,
        },
        DateCase {
            year: 1091,
            month: 6,
            day: 3,
        },
        DateCase {
            year: 1128,
            month: 8,
            day: 5,
        },
        DateCase {
            year: 1182,
            month: 2,
            day: 4,
        },
        DateCase {
            year: 1234,
            month: 10,
            day: 11,
        },
        DateCase {
            year: 1255,
            month: 1,
            day: 12,
        },
        DateCase {
            year: 1321,
            month: 1,
            day: 22,
        },
        DateCase {
            year: 1348,
            month: 3,
            day: 20,
        },
        DateCase {
            year: 1360,
            month: 9,
            day: 9,
        },
        DateCase {
            year: 1362,
            month: 4,
            day: 14,
        },
        DateCase {
            year: 1362,
            month: 10,
            day: 8,
        },
        DateCase {
            year: 1412,
            month: 9,
            day: 14,
        },
        DateCase {
            year: 1416,
            month: 10,
            day: 6,
        },
        DateCase {
            year: 1460,
            month: 10,
            day: 13,
        },
        DateCase {
            year: 1518,
            month: 3,
            day: 6,
        },
    ];

    #[test]
    fn test_simulated_hijri_from_rd() {
        let calendar = HijriSimulated::new_mecca();
        let calendar = Ref(&calendar);
        for (case, f_date) in SIMULATED_CASES.iter().zip(TEST_RD.iter()) {
            let date = Date::try_new_simulated_hijri_with_calendar(
                case.year, case.month, case.day, calendar,
            )
            .unwrap();
            let iso = Date::from_rata_die(RataDie::new(*f_date), Iso);

            assert_eq!(iso.to_calendar(calendar).inner, date.inner, "{case:?}");
        }
    }

    #[test]
    fn test_rd_from_simulated_hijri() {
        let calendar = HijriSimulated::new_mecca();
        let calendar = Ref(&calendar);
        for (case, f_date) in SIMULATED_CASES.iter().zip(TEST_RD.iter()) {
            let date = Date::try_new_simulated_hijri_with_calendar(
                case.year, case.month, case.day, calendar,
            )
            .unwrap();
            assert_eq!(date.to_rata_die(), RataDie::new(*f_date), "{case:?}");
        }
    }

    #[test]
    fn test_rd_from_hijri() {
        let calendar = HijriTabular::new(HijriTabularLeapYears::TypeII, HijriTabularEpoch::Friday);
        let calendar = Ref(&calendar);
        for (case, f_date) in ARITHMETIC_CASES.iter().zip(TEST_RD.iter()) {
            let date = Date::try_new_hijri_tabular_with_calendar(
                case.year, case.month, case.day, calendar,
            )
            .unwrap();
            assert_eq!(date.to_rata_die(), RataDie::new(*f_date), "{case:?}");
        }
    }

    #[test]
    fn test_hijri_from_rd() {
        let calendar = HijriTabular::new(HijriTabularLeapYears::TypeII, HijriTabularEpoch::Friday);
        let calendar = Ref(&calendar);
        for (case, f_date) in ARITHMETIC_CASES.iter().zip(TEST_RD.iter()) {
            let date = Date::try_new_hijri_tabular_with_calendar(
                case.year, case.month, case.day, calendar,
            )
            .unwrap();
            let date_rd = Date::from_rata_die(RataDie::new(*f_date), calendar);

            assert_eq!(date, date_rd, "{case:?}");
        }
    }

    #[test]
    fn test_rd_from_hijri_tbla() {
        let calendar =
            HijriTabular::new(HijriTabularLeapYears::TypeII, HijriTabularEpoch::Thursday);
        let calendar = Ref(&calendar);
        for (case, f_date) in ASTRONOMICAL_CASES.iter().zip(TEST_RD.iter()) {
            let date = Date::try_new_hijri_tabular_with_calendar(
                case.year, case.month, case.day, calendar,
            )
            .unwrap();
            assert_eq!(date.to_rata_die(), RataDie::new(*f_date), "{case:?}");
        }
    }

    #[test]
    fn test_hijri_tbla_from_rd() {
        let calendar =
            HijriTabular::new(HijriTabularLeapYears::TypeII, HijriTabularEpoch::Thursday);
        let calendar = Ref(&calendar);
        for (case, f_date) in ASTRONOMICAL_CASES.iter().zip(TEST_RD.iter()) {
            let date = Date::try_new_hijri_tabular_with_calendar(
                case.year, case.month, case.day, calendar,
            )
            .unwrap();
            let date_rd = Date::from_rata_die(RataDie::new(*f_date), calendar);

            assert_eq!(date, date_rd, "{case:?}");
        }
    }

    #[test]
    fn test_saudi_hijri_from_rd() {
        let calendar = HijriUmmAlQura::new();
        let calendar = Ref(&calendar);
        for (case, f_date) in UMMALQURA_CASES.iter().zip(TEST_RD.iter()) {
            let date = Date::try_new_ummalqura(case.year, case.month, case.day).unwrap();
            let date_rd = Date::from_rata_die(RataDie::new(*f_date), calendar);

            assert_eq!(date, date_rd, "{case:?}");
        }
    }

    #[test]
    fn test_rd_from_saudi_hijri() {
        for (case, f_date) in UMMALQURA_CASES.iter().zip(TEST_RD.iter()) {
            let date = Date::try_new_ummalqura(case.year, case.month, case.day).unwrap();
            assert_eq!(date.to_rata_die(), RataDie::new(*f_date), "{case:?}");
        }
    }

    #[ignore] // slow
    #[test]
    fn test_days_in_provided_year_simulated() {
        let calendar = HijriSimulated::new_mecca();
        let calendar = Ref(&calendar);
        // -1245 1 1 = -214526 (R.D Date)
        // 1518 1 1 = 764589 (R.D Date)
        let sum_days_in_year: i64 = (START_YEAR..END_YEAR)
            .map(|year| {
                HijriSimulated::days_in_provided_year(
                    HijriSimulatedLocation::Mecca.compute_year_info(year),
                ) as i64
            })
            .sum();
        let expected_number_of_days =
            Date::try_new_simulated_hijri_with_calendar(END_YEAR, 1, 1, calendar)
                .unwrap()
                .to_rata_die()
                - Date::try_new_simulated_hijri_with_calendar(START_YEAR, 1, 1, calendar)
                    .unwrap()
                    .to_rata_die(); // The number of days between Hijri years -1245 and 1518
        let tolerance = 1; // One day tolerance (See Astronomical::month_length for more context)

        assert!(
            (sum_days_in_year - expected_number_of_days).abs() <= tolerance,
            "Difference between sum_days_in_year and expected_number_of_days is more than the tolerance"
        );
    }

    #[ignore] // slow
    #[test]
    fn test_days_in_provided_year_ummalqura() {
        // -1245 1 1 = -214528 (R.D Date)
        // 1518 1 1 = 764588 (R.D Date)
        let sum_days_in_year: i64 = (START_YEAR..END_YEAR)
            .map(|year| {
                HijriUmmAlQura::days_in_provided_year(HijriUmmAlQura.load_or_compute_info(year))
                    as i64
            })
            .sum();
        let expected_number_of_days = Date::try_new_ummalqura(END_YEAR, 1, 1)
            .unwrap()
            .to_rata_die()
            - (Date::try_new_ummalqura(START_YEAR, 1, 1).unwrap()).to_rata_die(); // The number of days between Umm al-Qura Hijri years -1245 and 1518

        assert_eq!(sum_days_in_year, expected_number_of_days);
    }

    #[test]
    fn test_regression_3868() {
        // This date used to panic on creation
        let iso = Date::try_new_iso(2011, 4, 4).unwrap();
        let hijri = iso.to_calendar(HijriUmmAlQura::new());
        // Data from https://www.ummulqura.org.sa/Index.aspx
        assert_eq!(hijri.day_of_month().0, 30);
        assert_eq!(hijri.month().ordinal, 4);
        assert_eq!(hijri.era_year().year, 1432);
    }

    #[test]
    fn test_regression_4914() {
        // https://github.com/unicode-org/icu4x/issues/4914
        let dt = HijriUmmAlQura::new()
            .from_codes(Some("bh"), 6824, MonthCode::new_normal(1).unwrap(), 1)
            .unwrap();
        assert_eq!(dt.0.day, 1);
        assert_eq!(dt.0.month, 1);
        assert_eq!(dt.0.year.value, -6823);
    }

    #[test]
    fn test_regression_5069_uaq() {
        let cached = HijriUmmAlQura::new();

        let cached = crate::Ref(&cached);

        let dt_cached = Date::try_new_ummalqura(1391, 1, 29).unwrap();

        assert_eq!(dt_cached.to_iso().to_calendar(cached), dt_cached);
    }

    #[test]
    fn test_regression_5069_obs() {
        let cached = HijriSimulated::new_mecca();
        let comp = HijriSimulated::new_mecca_always_calculating();

        let cached = crate::Ref(&cached);
        let comp = crate::Ref(&comp);

        let dt_cached = Date::try_new_simulated_hijri_with_calendar(1390, 1, 30, cached).unwrap();
        let dt_comp = Date::try_new_simulated_hijri_with_calendar(1390, 1, 30, comp).unwrap();

        assert_eq!(dt_cached.to_iso(), dt_comp.to_iso());

        assert_eq!(dt_comp.to_iso().to_calendar(comp), dt_comp);
        assert_eq!(dt_cached.to_iso().to_calendar(cached), dt_cached);

        let dt = Date::try_new_iso(2000, 5, 5).unwrap();

        assert!(dt.to_calendar(comp).day_of_month().0 > 0);
        assert!(dt.to_calendar(cached).day_of_month().0 > 0);
    }

    #[test]
    fn test_regression_6197() {
        let cached = HijriUmmAlQura::new();

        let cached = crate::Ref(&cached);

        let iso = Date::try_new_iso(2025, 2, 26).unwrap();

        let cached = iso.to_calendar(cached);

        // Data from https://www.ummulqura.org.sa/
        assert_eq!(
            (
                cached.day_of_month().0,
                cached.month().ordinal,
                cached.era_year().year
            ),
            (27, 8, 1446)
        );
    }

    #[test]
    fn test_uaq_icu4c_agreement() {
        // From https://github.com/unicode-org/icu/blob/1bf6bf774dbc8c6c2051963a81100ea1114b497f/icu4c/source/i18n/islamcal.cpp#L87
        const ICU4C_ENCODED_MONTH_LENGTHS: [u16; 1601 - 1300] = [
            0x0AAA, 0x0D54, 0x0EC9, 0x06D4, 0x06EA, 0x036C, 0x0AAD, 0x0555, 0x06A9, 0x0792, 0x0BA9,
            0x05D4, 0x0ADA, 0x055C, 0x0D2D, 0x0695, 0x074A, 0x0B54, 0x0B6A, 0x05AD, 0x04AE, 0x0A4F,
            0x0517, 0x068B, 0x06A5, 0x0AD5, 0x02D6, 0x095B, 0x049D, 0x0A4D, 0x0D26, 0x0D95, 0x05AC,
            0x09B6, 0x02BA, 0x0A5B, 0x052B, 0x0A95, 0x06CA, 0x0AE9, 0x02F4, 0x0976, 0x02B6, 0x0956,
            0x0ACA, 0x0BA4, 0x0BD2, 0x05D9, 0x02DC, 0x096D, 0x054D, 0x0AA5, 0x0B52, 0x0BA5, 0x05B4,
            0x09B6, 0x0557, 0x0297, 0x054B, 0x06A3, 0x0752, 0x0B65, 0x056A, 0x0AAB, 0x052B, 0x0C95,
            0x0D4A, 0x0DA5, 0x05CA, 0x0AD6, 0x0957, 0x04AB, 0x094B, 0x0AA5, 0x0B52, 0x0B6A, 0x0575,
            0x0276, 0x08B7, 0x045B, 0x0555, 0x05A9, 0x05B4, 0x09DA, 0x04DD, 0x026E, 0x0936, 0x0AAA,
            0x0D54, 0x0DB2, 0x05D5, 0x02DA, 0x095B, 0x04AB, 0x0A55, 0x0B49, 0x0B64, 0x0B71, 0x05B4,
            0x0AB5, 0x0A55, 0x0D25, 0x0E92, 0x0EC9, 0x06D4, 0x0AE9, 0x096B, 0x04AB, 0x0A93, 0x0D49,
            0x0DA4, 0x0DB2, 0x0AB9, 0x04BA, 0x0A5B, 0x052B, 0x0A95, 0x0B2A, 0x0B55, 0x055C, 0x04BD,
            0x023D, 0x091D, 0x0A95, 0x0B4A, 0x0B5A, 0x056D, 0x02B6, 0x093B, 0x049B, 0x0655, 0x06A9,
            0x0754, 0x0B6A, 0x056C, 0x0AAD, 0x0555, 0x0B29, 0x0B92, 0x0BA9, 0x05D4, 0x0ADA, 0x055A,
            0x0AAB, 0x0595, 0x0749, 0x0764, 0x0BAA, 0x05B5, 0x02B6, 0x0A56, 0x0E4D, 0x0B25, 0x0B52,
            0x0B6A, 0x05AD, 0x02AE, 0x092F, 0x0497, 0x064B, 0x06A5, 0x06AC, 0x0AD6, 0x055D, 0x049D,
            0x0A4D, 0x0D16, 0x0D95, 0x05AA, 0x05B5, 0x02DA, 0x095B, 0x04AD, 0x0595, 0x06CA, 0x06E4,
            0x0AEA, 0x04F5, 0x02B6, 0x0956, 0x0AAA, 0x0B54, 0x0BD2, 0x05D9, 0x02EA, 0x096D, 0x04AD,
            0x0A95, 0x0B4A, 0x0BA5, 0x05B2, 0x09B5, 0x04D6, 0x0A97, 0x0547, 0x0693, 0x0749, 0x0B55,
            0x056A, 0x0A6B, 0x052B, 0x0A8B, 0x0D46, 0x0DA3, 0x05CA, 0x0AD6, 0x04DB, 0x026B, 0x094B,
            0x0AA5, 0x0B52, 0x0B69, 0x0575, 0x0176, 0x08B7, 0x025B, 0x052B, 0x0565, 0x05B4, 0x09DA,
            0x04ED, 0x016D, 0x08B6, 0x0AA6, 0x0D52, 0x0DA9, 0x05D4, 0x0ADA, 0x095B, 0x04AB, 0x0653,
            0x0729, 0x0762, 0x0BA9, 0x05B2, 0x0AB5, 0x0555, 0x0B25, 0x0D92, 0x0EC9, 0x06D2, 0x0AE9,
            0x056B, 0x04AB, 0x0A55, 0x0D29, 0x0D54, 0x0DAA, 0x09B5, 0x04BA, 0x0A3B, 0x049B, 0x0A4D,
            0x0AAA, 0x0AD5, 0x02DA, 0x095D, 0x045E, 0x0A2E, 0x0C9A, 0x0D55, 0x06B2, 0x06B9, 0x04BA,
            0x0A5D, 0x052D, 0x0A95, 0x0B52, 0x0BA8, 0x0BB4, 0x05B9, 0x02DA, 0x095A, 0x0B4A, 0x0DA4,
            0x0ED1, 0x06E8, 0x0B6A, 0x056D, 0x0535, 0x0695, 0x0D4A, 0x0DA8, 0x0DD4, 0x06DA, 0x055B,
            0x029D, 0x062B, 0x0B15, 0x0B4A, 0x0B95, 0x05AA, 0x0AAE, 0x092E, 0x0C8F, 0x0527, 0x0695,
            0x06AA, 0x0AD6, 0x055D, 0x029D,
        ];

        // From https://github.com/unicode-org/icu/blob/1bf6bf774dbc8c6c2051963a81100ea1114b497f/icu4c/source/i18n/islamcal.cpp#L264
        const ICU4C_YEAR_START_ESTIMATE_FIX: [i64; 1601 - 1300] = [
            0, 0, -1, 0, -1, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 1, 0, 1, 1, 0, 0, 0, 0,
            1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, -1, -1, 0, 0, 0, 1, 0, 0, -1, 0, 0,
            0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 1, 1, 0, 0, -1, 0, 1, 0, 1, 1, 0, 0, -1,
            0, 1, 0, 0, 0, -1, 0, 1, 0, 1, 0, 0, 0, -1, 0, 0, 0, 0, -1, -1, 0, -1, 0, 1, 0, 0, 0,
            -1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, -1, -1, 0, 0, 0, 1, 0, 0, -1, -1, 0, -1, 0, 0,
            -1, -1, 0, -1, 0, -1, 0, 0, -1, -1, 0, 0, 0, 0, 0, 0, -1, 0, 1, 0, 1, 1, 0, 0, -1, 0,
            1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, -1, 0, 1, 0, 0, -1, -1, 0, 0, 0, 1, 0, 0, 0, 0, 0,
            0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, -1, 0, 0, 0, 1, 1, 0, 0, -1, 0, 1, 0, 1, 1, 0, 0, 0,
            0, 1, 0, 0, 0, -1, 0, 0, 0, 1, 0, 0, 0, -1, 0, 0, 0, 0, 0, -1, 0, -1, 0, 1, 0, 0, 0,
            -1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, -1, 0, 0, 0, 0, 1, 0, 0, 0, -1, 0, 0, 0, 0, -1,
            -1, 0, -1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 1, 0, 0, -1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1,
        ];

        let icu4c = ICU4C_ENCODED_MONTH_LENGTHS
            .into_iter()
            .zip(ICU4C_YEAR_START_ESTIMATE_FIX)
            .enumerate()
            .map(
                |(years_since_1300, (encoded_months_lengths, year_start_estimate_fix))| {
                    // https://github.com/unicode-org/icu/blob/1bf6bf774dbc8c6c2051963a81100ea1114b497f/icu4c/source/i18n/islamcal.cpp#L858
                    let month_lengths =
                        core::array::from_fn(|i| (1 << (11 - i)) & encoded_months_lengths != 0);
                    // From https://github.com/unicode-org/icu/blob/1bf6bf774dbc8c6c2051963a81100ea1114b497f/icu4c/source/i18n/islamcal.cpp#L813
                    let year_start = ((354.36720 * years_since_1300 as f64) + 460322.05 + 0.5)
                        as i64
                        + year_start_estimate_fix;
                    HijriYearInfo {
                        value: 1300 + years_since_1300 as i32,
                        month_lengths,
                        start_day: ISLAMIC_EPOCH_FRIDAY + year_start,
                    }
                },
            )
            .collect::<Vec<_>>();

        let icu4x = (1300..=1600)
            .map(|y| HijriUmmAlQura.load_or_compute_info(y))
            .collect::<Vec<_>>();

        assert_eq!(icu4x, icu4c);
    }
}
