// This file is part of ICU4X.
//
// The contents of this file implement algorithms from Calendrical Calculations
// by Reingold & Dershowitz, Cambridge University Press, 4th edition (2018),
// which have been released as Lisp code at <https://github.com/EdReingold/calendar-code2/>
// under the Apache-2.0 license. Accordingly, this file is released under
// the Apache License, Version 2.0 which can be found at the calendrical_calculations
// package root or at http://www.apache.org/licenses/LICENSE-2.0.

use crate::helpers::{i64_to_i32, I32CastError, IntegerRoundings};
use crate::rata_die::RataDie;

/// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L4720>
// Book states that the Persian epoch is the date: 3/19/622 and since the Persian Calendar has no year 0, the best choice was to use the Julian function.
const FIXED_PERSIAN_EPOCH: RataDie = crate::julian::fixed_from_julian(622, 3, 19);

// All these years are not leap, while they are considered leap by the 33-year
// rule. The year following each of them is leap, but it's considered non-leap
// by the 33-year rule. This table has been tested to match the modified
// astronomical algorithm based on the 52.5 degrees east meridian from 1178 AP
// (an arbitrary date before the Persian calendar was adopted in 1304 AP) to
// 3000 AP (an arbitrary date far into the future).
const NON_LEAP_CORRECTION: [i32; 78] = [
    1502, 1601, 1634, 1667, 1700, 1733, 1766, 1799, 1832, 1865, 1898, 1931, 1964, 1997, 2030, 2059,
    2063, 2096, 2129, 2158, 2162, 2191, 2195, 2224, 2228, 2257, 2261, 2290, 2294, 2323, 2327, 2356,
    2360, 2389, 2393, 2422, 2426, 2455, 2459, 2488, 2492, 2521, 2525, 2554, 2558, 2587, 2591, 2620,
    2624, 2653, 2657, 2686, 2690, 2719, 2723, 2748, 2752, 2756, 2781, 2785, 2789, 2818, 2822, 2847,
    2851, 2855, 2880, 2884, 2888, 2913, 2917, 2921, 2946, 2950, 2954, 2979, 2983, 2987,
];

const MIN_NON_LEAP_CORRECTION: i32 = NON_LEAP_CORRECTION[0];

/// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L4803>
/// Not used, but kept for comparative purposes
pub fn fixed_from_arithmetic_persian(year: i32, month: u8, day: u8) -> RataDie {
    let p_year = i64::from(year);
    let month = i64::from(month);
    let day = i64::from(day);
    let y = if p_year > 0 {
        p_year - 474
    } else {
        p_year - 473
    };
    let year = y.rem_euclid(2820) + 474;

    RataDie::new(
        FIXED_PERSIAN_EPOCH.to_i64_date() - 1
            + 1029983 * y.div_euclid(2820)
            + 365 * (year - 1)
            + (31 * year - 5).div_euclid(128)
            + if month <= 7 {
                31 * (month - 1)
            } else {
                30 * (month - 1) + 6
            }
            + day,
    )
}

/// fixed_from_arithmetic_persian, modified to use the more correct 33-year rule
pub fn fixed_from_fast_persian(year: i32, month: u8, day: u8) -> RataDie {
    let p_year = i64::from(year);
    let month = i64::from(month);
    let day = i64::from(day);
    let mut new_year = FIXED_PERSIAN_EPOCH.to_i64_date() - 1
        + 365 * (p_year - 1)
        + (8 * p_year + 21).div_euclid(33);
    if year > MIN_NON_LEAP_CORRECTION && NON_LEAP_CORRECTION.binary_search(&(year - 1)).is_ok() {
        new_year -= 1;
    }
    RataDie::new(
        new_year - 1
            + if month <= 7 {
                31 * (month - 1)
            } else {
                30 * (month - 1) + 6
            }
            + day,
    )
}

/// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L4857>
/// Not used, but kept for comparative purposes
pub fn arithmetic_persian_from_fixed(date: RataDie) -> Result<(i32, u8, u8), I32CastError> {
    let year = arithmetic_persian_year_from_fixed(date);
    let year = i64_to_i32(year)?;
    let day_of_year = 1_i64 + (date - fixed_from_arithmetic_persian(year, 1, 1));
    #[allow(unstable_name_collisions)] // div_ceil is unstable and polyfilled
    let month = if day_of_year <= 186 {
        day_of_year.div_ceil(31) as u8
    } else {
        (day_of_year - 6).div_ceil(30) as u8
    };
    let day = (date - fixed_from_arithmetic_persian(year, month, 1) + 1) as u8;
    Ok((year, month, day))
}

