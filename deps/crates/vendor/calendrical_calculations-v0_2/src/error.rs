// This file is part of ICU4X.
//
// The contents of this file implement algorithms from Calendrical Calculations
// by Reingold & Dershowitz, Cambridge University Press, 4th edition (2018),
// which have been released as Lisp code at <https://github.com/EdReingold/calendar-code2/>
// under the Apache-2.0 license. Accordingly, this file is released under
// the Apache License, Version 2.0 which can be found at the calendrical_calculations
// package root or at http://www.apache.org/licenses/LICENSE-2.0.

use displaydoc::Display;

/// A list of error outcomes for exceeding location bounds
#[derive(Display, Debug, Copy, Clone, PartialEq)]
#[non_exhaustive]
pub enum LocationOutOfBoundsError {
    /// Latitude value was out of bounds
    #[displaydoc("Latitude {0} outside bounds of -90 to 90")]
    Latitude(f64),

    /// Longitude value was out of bounds
    #[displaydoc("Longitude {0} outside bounds of -180 to 180")]
    Longitude(f64),

    /// Offset value was out of bounds
    #[displaydoc("Offset {0} outside bounds of {1} to {2}")]
    Offset(f64, f64, f64),
}

impl core::error::Error for LocationOutOfBoundsError {}
