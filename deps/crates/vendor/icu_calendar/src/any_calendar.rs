// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Module for working with multiple calendars at once

use crate::cal::hijri::HijriSimulatedLocation;
use crate::cal::iso::IsoDateInner;
use crate::cal::{
    Buddhist, Chinese, Coptic, Dangi, Ethiopian, EthiopianEraStyle, Gregorian, Hebrew,
    HijriSimulated, HijriTabular, HijriTabularEpoch, HijriTabularLeapYears, HijriUmmAlQura, Indian,
    Iso, Japanese, JapaneseExtended, Persian, Roc,
};
use crate::error::DateError;
use crate::types::YearInfo;
use crate::{types, AsCalendar, Calendar, Date, DateDuration, DateDurationUnit, Ref};

use crate::preferences::{CalendarAlgorithm, HijriCalendarAlgorithm};
use icu_locale_core::preferences::define_preferences;
use icu_locale_core::subtags::region;
use icu_provider::prelude::*;

use core::fmt;

define_preferences!(
    /// The preferences for calendars formatting.
    [Copy]
    CalendarPreferences,
    {
        /// The user's preferred calendar system.
        calendar_algorithm: CalendarAlgorithm
    }
);

/// This is a calendar that encompasses all formattable calendars supported by this crate
///
/// This allows for the construction of [`Date`] objects that have their calendar known at runtime.
///
/// This can be constructed by calling `.into()` on a concrete calendar type if the calendar type is known at
/// compile time. When the type is known at runtime, the [`AnyCalendar::new()`] and sibling methods may be used.
///
/// [`Date`] can also be converted to [`AnyCalendar`]-compatible ones
/// via [`Date::to_any()`](crate::Date::to_any()).
///
/// There are many ways of constructing an AnyCalendar'd date:
/// ```
/// use icu::calendar::{AnyCalendar, AnyCalendarKind, Date, cal::Japanese, types::MonthCode};
/// use icu::locale::locale;
/// use tinystr::tinystr;
/// # use std::rc::Rc;
///
/// let locale = locale!("en-u-ca-japanese"); // English with the Japanese calendar
///
/// let calendar = AnyCalendar::new(AnyCalendarKind::new(locale.into()));
/// let calendar = Rc::new(calendar); // Avoid cloning it each time
///                                   // If everything is a local reference, you may use icu::calendar::Ref instead.
///
/// // construct from era code, year, month code, day, and a calendar
/// // This is March 28, 15 Heisei
/// let manual_date = Date::try_new_from_codes(Some("heisei"), 15, MonthCode(tinystr!(4, "M03")), 28, calendar.clone())
///                     .expect("Failed to construct Date manually");
///
///
/// // construct another date by converting from ISO
/// let iso_date = Date::try_new_iso(2020, 9, 1)
///     .expect("Failed to construct ISO Date.");
/// let iso_converted = iso_date.to_calendar(calendar);
///
/// // Construct a date in the appropriate typed calendar and convert
/// let japanese_calendar = Japanese::new();
/// let japanese_date = Date::try_new_japanese_with_calendar("heisei", 15, 3, 28,
///                                                         japanese_calendar).unwrap();
/// // This is a Date<AnyCalendar>
/// let any_japanese_date = japanese_date.to_any();
/// ```
#[derive(Debug, Clone)]
#[non_exhaustive]
pub enum AnyCalendar {
    /// A [`Buddhist`] calendar
    Buddhist(Buddhist),
    /// A [`Chinese`] calendar
    Chinese(Chinese),
    /// A [`Coptic`] calendar
    Coptic(Coptic),
    /// A [`Dangi`] calendar
    Dangi(Dangi),
    /// An [`Ethiopian`] calendar
    Ethiopian(Ethiopian),
    /// A [`Gregorian`] calendar
    Gregorian(Gregorian),
    /// A [`Hebrew`] calendar
    Hebrew(Hebrew),
    /// An [`Indian`] calendar
    Indian(Indian),
    /// A [`HijriTabular`] calendar
    HijriTabular(HijriTabular),
    /// A [`HijriSimulated`] calendar
    HijriSimulated(HijriSimulated),
    /// A [`HijriUmmAlQura`] calendar
    HijriUmmAlQura(HijriUmmAlQura),
    /// An [`Iso`] calendar
    Iso(Iso),
    /// A [`Japanese`] calendar
    Japanese(Japanese),
    /// A [`JapaneseExtended`] calendar
    JapaneseExtended(JapaneseExtended),
    /// A [`Persian`] calendar
    Persian(Persian),
    /// A [`Roc`] calendar
    Roc(Roc),
}

// TODO(#3469): Decide on the best way to implement Ord.
/// The inner date type for [`AnyCalendar`]
#[derive(Clone, PartialEq, Eq, Debug, Copy)]
#[non_exhaustive]
pub enum AnyDateInner {
    /// A date for a [`Buddhist`] calendar
    Buddhist(<Buddhist as Calendar>::DateInner),
    /// A date for a [`Chinese`] calendar
    Chinese(<Chinese as Calendar>::DateInner),
    /// A date for a [`Coptic`] calendar
    Coptic(<Coptic as Calendar>::DateInner),
    /// A date for a [`Dangi`] calendar
    Dangi(<Dangi as Calendar>::DateInner),
    /// A date for an [`Ethiopian`] calendar
    Ethiopian(<Ethiopian as Calendar>::DateInner),
    /// A date for a [`Gregorian`] calendar
    Gregorian(<Gregorian as Calendar>::DateInner),
    /// A date for a [`Hebrew`] calendar
    Hebrew(<Hebrew as Calendar>::DateInner),
    /// A date for an [`Indian`] calendar
    Indian(<Indian as Calendar>::DateInner),
    /// A date for a [`HijriTabular`] calendar
    HijriTabular(
        <HijriTabular as Calendar>::DateInner,
        HijriTabularLeapYears,
        HijriTabularEpoch,
    ),
    /// A date for a [`HijriSimulated`] calendar
    HijriSimulated(<HijriSimulated as Calendar>::DateInner),
    /// A date for a [`HijriUmmAlQura`] calendar
    HijriUmmAlQura(<HijriUmmAlQura as Calendar>::DateInner),
    /// A date for an [`Iso`] calendar
    Iso(<Iso as Calendar>::DateInner),
    /// A date for a [`Japanese`] calendar
    Japanese(<Japanese as Calendar>::DateInner),
    /// A date for a [`JapaneseExtended`] calendar
    JapaneseExtended(<JapaneseExtended as Calendar>::DateInner),
    /// A date for a [`Persian`] calendar
    Persian(<Persian as Calendar>::DateInner),
    /// A date for a [`Roc`] calendar
    Roc(<Roc as Calendar>::DateInner),
}

macro_rules! match_cal_and_date {
    (match ($cal:ident, $date:ident): ($cal_matched:ident, $date_matched:ident) => $e:expr) => {
        match ($cal, $date) {
            (&Self::Buddhist(ref $cal_matched), &AnyDateInner::Buddhist(ref $date_matched)) => $e,
            (&Self::Chinese(ref $cal_matched), &AnyDateInner::Chinese(ref $date_matched)) => $e,
            (&Self::Coptic(ref $cal_matched), &AnyDateInner::Coptic(ref $date_matched)) => $e,
            (&Self::Dangi(ref $cal_matched), &AnyDateInner::Dangi(ref $date_matched)) => $e,
            (&Self::Ethiopian(ref $cal_matched), &AnyDateInner::Ethiopian(ref $date_matched)) => $e,
            (&Self::Gregorian(ref $cal_matched), &AnyDateInner::Gregorian(ref $date_matched)) => $e,
            (&Self::Hebrew(ref $cal_matched), &AnyDateInner::Hebrew(ref $date_matched)) => $e,
            (&Self::Indian(ref $cal_matched), &AnyDateInner::Indian(ref $date_matched)) => $e,
            (
                &Self::HijriTabular(ref $cal_matched),
                &AnyDateInner::HijriTabular(ref $date_matched, leap_years, epoch),
            ) if $cal_matched.epoch == epoch && $cal_matched.leap_years == leap_years => $e,
            (
                &Self::HijriSimulated(ref $cal_matched),
                &AnyDateInner::HijriSimulated(ref $date_matched),
            ) => $e,
            (
                &Self::HijriUmmAlQura(ref $cal_matched),
                &AnyDateInner::HijriUmmAlQura(ref $date_matched),
            ) => $e,
            (&Self::Iso(ref $cal_matched), &AnyDateInner::Iso(ref $date_matched)) => $e,
            (&Self::Japanese(ref $cal_matched), &AnyDateInner::Japanese(ref $date_matched)) => $e,
            (
                &Self::JapaneseExtended(ref $cal_matched),
                &AnyDateInner::JapaneseExtended(ref $date_matched),
            ) => $e,
            (&Self::Persian(ref $cal_matched), &AnyDateInner::Persian(ref $date_matched)) => $e,
            (&Self::Roc(ref $cal_matched), &AnyDateInner::Roc(ref $date_matched)) => $e,
            _ => panic!(
                "Found AnyCalendar with mixed calendar type {:?} and date type {:?}!",
                $cal.kind().debug_name(),
                $date.kind().debug_name()
            ),
        }
    };
}