/// arithmetic_persian_from_fixed, modified to use the 33-year rule method
pub fn fast_persian_from_fixed(date: RataDie) -> Result<(i32, u8, u8), I32CastError> {
    let year = fast_persian_year_from_fixed(date);
    let mut year = i64_to_i32(year)?;
    let mut day_of_year = 1_i64 + (date - fixed_from_fast_persian(year, 1, 1));
    if day_of_year == 366
        && year >= MIN_NON_LEAP_CORRECTION
        && NON_LEAP_CORRECTION.binary_search(&year).is_ok()
    {
        year += 1;
        day_of_year = 1;
    }
    #[allow(unstable_name_collisions)] // div_ceil is unstable and polyfilled
    let month = if day_of_year <= 186 {
        day_of_year.div_ceil(31) as u8
    } else {
        (day_of_year - 6).div_ceil(30) as u8
    };
    let day = (date - fixed_from_fast_persian(year, month, 1) + 1) as u8;
    Ok((year, month, day))
}

/// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L4829>
/// Not used, but kept for comparative purposes
fn arithmetic_persian_year_from_fixed(date: RataDie) -> i64 {
    let d0 = date - fixed_from_arithmetic_persian(475, 1, 1);
    let n2820 = d0.div_euclid(1029983);
    let d1 = d0.rem_euclid(1029983);
    let y2820 = if d1 == 1029982 {
        2820
    } else {
        (128 * d1 + 46878).div_euclid(46751)
    };
    let year = 474 + n2820 * 2820 + y2820;
    if year > 0 {
        year
    } else {
        year - 1
    }
}

/// arithmetic_persian_year_from_fixed modified for the 33-year rule
fn fast_persian_year_from_fixed(date: RataDie) -> i64 {
    let days_since_epoch = date - FIXED_PERSIAN_EPOCH + 1;
    1 + (33 * days_since_epoch + 3).div_euclid(12053)
}

/// Lisp code reference: https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L4789
/// Not used, but kept for comparative purposes
#[allow(dead_code)]
fn is_arithmetic_leap_year(p_year: i32) -> bool {
    let mut p_year = p_year as i64;
    if 0 < p_year {
        p_year -= 474;
    } else {
        p_year -= 473;
    };
    let year = p_year.rem_euclid(2820) + 474;

    ((year + 38) * 31).rem_euclid(128) < 31
}

/// Calculated using the 33-year rule
pub fn is_leap_year(p_year: i32) -> bool {
    if p_year >= MIN_NON_LEAP_CORRECTION && NON_LEAP_CORRECTION.binary_search(&p_year).is_ok() {
        false
    } else if p_year > MIN_NON_LEAP_CORRECTION
        && NON_LEAP_CORRECTION.binary_search(&(p_year - 1)).is_ok()
    {
        true
    } else {
        let p_year = p_year as i64;
        (25 * p_year + 11).rem_euclid(33) < 8
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn test_persian_epoch() {
        let epoch = FIXED_PERSIAN_EPOCH.to_i64_date();
        // Proleptic Gregorian year of Persian Epoch
        let epoch_year_from_fixed = crate::gregorian::year_from_fixed(RataDie::new(epoch)).unwrap();
        // 622 is the correct proleptic Gregorian year for the Persian Epoch
        assert_eq!(epoch_year_from_fixed, 622);
    }

    // Persian New Year occurring in March of proleptic Gregorian year (g_year) to fixed date
    fn nowruz(g_year: i32) -> RataDie {
        let (y, _m, _d) = crate::gregorian::gregorian_from_fixed(FIXED_PERSIAN_EPOCH).unwrap();
        let persian_year = g_year - y + 1;
        let year = if persian_year <= 0 {
            persian_year - 1
        } else {
            persian_year
        };
        fixed_from_fast_persian(year, 1, 1)
    }

    #[test]
    fn test_nowruz() {
        // These values are used as test data in appendix C of the "Calendrical Calculations" book
        let nowruz_test_year_start = 2000;
        let nowruz_test_year_end = 2103;

        for year in nowruz_test_year_start..=nowruz_test_year_end {
            let two_thousand_eight_to_fixed = nowruz(year).to_i64_date();
            let gregorian_date = crate::gregorian::fixed_from_gregorian(year, 3, 21);
            let (persian_year, _m, _d) = fast_persian_from_fixed(gregorian_date).unwrap();
            assert_eq!(
                fast_persian_from_fixed(RataDie::new(two_thousand_eight_to_fixed))
                    .unwrap()
                    .0,
                persian_year
            );
        }
    }
}
