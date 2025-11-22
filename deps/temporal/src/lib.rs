//! A native Rust implementation of ECMAScript's Temporal API.
//!
//! Temporal is an API for working with date and time in a calendar
//! and time zone aware manner.
//!
//! temporal_rs is designed with ECMAScript implementations and general
//! purpose Rust usage in mind, meaning that temporal_rs can be used to implement
//! the Temporal built-ins in an ECMAScript implementation or generally
//! used as a date and time library in a Rust project.
//!
//! temporal_rs is the primary library for the Temporal API implementation in Boa, Kiesel,
//! and V8. Each of these engines pass the large ECMAScript conformance test suite for
//! the specification.
//!
//! ## Why use temporal_rs?
//!
//! As previously mentioned, Temporal is an API for working with date and time in
//! a calendar and time zone aware manner. This means that calendar and time zone support
//! are first class in Temporal as well as in temporal_rs.
//!
//! For instance, converting between calendars is as simple as providing the calendar as
//! shown below.
//!
//! ```rust
//! use temporal_rs::{PlainDate, Calendar};
//! use tinystr::tinystr;
//! use core::str::FromStr;
//!
//! // Create a date with an ISO calendar
//! let iso8601_date = PlainDate::try_new_iso(2025, 3, 3).unwrap();
//!
//! // Create a new date with the japanese calendar
//! let japanese_date = iso8601_date.with_calendar(Calendar::JAPANESE);
//! assert_eq!(japanese_date.era(), Some(tinystr!(16, "reiwa")));
//! assert_eq!(japanese_date.era_year(), Some(7));
//! assert_eq!(japanese_date.month(), 3)
//! ```
//!
//! Beyond the general calendar use case, temporal_rs has robust support for
//! time zones which can generally by applied to a `PlainDate` via [`ZonedDateTime`].
//!
//! **Important Note:** The below API is enabled with the
//! `compiled_data` feature flag.
//!
//! ```rust
//! # #[cfg(feature = "compiled_data")] {
//! use temporal_rs::{ZonedDateTime, TimeZone};
//! use temporal_rs::options::{Disambiguation, OffsetDisambiguation};
//!
//! let zdt = ZonedDateTime::from_utf8(
//!     b"2025-03-01T11:16:10Z[America/Chicago][u-ca=iso8601]",
//!     Disambiguation::Compatible,
//!     OffsetDisambiguation::Reject
//! ).unwrap();
//! assert_eq!(zdt.year(), 2025);
//! assert_eq!(zdt.month(), 3);
//! assert_eq!(zdt.day(), 1);
//! // Using Z and a timezone projects a UTC datetime into the timezone.
//! assert_eq!(zdt.hour(), 5);
//! assert_eq!(zdt.minute(), 16);
//! assert_eq!(zdt.second(), 10);
//!
//! // You can also update a time zone easily.
//! let zurich_zone = TimeZone::try_from_str("Europe/Zurich").unwrap();
//! let zdt_zurich = zdt.with_timezone(zurich_zone).unwrap();
//! assert_eq!(zdt_zurich.year(), 2025);
//! assert_eq!(zdt_zurich.month(), 3);
//! assert_eq!(zdt_zurich.day(), 1);
//! assert_eq!(zdt_zurich.hour(), 12);
//! assert_eq!(zdt_zurich.minute(), 16);
//! assert_eq!(zdt_zurich.second(), 10);
//!
//! # }
//! ```
//!
//! ## Overview
//!
//! temporal_rs provides 8 core types for working with date and time. The core types are:
//!
//! - [PlainDate]
//! - [PlainTime]
//! - [PlainDateTime]
//! - [ZonedDateTime]
//! - [Instant]
//! - [Duration]
//! - [PlainYearMonth]
//! - [PlainMonthDay]
//!
//! In addition to these types, there are the [`Calendar`] and [`TimeZone`] type that
//! support the calendars or time zones. The specific support for calendars and time
//! zones per type are as follows.
//!
//! | Temporal type  | Category                             | Calendar support   | Time zone support  |
//! |----------------|--------------------------------------|--------------------|--------------------|
//! | PlainDate      | Calendar date                        |        yes         |         no         |
//! | PlainTime      | Wall-clock time                      |        no          |         no         |
//! | PlainDateTime  | Calendar date and wall-clock time    |        yes         |         no         |
//! | ZonedDateTime  | Calendar date and exact time         |        yes         |        yes         |
//! | Instant        | Exact time                           |        no          |         no         |
//! | Duration       | None                                 |        no          |         no         |
//! | PlainYearMonth | Calendar date                        |        yes         |         no         |
//! | PlainMonthDay  | Calendar date                        |        yes         |         no         |
//!
//! There is also the [`Now`][now::Now], which provides access to the current host system
//! time. This can then be used to map to any of the above Temporal types.
//!
//! **Important Note:** the below example is only available with the `sys` and
//! `compiled_data` feature flag enabled.
//!
//! ```rust
//! # #[cfg(all(feature = "sys", feature = "compiled_data"))] {
//! use core::cmp::Ordering;
//! use temporal_rs::{Temporal, Calendar, ZonedDateTime};
//! let current_instant = Temporal::now().instant().unwrap();
//! let current_zoned_date_time = Temporal::now().zoned_date_time_iso(None).unwrap();
//!
//! /// Create a `ZonedDateTime` from the requested instant.
//! let zoned_date_time_from_instant = ZonedDateTime::try_new(
//!     current_instant.as_i128(),
//!     *current_zoned_date_time.time_zone(),
//!     Calendar::ISO,
//! ).unwrap();
//!
//! // The two `ZonedDateTime` will be equal down to the second.
//! assert_eq!(current_zoned_date_time.year(), zoned_date_time_from_instant.year());
//! assert_eq!(current_zoned_date_time.month(), zoned_date_time_from_instant.month());
//! assert_eq!(current_zoned_date_time.day(), zoned_date_time_from_instant.day());
//! assert_eq!(current_zoned_date_time.hour(), zoned_date_time_from_instant.hour());
//! assert_eq!(current_zoned_date_time.minute(), zoned_date_time_from_instant.minute());
//! assert_eq!(current_zoned_date_time.second(), zoned_date_time_from_instant.second());
//!
//! // The `Instant` reading that occurred first will be less than the ZonedDateTime
//! // reading
//! assert_eq!(
//!     zoned_date_time_from_instant.compare_instant(&current_zoned_date_time),
//!     Ordering::Less
//! );
//! # }
//! ```
//!
//! ## General design
//!
//! While temporal_rs can be used in native Rust programs, the library is -- first and
//! foremost -- designed for use in ECMAScript implementations. This is not to detract
//! from temporal_rs's use in a native Rust program, but it is important information to
//! understand in order to understand the library's architecture and general API design.
//!
//! Without default feature flags, temporal_rs does not have with access to the host
//! environment and it does not embed any time zone data. This is important from an
//! interpreter perspective, because access to the host environment and time zone data
//! comes from the interpreter's agent, not from a dependency.
//!
//! Instead, temporal_rs provides the [`HostHooks`][host::HostHooks] and [`TimeZoneProvider`][provider::TimeZoneProvider]
//! traits that can be implemented and provided as function arguments that temporal_rs will
//! use to access the host system or time zone data. temporal_rs also provides some baseline
//! implementations of the traits that can be selected from depending on application needs.
//!
//! That being said, this does not mean that everyone must implement their own trait
//! implementations for that functionality to exist, but the APIs are there for power
//! users who may need a custom host system or time zone data implementation.
//!
//! A default host system and time zone provider have been implemented and are automatically
//! active as default features.
//!
//! ### A quick walkthrough
//!
//! For instance, the examples thus far have been using the general usage Rust API with
//! the `sys` and `compiled_data` feature.
//!
//! For instance, let's manually write our [`Now`][now::Now] implementation instead of using
//! [`Temporal::now()`] with an empty host system implementation.
//!
//! ```rust
//! # #[cfg(feature = "compiled_data")] {
//! use temporal_rs::{Instant, now::Now, host::EmptyHostSystem};
//!
//! // The empty host system is a system implementation HostHooks that always
//! // returns the UNIX_EPOCH and the "+00:00" time zone.
//! let now = Now::new(EmptyHostSystem);
//! let time_zone = now.time_zone().unwrap();
//! assert_eq!(time_zone.identifier().unwrap(), "+00:00");
//! let now = Now::new(EmptyHostSystem);
//! assert_eq!(now.instant(), Instant::try_new(0));
//! # }
//! ```
//!
//! However, even in our above example, we cheated a bit. We were still relying on the
//! `compiled_data` feature flag that provided time zone data for us. Let's try again,
//! but this time without the feature flag.
//!
//! ```rust
//! # #[cfg(feature = "tzdb")] {
//! use temporal_rs::{Instant, now::Now, host::EmptyHostSystem};
//! use timezone_provider::tzif::CompiledTzdbProvider;
//!
//! let provider = CompiledTzdbProvider::default();
//!
//! // The empty host system is a system implementation HostHooks that always
//! // returns the UNIX_EPOCH and the "+00:00" time zone.
//! let now = Now::new(EmptyHostSystem);
//! let time_zone = now.time_zone_with_provider(&provider).unwrap();
//! assert_eq!(time_zone.identifier_with_provider(&provider).unwrap(), "+00:00");
//!
//! let now = Now::new(EmptyHostSystem);
//! assert_eq!(now.instant(), Instant::try_new(0));
//! # }
//! ```
//!
//! Now -- pun only partially intended -- we've successfully written a no-default-features
//! example with temporal_rs!
//!
//! ### What have we learned going over this all this?
//!
//! First, any API that has the suffix `_with_provider` is a power user API for supplying
//! a custom or specific time zone data provider. Furthermore, any API that has a
//! `_with_provider` suffix will also have a version without the suffix that automagically
//! provides time zone data for you.
//!
//! Finally, sourcing time zone data is a very scary (but fun!) business. If you're interested
//! in learning more, feel free to check out the `timezone_provider` crate!
//!
//! With any luck, this also highlights the general design of temporal_rs. It provides a
//! general usage API that aligns with the Temporal specification while also being
//! flexible enough to provide an power user to take control of their host system access
//! and time zone data sourcing as needed.
//!
//! ## Formatting
//!
//! temporal_rs adheres to Temporal grammar, which is a strict version of
//! [RFC9557's IXDTF][ixdtf]. RFC9557 is an update to RFC3339 that adds
//! extensions to the format.
//!
//! ## More information
//!
//! [`Temporal`][proposal] is the Stage 3 proposal for ECMAScript that
//! provides new JS objects and functions for working with dates and
//! times that fully supports time zones and non-gregorian calendars.
//!
//! This library's primary development source is the Temporal
//! Proposal [specification][spec].
//!
//! [ixdtf]: https://www.rfc-editor.org/rfc/rfc9557.txt
//! [proposal]: https://github.com/tc39/proposal-temporal
//! [spec]: https://tc39.es/proposal-temporal/
#![doc(
    html_logo_url = "https://raw.githubusercontent.com/boa-dev/boa/main/assets/logo.svg",
    html_favicon_url = "https://raw.githubusercontent.com/boa-dev/boa/main/assets/logo.svg"
)]
#![no_std]
#![cfg_attr(not(test), forbid(clippy::unwrap_used))]
// We wish to reduce the number of panics, .expect() must be justified
// to never occur. Opt to have GIGO or .temporal_unwrap() behavior where possible.
#![cfg_attr(not(test), warn(clippy::expect_used, clippy::indexing_slicing))]
#![allow(
    // Currently throws a false positive regarding dependencies that are only used in benchmarks.
    unused_crate_dependencies,
    clippy::module_name_repetitions,
    clippy::redundant_pub_crate,
    clippy::too_many_lines,
    clippy::cognitive_complexity,
    clippy::missing_errors_doc,
    clippy::let_unit_value,
    clippy::option_if_let_else,
)]
#![forbid(unsafe_code)]