macro_rules! match_cal {
    (match $cal:ident: ($cal_matched:ident) => $e:expr) => {
        match $cal {
            &Self::Buddhist(ref $cal_matched) => AnyDateInner::Buddhist($e),
            &Self::Chinese(ref $cal_matched) => AnyDateInner::Chinese($e),
            &Self::Coptic(ref $cal_matched) => AnyDateInner::Coptic($e),
            &Self::Dangi(ref $cal_matched) => AnyDateInner::Dangi($e),
            &Self::Ethiopian(ref $cal_matched) => AnyDateInner::Ethiopian($e),
            &Self::Gregorian(ref $cal_matched) => AnyDateInner::Gregorian($e),
            &Self::Hebrew(ref $cal_matched) => AnyDateInner::Hebrew($e),
            &Self::Indian(ref $cal_matched) => AnyDateInner::Indian($e),
            &Self::HijriSimulated(ref $cal_matched) => AnyDateInner::HijriSimulated($e),
            &Self::HijriTabular(ref $cal_matched) => {
                AnyDateInner::HijriTabular($e, $cal_matched.leap_years, $cal_matched.epoch)
            }
            &Self::HijriUmmAlQura(ref $cal_matched) => AnyDateInner::HijriUmmAlQura($e),
            &Self::Iso(ref $cal_matched) => AnyDateInner::Iso($e),
            &Self::Japanese(ref $cal_matched) => AnyDateInner::Japanese($e),
            &Self::JapaneseExtended(ref $cal_matched) => AnyDateInner::JapaneseExtended($e),
            &Self::Persian(ref $cal_matched) => AnyDateInner::Persian($e),
            &Self::Roc(ref $cal_matched) => AnyDateInner::Roc($e),
        }
    };
}

impl crate::cal::scaffold::UnstableSealed for AnyCalendar {}
impl Calendar for AnyCalendar {
    type DateInner = AnyDateInner;
    type Year = YearInfo;

    fn from_codes(
        &self,
        era: Option<&str>,
        year: i32,
        month_code: types::MonthCode,
        day: u8,
    ) -> Result<Self::DateInner, DateError> {
        Ok(match_cal!(match self: (c) => c.from_codes(era, year, month_code, day)?))
    }

    fn from_iso(&self, iso: IsoDateInner) -> AnyDateInner {
        match_cal!(match self: (c) => c.from_iso(iso))
    }

    fn from_rata_die(&self, rd: calendrical_calculations::rata_die::RataDie) -> Self::DateInner {
        match_cal!(match self: (c) => c.from_rata_die(rd))
    }

    fn to_rata_die(&self, date: &Self::DateInner) -> calendrical_calculations::rata_die::RataDie {
        match_cal_and_date!(match (self, date): (c, d) => c.to_rata_die(d))
    }

    fn to_iso(&self, date: &Self::DateInner) -> IsoDateInner {
        match_cal_and_date!(match (self, date): (c, d) => c.to_iso(d))
    }

    fn months_in_year(&self, date: &Self::DateInner) -> u8 {
        match_cal_and_date!(match (self, date): (c, d) => c.months_in_year(d))
    }

    fn days_in_year(&self, date: &Self::DateInner) -> u16 {
        match_cal_and_date!(match (self, date): (c, d) => c.days_in_year(d))
    }

    fn days_in_month(&self, date: &Self::DateInner) -> u8 {
        match_cal_and_date!(match (self, date): (c, d) => c.days_in_month(d))
    }

    fn offset_date(&self, date: &mut Self::DateInner, offset: DateDuration<Self>) {
        match (self, date) {
            (Self::Buddhist(c), AnyDateInner::Buddhist(ref mut d)) => {
                c.offset_date(d, offset.cast_unit())
            }
            (Self::Chinese(c), AnyDateInner::Chinese(ref mut d)) => {
                c.offset_date(d, offset.cast_unit())
            }
            (Self::Coptic(c), AnyDateInner::Coptic(ref mut d)) => {
                c.offset_date(d, offset.cast_unit())
            }
            (Self::Dangi(c), AnyDateInner::Dangi(ref mut d)) => {
                c.offset_date(d, offset.cast_unit())
            }
            (Self::Ethiopian(c), AnyDateInner::Ethiopian(ref mut d)) => {
                c.offset_date(d, offset.cast_unit())
            }
            (Self::Gregorian(c), AnyDateInner::Gregorian(ref mut d)) => {
                c.offset_date(d, offset.cast_unit())
            }
            (Self::Hebrew(c), AnyDateInner::Hebrew(ref mut d)) => {
                c.offset_date(d, offset.cast_unit())
            }
            (Self::Indian(c), AnyDateInner::Indian(ref mut d)) => {
                c.offset_date(d, offset.cast_unit())
            }
            (
                Self::HijriTabular(c),
                &mut AnyDateInner::HijriTabular(ref mut d, leap_years, epoch),
            ) if c.epoch == epoch && c.leap_years == leap_years => {
                c.offset_date(d, offset.cast_unit())
            }
            (Self::HijriSimulated(c), AnyDateInner::HijriSimulated(ref mut d)) => {
                c.offset_date(d, offset.cast_unit())
            }
            (Self::HijriUmmAlQura(c), AnyDateInner::HijriUmmAlQura(ref mut d)) => {
                c.offset_date(d, offset.cast_unit())
            }
            (Self::Iso(c), AnyDateInner::Iso(ref mut d)) => c.offset_date(d, offset.cast_unit()),
            (Self::Japanese(c), AnyDateInner::Japanese(ref mut d)) => {
                c.offset_date(d, offset.cast_unit())
            }
            (Self::JapaneseExtended(c), AnyDateInner::JapaneseExtended(ref mut d)) => {
                c.offset_date(d, offset.cast_unit())
            }
            (Self::Persian(c), AnyDateInner::Persian(ref mut d)) => {
                c.offset_date(d, offset.cast_unit())
            }
            (Self::Roc(c), AnyDateInner::Roc(ref mut d)) => c.offset_date(d, offset.cast_unit()),
            // This is only reached from misuse of from_raw, a semi-internal api
            #[allow(clippy::panic)]
            (_, d) => panic!(
                "Found AnyCalendar with mixed calendar type {} and date type {}!",
                self.kind().debug_name(),
                d.kind().debug_name()
            ),
        }
    }

    fn until(
        &self,
        date1: &Self::DateInner,
        date2: &Self::DateInner,
        calendar2: &Self,
        largest_unit: DateDurationUnit,
        smallest_unit: DateDurationUnit,
    ) -> DateDuration<Self> {
        match (self, calendar2, date1, date2) {
            (
                Self::Buddhist(c1),
                Self::Buddhist(c2),
                AnyDateInner::Buddhist(d1),
                AnyDateInner::Buddhist(d2),
            ) => c1
                .until(d1, d2, c2, largest_unit, smallest_unit)
                .cast_unit(),
            (
                Self::Chinese(c1),
                Self::Chinese(c2),
                AnyDateInner::Chinese(d1),
                AnyDateInner::Chinese(d2),
            ) => c1
                .until(d1, d2, c2, largest_unit, smallest_unit)
                .cast_unit(),
            (
                Self::Coptic(c1),
                Self::Coptic(c2),
                AnyDateInner::Coptic(d1),
                AnyDateInner::Coptic(d2),
            ) => c1
                .until(d1, d2, c2, largest_unit, smallest_unit)
                .cast_unit(),
            (
                Self::Dangi(c1),
                Self::Dangi(c2),
                AnyDateInner::Dangi(d1),
                AnyDateInner::Dangi(d2),
            ) => c1
                .until(d1, d2, c2, largest_unit, smallest_unit)
                .cast_unit(),
            (
                Self::Ethiopian(c1),
                Self::Ethiopian(c2),
                AnyDateInner::Ethiopian(d1),
                AnyDateInner::Ethiopian(d2),
            ) => c1
                .until(d1, d2, c2, largest_unit, smallest_unit)
                .cast_unit(),
            (
                Self::Gregorian(c1),
                Self::Gregorian(c2),
                AnyDateInner::Gregorian(d1),
                AnyDateInner::Gregorian(d2),
            ) => c1
                .until(d1, d2, c2, largest_unit, smallest_unit)
                .cast_unit(),
            (
                Self::Hebrew(c1),
                Self::Hebrew(c2),
                AnyDateInner::Hebrew(d1),
                AnyDateInner::Hebrew(d2),
            ) => c1
                .until(d1, d2, c2, largest_unit, smallest_unit)
                .cast_unit(),
            (
                Self::Indian(c1),
                Self::Indian(c2),
                AnyDateInner::Indian(d1),
                AnyDateInner::Indian(d2),
            ) => c1
                .until(d1, d2, c2, largest_unit, smallest_unit)
                .cast_unit(),
            (
                Self::HijriTabular(c1),
                Self::HijriTabular(c2),
                &AnyDateInner::HijriTabular(ref d1, l1, e1),
                &AnyDateInner::HijriTabular(ref d2, l2, e2),
            ) if c1.epoch == c2.epoch
                && c2.epoch == e1
                && e1 == e2
                && c1.leap_years == c2.leap_years
                && c2.leap_years == l1
                && l1 == l2 =>
            {
                c1.until(d1, d2, c2, largest_unit, smallest_unit)
                    .cast_unit()
            }
            (
                Self::HijriSimulated(c1),
                Self::HijriSimulated(c2),
                AnyDateInner::HijriSimulated(d1),
                AnyDateInner::HijriSimulated(d2),
            ) => c1
                .until(d1, d2, c2, largest_unit, smallest_unit)
                .cast_unit(),
            (
                Self::HijriUmmAlQura(c1),
                Self::HijriUmmAlQura(c2),
                AnyDateInner::HijriUmmAlQura(d1),
                AnyDateInner::HijriUmmAlQura(d2),
            ) => c1
                .until(d1, d2, c2, largest_unit, smallest_unit)
                .cast_unit(),
            (Self::Iso(c1), Self::Iso(c2), AnyDateInner::Iso(d1), AnyDateInner::Iso(d2)) => c1
                .until(d1, d2, c2, largest_unit, smallest_unit)
                .cast_unit(),
            (
                Self::Japanese(c1),
                Self::Japanese(c2),
                AnyDateInner::Japanese(d1),
                AnyDateInner::Japanese(d2),
            ) => c1
                .until(d1, d2, c2, largest_unit, smallest_unit)
                .cast_unit(),
            (
                Self::JapaneseExtended(c1),
                Self::JapaneseExtended(c2),
                AnyDateInner::JapaneseExtended(d1),
                AnyDateInner::JapaneseExtended(d2),
            ) => c1
                .until(d1, d2, c2, largest_unit, smallest_unit)
                .cast_unit(),
            (
                Self::Persian(c1),
                Self::Persian(c2),
                AnyDateInner::Persian(d1),
                AnyDateInner::Persian(d2),
            ) => c1
                .until(d1, d2, c2, largest_unit, smallest_unit)
                .cast_unit(),
            (Self::Roc(c1), Self::Roc(c2), AnyDateInner::Roc(d1), AnyDateInner::Roc(d2)) => c1
                .until(d1, d2, c2, largest_unit, smallest_unit)
                .cast_unit(),
            _ => {
                // attempt to convert
                let iso = calendar2.to_iso(date2);

                match_cal_and_date!(match (self, date1):
                    (c1, d1) => {
                        let d2 = c1.from_iso(iso);
                        let until = c1.until(d1, &d2, c1, largest_unit, smallest_unit);
                        until.cast_unit::<AnyCalendar>()
                    }
                )
            }
        }
    }

