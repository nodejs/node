// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::calendar_arithmetic::ArithmeticDate;
use crate::calendar_arithmetic::DateFieldsResolver;
use crate::calendar_arithmetic::ToExtendedYear;
use crate::error::{DateError, DateFromFieldsError, EcmaReferenceYearError, UnknownEraError};
use crate::options::DateFromFieldsOptions;
use crate::options::{DateAddOptions, DateDifferenceOptions};
use crate::types::DateFields;
use crate::{types, Calendar, Date};
use crate::{AsCalendar, RangeError};
use calendrical_calculations::islamic::{
    ISLAMIC_EPOCH_FRIDAY, ISLAMIC_EPOCH_THURSDAY, WELL_BEHAVED_ASTRONOMICAL_RANGE,
};
use calendrical_calculations::rata_die::RataDie;
use core::fmt::Debug;
use icu_locale_core::preferences::extensions::unicode::keywords::{
    CalendarAlgorithm, HijriCalendarAlgorithm,
};
use icu_provider::prelude::*;
use tinystr::tinystr;

#[path = "hijri/simulated_mecca_data.rs"]
mod simulated_mecca_data;
#[path = "hijri/ummalqura_data.rs"]
mod ummalqura_data;

/// The [Hijri Calendar](https://en.wikipedia.org/wiki/Islamic_calendar)
///
/// There are many variants of this calendar, using different lunar observations or calculations
/// (see [`Rules`]). Currently, [`Rules`] is an unstable trait, but some of its implementors
/// are stable, and can be constructed via the various `Hijri::new_*` constructors. Please comment
/// on [this issue](https://github.com/unicode-org/icu4x/issues/6962)
/// if you would like to see this the ability to implement custom [`Rules`] stabilized.
///
/// This implementation supports only variants where months are either 29 or 30 days.
///
/// This corresponds to various `"islamic-*"` [CLDR calendars](https://unicode.org/reports/tr35/#UnicodeCalendarIdentifier),
/// see the individual implementors of [`Rules`] ([`TabularAlgorithm`], [`UmmAlQura`], [`AstronomicalSimulation`]) for more information.
///
/// # Era codes
///
/// This calendar uses two era codes: `ah`, and `bh`, corresponding to the Anno Hegirae and Before Hijrah eras
///
/// # Months and days
///
/// The 12 months are called al-Muḥarram (`M01`), Ṣafar (`M02`), Rabīʿ al-ʾAwwal (`M03`),
/// Rabīʿ ath-Thānī or Rabīʿ al-ʾĀkhir (`M04`), Jumādā al-ʾŪlā (`M05`), Jumādā ath-Thāniyah
/// or Jumādā al-ʾĀkhirah (`M06`), Rajab (`M07`), Shaʿbān (`M08`), Ramaḍān (`M09`), Shawwāl (`M10`),
/// Ḏū al-Qaʿdah (`M11`), Ḏū al-Ḥijjah (`M12`).
///
/// As a true lunar calendar, the lengths of the months depend on the lunar cycle (a month starts on the day
/// where the waxing crescent is first observed), and will be either 29 or 30 days.
///
/// The lengths of the months are determined by the concrete [`Rules`] implementation.
///
/// There are either 6 or 7 30-day months, so the length of the year is 354 or 355 days.
///
/// # Calendar drift
///
/// As a lunar calendar, this calendar does not intend to follow the solar year, and drifts more
/// than 10 days per year with respect to the seasons.
#[derive(Clone, Debug, Default, Copy)]
#[allow(clippy::exhaustive_structs)] // newtype
pub struct Hijri<S>(pub S);

/// Defines a variant of the [`Hijri`] calendar.
///
/// This crate includes the [`UmmAlQura`], [`AstronomicalSimulation`], and [`TabularAlgorithm`]
/// rules, other rules can be implemented by users.
///
/// <div class="stab unstable">
/// 🚫 This trait is sealed; it should not be implemented by user code. If an API requests an item that implements this
/// trait, please consider using a type from the implementors listed below.
///
/// It is still possible to implement this trait in userland (since `UnstableSealed` is public),
/// do not do so unless you are prepared for things to occasionally break.
/// </div>
pub trait Rules: Clone + Debug + crate::cal::scaffold::UnstableSealed {
    /// Returns data about the given year.
    fn year_data(&self, extended_year: i32) -> HijriYearData;

    /// Returns an ECMA reference year that contains the given month-day combination.
    ///
    /// If the day is out of range, it will return a year that contains the given month
    /// and the maximum day possible for that month. See [the spec][spec] for the
    /// precise algorithm used.
    ///
    /// This API only matters when using [`MissingFieldsStrategy::Ecma`] to compute
    /// a date without providing a year in [`Date::try_from_fields()`]. The default impl
    /// will just error, and custom calendars who do not care about ECMA/Temporal
    /// reference years do not need to override this.
    ///
    /// [spec]: https://tc39.es/proposal-temporal/#sec-temporal-nonisomonthdaytoisoreferencedate
    /// [`MissingFieldsStrategy::Ecma`]: crate::options::MissingFieldsStrategy::Ecma
    fn ecma_reference_year(
        &self,
        // TODO: Consider accepting ValidMonthCode
        _month_code: (u8, bool),
        _day: u8,
    ) -> Result<i32, EcmaReferenceYearError> {
        Err(EcmaReferenceYearError::Unimplemented)
    }

    /// The BCP-47 [`CalendarAlgorithm`] for the Hijri calendar using these rules, if defined.
    fn calendar_algorithm(&self) -> Option<CalendarAlgorithm> {
        None
    }

    /// The debug name for these rules.
    fn debug_name(&self) -> &'static str {
        "Hijri (custom rules)"
    }
}

