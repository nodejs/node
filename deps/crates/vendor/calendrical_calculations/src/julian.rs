// This file is part of ICU4X.
//
// The contents of this file implement algorithms from Calendrical Calculations
// by Reingold & Dershowitz, Cambridge University Press, 4th edition (2018),
// which have been released as Lisp code at <https://github.com/EdReingold/calendar-code2/>
// under the Apache-2.0 license. Accordingly, this file is released under
// the Apache License, Version 2.0 which can be found at the calendrical_calculations
// package root or at http://www.apache.org/licenses/LICENSE-2.0.

use crate::helpers::{i64_to_i32, k_day_after, I32CastError};
use crate::rata_die::RataDie;

// 1st Jan of 1st year proleptic Julian is equivalent to December 30th of 0th year proleptic Gregorian
const JULIAN_EPOCH: RataDie = crate::gregorian::fixed_from_gregorian(0, 12, 30);

/// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/1ee51ecfaae6f856b0d7de3e36e9042100b4f424/calendar.l#L1684-L1687>
#[inline(always)]
pub const fn is_leap_year(year: i32) -> bool {
    year % 4 == 0
}

/// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/1ee51ecfaae6f856b0d7de3e36e9042100b4f424/calendar.l#L1689-L1709>
pub const fn fixed_from_julian(year: i32, month: u8, day: u8) -> RataDie {
    let mut fixed =
        JULIAN_EPOCH.to_i64_date() - 1 + 365 * (year as i64 - 1) + (year as i64 - 1).div_euclid(4);
    debug_assert!(month > 0 && month < 13, "Month should be in range 1..=12.");
    fixed += match month {
        1 => 0,
        2 => 31,
        3 => 59,
        4 => 90,
        5 => 120,
        6 => 151,
        7 => 181,
        8 => 212,
        9 => 243,
        10 => 273,
        11 => 304,
        12 => 334,
        _ => -1,
    };
    // Only add one if the month is after February (month > 2), since leap days are added to the end of February
    if month > 2 && is_leap_year(year) {
        fixed += 1;
    }
    RataDie::new(fixed + (day as i64))
}

/// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/1ee51ecfaae6f856b0d7de3e36e9042100b4f424/calendar.l#L1711-L1738>
pub fn julian_from_fixed(date: RataDie) -> Result<(i32, u8, u8), I32CastError> {
    let approx = (4 * date.to_i64_date() + 1464).div_euclid(1461);
    let year = i64_to_i32(approx)?;
    let prior_days = date
        - fixed_from_julian(year, 1, 1)
        - if is_leap_year(year) && date > fixed_from_julian(year, 2, 28) {
            1
        } else {
            0
        };
    let adjusted_year = if prior_days >= 365 {
        year.saturating_add(1)
    } else {
        year
    };
    let adjusted_prior_days = prior_days.rem_euclid(365);
    debug_assert!((0..365).contains(&adjusted_prior_days));
    let month = if adjusted_prior_days < 31 {
        1
    } else if adjusted_prior_days < 59 {
        2
    } else if adjusted_prior_days < 90 {
        3
    } else if adjusted_prior_days < 120 {
        4
    } else if adjusted_prior_days < 151 {
        5
    } else if adjusted_prior_days < 181 {
        6
    } else if adjusted_prior_days < 212 {
        7
    } else if adjusted_prior_days < 243 {
        8
    } else if adjusted_prior_days < 273 {
        9
    } else if adjusted_prior_days < 304 {
        10
    } else if adjusted_prior_days < 334 {
        11
    } else {
        12
    };
    let day = (date - fixed_from_julian(adjusted_year, month, 1) + 1) as u8; // as days_in_month is < u8::MAX
    debug_assert!(day <= 31, "Day assertion failed; date: {date:?}, adjusted_year: {adjusted_year}, prior_days: {prior_days}, month: {month}, day: {day}");

    Ok((adjusted_year, month, day))
}

/// Get a fixed date from the ymd of a Julian date.
///
/// Years are counted as in _Calendrical Calculations_ by Reingold & Dershowitz,
/// meaning there is no year 0. For instance, near the epoch date, years are counted: -3, -2, -1, 1, 2, 3 instead of -2, -1, 0, 1, 2, 3.
///
/// Primarily useful for use with code constructing epochs specified in the bookg
pub const fn fixed_from_julian_book_version(book_year: i32, month: u8, day: u8) -> RataDie {
    debug_assert!(book_year != 0);
    // TODO: Should we check the bounds here?
    fixed_from_julian(
        if book_year < 0 {
            book_year + 1
        } else {
            book_year
        },
        month,
        day,
    )
}

/// Calculates the date of Easter in the given year
pub fn easter(year: i32) -> RataDie {
    let shifted_epact = (14 + 11 * year.rem_euclid(19)) % 30;
    let paschal_moon = fixed_from_julian(year, 4, 19) - shifted_epact as i64;
    k_day_after(0, paschal_moon)
}

#[test]
fn test_easter() {
    // https://en.wikipedia.org/wiki/List_of_dates_for_Easter, dates in Gregorian
    for (y, m, d) in [
        (2021, 5, 2),
        (2022, 4, 24),
        (2023, 4, 16),
        (2024, 5, 5),
        (2025, 4, 20),
        (2026, 4, 12),
        (2027, 5, 2),
        (2028, 4, 16),
        (2029, 4, 8),
    ] {
        assert_eq!(easter(y), crate::gregorian::fixed_from_gregorian(y, m, d));
    }
}
