// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! This crate contains utilities for working with ICU4C's zoneinfo64 format
//!
//! ```rust
//! # use zoneinfo64::{Offset, PossibleOffset, ZoneInfo64, UtcOffset};
//!
//! // Needs to be u32-aligned
//! let resb = resb::include_bytes_as_u32!("./data/zoneinfo64.res");
//! // Then we parse the data
//! let zoneinfo = ZoneInfo64::try_from_u32s(resb)
//!            .expect("Error processing resource bundle file");
//!
//! let pacific = zoneinfo.get("America/Los_Angeles").unwrap();
//! // Calculate the timezone offset for 2024-01-01
//! let offset = pacific.for_timestamp(1704067200000);
//! let offset_seven = UtcOffset::from_seconds(-7 * 3600);
//! assert_eq!(offset.offset, offset_seven);
//!
//! // Calculate possible offsets at 2025-11-02T01:00:00
//! // This is during a DST switchover and is ambiguous
//! let possible = pacific.for_date_time(2025, 11, 2, 1, 0, 0);
//! let offset_eight = UtcOffset::from_seconds(-8 * 3600);
//! assert_eq!(possible, PossibleOffset::Ambiguous {
//!     before: Offset { offset: offset_seven, rule_applies: true },
//!     after: Offset { offset: offset_eight, rule_applies: false },
//!     transition: 1762074000,
//! });
//! ```

use std::fmt::Debug;

use calendrical_calculations::rata_die::RataDie;
use potential_utf::PotentialUtf16;
use resb::binary::BinaryDeserializerError;

use icu_locale_core::subtags::Region;

#[cfg(feature = "chrono")]
mod chrono_impls;

mod rule;
use rule::*;
mod deserialize;

/// A bundled zoneinfo64.res that can be used for testing. No guarantee is made
/// as to the version in use; though we will try to keep it up to date.
pub const ZONEINFO64_RES_FOR_TESTING: &[u32] = resb::include_bytes_as_u32!("./data/zoneinfo64.res");

const EPOCH: RataDie = calendrical_calculations::iso::const_fixed_from_iso(1970, 1, 1);
const SECONDS_IN_UTC_DAY: i64 = 24 * 60 * 60;

/// An offset from UTC time (stored to seconds precision)
#[derive(Debug, Clone, Copy, PartialEq, Default)]
pub struct UtcOffset(i32);

impl UtcOffset {
    pub fn from_seconds(x: i32) -> Self {
        Self(x)
    }

    pub fn seconds(self) -> i32 {
        self.0
    }
}

#[derive(Debug)]
/// The primary type containing parsed ZoneInfo64 data
pub struct ZoneInfo64<'a> {
    // Invariant: non-empty
    zones: Vec<TzZone<'a>>,
    // Invariant: same size as zones
    names: Vec<&'a PotentialUtf16>,
    rules: Vec<TzRule>,
    // Invariant: same size as zones
    regions: Vec<Region>,
}

#[derive(Debug, Clone)]
enum TzZone<'a> {
    // The rule data is boxed here due to the large size difference between the
    // `TzDataRuleData` struct and `u32`. It's not strictly necessary.
    Table(Box<TzZoneData<'a>>),
    Int(u32),
}

#[derive(Clone)]
struct TzZoneData<'a> {
    /// Transitions before the epoch of i32::MIN
    trans_pre32: &'a [(i32, i32)],
    /// Transitions with epoch values that can fit in an i32
    trans: &'a [i32],
    /// Transitions after the epoch of i32::MAX
    trans_post32: &'a [(i32, i32)],
    /// Map to offset from transitions. Treat [trans_pre32, trans, trans_post32]
    /// as a single array and use its corresponding index into this to get the index
    /// in type_offsets. The index in type_offsets is the *new* offset after the
    /// matching transition
    type_map: &'a [u8],
    /// Offsets. First entry is standard time, second entry is offset from standard time (if any)
    type_offsets: &'a [(i32, i32)],
    /// An index into the Rules table,
    /// its standard_offset_seconds, and its starting year.
    final_rule_offset_year: Option<(u32, i32, i32)>,
    #[allow(dead_code)]
    links: &'a [u32],
}