/// [`Hijri`] [`Rules`] based on an astronomical simulation for a particular location.
///
/// These simulations are unofficial and are known to not necessarily match sightings
/// on the ground. Unless you know otherwise for sure, instead of this variant, use
/// [`UmmAlQura`], which uses the results of KACST's Mecca-based calculations.
///
/// As floating point arithmetic degenerates for far-away dates, this falls back to
/// the tabular calendar at some point.
///
/// The precise behavior of this calendar may change in the future if:
/// - We decide to tweak the precise astronomical simulation used
/// - We decide to expand or reduce the range where we are using the astronomical simulation.
///
/// This corresponds to the `"islamic-rgsa"` [CLDR calendar](https://unicode.org/reports/tr35/#UnicodeCalendarIdentifier)
/// if constructed with [`Hijri::new_simulated_mecca()`].
#[derive(Copy, Clone, Debug)]
pub struct AstronomicalSimulation {
    pub(crate) location: SimulatedLocation,
}

#[derive(Clone, Debug, Copy, PartialEq)]
pub(crate) enum SimulatedLocation {
    Mecca,
}

impl crate::cal::scaffold::UnstableSealed for AstronomicalSimulation {}
impl Rules for AstronomicalSimulation {
    fn debug_name(&self) -> &'static str {
        match self.location {
            SimulatedLocation::Mecca => "Hijri (simulated, Mecca)",
        }
    }

    fn year_data(&self, extended_year: i32) -> HijriYearData {
        if let Some(data) = HijriYearData::lookup(
            extended_year,
            simulated_mecca_data::STARTING_YEAR,
            simulated_mecca_data::DATA,
        ) {
            return data;
        }

        let location = match self.location {
            SimulatedLocation::Mecca => calendrical_calculations::islamic::MECCA,
        };

        let start_day = calendrical_calculations::islamic::fixed_from_observational_islamic(
            extended_year,
            1,
            1,
            location,
        );
        let next_start_day = calendrical_calculations::islamic::fixed_from_observational_islamic(
            extended_year + 1,
            1,
            1,
            location,
        );
        match (next_start_day - start_day) as u16 {
            355 | 354 => (),
            353 => {
                icu_provider::log::trace!(
                    "({}) Found year {extended_year} AH with length {}. See <https://github.com/unicode-org/icu4x/issues/4930>",
                    self.debug_name(),
                    next_start_day - start_day
                );
            }
            other => {
                debug_assert!(
                    !WELL_BEHAVED_ASTRONOMICAL_RANGE.contains(&start_day),
                    "({}) Found year {extended_year} AH with length {}!",
                    self.debug_name(),
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
                        location,
                    );
                match days_in_month {
                    29 => false,
                    30 => true,
                    31 => {
                        icu_provider::log::trace!(
                            "({}) Found year {extended_year} AH with month length {days_in_month} for month {}.",
                            self.debug_name(),
                            month_idx + 1
                        );
                        excess_days += 1;
                        true
                    }
                    _ => {
                        debug_assert!(
                            !WELL_BEHAVED_ASTRONOMICAL_RANGE.contains(&start_day),
                            "({}) Found year {extended_year} AH with month length {days_in_month} for month {}!",
                            self.debug_name(),
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
                debug_assert!(
                    excess_days == 1 || !WELL_BEHAVED_ASTRONOMICAL_RANGE.contains(&start_day),
                    "({}) Found year {extended_year} AH with more than one excess day!",
                    self.debug_name()
                );
                if let Some(l) = month_lengths.iter_mut().find(|l| !(**l)) {
                    *l = true;
                }
            }
            month_lengths
        };
        HijriYearData::try_new(extended_year, start_day, month_lengths)
            .unwrap_or_else(|| UmmAlQura.year_data(extended_year))
    }
}

/// [`Hijri`] [`Rules`] for the [Umm al-Qura](https://en.wikipedia.org/wiki/Islamic_calendar#Saudi_Arabia's_Umm_al-Qura_calendar) calendar.
///
/// From the start of 1300 AH (1882-11-12 ISO) to the end of 1600 AH (2174-11-25 ISO), this
/// `Rules` implementation uses Umm al-Qura month lengths obtained from
/// [KACST](https://kacst.gov.sa/). Outside this range, this implementation falls back to
/// [`TabularAlgorithm`] with [`TabularAlgorithmLeapYears::TypeII`] and [`TabularAlgorithmEpoch::Friday`].
///
/// The precise behavior of this calendar may change in the future if:
/// - New ground truth is established by published government sources
/// - We decide to use a different algorithm outside the KACST range
/// - We decide to expand or reduce the range where we are correctly handling past dates.
///
/// This corresponds to the `"islamic-umalqura"` [CLDR calendar](https://unicode.org/reports/tr35/#UnicodeCalendarIdentifier).
#[derive(Copy, Clone, Debug, Default)]
#[non_exhaustive]
pub struct UmmAlQura;

impl crate::cal::scaffold::UnstableSealed for UmmAlQura {}
impl Rules for UmmAlQura {
    fn calendar_algorithm(&self) -> Option<CalendarAlgorithm> {
        Some(CalendarAlgorithm::Hijri(Some(
            HijriCalendarAlgorithm::Umalqura,
        )))
    }

    fn ecma_reference_year(
        &self,
        month_code: (u8, bool),
        day: u8,
    ) -> Result<i32, EcmaReferenceYearError> {
        let (ordinal_month, false) = month_code else {
            return Err(EcmaReferenceYearError::MonthCodeNotInCalendar);
        };

        let extended_year = match (ordinal_month, day) {
            (1, _) => 1392,
            (2, 30..) => 1390,
            (2, _) => 1392,
            (3, 30..) => 1391,
            (3, _) => 1392,
            (4, _) => 1392,
            (5, 30..) => 1391,
            (5, _) => 1392,
            (6, _) => 1392,
            (7, 30..) => 1389,
            (7, _) => 1392,
            (8, _) => 1392,
            (9, _) => 1392,
            (10, 30..) => 1390,
            (10, _) => 1392,
            (11, ..=25) => 1392,
            (11, _) => 1391,
            (12, 30..) => 1390,
            (12, _) => 1391,
            _ => return Err(EcmaReferenceYearError::MonthCodeNotInCalendar),
        };
        Ok(extended_year)
    }

    fn debug_name(&self) -> &'static str {
        "Hijri (Umm al-Qura)"
    }

    fn year_data(&self, extended_year: i32) -> HijriYearData {
        if let Some(data) = HijriYearData::lookup(
            extended_year,
            ummalqura_data::STARTING_YEAR,
            ummalqura_data::DATA,
        ) {
            data
        } else {
            TabularAlgorithm {
                leap_years: TabularAlgorithmLeapYears::TypeII,
                epoch: TabularAlgorithmEpoch::Friday,
            }
            .year_data(extended_year)
        }
    }
}

/// [`Hijri`] [`Rules`] for the [Tabular Hijri Algorithm](https://en.wikipedia.org/wiki/Tabular_Islamic_calendar).
///
/// See [`TabularAlgorithmEpoch`] and [`TabularAlgorithmLeapYears`] for customization.
///
/// The most common version of these rules uses [`TabularAlgorithmEpoch::Friday`] and [`TabularAlgorithmLeapYears::TypeII`].
///
/// When constructed with [`TabularAlgorithmLeapYears::TypeII`], and either [`TabularAlgorithmEpoch::Friday`] or [`TabularAlgorithmEpoch::Thursday`],
/// this corresponds to the `"islamic-civil"` and `"islamic-tbla"` [CLDR calendars](https://unicode.org/reports/tr35/#UnicodeCalendarIdentifier) respectively.
#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord)]
pub struct TabularAlgorithm {
    pub(crate) leap_years: TabularAlgorithmLeapYears,
    pub(crate) epoch: TabularAlgorithmEpoch,
}

impl TabularAlgorithm {
    /// Construct a new [`TabularAlgorithm`] with the given leap year rule and epoch.
    pub const fn new(leap_years: TabularAlgorithmLeapYears, epoch: TabularAlgorithmEpoch) -> Self {
        Self { epoch, leap_years }
    }
}

impl crate::cal::scaffold::UnstableSealed for TabularAlgorithm {}
impl Rules for TabularAlgorithm {
    fn calendar_algorithm(&self) -> Option<CalendarAlgorithm> {
        Some(match (self.epoch, self.leap_years) {
            (TabularAlgorithmEpoch::Friday, TabularAlgorithmLeapYears::TypeII) => {
                CalendarAlgorithm::Hijri(Some(HijriCalendarAlgorithm::Civil))
            }
            (TabularAlgorithmEpoch::Thursday, TabularAlgorithmLeapYears::TypeII) => {
                CalendarAlgorithm::Hijri(Some(HijriCalendarAlgorithm::Tbla))
            }
        })
    }

    fn ecma_reference_year(
        &self,
        month_code: (u8, bool),
        day: u8,
    ) -> Result<i32, EcmaReferenceYearError> {
        let (ordinal_month, false) = month_code else {
            return Err(EcmaReferenceYearError::MonthCodeNotInCalendar);
        };

        Ok(match (ordinal_month, day) {
            (1, _) => 1392,
            (2, 30..) => 1389,
            (2, _) => 1392,
            (3, _) => 1392,
            (4, 30..) => 1389,
            (4, _) => 1392,
            (5, _) => 1392,
            (6, 30..) => 1389,
            (6, _) => 1392,
            (7, _) => 1392,
            (8, 30..) => 1389,
            (8, _) => 1392,
            (9, _) => 1392,
            (10, 30..) => 1389,
            (10, _) => 1392,
            (11, ..=26) if self.epoch == TabularAlgorithmEpoch::Thursday => 1392,
            (11, ..=25) if self.epoch == TabularAlgorithmEpoch::Friday => 1392,
            (11, _) => 1391,
            (12, 30..) => 1390,
            (12, _) => 1391,
            _ => return Err(EcmaReferenceYearError::MonthCodeNotInCalendar),
        })
    }

    fn debug_name(&self) -> &'static str {
        match self.epoch {
            TabularAlgorithmEpoch::Friday => "Hijri (civil)",
            TabularAlgorithmEpoch::Thursday => "Hijri (astronomical)",
        }
    }

    fn year_data(&self, extended_year: i32) -> HijriYearData {
        let start_day = calendrical_calculations::islamic::fixed_from_tabular_islamic(
            extended_year,
            1,
            1,
            self.epoch.rata_die(),
        );
        let month_lengths = core::array::from_fn(|m| {
            m % 2 == 0
                || m == 11
                    && match self.leap_years {
                        TabularAlgorithmLeapYears::TypeII => {
                            (14 + 11 * extended_year as i64).rem_euclid(30) < 11
                        }
                    }
        });
        HijriYearData {
            // start_day is within 5 days of the tabular start day (trivial), and month lengths
            // has either 6 or 7 long months.
            packed: PackedHijriYearData::new_unchecked(extended_year, month_lengths, start_day),
            extended_year,
        }
    }
}

