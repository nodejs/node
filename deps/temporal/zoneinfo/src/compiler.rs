//! Zone info compiler functionality
//!
//! This module contains the zone info compiler logic along
//! with output types.
//!

use alloc::collections::BTreeSet;
use alloc::string::String;
use hashbrown::HashMap;

#[derive(Debug, Clone, PartialEq)]
pub struct LocalTimeRecord {
    pub offset: i64,
    pub saving: Time,
    pub letter: Option<String>,
    pub designation: String, // AKA format / abbr
}

// TODO: improve `Transition` repr this type provides a lot of
// information by design, but the local time record data
// should be separated from the transition info with a clear
// separation.
//
// EX:
// pub struct Transition {
//     /// The time to transition at
//     pub at_time: i64,
//     /// The transition time kind.
//     pub time_type: QualifiedTimeKind,
//     /// LocalTimeRecord transitioned into
//     pub to_local: ZoneInfoLocalTimeRecord,
// }
//
/// The primary transition data.
#[derive(Debug, Clone, PartialEq, Eq)]
#[non_exhaustive]
pub struct Transition {
    /// The time to transition at
    ///
    /// This represents the time in Unix Epoch seconds
    /// at which a transition should occur.
    pub at_time: i64,
    /// The transition time kind.
    ///
    /// Whether the transition was specified in Local, Standard, or Universal time.
    pub time_type: QualifiedTimeKind,

    // TODO: Below are fields that should be split into a
    // currently non-existent LocalTime record.
    /// The offset of the transition.
    pub offset: i64,
    /// Whether the transition is a savings offset or not
    ///
    /// This flag corresponds to the `is_dst` flag
    pub dst: bool,
    /// The savings for the local time record
    ///
    /// This field represents the exact [`Time`] value
    /// used for savings.
    pub savings: Time,
    /// The letter designation for the local time record
    ///
    /// The LETTER designation used in the fully formatted
    /// abbreviation
    pub letter: Option<String>,
    /// The abbreviation format for the local time record.
    pub format: String,
}

impl Ord for Transition {
    fn cmp(&self, other: &Self) -> core::cmp::Ordering {
        self.at_time.cmp(&other.at_time)
    }
}

impl PartialOrd for Transition {
    fn partial_cmp(&self, other: &Self) -> Option<core::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

/// `CompiledTransitions` is the complete compiled transition data
/// for one zone.
///
/// The compiled transition data contains an initial local time record, an ordered
/// set of transition data, and a POSIX time zone string.
///
/// In general, this struct offers the required data in a consummable format
/// for anyone who compiled zoneinfo data.
#[non_exhaustive]
#[derive(Debug, PartialEq)]
pub struct CompiledTransitions {
    /// The initial local time record.
    ///
    /// This is used in the case where a time predates a transition time.
    pub initial_record: LocalTimeRecord,

    /// The full set of calculated time zone transitions
    pub transitions: BTreeSet<Transition>,

    /// The POSIX time zone string
    ///
    /// This string should be used to calculate the time zone beyond the last available transition.
    pub posix_time_zone: PosixTimeZone,
}

// NOTE: candidate for removal? Should this library offer TZif structs long term?
//
// I think I would prefer all of that live in the `tzif` crate, but that will
// be a process to update. So implement it here, and then upstream it?
impl CompiledTransitions {
    pub fn to_v2_data_block(&self) -> TzifBlockV2 {
        TzifBlockV2::from_transition_data(self)
    }
}

/// The `CompiledTransitionsMap` struct contains a mapping of zone identifiers (AKA IANA identifiers) to
/// the zone's `CompiledTransitions`
#[derive(Debug, Default)]
pub struct CompiledTransitionsMap {
    pub data: HashMap<String, CompiledTransitions>,
}

// ==== ZoneInfoCompiler build / compile methods ====

use crate::{
    posix::PosixTimeZone,
    types::{QualifiedTimeKind, Time},
    tzif::TzifBlockV2,
    zone::ZoneRecord,
    ZoneInfoData,
};

/// The compiler for turning `ZoneInfoData` into `CompiledTransitionsData`
pub struct ZoneInfoCompiler {
    data: ZoneInfoData,
}

impl ZoneInfoCompiler {
    /// Create a new `ZoneInfoCompiler` instance with provided `ZoneInfoData`.
    pub fn new(data: ZoneInfoData) -> Self {
        Self { data }
    }

    /// Build transition data for a specific zone.
    pub fn build_zone(&mut self, target: &str) -> CompiledTransitions {
        if let Some(zone) = self.data.zones.get_mut(target) {
            zone.associate_rules(&self.data.rules);
        }
        self.build_zone_internal(target)
    }

    pub fn build(&mut self) -> CompiledTransitionsMap {
        // Associate the necessary rules with the ZoneTable
        self.associate();
        // TODO: Validate and resolve settings here.
        let mut zoneinfo = CompiledTransitionsMap::default();
        for identifier in self.data.zones.keys() {
            let transition_data = self.build_zone_internal(identifier);
            let _ = zoneinfo.data.insert(identifier.clone(), transition_data);
        }
        zoneinfo
    }

    /// The internal method for retrieving a zone table and compiling it.
    pub(crate) fn build_zone_internal(&self, target: &str) -> CompiledTransitions {
        let zone_table = self
            .data
            .zones
            .get(target)
            .expect("Invalid identifier provided.");
        zone_table.compile()
    }

    pub fn get_posix_time_zone(&mut self, target: &str) -> Option<PosixTimeZone> {
        self.associate();
        self.data
            .zones
            .get(target)
            .map(ZoneRecord::get_posix_time_zone)
    }

    /// Associates the current `ZoneTables` with their applicable rules.
    pub fn associate(&mut self) {
        for zones in self.data.zones.values_mut() {
            zones.associate_rules(&self.data.rules);
        }
    }
}