extern crate alloc;
extern crate core;

#[cfg(any(test, feature = "std"))]
extern crate std;

pub mod error;
pub mod host;
pub mod iso;
pub mod options;
pub mod parsers;
pub mod primitive;
pub mod provider;

#[cfg(feature = "sys")]
pub(crate) mod sys;

mod builtins;

#[cfg(feature = "tzdb")]
pub(crate) mod tzdb;

#[doc(hidden)]
pub(crate) mod rounding;
#[doc(hidden)]
pub(crate) mod utils;

use core::cmp::Ordering;

// TODO: evaluate positives and negatives of using tinystr. Re-exporting
// tinystr as a convenience, as it is currently tied into the API.
/// Re-export of `TinyAsciiStr` from `tinystr`.
pub use tinystr::TinyAsciiStr;

/// The `Temporal` result type
pub type TemporalResult<T> = Result<T, TemporalError>;

#[doc(inline)]
pub use error::TemporalError;

#[cfg(feature = "sys")]
#[doc(inline)]
pub use sys::Temporal;

pub mod partial {
    //! Partial date and time component records
    //!
    //! The partial records are `temporal_rs`'s method of addressing
    //! `TemporalFields` in the specification.
    pub use crate::builtins::core::{
        PartialDate, PartialDateTime, PartialDuration, PartialTime, PartialYearMonth,
        PartialZonedDateTime,
    };
}

