use crate::{
    builtins::TZ_PROVIDER,
    options::{RelativeTo, RoundingOptions, Unit},
    primitive::FiniteF64,
    Duration, TemporalResult,
};

use core::cmp::Ordering;

#[cfg(test)]
mod tests;

impl Duration {
    /// Rounds the current [`Duration`] according to the provided [`RoundingOptions`] and an optional
    /// [`RelativeTo`]
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn round(
        &self,
        options: RoundingOptions,
        relative_to: Option<RelativeTo>,
    ) -> TemporalResult<Self> {
        self.round_with_provider(options, relative_to, &*TZ_PROVIDER)
    }

    /// Returns the ordering between two [`Duration`], takes an optional
    /// [`RelativeTo`]
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn compare(
        &self,
        two: &Duration,
        relative_to: Option<RelativeTo>,
    ) -> TemporalResult<Ordering> {
        self.compare_with_provider(two, relative_to, &*TZ_PROVIDER)
    }

    pub fn total(&self, unit: Unit, relative_to: Option<RelativeTo>) -> TemporalResult<FiniteF64> {
        self.total_with_provider(unit, relative_to, &*TZ_PROVIDER)
    }
}