impl Hijri<AstronomicalSimulation> {
    /// Use [`Self::new_simulated_mecca`].
    #[cfg(feature = "compiled_data")]
    #[deprecated(since = "2.1.0", note = "use `Hijri::new_simulated_mecca`")]
    pub const fn new_mecca() -> Self {
        Self::new_simulated_mecca()
    }

    /// Creates a [`Hijri`] calendar using simulated sightings at Mecca.
    ///
    /// These simulations are unofficial and are known to not necessarily match sightings
    /// on the ground. Unless you know otherwise for sure, instead of this variant, use
    /// [`Hijri::new_umm_al_qura`], which uses the results of KACST's Mecca-based calculations.
    pub const fn new_simulated_mecca() -> Self {
        Self(AstronomicalSimulation {
            location: SimulatedLocation::Mecca,
        })
    }

    #[cfg(feature = "serde")]
    #[doc = icu_provider::gen_buffer_unstable_docs!(BUFFER,Self::new)]
    #[deprecated(since = "2.1.0", note = "use `Hijri::new_simulated_mecca`")]
    pub fn try_new_mecca_with_buffer_provider(
        _provider: &(impl icu_provider::buf::BufferProvider + ?Sized),
    ) -> Result<Self, DataError> {
        Ok(Self::new_simulated_mecca())
    }

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::new_mecca)]
    #[deprecated(since = "2.1.0", note = "use `Hijri::new_simulated_mecca`")]
    pub fn try_new_mecca_unstable<D: ?Sized>(_provider: &D) -> Result<Self, DataError> {
        Ok(Self::new_simulated_mecca())
    }

    /// Use [`Self::new_simulated_mecca`].
    #[deprecated(since = "2.1.0", note = "use `Hijri::new_simulated_mecca`")]
    pub const fn new_mecca_always_calculating() -> Self {
        Self::new_simulated_mecca()
    }
}

