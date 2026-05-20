use crate::{builtins::TZ_PROVIDER, PlainDate, PlainTime, TemporalResult, TimeZone};

impl PlainDate {
    /// Converts a `Date` to a `ZonedDateTime` in the UTC time zone.
    pub fn to_zoned_date_time(
        &self,
        time_zone: TimeZone,
        plain_time: Option<PlainTime>,
    ) -> TemporalResult<crate::ZonedDateTime> {
        self.to_zoned_date_time_with_provider(time_zone, plain_time, &*TZ_PROVIDER)
    }
}
