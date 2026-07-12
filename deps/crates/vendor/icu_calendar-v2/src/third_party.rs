// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Support for third-party date libraries.
//!
//! ICU4X provides conversions from types from popular third-party date libraries
//! into [`Date`](crate::Date).
//!
//! ✨ *To enable this support, the corresponding Cargo feature must be enabled:*
//!
//! - `unstable_chrono_0_4` for [chrono](https://crates.io/crates/chrono)
//! - `unstable_jiff_0_2` for [jiff](https://crates.io/crates/jiff)
//! - `unstable_time_0_3` for [time](https://crates.io/crates/time)
//!
//! <div class="stab unstable">
//! 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
//! including in SemVer minor releases. Use with caution.
//! </div>
//!
//! # Chrono
//!
//! The following examples show how to use [`chrono`] types with ICU4X calendars.
//!
//! ```
//! # #[cfg(feature = "unstable_chrono_0_4")] {
//! use icu::calendar::{Date, Gregorian, types::Weekday};
//!
//! // Convert a chrono::NaiveDate into an ICU4X Date<Gregorian>
//! let chrono_date = chrono::NaiveDate::from_ymd_opt(2025, 1, 15).unwrap();
//! let icu_date = Date::<Gregorian>::from(chrono_date);
//! assert_eq!(icu_date.era_year().year, 2025);
//!
//! // Convert a chrono::Weekday into an ICU4X Weekday
//! let chrono_weekday = chrono::Weekday::Wed;
//! let icu_weekday = Weekday::from(chrono_weekday);
//! assert_eq!(icu_weekday, Weekday::Wednesday);
//! # }
//! ```
//!
//! # Jiff
//!
//! The following examples show how to use [`jiff`] types with ICU4X calendars.
//!
//! ```
//! # #[cfg(feature = "unstable_jiff_0_2")] {
//! use icu::calendar::{Date, Gregorian, types::Weekday};
//!
//! // Convert a jiff::civil::Date into an ICU4X Date<Gregorian>
//! let jiff_date = jiff::civil::date(2025, 1, 15);
//! let icu_date = Date::<Gregorian>::from(jiff_date);
//! assert_eq!(icu_date.era_year().year, 2025);
//!
//! // Convert a jiff::civil::Weekday into an ICU4X Weekday
//! let jiff_weekday = jiff::civil::Weekday::Wednesday;
//! let icu_weekday = Weekday::from(jiff_weekday);
//! assert_eq!(icu_weekday, Weekday::Wednesday);
//! # }
//! ```
//!
//! # Time
//!
//! The following examples show how to use [`time`] types with ICU4X calendars.
//!
//! ```
//! # #[cfg(feature = "unstable_time_0_3")] {
//! use icu::calendar::{Date, Gregorian, types::Weekday};
//!
//! // Convert a time::Date into an ICU4X Date<Gregorian>
//! let time_date = time::Date::from_calendar_date(2025, time::Month::January, 15).unwrap();
//! let icu_date = Date::<Gregorian>::from(time_date);
//! assert_eq!(icu_date.era_year().year, 2025);
//!
//! // Convert a time::Weekday into an ICU4X Weekday
//! let time_weekday = time::Weekday::Wednesday;
//! let icu_weekday = Weekday::from(time_weekday);
//! assert_eq!(icu_weekday, Weekday::Wednesday);
//! # }
//! ```
