//! The `TimeZoneProvider` trait.

pub use timezone_provider::provider::{
    CandidateEpochNanoseconds, EpochNanosecondsAndOffset, GapEntryOffsets, NeverProvider,
    ParseDirectionError, TimeZoneId, TimeZoneProvider, TransitionDirection, UtcOffsetSeconds,
};

#[cfg(feature = "compiled_data")]
pub use crate::builtins::TZ_PROVIDER as COMPILED_TZ_PROVIDER;