impl Debug for TzZoneData<'_> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "TzZoneData {{ ")?;

        fn dbg_timestamp(f: &mut std::fmt::Formatter<'_>, t: i64) -> std::fmt::Result {
            #[cfg(feature = "chrono")]
            let t = chrono::DateTime::from_timestamp(t, 0).unwrap();
            write!(f, "{t:?}, ")
        }

        write!(f, "transitions/offsets: [")?;
        let (std, rule) = self.type_offsets[0];
        write!(f, "{:?}, ", (std as f64 / 3600.0, rule as f64 / 3600.0))?;
        let mut i = 0;
        for &(hi, lo) in self.trans_pre32 {
            dbg_timestamp(f, ((hi as u32 as u64) << 32 | (lo as u32 as u64)) as i64)?;
            let (std, rule) = self.type_offsets[self.type_map[i] as usize];
            write!(f, "{:?}, ", (std as f64 / 3600.0, rule as f64 / 3600.0))?;
            i += 1;
        }
        for &t in self.trans {
            dbg_timestamp(f, t as i64)?;
            let (std, rule) = self.type_offsets[self.type_map[i] as usize];
            write!(f, "{:?}, ", (std as f64 / 3600.0, rule as f64 / 3600.0))?;
            i += 1;
        }
        for &(hi, lo) in self.trans_post32 {
            dbg_timestamp(f, ((hi as u32 as u64) << 32 | (lo as u32 as u64)) as i64)?;
            let (std, rule) = self.type_offsets[self.type_map[i] as usize];
            write!(f, "{:?}, ", (std as f64 / 3600.0, rule as f64 / 3600.0))?;
            i += 1;
        }
        write!(f, "], ")?;

        write!(f, "}}")
    }
}

impl<'a> ZoneInfo64<'a> {
    /// Parse this object from 4-byte aligned data
    pub fn try_from_u32s(resb: &'a [u32]) -> Result<Self, BinaryDeserializerError> {
        crate::deserialize::deserialize(resb)
    }
    #[cfg(test)]
    fn is_alias(&self, iana: &str) -> bool {
        let Some(idx) = self
            .names
            .binary_search_by(|&n| n.chars().cmp(iana.chars()))
            .ok()
        else {
            return false;
        };

        #[expect(clippy::indexing_slicing)] // zones and names have the same length
        let zone = &self.zones[idx];

        matches!(zone, &TzZone::Int(_))
    }
    #[cfg(test)]
    fn iter(&'a self) -> impl Iterator<Item = Zone<'a>> {
        (0..self.names.len()).map(move |i| Zone::from_raw_parts((i as u16, self)))
    }

    /// Get data for a given IANA timezone id. Aliases are supported.
    pub fn get(&'a self, iana: &str) -> Option<Zone<'a>> {
        let idx = self
            .names
            .binary_search_by(|&n| n.chars().cmp(iana.chars()))
            .ok()?;

        #[expect(clippy::indexing_slicing)] // just validated
        let resolved_idx = if let TzZone::Int(i) = self.zones[idx] {
            i as u16
        } else {
            idx as u16
        };

        // idx is a valid index into info.names by binary search
        // zone_idx is a valid index into info.zones by the invariant that links don't point to links
        Some(Zone {
            idx: idx as u16,
            resolved_idx,
            info: self,
        })
    }
}

/// Data for a given time zone
#[derive(Clone, Copy)]
pub struct Zone<'a> {
    // a valid index into info.names
    idx: u16,
    // a resolved index into info.zones which points to a TzZone::Table
    resolved_idx: u16,
    info: &'a ZoneInfo64<'a>,
}

impl<'a> Zone<'a> {
    /// Decomposes this [`Zone`] into its raw parts, consisting
    /// of some state stored in a `u16`, and the associated [`ZoneInfo64`].
    ///
    /// See [`Self::from_raw_parts`] for the inverse operation.
    pub fn into_raw_parts(self) -> (u16, &'a ZoneInfo64<'a>) {
        (self.idx, self.info)
    }