    fn year_info(&self, date: &Self::DateInner) -> types::YearInfo {
        match_cal_and_date!(match (self, date): (c, d) => c.year_info(d).into())
    }

    fn extended_year(&self, date: &Self::DateInner) -> i32 {
        match_cal_and_date!(match (self, date): (c, d) => c.extended_year(d))
    }

    /// The calendar-specific check if `date` is in a leap year
    fn is_in_leap_year(&self, date: &Self::DateInner) -> bool {
        match_cal_and_date!(match (self, date): (c, d) => c.is_in_leap_year(d))
    }

    /// The calendar-specific month represented by `date`
    fn month(&self, date: &Self::DateInner) -> types::MonthInfo {
        match_cal_and_date!(match (self, date): (c, d) => c.month(d))
    }

    /// The calendar-specific day-of-month represented by `date`
    fn day_of_month(&self, date: &Self::DateInner) -> types::DayOfMonth {
        match_cal_and_date!(match (self, date): (c, d) => c.day_of_month(d))
    }

    /// Information of the day of the year
    fn day_of_year(&self, date: &Self::DateInner) -> types::DayOfYear {
        match_cal_and_date!(match (self, date): (c, d) => c.day_of_year(d))
    }

    fn debug_name(&self) -> &'static str {
        match self.kind() {
            AnyCalendarKind::Buddhist => "AnyCalendar (Buddhist)",
            AnyCalendarKind::Chinese => "AnyCalendar (Chinese)",
            AnyCalendarKind::Coptic => "AnyCalendar (Coptic)",
            AnyCalendarKind::Dangi => "AnyCalendar (Dangi)",
            AnyCalendarKind::Ethiopian => "AnyCalendar (Ethiopian, Amete Miret)",
            AnyCalendarKind::EthiopianAmeteAlem => "AnyCalendar (Ethiopian, Amete Alem)",
            AnyCalendarKind::Gregorian => "AnyCalendar (Gregorian)",
            AnyCalendarKind::Hebrew => "AnyCalendar (Hebrew)",
            AnyCalendarKind::Indian => "AnyCalendar (Indian)",
            AnyCalendarKind::HijriTabularTypeIIFriday => {
                "AnyCalendar (Hijri, tabular, type II leap years, Friday epoch)"
            }
            AnyCalendarKind::HijriTabularTypeIIThursday => {
                "AnyCalendar (Hijri, tabular, type II leap years, Thursday epoch)"
            }
            AnyCalendarKind::HijriSimulatedMecca => "AnyCalendar (Hijri, simulated Mecca)",
            AnyCalendarKind::HijriUmmAlQura => "AnyCalendar (Hijri, Umm al-Qura)",
            AnyCalendarKind::Iso => "AnyCalendar (Iso)",
            AnyCalendarKind::Japanese => "AnyCalendar (Japanese)",
            AnyCalendarKind::JapaneseExtended => "AnyCalendar (Japanese, historical era data)",
            AnyCalendarKind::Persian => "AnyCalendar (Persian)",
            AnyCalendarKind::Roc => "AnyCalendar (Roc)",
        }
    }

    fn calendar_algorithm(&self) -> Option<CalendarAlgorithm> {
        match self {
            Self::Buddhist(ref c) => c.calendar_algorithm(),
            Self::Chinese(ref c) => c.calendar_algorithm(),
            Self::Coptic(ref c) => c.calendar_algorithm(),
            Self::Dangi(ref c) => c.calendar_algorithm(),
            Self::Ethiopian(ref c) => c.calendar_algorithm(),
            Self::Gregorian(ref c) => c.calendar_algorithm(),
            Self::Hebrew(ref c) => c.calendar_algorithm(),
            Self::Indian(ref c) => c.calendar_algorithm(),
            Self::HijriSimulated(ref c) => c.calendar_algorithm(),
            Self::HijriTabular(ref c) => c.calendar_algorithm(),
            Self::HijriUmmAlQura(ref c) => c.calendar_algorithm(),
            Self::Iso(ref c) => c.calendar_algorithm(),
            Self::Japanese(ref c) => c.calendar_algorithm(),
            Self::JapaneseExtended(ref c) => c.calendar_algorithm(),
            Self::Persian(ref c) => c.calendar_algorithm(),
            Self::Roc(ref c) => c.calendar_algorithm(),
        }
    }
}

