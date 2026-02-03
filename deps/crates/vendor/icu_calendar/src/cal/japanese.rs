// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! This module contains types and implementations for the Japanese calendar.
//!
//! ```rust
//! use icu::calendar::cal::Japanese;
//! use icu::calendar::Date;
//! use tinystr::tinystr;
//!
//! let japanese_calendar = Japanese::new();
//!
//! let date_iso = Date::try_new_iso(1970, 1, 2)
//!     .expect("Failed to initialize ISO Date instance.");
//! let date_japanese = Date::new_from_iso(date_iso, japanese_calendar);
//!
//! assert_eq!(date_japanese.era_year().year, 45);
//! assert_eq!(date_japanese.month().ordinal, 1);
//! assert_eq!(date_japanese.day_of_month().0, 2);
//! assert_eq!(date_japanese.era_year().era, "showa");
//! ```

use crate::cal::iso::{Iso, IsoDateInner};
use crate::error::{year_check, DateError};
use crate::provider::{CalendarJapaneseExtendedV1, CalendarJapaneseModernV1, EraStartDate};
use crate::{types, AsCalendar, Calendar, Date, DateDuration, DateDurationUnit, Ref};
use calendrical_calculations::rata_die::RataDie;
use icu_provider::prelude::*;
use tinystr::{tinystr, TinyStr16};

/// The [Japanese Calendar] (with modern eras only)
///
/// The [Japanese calendar] is a solar calendar used in Japan, with twelve months.
/// The months and days are identical to that of the Gregorian calendar, however the years are counted
/// differently using the Japanese era system.
///
/// This calendar only contains eras after Meiji, for all historical eras, check out [`JapaneseExtended`].
///
/// This type can be used with [`Date`] to represent dates in this calendar.
///
/// [Japanese calendar]: https://en.wikipedia.org/wiki/Japanese_calendar
///
/// # Era codes
///
/// This calendar currently supports seven era codes. It supports the five post-Meiji eras
/// (`meiji`, `taisho`, `showa`, `heisei`, `reiwa`), as well as using the Gregorian
/// `bce` (alias `bc`), and `ce` (alias `ad`) for dates before the Meiji era.
///
/// Future eras will also be added to this type when they are decided.
///
/// These eras are loaded from data, requiring a data provider capable of providing [`CalendarJapaneseModernV1`]
/// data.
///
/// # Month codes
///
/// This calendar supports 12 solar month codes (`M01` - `M12`)
#[derive(Clone, Debug, Default)]
pub struct Japanese {
    eras: DataPayload<CalendarJapaneseModernV1>,
}

/// The [Japanese Calendar] (with historical eras)
///
/// The [Japanese calendar] is a solar calendar used in Japan, with twelve months.
/// The months and days are identical to that of the Gregorian calendar, however the years are counted
/// differently using the Japanese era system.
///
/// This type can be used with [`Date`] to represent dates in this calendar.
///
/// [Japanese calendar]: https://en.wikipedia.org/wiki/Japanese_calendar
///
/// # Era codes
///
/// This calendar supports a large number of era codes. It supports the five post-Meiji eras
/// (`meiji`, `taisho`, `showa`, `heisei`, `reiwa`). Pre-Meiji eras are represented
/// with their names converted to lowercase ascii and followed by their start year. E.g. the *Ten'ō*
/// era (781 - 782 CE) has the code `teno-781`. The  Gregorian `bce` (alias `bc`), and `ce` (alias `ad`)
/// are used for dates before the first known era era.
///
///
/// These eras are loaded from data, requiring a data provider capable of providing [`CalendarJapaneseExtendedV1`]
/// data.
///
/// # Month codes
///
/// This calendar supports 12 solar month codes (`M01` - `M12`)
#[derive(Clone, Debug, Default)]
pub struct JapaneseExtended(Japanese);

#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord)]
/// The inner date type used for representing [`Date`]s of [`Japanese`]. See [`Date`] and [`Japanese`] for more details.
pub struct JapaneseDateInner {
    inner: IsoDateInner,
    adjusted_year: i32,
    era: TinyStr16,
}

