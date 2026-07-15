// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Module for working with multiple calendars at once

#![allow(clippy::unit_arg)]

use crate::cal::*;
use crate::{AsCalendar, Calendar, Date, Ref};

use crate::preferences::{CalendarAlgorithm, CalendarPreferences, HijriCalendarAlgorithm};
use icu_provider::prelude::*;

use core::fmt;

macro_rules! make_any_calendar {
    (
        $(#[$any_calendar_meta:meta])*
        $any_calendar_ident:ident,
        $(#[$any_date_meta:meta])*
        $any_date_ident:ident,

        $(
            $(#[$variant_meta:meta])*
            $variant:ident($ty:ty),
        )+
    ) => {
        $(#[$any_calendar_meta])*
        #[derive(Debug, Clone)]
        #[allow(deprecated)]
        pub enum $any_calendar_ident {
            $(
                #[doc = concat!("A [`", stringify!($ty), "`] calendar")]
                $(#[$variant_meta])*
                $variant($ty),
            )+
        }

        $(#[$any_date_meta])*
        #[doc = concat!("The inner date type for [`", stringify!($any_calendar_ident), "`]")]
        #[derive(Clone, PartialEq, Eq, Debug, Copy)]
        #[allow(deprecated)]
        pub enum $any_date_ident {
            $(
                #[doc = concat!("A date for a [`", stringify!($variant), "`] calendar")]
                $variant(<$ty as $crate::Calendar>::DateInner),
            )+
        }

        impl PartialOrd for $any_date_ident {
            fn partial_cmp(&self, other: &Self) -> Option<core::cmp::Ordering> {
                use $any_date_ident::*;
                match (self, other) {
                    $(
                        ($variant(d1), $variant(d2)) => d1.partial_cmp(d2),
                    )+
                    _ => None,
                }
            }
        }

        impl $crate::cal::scaffold::UnstableSealed for $any_calendar_ident {}
        #[allow(deprecated)]
        impl $crate::Calendar for $any_calendar_ident {
            type DateInner = $any_date_ident;
            type Year = $crate::types::YearInfo;
            type DateCompatibilityError = $crate::error::MismatchedCalendarError;

            fn new_date(
                &self,
                year: $crate::types::YearInput,
                month: $crate::types::Month,
                day: u8,
            ) -> Result<Self::DateInner, $crate::error::DateNewError> {
                Ok(match self {
                    $(
                        &Self::$variant(ref c) => $any_date_ident::$variant(c.new_date(year, month, day)?),
                    )+
                })
            }

            fn from_fields(
                &self,
                fields: $crate::types::DateFields,
                options: $crate::options::DateFromFieldsOptions,
            ) -> Result<Self::DateInner, $crate::error::DateFromFieldsError> {
                Ok(match self {
                    $(
                        &Self::$variant(ref c) => $any_date_ident::$variant(c.from_fields(fields, options)?),
                    )+
                })
            }

            fn has_cheap_iso_conversion(&self) -> bool {
                match self {
                    $(
                        Self::$variant(ref c) => c.has_cheap_iso_conversion(),
                    )+
                }
            }

            fn from_iso(&self, iso: <$crate::Iso as $crate::Calendar>::DateInner) -> Self::DateInner {
                match self {
                    $(
                        &Self::$variant(ref c) => $any_date_ident::$variant(c.from_iso(iso)),
                    )+
                }
            }

            fn to_iso(&self, date: &Self::DateInner) -> <$crate::Iso as $crate::Calendar>::DateInner {
                match (self, date) {
                    $(
                        (&Self::$variant(ref c), &$any_date_ident::$variant(d)) => c.to_iso(&d),
                    )+
                    // This is only reached from misuse of from_raw, a semi-internal api
                    _ => panic!(concat!(stringify!($any_calendar_ident), " with mismatched date type")),
                }
            }

            fn from_rata_die(&self, rd: $crate::types::RataDie) -> Self::DateInner {
                match self {
                    $(
                        &Self::$variant(ref c) => $any_date_ident::$variant(c.from_rata_die(rd)),
                    )+
                }
            }

            fn to_rata_die(&self, date: &Self::DateInner) -> $crate::types::RataDie {
                match (self, date) {
                    $(
                        (&Self::$variant(ref c), &$any_date_ident::$variant(d)) => c.to_rata_die(&d),
                    )+
                    // This is only reached from misuse of from_raw, a semi-internal api
                    _ => panic!(concat!(stringify!($any_calendar_ident), " with mismatched date type")),
                }
            }

            fn months_in_year(&self, date: &Self::DateInner) -> u8 {
                match (self, date) {
                    $(
                        (&Self::$variant(ref c), &$any_date_ident::$variant(d)) => c.months_in_year(&d),
                    )+
                    // This is only reached from misuse of from_raw, a semi-internal api
                    _ => panic!(concat!(stringify!($any_calendar_ident), " with mismatched date type")),
                }
            }

            fn days_in_year(&self, date: &Self::DateInner) -> u16 {
                match (self, date) {
                    $(
                        (&Self::$variant(ref c), &$any_date_ident::$variant(d)) => c.days_in_year(&d),
                    )+
                    // This is only reached from misuse of from_raw, a semi-internal api
                    _ => panic!(concat!(stringify!($any_calendar_ident), " with mismatched date type")),
                }
            }

            fn days_in_month(&self, date: &Self::DateInner) -> u8 {
                match (self, date) {
                    $(
                        (&Self::$variant(ref c), &$any_date_ident::$variant(d)) => c.days_in_month(&d),
                    )+
                    // This is only reached from misuse of from_raw, a semi-internal api
                    _ => panic!(concat!(stringify!($any_calendar_ident), " with mismatched date type")),
                }
            }

            fn year_info(&self, date: &Self::DateInner) -> $crate::types::YearInfo {
                match (self, date) {
                    $(
                        (&Self::$variant(ref c), &$any_date_ident::$variant(d)) => c.year_info(&d).into(),
                    )+
                    // This is only reached from misuse of from_raw, a semi-internal api
                    _ => panic!(concat!(stringify!($any_calendar_ident), " with mismatched date type")),
                }
            }

            /// The calendar-specific check if `date` is in a leap year
            fn is_in_leap_year(&self, date: &Self::DateInner) -> bool {
                match (self, date) {
                    $(
                        (&Self::$variant(ref c), &$any_date_ident::$variant(d)) => c.is_in_leap_year(&d),
                    )+
                    // This is only reached from misuse of from_raw, a semi-internal api
                    _ => panic!(concat!(stringify!($any_calendar_ident), " with mismatched date type")),
                }
            }

            /// The calendar-specific month represented by `date`
            fn month(&self, date: &Self::DateInner) -> $crate::types::MonthInfo {
                match (self, date) {
                    $(
                        (&Self::$variant(ref c), &$any_date_ident::$variant(d)) => c.month(&d),
                    )+
                    // This is only reached from misuse of from_raw, a semi-internal api
                    _ => panic!(concat!(stringify!($any_calendar_ident), " with mismatched date type")),
                }
            }

            /// The calendar-specific day-of-month represented by `date`
            fn day_of_month(&self, date: &Self::DateInner) -> $crate::types::DayOfMonth {
                match (self, date) {
                    $(
                        (&Self::$variant(ref c), &$any_date_ident::$variant(d)) => c.day_of_month(&d),
                    )+
                    // This is only reached from misuse of from_raw, a semi-internal api
                    _ => panic!(concat!(stringify!($any_calendar_ident), " with mismatched date type")),
                }
            }

            /// Information of the day of the year
            fn day_of_year(&self, date: &Self::DateInner) -> $crate::types::DayOfYear {
                match (self, date) {
                    $(
                        (&Self::$variant(ref c), &$any_date_ident::$variant(d)) => c.day_of_year(&d),
                    )+
                    // This is only reached from misuse of from_raw, a semi-internal api
                    _ => panic!(concat!(stringify!($any_calendar_ident), " with mismatched date type")),
                }
            }

            fn add(
                &self,
                date: &Self::DateInner,
                duration: $crate::types::DateDuration,
                options: $crate::options::DateAddOptions,
            ) -> Result<Self::DateInner, $crate::error::DateAddError> {
                let mut date = *date;
                match (self, &mut date) {
                    $(
                        (&Self::$variant(ref c), $any_date_ident::$variant(ref mut d)) => {
                            *d = c.add(d, duration, options)?;
                        },
                    )+
                    // This is only reached from misuse of from_raw, a semi-internal api
                    _ => panic!(concat!(stringify!($any_calendar_ident), " with mismatched date type")),
                }
                Ok(date)
            }

            fn until(
                &self,
                date1: &Self::DateInner,
                date2: &Self::DateInner,
                options: $crate::options::DateDifferenceOptions,
            ) -> $crate::types::DateDuration {
                match (self, date1, date2) {
                    $(
                        (Self::$variant(ref c), $any_date_ident::$variant(d1), $any_date_ident::$variant(d2)) => {
                            c.until(d1, d2, options)
                        }
                    )+
                    // This is only reached from misuse of from_raw, a semi-internal api
                    _ => panic!(concat!(stringify!($any_calendar_ident), " with mismatched date type")),
                }
            }

            fn check_date_compatibility(&self, other: &Self) -> Result<(), Self::DateCompatibilityError> {
                match (self, other) {
                    $(
                        (Self::$variant(ref c1), Self::$variant(ref c2)) if c1.check_date_compatibility(c2).is_ok() => {
                            Ok(())
                        }
                    )+
                    _ => Err($crate::error::MismatchedCalendarError),
                }
            }

            fn debug_name(&self) -> &'static str {
                match self {
                    $(
                        &Self::$variant(_) => concat!(stringify!($any_calendar_ident), " (", stringify!($variant), ")"),
                    )+
                }
            }

            fn calendar_algorithm(&self) -> Option<CalendarAlgorithm> {
                match self {
                    $(
                        Self::$variant(ref c) => c.calendar_algorithm(),
                    )+
                }
            }
        }
    };
}

macro_rules! make_any_calendar_impls {
    ($($variant:ident($ty:ty),)+) => {
        $(
            #[allow(deprecated)]
            impl IntoAnyCalendar for $ty {
                #[inline]
                fn to_any(self) -> AnyCalendar {
                    AnyCalendar::$variant(self)
                }
                #[inline]
                fn kind(&self) -> AnyCalendarKind {
                    AnyCalendar::$variant(*self).kind()
                }
                #[inline]
                fn from_any(any: AnyCalendar) -> Result<Self, AnyCalendar> {
                    if let AnyCalendar::$variant(c) = any {
                        Ok(c)
                    } else {
                        Err(any)
                    }

                }
                #[inline]
                fn from_any_ref(any: &AnyCalendar) -> Option<&Self> {
                    if let AnyCalendar::$variant(c) = any {
                        Some(c)
                    } else {
                        None
                    }
                }
                #[inline]
                fn date_to_any(&self, d: &Self::DateInner) -> AnyDateInner {
                    AnyDateInner::$variant(*d)
                }
            }
        )+

        #[allow(deprecated)]
        impl<C: AsCalendar<Calendar = AnyCalendar>> Date<C> {
            /// Convert this `Date<AnyCalendar>` to another [`AnyCalendar`], if conversion is needed
            pub fn convert_any<'a>(&self, calendar: &'a AnyCalendar) -> Date<Ref<'a, AnyCalendar>> {
                match (self.calendar(), calendar) {
                    $(
                        (AnyCalendar::$variant(c1), AnyCalendar::$variant(c2)) if c1.check_date_compatibility(c2).is_ok()
                            => Date::from_raw(*self.inner(), Ref(calendar)),
                    )+
                    _ => self.to_calendar(Ref(calendar)),
                }
            }
        }
    };
}

make_any_calendar!(
    /// This is a calendar that encompasses a selection of calendars from this crate.
    ///
    /// This allows for the construction of [`Date`] objects that have their calendar known at runtime.
    ///
    /// This can be constructed by calling `.into()` on a concrete calendar type if the calendar type is known at
    /// compile time. When the type is known at runtime, the [`AnyCalendar::new()`] and sibling methods may be used.
    ///
    /// [`Date`] can also be converted to [`AnyCalendar`]-compatible ones
    /// via [`Date::to_any()`](crate::Date::to_any()).
    ///
    /// There are many ways of constructing an [`AnyCalendar`]'d date:
    /// ```
    /// use icu::calendar::{AnyCalendar, AnyCalendarKind, Date, cal::{Japanese, Gregorian}, types::MonthCode};
    /// use icu::locale::locale;
    /// use tinystr::tinystr;
    /// # use std::rc::Rc;
    ///
    /// let locale = locale!("en-u-ca-japanese"); // English with the Japanese calendar
    ///
    /// let calendar = AnyCalendar::new(AnyCalendarKind::new(locale.into()));
    ///
    /// // This is a Date<AnyCalendar>
    /// let any_japanese_date = Date::try_new_gregorian(2020, 9, 1)
    ///     .expect("Failed to construct Gregorian Date.")
    ///     .to_calendar(calendar)
    ///     .to_any();
    ///
    /// // Construct a date in the appropriate typed calendar and convert
    /// let japanese_calendar = Japanese::new();
    /// let japanese_date = Date::try_new_japanese_with_calendar("reiwa", 2, 9, 1,
    ///                                                         japanese_calendar).unwrap();
    /// assert_eq!(japanese_date.to_any(), any_japanese_date);
    ///
    /// // this is also Date<AnyCalendar>, but it uses a different calendar
    /// let any_gregorian_date = any_japanese_date.to_calendar(Gregorian).to_any();
    ///
    /// // Date<AnyCalendar> does not have a total order
    /// assert!(any_gregorian_date <= any_gregorian_date);
    /// assert!(any_japanese_date <= any_japanese_date);
    /// assert!(!(any_gregorian_date <= any_japanese_date) && !(any_japanese_date <= any_gregorian_date));
    /// ```
    #[non_exhaustive]
    AnyCalendar,

    #[non_exhaustive]
    AnyDateInner,

    Buddhist(Buddhist),
    Chinese(ChineseTraditional),
    Coptic(Coptic),
    Dangi(KoreanTraditional),
    Ethiopian(Ethiopian),
    Gregorian(Gregorian),
    Hebrew(Hebrew),
    #[deprecated(since = "2.2.0", note = "see `HijriUmmAlQura`")]
    HijriSimulated(Hijri<hijri::AstronomicalSimulation>),
    HijriTabular(Hijri<hijri::TabularAlgorithm>),
    HijriUmmAlQura(Hijri<hijri::UmmAlQura>),
    Indian(Indian),
    Iso(Iso),
    Japanese(Japanese),
    #[deprecated(since = "2.2.0", note = "see `Japanese`")]
    JapaneseExtended(Japanese),
    Persian(Persian),
    Roc(Roc),
);

make_any_calendar_impls!(
    Buddhist(Buddhist),
    Chinese(ChineseTraditional),
    Coptic(Coptic),
    Dangi(KoreanTraditional),
    Ethiopian(Ethiopian),
    Gregorian(Gregorian),
    Hebrew(Hebrew),
    HijriTabular(Hijri<hijri::TabularAlgorithm>),
    HijriUmmAlQura(Hijri<hijri::UmmAlQura>),
    Indian(Indian),
    Iso(Iso),
    Japanese(Japanese),
    Persian(Persian),
    Roc(Roc),
    HijriSimulated(Hijri<hijri::AstronomicalSimulation>),
);

impl AnyCalendar {
    /// Constructs an [`AnyCalendar`] for a given calendar kind from compiled data.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub const fn new(kind: AnyCalendarKind) -> Self {
        match kind {
            AnyCalendarKind::Buddhist => AnyCalendar::Buddhist(Buddhist),
            AnyCalendarKind::Chinese => AnyCalendar::Chinese(ChineseTraditional::new()),
            AnyCalendarKind::Coptic => AnyCalendar::Coptic(Coptic),
            AnyCalendarKind::Dangi => AnyCalendar::Dangi(KoreanTraditional::new()),
            AnyCalendarKind::Ethiopian => AnyCalendar::Ethiopian(Ethiopian::new_with_era_style(
                EthiopianEraStyle::AmeteMihret,
            )),
            AnyCalendarKind::EthiopianAmeteAlem => {
                AnyCalendar::Ethiopian(Ethiopian::new_with_era_style(EthiopianEraStyle::AmeteAlem))
            }
            AnyCalendarKind::Gregorian => AnyCalendar::Gregorian(Gregorian),
            AnyCalendarKind::Hebrew => AnyCalendar::Hebrew(Hebrew),
            #[allow(deprecated)]
            AnyCalendarKind::HijriSimulatedMecca => {
                AnyCalendar::HijriSimulated(Hijri::new_simulated_mecca())
            }
            AnyCalendarKind::HijriTabularTypeIIFriday => {
                AnyCalendar::HijriTabular(Hijri::new_tabular(
                    hijri::TabularAlgorithmLeapYears::TypeII,
                    hijri::TabularAlgorithmEpoch::Friday,
                ))
            }
            AnyCalendarKind::HijriTabularTypeIIThursday => {
                AnyCalendar::HijriTabular(Hijri::new_tabular(
                    hijri::TabularAlgorithmLeapYears::TypeII,
                    hijri::TabularAlgorithmEpoch::Thursday,
                ))
            }
            AnyCalendarKind::HijriUmmAlQura => {
                AnyCalendar::HijriUmmAlQura(Hijri::new_umm_al_qura())
            }
            AnyCalendarKind::Indian => AnyCalendar::Indian(Indian),
            AnyCalendarKind::Iso => AnyCalendar::Iso(Iso),
            #[allow(deprecated)]
            AnyCalendarKind::Japanese | AnyCalendarKind::JapaneseExtended => {
                AnyCalendar::Japanese(Japanese::new())
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
            AnyCalendarKind::Chinese => AnyCalendar::Chinese(ChineseTraditional::new()),
            AnyCalendarKind::Coptic => AnyCalendar::Coptic(Coptic),
            AnyCalendarKind::Dangi => AnyCalendar::Dangi(KoreanTraditional::new()),
            AnyCalendarKind::Ethiopian => AnyCalendar::Ethiopian(Ethiopian::new_with_era_style(
                EthiopianEraStyle::AmeteMihret,
            )),
            AnyCalendarKind::EthiopianAmeteAlem => {
                AnyCalendar::Ethiopian(Ethiopian::new_with_era_style(EthiopianEraStyle::AmeteAlem))
            }
            AnyCalendarKind::Gregorian => AnyCalendar::Gregorian(Gregorian),
            AnyCalendarKind::Hebrew => AnyCalendar::Hebrew(Hebrew),
            #[allow(deprecated)]
            AnyCalendarKind::HijriSimulatedMecca => {
                AnyCalendar::HijriSimulated(Hijri::new_simulated_mecca())
            }
            AnyCalendarKind::HijriTabularTypeIIFriday => {
                AnyCalendar::HijriTabular(Hijri::new_tabular(
                    hijri::TabularAlgorithmLeapYears::TypeII,
                    hijri::TabularAlgorithmEpoch::Friday,
                ))
            }
            AnyCalendarKind::HijriTabularTypeIIThursday => {
                AnyCalendar::HijriTabular(Hijri::new_tabular(
                    hijri::TabularAlgorithmLeapYears::TypeII,
                    hijri::TabularAlgorithmEpoch::Thursday,
                ))
            }
            AnyCalendarKind::HijriUmmAlQura => {
                AnyCalendar::HijriUmmAlQura(Hijri::new_umm_al_qura())
            }
            AnyCalendarKind::Indian => AnyCalendar::Indian(Indian),
            AnyCalendarKind::Iso => AnyCalendar::Iso(Iso),
            #[allow(deprecated)]
            AnyCalendarKind::Japanese | AnyCalendarKind::JapaneseExtended => {
                AnyCalendar::Japanese(Japanese::try_new_with_buffer_provider(provider)?)
            }
            AnyCalendarKind::Persian => AnyCalendar::Persian(Persian),
            AnyCalendarKind::Roc => AnyCalendar::Roc(Roc),
        })
    }

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::new)]
    pub fn try_new_unstable<P>(provider: &P, kind: AnyCalendarKind) -> Result<Self, DataError>
    where
        P: DataProvider<crate::provider::CalendarJapaneseModernV1> + ?Sized,
    {
        Ok(match kind {
            AnyCalendarKind::Buddhist => AnyCalendar::Buddhist(Buddhist),
            AnyCalendarKind::Chinese => AnyCalendar::Chinese(ChineseTraditional::new()),
            AnyCalendarKind::Coptic => AnyCalendar::Coptic(Coptic),
            AnyCalendarKind::Dangi => AnyCalendar::Dangi(KoreanTraditional::new()),
            AnyCalendarKind::Ethiopian => AnyCalendar::Ethiopian(Ethiopian::new_with_era_style(
                EthiopianEraStyle::AmeteMihret,
            )),
            AnyCalendarKind::EthiopianAmeteAlem => {
                AnyCalendar::Ethiopian(Ethiopian::new_with_era_style(EthiopianEraStyle::AmeteAlem))
            }
            AnyCalendarKind::Gregorian => AnyCalendar::Gregorian(Gregorian),
            AnyCalendarKind::Hebrew => AnyCalendar::Hebrew(Hebrew),
            AnyCalendarKind::HijriTabularTypeIIFriday => {
                AnyCalendar::HijriTabular(Hijri::new_tabular(
                    hijri::TabularAlgorithmLeapYears::TypeII,
                    hijri::TabularAlgorithmEpoch::Friday,
                ))
            }
            #[allow(deprecated)]
            AnyCalendarKind::HijriSimulatedMecca => {
                AnyCalendar::HijriSimulated(Hijri::new_simulated_mecca())
            }
            AnyCalendarKind::HijriTabularTypeIIThursday => {
                AnyCalendar::HijriTabular(Hijri::new_tabular(
                    hijri::TabularAlgorithmLeapYears::TypeII,
                    hijri::TabularAlgorithmEpoch::Thursday,
                ))
            }
            AnyCalendarKind::HijriUmmAlQura => {
                AnyCalendar::HijriUmmAlQura(Hijri::new_umm_al_qura())
            }
            AnyCalendarKind::Indian => AnyCalendar::Indian(Indian),
            AnyCalendarKind::Iso => AnyCalendar::Iso(Iso),
            #[allow(deprecated)]
            AnyCalendarKind::Japanese | AnyCalendarKind::JapaneseExtended => {
                AnyCalendar::Japanese(Japanese::try_new_unstable(provider)?)
            }
            AnyCalendarKind::Persian => AnyCalendar::Persian(Persian),
            AnyCalendarKind::Roc => AnyCalendar::Roc(Roc),
        })
    }

    #[cfg(feature = "datagen")]
    #[doc(hidden)]
    /// Used by datagen to determine era indices in the absence of any data.
    pub fn new_without_data(kind: AnyCalendarKind) -> Self {
        match kind {
            AnyCalendarKind::Buddhist => AnyCalendar::Buddhist(Buddhist),
            AnyCalendarKind::Chinese => AnyCalendar::Chinese(ChineseTraditional::new()),
            AnyCalendarKind::Coptic => AnyCalendar::Coptic(Coptic),
            AnyCalendarKind::Dangi => AnyCalendar::Dangi(KoreanTraditional::new()),
            AnyCalendarKind::Ethiopian => AnyCalendar::Ethiopian(Ethiopian::new_with_era_style(
                EthiopianEraStyle::AmeteMihret,
            )),
            AnyCalendarKind::EthiopianAmeteAlem => {
                AnyCalendar::Ethiopian(Ethiopian::new_with_era_style(EthiopianEraStyle::AmeteAlem))
            }
            AnyCalendarKind::Gregorian => AnyCalendar::Gregorian(Gregorian),
            AnyCalendarKind::Hebrew => AnyCalendar::Hebrew(Hebrew),
            AnyCalendarKind::HijriTabularTypeIIFriday => {
                AnyCalendar::HijriTabular(Hijri::new_tabular(
                    hijri::TabularAlgorithmLeapYears::TypeII,
                    hijri::TabularAlgorithmEpoch::Friday,
                ))
            }
            #[allow(deprecated)]
            AnyCalendarKind::HijriSimulatedMecca => {
                AnyCalendar::HijriSimulated(Hijri::new_simulated_mecca())
            }
            AnyCalendarKind::HijriTabularTypeIIThursday => {
                AnyCalendar::HijriTabular(Hijri::new_tabular(
                    hijri::TabularAlgorithmLeapYears::TypeII,
                    hijri::TabularAlgorithmEpoch::Thursday,
                ))
            }
            AnyCalendarKind::HijriUmmAlQura => {
                AnyCalendar::HijriUmmAlQura(Hijri::new_umm_al_qura())
            }
            AnyCalendarKind::Indian => AnyCalendar::Indian(Indian),
            AnyCalendarKind::Iso => AnyCalendar::Iso(Iso),
            #[allow(deprecated)]
            AnyCalendarKind::Japanese | AnyCalendarKind::JapaneseExtended => {
                AnyCalendar::Japanese(Japanese::default())
            }
            AnyCalendarKind::Persian => AnyCalendar::Persian(Persian),
            AnyCalendarKind::Roc => AnyCalendar::Roc(Roc),
        }
    }

    /// The [`AnyCalendarKind`] corresponding to the calendar this contains
    pub fn kind(&self) -> AnyCalendarKind {
        match *self {
            Self::Buddhist(_) => AnyCalendarKind::Buddhist,
            Self::Chinese(_) => AnyCalendarKind::Chinese,
            Self::Coptic(_) => AnyCalendarKind::Coptic,
            Self::Dangi(_) => AnyCalendarKind::Dangi,
            Self::Ethiopian(ref c) => match c.era_style() {
                EthiopianEraStyle::AmeteAlem => AnyCalendarKind::EthiopianAmeteAlem,
                EthiopianEraStyle::AmeteMihret => AnyCalendarKind::Ethiopian,
            },
            Self::Gregorian(_) => AnyCalendarKind::Gregorian,
            Self::Hebrew(_) => AnyCalendarKind::Hebrew,
            #[allow(deprecated)]
            Self::HijriSimulated(_) => AnyCalendarKind::HijriSimulatedMecca,
            Self::HijriTabular(ref c) => match c.0 {
                hijri::TabularAlgorithm {
                    leap_years: hijri::TabularAlgorithmLeapYears::TypeII,
                    epoch: hijri::TabularAlgorithmEpoch::Friday,
                } => AnyCalendarKind::HijriTabularTypeIIFriday,
                hijri::TabularAlgorithm {
                    leap_years: hijri::TabularAlgorithmLeapYears::TypeII,
                    epoch: hijri::TabularAlgorithmEpoch::Thursday,
                } => AnyCalendarKind::HijriTabularTypeIIThursday,
            },
            Self::HijriUmmAlQura(_) => AnyCalendarKind::HijriUmmAlQura,
            Self::Indian(_) => AnyCalendarKind::Indian,
            Self::Iso(_) => AnyCalendarKind::Iso,
            #[allow(deprecated)]
            Self::Japanese(_) | Self::JapaneseExtended(_) => AnyCalendarKind::Japanese,
            Self::Persian(_) => AnyCalendarKind::Persian,
            Self::Roc(_) => AnyCalendarKind::Roc,
        }
    }
}

