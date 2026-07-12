// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::cal::abstract_gregorian::{
    impl_with_abstract_gregorian, AbstractGregorian, GregorianYears,
};
use crate::cal::gregorian::CeBce;
use crate::calendar_arithmetic::ArithmeticDate;
use crate::error::{DateError, UnknownEraError};
use crate::provider::{CalendarJapaneseModernV1, EraStartDate, PackedEra};
use crate::{types, AsCalendar, Date};
use icu_provider::prelude::*;
use tinystr::{tinystr, TinyAsciiStr};

/// The [Japanese Calendar] (with modern eras only)
///
/// The [Japanese Calendar] is a variant of the [`Gregorian`](crate::cal::Gregorian) calendar
/// created by the Japanese government. It is identical to the Gregorian calendar except that
/// is uses Japanese eras instead of the Common Era.
///
/// This implementation extends proleptically for dates before the calendar's creation
/// in 6 Meiji (1873 CE). Gregorian eras are used before that.
///
/// This corresponds to the `"japanese"` [CLDR calendar](https://unicode.org/reports/tr35/#UnicodeCalendarIdentifier).
///
/// [Japanese calendar]: https://en.wikipedia.org/wiki/Japanese_calendar
///
/// # Era codes
///
/// This calendar currently supports seven era codes. It supports the five eras since its
/// introduction (`meiji`, `taisho`, `showa`, `heisei`, `reiwa`), as well as the Gregorian
/// `bce` (alias `bc`), and `ce` (alias `ad`) for earlier dates.
///
/// Future eras will also be added to this type when they are decided.
///
/// These eras are loaded from data, requiring a data provider capable of providing [`CalendarJapaneseModernV1`]
/// data.
#[derive(Clone, Debug, Default, Copy)]
pub struct Japanese {
    post_reiwa_era: Option<PackedEra>,
}

impl Japanese {
    /// Creates a new [`Japanese`] using only modern eras (post-meiji) from compiled data.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub const fn new() -> Self {
        const {
            Self {
                post_reiwa_era: crate::provider::Baked::SINGLETON_CALENDAR_JAPANESE_MODERN_V1
                    .last_after_reiwa(),
            }
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
            post_reiwa_era: provider
                .load(Default::default())?
                .payload
                .get()
                .last_after_reiwa(),
        })
    }
}

impl Japanese {
    fn eras(self) -> impl Iterator<Item = (EraStartDate, TinyAsciiStr<8>, u8)> {
        self.post_reiwa_era.map(|p| p.unpack()).into_iter().chain([
            (
                EraStartDate {
                    year: 2019,
                    month: 5,
                    day: 1,
                },
                tinystr!(8, "reiwa"),
                6,
            ),
            (
                EraStartDate {
                    year: 1989,
                    month: 1,
                    day: 8,
                },
                tinystr!(8, "heisei"),
                5,
            ),
            (
                EraStartDate {
                    year: 1926,
                    month: 12,
                    day: 25,
                },
                tinystr!(8, "showa"),
                4,
            ),
            (
                EraStartDate {
                    year: 1912,
                    month: 7,
                    day: 30,
                },
                tinystr!(8, "taisho"),
                3,
            ),
            (
                EraStartDate {
                    year: 1868,
                    month: 10,
                    day: 23,
                },
                tinystr!(8, "meiji"),
                2,
            ),
        ])
    }
}

impl GregorianYears for Japanese {
    fn extended_from_era_year(
        &self,
        era: Option<&[u8]>,
        year: i32,
    ) -> Result<i32, UnknownEraError> {
        if let Ok(g) = CeBce.extended_from_era_year(era, year) {
            return Ok(g);
        }

        let (start, ..) = self
            .eras()
            .find(|(_, name, _)| era == Some(name.as_bytes()))
            .ok_or(UnknownEraError)?;

        Ok(year - 1 + start.year)
    }

