//! This module implements native Rust wrappers for the Temporal builtins.

mod duration;
mod instant;
mod now;
mod plain_date;
mod plain_date_time;
mod plain_month_day;
mod plain_year_month;
mod zoned_date_time;

mod options {
    use crate::{builtins::TZ_PROVIDER, options::RelativeTo, TemporalResult};

    impl RelativeTo {
        pub fn try_from_str(source: &str) -> TemporalResult<Self> {
            Self::try_from_str_with_provider(source, &*TZ_PROVIDER)
        }
    }
}
