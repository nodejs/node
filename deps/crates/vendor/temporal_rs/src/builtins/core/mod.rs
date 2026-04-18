//! The primary date-time components provided by Temporal.
//!
//! The Temporal specification, along with this implementation aims to
//! provide full support for time zones and non-gregorian calendars that
//! are compliant with standards like ISO 8601, RFC 3339, and RFC 5545.

// TODO: Expand upon above introduction.

pub mod calendar;
pub mod duration;
pub mod time_zone;

mod instant;
mod plain_date;
mod plain_date_time;
mod plain_month_day;
mod plain_time;
mod plain_year_month;
pub(crate) mod zoned_date_time;

mod now;

#[doc(inline)]
pub use now::Now;

#[doc(inline)]
pub use duration::{DateDuration, Duration, PartialDuration};
#[doc(inline)]
pub use instant::Instant;
#[doc(inline)]
pub use plain_date::{PartialDate, PlainDate};
#[doc(inline)]
pub use plain_date_time::{DateTimeFields, PartialDateTime, PlainDateTime};
#[doc(inline)]
pub use plain_month_day::PlainMonthDay;
#[doc(inline)]
pub use plain_time::{PartialTime, PlainTime};
#[doc(inline)]
pub use plain_year_month::{PartialYearMonth, PlainYearMonth};
#[doc(inline)]
pub use zoned_date_time::{PartialZonedDateTime, ZonedDateTime, ZonedDateTimeFields};
