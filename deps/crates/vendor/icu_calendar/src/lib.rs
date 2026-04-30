// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Types for dealing with dates and custom calendars.
//!
//! This module is published as its own crate ([`icu_calendar`](https://docs.rs/icu_calendar/latest/icu_calendar/))
//! and as part of the [`icu`](https://docs.rs/icu/latest/icu/) crate. See the latter for more details on the ICU4X project.
//! The [`types`] module has a lot of common types for dealing with dates.
//!
//! [`Calendar`] is a trait that allows one to define custom calendars, and [`Date`]
//! can represent dates for arbitrary calendars.
//!
//! The [`Iso`] and [`Gregorian`] types are implementations for the ISO and
//! Gregorian calendars respectively. Further calendars can be found in the [`cal`] module.
//!
//! Most interaction with this crate will be done via the [`Date`] type.
//!
//! Some of the algorithms implemented here are based on
//! Dershowitz, Nachum, and Edward M. Reingold. _Calendrical calculations_. Cambridge University Press, 2008.
//! with associated Lisp code found at <https://github.com/EdReingold/calendar-code2>.
//!
//! # Examples
//!
//! Examples of date manipulation using `Date` object. `Date` objects are useful
//! for working with dates, encompassing information about the day, month, year,
//! as well as the calendar type.
//!
//! ```rust
//! use icu::calendar::{types::Weekday, Date};
//!
//! // Creating ISO date: 1992-09-02.
//! let mut date_iso = Date::try_new_iso(1992, 9, 2)
//!     .expect("Failed to initialize ISO Date instance.");
//!
//! assert_eq!(date_iso.day_of_week(), Weekday::Wednesday);
//! assert_eq!(date_iso.era_year().year, 1992);
//! assert_eq!(date_iso.month().ordinal, 9);
//! assert_eq!(date_iso.day_of_month().0, 2);
//!
//! // Answering questions about days in month and year.
//! assert_eq!(date_iso.days_in_year(), 366);
//! assert_eq!(date_iso.days_in_month(), 30);
//! ```
//!
//! Example of converting an ISO date across Indian and Buddhist calendars.
//!
//! ```rust
//! use icu::calendar::cal::{Buddhist, Indian};
//! use icu::calendar::Date;
//!
//! // Creating ISO date: 1992-09-02.
//! let mut date_iso = Date::try_new_iso(1992, 9, 2)
//!     .expect("Failed to initialize ISO Date instance.");
//!
//! assert_eq!(date_iso.era_year().year, 1992);
//! assert_eq!(date_iso.month().ordinal, 9);
//! assert_eq!(date_iso.day_of_month().0, 2);
//!
//! // Conversion into Indian calendar: 1914-08-02.
//! let date_indian = date_iso.to_calendar(Indian);
//! assert_eq!(date_indian.era_year().year, 1914);
//! assert_eq!(date_indian.month().ordinal, 6);
//! assert_eq!(date_indian.day_of_month().0, 11);
//!
//! // Conversion into Buddhist calendar: 2535-09-02.
//! let date_buddhist = date_iso.to_calendar(Buddhist);
//! assert_eq!(date_buddhist.era_year().year, 2535);
//! assert_eq!(date_buddhist.month().ordinal, 9);
//! assert_eq!(date_buddhist.day_of_month().0, 2);
//! ```
//!
//! [`ICU4X`]: ../icu/index.html

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

#[cfg(feature = "alloc")]
extern crate alloc;

// Make sure inherent docs go first
mod date;

// Public modules
mod any_calendar;
pub mod cal;
pub mod provider;
pub mod types;
pub mod week;

mod calendar;
mod calendar_arithmetic;
mod duration;
mod error;
#[cfg(feature = "ixdtf")]
mod ixdtf;

// Top-level types
pub use any_calendar::IntoAnyCalendar;
pub use calendar::Calendar;
pub use date::{AsCalendar, Date, Ref};
#[doc(hidden)] // unstable
pub use duration::{DateDuration, DateDurationUnit};
pub use error::{DateError, RangeError};
#[cfg(feature = "ixdtf")]
pub use ixdtf::ParseError;

// Reexports
#[doc(no_inline)]
pub use cal::{AnyCalendar, AnyCalendarKind, Gregorian, Iso};

/// Locale preferences used by this crate
pub mod preferences {
    pub use crate::any_calendar::CalendarPreferences;
    #[doc(inline)]
    /// **This is a reexport of a type in [`icu::locale`](icu_locale_core::preferences::extensions::unicode::keywords)**.
    #[doc = "\n"] // prevent autoformatting
    pub use icu_locale_core::preferences::extensions::unicode::keywords::CalendarAlgorithm;
    #[doc(inline)]
    /// **This is a reexport of a type in [`icu::locale`](icu_locale_core::preferences::extensions::unicode::keywords)**.
    #[doc = "\n"] // prevent autoformatting
    pub use icu_locale_core::preferences::extensions::unicode::keywords::HijriCalendarAlgorithm;
}

#[cfg(test)]
mod tests;
