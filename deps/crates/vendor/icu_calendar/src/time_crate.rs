// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::cal::abstract_gregorian::AbstractGregorianYear;
use crate::cal::gregorian::GregorianDateInner;
use crate::calendar_arithmetic::ArithmeticDate;
use crate::Date;
use crate::Gregorian;

impl From<time::Date> for Date<Gregorian> {
    fn from(other: time::Date) -> Self {
        // time stores dates as year + day of year. we work with that instead
        // of using its conversion code
        let (y, day_of_year) = other.to_ordinal_date();
        let (m, d) = calendrical_calculations::gregorian::year_day(y, day_of_year);

        // time's MIN/MAX value are inside VALID_RD_RANGE
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

    let julian_day_epoch = calendrical_calculations::gregorian::fixed_from_gregorian(-4713, 11, 24);

    assert!(VALID_RD_RANGE.contains(&(julian_day_epoch + time::Date::MIN.to_julian_day() as i64)));
    assert!(VALID_RD_RANGE.contains(&(julian_day_epoch + time::Date::MAX.to_julian_day() as i64)));
}

impl From<time::Weekday> for crate::types::Weekday {
    fn from(value: time::Weekday) -> Self {
        match value {
            time::Weekday::Monday => Self::Monday,
            time::Weekday::Tuesday => Self::Tuesday,
            time::Weekday::Wednesday => Self::Wednesday,
            time::Weekday::Thursday => Self::Thursday,
            time::Weekday::Friday => Self::Friday,
            time::Weekday::Saturday => Self::Saturday,
            time::Weekday::Sunday => Self::Sunday,
        }
    }
}
