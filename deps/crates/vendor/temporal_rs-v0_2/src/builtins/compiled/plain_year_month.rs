use crate::{
    builtins::TZ_PROVIDER, unix_time::EpochNanoseconds, PlainYearMonth, TemporalResult, TimeZone,
};

impl PlainYearMonth {
    pub fn epoch_ns_for(&self, time_zone: TimeZone) -> TemporalResult<EpochNanoseconds> {
        self.epoch_ns_for_with_provider(time_zone, &*TZ_PROVIDER)
    }
}
