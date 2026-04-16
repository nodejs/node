// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Locale preferences used by this crate

#[doc(inline)]
/// **This is a reexport of a type in [`icu::locale`](icu_locale_core::preferences::extensions::unicode::keywords)**.
#[doc = "\n"] // prevent autoformatting
pub use icu_locale_core::preferences::extensions::unicode::keywords::CalendarAlgorithm;
/// **This is a reexport of a type in [`icu::locale`](icu_locale_core::preferences::extensions::unicode::keywords)**.
#[doc = "\n"] // prevent autoformatting
pub use icu_locale_core::preferences::extensions::unicode::keywords::FirstDay;
#[doc(inline)]
/// **This is a reexport of a type in [`icu::locale`](icu_locale_core::preferences::extensions::unicode::keywords)**.
#[doc = "\n"] // prevent autoformatting
pub use icu_locale_core::preferences::extensions::unicode::keywords::HijriCalendarAlgorithm;

use icu_locale_core::preferences::define_preferences;

define_preferences!(
    /// The preferences for calendars formatting.
    [Copy]
    CalendarPreferences,
    {
        /// The user's preferred calendar system.
        calendar_algorithm: CalendarAlgorithm
    }
);

define_preferences!(
    /// The preferences for the week information.
    [Copy]
    WeekPreferences,
    {
        /// The first day of the week
        first_weekday: FirstDay
    }
);

impl CalendarPreferences {
    /// Selects the [`CalendarAlgorithm`] appropriate for the given [`CalendarPreferences`].
    ///
    /// This is guaranteed to return a [`CalendarAlgorithm`] that successfully converts into an
    /// [`AnyCalendarKind`](crate::AnyCalendarKind) (i.e. not `-u-ca-islamic` or `-u-ca-islamic-rgsa`).
    ///
    /// # Example
    ///
    /// ```
    /// use icu::calendar::preferences::{CalendarAlgorithm, CalendarPreferences, HijriCalendarAlgorithm};
    /// use icu::locale::locale;
    ///
    /// assert_eq!(CalendarPreferences::from(&locale!("und")).resolved_algorithm(), CalendarAlgorithm::Gregory);
    /// assert_eq!(CalendarPreferences::from(&locale!("und-US-u-ca-hebrew")).resolved_algorithm(), CalendarAlgorithm::Hebrew);
    /// assert_eq!(CalendarPreferences::from(&locale!("und-AF")).resolved_algorithm(), CalendarAlgorithm::Persian);
    /// assert_eq!(CalendarPreferences::from(&locale!("und-US-u-rg-thxxxx")).resolved_algorithm(), CalendarAlgorithm::Buddhist);
    /// assert_eq!(
    ///     CalendarPreferences::from(&locale!("und-US-u-ca-islamic")).resolved_algorithm(),
    ///     CalendarAlgorithm::Hijri(Some(HijriCalendarAlgorithm::Civil))
    /// );
    /// # assert_eq!(CalendarPreferences::from(&"und-US-u-ca-islamic-rgsa".parse::<icu::locale::Locale>().unwrap()).resolved_algorithm(),
    /// #     CalendarAlgorithm::Hijri(Some(HijriCalendarAlgorithm::Civil))
    /// # );
    /// # assert_eq!(CalendarPreferences::from(&"und-US-u-ca-islamic-foo".parse::<icu::locale::Locale>().unwrap()).resolved_algorithm(),
    /// #     CalendarAlgorithm::Gregory
    /// # );
    /// # assert_eq!(CalendarPreferences::from(&"und-US-u-ca-hebrew-foo".parse::<icu::locale::Locale>().unwrap()).resolved_algorithm(),
    /// #     CalendarAlgorithm::Gregory
    /// # );
    /// ```
    pub fn resolved_algorithm(self) -> CalendarAlgorithm {
        use icu_locale_core::subtags::{region, Region};
        const AE: Region = region!("AE");
        const BH: Region = region!("BH");
        const KW: Region = region!("KW");
        const QA: Region = region!("QA");
        const SA: Region = region!("SA");
        const TH: Region = region!("TH");
        const AF: Region = region!("AF");
        const IR: Region = region!("IR");

        // This is tested to be consistent with CLDR in icu_provider_source::calendar::test_calendar_resolution
        match (
            self.calendar_algorithm,
            self.locale_preferences
                .to_data_locale_region_priority()
                .region,
        ) {
            (
                Some(CalendarAlgorithm::Hijri(None | Some(HijriCalendarAlgorithm::Rgsa))),
                Some(AE | BH | KW | QA | SA),
            ) => CalendarAlgorithm::Hijri(Some(HijriCalendarAlgorithm::Umalqura)),
            (Some(CalendarAlgorithm::Hijri(None | Some(HijriCalendarAlgorithm::Rgsa))), _) => {
                CalendarAlgorithm::Hijri(Some(HijriCalendarAlgorithm::Civil))
            }
            (Some(a), _) => a,
            (None, Some(TH)) => CalendarAlgorithm::Buddhist,
            (None, Some(AF | IR)) => CalendarAlgorithm::Persian,
            (None, _) => CalendarAlgorithm::Gregory,
        }
    }
}