impl AnyCalendar {
    /// Constructs an AnyCalendar for a given calendar kind from compiled data.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub const fn new(kind: AnyCalendarKind) -> Self {
        match kind {
            AnyCalendarKind::Buddhist => AnyCalendar::Buddhist(Buddhist),
            AnyCalendarKind::Chinese => AnyCalendar::Chinese(Chinese::new()),
            AnyCalendarKind::Coptic => AnyCalendar::Coptic(Coptic),
            AnyCalendarKind::Dangi => AnyCalendar::Dangi(Dangi::new()),
            AnyCalendarKind::Ethiopian => AnyCalendar::Ethiopian(Ethiopian::new_with_era_style(
                EthiopianEraStyle::AmeteMihret,
            )),
            AnyCalendarKind::EthiopianAmeteAlem => {
                AnyCalendar::Ethiopian(Ethiopian::new_with_era_style(EthiopianEraStyle::AmeteAlem))
            }
            AnyCalendarKind::Gregorian => AnyCalendar::Gregorian(Gregorian),
            AnyCalendarKind::Hebrew => AnyCalendar::Hebrew(Hebrew),
            AnyCalendarKind::Indian => AnyCalendar::Indian(Indian),
            AnyCalendarKind::HijriTabularTypeIIFriday => {
                AnyCalendar::HijriTabular(HijriTabular::new(
                    crate::cal::hijri::HijriTabularLeapYears::TypeII,
                    HijriTabularEpoch::Friday,
                ))
            }
            AnyCalendarKind::HijriSimulatedMecca => {
                AnyCalendar::HijriSimulated(HijriSimulated::new_mecca())
            }
            AnyCalendarKind::HijriTabularTypeIIThursday => {
                AnyCalendar::HijriTabular(HijriTabular::new(
                    crate::cal::hijri::HijriTabularLeapYears::TypeII,
                    HijriTabularEpoch::Thursday,
                ))
            }
            AnyCalendarKind::HijriUmmAlQura => AnyCalendar::HijriUmmAlQura(HijriUmmAlQura::new()),
            AnyCalendarKind::Iso => AnyCalendar::Iso(Iso),
            AnyCalendarKind::Japanese => AnyCalendar::Japanese(Japanese::new()),
            AnyCalendarKind::JapaneseExtended => {
                AnyCalendar::JapaneseExtended(JapaneseExtended::new())
            }
            AnyCalendarKind::Persian => AnyCalendar::Persian(Persian),
            AnyCalendarKind::Roc => AnyCalendar::Roc(Roc),
        }
    }

    #[cfg(feature = "serde")]
    #[doc = icu_provider::gen_buffer_unstable_docs!(BUFFER, Self::new)]
    pub fn try_new_with_buffer_provider<P>(
        provider: &P,
        kind: AnyCalendarKind,
    ) -> Result<Self, DataError>
    where
        P: BufferProvider + ?Sized,
    {
        Ok(match kind {
            AnyCalendarKind::Buddhist => AnyCalendar::Buddhist(Buddhist),
            AnyCalendarKind::Chinese => {
                AnyCalendar::Chinese(Chinese::try_new_with_buffer_provider(provider)?)
            }
            AnyCalendarKind::Coptic => AnyCalendar::Coptic(Coptic),
            AnyCalendarKind::Dangi => {
                AnyCalendar::Dangi(Dangi::try_new_with_buffer_provider(provider)?)
            }
            AnyCalendarKind::Ethiopian => AnyCalendar::Ethiopian(Ethiopian::new_with_era_style(
                EthiopianEraStyle::AmeteMihret,
            )),
            AnyCalendarKind::EthiopianAmeteAlem => {
                AnyCalendar::Ethiopian(Ethiopian::new_with_era_style(EthiopianEraStyle::AmeteAlem))
            }
            AnyCalendarKind::Gregorian => AnyCalendar::Gregorian(Gregorian),
            AnyCalendarKind::Hebrew => AnyCalendar::Hebrew(Hebrew),
            AnyCalendarKind::Indian => AnyCalendar::Indian(Indian),
            AnyCalendarKind::HijriTabularTypeIIFriday => {
                AnyCalendar::HijriTabular(HijriTabular::new(
                    crate::cal::hijri::HijriTabularLeapYears::TypeII,
                    HijriTabularEpoch::Friday,
                ))
            }
            AnyCalendarKind::HijriSimulatedMecca => AnyCalendar::HijriSimulated(
                HijriSimulated::try_new_mecca_with_buffer_provider(provider)?,
            ),
            AnyCalendarKind::HijriTabularTypeIIThursday => {
                AnyCalendar::HijriTabular(HijriTabular::new(
                    crate::cal::hijri::HijriTabularLeapYears::TypeII,
                    HijriTabularEpoch::Thursday,
                ))
            }
            AnyCalendarKind::HijriUmmAlQura => AnyCalendar::HijriUmmAlQura(HijriUmmAlQura::new()),
            AnyCalendarKind::Iso => AnyCalendar::Iso(Iso),
            AnyCalendarKind::Japanese => {
                AnyCalendar::Japanese(Japanese::try_new_with_buffer_provider(provider)?)
            }
            AnyCalendarKind::JapaneseExtended => AnyCalendar::JapaneseExtended(
                JapaneseExtended::try_new_with_buffer_provider(provider)?,
            ),
            AnyCalendarKind::Persian => AnyCalendar::Persian(Persian),
            AnyCalendarKind::Roc => AnyCalendar::Roc(Roc),
        })
    }

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::new)]
    pub fn try_new_unstable<P>(provider: &P, kind: AnyCalendarKind) -> Result<Self, DataError>
    where
        P: DataProvider<crate::provider::CalendarJapaneseModernV1>
            + DataProvider<crate::provider::CalendarJapaneseExtendedV1>
            + DataProvider<crate::provider::CalendarChineseV1>
            + DataProvider<crate::provider::CalendarDangiV1>
            + DataProvider<crate::provider::CalendarHijriSimulatedMeccaV1>
            + ?Sized,
    {
        Ok(match kind {
            AnyCalendarKind::Buddhist => AnyCalendar::Buddhist(Buddhist),
            AnyCalendarKind::Chinese => AnyCalendar::Chinese(Chinese::try_new_unstable(provider)?),
            AnyCalendarKind::Coptic => AnyCalendar::Coptic(Coptic),
            AnyCalendarKind::Dangi => AnyCalendar::Dangi(Dangi::try_new_unstable(provider)?),
            AnyCalendarKind::Ethiopian => AnyCalendar::Ethiopian(Ethiopian::new_with_era_style(
                EthiopianEraStyle::AmeteMihret,
            )),
            AnyCalendarKind::EthiopianAmeteAlem => {
                AnyCalendar::Ethiopian(Ethiopian::new_with_era_style(EthiopianEraStyle::AmeteAlem))
            }
            AnyCalendarKind::Gregorian => AnyCalendar::Gregorian(Gregorian),
            AnyCalendarKind::Hebrew => AnyCalendar::Hebrew(Hebrew),
            AnyCalendarKind::Indian => AnyCalendar::Indian(Indian),
            AnyCalendarKind::HijriTabularTypeIIFriday => {
                AnyCalendar::HijriTabular(HijriTabular::new(
                    crate::cal::hijri::HijriTabularLeapYears::TypeII,
                    HijriTabularEpoch::Friday,
                ))
            }
            AnyCalendarKind::HijriSimulatedMecca => {
                AnyCalendar::HijriSimulated(HijriSimulated::try_new_mecca_unstable(provider)?)
            }
            AnyCalendarKind::HijriTabularTypeIIThursday => {
                AnyCalendar::HijriTabular(HijriTabular::new(
                    crate::cal::hijri::HijriTabularLeapYears::TypeII,
                    HijriTabularEpoch::Thursday,
                ))
            }
            AnyCalendarKind::HijriUmmAlQura => AnyCalendar::HijriUmmAlQura(HijriUmmAlQura::new()),
            AnyCalendarKind::Iso => AnyCalendar::Iso(Iso),
            AnyCalendarKind::Japanese => {
                AnyCalendar::Japanese(Japanese::try_new_unstable(provider)?)
            }
            AnyCalendarKind::JapaneseExtended => {
                AnyCalendar::JapaneseExtended(JapaneseExtended::try_new_unstable(provider)?)
            }
            AnyCalendarKind::Persian => AnyCalendar::Persian(Persian),
            AnyCalendarKind::Roc => AnyCalendar::Roc(Roc),
        })
    }

    /// The [`AnyCalendarKind`] corresponding to the calendar this contains
    pub fn kind(&self) -> AnyCalendarKind {
        match *self {
            Self::Buddhist(_) => AnyCalendarKind::Buddhist,
            Self::Chinese(_) => AnyCalendarKind::Chinese,
            Self::Coptic(_) => AnyCalendarKind::Coptic,
            Self::Dangi(_) => AnyCalendarKind::Dangi,
            Self::Ethiopian(ref e) => IntoAnyCalendar::kind(e),
            Self::Gregorian(_) => AnyCalendarKind::Gregorian,
            Self::Hebrew(_) => AnyCalendarKind::Hebrew,
            Self::Indian(_) => AnyCalendarKind::Indian,
            Self::HijriTabular(ref h) => IntoAnyCalendar::kind(h),
            Self::HijriSimulated(ref h) => IntoAnyCalendar::kind(h),
            Self::HijriUmmAlQura(_) => AnyCalendarKind::HijriUmmAlQura,
            Self::Iso(_) => AnyCalendarKind::Iso,
            Self::Japanese(_) => AnyCalendarKind::Japanese,
            Self::JapaneseExtended(_) => AnyCalendarKind::JapaneseExtended,
            Self::Persian(_) => AnyCalendarKind::Persian,
            Self::Roc(_) => AnyCalendarKind::Roc,
        }
    }
}

impl<C: AsCalendar<Calendar = AnyCalendar>> Date<C> {
    /// Convert this `Date<AnyCalendar>` to another `AnyCalendar`, if conversion is needed
    pub fn convert_any<'a>(&self, calendar: &'a AnyCalendar) -> Date<Ref<'a, AnyCalendar>> {
        if calendar.kind() != self.calendar.as_calendar().kind() {
            Date::new_from_iso(self.to_iso(), Ref(calendar))
        } else {
            Date {
                inner: self.inner,
                calendar: Ref(calendar),
            }
        }
    }
}

impl AnyDateInner {
    fn kind(&self) -> AnyCalendarKind {
        match *self {
            AnyDateInner::Buddhist(_) => AnyCalendarKind::Buddhist,
            AnyDateInner::Chinese(_) => AnyCalendarKind::Chinese,
            AnyDateInner::Coptic(_) => AnyCalendarKind::Coptic,
            AnyDateInner::Dangi(_) => AnyCalendarKind::Dangi,
            AnyDateInner::Ethiopian(_) => AnyCalendarKind::Ethiopian,
            AnyDateInner::Gregorian(_) => AnyCalendarKind::Gregorian,
            AnyDateInner::Hebrew(_) => AnyCalendarKind::Hebrew,
            AnyDateInner::Indian(_) => AnyCalendarKind::Indian,
            AnyDateInner::HijriTabular(
                _,
                HijriTabularLeapYears::TypeII,
                HijriTabularEpoch::Friday,
            ) => AnyCalendarKind::HijriTabularTypeIIFriday,
            AnyDateInner::HijriSimulated(_) => AnyCalendarKind::HijriSimulatedMecca,
            AnyDateInner::HijriTabular(
                _,
                HijriTabularLeapYears::TypeII,
                HijriTabularEpoch::Thursday,
            ) => AnyCalendarKind::HijriTabularTypeIIThursday,
            AnyDateInner::HijriUmmAlQura(_) => AnyCalendarKind::HijriUmmAlQura,
            AnyDateInner::Iso(_) => AnyCalendarKind::Iso,
            AnyDateInner::Japanese(_) => AnyCalendarKind::Japanese,
            AnyDateInner::JapaneseExtended(_) => AnyCalendarKind::JapaneseExtended,
            AnyDateInner::Persian(_) => AnyCalendarKind::Persian,
            AnyDateInner::Roc(_) => AnyCalendarKind::Roc,
        }
    }
}

/// Convenient type for selecting the kind of AnyCalendar to construct
#[non_exhaustive]
#[derive(Copy, Clone, Eq, PartialEq, Ord, PartialOrd, Hash, Debug)]
pub enum AnyCalendarKind {
    /// The kind of a [`Buddhist`] calendar
    Buddhist,
    /// The kind of a [`Chinese`] calendar
    Chinese,
    /// The kind of a [`Coptic`] calendar
    Coptic,
    /// The kind of a [`Dangi`] calendar
    Dangi,
    /// The kind of an [`Ethiopian`] calendar, with Amete Mihret era
    Ethiopian,
    /// The kind of an [`Ethiopian`] calendar, with Amete Alem era
    EthiopianAmeteAlem,
    /// The kind of a [`Gregorian`] calendar
    Gregorian,
    /// The kind of a [`Hebrew`] calendar
    Hebrew,
    /// The kind of a [`Indian`] calendar
    Indian,
    /// The kind of an [`HijriTabular`] calendar using [`HijriTabularLeapYears::TypeII`] and [`HijriTabularEpoch::Friday`]
    HijriTabularTypeIIFriday,
    /// The kind of an [`HijriSimulated`], Mecca calendar
    HijriSimulatedMecca,
    /// The kind of an [`HijriTabular`] calendar using [`HijriTabularLeapYears::TypeII`] and [`HijriTabularEpoch::Thursday`]
    HijriTabularTypeIIThursday,
    /// The kind of an [`HijriUmmAlQura`] calendar
    HijriUmmAlQura,
    /// The kind of an [`Iso`] calendar
    Iso,
    /// The kind of a [`Japanese`] calendar
    Japanese,
    /// The kind of a [`JapaneseExtended`] calendar
    JapaneseExtended,
    /// The kind of a [`Persian`] calendar
    Persian,
    /// The kind of a [`Roc`] calendar
    Roc,
}

