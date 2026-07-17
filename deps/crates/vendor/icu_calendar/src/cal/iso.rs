// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::cal::abstract_gregorian::{impl_with_abstract_gregorian, GregorianYears};
use crate::calendar_arithmetic::ArithmeticDate;
use crate::error::UnknownEraError;
use crate::{types, Date, DateError, RangeError};
use tinystr::tinystr;

/// The [ISO-8601 Calendar](https://en.wikipedia.org/wiki/ISO_8601#Dates)
///
/// This calendar is identical to the [`Gregorian`](super::Gregorian) calendar,
/// except that it uses a single `default` era instead of `bce` and `ce`.
///
/// This corresponds to the `"iso8601"` [CLDR calendar](https://unicode.org/reports/tr35/#UnicodeCalendarIdentifier).
///
/// # Era codes
///
/// This calendar uses a single era: `default`
#[derive(Copy, Clone, Debug, Default, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct Iso;

impl_with_abstract_gregorian!(crate::cal::Iso, IsoDateInner, IsoEra, _x, IsoEra);

#[derive(Copy, Clone, Debug, Default, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub(crate) struct IsoEra;

impl GregorianYears for IsoEra {
    fn extended_from_era_year(
        &self,
        era: Option<&[u8]>,
        year: i32,
    ) -> Result<i32, UnknownEraError> {
        match era {
            Some(b"default") | None => Ok(year),
            Some(_) => Err(UnknownEraError),
        }
    }

    fn era_year_from_extended(&self, extended_year: i32, _month: u8, _day: u8) -> types::EraYear {
        types::EraYear {
            era_index: Some(0),
            era: tinystr!(16, "default"),
            year: extended_year,
            extended_year,
            ambiguity: types::YearAmbiguity::Unambiguous,
        }
    }

    fn debug_name(&self) -> &'static str {
        "ISO"
    }
}

impl Date<Iso> {
    /// Construct a new ISO date from integers.
    ///
    /// ```rust
    /// use icu::calendar::Date;
    ///
    /// let date_iso = Date::try_new_iso(1970, 1, 2)
    ///     .expect("Failed to initialize ISO Date instance.");
    ///
    /// assert_eq!(date_iso.era_year().year, 1970);
    /// assert_eq!(date_iso.month().ordinal, 1);
    /// assert_eq!(date_iso.day_of_month().0, 2);
    /// ```
    pub fn try_new_iso(year: i32, month: u8, day: u8) -> Result<Date<Iso>, RangeError> {
        ArithmeticDate::new_gregorian::<IsoEra>(year, month, day)
            .map(IsoDateInner)
            .map(|i| Date::from_raw(i, Iso))
    }
}