pub mod parsed_intermediates;

// TODO: Potentially bikeshed how `EpochNanoseconds` should be exported.
/// A module for structs related to the UNIX epoch
pub mod unix_time {
    pub use timezone_provider::epoch_nanoseconds::EpochNanoseconds;
}

/// The `Now` module includes type for building a Now
pub mod now {
    pub use crate::builtins::Now;
}

/// Duration related types
pub mod duration {
    pub use crate::builtins::DateDuration;
}

/// Calendar field records
pub mod fields {
    pub use crate::builtins::{
        calendar::{CalendarFields, YearMonthCalendarFields},
        DateTimeFields, ZonedDateTimeFields,
    };
}

// TODO: Should we be exporting MonthCode and UtcOffset here.
pub use crate::builtins::{
    calendar::{Calendar, MonthCode},
    core::time_zone::{TimeZone, UtcOffset},
    Duration, Instant, PlainDate, PlainDateTime, PlainMonthDay, PlainTime, PlainYearMonth,
    ZonedDateTime,
};

/// A library specific trait for unwrapping assertions.
pub(crate) trait TemporalUnwrap {
    type Output;

    /// `temporal_rs` based assertion for unwrapping. This will panic in
    /// debug builds, but throws error during runtime.
    fn temporal_unwrap(self) -> TemporalResult<Self::Output>;
}

