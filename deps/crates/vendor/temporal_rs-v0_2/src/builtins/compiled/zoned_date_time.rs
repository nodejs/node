use crate::builtins::zoned_date_time::ZonedDateTimeFields;
use crate::builtins::TZ_PROVIDER;
use crate::partial::PartialZonedDateTime;
use crate::provider::TransitionDirection;
use crate::{
    options::{
        DifferenceSettings, Disambiguation, DisplayCalendar, DisplayOffset, DisplayTimeZone,
        OffsetDisambiguation, Overflow, RoundingOptions, ToStringRoundingOptions,
    },
    Calendar, Duration, PlainTime, TemporalResult, TimeZone,
};
use crate::{Instant, ZonedDateTime};
use alloc::string::String;

impl core::fmt::Display for ZonedDateTime {
    /// The [`core::fmt::Display`] implementation for `ZonedDateTime`.
    ///
    /// Enable with the `compiled_data` feature flag.
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        let string = self.to_ixdtf_string(
            DisplayOffset::Auto,
            DisplayTimeZone::Auto,
            DisplayCalendar::Auto,
            ToStringRoundingOptions::default(),
        );
        debug_assert!(
            string.is_ok(),
            "A valid ZonedDateTime string with default options."
        );
        f.write_str(&string.map_err(|_| Default::default())?)
    }
}

// ==== Experimental TZ_PROVIDER calendar method implementations ====

/// Calendar method implementations for `ZonedDateTime`.
///
/// The following [`ZonedDateTime`] methods are feature gated behind the
/// `compiled_data` feature flag.
impl ZonedDateTime {
    // TODO: Update direction to correct option
    pub fn get_time_zone_transition(
        &self,
        direction: TransitionDirection,
    ) -> TemporalResult<Option<Self>> {
        self.get_time_zone_transition_with_provider(direction, &*TZ_PROVIDER)
    }

    /// Returns the hours in the day.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn hours_in_day(&self) -> TemporalResult<f64> {
        self.hours_in_day_with_provider(&*TZ_PROVIDER)
    }
}

// ==== Experimental TZ_PROVIDER method implementations ====

/// The primary `ZonedDateTime` method implementations.
///
/// The following [`ZonedDateTime`] methods are feature gated behind the
/// `compiled_data` feature flag.
impl ZonedDateTime {
    /// Creates a new valid `ZonedDateTime`.
    #[inline]
    pub fn try_new(nanos: i128, time_zone: TimeZone, calendar: Calendar) -> TemporalResult<Self> {
        Self::try_new_with_provider(nanos, time_zone, calendar, &*TZ_PROVIDER)
    }

    /// Creates a new valid `ZonedDateTime` with an ISO 8601 calendar.
    #[inline]
    pub fn try_new_iso(nanos: i128, time_zone: TimeZone) -> TemporalResult<Self> {
        Self::try_new_iso_with_provider(nanos, time_zone, &*TZ_PROVIDER)
    }

    /// Creates a new valid `ZonedDateTime` from an [`Instant`].
    #[inline]
    pub fn try_new_from_instant(
        instant: Instant,
        time_zone: TimeZone,
        calendar: Calendar,
    ) -> TemporalResult<Self> {
        Self::try_new_from_instant_with_provider(instant, time_zone, calendar, &*TZ_PROVIDER)
    }

    /// Creates a new valid `ZonedDateTime` from an [`Instant`] with an ISO 8601 calendar.
    #[inline]
    pub fn try_new_iso_from_instant(instant: Instant, time_zone: TimeZone) -> TemporalResult<Self> {
        Self::try_new_iso_from_instant_with_provider(instant, time_zone, &*TZ_PROVIDER)
    }

    #[inline]
    pub fn from_partial(
        partial: PartialZonedDateTime,
        overflow: Option<Overflow>,
        disambiguation: Option<Disambiguation>,
        offset_option: Option<OffsetDisambiguation>,
    ) -> TemporalResult<Self> {
        Self::from_partial_with_provider(
            partial,
            overflow,
            disambiguation,
            offset_option,
            &*crate::builtins::TZ_PROVIDER,
        )
    }

