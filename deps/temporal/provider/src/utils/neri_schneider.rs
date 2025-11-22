//! Gregorian Date Calculations
//!
//! This module contains the logic for Gregorian Date Calculations based
//! off Cassio Neri and Lorenz Schneider's paper, [Euclidean affine functions
//! and their application to calendar algorithms][eaf-calendar-algorithms].
//!
//! ## General Usage Note
//!
//! Unless specified, Rata Die refers to the computational rata die as referenced
//! in the paper.
//!
//! ## Extending Neri-Schneider shift window
//! Temporal must support the year range [-271_821, 275_760]
//!
//! This means the epoch day range must be epoch_days.abs() <= 100_000_001
//!
//! Neri-Schneider mention shifting for a range of 32_767, so the shift
//! will need to be much greater.
//!
//! (-271_821 / 400).ciel() = s // 680
//!
//! In their paper, Neri and Schneider calculated for a Rata Die cycle
//! shift of constant of 82, but that was not sufficient in order to
//! support Temporal's date range, so below is a small addendum table
//! on extending the date range from a cyle shift of 82 to 680 in order
//! to accomodate Temporal's range.
//!
//! | Significant Date | Computational Rata Die | Rata Die Shift
//! | -----------------|------------------------|-----------------|
//! | April 19, -271_821 | -99,280,532 | 65,429 |
//! | January 1, 1970 | 719,468 | 100,065,428 |
//! | September 14, 275,760 | 100_719_469 | 200,065,429 |
//!
//! However, this shift has also been implemented by Cassio Neri, who
//! recommends using a [shift of 3670][neri-shift-context] which places the Epoch in the
//! center of the shift
//!
//! [neri-shift-context]: https://hg.mozilla.org/integration/autoland/rev/54ebf8bd2e11#l3.70
//! [eaf-calendar-algorithms]: https://onlinelibrary.wiley.com/doi/full/10.1002/spe.3172

pub const EPOCH_COMPUTATIONAL_RATA_DIE: i32 = 719_468;
pub const DAYS_IN_A_400Y_CYCLE: u32 = 146_097;

#[cfg(feature = "tzif")]
const TWO_POWER_THIRTY_NINE: u64 = 549_755_813_888; // 2^39 constant
const TWO_POWER_THIRTY_TWO: u64 = 4_294_967_296; // 2^32 constant
const TWO_POWER_SIXTEEN: u32 = 65_536; // 2^16 constant
                                       // NOTE (nekevss): Shift constant is optimized and safe for supported date range.
const SHIFT_CONSTANT: i32 = 3670;
// NOTE (nekevss): Extended const function, because Temporal
// needs to support calculating for an arbitrarily
// large epoch day range.
const SHIFT_CONSTANT_EXTENDED: i64 = 5_368_710;

/// Calculate Rata Die value from gregorian
pub const fn epoch_days_from_gregorian_date(year: i32, month: u8, day: u8) -> i64 {
    let shift =
        SHIFT_CONSTANT_EXTENDED * DAYS_IN_A_400Y_CYCLE as i64 + EPOCH_COMPUTATIONAL_RATA_DIE as i64;
    let (comp_year, comp_month, comp_day, century) = rata_die_first_equations(year, month, day);
    let y_star = 1461 * comp_year / 4 - century + century / 4;
    let m_star = (979 * comp_month - 2919) / 32;
    (y_star as i64 + m_star + comp_day) - shift
}

// Returns Y, M, D, C
const fn rata_die_first_equations(year: i32, month: u8, day: u8) -> (u64, i64, i64, u64) {
    let j = (month <= 2) as i64;
    let computational_year = (year as i64 + 400 * SHIFT_CONSTANT_EXTENDED) - j;
    let computation_month = month as i64 + 12 * j;
    let computation_day = day as i64 - 1;
    (
        computational_year as u64,
        computation_month,
        computation_day,
        computational_year as u64 / 100,
    )
}