impl<T> TemporalUnwrap for Option<T> {
    type Output = T;

    fn temporal_unwrap(self) -> TemporalResult<Self::Output> {
        debug_assert!(self.is_some());
        self.ok_or(TemporalError::assert())
    }
}

impl<T> TemporalUnwrap for TemporalResult<T> {
    type Output = T;

    fn temporal_unwrap(self) -> Self {
        debug_assert!(self.is_ok(), "Assertion failed: {:?}", self.err());
        self.map_err(|e| TemporalError::assert().with_message(e.into_message()))
    }
}

#[doc(hidden)]
#[macro_export]
macro_rules! temporal_assert {
    ($condition:expr $(,)*) => {
        if !$condition {
            return Err(TemporalError::assert());
        }
    };
    ($condition:expr, $($args:tt)+) => {
        if !$condition {
            #[cfg(feature = "log")]
            log::error!($($args)+);
            return Err(TemporalError::assert());
        }
    };
}

// TODO: Determine final home or leave in the top level? ops::Sign,
// types::Sign, etc.
/// A general Sign type.
#[repr(i8)]
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub enum Sign {
    #[default]
    Positive = 1,
    Zero = 0,
    Negative = -1,
}

impl From<i8> for Sign {
    fn from(value: i8) -> Self {
        match value.cmp(&0) {
            Ordering::Greater => Self::Positive,
            Ordering::Equal => Self::Zero,
            Ordering::Less => Self::Negative,
        }
    }
}

impl Sign {
    /// Coerces the current `Sign` to be either negative or positive.
    pub(crate) const fn as_sign_multiplier(&self) -> i8 {
        if matches!(self, Self::Zero) {
            return 1;
        }
        *self as i8
    }

    pub(crate) fn negate(&self) -> Sign {
        Sign::from(-(*self as i8))
    }
}

// Relevant numeric constants

/// Nanoseconds per day constant: 8.64e+13
#[doc(hidden)]
pub const NS_PER_DAY: u64 = MS_PER_DAY as u64 * 1_000_000;
#[doc(hidden)]
pub const NS_PER_DAY_NONZERO: core::num::NonZeroU128 =
    if let Some(nz) = core::num::NonZeroU128::new(NS_PER_DAY as u128) {
        nz
    } else {
        unreachable!()
    };
/// Milliseconds per day constant: 8.64e+7
#[doc(hidden)]
pub const MS_PER_DAY: u32 = 24 * 60 * 60 * 1000;
/// Max Instant nanosecond constant
#[doc(hidden)]
pub(crate) const NS_MAX_INSTANT: i128 = NS_PER_DAY as i128 * 100_000_000i128;
/// Min Instant nanosecond constant
#[doc(hidden)]
pub(crate) const NS_MIN_INSTANT: i128 = -NS_MAX_INSTANT;