/// Convenient type for selecting the kind of [`AnyCalendar`] to construct
#[non_exhaustive]
#[derive(Copy, Clone, Eq, PartialEq, Ord, PartialOrd, Hash, Debug)]
pub enum AnyCalendarKind {
    /// The kind of a [`Buddhist`] calendar
    ///
    /// This corresponds to the `"buddhist"` [CLDR calendar](https://unicode.org/reports/tr35/#UnicodeCalendarIdentifier).
    Buddhist,
    /// The kind of a [`Chinese`] calendar
    ///
    /// This corresponds to the `"chinese"` [CLDR calendar](https://unicode.org/reports/tr35/#UnicodeCalendarIdentifier).
    Chinese,
    /// The kind of a [`Coptic`] calendar
    ///
    /// This corresponds to the `"coptic"` [CLDR calendar](https://unicode.org/reports/tr35/#UnicodeCalendarIdentifier).
    Coptic,
    /// The kind of a [`Dangi`] calendar
    ///
    /// This corresponds to the `"dangi"` [CLDR calendar](https://unicode.org/reports/tr35/#UnicodeCalendarIdentifier).
    Dangi,
    /// The kind of an [`Ethiopian`] calendar, with Amete Mihret era
    ///
    /// This corresponds to the `"ethiopic"` [CLDR calendar](https://unicode.org/reports/tr35/#UnicodeCalendarIdentifier).
    Ethiopian,
    /// The kind of an [`Ethiopian`] calendar, with Amete Alem era
    ///
    /// This corresponds to the `"ethioaa"` [CLDR calendar](https://unicode.org/reports/tr35/#UnicodeCalendarIdentifier).
    EthiopianAmeteAlem,
    /// The kind of a [`Gregorian`] calendar
    ///
    /// This corresponds to the `"gregory"` [CLDR calendar](https://unicode.org/reports/tr35/#UnicodeCalendarIdentifier).
    Gregorian,
    /// The kind of a [`Hebrew`] calendar
    ///
    /// This corresponds to the `"hebrew"` [CLDR calendar](https://unicode.org/reports/tr35/#UnicodeCalendarIdentifier).
    Hebrew,
    /// The kind of an [`Indian`] calendar
    ///
    /// This corresponds to the `"indian"` [CLDR calendar](https://unicode.org/reports/tr35/#UnicodeCalendarIdentifier).
    Indian,
    /// The kind of a [`HijriTabular`] calendar using [`HijriTabularLeapYears::TypeII`] and [`HijriTabularEpoch::Friday`]
    ///
    /// This corresponds to the `"islamic-civil"` [CLDR calendar](https://unicode.org/reports/tr35/#UnicodeCalendarIdentifier).
    HijriTabularTypeIIFriday,
    /// The kind of a [`HijriSimulated`] calendar
    ///
    /// This does not correspond to a CLDR calendar.
    HijriSimulatedMecca,
    /// The kind of a [`HijriTabular`] calendar using [`HijriTabularLeapYears::TypeII`] and [`HijriTabularEpoch::Thursday`]
    ///
    /// This corresponds to the `"islamic-tbla"` [CLDR calendar](https://unicode.org/reports/tr35/#UnicodeCalendarIdentifier).
    HijriTabularTypeIIThursday,
    /// The kind of a [`HijriUmmAlQura`] calendar
    ///
    /// This corresponds to the `"islamic-umalqura"` [CLDR calendar](https://unicode.org/reports/tr35/#UnicodeCalendarIdentifier).
    HijriUmmAlQura,
    /// The kind of an [`Iso`] calendar
    ///
    /// This corresponds to the `"iso8601"` [CLDR calendar](https://unicode.org/reports/tr35/#UnicodeCalendarIdentifier).
    Iso,
    /// The kind of a [`Japanese`] calendar
    ///
    /// This corresponds to the `"japanese"` [CLDR calendar](https://unicode.org/reports/tr35/#UnicodeCalendarIdentifier).
    Japanese,
    /// Deprecated, use `Japanese`.
    #[deprecated(since = "2.2.0", note = "use `Japanese`")]
    JapaneseExtended,
    /// The kind of a [`Persian`] calendar
    ///
    /// This corresponds to the `"persian"` [CLDR calendar](https://unicode.org/reports/tr35/#UnicodeCalendarIdentifier).
    Persian,
    /// The kind of a [`Roc`] calendar
    ///
    /// This corresponds to the `"roc"` [CLDR calendar](https://unicode.org/reports/tr35/#UnicodeCalendarIdentifier).
    Roc,
}