impl AnyCalendarKind {
    /// Selects the [`AnyCalendarKind`] appropriate for the given [`CalendarPreferences`].
    pub fn new(prefs: CalendarPreferences) -> Self {
        let algo = prefs.calendar_algorithm;
        let region = prefs.locale_preferences.region();
        if let Some(kind) = algo.and_then(|a| a.try_into().ok()) {
            return kind;
        }
        if region == Some(region!("TH")) {
            AnyCalendarKind::Buddhist
        } else if region == Some(region!("AF")) || region == Some(region!("IR")) {
            AnyCalendarKind::Persian
        } else if region == Some(region!("SA")) && algo == Some(CalendarAlgorithm::Hijri(None)) {
            AnyCalendarKind::HijriSimulatedMecca
        } else {
            AnyCalendarKind::Gregorian
        }
    }

    fn debug_name(self) -> &'static str {
        match self {
            AnyCalendarKind::Buddhist => Buddhist.debug_name(),
            AnyCalendarKind::Chinese => Chinese::DEBUG_NAME,
            AnyCalendarKind::Coptic => Coptic.debug_name(),
            AnyCalendarKind::Dangi => Dangi::DEBUG_NAME,
            AnyCalendarKind::Ethiopian => Ethiopian(false).debug_name(),
            AnyCalendarKind::EthiopianAmeteAlem => Ethiopian(true).debug_name(),
            AnyCalendarKind::Gregorian => Gregorian.debug_name(),
            AnyCalendarKind::Hebrew => Hebrew.debug_name(),
            AnyCalendarKind::Indian => Indian.debug_name(),
            AnyCalendarKind::HijriTabularTypeIIFriday => HijriTabular::new(
                crate::cal::hijri::HijriTabularLeapYears::TypeII,
                HijriTabularEpoch::Friday,
            )
            .debug_name(),
            AnyCalendarKind::HijriSimulatedMecca => HijriSimulated::DEBUG_NAME,
            AnyCalendarKind::HijriTabularTypeIIThursday => HijriTabular::new(
                crate::cal::hijri::HijriTabularLeapYears::TypeII,
                HijriTabularEpoch::Thursday,
            )
            .debug_name(),
            AnyCalendarKind::HijriUmmAlQura => HijriUmmAlQura::DEBUG_NAME,
            AnyCalendarKind::Iso => Iso.debug_name(),
            AnyCalendarKind::Japanese => Japanese::DEBUG_NAME,
            AnyCalendarKind::JapaneseExtended => JapaneseExtended::DEBUG_NAME,
            AnyCalendarKind::Persian => Persian.debug_name(),
            AnyCalendarKind::Roc => Roc.debug_name(),
        }
    }
}

impl TryFrom<CalendarAlgorithm> for AnyCalendarKind {
    type Error = ();
    fn try_from(v: CalendarAlgorithm) -> Result<Self, Self::Error> {
        use CalendarAlgorithm::*;
        match v {
            Buddhist => Ok(AnyCalendarKind::Buddhist),
            Chinese => Ok(AnyCalendarKind::Chinese),
            Coptic => Ok(AnyCalendarKind::Coptic),
            Dangi => Ok(AnyCalendarKind::Dangi),
            Ethioaa => Ok(AnyCalendarKind::EthiopianAmeteAlem),
            Ethiopic => Ok(AnyCalendarKind::Ethiopian),
            Gregory => Ok(AnyCalendarKind::Gregorian),
            Hebrew => Ok(AnyCalendarKind::Hebrew),
            Indian => Ok(AnyCalendarKind::Indian),
            Hijri(None) => Err(()),
            Hijri(Some(HijriCalendarAlgorithm::Umalqura)) => Ok(AnyCalendarKind::HijriUmmAlQura),
            Hijri(Some(HijriCalendarAlgorithm::Tbla)) => {
                Ok(AnyCalendarKind::HijriTabularTypeIIThursday)
            }
            Hijri(Some(HijriCalendarAlgorithm::Civil)) => {
                Ok(AnyCalendarKind::HijriTabularTypeIIFriday)
            }
            Hijri(Some(HijriCalendarAlgorithm::Rgsa)) => Ok(AnyCalendarKind::HijriSimulatedMecca),
            Iso8601 => Ok(AnyCalendarKind::Iso),
            Japanese => Ok(AnyCalendarKind::Japanese),
            Persian => Ok(AnyCalendarKind::Persian),
            Roc => Ok(AnyCalendarKind::Roc),
            _ => {
                debug_assert!(false, "unknown calendar algorithm {v:?}");
                Err(())
            }
        }
    }
}

impl fmt::Display for AnyCalendarKind {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::Debug::fmt(self, f)
    }
}

/// Trait for calendars that may be converted to [`AnyCalendar`]
pub trait IntoAnyCalendar: Calendar + Sized {
    /// Convert this calendar into an [`AnyCalendar`], moving it
    ///
    /// You should not need to call this method directly
    fn to_any(self) -> AnyCalendar;

    /// The [`AnyCalendarKind`] enum variant associated with this calendar
    fn kind(&self) -> AnyCalendarKind;

    /// Move an [`AnyCalendar`] into a `Self`, or returning it as an error
    /// if the types do not match.
    ///
    /// You should not need to call this method directly
    fn from_any(any: AnyCalendar) -> Result<Self, AnyCalendar>;

    /// Convert an [`AnyCalendar`] reference into a `Self` reference.
    ///
    /// You should not need to call this method directly
    fn from_any_ref(any: &AnyCalendar) -> Option<&Self>;

    /// Convert a date for this calendar into an `AnyDateInner`
    ///
    /// You should not need to call this method directly
    fn date_to_any(&self, d: &Self::DateInner) -> AnyDateInner;
}

impl IntoAnyCalendar for AnyCalendar {
    #[inline]
    fn to_any(self) -> AnyCalendar {
        self
    }
    #[inline]
    fn kind(&self) -> AnyCalendarKind {
        self.kind()
    }
    #[inline]
    fn from_any(any: AnyCalendar) -> Result<Self, AnyCalendar> {
        Ok(any)
    }
    #[inline]
    fn from_any_ref(any: &AnyCalendar) -> Option<&Self> {
        Some(any)
    }
    #[inline]
    fn date_to_any(&self, d: &Self::DateInner) -> AnyDateInner {
        *d
    }
}

impl IntoAnyCalendar for Buddhist {
    #[inline]
    fn to_any(self) -> AnyCalendar {
        AnyCalendar::Buddhist(Buddhist)
    }
    #[inline]
    fn kind(&self) -> AnyCalendarKind {
        AnyCalendarKind::Buddhist
    }
    #[inline]
    fn from_any(any: AnyCalendar) -> Result<Self, AnyCalendar> {
        if let AnyCalendar::Buddhist(cal) = any {
            Ok(cal)
        } else {
            Err(any)
        }
    }
    #[inline]
    fn from_any_ref(any: &AnyCalendar) -> Option<&Self> {
        if let AnyCalendar::Buddhist(cal) = any {
            Some(cal)
        } else {
            None
        }
    }
    #[inline]
    fn date_to_any(&self, d: &Self::DateInner) -> AnyDateInner {
        AnyDateInner::Buddhist(*d)
    }
}

impl From<Buddhist> for AnyCalendar {
    fn from(value: Buddhist) -> AnyCalendar {
        value.to_any()
    }
}

impl IntoAnyCalendar for Chinese {
    #[inline]
    fn to_any(self) -> AnyCalendar {
        AnyCalendar::Chinese(self)
    }
    #[inline]
    fn kind(&self) -> AnyCalendarKind {
        AnyCalendarKind::Chinese
    }
    #[inline]
    fn from_any(any: AnyCalendar) -> Result<Self, AnyCalendar> {
        if let AnyCalendar::Chinese(cal) = any {
            Ok(cal)
        } else {
            Err(any)
        }
    }
    #[inline]
    fn from_any_ref(any: &AnyCalendar) -> Option<&Self> {
        if let AnyCalendar::Chinese(cal) = any {
            Some(cal)
        } else {
            None
        }
    }
    #[inline]
    fn date_to_any(&self, d: &Self::DateInner) -> AnyDateInner {
        AnyDateInner::Chinese(*d)
    }
}

impl From<Chinese> for AnyCalendar {
    fn from(value: Chinese) -> AnyCalendar {
        value.to_any()
    }
}

impl IntoAnyCalendar for Coptic {
    #[inline]
    fn to_any(self) -> AnyCalendar {
        AnyCalendar::Coptic(Coptic)
    }
    #[inline]
    fn kind(&self) -> AnyCalendarKind {
        AnyCalendarKind::Coptic
    }
    #[inline]
    fn from_any(any: AnyCalendar) -> Result<Self, AnyCalendar> {
        if let AnyCalendar::Coptic(cal) = any {
            Ok(cal)
        } else {
            Err(any)
        }
    }
    #[inline]
    fn from_any_ref(any: &AnyCalendar) -> Option<&Self> {
        if let AnyCalendar::Coptic(cal) = any {
            Some(cal)
        } else {
            None
        }
    }
    #[inline]
    fn date_to_any(&self, d: &Self::DateInner) -> AnyDateInner {
        AnyDateInner::Coptic(*d)
    }
}

impl From<Coptic> for AnyCalendar {
    fn from(value: Coptic) -> AnyCalendar {
        value.to_any()
    }
}