// Computational days to gregorian YMD

// Determine j
#[cfg(feature = "tzif")]
const fn j(rata_die: u32) -> u32 {
    (computational_day_of_year(rata_die) >= 306) as u32
}

const fn n_one(rata_die: u32) -> u32 {
    4 * rata_die + 3
}

#[cfg(feature = "tzif")]
const fn n_two(rata_die: u32) -> u32 {
    century_rem(rata_die) | 3
}

// Returns C, N_c AKA century number and century remainder
const fn first_equations(rata_die: u32) -> (u32, u32) {
    let n_one = n_one(rata_die);
    let century_rem = n_one.rem_euclid(146_097);
    let century_num = n_one.div_euclid(DAYS_IN_A_400Y_CYCLE);
    (century_num, century_rem)
}

#[cfg(feature = "tzif")]
const fn century_rem(rata_die: u32) -> u32 {
    n_one(rata_die).rem_euclid(DAYS_IN_A_400Y_CYCLE)
}

#[cfg(feature = "tzif")]
pub const fn century_number(rata_die: u32) -> u32 {
    n_one(rata_die).div_euclid(DAYS_IN_A_400Y_CYCLE)
}

/// returns Y, N_y AKA, year and day_of_year
const fn second_equations(rata_die: u32) -> (u32, u32) {
    let (century, rem) = first_equations(rata_die);
    let n_two = rem | 3;
    let p2 = 2_939_745 * n_two as u64;
    let year_of_century = p2.div_euclid(TWO_POWER_THIRTY_TWO) as u32;
    let day_of_year = p2
        .rem_euclid(TWO_POWER_THIRTY_TWO)
        .div_euclid(2_939_745)
        .div_euclid(4) as u32;
    let year = 100 * century + year_of_century;
    (year, day_of_year)
}

// Returns Y, M, D, N_y, AKA year, month, day, day_of_year
const fn third_equations(rata_die: u32) -> (u32, u32, u32, u32) {
    let (year, day_of_year) = second_equations(rata_die);
    let n_three = 2141 * day_of_year + 197_913;
    let month = n_three.div_euclid(TWO_POWER_SIXTEEN);
    let day = n_three.rem_euclid(TWO_POWER_SIXTEEN).div_euclid(2141);
    (year, month, day, day_of_year)
}

// Z
#[cfg(feature = "tzif")]
pub const fn computational_year_of_century(rata_die: u32) -> u64 {
    (376_287_347 * n_two(rata_die) as u64).div_euclid(TWO_POWER_THIRTY_NINE)
}

// N_y
#[cfg(feature = "tzif")]
pub const fn computational_day_of_year(rata_die: u32) -> u32 {
    (n_two(rata_die) - 1461 * computational_year_of_century(rata_die) as u32).div_euclid(4)
}

// Y
#[cfg(feature = "tzif")]
pub const fn computational_year(rata_die: u32) -> u32 {
    100 * century_number(rata_die) + computational_year_of_century(rata_die) as u32
}

#[cfg(feature = "tzif")]
pub const fn year(computational_rata_die: u32, shift_constant: i32) -> i32 {
    (computational_year(computational_rata_die) + j(computational_rata_die)) as i32 - shift_constant
}

const fn gregorian_ymd(rata_die: u32) -> (i32, u8, u8) {
    let (year, month, day, day_of_year) = third_equations(rata_die);
    let j = (day_of_year >= 306) as u32;
    let year = year + j;
    let month = month - 12 * j;
    let day = day + 1;
    (year as i32, month as u8, day as u8)
}

/// Get the computational Rata Die for given Epoch Days with the cycle shiftc.
///
/// For more on `cycle_shifts`, see [`ymd_from_epoch_days`]
pub const fn rata_die_for_epoch_days(epoch_days: i32) -> (u32, i32) {
    let rata_die = (epoch_days
        + EPOCH_COMPUTATIONAL_RATA_DIE
        + DAYS_IN_A_400Y_CYCLE as i32 * SHIFT_CONSTANT) as u32; // epoch_days + K
    (rata_die, 400 * SHIFT_CONSTANT)
}