impl Japanese {
    /// Creates a new [`Japanese`] using only modern eras (post-meiji) from compiled data.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub const fn new() -> Self {
        Self {
            eras: DataPayload::from_static_ref(
                crate::provider::Baked::SINGLETON_CALENDAR_JAPANESE_MODERN_V1,
            ),
        }
    }

    icu_provider::gen_buffer_data_constructors!(() -> error: DataError,
        functions: [
            new: skip,
            try_new_with_buffer_provider,
            try_new_unstable,
            Self,
    ]);

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::new)]
    pub fn try_new_unstable<D: DataProvider<CalendarJapaneseModernV1> + ?Sized>(
        provider: &D,
    ) -> Result<Self, DataError> {
        Ok(Self {
            eras: provider.load(Default::default())?.payload,
        })
    }

    pub(crate) const DEBUG_NAME: &'static str = "Japanese";
}

impl JapaneseExtended {
    /// Creates a new [`Japanese`] from using all eras (including pre-meiji) from compiled data.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub const fn new() -> Self {
        Self(Japanese {
            eras: DataPayload::from_static_ref(
                crate::provider::Baked::SINGLETON_CALENDAR_JAPANESE_EXTENDED_V1,
            ),
        })
    }

    icu_provider::gen_buffer_data_constructors!(() -> error: DataError,
        functions: [
            new: skip,
            try_new_with_buffer_provider,
            try_new_unstable,
            Self,
    ]);

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::new)]
    pub fn try_new_unstable<D: DataProvider<CalendarJapaneseExtendedV1> + ?Sized>(
        provider: &D,
    ) -> Result<Self, DataError> {
        Ok(Self(Japanese {
            eras: provider.load(Default::default())?.payload.cast(),
        }))
    }

    pub(crate) const DEBUG_NAME: &'static str = "Japanese (historical era data)";
}

impl crate::cal::scaffold::UnstableSealed for Japanese {}
impl Calendar for Japanese {
    type DateInner = JapaneseDateInner;
    type Year = types::EraYear;

    fn from_codes(
        &self,
        era: Option<&str>,
        year: i32,
        month_code: types::MonthCode,
        day: u8,
    ) -> Result<Self::DateInner, DateError> {
        let Some((month, false)) = month_code.parsed() else {
            return Err(DateError::UnknownMonthCode(month_code));
        };

        if month > 12 {
            return Err(DateError::UnknownMonthCode(month_code));
        }

        self.new_japanese_date_inner(era.unwrap_or("ce"), year, month, day)
    }

    fn from_rata_die(&self, rd: RataDie) -> Self::DateInner {
        self.from_iso(Iso.from_rata_die(rd))
    }

    fn to_rata_die(&self, date: &Self::DateInner) -> RataDie {
        Iso.to_rata_die(&self.to_iso(date))
    }

    fn from_iso(&self, iso: IsoDateInner) -> JapaneseDateInner {
        let (adjusted_year, era) = self.adjusted_year_for(iso);
        JapaneseDateInner {
            inner: iso,
            adjusted_year,
            era,
        }
    }

    fn to_iso(&self, date: &Self::DateInner) -> IsoDateInner {
        date.inner
    }

    fn months_in_year(&self, date: &Self::DateInner) -> u8 {
        Iso.months_in_year(&date.inner)
    }

    fn days_in_year(&self, date: &Self::DateInner) -> u16 {
        Iso.days_in_year(&date.inner)
    }

    fn days_in_month(&self, date: &Self::DateInner) -> u8 {
        Iso.days_in_month(&date.inner)
    }

    fn offset_date(&self, date: &mut Self::DateInner, offset: DateDuration<Self>) {
        Iso.offset_date(&mut date.inner, offset.cast_unit());
        let (adjusted_year, era) = self.adjusted_year_for(date.inner);
        date.adjusted_year = adjusted_year;
        date.era = era
    }

