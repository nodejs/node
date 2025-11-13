//! Utiltiy functions

pub const SECONDS_PER_DAY: i64 = 24 * 60 * 60;

pub(crate) fn mathematical_days_in_year(y: i32) -> i32 {
    if y % 4 != 0 {
        365
    } else if y % 4 == 0 && y % 100 != 0 {
        366
    } else if y % 100 == 0 && y % 400 != 0 {
        365
    } else {
        // Assert that y is divisble by 400 to ensure we are returning the correct result.
        assert_eq!(y % 400, 0);
        366
    }
}

pub(crate) fn num_leap_days(year: i32) -> i32 {
    mathematical_days_in_year(year) - 365
}

pub(crate) fn month_to_day(m: u8, leap_day: i32) -> i32 {
    match m {
        1 => 0,
        2 => 31,
        3 => 59 + leap_day,
        4 => 90 + leap_day,
        5 => 120 + leap_day,
        6 => 151 + leap_day,
        7 => 181 + leap_day,
        8 => 212 + leap_day,
        9 => 243 + leap_day,
        10 => 273 + leap_day,
        11 => 304 + leap_day,
        12 => 334 + leap_day,
        // Sort of hack to return end of month days
        13 => 365 + leap_day,
        _ => unreachable!(),
    }
}

pub(crate) fn epoch_days_for_year(y: i32) -> i32 {
    365 * (y - 1970) + (y - 1969).div_euclid(4) - (y - 1901).div_euclid(100)
        + (y - 1601).div_euclid(400)
}

pub(crate) fn epoch_seconds_for_epoch_days(epoch_days: i32) -> i64 {
    epoch_days as i64 * SECONDS_PER_DAY
}

// The weekday number for 1970-1-1
const EPOCH_DAY_OF_WEEK_CONSTANT: i32 = 4;

pub(crate) fn epoch_days_to_week_day(epoch_days: i32) -> u8 {
    // Wrapping here will be off by 1. (Sunday = 7; Mon = 1)
    (epoch_days + EPOCH_DAY_OF_WEEK_CONSTANT).rem_euclid(7) as u8
}
