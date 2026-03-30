//! Traits and struct for creating time zone data providers.
//!
//! This module contains the traits needed to implement a time zone data
//! provider along with relevant structs.

use core::str::FromStr;

use crate::utils;
use crate::CompiledNormalizer;
use crate::{epoch_nanoseconds::EpochNanoseconds, TimeZoneProviderError};
use alloc::borrow::Cow;

pub(crate) type TimeZoneProviderResult<T> = Result<T, TimeZoneProviderError>;

/// `UtcOffsetSeconds` represents the amount of seconds we need to add to the UTC to reach the local time.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct UtcOffsetSeconds(pub i64);

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct IsoDateTime {
    pub year: i32,
    pub month: u8,
    pub day: u8,
    pub hour: u8,
    pub minute: u8,
    pub second: u8,
    pub millisecond: u16,
    pub microsecond: u16,
    pub nanosecond: u16,
}

impl IsoDateTime {
    fn to_epoch_days(self) -> i64 {
        utils::epoch_days_from_gregorian_date(self.year, self.month, self.day)
    }
    /// `IsoTimeToEpochMs`
    ///
    /// Note: This method is library specific and not in spec
    ///
    /// Functionally the same as Date's `MakeTime`
    fn time_to_epoch_ms(self) -> i64 {
        ((i64::from(self.hour) * utils::MS_PER_HOUR
            + i64::from(self.minute) * utils::MS_PER_MINUTE)
            + i64::from(self.second) * 1000i64)
            + i64::from(self.millisecond)
    }

    /// Convert this datetime to nanoseconds since the Unix epoch
    pub fn as_nanoseconds(&self) -> EpochNanoseconds {
        let time_ms = self.time_to_epoch_ms();
        let epoch_ms = utils::epoch_days_to_epoch_ms(self.to_epoch_days(), time_ms);
        EpochNanoseconds(
            epoch_ms as i128 * 1_000_000
                + self.microsecond as i128 * 1_000
                + self.nanosecond as i128,
        )
    }
}

#[cfg(feature = "tzif")]
use tzif::data::{posix::TimeZoneVariantInfo, tzif::LocalTimeTypeRecord};

#[cfg(feature = "tzif")]
impl From<&TimeZoneVariantInfo> for UtcOffsetSeconds {
    fn from(value: &TimeZoneVariantInfo) -> Self {
        // The POSIX tz string stores offsets as negative offsets;
        // i.e. "seconds that must be added to reach UTC"
        Self(-value.offset.0)
    }
}

#[cfg(feature = "tzif")]
impl From<LocalTimeTypeRecord> for UtcOffsetSeconds {
    fn from(value: LocalTimeTypeRecord) -> Self {
        Self(value.utoff.0)
    }
}

/// An EpochNanoseconds and a UTC offset
#[derive(Copy, Clone, Debug, PartialEq)]
pub struct EpochNanosecondsAndOffset {
    /// The resolved nanoseconds value
    pub ns: EpochNanoseconds,
    /// The resolved time zone offset corresponding
    /// to the nanoseconds value, in the given time zone
    pub offset: UtcOffsetSeconds,
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum TransitionDirection {
    Next,
    Previous,
}

#[derive(Debug, Clone, Copy)]
pub struct ParseDirectionError;

impl core::fmt::Display for ParseDirectionError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.write_str("provided string was not a valid direction.")
    }
}

impl FromStr for TransitionDirection {
    type Err = ParseDirectionError;
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "next" => Ok(Self::Next),
            "previous" => Ok(Self::Previous),
            _ => Err(ParseDirectionError),
        }
    }
}

impl core::fmt::Display for TransitionDirection {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            Self::Next => "next",
            Self::Previous => "previous",
        }
        .fmt(f)
    }
}

/// Used in disambiguate_possible_epoch_nanos
///
/// When we have a LocalTimeRecordResult::Empty,
/// it is useful to know the offsets before and after.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct GapEntryOffsets {
    pub offset_before: UtcOffsetSeconds,
    pub offset_after: UtcOffsetSeconds,
    pub transition_epoch: EpochNanoseconds,
}

/// The potential candidates for a given local datetime
#[derive(Copy, Clone, Debug, PartialEq)]
pub enum CandidateEpochNanoseconds {
    Zero(GapEntryOffsets),
    One(EpochNanosecondsAndOffset),
    Two([EpochNanosecondsAndOffset; 2]),
}