    /// Recreates the [`Zone`] from raw parts.
    ///
    /// Returns garbage if `parts` was not obtained from [`Self::into_raw_parts`].
    pub fn from_raw_parts(parts: (u16, &'a ZoneInfo64<'a>)) -> Self {
        let (idx, info) = parts;
        // info.zones.len() > 0 by invariant, so this doesn't underflow ...
        let idx = core::cmp::min(info.zones.len() - 1, idx as usize);
        #[expect(clippy::indexing_slicing)] // ... and idx is a valid index
        let resolved_idx = if let TzZone::Int(i) = info.zones[idx] {
            i as u16
        } else {
            idx as u16
        };

        // idx is a valid index into info.names by safety invariant
        // zone_idx is a valid index into info.zones by the invariant that links don't point to links
        Self {
            idx: idx as u16,
            resolved_idx,
            info,
        }
    }
}

impl Debug for Zone<'_> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Zone")
            .field("simple", self.simple())
            .field("rule", &self.simple().final_rule(&self.info.rules))
            .field("name", &self.name().chars().collect::<String>())
            .field("region", &self.region())
            .finish()
    }
}

impl PartialEq for Zone<'_> {
    fn eq(&self, other: &Self) -> bool {
        self.name() == other.name()
            && self.region() == other.region()
            && self.simple().trans == other.simple().trans
            && self.simple().trans_post32 == other.simple().trans_post32
            && self.simple().trans_pre32 == other.simple().trans_pre32
            && self.simple().type_map == other.simple().type_map
            && self.simple().type_offsets == other.simple().type_offsets
            && self.simple().links == other.simple().links
            && self.simple().final_rule(&self.info.rules)
                == other.simple().final_rule(&other.info.rules)
    }
}

/// A resolved offset for a given point in time
#[derive(Debug, Clone, Copy, PartialEq, Default)]
pub struct Offset {
    /// The offset from UTC of this time zone
    pub offset: UtcOffset,
    /// Whether or not the Rule (i.e. "non standard" time) applies
    pub rule_applies: bool,
}

/// A transition
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Transition {
    /// When the transition starts
    pub since: i64,
    /// The offset from UTC after this transition
    pub offset: UtcOffset,
    /// Whether or not the rule (i.e. "non standard" time) applies
    pub rule_applies: bool,
}

impl From<Transition> for Offset {
    fn from(other: Transition) -> Self {
        Self {
            offset: other.offset,
            rule_applies: other.rule_applies,
        }
    }
}

/// Possible offsets for a local datetime
#[derive(Debug, PartialEq)]
pub enum PossibleOffset {
    /// There is a single possible offset
    Single(Offset),
    /// There are multiple possible offsets, because we are inside a backward transition
    ///
    /// Note: Temporal requires these to be in ascending order of offset, Temporal consumers should sort them
    // <https://tc39.es/proposal-temporal/#sec-getnamedtimezoneepochnanoseconds>
    Ambiguous {
        /// The offset before the transition
        before: Offset,
        /// The offset after the transition
        after: Offset,
        /// The transition epoch in seconds
        transition: i64,
    },
    /// There is no possible offset, because we are at a forward transition
    None {
        /// The offset before this transition
        ///
        /// This is useful when performing fallback behavior on hitting a
        /// transition where the local time has a gap.
        before: Offset,
        /// The offset after this transition
        after: Offset,
        /// The transition epoch in seconds
        transition: i64,
    },
}

impl<'a> TzZoneData<'a> {
    /// Returns the index of the previous transition.
    ///
    /// As this can be -1, it returns an isize.
    ///
    /// Does not consider rule transitions
    fn prev_transition_offset_idx(&self, seconds_since_epoch: i64) -> isize {
        if seconds_since_epoch < i32::MIN as i64 {
            self.trans_pre32
                .binary_search(&(
                    (seconds_since_epoch >> 32) as i32,
                    (seconds_since_epoch & 0xFFFFFFFF) as i32,
                ))
                .map(|i| i as isize)
                // binary_search returns the index of the next (higher) transition, so we subtract one
                .unwrap_or_else(|i| i as isize - 1)
        } else if seconds_since_epoch <= i32::MAX as i64 {
            self.trans_pre32.len() as isize
                + self
                    .trans
                    .binary_search(&(seconds_since_epoch as i32))
                    .map(|i| i as isize)
                    // binary_search returns the index of the next (higher) transition, so we subtract one
                    .unwrap_or_else(|i| i as isize - 1)
        } else {
            self.trans_pre32.len() as isize
                + self.trans.len() as isize
                + self
                    .trans_post32
                    .binary_search(&(
                        (seconds_since_epoch >> 32) as i32,
                        (seconds_since_epoch & 0xFFFFFFFF) as i32,
                    ))
                    .map(|i| i as isize)
                    // binary_search returns the index of the next (higher) transition, so we subtract one
                    .unwrap_or_else(|i| i as isize - 1)
        }
    }

