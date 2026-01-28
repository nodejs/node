use crate::CompiledNormalizer;
use core::str;
use zoneinfo64::{PossibleOffset, Zone, ZoneInfo64};

use crate::provider::{
    CandidateEpochNanoseconds, EpochNanosecondsAndOffset, GapEntryOffsets, IsoDateTime,
    NormalizerAndResolver, ResolvedId, TimeZoneProviderResult, TimeZoneResolver,
    TransitionDirection, UtcOffsetSeconds,
};
use crate::{
    epoch_nanoseconds::{seconds_to_nanoseconds, EpochNanoseconds, NS_IN_S},
    TimeZoneProviderError,
};
use zoneinfo64::UtcOffset;

impl From<UtcOffset> for UtcOffsetSeconds {
    fn from(other: UtcOffset) -> Self {
        Self(i64::from(other.seconds()))
    }
}

pub use zoneinfo64::ZONEINFO64_RES_FOR_TESTING;

/// A TimeZoneProvider that works using ICU4C zoneinfo64.res data
pub type ZoneInfo64TzdbProvider<'a> = NormalizerAndResolver<CompiledNormalizer, ZoneInfo64<'a>>;

impl ZoneInfo64TzdbProvider<'_> {
    /// Produces a zoneinfo64 provider using the builtin zoneinfo64 data,
    /// for testing use only. We do not provide strong guarantees for which version of zoneinfo64
    /// this will be.
    pub fn zoneinfo64_provider_for_testing() -> Option<Self> {
        let zi_data = zoneinfo64::ZoneInfo64::try_from_u32s(ZONEINFO64_RES_FOR_TESTING).ok()?;
        Some(ZoneInfo64TzdbProvider::new(zi_data))
    }
}

fn get<'a>(zi: &'a ZoneInfo64<'a>, id: ResolvedId) -> TimeZoneProviderResult<Zone<'a>> {
    let id = u16::try_from(id.0)
        .map_err(|_| TimeZoneProviderError::Range("Unknown timezone identifier"))?;
    Ok(Zone::from_raw_parts((id, zi)))
}

impl TimeZoneResolver for ZoneInfo64<'_> {
    fn get_id(&self, normalized_identifier: &[u8]) -> TimeZoneProviderResult<ResolvedId> {
        let utf8 = str::from_utf8(normalized_identifier)
            .map_err(|_| TimeZoneProviderError::Range("Unknown timezone identifier"))?;
        let zone = self
            .get(utf8)
            .ok_or(TimeZoneProviderError::Range("Unknown timezone identifier"))?;
        let (id, _) = zone.into_raw_parts();
        Ok(ResolvedId(usize::from(id)))
    }

    fn candidate_nanoseconds_for_local_epoch_nanoseconds(
        &self,
        identifier: ResolvedId,
        local_datetime: IsoDateTime,
    ) -> TimeZoneProviderResult<CandidateEpochNanoseconds> {
        let zone = get(self, identifier)?;
        let epoch_nanos = (local_datetime).as_nanoseconds();
        let possible_offset = zone.for_date_time(
            local_datetime.year,
            local_datetime.month,
            local_datetime.day,
            local_datetime.hour,
            local_datetime.minute,
            local_datetime.second,
        );

        let result = match possible_offset {
            PossibleOffset::None {
                before,
                after,
                transition,
            } => CandidateEpochNanoseconds::Zero(GapEntryOffsets {
                offset_before: before.offset.into(),
                offset_after: after.offset.into(),
                transition_epoch: EpochNanoseconds::from(seconds_to_nanoseconds(transition)),
            }),
            PossibleOffset::Single(o) => {
                let epoch_ns = EpochNanoseconds::from(
                    epoch_nanos.0 - seconds_to_nanoseconds(i64::from(o.offset.seconds())),
                );
                CandidateEpochNanoseconds::One(EpochNanosecondsAndOffset {
                    ns: epoch_ns,
                    offset: o.offset.into(),
                })
            }
            PossibleOffset::Ambiguous { before, after, .. } => {
                let first_epoch_ns = EpochNanoseconds::from(
                    epoch_nanos.0 - seconds_to_nanoseconds(i64::from(before.offset.seconds())),
                );
                let second_epoch_ns = EpochNanoseconds::from(
                    epoch_nanos.0 - seconds_to_nanoseconds(i64::from(after.offset.seconds())),
                );
                CandidateEpochNanoseconds::Two([
                    EpochNanosecondsAndOffset {
                        ns: first_epoch_ns,
                        offset: before.offset.into(),
                    },
                    EpochNanosecondsAndOffset {
                        ns: second_epoch_ns,
                        offset: after.offset.into(),
                    },
                ])
            }
        };
        Ok(result)
    }

    fn transition_nanoseconds_for_utc_epoch_nanoseconds(
        &self,
        identifier: ResolvedId,
        epoch_nanoseconds: i128,
    ) -> TimeZoneProviderResult<UtcOffsetSeconds> {
        let zone = get(self, identifier)?;

        let Ok(mut seconds) = i64::try_from(epoch_nanoseconds / NS_IN_S) else {
            return Err(TimeZoneProviderError::Range(
                "Epoch nanoseconds out of range",
            ));
        };
        // The rounding is inexact. Transitions are only at second
        // boundaries, so the offset at N s is the same as the offset at N.001,
        // but the offset at -Ns is not the same as the offset at -N.001,
        // the latter matches -N - 1 s instead.
        if seconds < 0 && epoch_nanoseconds % NS_IN_S != 0 {
            seconds -= 1;
        }
        let offset = zone.for_timestamp(seconds);

        Ok(offset.offset.into())
    }

    fn get_time_zone_transition(
        &self,
        identifier: ResolvedId,
        epoch_nanoseconds: i128,
        direction: TransitionDirection,
    ) -> TimeZoneProviderResult<Option<EpochNanoseconds>> {
        let zone = get(self, identifier)?;
        // We want div_floor behavior
        let div = epoch_nanoseconds.div_euclid(NS_IN_S);
        let Ok(seconds) = i64::try_from(div) else {
            return Err(TimeZoneProviderError::Range(
                "Epoch nanoseconds out of range",
            ));
        };

        let transition = match direction {
            TransitionDirection::Previous => {
                let seconds_is_exact = (epoch_nanoseconds % NS_IN_S) == 0;
                zone.prev_transition(
                    seconds,
                    seconds_is_exact,
                    /* require_offset_change */ true,
                )
            }
            TransitionDirection::Next => {
                zone.next_transition(seconds, /* require_offset_change */ true)
            }
        };

        Ok(transition.map(|transition| EpochNanoseconds::from_seconds(transition.since)))
    }
}