/// Calculate a Gregorian year, month, and date for the provided epoch days.
pub const fn ymd_from_epoch_days(epoch_days: i32) -> (i32, u8, u8) {
    let (rata_die, year_shift_constant) = rata_die_for_epoch_days(epoch_days);

    let (year, month, day) = gregorian_ymd(rata_die);
    // Shift the year back to the proper date
    (year - year_shift_constant, month, day)
}

#[cfg(test)]
#[cfg(feature = "tzif")]
mod tests {
    use super::*;

    const EPOCH_RATA_DIE: u32 = 719_468; // This is the Rata Die for 1970-01-01

    const fn days_in_century(rata_die: u32) -> u32 {
        century_rem(rata_die).div_euclid(4)
    }

    #[test]
    fn epoch_century_number() {
        let century_number = century_number(EPOCH_RATA_DIE);
        assert_eq!(century_number, 19);
        let day_number_in_century = days_in_century(EPOCH_RATA_DIE);
        assert_eq!(day_number_in_century, 25508);
    }

    #[test]
    fn epoch_year_of_century() {
        let year = computational_year_of_century(EPOCH_RATA_DIE);
        assert_eq!(year, 69);
    }

    #[test]
    fn epoch_day_of_year() {
        let day = computational_day_of_year(EPOCH_RATA_DIE);
        assert_eq!(day, 306); // Beginning of January in the computational calendar is day number 306
    }

    #[test]
    fn epoch_year() {
        let year = computational_year(EPOCH_RATA_DIE);
        assert_eq!(year, 1969);
    }

    #[test]
    fn epoch_ymd() {
        let ymd = gregorian_ymd(EPOCH_RATA_DIE);
        assert_eq!(ymd, (1970, 1, 1))
    }

    #[test]
    fn rata_die_from_date() {
        let epoch_rata_die = epoch_days_from_gregorian_date(1970, 1, 1);
        assert_eq!(epoch_rata_die, 0);
        let date = ymd_from_epoch_days(epoch_rata_die as i32);
        assert_eq!(date, (1970, 1, 1));
        let neri_scneider_limit_max_rata_die = epoch_days_from_gregorian_date(32767, 12, 31);
        assert_eq!(neri_scneider_limit_max_rata_die, 11_248_737);
        let neri_schneider_limit_min_rata_die = epoch_days_from_gregorian_date(-32767, 1, 1);
        assert_eq!(neri_schneider_limit_min_rata_die, -12_687_428);
        let js_max_rata_die = epoch_days_from_gregorian_date(275_760, 9, 14);
        assert_eq!(js_max_rata_die, 100_000_001);
        let js_min_rata_die = epoch_days_from_gregorian_date(-271_821, 4, 19);
        assert_eq!(js_min_rata_die, -100_000_001);
    }

    #[test]
    fn epoch_days_temporal_limit_to_date() {
        let max_date = ymd_from_epoch_days(100_000_001);
        assert_eq!(max_date, (275_760, 9, 14));
        let min_date = ymd_from_epoch_days(-100_000_001);
        assert_eq!(min_date, (-271_821, 4, 19));
    }

    #[test]
    fn epoch_days_from() {
        let epoch_days = epoch_days_from_gregorian_date(1970, 1, 1);
        assert_eq!(epoch_days, 0);
        let epoch_days = epoch_days_from_gregorian_date(275_760, 9, 14);
        assert_eq!(epoch_days, 100_000_001);
        let epoch_days = epoch_days_from_gregorian_date(-271_821, 4, 19);
        let result = ymd_from_epoch_days(epoch_days as i32);
        assert_eq!(result, (-271_821, 4, 19));
        assert_eq!(epoch_days, -100_000_001);
    }
}