    fn until(
        &self,
        date1: &Self::DateInner,
        date2: &Self::DateInner,
        _calendar2: &Self,
        largest_unit: DateDurationUnit,
        smallest_unit: DateDurationUnit,
    ) -> DateDuration<Self> {
        Iso.until(
            &date1.inner,
            &date2.inner,
            &Iso,
            largest_unit,
            smallest_unit,
        )
        .cast_unit()
    }

    fn year_info(&self, date: &Self::DateInner) -> Self::Year {
        types::EraYear {
            era: date.era,
            era_index: None,
            year: date.adjusted_year,
            ambiguity: types::YearAmbiguity::CenturyRequired,
        }
    }

    fn extended_year(&self, date: &Self::DateInner) -> i32 {
        Iso.extended_year(&date.inner)
    }

    fn is_in_leap_year(&self, date: &Self::DateInner) -> bool {
        Iso.is_in_leap_year(&date.inner)
    }

    /// The calendar-specific month represented by `date`
    fn month(&self, date: &Self::DateInner) -> types::MonthInfo {
        Iso.month(&date.inner)
    }

    /// The calendar-specific day-of-month represented by `date`
    fn day_of_month(&self, date: &Self::DateInner) -> types::DayOfMonth {
        Iso.day_of_month(&date.inner)
    }

    fn day_of_year(&self, date: &Self::DateInner) -> types::DayOfYear {
        Iso.day_of_year(&date.inner)
    }

    fn debug_name(&self) -> &'static str {
        Self::DEBUG_NAME
    }

    fn calendar_algorithm(&self) -> Option<crate::preferences::CalendarAlgorithm> {
        Some(crate::preferences::CalendarAlgorithm::Japanese)
    }
}

impl crate::cal::scaffold::UnstableSealed for JapaneseExtended {}
impl Calendar for JapaneseExtended {
    type DateInner = JapaneseDateInner;
    type Year = types::EraYear;

    fn from_codes(
        &self,
        era: Option<&str>,
        year: i32,
        month_code: types::MonthCode,
        day: u8,
    ) -> Result<Self::DateInner, DateError> {
        self.0.from_codes(era, year, month_code, day)
    }

    fn from_rata_die(&self, rd: RataDie) -> Self::DateInner {
        Japanese::from_rata_die(&self.0, rd)
    }

    fn to_rata_die(&self, date: &Self::DateInner) -> RataDie {
        Japanese::to_rata_die(&self.0, date)
    }

    fn from_iso(&self, iso: IsoDateInner) -> JapaneseDateInner {
        Japanese::from_iso(&self.0, iso)
    }

    fn to_iso(&self, date: &Self::DateInner) -> IsoDateInner {
        Japanese::to_iso(&self.0, date)
    }

    fn months_in_year(&self, date: &Self::DateInner) -> u8 {
        Japanese::months_in_year(&self.0, date)
    }

    fn days_in_year(&self, date: &Self::DateInner) -> u16 {
        Japanese::days_in_year(&self.0, date)
    }

    fn days_in_month(&self, date: &Self::DateInner) -> u8 {
        Japanese::days_in_month(&self.0, date)
    }

    fn offset_date(&self, date: &mut Self::DateInner, offset: DateDuration<Self>) {
        Japanese::offset_date(&self.0, date, offset.cast_unit())
    }

    fn until(
        &self,
        date1: &Self::DateInner,
        date2: &Self::DateInner,
        calendar2: &Self,
        largest_unit: DateDurationUnit,
        smallest_unit: DateDurationUnit,
    ) -> DateDuration<Self> {
        Japanese::until(
            &self.0,
            date1,
            date2,
            &calendar2.0,
            largest_unit,
            smallest_unit,
        )
        .cast_unit()
    }

    fn year_info(&self, date: &Self::DateInner) -> Self::Year {
        Japanese::year_info(&self.0, date)
    }

    fn extended_year(&self, date: &Self::DateInner) -> i32 {
        Japanese::extended_year(&self.0, date)
    }

