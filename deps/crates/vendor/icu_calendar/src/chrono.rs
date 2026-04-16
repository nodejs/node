// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::cal::abstract_gregorian::AbstractGregorianYear;
use crate::cal::gregorian::GregorianDateInner;
use crate::calendar_arithmetic::ArithmeticDate;
use crate::Date;
use crate::Gregorian;

impl From<chrono::NaiveDate> for Date<Gregorian> {
    fn from(chrono: chrono::NaiveDate) -> Self {
        use chrono::Datelike;

        // chrono stores dates as year + day of year. we work with that instead
        // of using its conversion code
        let y = chrono.year();
        let (m, d) = calendrical_calculations::gregorian::year_day(y, chrono.ordinal() as u16);

        // chrono's MIN/MAX value are inside VALID_RD_RANGE
        Self::from_raw(
            GregorianDateInner(ArithmeticDate::new_unchecked(
                AbstractGregorianYear::from_iso_year(y),
                m,
                d,
            )),
            Gregorian,
        )
    }
}

#[test]
fn assert_range() {
    use crate::calendar_arithmetic::VALID_RD_RANGE;
    use crate::types::RataDie;
    use chrono::Datelike;

    assert!(VALID_RD_RANGE
        .contains(&(RataDie::new(0) + chrono::NaiveDate::MIN.num_days_from_ce() as i64)));
    assert!(VALID_RD_RANGE
        .contains(&(RataDie::new(0) + chrono::NaiveDate::MAX.num_days_from_ce() as i64)));
}

impl From<chrono::Weekday> for crate::types::Weekday {
    fn from(value: chrono::Weekday) -> Self {
        match value {
            chrono::Weekday::Mon => Self::Monday,
            chrono::Weekday::Tue => Self::Tuesday,
            chrono::Weekday::Wed => Self::Wednesday,
            chrono::Weekday::Thu => Self::Thursday,
            chrono::Weekday::Fri => Self::Friday,
            chrono::Weekday::Sat => Self::Saturday,
            chrono::Weekday::Sun => Self::Sunday,
        }
    }
}
