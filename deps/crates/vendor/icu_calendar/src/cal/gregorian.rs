// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::cal::abstract_gregorian::{
    impl_with_abstract_gregorian, AbstractGregorian, GregorianYears,
};
use crate::calendar_arithmetic::ArithmeticDate;
use crate::error::UnknownEraError;
use crate::preferences::CalendarAlgorithm;
use crate::{types, Date, RangeError};
use tinystr::tinystr;

impl_with_abstract_gregorian!(Gregorian, GregorianDateInner, CeBce, _x, CeBce);

#[derive(Copy, Clone, Debug, Default, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub(crate) struct CeBce;

impl GregorianYears for CeBce {
    fn extended_from_era_year(
        &self,
        era: Option<&[u8]>,
        year: i32,
    ) -> Result<i32, UnknownEraError> {
        match era {
            None => Ok(year),
            Some(b"ad" | b"ce") => Ok(year),
            Some(b"bce" | b"bc") => Ok(1 - year),
            Some(_) => Err(UnknownEraError),
        }
    }

    fn era_year_from_extended(&self, extended_year: i32, _month: u8, _day: u8) -> types::EraYear {
        if extended_year > 0 {
            types::EraYear {
                era: tinystr!(16, "ce"),
                era_index: Some(1),
                year: extended_year,
                extended_year,
                ambiguity: match extended_year {
                    ..=999 => types::YearAmbiguity::EraAndCenturyRequired,
                    1000..=1949 => types::YearAmbiguity::CenturyRequired,
                    1950..=2049 => types::YearAmbiguity::Unambiguous,
                    2050.. => types::YearAmbiguity::CenturyRequired,
                },
            }
        } else {
            types::EraYear {
                era: tinystr!(16, "bce"),
                era_index: Some(0),
                year: 1 - extended_year,
                extended_year,
                ambiguity: types::YearAmbiguity::EraAndCenturyRequired,
            }
        }
    }

    fn debug_name(&self) -> &'static str {
        "Gregorian"
    }

    fn calendar_algorithm(&self) -> Option<CalendarAlgorithm> {
        Some(CalendarAlgorithm::Gregory)
    }
}

/// The [Gregorian Calendar](https://en.wikipedia.org/wiki/Gregorian_calendar)
///
/// The Gregorian calendar is an improvement over the [`Julian`](super::Julian) calendar.
/// It was adopted under Pope Gregory XIII in 1582 CE by much of Roman Catholic Europe,
/// and over the following centuries by all other countries that had been using
/// the Julian calendar. Eventually even countries that had been using other calendars
/// adopted this calendar, and today it is used as the international civil calendar.
///
/// This implementation extends proleptically for dates before the calendar's creation.
///
/// This corresponds to the `"gregory"` [CLDR calendar](https://unicode.org/reports/tr35/#UnicodeCalendarIdentifier).
///
/// # Era codes
///
/// This calendar uses two era codes: `bce` (alias `bc`), and `ce` (alias `ad`), corresponding to the BCE and CE eras.
///
/// # Months and days
///
/// The 12 months are called January (`M01`, 31 days), February (`M02`, 28 days),
/// March (`M03`, 31 days), April (`M04`, 30 days), May (`M05`, 31 days), June (`M06`, 30 days),
/// July (`M07`, 31 days), August (`M08`, 31 days), September (`M09`, 30 days),
/// October (`M10`, 31 days), November (`M11`, 30 days), December (`M12`, 31 days).
///
/// In leap years (years divisible by 4 but not 100 except when divisble by 400), February gains a 29th day.
///
/// Standard years thus have 365 days, and leap years 366.
///
/// # Calendar drift
///
/// The Gregorian calendar has an average year length of 365.2425, slightly longer than
/// the mean solar year, so this calendar drifts 1 day in ~7700 years with respect
/// to the seasons.
///
/// # Historical accuracy
///
/// This type implements the [*proleptic* Gregorian calendar](
/// https://en.wikipedia.org/wiki/Gregorian_calendar#Proleptic_Gregorian_calendar),
/// with dates before 1582 CE using the rules projected backwards. Care needs to be taken
/// when intepreting historical dates before or during the transition from the Julian to
/// the Gregorian calendar. [Some regions](https://en.wikipedia.org/wiki/Adoption_of_the_Gregorian_calendar)
/// continued using the Julian calendar as late as the 20th century. Sources often
/// mark dates as "New Style" (Gregorian) or "Old Style" (Julian) if there is ambiguity.
///
/// Historically, the Julian/Gregorian calendars were used with a variety of year reckoning
/// schemes (see [`Julian`](super::Julian) for more detail). The Gregorian calendar has generally
/// been used with the [Anno Domini](https://en.wikipedia.org/wiki/Anno_Domini)/[Common era](
/// https://en.wikipedia.org/wiki/Common_Era) since its inception. However, some countries
/// that have adopted the Gregorian calendar more recently are still using their traditional
/// year-reckoning schemes. This crate implements some of these as different types, i.e the Thai
/// [`Buddhist`](super::Buddhist) calendar, the [`Japanese`](super::Japanese) calendar, and the
/// Chinese Republican Calendar ([`Roc`](super::Roc)).
#[derive(Copy, Clone, Debug, Default)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct Gregorian;