    fn transition_count(&self) -> isize {
        self.type_map.len() as isize
    }

    /// Gets the information for the transition offset at idx.
    ///
    /// Invariant: idx must be in-range for the transitions table. It is allowed to be 0
    /// when the table is empty, and it is allowed to be -1 to refer to the offsets before the transitions table.
    ///
    /// Does not handle rule transitions
    fn transition_offset_at(&self, idx: isize) -> Transition {
        // before first transition don't use `type_map`, just the first entry in `type_offsets`
        if idx < 0 || self.type_map.is_empty() {
            #[expect(clippy::unwrap_used)] // type_offsets non-empty by invariant
            let &(standard, rule_additional) = self.type_offsets.first().unwrap();
            return Transition {
                since: i64::MIN,
                offset: UtcOffset(standard + rule_additional),
                rule_applies: rule_additional > 0,
            };
        }

        debug_assert!(idx < self.transition_count(), "Called transition_offset_at with out-of-range index (got {idx}, but only have {} transitions)", self.transition_count());

        let idx = core::cmp::min(idx, self.transition_count() - 1);

        let idx = idx as usize;

        #[expect(clippy::indexing_slicing)]
        // type_map has length sum(trans*), and type_map values are validated to be valid indices in type_offsets
        let (standard, rule_additional) = self.type_offsets[self.type_map[idx] as usize];

        #[expect(clippy::indexing_slicing)] // by guards or invariant
        let since = if idx < self.trans_pre32.len() {
            let (hi, lo) = self.trans_pre32[idx];
            ((hi as u32 as u64) << 32 | (lo as u32 as u64)) as i64
        } else if idx - self.trans_pre32.len() < self.trans.len() {
            self.trans[idx - self.trans_pre32.len()] as i64
        } else {
            let (hi, lo) = self.trans_post32[idx - self.trans_pre32.len() - self.trans.len()];
            ((hi as u32 as u64) << 32 | (lo as u32 as u64)) as i64
        };

        Transition {
            since,
            offset: UtcOffset(standard + rule_additional),
            rule_applies: rule_additional > 0,
        }
    }

    fn final_rule(&self, rules: &'a [TzRule]) -> Option<Rule<'a>> {
        #[expect(clippy::indexing_slicing)] // rules indices are all valid
        self.final_rule_offset_year
            .map(|(idx, standard_offset_seconds, start_year)| Rule {
                start_year,
                standard_offset_seconds,
                inner: &rules[idx as usize],
            })
    }
}

impl<'a> Zone<'a> {
    fn simple(&self) -> &'a TzZoneData<'a> {
        #[expect(clippy::indexing_slicing)] // invariant
        let TzZone::Table(ref zone) = &self.info.zones[self.resolved_idx as usize] else {
            unreachable!() // invariant
        };

        zone
    }