    #[inline]
    pub fn with(
        &self,
        fields: ZonedDateTimeFields,
        disambiguation: Option<Disambiguation>,
        offset_option: Option<OffsetDisambiguation>,
        overflow: Option<Overflow>,
    ) -> TemporalResult<Self> {
        self.with_with_provider(
            fields,
            disambiguation,
            offset_option,
            overflow,
            &*TZ_PROVIDER,
        )
    }

    /// Creates a new `ZonedDateTime` from the current `ZonedDateTime` with the provided `PlainTime`.
    ///
    /// combined with the provided `TimeZone`.
    pub fn with_plain_time(&self, time: Option<PlainTime>) -> TemporalResult<Self> {
        self.with_plain_time_and_provider(time, &*TZ_PROVIDER)
    }

    /// Creates a new `ZonedDateTime` from the current `ZonedDateTime`
    /// combined with the provided `TimeZone`.
    pub fn with_timezone(&self, timezone: TimeZone) -> TemporalResult<Self> {
        self.with_time_zone_with_provider(timezone, &*TZ_PROVIDER)
    }

    /// Adds a [`Duration`] to the current `ZonedDateTime`.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn add(&self, duration: &Duration, overflow: Option<Overflow>) -> TemporalResult<Self> {
        self.add_with_provider(duration, overflow, &*TZ_PROVIDER)
    }

    /// Subtracts a [`Duration`] to the current `ZonedDateTime`.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn subtract(
        &self,
        duration: &Duration,
        overflow: Option<Overflow>,
    ) -> TemporalResult<Self> {
        self.subtract_with_provider(duration, overflow, &*TZ_PROVIDER)
    }

    pub fn equals(&self, other: &Self) -> TemporalResult<bool> {
        self.equals_with_provider(other, &*TZ_PROVIDER)
    }

    /// Returns a [`Duration`] representing the period of time from this `ZonedDateTime` since the other `ZonedDateTime`.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn since(&self, other: &Self, options: DifferenceSettings) -> TemporalResult<Duration> {
        self.since_with_provider(other, options, &*TZ_PROVIDER)
    }

    /// Returns a [`Duration`] representing the period of time from this `ZonedDateTime` since the other `ZonedDateTime`.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn until(&self, other: &Self, options: DifferenceSettings) -> TemporalResult<Duration> {
        self.until_with_provider(other, options, &*TZ_PROVIDER)
    }

    /// Returns the start of day for the current `ZonedDateTime`.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn start_of_day(&self) -> TemporalResult<Self> {
        self.start_of_day_with_provider(&*TZ_PROVIDER)
    }

    /// Rounds this [`ZonedDateTime`] to the nearest value according to the given rounding options.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn round(&self, options: RoundingOptions) -> TemporalResult<Self> {
        self.round_with_provider(options, &*TZ_PROVIDER)
    }

    /// Returns a RFC9557 (IXDTF) string with the provided options.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn to_ixdtf_string(
        &self,
        display_offset: DisplayOffset,
        display_timezone: DisplayTimeZone,
        display_calendar: DisplayCalendar,
        options: ToStringRoundingOptions,
    ) -> TemporalResult<String> {
        self.to_ixdtf_string_with_provider(
            display_offset,
            display_timezone,
            display_calendar,
            options,
            &*TZ_PROVIDER,
        )
    }

    /// Attempts to parse and create a `ZonedDateTime` from an IXDTF formatted [`&str`].
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn from_utf8(
        source: &[u8],
        disambiguation: Disambiguation,
        offset_option: OffsetDisambiguation,
    ) -> TemporalResult<Self> {
        ZonedDateTime::from_utf8_with_provider(source, disambiguation, offset_option, &*TZ_PROVIDER)
    }
    /// Attempts to parse and create a `ZonedDateTime` from an IXDTF formatted [`&str`].
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn from_parsed(
        parsed: crate::parsed_intermediates::ParsedZonedDateTime,
        disambiguation: Disambiguation,
        offset_option: OffsetDisambiguation,
    ) -> TemporalResult<Self> {
        ZonedDateTime::from_parsed_with_provider(
            parsed,
            disambiguation,
            offset_option,
            &*TZ_PROVIDER,
        )
    }
}