    fn era_year_from_extended(&self, year: i32, month: u8, day: u8) -> types::EraYear {
        let date: EraStartDate = EraStartDate { year, month, day };

        let ret = if let Some((start, code, idx)) = self.eras().find(|&(start, ..)| date >= start) {
            types::EraYear {
                era: code.resize(),
                era_index: Some(idx),
                year: year - start.year + 1,
                extended_year: year,
                ambiguity: types::YearAmbiguity::CenturyRequired,
            }
        } else {
            CeBce.era_year_from_extended(year, month, day)
        };

        // Special case: Meiji 1 to 5 are treated as CE.
        //
        // This calendar was adopted in Meiji 6 (1873), but Meiji started
        // on 1868-10-23, so for Meiji 1 - 5 the lunisolar calendar was still
        // in use. In order to not produce incorrect pre-Meiji 6
        // dates, we force the Gregorian calendar there.
        //
        // https://tc39.es/proposal-intl-era-monthcode/#table-calendar-types
        // https://github.com/tc39/proposal-intl-era-monthcode/issues/86
        if ret.era == "meiji" && ret.year < 6 {
            return CeBce.era_year_from_extended(year, month, day);
        }

        ret
    }

    fn debug_name(&self) -> &'static str {
        "Japanese"
    }

    fn calendar_algorithm(&self) -> Option<crate::preferences::CalendarAlgorithm> {
        Some(crate::preferences::CalendarAlgorithm::Japanese)
    }
}

impl_with_abstract_gregorian!(Japanese, JapaneseDateInner, Japanese, this, *this);

impl Date<Japanese> {
    /// Construct a new Japanese [`Date`].
    ///
    /// Years are specified in the era provided, and must be in range for Japanese
    /// eras (e.g. dates past April 30 Heisei 31 must be in Reiwa; "Jun 5 Heisei 31" and "Jan 1 Heisei 32"
    /// will not be adjusted to being in Reiwa 1 and 2 respectively)
    ///
    /// However, dates may always be specified in "bce" or "ce" and they will be adjusted as necessary.
    ///
    /// This function accepts years in the range `-9999..=9999`, where the Gregorian year
    /// is also in the range `-9999..=9999`.
    ///
    /// ```rust
    /// use icu::calendar::cal::Japanese;
    /// use icu::calendar::{Date, Ref};
    /// use tinystr::tinystr;
    ///
    /// let era = "heisei";
    ///
    /// let date =
    ///     Date::try_new_japanese_with_calendar(era, 14, 1, 2, Japanese::new())
    ///         .expect("Constructing a date should succeed");
    ///
    /// assert_eq!(date.era_year().era, era);
    /// assert_eq!(date.era_year().year, 14);
    /// assert_eq!(date.month().ordinal, 1);
    /// assert_eq!(date.day_of_month().0, 2);
    ///
    /// // This function will error for unknown eras
    /// let fake_era = "neko"; // 🐱
    /// let fake_date = Date::try_new_japanese_with_calendar(
    ///     fake_era,
    ///     10,
    ///     1,
    ///     2,
    ///     Japanese::new(),
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
        ArithmeticDate::from_era_year_month_day(
            era,
            year,
            month,
            day,
            &AbstractGregorian(*japanese_calendar.as_calendar()),
        )
        .map(ArithmeticDate::cast)
        .map(JapaneseDateInner)
        .map(|i| Date::from_raw(i, japanese_calendar))
    }
}