impl IntoAnyCalendar for Dangi {
    #[inline]
    fn to_any(self) -> AnyCalendar {
        AnyCalendar::Dangi(self)
    }
    #[inline]
    fn kind(&self) -> AnyCalendarKind {
        AnyCalendarKind::Dangi
    }
    #[inline]
    fn from_any(any: AnyCalendar) -> Result<Self, AnyCalendar> {
        if let AnyCalendar::Dangi(cal) = any {
            Ok(cal)
        } else {
            Err(any)
        }
    }
    #[inline]
    fn from_any_ref(any: &AnyCalendar) -> Option<&Self> {
        if let AnyCalendar::Dangi(cal) = any {
            Some(cal)
        } else {
            None
        }
    }
    #[inline]
    fn date_to_any(&self, d: &Self::DateInner) -> AnyDateInner {
        AnyDateInner::Dangi(*d)
    }
}

impl From<Dangi> for AnyCalendar {
    fn from(value: Dangi) -> AnyCalendar {
        value.to_any()
    }
}

impl IntoAnyCalendar for Ethiopian {
    // Amete Mihret calendars are the default
    #[inline]
    fn to_any(self) -> AnyCalendar {
        AnyCalendar::Ethiopian(self)
    }
    #[inline]
    fn kind(&self) -> AnyCalendarKind {
        if self.0 {
            AnyCalendarKind::EthiopianAmeteAlem
        } else {
            AnyCalendarKind::Ethiopian
        }
    }
    #[inline]
    fn from_any(any: AnyCalendar) -> Result<Self, AnyCalendar> {
        if let AnyCalendar::Ethiopian(cal) = any {
            Ok(cal)
        } else {
            Err(any)
        }
    }
    #[inline]
    fn from_any_ref(any: &AnyCalendar) -> Option<&Self> {
        if let AnyCalendar::Ethiopian(cal) = any {
            Some(cal)
        } else {
            None
        }
    }
    #[inline]
    fn date_to_any(&self, d: &Self::DateInner) -> AnyDateInner {
        AnyDateInner::Ethiopian(*d)
    }
}

impl From<Ethiopian> for AnyCalendar {
    fn from(value: Ethiopian) -> AnyCalendar {
        value.to_any()
    }
}

impl IntoAnyCalendar for Gregorian {
    #[inline]
    fn to_any(self) -> AnyCalendar {
        AnyCalendar::Gregorian(Gregorian)
    }
    #[inline]
    fn kind(&self) -> AnyCalendarKind {
        AnyCalendarKind::Gregorian
    }
    #[inline]
    fn from_any(any: AnyCalendar) -> Result<Self, AnyCalendar> {
        if let AnyCalendar::Gregorian(cal) = any {
            Ok(cal)
        } else {
            Err(any)
        }
    }
    #[inline]
    fn from_any_ref(any: &AnyCalendar) -> Option<&Self> {
        if let AnyCalendar::Gregorian(cal) = any {
            Some(cal)
        } else {
            None
        }
    }
    #[inline]
    fn date_to_any(&self, d: &Self::DateInner) -> AnyDateInner {
        AnyDateInner::Gregorian(*d)
    }
}

impl From<Gregorian> for AnyCalendar {
    fn from(value: Gregorian) -> AnyCalendar {
        value.to_any()
    }
}

impl IntoAnyCalendar for Hebrew {
    #[inline]
    fn to_any(self) -> AnyCalendar {
        AnyCalendar::Hebrew(Hebrew)
    }
    #[inline]
    fn kind(&self) -> AnyCalendarKind {
        AnyCalendarKind::Hebrew
    }
    #[inline]
    fn from_any(any: AnyCalendar) -> Result<Self, AnyCalendar> {
        if let AnyCalendar::Hebrew(cal) = any {
            Ok(cal)
        } else {
            Err(any)
        }
    }
    #[inline]
    fn from_any_ref(any: &AnyCalendar) -> Option<&Self> {
        if let AnyCalendar::Hebrew(cal) = any {
            Some(cal)
        } else {
            None
        }
    }
    #[inline]
    fn date_to_any(&self, d: &Self::DateInner) -> AnyDateInner {
        AnyDateInner::Hebrew(*d)
    }
}

impl From<Hebrew> for AnyCalendar {
    fn from(value: Hebrew) -> AnyCalendar {
        value.to_any()
    }
}

impl IntoAnyCalendar for Indian {
    #[inline]
    fn to_any(self) -> AnyCalendar {
        AnyCalendar::Indian(Indian)
    }
    #[inline]
    fn kind(&self) -> AnyCalendarKind {
        AnyCalendarKind::Indian
    }
    #[inline]
    fn from_any(any: AnyCalendar) -> Result<Self, AnyCalendar> {
        if let AnyCalendar::Indian(cal) = any {
            Ok(cal)
        } else {
            Err(any)
        }
    }
    #[inline]
    fn from_any_ref(any: &AnyCalendar) -> Option<&Self> {
        if let AnyCalendar::Indian(cal) = any {
            Some(cal)
        } else {
            None
        }
    }
    #[inline]
    fn date_to_any(&self, d: &Self::DateInner) -> AnyDateInner {
        AnyDateInner::Indian(*d)
    }
}

impl From<Indian> for AnyCalendar {
    fn from(value: Indian) -> AnyCalendar {
        value.to_any()
    }
}

impl IntoAnyCalendar for HijriTabular {
    #[inline]
    fn to_any(self) -> AnyCalendar {
        AnyCalendar::HijriTabular(self)
    }
    #[inline]
    fn kind(&self) -> AnyCalendarKind {
        match (self.leap_years, self.epoch) {
            (HijriTabularLeapYears::TypeII, HijriTabularEpoch::Friday) => {
                AnyCalendarKind::HijriTabularTypeIIFriday
            }
            (HijriTabularLeapYears::TypeII, HijriTabularEpoch::Thursday) => {
                AnyCalendarKind::HijriTabularTypeIIThursday
            }
        }
    }
    #[inline]
    fn from_any(any: AnyCalendar) -> Result<Self, AnyCalendar> {
        if let AnyCalendar::HijriTabular(cal) = any {
            Ok(cal)
        } else {
            Err(any)
        }
    }
    #[inline]
    fn from_any_ref(any: &AnyCalendar) -> Option<&Self> {
        if let AnyCalendar::HijriTabular(cal) = any {
            Some(cal)
        } else {
            None
        }
    }
    #[inline]
    fn date_to_any(&self, d: &Self::DateInner) -> AnyDateInner {
        AnyDateInner::HijriTabular(*d, self.leap_years, self.epoch)
    }
}

impl From<HijriTabular> for AnyCalendar {
    fn from(value: HijriTabular) -> AnyCalendar {
        value.to_any()
    }
}

impl IntoAnyCalendar for HijriSimulated {
    #[inline]
    fn to_any(self) -> AnyCalendar {
        AnyCalendar::HijriSimulated(self)
    }
    #[inline]
    fn kind(&self) -> AnyCalendarKind {
        match self.location {
            HijriSimulatedLocation::Mecca => AnyCalendarKind::HijriSimulatedMecca,
        }
    }
    #[inline]
    fn from_any(any: AnyCalendar) -> Result<Self, AnyCalendar> {
        if let AnyCalendar::HijriSimulated(cal) = any {
            Ok(cal)
        } else {
            Err(any)
        }
    }
    #[inline]
    fn from_any_ref(any: &AnyCalendar) -> Option<&Self> {
        if let AnyCalendar::HijriSimulated(cal) = any {
            Some(cal)
        } else {
            None
        }
    }
    #[inline]
    fn date_to_any(&self, d: &Self::DateInner) -> AnyDateInner {
        AnyDateInner::HijriSimulated(*d)
    }
}

impl From<HijriSimulated> for AnyCalendar {
    fn from(value: HijriSimulated) -> AnyCalendar {
        value.to_any()
    }
}

impl IntoAnyCalendar for HijriUmmAlQura {
    #[inline]
    fn to_any(self) -> AnyCalendar {
        AnyCalendar::HijriUmmAlQura(self)
    }
    #[inline]
    fn kind(&self) -> AnyCalendarKind {
        AnyCalendarKind::HijriUmmAlQura
    }
    #[inline]
    fn from_any(any: AnyCalendar) -> Result<Self, AnyCalendar> {
        if let AnyCalendar::HijriUmmAlQura(cal) = any {
            Ok(cal)
        } else {
            Err(any)
        }
    }
    #[inline]
    fn from_any_ref(any: &AnyCalendar) -> Option<&Self> {
        if let AnyCalendar::HijriUmmAlQura(cal) = any {
            Some(cal)
        } else {
            None
        }
    }
    #[inline]
    fn date_to_any(&self, d: &Self::DateInner) -> AnyDateInner {
        AnyDateInner::HijriUmmAlQura(*d)
    }
}

impl From<HijriUmmAlQura> for AnyCalendar {
    fn from(value: HijriUmmAlQura) -> AnyCalendar {
        value.to_any()
    }
}

impl IntoAnyCalendar for Iso {
    #[inline]
    fn to_any(self) -> AnyCalendar {
        AnyCalendar::Iso(Iso)
    }
    #[inline]
    fn kind(&self) -> AnyCalendarKind {
        AnyCalendarKind::Iso
    }
    #[inline]
    fn from_any(any: AnyCalendar) -> Result<Self, AnyCalendar> {
        if let AnyCalendar::Iso(cal) = any {
            Ok(cal)
        } else {
            Err(any)
        }
    }
    #[inline]
    fn from_any_ref(any: &AnyCalendar) -> Option<&Self> {
        if let AnyCalendar::Iso(cal) = any {
            Some(cal)
        } else {
            None
        }
    }
    #[inline]
    fn date_to_any(&self, d: &Self::DateInner) -> AnyDateInner {
        AnyDateInner::Iso(*d)
    }
}