    /// Get the possible offsets for a local datetime.
    pub fn for_date_time(
        &self,
        year: i32,
        month: u8,
        day: u8,
        hour: u8,
        minute: u8,
        second: u8,
    ) -> PossibleOffset {
        let day_before_year = calendrical_calculations::iso::day_before_year(year);
        let seconds_since_local_epoch = (day_before_year
            + calendrical_calculations::iso::days_before_month(year, month) as i64
            + day as i64
            - EPOCH)
            * SECONDS_IN_UTC_DAY
            + ((hour as i64 * 60 + minute as i64) * 60 + second as i64);

        let simple = self.simple();
        let rule = simple
            .final_rule_offset_year
            .filter(|&(_, _, start_year)| year >= start_year)
            .map(|(idx, standard_offset_seconds, start_year)| Rule {
                    start_year,
                    standard_offset_seconds,
                    #[expect(clippy::indexing_slicing)] // rules indices are all valid
                    inner: &self.info.rules[idx as usize],
                });
        let mut idx = 0;

        // Compute the candidate transition and the offset that was used before the transition (which is
        // required to validated times around the first transition).
        // This is either from the rule or the transitions.
        let (before_first_candidate, first_candidate) = if let Some(rule) = rule {
            // The rule applies and we use this year's first transition as the first candidate.
            let (before, candidate) = rule.transition(year, day_before_year, false);
            (Some(before), candidate)
        } else {
            // Pretend date time is UTC to get a candidate
            idx = simple.prev_transition_offset_idx(seconds_since_local_epoch);

            // We use the transition before the "timestamp" as the first candidate.
            //
            // We do not need to check transitions that are further back or forward;
            // since the data does not have any duplicate transitions (`test_monotonic_transition_times`),
            // and we know that prior transitions are far enough away that there is no chance of their
            // wall times overlapping (`test_transition_local_times_do_not_overlap`)
            (
                (idx >= 0).then(|| simple.transition_offset_at(idx - 1).into()),
                simple.transition_offset_at(idx),
            )
        };

        // There's only an actual transition into `first_candidate` if there's an offset before it.
        if let Some(before_first_candidate) = before_first_candidate {
            let wall_before = first_candidate.since + i64::from(before_first_candidate.offset.0);
            let wall_after = first_candidate.since + i64::from(first_candidate.offset.0);

            match (
                seconds_since_local_epoch < wall_before,
                seconds_since_local_epoch < wall_after,
            ) {
                // We are before the first transition entirely
                (true, true) => return PossibleOffset::Single(before_first_candidate),
                // We are within the first candidate's transition
                (true, false) => {
                    // This is impossible: if the candidates are equal then
                    // there can be no repeated local times.
                    debug_assert_ne!(before_first_candidate.offset, first_candidate.offset);

                    return PossibleOffset::Ambiguous {
                        before: before_first_candidate,
                        after: first_candidate.into(),
                        transition: first_candidate.since,
                    };
                }
                // We are in the first candidate's gap
                (false, true) => {
                    return PossibleOffset::None {
                        before: before_first_candidate,
                        after: first_candidate.into(),
                        transition: first_candidate.since,
                    }
                }
                // We are after the first candidate, try the second
                (false, false) => {}
            }
        }

        let second_candidate = if let Some(rule) = rule {
            // The rule applies and we use this year's second transition as the second candidate.
            Some(rule.transition(year, day_before_year, true).1)
        } else {
            // We use the transition after the "timestamp" as the second candidate.
            (idx + 1 < simple.transition_count()).then(|| simple.transition_offset_at(idx + 1))
        };

        if let Some(second_candidate) = second_candidate {
            let wall_before = second_candidate.since + i64::from(first_candidate.offset.0);
            let wall_after = second_candidate.since + i64::from(second_candidate.offset.0);

            match (
                seconds_since_local_epoch < wall_before,
                seconds_since_local_epoch < wall_after,
            ) {
                // We are before the second transition entirely
                (true, true) => return PossibleOffset::Single(first_candidate.into()),
                // We are within the second candidate's transition
                (true, false) => {
                    // This is impossible: if the candidates are equal then
                    // there can be no repeated local times.
                    debug_assert_ne!(first_candidate.offset, second_candidate.offset);

                    return PossibleOffset::Ambiguous {
                        before: first_candidate.into(),
                        after: second_candidate.into(),
                        transition: second_candidate.since,
                    };
                }
                // We are in the second candidate's gap
                (false, true) => {
                    return PossibleOffset::None {
                        before: first_candidate.into(),
                        after: second_candidate.into(),
                        transition: second_candidate.since,
                    }
                }
                // We are after the second candidate
                (false, false) => return PossibleOffset::Single(second_candidate.into()),
            }
        }

        PossibleOffset::Single(first_candidate.into())
    }

    /// Get the offset for a timestamp (as seconds since the Unix epoch).
    pub fn for_timestamp(&self, seconds_since_epoch: i64) -> Offset {
        let simple = self.simple();
        let idx = simple.prev_transition_offset_idx(seconds_since_epoch);
        // If the previous transition is the last one then we need to check
        // against the rule
        if idx == simple.transition_count() - 1 {
            if let Some(rule) = simple.final_rule(&self.info.rules) {
                if let Some(resolved) = rule.for_timestamp(seconds_since_epoch) {
                    return resolved;
                }
            }
        }
        simple.transition_offset_at(idx).into()
    }