    fn is_in_leap_year(&self, date: &Self::DateInner) -> bool {
        Japanese::is_in_leap_year(&self.0, date)
    }

    /// The calendar-specific month represented by `date`
    fn month(&self, date: &Self::DateInner) -> types::MonthInfo {
        Japanese::month(&self.0, date)
    }

    /// The calendar-specific day-of-month represented by `date`
    fn day_of_month(&self, date: &Self::DateInner) -> types::DayOfMonth {
        Japanese::day_of_month(&self.0, date)
    }

    /// Information of the day of the year
    fn day_of_year(&self, date: &Self::DateInner) -> types::DayOfYear {
        Japanese::day_of_year(&self.0, date)
    }

    fn debug_name(&self) -> &'static str {
        Self::DEBUG_NAME
    }

    fn calendar_algorithm(&self) -> Option<crate::preferences::CalendarAlgorithm> {
        Some(crate::preferences::CalendarAlgorithm::Japanese)
    }
}

impl Date<Japanese> {
    /// Construct a new Japanese Date.
    ///
    /// Years are specified in the era provided, and must be in range for Japanese
    /// eras (e.g. dates past April 30 Heisei 31 must be in Reiwa; "Jun 5 Heisei 31" and "Jan 1 Heisei 32"
    /// will not be adjusted to being in Reiwa 1 and 2 respectively)
    ///
    /// However, dates may always be specified in "bce" or "ce" and they will be adjusted as necessary.
    ///
    /// ```rust
    /// use icu::calendar::cal::Japanese;
    /// use icu::calendar::{Date, Ref};
    /// use tinystr::tinystr;
    ///
    /// let japanese_calendar = Japanese::new();
    /// // for easy sharing
    /// let japanese_calendar = Ref(&japanese_calendar);
    ///
    /// let era = "heisei";
    ///
    /// let date =
    ///     Date::try_new_japanese_with_calendar(era, 14, 1, 2, japanese_calendar)
    ///         .expect("Constructing a date should succeed");
    ///
    /// assert_eq!(date.era_year().era, era);
    /// assert_eq!(date.era_year().year, 14);
    /// assert_eq!(date.month().ordinal, 1);
    /// assert_eq!(date.day_of_month().0, 2);
    ///
    /// // This function will error for eras that are out of bounds:
    /// // (Heisei was 32 years long, Heisei 33 is in Reiwa)
    /// let oob_date =
    ///     Date::try_new_japanese_with_calendar(era, 33, 1, 2, japanese_calendar);
    /// assert!(oob_date.is_err());
    ///
    /// // and for unknown eras
    /// let fake_era = "neko"; // 🐱
    /// let fake_date = Date::try_new_japanese_with_calendar(
    ///     fake_era,
    ///     10,
    ///     1,
    ///     2,
    ///     japanese_calendar,
    /// );
    /// assert!(fake_date.is_err());
    /// ```
    pub fn try_new_japanese_with_calendar<A: AsCalendar<Calendar = Japanese>>(
        era: &str,
        year: i32,
        month: u8,
        day: u8,
        japanese_calendar: A,
    ) -> Result<Date<A>, DateError> {
        let inner = japanese_calendar
            .as_calendar()
            .new_japanese_date_inner(era, year, month, day)?;
        Ok(Date::from_raw(inner, japanese_calendar))
    }
}

