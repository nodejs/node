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

const DAYS_IN_YEAR: i64 = 365;

// One leap year every 4 years
const DAYS_IN_4_YEAR_CYCLE: i64 = DAYS_IN_YEAR * 4 + 1;

/// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/1ee51ecfaae6f856b0d7de3e36e9042100b4f424/calendar.l#L1684-L1687>
#[inline(always)]
pub const fn is_leap_year(year: i32) -> bool {
    year % 4 == 0
}

/// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/1ee51ecfaae6f856b0d7de3e36e9042100b4f424/calendar.l#L1689-L1709>
pub const fn fixed_from_julian(year: i32, month: u8, day: u8) -> RataDie {
    day_before_year(year)
        .add(days_before_month(year, month) as i64)
        .add(day as i64)
}

/// The number of days in this year before this month starts
///
/// Inspired by Neri-Schneider <https://onlinelibrary.wiley.com/doi/10.1002/spe.3172>
pub const fn days_before_month(year: i32, month: u8) -> u16 {
    if month < 3 {
        // This compiles to a conditional move, so there's only one branch in this function
        if month == 1 {
            0
        } else {
            31
        }
    } else {
        31 + 28 + is_leap_year(year) as u16 + ((979 * (month as u32) - 2919) >> 5) as u16
    }
}

/// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/1ee51ecfaae6f856b0d7de3e36e9042100b4f424/calendar.l#L1191-L1217>
const fn year_from_fixed(date: RataDie) -> Result<i32, I32CastError> {
    // Shouldn't overflow because it's not possbile to construct extreme values of RataDie
    let date = date.since(JULIAN_EPOCH);

    let (n_4, date) = (
        date.div_euclid(DAYS_IN_4_YEAR_CYCLE),
        date.rem_euclid(DAYS_IN_4_YEAR_CYCLE),
    );

    let n_1 = date / DAYS_IN_YEAR;

    let year = 4 * n_4 + n_1 + (n_1 != 4) as i64;

    i64_to_i32(year)
}

/// Calculates the day before Jan 1 of `year`.
pub const fn day_before_year(year: i32) -> RataDie {
    let prev_year = (year as i64) - 1;
    // Calculate days per year
    let mut fixed: i64 = DAYS_IN_YEAR * prev_year;
    // Adjust for leap year logic. We can avoid the branch of div_euclid by making prev_year positive:
    // YEAR_SHIFT is larger (in magnitude) than any prev_year, and, being divisible by 4,
    // distributes correctly over the calculation on the next line.
    const YEAR_SHIFT: i64 = (-(i32::MIN as i64 - 1) / 4 + 1) * 4;
    fixed += (prev_year + YEAR_SHIFT) / 4 - const { YEAR_SHIFT / 4 };
    JULIAN_EPOCH.add(fixed - 1)
}

/// Calculates the month/day from the 1-based day of the year
pub fn year_day(year: i32, day_of_year: u16) -> (u8, u8) {
    // Calculates the prior days of the year, then applies a correction based on leap year conditions for the correct ISO date conversion.
    let correction = if day_of_year < 31 + 28 + is_leap_year(year) as u16 {
        -1
    } else {
        (!is_leap_year(year)) as i32
    };
    let month = ((12 * (day_of_year as i32 + correction) + 373) / 367) as u8; // in 1..12 < u8::MAX
    let day = (day_of_year - days_before_month(year, month)) as u8; // <= days_in_month < u8::MAX
    (month, day)
}

/// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/1ee51ecfaae6f856b0d7de3e36e9042100b4f424/calendar.l#L1711-L1738>
pub fn julian_from_fixed(date: RataDie) -> Result<(i32, u8, u8), I32CastError> {
    let year = year_from_fixed(date)?;
    let day_of_year = date - day_before_year(year);
    let (month, day) = year_day(year, day_of_year as u16);
    Ok((year, month, day))
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