impl Date<Japanese> {
    /// Use [`Self::try_new_japanese_with_calendar`]
    #[deprecated(since = "2.2.0", note = "use `try_new_japanese_with_calendar`")]
    pub fn try_new_japanese_extended_with_calendar<A: AsCalendar<Calendar = Japanese>>(
        era: &str,
        year: i32,
        month: u8,
        day: u8,
        japanese_calendar: A,
    ) -> Result<Date<A>, DateError> {
        Self::try_new_japanese_with_calendar(era, year, month, day, japanese_calendar)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const CALENDAR: Japanese = Japanese {
        post_reiwa_era: Some(PackedEra::pack(
            EraStartDate {
                year: 2086,
                month: 11,
                day: 1,
            },
            tinystr!(8, "fuzu"),
            8,
        )),
    };

    fn single_test_roundtrip(era: &str, year: i32, month: u8, day: u8) {
        let date = Date::try_new_japanese_with_calendar(era, year, month, day, CALENDAR)
            .unwrap_or_else(|e| {
                panic!("Failed to construct date with {era:?}, {year}, {month}, {day}: {e:?}")
            });
        let reconstructed = Date::from_rata_die(date.to_rata_die(), CALENDAR);
        assert_eq!(
            date, reconstructed,
            "Failed to roundtrip with {era:?}, {year}, {month}, {day}"
        );

        // Extra coverage for https://github.com/unicode-org/icu4x/issues/4968
        assert_eq!(reconstructed.era_year().era, era);
        assert_eq!(reconstructed.era_year().year, year);
    }

    // test that out-of-range era values convert to other eras
    fn single_test_era_range_roundtrip(
        era: &str,
        year: i32,
        month: u8,
        day: u8,
        era2: &str,
        year2: i32,
    ) {
        let expected = Date::try_new_japanese_with_calendar(era2, year2, month, day, CALENDAR)
            .unwrap_or_else(|e| {
                panic!(
                    "Failed to construct expectation date with {era2:?}, {year2}, {month}, {day}: {e:?}"
                )
            });

        let date = Date::try_new_japanese_with_calendar(era, year, month, day, CALENDAR)
            .unwrap_or_else(|e| {
                panic!("Failed to construct date with {era:?}, {year}, {month}, {day}: {e:?}")
            });
        assert_eq!(
            expected, date,
            "Failed to convert with {era:?}, {year}, {month}, {day}"
        )
    }

    #[test]
    fn test_japanese() {
        single_test_roundtrip("heisei", 12, 3, 1);
        single_test_roundtrip("taisho", 3, 3, 1);
        // Heisei did not start until later in the year
        single_test_era_range_roundtrip("heisei", 1, 1, 1, "showa", 64);

        // handle bce/ce
        single_test_roundtrip("bce", 100, 3, 1);
        single_test_roundtrip("bce", 1, 3, 1);
        single_test_roundtrip("ce", 1, 3, 1);
        single_test_roundtrip("ce", 100, 3, 1);
        single_test_roundtrip("ce", 1000, 3, 1);
        single_test_era_range_roundtrip("ce", 0, 3, 1, "bce", 1);
        single_test_era_range_roundtrip("bce", -1, 3, 1, "ce", 2);

        // check post-reiwa
        single_test_roundtrip("reiwa", 68, 10, 31);
        single_test_era_range_roundtrip("fuzu", 1, 10, 31, "reiwa", 68);
        single_test_roundtrip("fuzu", 1, 11, 1);

        // handle the cases where bce/ce get adjusted to different eras
        // single_test_gregorian_roundtrip("ce", 2021, 3, 1, "reiwa", 3);
        single_test_era_range_roundtrip("ce", 2024, 3, 1, "reiwa", 6);
    }

    #[test]
    /// Pre-Meiji 6 dates are treated as CE, make sure that works right
    fn test_meiji_6_switchover() {
        // Pre-meiji 6 dates specified as CE
        single_test_roundtrip("ce", 1868, 1, 1);
        single_test_roundtrip("ce", 1868, 10, 1);
        single_test_roundtrip("ce", 1868, 10, 23);
        single_test_roundtrip("ce", 1868, 10, 24);
        single_test_roundtrip("ce", 1868, 11, 1);
        single_test_roundtrip("ce", 1869, 1, 1);
        single_test_roundtrip("ce", 1871, 1, 1);

        // Pre-meiji 6 dates specified as Meiji
        single_test_era_range_roundtrip("meiji", 1, 1, 1, "ce", 1868);
        single_test_era_range_roundtrip("meiji", 1, 10, 1, "ce", 1868);
        single_test_era_range_roundtrip("meiji", 1, 10, 23, "ce", 1868);
        single_test_era_range_roundtrip("meiji", 1, 10, 24, "ce", 1868);
        single_test_era_range_roundtrip("meiji", 1, 11, 1, "ce", 1868);
        single_test_era_range_roundtrip("meiji", 1, 11, 1, "ce", 1868);
        single_test_era_range_roundtrip("meiji", 2, 1, 1, "ce", 1869);
        single_test_era_range_roundtrip("meiji", 5, 1, 1, "ce", 1872);

        // Post-Meiji 6 dates specified as Meiji
        single_test_roundtrip("meiji", 6, 1, 1);
        single_test_roundtrip("meiji", 6, 5, 1);
        single_test_roundtrip("meiji", 7, 1, 1);

        // Post-Meiji 6 dates specified as CE
        single_test_era_range_roundtrip("ce", 1873, 1, 1, "meiji", 6);
        single_test_era_range_roundtrip("ce", 1873, 5, 1, "meiji", 6);
        single_test_era_range_roundtrip("ce", 1874, 1, 1, "meiji", 7);
    }
}