impl Date<JapaneseExtended> {
    /// Construct a new Japanese Date with all eras.
    ///
    /// Years are specified in the era provided, and must be in range for Japanese
    /// eras (e.g. dates past April 30 Heisei 31 must be in Reiwa; "Jun 5 Heisei 31" and "Jan 1 Heisei 32"
    /// will not be adjusted to being in Reiwa 1 and 2 respectively)
    ///
    /// However, dates may always be specified in "bce" or "ce" and they will be adjusted as necessary.
    ///
    /// ```rust
    /// use icu::calendar::cal::JapaneseExtended;
    /// use icu::calendar::{Date, Ref};
    /// use tinystr::tinystr;
    ///
    /// let japanext_calendar = JapaneseExtended::new();
    /// // for easy sharing
    /// let japanext_calendar = Ref(&japanext_calendar);
    ///
    /// let era = "kansei-1789";
    ///
    /// let date = Date::try_new_japanese_extended_with_calendar(
    ///     era,
    ///     7,
    ///     1,
    ///     2,
    ///     japanext_calendar,
    /// )
    /// .expect("Constructing a date should succeed");
    ///
    /// assert_eq!(date.era_year().era, era);
    /// assert_eq!(date.era_year().year, 7);
    /// assert_eq!(date.month().ordinal, 1);
    /// assert_eq!(date.day_of_month().0, 2);
    /// ```
    pub fn try_new_japanese_extended_with_calendar<A: AsCalendar<Calendar = JapaneseExtended>>(
        era: &str,
        year: i32,
        month: u8,
        day: u8,
        japanext_calendar: A,
    ) -> Result<Date<A>, DateError> {
        let inner = japanext_calendar
            .as_calendar()
            .0
            .new_japanese_date_inner(era, year, month, day)?;
        Ok(Date::from_raw(inner, japanext_calendar))
    }
}

const MEIJI_START: EraStartDate = EraStartDate {
    year: 1868,
    month: 10,
    day: 23,
};
const TAISHO_START: EraStartDate = EraStartDate {
    year: 1912,
    month: 7,
    day: 30,
};
const SHOWA_START: EraStartDate = EraStartDate {
    year: 1926,
    month: 12,
    day: 25,
};
const HEISEI_START: EraStartDate = EraStartDate {
    year: 1989,
    month: 1,
    day: 8,
};
const REIWA_START: EraStartDate = EraStartDate {
    year: 2019,
    month: 5,
    day: 1,
};

impl Japanese {
    /// Given an ISO date, give year and era for that date in the Japanese calendar
    ///
    /// This will also use Gregorian eras for eras that are before the earliest era
    fn adjusted_year_for(&self, date: IsoDateInner) -> (i32, TinyStr16) {
        let date: EraStartDate = EraStartDate {
            year: date.0.year,
            month: date.0.month,
            day: date.0.day,
        };
        let (start, era) = self.japanese_era_for(date);
        // The year in which an era starts is Year 1, and it may be short
        // The only time this function will experience dates that are *before*
        // the era start date are for the first era (Currently, taika-645
        // for japanext, meiji for japanese),
        // In such a case, we instead fall back to Gregorian era codes
        if date < start {
            if date.year <= 0 {
                (1 - date.year, tinystr!(16, "bce"))
            } else {
                (date.year, tinystr!(16, "ce"))
            }
        } else {
            (date.year - start.year + 1, era)
        }
    }

    /// Given an date, obtain the era data (not counting spliced gregorian eras)
    fn japanese_era_for(&self, date: EraStartDate) -> (EraStartDate, TinyStr16) {
        let era_data = self.eras.get();
        // We optimize for the five "modern" post-Meiji eras, which are stored in a smaller
        // array and also hardcoded. The hardcoded version is not used if data indicates the
        // presence of newer eras.
        if date >= MEIJI_START
            && era_data.dates_to_eras.last().map(|x| x.1) == Some(tinystr!(16, "reiwa"))
        {
            // Fast path in case eras have not changed since this code was written
            return if date >= REIWA_START {
                (REIWA_START, tinystr!(16, "reiwa"))
            } else if date >= HEISEI_START {
                (HEISEI_START, tinystr!(16, "heisei"))
            } else if date >= SHOWA_START {
                (SHOWA_START, tinystr!(16, "showa"))
            } else if date >= TAISHO_START {
                (TAISHO_START, tinystr!(16, "taisho"))
            } else {
                (MEIJI_START, tinystr!(16, "meiji"))
            };
        }
        let data = &era_data.dates_to_eras;
        match data.binary_search_by(|(d, _)| d.cmp(&date)) {
            Ok(index) => data.get(index),
            Err(index) if index == 0 => data.get(index),
            Err(index) => data.get(index - 1).or_else(|| data.iter().next_back()),
        }
        .unwrap_or((REIWA_START, tinystr!(16, "reiwa")))
    }

