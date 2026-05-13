// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::cal::abstract_gregorian::{impl_with_abstract_gregorian, GregorianYears};
use crate::calendar_arithmetic::ArithmeticDate;
use crate::error::UnknownEraError;
use crate::preferences::CalendarAlgorithm;
use crate::{types, Date, DateError, RangeError};
use tinystr::tinystr;

impl_with_abstract_gregorian!(crate::cal::Gregorian, GregorianDateInner, CeBce, _x, CeBce);

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
    /// Construct a new Gregorian Date.
    ///
    /// Years are specified as ISO years.
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
        ArithmeticDate::new_gregorian::<CeBce>(year, month, day)
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
    use crate::cal::Iso;
    use calendrical_calculations::rata_die::RataDie;

    use super::*;

    #[derive(Debug)]
    struct TestCase {
        rd: RataDie,
        iso_year: i32,
        iso_month: u8,
        iso_day: u8,
        expected_year: i32,
        expected_era: &'static str,
        expected_month: u8,
        expected_day: u8,
    }

    fn check_test_case(case: TestCase) {
        let iso_from_rd = Date::from_rata_die(case.rd, Iso);
        let greg_date_from_rd = Date::from_rata_die(case.rd, Gregorian);
        assert_eq!(greg_date_from_rd.era_year().year, case.expected_year,
            "Failed year check from RD: {case:?}\nISO: {iso_from_rd:?}\nGreg: {greg_date_from_rd:?}");
        assert_eq!(
            greg_date_from_rd.era_year().era,
            case.expected_era,
            "Failed era check from RD: {case:?}\nISO: {iso_from_rd:?}\nGreg: {greg_date_from_rd:?}"
        );
        assert_eq!(greg_date_from_rd.month().ordinal, case.expected_month,
            "Failed month check from RD: {case:?}\nISO: {iso_from_rd:?}\nGreg: {greg_date_from_rd:?}");
        assert_eq!(
            greg_date_from_rd.day_of_month().0,
            case.expected_day,
            "Failed day check from RD: {case:?}\nISO: {iso_from_rd:?}\nGreg: {greg_date_from_rd:?}"
        );

        let iso_date_man: Date<Iso> =
            Date::try_new_iso(case.iso_year, case.iso_month, case.iso_day)
                .expect("Failed to initialize ISO date for {case:?}");
        let greg_date_man: Date<Gregorian> = Date::new_from_iso(iso_date_man, Gregorian);
        assert_eq!(iso_from_rd, iso_date_man,
            "ISO from RD not equal to ISO generated from manually-input ymd\nCase: {case:?}\nRD: {iso_from_rd:?}\nMan: {iso_date_man:?}");
        assert_eq!(greg_date_from_rd, greg_date_man,
            "Greg. date from RD not equal to Greg. generated from manually-input ymd\nCase: {case:?}\nRD: {greg_date_from_rd:?}\nMan: {greg_date_man:?}");
    }

    #[test]
    fn test_gregorian_ce() {
        // Tests that the Gregorian calendar gives the correct expected
        // day, month, and year for positive years (AD/CE/gregory era)

        let cases = [
            TestCase {
                rd: RataDie::new(1),
                iso_year: 1,
                iso_month: 1,
                iso_day: 1,
                expected_year: 1,
                expected_era: "ce",
                expected_month: 1,
                expected_day: 1,
            },
            TestCase {
                rd: RataDie::new(181),
                iso_year: 1,
                iso_month: 6,
                iso_day: 30,
                expected_year: 1,
                expected_era: "ce",
                expected_month: 6,
                expected_day: 30,
            },
            TestCase {
                rd: RataDie::new(1155),
                iso_year: 4,
                iso_month: 2,
                iso_day: 29,
                expected_year: 4,
                expected_era: "ce",
                expected_month: 2,
                expected_day: 29,
            },
            TestCase {
                rd: RataDie::new(1344),
                iso_year: 4,
                iso_month: 9,
                iso_day: 5,
                expected_year: 4,
                expected_era: "ce",
                expected_month: 9,
                expected_day: 5,
            },
            TestCase {
                rd: RataDie::new(36219),
                iso_year: 100,
                iso_month: 3,
                iso_day: 1,
                expected_year: 100,
                expected_era: "ce",
                expected_month: 3,
                expected_day: 1,
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
                iso_year: 0,
                iso_month: 12,
                iso_day: 31,
                expected_year: 1,
                expected_era: "bce",
                expected_month: 12,
                expected_day: 31,
            },
            TestCase {
                rd: RataDie::new(-365), // This is a leap year
                iso_year: 0,
                iso_month: 1,
                iso_day: 1,
                expected_year: 1,
                expected_era: "bce",
                expected_month: 1,
                expected_day: 1,
            },
            TestCase {
                rd: RataDie::new(-366),
                iso_year: -1,
                iso_month: 12,
                iso_day: 31,
                expected_year: 2,
                expected_era: "bce",
                expected_month: 12,
                expected_day: 31,
            },
            TestCase {
                rd: RataDie::new(-1461),
                iso_year: -4,
                iso_month: 12,
                iso_day: 31,
                expected_year: 5,
                expected_era: "bce",
                expected_month: 12,
                expected_day: 31,
            },
            TestCase {
                rd: RataDie::new(-1826),
                iso_year: -4,
                iso_month: 1,
                iso_day: 1,
                expected_year: 5,
                expected_era: "bce",
                expected_month: 1,
                expected_day: 1,
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
                let iso_i = Date::from_rata_die(RataDie::new(i), Iso);
                let iso_j = Date::from_rata_die(RataDie::new(j), Iso);

                let greg_i = iso_i.to_calendar(Gregorian);
                let greg_j = iso_j.to_calendar(Gregorian);

                assert_eq!(
                    i.cmp(&j),
                    iso_i.cmp(&iso_j),
                    "ISO directionality inconsistent with directionality for i: {i}, j: {j}"
                );
                assert_eq!(
                    i.cmp(&j),
                    greg_i.cmp(&greg_j),
                    "Gregorian directionality inconsistent with directionality for i: {i}, j: {j}"
                );
            }
        }
    }
}