impl Date<Gregorian> {
    /// Construct a new Gregorian [`Date`].
    ///
    /// Years are arithmetic, meaning there is a year 0 preceded by negative years, with a
    /// valid range of `-9999..=9999`.
    ///
    /// ```rust
    /// use icu::calendar::Date;
    ///
    /// // Conversion from ISO to Gregorian
    /// let date_gregorian = Date::try_new_gregorian(1970, 1, 2)
    ///     .expect("Failed to initialize Gregorian Date instance.");
    ///
    /// assert_eq!(date_gregorian.era_year().year, 1970);
    /// assert_eq!(date_gregorian.month().ordinal, 1);
    /// assert_eq!(date_gregorian.day_of_month().0, 2);
    /// ```
    pub fn try_new_gregorian(year: i32, month: u8, day: u8) -> Result<Date<Gregorian>, RangeError> {
        ArithmeticDate::from_year_month_day(year, month, day, &AbstractGregorian(CeBce))
            .map(ArithmeticDate::cast)
            .map(GregorianDateInner)
            .map(|i| Date::from_raw(i, Gregorian))
    }
}

impl Gregorian {
    /// Returns the date of Easter in the given year.
    pub fn easter(year: i32) -> Date<Self> {
        Date::from_rata_die(calendrical_calculations::gregorian::easter(year), Self)
    }
}

#[cfg(test)]
mod test {
    use calendrical_calculations::rata_die::RataDie;

    use super::*;

    #[derive(Debug)]
    struct TestCase {
        rd: RataDie,
        extended_year: i32,
        month: u8,
        day: u8,
        era_year: i32,
        era: &'static str,
    }

    fn check_test_case(case: TestCase) {
        let date = Date::from_rata_die(case.rd, Gregorian);

        assert_eq!(date.to_rata_die(), case.rd, "{case:?}");

        assert_eq!(date.era_year().year, case.era_year, "{case:?}");
        assert_eq!(
            date.era_year().extended_year,
            case.extended_year,
            "{case:?}"
        );
        assert_eq!(date.era_year().era, case.era, "{case:?}");
        assert_eq!(date.month().ordinal, case.month, "{case:?}");
        assert_eq!(date.day_of_month().0, case.day, "{case:?}");

        assert_eq!(
            Date::try_new_gregorian(
                date.era_year().extended_year,
                date.month().ordinal,
                date.day_of_month().0
            ),
            Ok(date),
            "{case:?}"
        );
    }

    #[test]
    fn test_gregorian_ce() {
        // Tests that the Gregorian calendar gives the correct expected
        // day, month, and year for positive years (AD/CE/gregory era)

        let cases = [
            TestCase {
                rd: RataDie::new(1),
                extended_year: 1,
                month: 1,
                day: 1,
                era_year: 1,
                era: "ce",
            },
            TestCase {
                rd: RataDie::new(181),
                extended_year: 1,
                month: 6,
                day: 30,
                era_year: 1,
                era: "ce",
            },
            TestCase {
                rd: RataDie::new(1155),
                extended_year: 4,
                month: 2,
                day: 29,
                era_year: 4,
                era: "ce",
            },
            TestCase {
                rd: RataDie::new(1344),
                extended_year: 4,
                month: 9,
                day: 5,
                era_year: 4,
                era: "ce",
            },
            TestCase {
                rd: RataDie::new(36219),
                extended_year: 100,
                month: 3,
                day: 1,
                era_year: 100,
                era: "ce",
            },
        ];

        for case in cases {
            check_test_case(case);
        }
    }

    #[test]
    fn test_gregorian_bce() {
        // Tests that the Gregorian calendar gives the correct expected
        // day, month, and year for negative years (BC/BCE era)

        let cases = [
            TestCase {
                rd: RataDie::new(0),
                extended_year: 0,
                month: 12,
                day: 31,
                era_year: 1,
                era: "bce",
            },
            TestCase {
                rd: RataDie::new(-365), // This is a leap year
                extended_year: 0,
                month: 1,
                day: 1,
                era_year: 1,
                era: "bce",
            },
            TestCase {
                rd: RataDie::new(-366),
                extended_year: -1,
                month: 12,
                day: 31,
                era_year: 2,
                era: "bce",
            },
            TestCase {
                rd: RataDie::new(-1461),
                extended_year: -4,
                month: 12,
                day: 31,
                era_year: 5,
                era: "bce",
            },
            TestCase {
                rd: RataDie::new(-1826),
                extended_year: -4,
                month: 1,
                day: 1,
                era_year: 5,
                era: "bce",
            },
        ];

        for case in cases {
            check_test_case(case);
        }
    }

    #[test]
    fn check_gregorian_directionality() {
        // Tests that for a large range of RDs, if a RD
        // is less than another, the corresponding YMD should also be less
        // than the other, without exception.
        for i in -100..100 {
            for j in -100..100 {
                let greg_i = Date::from_rata_die(RataDie::new(i), Gregorian);
                let greg_j = Date::from_rata_die(RataDie::new(j), Gregorian);

                assert_eq!(
                    i.cmp(&j),
                    greg_i.cmp(&greg_j),
                    "Gregorian directionality inconsistent with directionality for i: {i}, j: {j}"
                );
            }
        }
    }
}
