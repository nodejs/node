use crate::{
    builtins::TZ_PROVIDER, unix_time::EpochNanoseconds, PlainMonthDay, TemporalResult, TimeZone,
};

impl PlainMonthDay {
    pub fn epoch_ns_for(&self, time_zone: TimeZone) -> TemporalResult<EpochNanoseconds> {
        self.epoch_ns_for_with_provider(time_zone, &*TZ_PROVIDER)
    }
}