impl Hijri<UmmAlQura> {
    /// Use [`Self::new_umm_al_qura`]
    #[deprecated(since = "2.1.0", note = "use `Self::new_umm_al_qura`")]
    pub const fn new() -> Self {
        Self(UmmAlQura)
    }

    /// Creates a [`Hijri`] calendar using [`UmmAlQura`] rules.
    pub const fn new_umm_al_qura() -> Self {
        Self(UmmAlQura)
    }
}

/// The epoch for the [`TabularAlgorithm`] rules.
#[non_exhaustive]
#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord)]
pub enum TabularAlgorithmEpoch {
    /// Thusday July 15, 622 AD Julian (0622-07-18 ISO)
    Thursday,
    /// Friday July 16, 622 AD Julian (0622-07-19 ISO)
    Friday,
}

impl TabularAlgorithmEpoch {
    fn rata_die(self) -> RataDie {
        match self {
            Self::Thursday => ISLAMIC_EPOCH_THURSDAY,
            Self::Friday => ISLAMIC_EPOCH_FRIDAY,
        }
    }
}

/// The leap year rule for the [`TabularAlgorithm`] rules.
///
/// This specifies which years of a 30-year cycle have an additional day at
/// the end of the year.
#[non_exhaustive]
#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord)]
pub enum TabularAlgorithmLeapYears {
    /// Leap years 2, 5, 7, 10, 13, 16, 18, 21, 24, 26, 29
    TypeII,
}

impl Hijri<TabularAlgorithm> {
    /// Use [`Self::new_tabular`]
    #[deprecated(since = "2.1.0", note = "use `Hijri::new_tabular`")]
    pub const fn new(leap_years: TabularAlgorithmLeapYears, epoch: TabularAlgorithmEpoch) -> Self {
        Hijri::new_tabular(leap_years, epoch)
    }

    /// Creates a [`Hijri`] calendar with tabular rules and the given leap year rule and epoch.
    pub const fn new_tabular(
        leap_years: TabularAlgorithmLeapYears,
        epoch: TabularAlgorithmEpoch,
    ) -> Self {
        Self(TabularAlgorithm::new(leap_years, epoch))
    }
}

/// Information about a Hijri year.
#[derive(Copy, Clone, Debug, Hash, PartialEq, Eq, PartialOrd, Ord)]
pub struct HijriYearData {
    packed: PackedHijriYearData,
    extended_year: i32,
}

impl ToExtendedYear for HijriYearData {
    fn to_extended_year(&self) -> i32 {
        self.extended_year
    }
}

impl HijriYearData {
    /// Creates [`HijriYearData`] from the given parts.
    ///
    /// `start_day` is the date for the first day of the year, see [`Date::to_rata_die`]
    /// to obtain a [`RataDie`] from a [`Date`] in an arbitrary calendar. `start_day` has
    /// to be within 5 days of the start of the year of the [`TabularAlgorithm`].
    ///
    /// `month_lengths[n - 1]` is true if the nth month has 30 days, and false otherwise.
    /// Either 6 or 7 months need to have 30 days.
    pub fn try_new(
        extended_year: i32,
        start_day: RataDie,
        month_lengths: [bool; 12],
    ) -> Option<Self> {
        Some(Self {
            packed: PackedHijriYearData::try_new(extended_year, month_lengths, start_day)?,
            extended_year,
        })
    }

    fn lookup(
        extended_year: i32,
        starting_year: i32,
        data: &[PackedHijriYearData],
    ) -> Option<Self> {
        Some(extended_year)
            .and_then(|e| usize::try_from(e.checked_sub(starting_year)?).ok())
            .and_then(|i| data.get(i))
            .map(|&packed| Self {
                extended_year,
                packed,
            })
    }

    fn new_year(self) -> RataDie {
        self.packed.new_year(self.extended_year)
    }
}

/// The struct containing compiled Hijri YearInfo
///
/// * `start_day` has to be within 5 days of the start of the year of the [`TabularAlgorithm`].
/// * `month_lengths[n - 1]` has either 6 or 7 long months.
///
/// Bit structure
///
/// ```text
/// Bit:              F.........C  B.............0
/// Value:           [ start day ][ month lengths ]
/// ```
///
/// The start day is encoded as a signed offset from `Self::mean_tabular_start_day`. This number does not
/// appear to be less than 2, however we use all remaining bits for it in case of drift in the math.
/// The month lengths are stored as 1 = 30, 0 = 29 for each month including the leap month.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Copy, Clone, Hash, PartialEq, Eq, PartialOrd, Ord, Debug)]
struct PackedHijriYearData(u16);

impl PackedHijriYearData {
    const fn try_new(
        extended_year: i32,
        month_lengths: [bool; 12],
        start_day: RataDie,
    ) -> Option<Self> {
        let start_offset = start_day.since(Self::mean_tabular_start_day(extended_year));

        if !(-8 < start_offset && start_offset < 8
            || calendrical_calculations::islamic::WELL_BEHAVED_ASTRONOMICAL_RANGE
                .start
                .to_i64_date()
                > start_day.to_i64_date()
            || calendrical_calculations::islamic::WELL_BEHAVED_ASTRONOMICAL_RANGE
                .end
                .to_i64_date()
                < start_day.to_i64_date())
        {
            return None;
        }
        let start_offset = start_offset as i8 & 0b1000_0111u8 as i8;

        let mut all = 0u16;

        let mut num_days = 29 * 12;

        let mut i = 0;
        while i < 12 {
            #[expect(clippy::indexing_slicing)]
            if month_lengths[i] {
                all |= 1 << i;
                num_days += 1;
            }
            i += 1;
        }

        if !matches!(num_days, 354 | 355) {
            return None;
        }

        if start_offset < 0 {
            all |= 1 << 12;
        }
        all |= (start_offset.unsigned_abs() as u16) << 13;
        Some(Self(all))
    }