impl CandidateEpochNanoseconds {
    pub fn as_slice(&self) -> &[EpochNanosecondsAndOffset] {
        match *self {
            Self::Zero(..) => &[],
            Self::One(ref one) => core::slice::from_ref(one),
            Self::Two(ref multiple) => &multiple[..],
        }
    }

    #[allow(unused)] // Used in tests in some feature configurations
    pub fn is_empty(&self) -> bool {
        matches!(*self, Self::Zero(..))
    }

    #[allow(unused)] // Used in tests in some feature configurations
    pub fn len(&self) -> usize {
        match *self {
            Self::Zero(..) => 0,
            Self::One(..) => 1,
            Self::Two(..) => 2,
        }
    }

    pub fn first(&self) -> Option<EpochNanosecondsAndOffset> {
        match *self {
            Self::Zero(..) => None,
            Self::One(one) | Self::Two([one, _]) => Some(one),
        }
    }

    pub fn last(&self) -> Option<EpochNanosecondsAndOffset> {
        match *self {
            Self::Zero(..) => None,
            Self::One(last) | Self::Two([_, last]) => Some(last),
        }
    }
}

#[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash, Default)]
pub struct TimeZoneId {
    pub normalized: NormalizedId,
    pub resolved: ResolvedId,
}

pub trait TimeZoneProvider {
    fn get(&self, ident: &[u8]) -> TimeZoneProviderResult<TimeZoneId>;
    fn identifier(&self, id: TimeZoneId) -> TimeZoneProviderResult<Cow<'_, str>>;

    fn canonicalized(&self, id: TimeZoneId) -> TimeZoneProviderResult<TimeZoneId>;

    fn candidate_nanoseconds_for_local_epoch_nanoseconds(
        &self,
        id: TimeZoneId,
        local_datetime: IsoDateTime,
    ) -> TimeZoneProviderResult<CandidateEpochNanoseconds>;

    fn transition_nanoseconds_for_utc_epoch_nanoseconds(
        &self,
        id: TimeZoneId,
        epoch_nanoseconds: i128,
    ) -> TimeZoneProviderResult<UtcOffsetSeconds>;

    fn get_time_zone_transition(
        &self,
        id: TimeZoneId,
        epoch_nanoseconds: i128,
        direction: TransitionDirection,
    ) -> TimeZoneProviderResult<Option<EpochNanoseconds>>;
}

/// An id for a resolved timezone, for use with a [`TimeZoneNormalizer`]
#[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash, Default)]
pub struct NormalizedId(pub usize);

/// A type capable of normalizing and canonicalizing time zones
pub trait TimeZoneNormalizer {
    // Needed for temporal support
    fn normalized(&self, _: &[u8]) -> TimeZoneProviderResult<NormalizedId>;

    // Support for Intl support and canonicalize-tz proposal
    fn canonicalized(&self, _: NormalizedId) -> TimeZoneProviderResult<NormalizedId>;

    fn identifier(&self, _: NormalizedId) -> TimeZoneProviderResult<&str>;
}

/// An id for a resolved timezone, for use with a [`TimeZoneResolver`]
#[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash, Default)]
pub struct ResolvedId(pub usize);

/// A type capable of resolving time zone data
pub trait TimeZoneResolver {
    fn get_id(&self, normalized_identifier: &[u8]) -> TimeZoneProviderResult<ResolvedId>;

    fn candidate_nanoseconds_for_local_epoch_nanoseconds(
        &self,
        identifier: ResolvedId,
        local_datetime: IsoDateTime,
    ) -> TimeZoneProviderResult<CandidateEpochNanoseconds>;

    fn transition_nanoseconds_for_utc_epoch_nanoseconds(
        &self,
        identifier: ResolvedId,
        epoch_nanoseconds: i128,
    ) -> TimeZoneProviderResult<UtcOffsetSeconds>;

    fn get_time_zone_transition(
        &self,
        identifier: ResolvedId,
        epoch_nanoseconds: i128,
        direction: TransitionDirection,
    ) -> TimeZoneProviderResult<Option<EpochNanoseconds>>;
}

/// A type that can both normalize and resolve, which implements [`TimeZoneProvider`]
#[derive(Default, Copy, Clone, Debug)]
pub struct NormalizerAndResolver<C, R> {
    resolver: R,
    canonicalizer: C,
}