impl From<Iso> for AnyCalendar {
    fn from(value: Iso) -> AnyCalendar {
        value.to_any()
    }
}

impl IntoAnyCalendar for Japanese {
    #[inline]
    fn to_any(self) -> AnyCalendar {
        AnyCalendar::Japanese(self)
    }
    #[inline]
    fn kind(&self) -> AnyCalendarKind {
        AnyCalendarKind::Japanese
    }
    #[inline]
    fn from_any(any: AnyCalendar) -> Result<Self, AnyCalendar> {
        if let AnyCalendar::Japanese(cal) = any {
            Ok(cal)
        } else {
            Err(any)
        }
    }
    #[inline]
    fn from_any_ref(any: &AnyCalendar) -> Option<&Self> {
        if let AnyCalendar::Japanese(cal) = any {
            Some(cal)
        } else {
            None
        }
    }
    #[inline]
    fn date_to_any(&self, d: &Self::DateInner) -> AnyDateInner {
        AnyDateInner::Japanese(*d)
    }
}

impl From<Japanese> for AnyCalendar {
    fn from(value: Japanese) -> AnyCalendar {
        value.to_any()
    }
}

impl IntoAnyCalendar for JapaneseExtended {
    #[inline]
    fn to_any(self) -> AnyCalendar {
        AnyCalendar::JapaneseExtended(self)
    }
    #[inline]
    fn kind(&self) -> AnyCalendarKind {
        AnyCalendarKind::JapaneseExtended
    }
    #[inline]
    fn from_any(any: AnyCalendar) -> Result<Self, AnyCalendar> {
        if let AnyCalendar::JapaneseExtended(cal) = any {
            Ok(cal)
        } else {
            Err(any)
        }
    }
    #[inline]
    fn from_any_ref(any: &AnyCalendar) -> Option<&Self> {
        if let AnyCalendar::JapaneseExtended(cal) = any {
            Some(cal)
        } else {
            None
        }
    }
    #[inline]
    fn date_to_any(&self, d: &Self::DateInner) -> AnyDateInner {
        AnyDateInner::JapaneseExtended(*d)
    }
}

impl From<JapaneseExtended> for AnyCalendar {
    fn from(value: JapaneseExtended) -> AnyCalendar {
        value.to_any()
    }
}

impl IntoAnyCalendar for Persian {
    #[inline]
    fn to_any(self) -> AnyCalendar {
        AnyCalendar::Persian(Persian)
    }
    #[inline]
    fn kind(&self) -> AnyCalendarKind {
        AnyCalendarKind::Persian
    }
    #[inline]
    fn from_any(any: AnyCalendar) -> Result<Self, AnyCalendar> {
        if let AnyCalendar::Persian(cal) = any {
            Ok(cal)
        } else {
            Err(any)
        }
    }
    #[inline]
    fn from_any_ref(any: &AnyCalendar) -> Option<&Self> {
        if let AnyCalendar::Persian(cal) = any {
            Some(cal)
        } else {
            None
        }
    }
    #[inline]
    fn date_to_any(&self, d: &Self::DateInner) -> AnyDateInner {
        AnyDateInner::Persian(*d)
    }
}

impl From<Persian> for AnyCalendar {
    fn from(value: Persian) -> AnyCalendar {
        value.to_any()
    }
}

impl IntoAnyCalendar for Roc {
    #[inline]
    fn to_any(self) -> AnyCalendar {
        AnyCalendar::Roc(Roc)
    }
    #[inline]
    fn kind(&self) -> AnyCalendarKind {
        AnyCalendarKind::Roc
    }
    #[inline]
    fn from_any(any: AnyCalendar) -> Result<Self, AnyCalendar> {
        if let AnyCalendar::Roc(cal) = any {
            Ok(cal)
        } else {
            Err(any)
        }
    }
    #[inline]
    fn from_any_ref(any: &AnyCalendar) -> Option<&Self> {
        if let AnyCalendar::Roc(cal) = any {
            Some(cal)
        } else {
            None
        }
    }
    #[inline]
    fn date_to_any(&self, d: &Self::DateInner) -> AnyDateInner {
        AnyDateInner::Roc(*d)
    }
}

impl From<Roc> for AnyCalendar {
    fn from(value: Roc) -> AnyCalendar {
        value.to_any()
    }
}

#[cfg(test)]
mod tests {
    use tinystr::tinystr;
    use types::MonthCode;

    use super::*;
    use crate::Ref;

    #[track_caller]
    fn single_test_roundtrip(
        calendar: Ref<AnyCalendar>,
        era: Option<(&str, Option<u8>)>,
        year: i32,
        month_code: &str,
        day: u8,
    ) {
        let month = types::MonthCode(month_code.parse().expect("month code must parse"));

        let date = Date::try_new_from_codes(era.map(|x| x.0), year, month, day, calendar)
            .unwrap_or_else(|e| {
                panic!(
                    "Failed to construct date for {} with {era:?}, {year}, {month}, {day}: {e:?}",
                    calendar.debug_name(),
                )
            });

        let roundtrip_year = date.year();
        // FIXME: these APIs should be improved
        let roundtrip_year = roundtrip_year.era_year_or_related_iso();
        let roundtrip_month = date.month().standard_code;
        let roundtrip_day = date.day_of_month().0;

        assert_eq!(
            (year, month, day),
            (roundtrip_year, roundtrip_month, roundtrip_day),
            "Failed to roundtrip for calendar {}",
            calendar.debug_name()
        );

        if let Some((era_code, era_index)) = era {
            let roundtrip_era_year = date.year().era().expect("year type should be era");
            assert_eq!(
                (era_code, era_index),
                (
                    roundtrip_era_year.era.as_str(),
                    roundtrip_era_year.era_index
                ),
                "Failed to roundtrip era for calendar {}",
                calendar.debug_name()
            )
        }

        let iso = date.to_iso();
        let reconstructed = Date::new_from_iso(iso, calendar);
        assert_eq!(
            date, reconstructed,
            "Failed to roundtrip via iso with {era:?}, {year}, {month}, {day}"
        )
    }

    #[track_caller]
    fn single_test_error(
        calendar: Ref<AnyCalendar>,
        era: Option<(&str, Option<u8>)>,
        year: i32,
        month_code: &str,
        day: u8,
        error: DateError,
    ) {
        let month = types::MonthCode(month_code.parse().expect("month code must parse"));

        let date = Date::try_new_from_codes(era.map(|x| x.0), year, month, day, calendar);
        assert_eq!(
            date,
            Err(error),
            "Construction with {era:?}, {year}, {month}, {day} did not return {error:?}"
        )
    }