    const fn new_unchecked(
        extended_year: i32,
        month_lengths: [bool; 12],
        start_day: RataDie,
    ) -> Self {
        let start_offset = start_day.since(Self::mean_tabular_start_day(extended_year));

        let start_offset = start_offset as i8 & 0b1000_0111u8 as i8;

        let mut all = 0u16;

        let mut i = 0;
        while i < 12 {
            #[expect(clippy::indexing_slicing)]
            if month_lengths[i] {
                all |= 1 << i;
            }
            i += 1;
        }

        if start_offset < 0 {
            all |= 1 << 12;
        }
        all |= (start_offset.unsigned_abs() as u16) << 13;
        Self(all)
    }

    fn new_year(self, extended_year: i32) -> RataDie {
        let start_offset = if (self.0 & 0b1_0000_0000_0000) != 0 {
            -((self.0 >> 13) as i64)
        } else {
            (self.0 >> 13) as i64
        };
        Self::mean_tabular_start_day(extended_year) + start_offset
    }

    fn month_has_30_days(self, month: u8) -> bool {
        self.0 & (1 << (month - 1) as u16) != 0
    }

    fn is_leap(self) -> bool {
        (self.0 & ((1 << 12) - 1)).count_ones() == 7
    }

    // month is 1-indexed, but 0 is a valid input, producing 0
    fn last_day_of_month(self, month: u8) -> u16 {
        // month is 1-indexed, so `29 * month` includes the current month
        let mut prev_month_lengths = 29 * month as u16;
        // month is 1-indexed, so `1 << month` is a mask with all zeroes except
        // for a 1 at the bit index at the next month. Subtracting 1 from it gets us
        // a bitmask for all months up to now
        let long_month_bits = self.0 & ((1 << month as u16) - 1);
        prev_month_lengths += long_month_bits.count_ones().try_into().unwrap_or(0);
        prev_month_lengths
    }

    fn days_in_year(self) -> u16 {
        self.last_day_of_month(12)
    }

    const fn mean_tabular_start_day(extended_year: i32) -> RataDie {
        // -1 because the epoch is new year of year 1
        calendrical_calculations::islamic::ISLAMIC_EPOCH_FRIDAY
            .add((extended_year as i64 - 1) * (354 * 30 + 11) / 30)
    }
}

impl<A: AsCalendar<Calendar = Hijri<AstronomicalSimulation>>> Date<A> {
    /// Deprecated
    #[deprecated(since = "2.1.0", note = "use `Date::try_new_hijri_with_calendar`")]
    pub fn try_new_simulated_hijri_with_calendar(
        year: i32,
        month: u8,
        day: u8,
        calendar: A,
    ) -> Result<Date<A>, RangeError> {
        Date::try_new_hijri_with_calendar(year, month, day, calendar)
    }
}

#[test]
fn computer_reference_years() {
    let rules = UmmAlQura;

    fn compute_hijri_reference_year<C>(
        ordinal_month: u8,
        day: u8,
        cal: &C,
        year_info_from_extended: impl Fn(i32) -> C::YearInfo,
    ) -> Result<C::YearInfo, DateError>
    where
        C: DateFieldsResolver,
    {
        let dec_31 = Date::from_rata_die(
            crate::cal::abstract_gregorian::LAST_DAY_OF_REFERENCE_YEAR,
            crate::Ref(cal),
        );
        // December 31, 1972 occurs in the 11th month, 1392 AH, but the day could vary
        debug_assert_eq!(dec_31.month().ordinal, 11);
        let (y0, y1, y2, y3) =
            if ordinal_month < 11 || (ordinal_month == 11 && day <= dec_31.day_of_month().0) {
                (1389, 1390, 1391, 1392)
            } else {
                (1388, 1389, 1390, 1391)
            };
        let year_info = year_info_from_extended(y3);
        if day <= C::days_in_provided_month(year_info, ordinal_month) {
            return Ok(year_info);
        }
        let year_info = year_info_from_extended(y2);
        if day <= C::days_in_provided_month(year_info, ordinal_month) {
            return Ok(year_info);
        }
        let year_info = year_info_from_extended(y1);
        if day <= C::days_in_provided_month(year_info, ordinal_month) {
            return Ok(year_info);
        }
        let year_info = year_info_from_extended(y0);
        // This function might be called with out-of-range days that are handled later.
        // Some calendars don't have day 30s in every month so we don't check those.
        if day <= 29 {
            debug_assert!(
                day <= C::days_in_provided_month(year_info, ordinal_month),
                "{ordinal_month}/{day}"
            );
        }
        Ok(year_info)
    }
    for month in 1..=12 {
        for day in [30, 29] {
            let y = compute_hijri_reference_year(month, day, &Hijri(rules), |e| rules.year_data(e))
                .unwrap()
                .extended_year;

            if day == 30 {
                println!("({month}, {day}) => {y},")
            } else {
                println!("({month}, _) => {y},")
            }
        }
    }
}

#[allow(clippy::derived_hash_with_manual_eq)] // bounds
#[derive(Clone, Debug, Hash)]
/// The inner date type used for representing [`Date`]s of [`Hijri`]. See [`Date`] and [`Hijri`] for more details.
pub struct HijriDateInner<R: Rules>(ArithmeticDate<Hijri<R>>);

impl<R: Rules> Copy for HijriDateInner<R> {}
impl<R: Rules> PartialEq for HijriDateInner<R> {
    fn eq(&self, other: &Self) -> bool {
        self.0 == other.0
    }
}
impl<R: Rules> Eq for HijriDateInner<R> {}
impl<R: Rules> PartialOrd for HijriDateInner<R> {
    fn partial_cmp(&self, other: &Self) -> Option<core::cmp::Ordering> {
        Some(self.cmp(other))
    }
}
impl<R: Rules> Ord for HijriDateInner<R> {
    fn cmp(&self, other: &Self) -> core::cmp::Ordering {
        self.0.cmp(&other.0)
    }
}

impl<R: Rules> DateFieldsResolver for Hijri<R> {
    type YearInfo = HijriYearData;

    fn days_in_provided_month(year: Self::YearInfo, month: u8) -> u8 {
        29 + year.packed.month_has_30_days(month) as u8
    }

    fn months_in_provided_year(_year: Self::YearInfo) -> u8 {
        12
    }