    /// Returns the latest transition with a `since` field
    /// strictly less than `seconds_since_epoch` (if `seconds_exact == true`),
    /// or less or equal than `seconds_since_epoch` (if `seconds_exact == false`).
    ///
    /// `seconds_exact` can be used if the actual timestamp has more precision
    /// than seconds. Make sure to round in the direction of negative infinity,
    /// i.e. use `div_euclid` to remove precision and `rem_euclid` to determine
    /// `seconds_exact`.
    ///
    /// `require_offset_change` can be used to skip transitions where the offset
    /// does not change (i.e. where only the `rule_applies` flag changes).
    pub fn prev_transition(
        &self,
        seconds_since_epoch: i64,
        seconds_exact: bool,
        require_offset_change: bool,
    ) -> Option<Transition> {
        let simple = self.simple();
        let mut idx = simple.prev_transition_offset_idx(seconds_since_epoch);

        // `self.transition_offset_at()` returns a synthetic transition at `i64::MIN`
        // for the time range before the actual first transition.
        if idx == -1 {
            return None;
        }

        if idx == simple.transition_count() - 1 {
            // If the previous transition is the last one then we need to check
            // against the rule
            if let Some(rule) = simple.final_rule(&self.info.rules) {
                if let Some(resolved) = rule.prev_transition(seconds_since_epoch, seconds_exact) {
                    return Some(resolved);
                }
            }
        }

        let mut candidate = simple.transition_offset_at(idx);
        if candidate.since == seconds_since_epoch && seconds_exact {
            // If the transition is an exact hit, we actually want the one before
            if idx <= 0 {
                return None;
            }
            idx -= 1;
            candidate = simple.transition_offset_at(idx);
        }

        while require_offset_change && idx > 0 {
            let prev = simple.transition_offset_at(idx - 1);
            if prev.offset == candidate.offset {
                candidate = prev;
                idx -= 1;
            } else {
                break;
            }
        }

        Some(candidate)
    }

    /// Returns the earliest transition with a `since` field
    /// strictly greater than `seconds_since_epoch`.
    ///
    /// `require_offset_change` can be used to skip transitions where the offset
    /// does not change (i.e. where only the `rule_applies` flag changes).
    pub fn next_transition(
        &self,
        seconds_since_epoch: i64,
        require_offset_change: bool,
    ) -> Option<Transition> {
        let simple = self.simple();
        let mut idx = simple.prev_transition_offset_idx(seconds_since_epoch);
        while require_offset_change
            && idx < simple.transition_count() - 1
            && simple.transition_offset_at(idx).offset
                == simple.transition_offset_at(idx + 1).offset
        {
            idx += 1;
        }

        Some(if idx == simple.transition_count() - 1 {
            // If the previous transition is the last one then the next one
            // can only be the rule.
            simple
                .final_rule(&self.info.rules)?
                .next_transition(seconds_since_epoch)
        } else {
            simple.transition_offset_at(idx + 1)
        })
    }

    // Returns the name of the timezone
    pub fn name(&self) -> &'a PotentialUtf16 {
        #[expect(clippy::indexing_slicing)] // idx is a valid index into info.names
        self.info.names[self.idx as usize]
    }