    /// Returns the range of dates for a given Japanese era code,
    /// not handling "bce" or "ce"
    ///
    /// Returns (era_start, era_end)
    fn japanese_era_range_for(
        &self,
        era: TinyStr16,
    ) -> Result<(EraStartDate, Option<EraStartDate>), DateError> {
        // Avoid linear search by trying well known eras
        if era == tinystr!(16, "reiwa") {
            // Check if we're the last
            if let Some(last) = self.eras.get().dates_to_eras.last() {
                if last.1 == era {
                    return Ok((REIWA_START, None));
                }
            }
        } else if era == tinystr!(16, "heisei") {
            return Ok((HEISEI_START, Some(REIWA_START)));
        } else if era == tinystr!(16, "showa") {
            return Ok((SHOWA_START, Some(HEISEI_START)));
        } else if era == tinystr!(16, "taisho") {
            return Ok((TAISHO_START, Some(SHOWA_START)));
        } else if era == tinystr!(16, "meiji") {
            return Ok((MEIJI_START, Some(TAISHO_START)));
        }

        let era_data = self.eras.get();
        let data = &era_data.dates_to_eras;
        // Try to avoid linear search by binary searching for the year suffix
        if let Some(year) = era.split('-').nth(1) {
            if let Ok(ref int) = year.parse::<i32>() {
                if let Ok(index) = data.binary_search_by(|(d, _)| d.year.cmp(int)) {
                    #[allow(clippy::expect_used)] // see expect message
                    let (era_start, code) = data
                        .get(index)
                        .expect("Indexing from successful binary search must succeed");
                    // There is a slight chance we hit the case where there are two eras in the same year
                    // There are a couple of rare cases of this, but it's not worth writing a range-based binary search
                    // to catch them since this is an optimization
                    if code == era {
                        return Ok((era_start, data.get(index + 1).map(|e| e.0)));
                    }
                }
            }
        }

        // Avoidance didn't work. Let's find the era manually, searching back from the present
        if let Some((index, (start, _))) = data.iter().enumerate().rev().find(|d| d.1 .1 == era) {
            return Ok((start, data.get(index + 1).map(|e| e.0)));
        }

        Err(DateError::UnknownEra)
    }

