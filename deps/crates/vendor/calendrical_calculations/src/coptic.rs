// This file is part of ICU4X.
//
// The contents of this file implement algorithms from Calendrical Calculations
// by Reingold & Dershowitz, Cambridge University Press, 4th edition (2018),
// which have been released as Lisp code at <https://github.com/EdReingold/calendar-code2/>
// under the Apache-2.0 license. Accordingly, this file is released under
// the Apache License, Version 2.0 which can be found at the calendrical_calculations
// package root or at http://www.apache.org/licenses/LICENSE-2.0.

use crate::helpers::{i64_to_i32, I32CastError};
use crate::rata_die::RataDie;

pub(crate) const COPTIC_EPOCH: RataDie = crate::julian::fixed_from_julian(284, 8, 29);

/// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/1ee51ecfaae6f856b0d7de3e36e9042100b4f424/calendar.l#L1978>
pub fn fixed_from_coptic(year: i32, month: u8, day: u8) -> RataDie {
    COPTIC_EPOCH - 1
        + 365 * (year as i64 - 1)
        + year.div_euclid(4) as i64
        + 30 * (month as i64 - 1)
        + day as i64
}

/// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/1ee51ecfaae6f856b0d7de3e36e9042100b4f424/calendar.l#L1990>
pub fn coptic_from_fixed(date: RataDie) -> Result<(i32, u8, u8), I32CastError> {
    let year = (4 * (date - COPTIC_EPOCH) + 1463).div_euclid(1461);
    let year = i64_to_i32(year)?;
    let month = ((date - fixed_from_coptic(year, 1, 1)).div_euclid(30) + 1) as u8; // <= 12 < u8::MAX
    let day = (date + 1 - fixed_from_coptic(year, month, 1)) as u8; // <= days_in_month < u8::MAX

    Ok((year, month, day))
}