    #[test]
    fn test_any_construction() {
        let buddhist = AnyCalendar::new(AnyCalendarKind::Buddhist);
        let chinese = AnyCalendar::new(AnyCalendarKind::Chinese);
        let coptic = AnyCalendar::new(AnyCalendarKind::Coptic);
        let dangi = AnyCalendar::new(AnyCalendarKind::Dangi);
        let ethioaa = AnyCalendar::new(AnyCalendarKind::EthiopianAmeteAlem);
        let ethiopian = AnyCalendar::new(AnyCalendarKind::Ethiopian);
        let gregorian = AnyCalendar::new(AnyCalendarKind::Gregorian);
        let hebrew = AnyCalendar::new(AnyCalendarKind::Hebrew);
        let indian = AnyCalendar::new(AnyCalendarKind::Indian);
        let hijri_civil: AnyCalendar = AnyCalendar::new(AnyCalendarKind::HijriTabularTypeIIFriday);
        let hijri_simulated: AnyCalendar = AnyCalendar::new(AnyCalendarKind::HijriSimulatedMecca);
        let hijri_astronomical: AnyCalendar =
            AnyCalendar::new(AnyCalendarKind::HijriTabularTypeIIThursday);
        let hijri_umm_al_qura: AnyCalendar = AnyCalendar::new(AnyCalendarKind::HijriUmmAlQura);
        let japanese = AnyCalendar::new(AnyCalendarKind::Japanese);
        let japanext = AnyCalendar::new(AnyCalendarKind::JapaneseExtended);
        let persian = AnyCalendar::new(AnyCalendarKind::Persian);
        let roc = AnyCalendar::new(AnyCalendarKind::Roc);
        let buddhist = Ref(&buddhist);
        let chinese = Ref(&chinese);
        let coptic = Ref(&coptic);
        let dangi = Ref(&dangi);
        let ethioaa = Ref(&ethioaa);
        let ethiopian = Ref(&ethiopian);
        let gregorian = Ref(&gregorian);
        let hebrew = Ref(&hebrew);
        let indian = Ref(&indian);
        let hijri_civil = Ref(&hijri_civil);
        let hijri_simulated = Ref(&hijri_simulated);
        let hijri_astronomical = Ref(&hijri_astronomical);
        let hijri_umm_al_qura = Ref(&hijri_umm_al_qura);
        let japanese = Ref(&japanese);
        let japanext = Ref(&japanext);
        let persian = Ref(&persian);
        let roc = Ref(&roc);

        single_test_roundtrip(buddhist, Some(("be", Some(0))), 100, "M03", 1);
        single_test_roundtrip(buddhist, None, 2000, "M03", 1);
        single_test_roundtrip(buddhist, Some(("be", Some(0))), -100, "M03", 1);
        single_test_error(
            buddhist,
            Some(("be", Some(0))),
            100,
            "M13",
            1,
            DateError::UnknownMonthCode(MonthCode(tinystr!(4, "M13"))),
        );

        single_test_roundtrip(coptic, Some(("am", Some(0))), 100, "M03", 1);
        single_test_roundtrip(coptic, None, 2000, "M03", 1);
        single_test_roundtrip(coptic, Some(("am", Some(0))), -99, "M03", 1);
        single_test_roundtrip(coptic, Some(("am", Some(0))), 100, "M13", 1);
        single_test_error(
            coptic,
            Some(("am", Some(0))),
            100,
            "M14",
            1,
            DateError::UnknownMonthCode(MonthCode(tinystr!(4, "M14"))),
        );

        single_test_roundtrip(ethiopian, Some(("am", Some(1))), 100, "M03", 1);
        single_test_roundtrip(ethiopian, None, 2000, "M03", 1);
        single_test_roundtrip(ethiopian, Some(("am", Some(1))), 2000, "M13", 1);
        single_test_roundtrip(ethiopian, Some(("aa", Some(0))), 5400, "M03", 1);
        single_test_error(
            ethiopian,
            Some(("am", Some(0))),
            0,
            "M03",
            1,
            DateError::Range {
                field: "year",
                value: 0,
                min: 1,
                max: i32::MAX,
            },
        );
        single_test_error(
            ethiopian,
            Some(("aa", Some(0))),
            5600,
            "M03",
            1,
            DateError::Range {
                field: "year",
                value: 5600,
                min: i32::MIN,
                max: 5500,
            },
        );
        single_test_error(
            ethiopian,
            Some(("am", Some(0))),
            100,
            "M14",
            1,
            DateError::UnknownMonthCode(MonthCode(tinystr!(4, "M14"))),
        );

        single_test_roundtrip(ethioaa, Some(("aa", Some(0))), 7000, "M13", 1);
        single_test_roundtrip(ethioaa, None, 7000, "M13", 1);
        single_test_roundtrip(ethioaa, Some(("aa", Some(0))), 100, "M03", 1);
        single_test_error(
            ethiopian,
            Some(("aa", Some(0))),
            100,
            "M14",
            1,
            DateError::UnknownMonthCode(MonthCode(tinystr!(4, "M14"))),
        );

        single_test_roundtrip(gregorian, Some(("ce", Some(1))), 100, "M03", 1);
        single_test_roundtrip(gregorian, None, 2000, "M03", 1);
        single_test_roundtrip(gregorian, Some(("bce", Some(0))), 100, "M03", 1);
        single_test_error(
            gregorian,
            Some(("ce", Some(1))),
            0,
            "M03",
            1,
            DateError::Range {
                field: "year",
                value: 0,
                min: 1,
                max: i32::MAX,
            },
        );
        single_test_error(
            gregorian,
            Some(("bce", Some(0))),
            0,
            "M03",
            1,
            DateError::Range {
                field: "year",
                value: 0,
                min: 1,
                max: i32::MAX,
            },
        );

        single_test_error(
            gregorian,
            Some(("bce", Some(0))),
            100,
            "M13",
            1,
            DateError::UnknownMonthCode(MonthCode(tinystr!(4, "M13"))),
        );

        single_test_roundtrip(indian, Some(("shaka", Some(0))), 100, "M03", 1);
        single_test_roundtrip(indian, None, 2000, "M12", 1);
        single_test_roundtrip(indian, None, -100, "M03", 1);
        single_test_roundtrip(indian, Some(("shaka", Some(0))), 0, "M03", 1);
        single_test_error(
            indian,
            Some(("shaka", Some(0))),
            100,
            "M13",
            1,
            DateError::UnknownMonthCode(MonthCode(tinystr!(4, "M13"))),
        );

        single_test_roundtrip(chinese, None, 400, "M02", 5);
        single_test_roundtrip(chinese, None, 4660, "M07", 29);
        single_test_roundtrip(chinese, None, -100, "M11", 12);
        single_test_error(
            chinese,
            None,
            4658,
            "M13",
            1,
            DateError::UnknownMonthCode(MonthCode(tinystr!(4, "M13"))),
        );

        single_test_roundtrip(dangi, None, 400, "M02", 5);
        single_test_roundtrip(dangi, None, 4660, "M08", 29);
        single_test_roundtrip(dangi, None, -1300, "M11", 12);
        single_test_error(
            dangi,
            None,
            10393,
            "M00L",
            1,
            DateError::UnknownMonthCode(MonthCode(tinystr!(4, "M00L"))),
        );

        single_test_roundtrip(japanese, Some(("reiwa", None)), 3, "M03", 1);
        single_test_roundtrip(japanese, Some(("heisei", None)), 6, "M12", 1);
        single_test_roundtrip(japanese, Some(("meiji", None)), 10, "M03", 1);
        single_test_roundtrip(japanese, Some(("ce", None)), 1000, "M03", 1);
        single_test_roundtrip(japanese, None, 1000, "M03", 1);
        single_test_roundtrip(japanese, Some(("bce", None)), 10, "M03", 1);
        single_test_error(
            japanese,
            Some(("ce", None)),
            0,
            "M03",
            1,
            DateError::Range {
                field: "year",
                value: 0,
                min: 1,
                max: i32::MAX,
            },
        );
        single_test_error(
            japanese,
            Some(("bce", Some(0))),
            0,
            "M03",
            1,
            DateError::Range {
                field: "year",
                value: 0,
                min: 1,
                max: i32::MAX,
            },
        );

        single_test_error(
            japanese,
            Some(("reiwa", None)),
            2,
            "M13",
            1,
            DateError::UnknownMonthCode(MonthCode(tinystr!(4, "M13"))),
        );

        single_test_roundtrip(japanext, Some(("reiwa", None)), 3, "M03", 1);
        single_test_roundtrip(japanext, Some(("heisei", None)), 6, "M12", 1);
        single_test_roundtrip(japanext, Some(("meiji", None)), 10, "M03", 1);
        single_test_roundtrip(japanext, Some(("tenpyokampo-749", None)), 1, "M04", 20);
        single_test_roundtrip(japanext, Some(("ce", None)), 100, "M03", 1);
        single_test_roundtrip(japanext, Some(("bce", None)), 10, "M03", 1);
        single_test_error(
            japanext,
            Some(("ce", None)),
            0,
            "M03",
            1,
            DateError::Range {
                field: "year",
                value: 0,
                min: 1,
                max: i32::MAX,
            },
        );
        single_test_error(
            japanext,
            Some(("bce", Some(0))),
            0,
            "M03",
            1,
            DateError::Range {
                field: "year",
                value: 0,
                min: 1,
                max: i32::MAX,
            },
        );

        single_test_error(
            japanext,
            Some(("reiwa", None)),
            2,
            "M13",
            1,
            DateError::UnknownMonthCode(MonthCode(tinystr!(4, "M13"))),
        );

        single_test_roundtrip(persian, Some(("ap", Some(0))), 477, "M03", 1);
        single_test_roundtrip(persian, None, 2083, "M07", 21);
        single_test_roundtrip(persian, Some(("ap", Some(0))), 1600, "M12", 20);
        single_test_error(
            persian,
            Some(("ap", Some(0))),
            100,
            "M9",
            1,
            DateError::UnknownMonthCode(MonthCode(tinystr!(4, "M9"))),
        );

        single_test_roundtrip(hebrew, Some(("am", Some(0))), 5773, "M03", 1);
        single_test_roundtrip(hebrew, None, 4993, "M07", 21);
        single_test_roundtrip(hebrew, Some(("am", Some(0))), 5012, "M12", 20);
        single_test_error(
            hebrew,
            Some(("am", Some(0))),
            100,
            "M9",
            1,
            DateError::UnknownMonthCode(MonthCode(tinystr!(4, "M9"))),
        );

        single_test_roundtrip(roc, Some(("roc", Some(1))), 10, "M05", 3);
        single_test_roundtrip(roc, Some(("broc", Some(0))), 15, "M01", 10);
        single_test_roundtrip(roc, None, 100, "M10", 30);

        single_test_roundtrip(hijri_simulated, Some(("ah", Some(0))), 477, "M03", 1);
        single_test_roundtrip(hijri_simulated, None, 2083, "M07", 21);
        single_test_roundtrip(hijri_simulated, Some(("ah", Some(0))), 1600, "M12", 20);
        single_test_error(
            hijri_simulated,
            Some(("ah", Some(0))),
            100,
            "M9",
            1,
            DateError::UnknownMonthCode(MonthCode(tinystr!(4, "M9"))),
        );

        single_test_roundtrip(hijri_civil, Some(("ah", Some(0))), 477, "M03", 1);
        single_test_roundtrip(hijri_civil, None, 2083, "M07", 21);
        single_test_roundtrip(hijri_civil, Some(("ah", Some(0))), 1600, "M12", 20);
        single_test_error(
            hijri_civil,
            Some(("ah", Some(0))),
            100,
            "M9",
            1,
            DateError::UnknownMonthCode(MonthCode(tinystr!(4, "M9"))),
        );

        single_test_roundtrip(hijri_umm_al_qura, Some(("ah", Some(0))), 477, "M03", 1);
        single_test_roundtrip(hijri_umm_al_qura, None, 2083, "M07", 21);
        single_test_roundtrip(hijri_umm_al_qura, Some(("ah", Some(0))), 1600, "M12", 20);
        single_test_error(
            hijri_umm_al_qura,
            Some(("ah", Some(0))),
            100,
            "M9",
            1,
            DateError::UnknownMonthCode(MonthCode(tinystr!(4, "M9"))),
        );

        single_test_roundtrip(hijri_astronomical, Some(("ah", Some(0))), 477, "M03", 1);
        single_test_roundtrip(hijri_astronomical, None, 2083, "M07", 21);
        single_test_roundtrip(hijri_astronomical, Some(("ah", Some(0))), 1600, "M12", 20);
        single_test_error(
            hijri_astronomical,
            Some(("ah", Some(0))),
            100,
            "M9",
            1,
            DateError::UnknownMonthCode(MonthCode(tinystr!(4, "M9"))),
        );
    }
}