    fn new_japanese_date_inner(
        &self,
        era: &str,
        year: i32,
        month: u8,
        day: u8,
    ) -> Result<JapaneseDateInner, DateError> {
        let cal = Ref(self);
        let era = match era {
            "ce" | "ad" => {
                return Ok(Date::try_new_gregorian(year_check(year, 1..)?, month, day)?
                    .to_calendar(cal)
                    .inner);
            }
            "bce" | "bc" => {
                return Ok(
                    Date::try_new_gregorian(1 - year_check(year, 1..)?, month, day)?
                        .to_calendar(cal)
                        .inner,
                );
            }
            e => e.parse().map_err(|_| DateError::UnknownEra)?,
        };

        let (era_start, next_era_start) = self.japanese_era_range_for(era)?;

        let next_era_start = next_era_start.unwrap_or(EraStartDate {
            year: i32::MAX,
            month: 12,
            day: 31,
        });

        let date_in_iso = EraStartDate {
            year: era_start.year + year - 1,
            month,
            day,
        };

        if date_in_iso < era_start {
            return Err(if date_in_iso.year < era_start.year {
                DateError::Range {
                    field: "year",
                    value: year,
                    min: 1,
                    max: 1 + next_era_start.year - era_start.year,
                }
            } else if date_in_iso.month < era_start.month {
                DateError::Range {
                    field: "month",
                    value: month as i32,
                    min: era_start.month as i32,
                    max: 12,
                }
            } else
            /* if date_in_iso.day < era_start.day */
            {
                DateError::Range {
                    field: "day",
                    value: day as i32,
                    min: era_start.day as i32,
                    max: 31,
                }
            });
        } else if date_in_iso >= next_era_start {
            return Err(if date_in_iso.year > era_start.year {
                DateError::Range {
                    field: "year",
                    value: year,
                    min: 1,
                    max: 1 + next_era_start.year - era_start.year,
                }
            } else if date_in_iso.month > era_start.month {
                DateError::Range {
                    field: "month",
                    value: month as i32,
                    min: 1,
                    max: next_era_start.month as i32 - 1,
                }
            } else
            /* if date_in_iso.day >= era_start.day */
            {
                DateError::Range {
                    field: "day",
                    value: day as i32,
                    min: 1,
                    max: next_era_start.day as i32 - 1,
                }
            });
        }

        let iso = Date::try_new_iso(date_in_iso.year, date_in_iso.month, date_in_iso.day)?;
        Ok(JapaneseDateInner {
            inner: iso.inner,
            adjusted_year: year,
            era,
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::Ref;

    fn single_test_roundtrip(calendar: Ref<Japanese>, era: &str, year: i32, month: u8, day: u8) {
        let date = Date::try_new_japanese_with_calendar(era, year, month, day, calendar)
            .unwrap_or_else(|e| {
                panic!("Failed to construct date with {era:?}, {year}, {month}, {day}: {e:?}")
            });
        let iso = date.to_iso();
        let reconstructed = Date::new_from_iso(iso, calendar);
        assert_eq!(
            date, reconstructed,
            "Failed to roundtrip with {era:?}, {year}, {month}, {day}"
        );

        // Extra coverage for https://github.com/unicode-org/icu4x/issues/4968
        assert_eq!(reconstructed.era_year().era, era);
        assert_eq!(reconstructed.era_year().year, year);
    }

    fn single_test_roundtrip_ext(
        calendar: Ref<JapaneseExtended>,
        era: &str,
        year: i32,
        month: u8,
        day: u8,
    ) {
        let date = Date::try_new_japanese_extended_with_calendar(era, year, month, day, calendar)
            .unwrap_or_else(|e| {
                panic!("Failed to construct date with {era:?}, {year}, {month}, {day}: {e:?}")
            });
        let iso = date.to_iso();
        let reconstructed = Date::new_from_iso(iso, calendar);
        assert_eq!(
            date, reconstructed,
            "Failed to roundtrip with {era:?}, {year}, {month}, {day}"
        )
    }

    // test that the Gregorian eras roundtrip to Japanese ones
    fn single_test_gregorian_roundtrip_ext(
        calendar: Ref<JapaneseExtended>,
        era: &str,
        year: i32,
        month: u8,
        day: u8,
        era2: &str,
        year2: i32,
    ) {
        let expected = Date::try_new_japanese_extended_with_calendar(era2, year2, month, day, calendar)
            .unwrap_or_else(|e| {
                panic!(
                    "Failed to construct expectation date with {era2:?}, {year2}, {month}, {day}: {e:?}"
                )
            });

        let date = Date::try_new_japanese_extended_with_calendar(era, year, month, day, calendar)
            .unwrap_or_else(|e| {
                panic!("Failed to construct date with {era:?}, {year}, {month}, {day}: {e:?}")
            });
        let iso = date.to_iso();
        let reconstructed = Date::new_from_iso(iso, calendar);
        assert_eq!(
            expected, reconstructed,
            "Failed to roundtrip with {era:?}, {year}, {month}, {day} == {era2:?}, {year}"
        )
    }

    fn single_test_error(
        calendar: Ref<Japanese>,
        era: &str,
        year: i32,
        month: u8,
        day: u8,
        error: DateError,
    ) {
        let date = Date::try_new_japanese_with_calendar(era, year, month, day, calendar);
        assert_eq!(
            date,
            Err(error),
            "Construction with {era:?}, {year}, {month}, {day} did not return {error:?}"
        )
    }

    fn single_test_error_ext(
        calendar: Ref<JapaneseExtended>,
        era: &str,
        year: i32,
        month: u8,
        day: u8,
        error: DateError,
    ) {
        let date = Date::try_new_japanese_extended_with_calendar(era, year, month, day, calendar);
        assert_eq!(
            date,
            Err(error),
            "Construction with {era:?}, {year}, {month}, {day} did not return {error:?}"
        )
    }

    #[test]
    fn test_japanese() {
        let calendar = Japanese::new();
        let calendar_ext = JapaneseExtended::new();
        let calendar = Ref(&calendar);
        let calendar_ext = Ref(&calendar_ext);

        single_test_roundtrip(calendar, "heisei", 12, 3, 1);
        single_test_roundtrip(calendar, "taisho", 3, 3, 1);
        // Heisei did not start until later in the year
        single_test_error(
            calendar,
            "heisei",
            1,
            1,
            1,
            DateError::Range {
                field: "day",
                value: 1,
                min: 8,
                max: 31,
            },
        );

        single_test_roundtrip_ext(calendar_ext, "heisei", 12, 3, 1);
        single_test_roundtrip_ext(calendar_ext, "taisho", 3, 3, 1);
        single_test_error_ext(
            calendar_ext,
            "heisei",
            1,
            1,
            1,
            DateError::Range {
                field: "day",
                value: 1,
                min: 8,
                max: 31,
            },
        );

        single_test_roundtrip_ext(calendar_ext, "hakuho-672", 4, 3, 1);
        single_test_error(calendar, "hakuho-672", 4, 3, 1, DateError::UnknownEra);

        // handle bce/ce
        single_test_roundtrip(calendar, "bce", 100, 3, 1);
        single_test_roundtrip(calendar, "bce", 1, 3, 1);
        single_test_roundtrip(calendar, "ce", 1, 3, 1);
        single_test_roundtrip(calendar, "ce", 100, 3, 1);
        single_test_roundtrip_ext(calendar_ext, "ce", 100, 3, 1);
        single_test_roundtrip(calendar, "ce", 1000, 3, 1);
        single_test_error(
            calendar,
            "ce",
            0,
            3,
            1,
            DateError::Range {
                field: "year",
                value: 0,
                min: 1,
                max: i32::MAX,
            },
        );
        single_test_error(
            calendar,
            "bce",
            -1,
            3,
            1,
            DateError::Range {
                field: "year",
                value: -1,
                min: 1,
                max: i32::MAX,
            },
        );

        // handle the cases where bce/ce get adjusted to different eras
        // single_test_gregorian_roundtrip(calendar, "ce", 2021, 3, 1, "reiwa", 3);
        single_test_gregorian_roundtrip_ext(calendar_ext, "ce", 1000, 3, 1, "choho-999", 2);
        single_test_gregorian_roundtrip_ext(calendar_ext, "ce", 749, 5, 10, "tenpyokampo-749", 1);
        single_test_gregorian_roundtrip_ext(calendar_ext, "bce", 10, 3, 1, "bce", 10);

        // There were multiple eras in this year
        // This one is from Apr 14 to July 2
        single_test_roundtrip_ext(calendar_ext, "tenpyokampo-749", 1, 4, 20);
        single_test_roundtrip_ext(calendar_ext, "tenpyokampo-749", 1, 4, 14);
        single_test_roundtrip_ext(calendar_ext, "tenpyokampo-749", 1, 7, 1);
        single_test_error_ext(
            calendar_ext,
            "tenpyokampo-749",
            1,
            7,
            5,
            DateError::Range {
                field: "month",
                value: 7,
                min: 1,
                max: 6,
            },
        );
        single_test_error_ext(
            calendar_ext,
            "tenpyokampo-749",
            1,
            4,
            13,
            DateError::Range {
                field: "day",
                value: 13,
                min: 14,
                max: 31,
            },
        );
    }
}