    #[inline]
    fn year_info_from_era(
        &self,
        era: &[u8],
        era_year: i32,
    ) -> Result<Self::YearInfo, UnknownEraError> {
        let extended_year = match era {
            b"ah" => era_year,
            b"bh" => 1 - era_year,
            _ => return Err(UnknownEraError),
        };
        Ok(self.year_info_from_extended(extended_year))
    }

    #[inline]
    fn year_info_from_extended(&self, extended_year: i32) -> Self::YearInfo {
        self.0.year_data(extended_year)
    }

    #[inline]
    fn reference_year_from_month_day(
        &self,
        month_code: types::ValidMonthCode,
        day: u8,
    ) -> Result<Self::YearInfo, EcmaReferenceYearError> {
        self.0
            .ecma_reference_year(month_code.to_tuple(), day)
            .map(|y| self.0.year_data(y))
    }
}

impl<R: Rules> crate::cal::scaffold::UnstableSealed for Hijri<R> {}
impl<R: Rules> Calendar for Hijri<R> {
    type DateInner = HijriDateInner<R>;
    type Year = types::EraYear;
    type DifferenceError = core::convert::Infallible;

    fn from_codes(
        &self,
        era: Option<&str>,
        year: i32,
        month_code: types::MonthCode,
        day: u8,
    ) -> Result<Self::DateInner, DateError> {
        ArithmeticDate::from_codes(era, year, month_code, day, self).map(HijriDateInner)
    }

    #[cfg(feature = "unstable")]
    fn from_fields(
        &self,
        fields: DateFields,
        options: DateFromFieldsOptions,
    ) -> Result<Self::DateInner, DateFromFieldsError> {
        ArithmeticDate::from_fields(fields, options, self).map(HijriDateInner)
    }

    fn from_rata_die(&self, rd: RataDie) -> Self::DateInner {
        // (354 * 30 + 11) / 30 is the mean year length for a tabular year
        // This is slightly different from the `calendrical_calculations::islamic::MEAN_YEAR_LENGTH`, which is based on
        // the (current) synodic month length.
        //
        // +1 because the epoch is new year of year 1
        // Before the epoch the division will round up (towards 0), so we need to
        // subtract 1, which is the same as not adding the 1.
        let extended_year = (rd - calendrical_calculations::islamic::ISLAMIC_EPOCH_FRIDAY) * 30
            / (354 * 30 + 11)
            + (rd >= calendrical_calculations::islamic::ISLAMIC_EPOCH_FRIDAY) as i64;

        let extended_year = extended_year.clamp(i32::MIN as i64, i32::MAX as i64) as i32;

        let mut year = self.0.year_data(extended_year);

        // We rounded the extended year down, so we might need to use the next year
        if rd >= year.new_year() + year.packed.days_in_year() as i64 && extended_year < i32::MAX {
            year = self.0.year_data(year.extended_year + 1)
        }

        // Clamp the RD to our year
        let rd = rd.clamp(
            year.new_year(),
            year.new_year() + year.packed.days_in_year() as i64,
        );

        let day_of_year = (rd - year.new_year()) as u16;

        // We divide by 30, not 29, to account for the case where all months before this
        // were length 30 (possible near the beginning of the year)
        let mut month = (day_of_year / 30) as u8 + 1;
        let mut last_day_of_month = year.packed.last_day_of_month(month);
        let mut last_day_of_prev_month = year.packed.last_day_of_month(month - 1);

        while day_of_year >= last_day_of_month {
            month += 1;
            last_day_of_prev_month = last_day_of_month;
            last_day_of_month = year.packed.last_day_of_month(month);
        }

        let day = (day_of_year + 1 - last_day_of_prev_month) as u8;

        HijriDateInner(ArithmeticDate::new_unchecked(year, month, day))
    }

    fn to_rata_die(&self, date: &Self::DateInner) -> RataDie {
        date.0.year.new_year()
            + date.0.year.packed.last_day_of_month(date.0.month - 1) as i64
            + (date.0.day - 1) as i64
    }

    fn has_cheap_iso_conversion(&self) -> bool {
        false
    }

    fn months_in_year(&self, date: &Self::DateInner) -> u8 {
        Self::months_in_provided_year(date.0.year)
    }

    fn days_in_year(&self, date: &Self::DateInner) -> u16 {
        date.0.year.packed.days_in_year()
    }

    fn days_in_month(&self, date: &Self::DateInner) -> u8 {
        Self::days_in_provided_month(date.0.year, date.0.month)
    }

    #[cfg(feature = "unstable")]
    fn add(
        &self,
        date: &Self::DateInner,
        duration: types::DateDuration,
        options: DateAddOptions,
    ) -> Result<Self::DateInner, DateError> {
        date.0.added(duration, self, options).map(HijriDateInner)
    }

    #[cfg(feature = "unstable")]
    fn until(
        &self,
        date1: &Self::DateInner,
        date2: &Self::DateInner,
        options: DateDifferenceOptions,
    ) -> Result<types::DateDuration, Self::DifferenceError> {
        Ok(date1.0.until(&date2.0, self, options))
    }

    fn debug_name(&self) -> &'static str {
        self.0.debug_name()
    }

    fn year_info(&self, date: &Self::DateInner) -> Self::Year {
        let extended_year = date.0.year.extended_year;
        if extended_year > 0 {
            types::EraYear {
                era: tinystr!(16, "ah"),
                era_index: Some(0),
                year: extended_year,
                extended_year,
                ambiguity: types::YearAmbiguity::CenturyRequired,
            }
        } else {
            types::EraYear {
                era: tinystr!(16, "bh"),
                era_index: Some(1),
                year: 1 - extended_year,
                extended_year,
                ambiguity: types::YearAmbiguity::CenturyRequired,
            }
        }
    }

    fn is_in_leap_year(&self, date: &Self::DateInner) -> bool {
        date.0.year.packed.is_leap()
    }

    fn month(&self, date: &Self::DateInner) -> types::MonthInfo {
        types::MonthInfo::non_lunisolar(date.0.month)
    }

    fn day_of_month(&self, date: &Self::DateInner) -> types::DayOfMonth {
        types::DayOfMonth(date.0.day)
    }

    fn day_of_year(&self, date: &Self::DateInner) -> types::DayOfYear {
        types::DayOfYear(date.0.year.packed.last_day_of_month(date.0.month - 1) + date.0.day as u16)
    }

    fn calendar_algorithm(&self) -> Option<crate::preferences::CalendarAlgorithm> {
        self.0.calendar_algorithm()
    }
}