impl Iso {
    /// Construct a new ISO Calendar
    pub fn new() -> Self {
        Self
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::types::{DateDuration, RataDie, Weekday};
    use crate::Calendar;

    #[test]
    fn iso_overflow() {
        #[derive(Debug)]
        struct TestCase {
            year: i32,
            month: u8,
            day: u8,
            rd: RataDie,
            saturating: bool,
        }
        // Calculates the max possible year representable using i32::MAX as the RD
        let max_year = Iso.from_rata_die(RataDie::new(i32::MAX as i64)).0.year;

        // Calculates the minimum possible year representable using i32::MIN as the RD
        // *Cannot be tested yet due to hard coded date not being available yet (see line 436)
        let min_year = -5879610;

        let cases = [
            TestCase {
                // Earliest date that can be represented before causing a minimum overflow
                year: min_year,
                month: 6,
                day: 22,
                rd: RataDie::new(i32::MIN as i64),
                saturating: false,
            },
            TestCase {
                year: min_year,
                month: 6,
                day: 23,
                rd: RataDie::new(i32::MIN as i64 + 1),
                saturating: false,
            },
            TestCase {
                year: min_year,
                month: 6,
                day: 21,
                rd: RataDie::new(i32::MIN as i64 - 1),
                saturating: false,
            },
            TestCase {
                year: min_year,
                month: 12,
                day: 31,
                rd: RataDie::new(-2147483456),
                saturating: false,
            },
            TestCase {
                year: min_year + 1,
                month: 1,
                day: 1,
                rd: RataDie::new(-2147483455),
                saturating: false,
            },
            TestCase {
                year: max_year,
                month: 6,
                day: 11,
                rd: RataDie::new(i32::MAX as i64 - 30),
                saturating: false,
            },
            TestCase {
                year: max_year,
                month: 7,
                day: 9,
                rd: RataDie::new(i32::MAX as i64 - 2),
                saturating: false,
            },
            TestCase {
                year: max_year,
                month: 7,
                day: 10,
                rd: RataDie::new(i32::MAX as i64 - 1),
                saturating: false,
            },
            TestCase {
                // Latest date that can be represented before causing a maximum overflow
                year: max_year,
                month: 7,
                day: 11,
                rd: RataDie::new(i32::MAX as i64),
                saturating: false,
            },
            TestCase {
                year: max_year,
                month: 7,
                day: 12,
                rd: RataDie::new(i32::MAX as i64 + 1),
                saturating: false,
            },
            TestCase {
                year: i32::MIN,
                month: 1,
                day: 2,
                rd: RataDie::new(-784352296669),
                saturating: false,
            },
            TestCase {
                year: i32::MIN,
                month: 1,
                day: 1,
                rd: RataDie::new(-784352296670),
                saturating: false,
            },
            TestCase {
                year: i32::MIN,
                month: 1,
                day: 1,
                rd: RataDie::new(-784352296671),
                saturating: true,
            },
            TestCase {
                year: i32::MAX,
                month: 12,
                day: 30,
                rd: RataDie::new(784352295938),
                saturating: false,
            },
            TestCase {
                year: i32::MAX,
                month: 12,
                day: 31,
                rd: RataDie::new(784352295939),
                saturating: false,
            },
            TestCase {
                year: i32::MAX,
                month: 12,
                day: 31,
                rd: RataDie::new(784352295940),
                saturating: true,
            },
        ];

        for case in cases {
            let date = Date::try_new_iso(case.year, case.month, case.day).unwrap();
            if !case.saturating {
                assert_eq!(date.to_rata_die(), case.rd, "{case:?}");
            }
            assert_eq!(Date::from_rata_die(case.rd, Iso), date, "{case:?}");
        }
    }

    // Calculates the minimum possible year representable using a large negative fixed date
    #[test]
    fn min_year() {
        assert_eq!(
            Date::from_rata_die(RataDie::big_negative(), Iso)
                .year()
                .era()
                .unwrap()
                .year,
            i32::MIN
        );
    }

    #[test]
    fn test_day_of_week() {
        // June 23, 2021 is a Wednesday
        assert_eq!(
            Date::try_new_iso(2021, 6, 23).unwrap().day_of_week(),
            Weekday::Wednesday,
        );
        // Feb 2, 1983 was a Wednesday
        assert_eq!(
            Date::try_new_iso(1983, 2, 2).unwrap().day_of_week(),
            Weekday::Wednesday,
        );
        // Jan 21, 2021 was a Tuesday
        assert_eq!(
            Date::try_new_iso(2020, 1, 21).unwrap().day_of_week(),
            Weekday::Tuesday,
        );
    }

    #[test]
    fn test_day_of_year() {
        // June 23, 2021 was day 174
        assert_eq!(Date::try_new_iso(2021, 6, 23).unwrap().day_of_year().0, 174,);
        // June 23, 2020 was day 175
        assert_eq!(Date::try_new_iso(2020, 6, 23).unwrap().day_of_year().0, 175,);
        // Feb 2, 1983 was a Wednesday
        assert_eq!(Date::try_new_iso(1983, 2, 2).unwrap().day_of_year().0, 33,);
    }

    #[test]
    fn test_offset() {
        let today = Date::try_new_iso(2021, 6, 23).unwrap();
        let today_plus_5000 = Date::try_new_iso(2035, 3, 2).unwrap();
        let offset = today
            .try_added_with_options(DateDuration::for_days(5000), Default::default())
            .unwrap();
        assert_eq!(offset, today_plus_5000);

        let today = Date::try_new_iso(2021, 6, 23).unwrap();
        let today_minus_5000 = Date::try_new_iso(2007, 10, 15).unwrap();
        let offset = today
            .try_added_with_options(DateDuration::for_days(-5000), Default::default())
            .unwrap();
        assert_eq!(offset, today_minus_5000);
    }

    #[test]
    fn test_offset_at_month_boundary() {
        let today = Date::try_new_iso(2020, 2, 28).unwrap();
        let today_plus_2 = Date::try_new_iso(2020, 3, 1).unwrap();
        let offset = today
            .try_added_with_options(DateDuration::for_days(2), Default::default())
            .unwrap();
        assert_eq!(offset, today_plus_2);

        let today = Date::try_new_iso(2020, 2, 28).unwrap();
        let today_plus_3 = Date::try_new_iso(2020, 3, 2).unwrap();
        let offset = today
            .try_added_with_options(DateDuration::for_days(3), Default::default())
            .unwrap();
        assert_eq!(offset, today_plus_3);

        let today = Date::try_new_iso(2020, 2, 28).unwrap();
        let today_plus_1 = Date::try_new_iso(2020, 2, 29).unwrap();
        let offset = today
            .try_added_with_options(DateDuration::for_days(1), Default::default())
            .unwrap();
        assert_eq!(offset, today_plus_1);

        let today = Date::try_new_iso(2019, 2, 28).unwrap();
        let today_plus_2 = Date::try_new_iso(2019, 3, 2).unwrap();
        let offset = today
            .try_added_with_options(DateDuration::for_days(2), Default::default())
            .unwrap();
        assert_eq!(offset, today_plus_2);

        let today = Date::try_new_iso(2019, 2, 28).unwrap();
        let today_plus_1 = Date::try_new_iso(2019, 3, 1).unwrap();
        let offset = today
            .try_added_with_options(DateDuration::for_days(1), Default::default())
            .unwrap();
        assert_eq!(offset, today_plus_1);

        let today = Date::try_new_iso(2020, 3, 1).unwrap();
        let today_minus_1 = Date::try_new_iso(2020, 2, 29).unwrap();
        let offset = today
            .try_added_with_options(DateDuration::for_days(-1), Default::default())
            .unwrap();
        assert_eq!(offset, today_minus_1);
    }

    #[test]
    fn test_offset_handles_negative_month_offset() {
        let today = Date::try_new_iso(2020, 3, 1).unwrap();
        let today_minus_2_months = Date::try_new_iso(2020, 1, 1).unwrap();
        let offset = today
            .try_added_with_options(DateDuration::for_months(-2), Default::default())
            .unwrap();
        assert_eq!(offset, today_minus_2_months);

        let today = Date::try_new_iso(2020, 3, 1).unwrap();
        let today_minus_4_months = Date::try_new_iso(2019, 11, 1).unwrap();
        let offset = today
            .try_added_with_options(DateDuration::for_months(-4), Default::default())
            .unwrap();
        assert_eq!(offset, today_minus_4_months);

        let today = Date::try_new_iso(2020, 3, 1).unwrap();
        let today_minus_24_months = Date::try_new_iso(2018, 3, 1).unwrap();
        let offset = today
            .try_added_with_options(DateDuration::for_months(-24), Default::default())
            .unwrap();
        assert_eq!(offset, today_minus_24_months);

        let today = Date::try_new_iso(2020, 3, 1).unwrap();
        let today_minus_27_months = Date::try_new_iso(2017, 12, 1).unwrap();
        let offset = today
            .try_added_with_options(DateDuration::for_months(-27), Default::default())
            .unwrap();
        assert_eq!(offset, today_minus_27_months);
    }

    #[test]
    fn test_offset_handles_out_of_bound_month_offset() {
        let today = Date::try_new_iso(2021, 1, 31).unwrap();
        // since 2021/02/31 isn't a valid date, `offset_date` auto-adjusts by constraining to the last day in February
        let today_plus_1_month = Date::try_new_iso(2021, 2, 28).unwrap();
        let offset = today
            .try_added_with_options(DateDuration::for_months(1), Default::default())
            .unwrap();
        assert_eq!(offset, today_plus_1_month);

        let today = Date::try_new_iso(2021, 1, 31).unwrap();
        // since 2021/02/31 isn't a valid date, `offset_date` auto-adjusts by constraining to the last day in February
        // and then adding the days
        let today_plus_1_month_1_day = Date::try_new_iso(2021, 3, 1).unwrap();
        let offset = today
            .try_added_with_options(
                DateDuration {
                    months: 1,
                    days: 1,
                    ..Default::default()
                },
                Default::default(),
            )
            .unwrap();
        assert_eq!(offset, today_plus_1_month_1_day);
    }

    #[test]
    fn test_iso_to_from_rd() {
        // Reminder: ISO year 0 is Gregorian year 1 BCE.
        // Year 0 is a leap year due to the 400-year rule.
        fn check(rd: i64, year: i32, month: u8, day: u8) {
            let rd = RataDie::new(rd);

            assert_eq!(
                Date::from_rata_die(rd, Iso),
                Date::try_new_iso(year, month, day).unwrap(),
                "RD: {rd:?}"
            );
        }
        check(-1828, -5, 12, 30);
        check(-1827, -5, 12, 31); // leap year
        check(-1826, -4, 1, 1);
        check(-1462, -4, 12, 30);
        check(-1461, -4, 12, 31);
        check(-1460, -3, 1, 1);
        check(-1459, -3, 1, 2);
        check(-732, -2, 12, 30);
        check(-731, -2, 12, 31);
        check(-730, -1, 1, 1);
        check(-367, -1, 12, 30);
        check(-366, -1, 12, 31);
        check(-365, 0, 1, 1); // leap year
        check(-364, 0, 1, 2);
        check(-1, 0, 12, 30);
        check(0, 0, 12, 31);
        check(1, 1, 1, 1);
        check(2, 1, 1, 2);
        check(364, 1, 12, 30);
        check(365, 1, 12, 31);
        check(366, 2, 1, 1);
        check(1459, 4, 12, 29);
        check(1460, 4, 12, 30);
        check(1461, 4, 12, 31); // leap year
        check(1462, 5, 1, 1);
    }
}
