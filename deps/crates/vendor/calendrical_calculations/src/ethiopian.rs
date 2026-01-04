// This file is part of ICU4X.
//
// The contents of this file implement algorithms from Calendrical Calculations
// by Reingold & Dershowitz, Cambridge University Press, 4th edition (2018),
// which have been released as Lisp code at <https://github.com/EdReingold/calendar-code2/>
// under the Apache-2.0 license. Accordingly, this file is released under
// the Apache License, Version 2.0 which can be found at the calendrical_calculations
// package root or at http://www.apache.org/licenses/LICENSE-2.0.

use crate::helpers::I32CastError;
use crate::rata_die::RataDie;

const ETHIOPIC_TO_COPTIC_OFFSET: i64 =
    super::coptic::COPTIC_EPOCH.since(crate::julian::fixed_from_julian(8, 8, 29));

/// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/1ee51ecfaae6f856b0d7de3e36e9042100b4f424/calendar.l#L2017>
pub fn fixed_from_ethiopian(year: i32, month: u8, day: u8) -> RataDie {
    crate::coptic::fixed_from_coptic(year, month, day) - ETHIOPIC_TO_COPTIC_OFFSET
}

/// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/1ee51ecfaae6f856b0d7de3e36e9042100b4f424/calendar.l#L2028>
pub fn ethiopian_from_fixed(date: RataDie) -> Result<(i32, u8, u8), I32CastError> {
    crate::coptic::coptic_from_fixed(date + ETHIOPIC_TO_COPTIC_OFFSET)
}