impl<A: AsCalendar<Calendar = Hijri<R>>, R: Rules> Date<A> {
    /// Construct new Hijri Date.
    ///
    /// ```rust
    /// use icu::calendar::cal::Hijri;
    /// use icu::calendar::Date;
    ///
    /// let hijri = Hijri::new_simulated_mecca();
    ///
    /// let date_hijri = Date::try_new_hijri_with_calendar(1392, 4, 25, hijri)
    ///     .expect("Failed to initialize Hijri Date instance.");
    ///
    /// assert_eq!(date_hijri.era_year().year, 1392);
    /// assert_eq!(date_hijri.month().ordinal, 4);
    /// assert_eq!(date_hijri.day_of_month().0, 25);
    /// ```
    pub fn try_new_hijri_with_calendar(
        year: i32,
        month: u8,
        day: u8,
        calendar: A,
    ) -> Result<Self, RangeError> {
        let y = calendar.as_calendar().0.year_data(year);
        Ok(Date::from_raw(
            HijriDateInner(ArithmeticDate::try_from_ymd(y, month, day)?),
            calendar,
        ))
    }
}

impl Date<Hijri<UmmAlQura>> {
    /// Deprecated
    #[deprecated(since = "2.1.0", note = "use `Date::try_new_hijri_with_calendar")]
    pub fn try_new_ummalqura(year: i32, month: u8, day: u8) -> Result<Self, RangeError> {
        Date::try_new_hijri_with_calendar(year, month, day, Hijri::new_umm_al_qura())
    }
}

impl<A: AsCalendar<Calendar = Hijri<TabularAlgorithm>>> Date<A> {
    /// Deprecated
    #[deprecated(since = "2.1.0", note = "use `Date::try_new_hijri_with_calendar")]
    pub fn try_new_hijri_tabular_with_calendar(
        year: i32,
        month: u8,
        day: u8,
        calendar: A,
    ) -> Result<Date<A>, RangeError> {
        Date::try_new_hijri_with_calendar(year, month, day, calendar)
    }
}

#[cfg(test)]
mod test {
    use types::MonthCode;

