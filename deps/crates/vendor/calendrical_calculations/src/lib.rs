// This file is part of ICU4X.
//
// The contents of this file implement algorithms from Calendrical Calculations
// by Reingold & Dershowitz, Cambridge University Press, 4th edition (2018),
// which have been released as Lisp code at <https://github.com/EdReingold/calendar-code2/>
// under the Apache-2.0 license. Accordingly, this file is released under
// the Apache License, Version 2.0 which can be found at the calendrical_calculations
// package root or at http://www.apache.org/licenses/LICENSE-2.0.

//! Calendrical calculations
//!
//! This crate implements algorithms from
//! Calendrical Calculations by Reingold & Dershowitz, Cambridge University Press, 4th edition (2018)
//! as needed by [ICU4X](https://github.com/unicode-org/icu4x).
//!
//! Most of these algorithms can be found as lisp code in the book or
//! [on GithHub](https://github.com/EdReingold/calendar-code2/blob/main/calendar.l).
//!
//! The primary purpose of this crate is use by ICU4X, however if non-ICU4X users need this we are happy
//! to add more structure to this crate as needed.
// https://github.com/unicode-org/icu4x/blob/main/documents/process/boilerplate.md#library-annotations
#![cfg_attr(not(any(test, doc)), no_std)]
#![cfg_attr(
    not(test),
    deny(
        clippy::indexing_slicing,
        clippy::unwrap_used,
        clippy::expect_used,
        clippy::panic,
        clippy::exhaustive_structs,
        clippy::exhaustive_enums,
        clippy::trivially_copy_pass_by_ref,
        missing_debug_implementations,
    )
)]
#![warn(missing_docs)]

mod astronomy;
/// Chinese-like lunar calendars (Chinese, Dangi)
pub mod chinese_based;
/// The Coptic calendar
pub mod coptic;
/// Error handling
mod error;
/// The Ethiopian calendar
pub mod ethiopian;
/// The Gregorian calendar
pub mod gregorian;
/// The Hebrew calendar
pub mod hebrew;
pub mod hebrew_keviyah;
/// Additional math helpers
pub mod helpers;
/// Various islamic lunar calendars
pub mod islamic;
/// Aliases for the Gregorian calendar.
///
/// This is *not* the ISO week calendar as described in the book.
#[deprecated(since = "0.2.3", note = "use `gregorian`")]
pub mod iso {
    pub use crate::gregorian::day_before_year;
    pub use crate::gregorian::days_before_month;
    pub use crate::gregorian::easter;
    pub use crate::gregorian::fixed_from_gregorian as const_fixed_from_iso;
    pub use crate::gregorian::fixed_from_gregorian as fixed_from_iso;
    pub use crate::gregorian::gregorian_from_fixed as iso_from_fixed;
    pub use crate::gregorian::is_leap_year;
    pub use crate::gregorian::year_day;
    pub use crate::gregorian::year_from_fixed as iso_year_from_fixed;
}
/// The Julian calendar
pub mod julian;
/// The Persian calendar
pub mod persian;
/// Representation of Rata Die (R.D.) dates, which are
/// represented as the number of days since ISO date 0001-01-01.
pub mod rata_die;