impl<R> NormalizerAndResolver<CompiledNormalizer, R> {
    /// Create a new [`NormalizerAndResolver`] with a given resolver and compiled normalizer data
    pub fn new(resolver: R) -> Self {
        Self {
            resolver,
            canonicalizer: CompiledNormalizer,
        }
    }
}
impl<C: TimeZoneNormalizer, R: TimeZoneResolver> TimeZoneProvider for NormalizerAndResolver<C, R> {
    fn get(&self, ident: &[u8]) -> TimeZoneProviderResult<TimeZoneId> {
        let normalized = self.canonicalizer.normalized(ident)?;
        let normalized_id = self.canonicalizer.identifier(normalized)?;
        let resolved = self.resolver.get_id(normalized_id.as_bytes())?;

        Ok(TimeZoneId {
            normalized,
            resolved,
        })
    }

    fn identifier(&self, id: TimeZoneId) -> TimeZoneProviderResult<Cow<'_, str>> {
        self.canonicalizer.identifier(id.normalized).map(Into::into)
    }
    fn canonicalized(&self, id: TimeZoneId) -> TimeZoneProviderResult<TimeZoneId> {
        let canonical = self.canonicalizer.canonicalized(id.normalized)?;
        Ok(TimeZoneId {
            normalized: canonical,
            resolved: id.resolved,
        })
    }
    fn candidate_nanoseconds_for_local_epoch_nanoseconds(
        &self,
        id: TimeZoneId,
        local_datetime: IsoDateTime,
    ) -> TimeZoneProviderResult<CandidateEpochNanoseconds> {
        self.resolver
            .candidate_nanoseconds_for_local_epoch_nanoseconds(id.resolved, local_datetime)
    }

    fn transition_nanoseconds_for_utc_epoch_nanoseconds(
        &self,
        id: TimeZoneId,
        epoch_nanoseconds: i128,
    ) -> TimeZoneProviderResult<UtcOffsetSeconds> {
        self.resolver
            .transition_nanoseconds_for_utc_epoch_nanoseconds(id.resolved, epoch_nanoseconds)
    }

    fn get_time_zone_transition(
        &self,
        id: TimeZoneId,
        epoch_nanoseconds: i128,
        direction: TransitionDirection,
    ) -> TimeZoneProviderResult<Option<EpochNanoseconds>> {
        self.resolver
            .get_time_zone_transition(id.resolved, epoch_nanoseconds, direction)
    }
}

/// A [`TimeZoneNormalizer`] that always panics
#[derive(Copy, Clone, Debug, Default)]
pub struct NeverNormalizer;
/// A [`TimeZoneResolver`] that always panics
#[derive(Copy, Clone, Debug, Default)]
pub struct NeverResolver;

impl TimeZoneNormalizer for NeverNormalizer {
    fn normalized(&self, _: &[u8]) -> TimeZoneProviderResult<NormalizedId> {
        unimplemented!()
    }

    fn canonicalized(&self, _: NormalizedId) -> TimeZoneProviderResult<NormalizedId> {
        unimplemented!()
    }

    fn identifier(&self, _: NormalizedId) -> TimeZoneProviderResult<&str> {
        unimplemented!()
    }
}

impl TimeZoneResolver for NeverResolver {
    fn get_id(&self, _normalized_identifier: &[u8]) -> TimeZoneProviderResult<ResolvedId> {
        unimplemented!()
    }

    fn candidate_nanoseconds_for_local_epoch_nanoseconds(
        &self,
        _identifier: ResolvedId,
        _local_datetime: IsoDateTime,
    ) -> TimeZoneProviderResult<CandidateEpochNanoseconds> {
        unimplemented!()
    }

    fn transition_nanoseconds_for_utc_epoch_nanoseconds(
        &self,
        _identifier: ResolvedId,
        _epoch_nanoseconds: i128,
    ) -> TimeZoneProviderResult<UtcOffsetSeconds> {
        unimplemented!()
    }

    fn get_time_zone_transition(
        &self,
        _identifier: ResolvedId,
        _epoch_nanoseconds: i128,
        _direction: TransitionDirection,
    ) -> TimeZoneProviderResult<Option<EpochNanoseconds>> {
        unimplemented!()
    }
}

/// A [`TimeZoneProvider`] which just calls `unimplemented!()`
pub type NeverProvider = NormalizerAndResolver<NeverNormalizer, NeverResolver>;