    use super::*;

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
        let calendar = Hijri::new_simulated_mecca();
        for (case, f_date) in SIMULATED_CASES.iter().zip(TEST_RD.iter()) {
            let date = Date::try_new_hijri_with_calendar(case.year, case.month, case.day, calendar)
                .unwrap();
            let iso = Date::from_rata_die(RataDie::new(*f_date), crate::Iso);

            assert_eq!(iso.to_calendar(calendar).inner, date.inner, "{case:?}");
        }
    }

    #[test]
    fn test_rd_from_simulated_hijri() {
        let calendar = Hijri::new_simulated_mecca();
        for (case, f_date) in SIMULATED_CASES.iter().zip(TEST_RD.iter()) {
            let date = Date::try_new_hijri_with_calendar(case.year, case.month, case.day, calendar)
                .unwrap();
            assert_eq!(date.to_rata_die(), RataDie::new(*f_date), "{case:?}");
        }
    }

    #[test]
    fn test_rd_from_hijri() {
        let calendar = Hijri::new_tabular(
            TabularAlgorithmLeapYears::TypeII,
            TabularAlgorithmEpoch::Friday,
        );
        for (case, f_date) in ARITHMETIC_CASES.iter().zip(TEST_RD.iter()) {
            let date = Date::try_new_hijri_with_calendar(case.year, case.month, case.day, calendar)
                .unwrap();
            assert_eq!(date.to_rata_die(), RataDie::new(*f_date), "{case:?}");
        }
    }

    #[test]
    fn test_hijri_from_rd() {
        let calendar = Hijri::new_tabular(
            TabularAlgorithmLeapYears::TypeII,
            TabularAlgorithmEpoch::Friday,
        );
        for (case, f_date) in ARITHMETIC_CASES.iter().zip(TEST_RD.iter()) {
            let date = Date::try_new_hijri_with_calendar(case.year, case.month, case.day, calendar)
                .unwrap();
            let date_rd = Date::from_rata_die(RataDie::new(*f_date), calendar);

            assert_eq!(date, date_rd, "{case:?}");
        }
    }

    #[test]
    fn test_rd_from_hijri_tbla() {
        let calendar = Hijri::new_tabular(
            TabularAlgorithmLeapYears::TypeII,
            TabularAlgorithmEpoch::Thursday,
        );
        for (case, f_date) in ASTRONOMICAL_CASES.iter().zip(TEST_RD.iter()) {
            let date = Date::try_new_hijri_with_calendar(case.year, case.month, case.day, calendar)
                .unwrap();
            assert_eq!(date.to_rata_die(), RataDie::new(*f_date), "{case:?}");
        }
    }

    #[test]
    fn test_hijri_tbla_from_rd() {
        let calendar = Hijri::new_tabular(
            TabularAlgorithmLeapYears::TypeII,
            TabularAlgorithmEpoch::Thursday,
        );
        for (case, f_date) in ASTRONOMICAL_CASES.iter().zip(TEST_RD.iter()) {
            let date = Date::try_new_hijri_with_calendar(case.year, case.month, case.day, calendar)
                .unwrap();
            let date_rd = Date::from_rata_die(RataDie::new(*f_date), calendar);

            assert_eq!(date, date_rd, "{case:?}");
        }
    }

    #[test]
    fn test_saudi_hijri_from_rd() {
        let calendar = Hijri::new_umm_al_qura();
        for (case, f_date) in UMMALQURA_CASES.iter().zip(TEST_RD.iter()) {
            let date = Date::try_new_hijri_with_calendar(case.year, case.month, case.day, calendar)
                .unwrap();
            let date_rd = Date::from_rata_die(RataDie::new(*f_date), calendar);

            assert_eq!(date, date_rd, "{case:?}");
        }
    }

    #[test]
    fn test_rd_from_saudi_hijri() {
        let calendar = Hijri::new_umm_al_qura();
        for (case, f_date) in UMMALQURA_CASES.iter().zip(TEST_RD.iter()) {
            let date = Date::try_new_hijri_with_calendar(case.year, case.month, case.day, calendar)
                .unwrap();
            assert_eq!(date.to_rata_die(), RataDie::new(*f_date), "{case:?}");
        }
    }

    #[ignore] // slow
    #[test]
    fn test_days_in_provided_year_simulated() {
        let calendar = Hijri::new_simulated_mecca();
        // -1245 1 1 = -214526 (R.D Date)
        // 1518 1 1 = 764589 (R.D Date)
        let sum_days_in_year: i64 = (START_YEAR..END_YEAR)
            .map(|year| {
                Hijri::new_simulated_mecca()
                    .0
                    .year_data(year)
                    .packed
                    .days_in_year() as i64
            })
            .sum();
        let expected_number_of_days = Date::try_new_hijri_with_calendar(END_YEAR, 1, 1, calendar)
            .unwrap()
            .to_rata_die()
            - Date::try_new_hijri_with_calendar(START_YEAR, 1, 1, calendar)
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
        let calendar = Hijri::new_umm_al_qura();
        // -1245 1 1 = -214528 (R.D Date)
        // 1518 1 1 = 764588 (R.D Date)
        let sum_days_in_year: i64 = (START_YEAR..END_YEAR)
            .map(|year| calendar.0.year_data(year).packed.days_in_year() as i64)
            .sum();
        let expected_number_of_days = Date::try_new_hijri_with_calendar(END_YEAR, 1, 1, calendar)
            .unwrap()
            .to_rata_die()
            - (Date::try_new_hijri_with_calendar(START_YEAR, 1, 1, calendar).unwrap())
                .to_rata_die(); // The number of days between Umm al-Qura Hijri years -1245 and 1518

        assert_eq!(sum_days_in_year, expected_number_of_days);
    }

    #[test]
    fn test_regression_3868() {
        // This date used to panic on creation
        let iso = Date::try_new_iso(2011, 4, 4).unwrap();
        let hijri = iso.to_calendar(Hijri::new_umm_al_qura());
        // Data from https://www.ummulqura.org.sa/Index.aspx
        assert_eq!(hijri.day_of_month().0, 30);
        assert_eq!(hijri.month().ordinal, 4);
        assert_eq!(hijri.era_year().year, 1432);
    }

    #[test]
    fn test_regression_4914() {
        // https://github.com/unicode-org/icu4x/issues/4914
        let dt = Hijri::new_umm_al_qura()
            .from_codes(Some("bh"), 6824, MonthCode::new_normal(1).unwrap(), 1)
            .unwrap();
        assert_eq!(dt.0.day, 1);
        assert_eq!(dt.0.month, 1);
        assert_eq!(dt.0.year.extended_year, -6823);
    }

    #[test]
    fn test_regression_7056() {
        // https://github.com/unicode-org/icu4x/issues/7056
        let calendar = Hijri::new_tabular(
            TabularAlgorithmLeapYears::TypeII,
            TabularAlgorithmEpoch::Friday,
        );
        let iso = Date::try_new_iso(-62971, 3, 19).unwrap();
        let _dt = iso.to_calendar(calendar);
        let _dt = iso.to_calendar(Hijri::new_umm_al_qura());
    }

    #[test]
    fn test_regression_5069_uaq() {
        let calendar = Hijri::new_umm_al_qura();

        let dt = Date::try_new_hijri_with_calendar(1391, 1, 29, calendar).unwrap();

        assert_eq!(dt.to_iso().to_calendar(calendar), dt);
    }

    #[test]
    fn test_regression_5069_obs() {
        let cal = Hijri::new_simulated_mecca();

        let dt = Date::try_new_hijri_with_calendar(1390, 1, 30, cal).unwrap();

        assert_eq!(dt.to_iso().to_calendar(cal), dt);

        let dt = Date::try_new_iso(2000, 5, 5).unwrap();

        assert!(dt.to_calendar(cal).day_of_month().0 > 0);
    }

    #[test]
    fn test_regression_6197() {
        let calendar = Hijri::new_umm_al_qura();

        let iso = Date::try_new_iso(2025, 2, 26).unwrap();

        let date = iso.to_calendar(calendar);

        // Data from https://www.ummulqura.org.sa/
        assert_eq!(
            (
                date.day_of_month().0,
                date.month().ordinal,
                date.era_year().year
            ),
            (27, 8, 1446)
        );
    }

    #[test]
    fn test_hijri_packed_roundtrip() {
        fn single_roundtrip(month_lengths: [bool; 12], start_day: RataDie) -> Option<()> {
            let packed = PackedHijriYearData::try_new(1600, month_lengths, start_day)?;
            for i in 0..12 {
                assert_eq!(packed.month_has_30_days(i + 1), month_lengths[i as usize]);
            }
            assert_eq!(packed.new_year(1600), start_day);
            Some(())
        }

        let l = true;
        let s = false;
        let all_short = [s; 12];
        let all_long = [l; 12];
        let mixed1 = [l, s, l, s, l, s, l, s, l, s, l, s];
        let mixed2 = [s, s, l, l, l, s, l, s, s, s, l, l];

        let start_1600 = PackedHijriYearData::mean_tabular_start_day(1600);
        assert_eq!(single_roundtrip(all_short, start_1600), None);
        assert_eq!(single_roundtrip(all_long, start_1600), None);
        single_roundtrip(mixed1, start_1600).unwrap();
        single_roundtrip(mixed2, start_1600).unwrap();

        single_roundtrip(mixed1, start_1600 - 7).unwrap();
        single_roundtrip(mixed2, start_1600 + 7).unwrap();
        single_roundtrip(mixed2, start_1600 + 4).unwrap();
        single_roundtrip(mixed2, start_1600 + 1).unwrap();
        single_roundtrip(mixed2, start_1600 - 1).unwrap();
        single_roundtrip(mixed2, start_1600 - 4).unwrap();
    }
}
