//! Utility date and time equations for Temporal

pub(crate) use timezone_provider::utils::epoch_days_from_gregorian_date;

// NOTE: Potentially add more of tests.

// ==== Begin Date Equations ====

pub(crate) const MS_PER_HOUR: i64 = 3_600_000;
pub(crate) const MS_PER_MINUTE: i64 = 60_000;

pub(crate) use timezone_provider::utils::{
    epoch_days_to_epoch_ms, iso_days_in_month, ymd_from_epoch_milliseconds,
};

// ==== End Calendar Equations ====