impl AnyCalendarKind {
    /// Selects the [`AnyCalendarKind`] appropriate for the given [`CalendarPreferences`].
    pub fn new(prefs: CalendarPreferences) -> Self {
        prefs
            .resolved_algorithm()
            .try_into()
            .unwrap_or(Self::Gregorian)
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
            Hijri(Some(HijriCalendarAlgorithm::Rgsa)) => Err(()),
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

    /// Convert a date for this calendar into a `AnyDateInner`
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

impl<C: IntoAnyCalendar> Date<C> {
    /// Type-erase the date, converting it to a date for [`AnyCalendar`]
    pub fn to_any(self) -> Date<AnyCalendar> {
        Date::from_raw(
            self.calendar().date_to_any(self.inner()),
            self.into_calendar().to_any(),
        )
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::error::DateNewError;
    use crate::types::{Month, YearInput};
    use crate::Ref;

    #[track_caller]
    fn single_test_roundtrip(
        calendar: Ref<AnyCalendar>,
        era: Option<(&str, Option<u8>)>,
        year: i32,
        month: Month,
        day: u8,
    ) {
        let input = if let Some((era, _)) = era {
            YearInput::EraYear(era, year)
        } else {
            year.into()
        };
        let date = Date::try_new(input, month, day, calendar).unwrap_or_else(|e| {
            panic!(
                "Failed to construct date for {} with {era:?}, {year}, {month:?}, {day}: {e:?}",
                calendar.debug_name(),
            )
        });

        assert_eq!(
            (month, day),
            (date.month().to_input(), date.day_of_month().0),
            "Failed to roundtrip for calendar {}",
            calendar.debug_name()
        );

        if let Some((era_code, era_index)) = era {
            let roundtrip_era_year = date.year().era().expect("year type should be era");

            assert_eq!(
                (era_code, era_index, year),
                (
                    roundtrip_era_year.era.as_str(),
                    roundtrip_era_year.era_index,
                    roundtrip_era_year.year,
                ),
                "Failed to roundtrip era for calendar {}",
                calendar.debug_name()
            )
        } else {
            assert_eq!(
                year,
                date.year().extended_year(),
                "Failed to roundtrip year for calendar {}",
                calendar.debug_name()
            );
        }

        assert_eq!(
            Date::from_rata_die(date.to_rata_die(), calendar),
            date,
            "Failed to roundtrip via iso with {era:?}, {year}, {month:?}, {day}"
        )
    }

    #[track_caller]
    fn single_test_error(
        calendar: Ref<AnyCalendar>,
        era: Option<(&str, Option<u8>)>,
        year: i32,
        month: Month,
        day: u8,
        error: DateNewError,
    ) {
        let input = if let Some((era, _)) = era {
            YearInput::EraYear(era, year)
        } else {
            year.into()
        };
        let date = Date::try_new(input, month, day, calendar);
        assert_eq!(
            date,
            Err(error),
            "Construction with {era:?}, {year}, {month:?}, {day} did not return {error:?}"
        )
    }

    #[test]
    fn buddhist() {
        let buddhist = AnyCalendar::new(AnyCalendarKind::Buddhist);
        let buddhist = Ref(&buddhist);
        single_test_roundtrip(buddhist, Some(("be", Some(0))), 100, Month::new(3), 1);
        single_test_roundtrip(buddhist, None, 100, Month::new(3), 1);
        single_test_roundtrip(buddhist, None, -100, Month::new(3), 1);
        single_test_roundtrip(buddhist, Some(("be", Some(0))), -100, Month::new(3), 1);
        single_test_error(
            buddhist,
            Some(("be", Some(0))),
            100,
            Month::new(13),
            1,
            DateNewError::MonthNotInCalendar,
        );
    }

    #[test]
    fn coptic() {
        let coptic = AnyCalendar::new(AnyCalendarKind::Coptic);
        let coptic = Ref(&coptic);
        single_test_roundtrip(coptic, Some(("am", Some(0))), 100, Month::new(3), 1);
        single_test_roundtrip(coptic, None, 2000, Month::new(3), 1);
        single_test_roundtrip(coptic, None, -100, Month::new(3), 1);
        single_test_roundtrip(coptic, Some(("am", Some(0))), -99, Month::new(3), 1);
        single_test_roundtrip(coptic, Some(("am", Some(0))), 100, Month::new(13), 1);
        single_test_error(
            coptic,
            Some(("am", Some(0))),
            100,
            Month::new(14),
            1,
            DateNewError::MonthNotInCalendar,
        );
    }

    #[test]
    fn ethiopian() {
        let ethiopian = AnyCalendar::new(AnyCalendarKind::Ethiopian);
        let ethiopian = Ref(&ethiopian);
        single_test_roundtrip(ethiopian, Some(("am", Some(1))), 100, Month::new(3), 1);
        single_test_roundtrip(ethiopian, None, 2000, Month::new(3), 1);
        single_test_roundtrip(ethiopian, None, -100, Month::new(3), 1);
        single_test_roundtrip(ethiopian, Some(("am", Some(1))), 2000, Month::new(13), 1);
        single_test_roundtrip(ethiopian, Some(("aa", Some(0))), 5400, Month::new(3), 1);
        // Since #6910, the era range is not enforced in try_new
        /*
        single_test_error(
            ethiopian,
            Some(("am", Some(0))),
            0,
            Month::new(3),
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
            Month::new(3),
            1,
            DateError::Range {
                field: "year",
                value: 5600,
                min: i32::MIN,
                max: 5500,
            },
        );
        */
        single_test_error(
            ethiopian,
            Some(("am", Some(0))),
            100,
            Month::new(14),
            1,
            DateNewError::MonthNotInCalendar,
        );
    }

    #[test]
    fn ethiopian_amete_alem() {
        let ethiopian_amete_alem = AnyCalendar::new(AnyCalendarKind::EthiopianAmeteAlem);
        let ethiopian_amete_alem = Ref(&ethiopian_amete_alem);
        single_test_roundtrip(
            ethiopian_amete_alem,
            Some(("aa", Some(0))),
            7000,
            Month::new(13),
            1,
        );
        single_test_roundtrip(ethiopian_amete_alem, None, 7000, Month::new(13), 1);
        single_test_roundtrip(ethiopian_amete_alem, None, -100, Month::new(13), 1);
        single_test_roundtrip(
            ethiopian_amete_alem,
            Some(("aa", Some(0))),
            100,
            Month::new(3),
            1,
        );
        single_test_error(
            ethiopian_amete_alem,
            Some(("aa", Some(0))),
            100,
            Month::new(14),
            1,
            DateNewError::MonthNotInCalendar,
        );
    }

    #[test]
    fn gregorian() {
        let gregorian = AnyCalendar::new(AnyCalendarKind::Gregorian);
        let gregorian = Ref(&gregorian);
        single_test_roundtrip(gregorian, Some(("ce", Some(1))), 100, Month::new(3), 1);
        single_test_roundtrip(gregorian, None, 2000, Month::new(3), 1);
        single_test_roundtrip(gregorian, None, -100, Month::new(3), 1);
        single_test_roundtrip(gregorian, Some(("bce", Some(0))), 100, Month::new(3), 1);
        // Since #6910, the era range is not enforced in try_new
        /*
        single_test_error(
            gregorian,
            Some(("ce", Some(1))),
            0,
            Month::new(3),
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
            Month::new(3),
            1,
            DateError::Range {
                field: "year",
                value: 0,
                min: 1,
                max: i32::MAX,
            },
        );
        */
        single_test_error(
            gregorian,
            Some(("bce", Some(0))),
            100,
            Month::new(13),
            1,
            DateNewError::MonthNotInCalendar,
        );
    }

    #[test]
    fn indian() {
        let indian = AnyCalendar::new(AnyCalendarKind::Indian);
        let indian = Ref(&indian);
        single_test_roundtrip(indian, Some(("shaka", Some(0))), 100, Month::new(3), 1);
        single_test_roundtrip(indian, None, 2000, Month::new(12), 1);
        single_test_roundtrip(indian, None, -100, Month::new(3), 1);
        single_test_roundtrip(indian, Some(("shaka", Some(0))), 0, Month::new(3), 1);
        single_test_error(
            indian,
            Some(("shaka", Some(0))),
            100,
            Month::new(13),
            1,
            DateNewError::MonthNotInCalendar,
        );
    }

    #[test]
    fn chinese_traditional() {
        let chinese_traditional = AnyCalendar::new(AnyCalendarKind::Chinese);
        let chinese_traditional = Ref(&chinese_traditional);
        single_test_roundtrip(chinese_traditional, None, 400, Month::new(2), 5);
        single_test_roundtrip(chinese_traditional, None, 4660, Month::new(7), 29);
        single_test_roundtrip(chinese_traditional, None, -100, Month::new(11), 12);
        single_test_error(
            chinese_traditional,
            None,
            4658,
            Month::new(13),
            1,
            DateNewError::MonthNotInCalendar,
        );
    }

    #[test]
    fn korean_traditional() {
        let korean_traditional = AnyCalendar::new(AnyCalendarKind::Dangi);
        let korean_traditional = Ref(&korean_traditional);
        single_test_roundtrip(korean_traditional, None, 400, Month::new(2), 5);
        single_test_roundtrip(korean_traditional, None, 4660, Month::new(8), 29);
        single_test_roundtrip(korean_traditional, None, -1300, Month::new(11), 12);
        single_test_error(
            korean_traditional,
            None,
            9393,
            Month::leap(0),
            1,
            DateNewError::MonthNotInCalendar,
        );
    }

    #[test]
    fn japanese() {
        let japanese = AnyCalendar::new(AnyCalendarKind::Japanese);
        let japanese = Ref(&japanese);
        single_test_roundtrip(japanese, Some(("reiwa", Some(6))), 3, Month::new(3), 1);
        single_test_roundtrip(japanese, Some(("heisei", Some(5))), 6, Month::new(12), 1);
        single_test_roundtrip(japanese, Some(("meiji", Some(2))), 10, Month::new(3), 1);
        single_test_roundtrip(japanese, Some(("ce", Some(1))), 1000, Month::new(3), 1);
        single_test_roundtrip(japanese, None, 1000, Month::new(3), 1);
        single_test_roundtrip(japanese, None, -100, Month::new(3), 1);
        single_test_roundtrip(japanese, None, 2024, Month::new(3), 1);
        single_test_roundtrip(japanese, Some(("bce", Some(0))), 10, Month::new(3), 1);
        // Since #6910, the era range is not enforced in try_new
        /*
        single_test_error(
            japanese,
            Some(("ce", None)),
            0,
            Month::new(3),
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
            Month::new(3),
            1,
            DateError::Range {
                field: "year",
                value: 0,
                min: 1,
                max: i32::MAX,
            },
        );
        */
        single_test_error(
            japanese,
            Some(("reiwa", None)),
            2,
            Month::new(13),
            1,
            DateNewError::MonthNotInCalendar,
        );
    }

    #[test]
    fn persian() {
        let persian = AnyCalendar::new(AnyCalendarKind::Persian);
        let persian = Ref(&persian);
        single_test_roundtrip(persian, Some(("ap", Some(0))), 477, Month::new(3), 1);
        single_test_roundtrip(persian, None, 2083, Month::new(7), 21);
        single_test_roundtrip(persian, None, -100, Month::new(7), 21);
        single_test_roundtrip(persian, Some(("ap", Some(0))), 1600, Month::new(12), 20);
        single_test_error(
            persian,
            Some(("ap", Some(0))),
            100,
            Month::new(50),
            1,
            DateNewError::MonthNotInCalendar,
        );
    }

    #[test]
    fn hebrew() {
        let hebrew = AnyCalendar::new(AnyCalendarKind::Hebrew);
        let hebrew = Ref(&hebrew);
        single_test_roundtrip(hebrew, Some(("am", Some(0))), 5773, Month::new(3), 1);
        single_test_roundtrip(hebrew, None, 4993, Month::new(7), 21);
        single_test_roundtrip(hebrew, None, -100, Month::new(7), 21);
        single_test_roundtrip(hebrew, Some(("am", Some(0))), 5012, Month::new(12), 20);
        single_test_error(
            hebrew,
            Some(("am", Some(0))),
            100,
            Month::new(50),
            1,
            DateNewError::MonthNotInCalendar,
        );
    }

    #[test]
    fn roc() {
        let roc = AnyCalendar::new(AnyCalendarKind::Roc);
        let roc = Ref(&roc);
        single_test_roundtrip(roc, Some(("roc", Some(1))), 10, Month::new(5), 3);
        single_test_roundtrip(roc, Some(("broc", Some(0))), 15, Month::new(1), 10);
        single_test_roundtrip(roc, None, 100, Month::new(10), 30);
        single_test_roundtrip(roc, None, -100, Month::new(10), 30);
    }

    #[test]
    fn hijri_tabular_friday() {
        let hijri_tabular_friday: AnyCalendar =
            AnyCalendar::new(AnyCalendarKind::HijriTabularTypeIIFriday);
        let hijri_tabular_friday = Ref(&hijri_tabular_friday);
        single_test_roundtrip(
            hijri_tabular_friday,
            Some(("ah", Some(0))),
            477,
            Month::new(3),
            1,
        );
        single_test_roundtrip(hijri_tabular_friday, None, 2083, Month::new(7), 21);
        single_test_roundtrip(hijri_tabular_friday, None, -100, Month::new(7), 21);
        single_test_roundtrip(
            hijri_tabular_friday,
            Some(("ah", Some(0))),
            1600,
            Month::new(12),
            20,
        );
        single_test_error(
            hijri_tabular_friday,
            Some(("ah", Some(0))),
            100,
            Month::new(50),
            1,
            DateNewError::MonthNotInCalendar,
        );
    }

    #[test]
    fn hijri_umm_al_qura() {
        let hijri_umm_al_qura: AnyCalendar = AnyCalendar::new(AnyCalendarKind::HijriUmmAlQura);
        let hijri_umm_al_qura = Ref(&hijri_umm_al_qura);
        single_test_roundtrip(
            hijri_umm_al_qura,
            Some(("ah", Some(0))),
            477,
            Month::new(3),
            1,
        );
        single_test_roundtrip(hijri_umm_al_qura, None, 2083, Month::new(7), 21);
        single_test_roundtrip(hijri_umm_al_qura, None, -100, Month::new(7), 21);
        single_test_roundtrip(
            hijri_umm_al_qura,
            Some(("ah", Some(0))),
            1600,
            Month::new(12),
            20,
        );
        single_test_error(
            hijri_umm_al_qura,
            Some(("ah", Some(0))),
            100,
            Month::new(50),
            1,
            DateNewError::MonthNotInCalendar,
        );
    }

    #[test]
    fn hijri_tabular_thursday() {
        let hijri_tabular_thursday: AnyCalendar =
            AnyCalendar::new(AnyCalendarKind::HijriTabularTypeIIThursday);
        let hijri_tabular_thursday = Ref(&hijri_tabular_thursday);
        single_test_roundtrip(
            hijri_tabular_thursday,
            Some(("ah", Some(0))),
            477,
            Month::new(3),
            1,
        );
        single_test_roundtrip(hijri_tabular_thursday, None, 2083, Month::new(7), 21);
        single_test_roundtrip(hijri_tabular_thursday, None, -100, Month::new(7), 21);
        single_test_roundtrip(
            hijri_tabular_thursday,
            Some(("ah", Some(0))),
            1600,
            Month::new(12),
            20,
        );
        single_test_error(
            hijri_tabular_thursday,
            Some(("ah", Some(0))),
            100,
            Month::new(50),
            1,
            DateNewError::MonthNotInCalendar,
        );
    }

    #[test]
    fn iso() {
        let iso = AnyCalendar::new(AnyCalendarKind::Iso);
        let iso = Ref(&iso);
        single_test_roundtrip(iso, Some(("default", Some(0))), 100, Month::new(3), 1);
        single_test_roundtrip(iso, None, 2000, Month::new(3), 1);
        single_test_roundtrip(iso, None, -100, Month::new(3), 1);
        single_test_roundtrip(iso, Some(("default", Some(0))), -100, Month::new(3), 1);
        single_test_error(
            iso,
            Some(("default", Some(0))),
            100,
            Month::new(13),
            1,
            DateNewError::MonthNotInCalendar,
        );
    }
}