    // Returns the region of the timezone
    pub fn region(&self) -> Region {
        #[expect(clippy::indexing_slicing)]
        // idx is a valid index into info.names, which has the same length as info.regions
        self.info.regions[self.idx as usize]
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use chrono_tz::Tz;
    use itertools::Itertools;
    use std::{str::FromStr, sync::LazyLock};

    pub(crate) static TZDB: LazyLock<ZoneInfo64> = LazyLock::new(|| {
        ZoneInfo64::try_from_u32s(ZONEINFO64_RES_FOR_TESTING)
            .expect("Error processing resource bundle file")
    });

    /// Tests an invariant we rely on in our code
    #[test]
    fn test_monotonic_transition_times() {
        for chrono in time_zones_to_test() {
            let iana = chrono.name();
            let zoneinfo64 = TZDB.get(iana).unwrap().simple();

            for (prev, curr) in (-1..zoneinfo64.transition_count())
                .map(|idx| zoneinfo64.transition_offset_at(idx))
                .tuple_windows::<(_, _)>()
            {
                assert!(
                    prev.since < curr.since,
                    "{iana}: Transition times should be strictly increasing ({prev:?}, {curr:?})"
                );
            }
        }
    }

    /// Tests an invariant we rely on in our code
    #[test]
    fn test_transition_local_times_do_not_overlap() {
        for chrono in time_zones_to_test() {
            let iana = chrono.name();
            let zoneinfo64 = TZDB.get(iana).unwrap().simple();

            for (prev, curr) in (-1..zoneinfo64.transition_count())
                .map(|idx| zoneinfo64.transition_offset_at(idx))
                .tuple_windows::<(_, _)>()
            {
                let prev_wall = prev.since.saturating_add(i64::from(prev.offset.0));
                let curr_wall = curr.since.saturating_add(i64::from(curr.offset.0));

                assert!(
                    prev_wall < curr_wall,
                    "{iana}: Transitions should not be so close as to create a ambiguity ({prev:?}, {curr:?}"
                );
            }
        }
    }

    pub(crate) fn time_zones_to_test() -> impl Iterator<Item = Tz> {
        chrono_tz::TZ_VARIANTS
            .iter()
            .copied()
            .filter(|tz| !TZDB.is_alias(tz.name()))
    }

    fn has_rearguard_diff(iana: &str) -> bool {
        matches!(
            iana,
            "Africa/Casablanca"
                | "Africa/El_Aaiun"
                | "Africa/Windhoek"
                | "Europe/Dublin"
                | "Europe/Prague"
        )
    }

    #[test]
    fn test_against_chrono() {
        use chrono::Offset;
        use chrono::TimeZone;
        use chrono_tz::OffsetComponents;

        for chrono in time_zones_to_test() {
            let iana = chrono.name();

            let zoneinfo64 = TZDB.get(iana).unwrap();

            for seconds_since_epoch in transitions(iana, false)
                .into_iter()
                // 30-minute increments around a transition
                .flat_map(|t| (-3..=3).map(move |h| t.since + h * 30 * 60))
            {
                let utc_datetime = chrono::DateTime::from_timestamp(seconds_since_epoch, 0)
                    .unwrap()
                    .naive_utc();

                let zoneinfo64_date = zoneinfo64.from_utc_datetime(&utc_datetime);
                let chrono_date = chrono.from_utc_datetime(&utc_datetime);
                assert_eq!(
                    zoneinfo64_date.offset().fix(),
                    chrono_date.offset().fix(),
                    "{seconds_since_epoch}, {iana:?}",
                );

                let local_datetime = chrono_date.naive_local();
                assert_eq!(
                    zoneinfo64
                        .offset_from_local_datetime(&local_datetime)
                        .map(|o| o.fix()),
                    chrono
                        .offset_from_local_datetime(&local_datetime)
                        .map(|o| o.fix()),
                    "{seconds_since_epoch}, {zoneinfo64:?} {local_datetime}",
                );

                if has_rearguard_diff(iana) {
                    continue;
                }

                assert_eq!(
                    zoneinfo64_date.offset().rule_applies(),
                    !chrono_date.offset().dst_offset().is_zero(),
                    "{seconds_since_epoch}, {iana:?}",
                );
            }
        }
    }

    fn transitions(iana: &str, require_offset_change: bool) -> Vec<Transition> {
        let tz = jiff::tz::TimeZone::get(iana).unwrap();
        let mut transitions = tz
            // Chrono only evaluates rules until 2100
            .preceding(jiff::Timestamp::from_str("2100-01-01T00:00:00Z").unwrap())
            .map(|t| Transition {
                since: t.timestamp().as_second(),
                offset: UtcOffset(t.offset().seconds()),
                rule_applies: t.dst().is_dst(),
            })
            .collect::<Vec<_>>();

        transitions.reverse();

        // jiff returns transitions also if only the name changes, we don't
        transitions.retain(|t| {
            let before = tz.to_offset_info(jiff::Timestamp::from_second(t.since - 1).unwrap());
            if require_offset_change {
                before.offset().seconds() != t.offset.0
            } else {
                before.offset().seconds() != t.offset.0
                || before.dst().is_dst() != t.rule_applies
                // This is a super weird transition that would be removed by our rule,
                // but we want to keep it because it's in zoneinfo64.
                // 1944-04-03T01:00:00Z, (1.0, 1.0)
                // 1944-08-24T22:00:00Z, (0.0, 2.0) <- same offset and also DST
                // 1944-10-07T23:00:00Z, (0.0, 1.0)
                || (iana == "Europe/Paris" && t.since == -800071200)
            }
        });

        transitions
    }

    #[test]
    fn test_transition_against_jiff() {
        for (zone, require_offset_change) in
            time_zones_to_test().cartesian_product([true, false].into_iter())
        {
            let iana = zone.name();
            let transitions = transitions(iana, require_offset_change);

            if has_rearguard_diff(iana) || transitions.is_empty() {
                continue;
            }

            // TODO: investigate why these zones don't work with jiff/tzdb-bundle-always
            if matches!(
                iana,
                "America/Ciudad_Juarez"
                    | "America/Indiana/Petersburg"
                    | "America/Indiana/Vincennes"
                    | "America/Indiana/Winamac"
                    | "America/Metlakatla"
                    | "America/North_Dakota/Beulah"
            ) {
                continue;
            }

            let zoneinfo64 = TZDB.get(iana).unwrap();

            assert_eq!(
                zoneinfo64.prev_transition(i64::MIN + 1, true, require_offset_change),
                None
            );
            assert_eq!(
                zoneinfo64.prev_transition(i64::MIN + 1, false, require_offset_change),
                None
            );

            assert_eq!(
                zoneinfo64.next_transition(i64::MIN, true),
                transitions.first().copied()
            );

            for ts in transitions.windows(2) {
                let &[prev, curr] = ts else { unreachable!() };

                assert_eq!(
                    zoneinfo64.prev_transition(curr.since - 1, true, require_offset_change),
                    Some(prev)
                );
                assert_eq!(
                    zoneinfo64.prev_transition(curr.since - 1, false, require_offset_change),
                    Some(prev)
                );
                assert_eq!(
                    zoneinfo64.prev_transition(curr.since, true, require_offset_change),
                    Some(prev)
                );

                assert_eq!(
                    zoneinfo64.prev_transition(curr.since, false, require_offset_change),
                    Some(curr)
                );
                assert_eq!(
                    zoneinfo64.prev_transition(curr.since + 1, true, require_offset_change),
                    Some(curr)
                );
                assert_eq!(
                    zoneinfo64.prev_transition(curr.since + 1, false, require_offset_change),
                    Some(curr)
                );

                assert_eq!(
                    zoneinfo64.next_transition(prev.since - 1, require_offset_change),
                    Some(prev)
                );
                assert_eq!(
                    zoneinfo64.next_transition(prev.since, require_offset_change),
                    Some(curr),
                );
                assert_eq!(
                    zoneinfo64.next_transition(prev.since + 1, require_offset_change),
                    Some(curr)
                )
            }

            if zoneinfo64.simple().final_rule_offset_year.is_none() {
                assert_eq!(
                    zoneinfo64
                        .next_transition(transitions.last().unwrap().since, require_offset_change),
                    None
                );
            }
        }
    }

    #[test]
    fn test_raw() {
        // non-alias zone without a rule
        let simple_zone = TZDB
            .zones
            .iter()
            .position(|z| match z {
                TzZone::Table(z) => z.final_rule_offset_year.is_none(),
                _ => false,
            })
            .unwrap();

        let other_tzdb = ZoneInfo64 {
            zones: vec![TZDB.zones[simple_zone].clone()],
            names: vec![TZDB.names[simple_zone]],
            rules: vec![],
            regions: vec![TZDB.regions[simple_zone]],
        };

        for zone in TZDB.iter() {
            // Rountrips if we observe the precondition
            assert_eq!(zone, Zone::from_raw_parts(zone.into_raw_parts()));

            // If we mess with the state, we get garbage (which sometimes
            // is just the right garbage)
            if zone.idx as usize != TZDB.zones.len() - 1 {
                assert_ne!(
                    zone,
                    Zone::from_raw_parts((zone.into_raw_parts().0 + 1, &TZDB)),
                );
            }
            if zone.idx as usize != simple_zone {
                assert_ne!(
                    zone,
                    Zone::from_raw_parts((zone.into_raw_parts().0, &other_tzdb))
                );
            }
        }
    }
}
