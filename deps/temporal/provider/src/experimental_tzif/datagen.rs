use super::*;
use crate::tzdb::datagen::TzdbDataSource;
use alloc::collections::BTreeMap;
use alloc::vec::Vec;
use std::path::Path;
use zerotrie::ZeroTrieBuildError;
use zoneinfo_rs::{compiler::CompiledTransitions, ZoneInfoCompiler, ZoneInfoData};

impl From<&zoneinfo_rs::tzif::LocalTimeRecord> for LocalTimeRecord {
    fn from(value: &zoneinfo_rs::tzif::LocalTimeRecord) -> Self {
        Self {
            offset: value.offset,
        }
    }
}

impl ZeroTzif<'_> {
    fn from_transition_data(data: &CompiledTransitions) -> Self {
        let tzif = data.to_v2_data_block();
        let transitions = ZeroVec::alloc_from_slice(&tzif.transition_times);
        let transition_types = ZeroVec::alloc_from_slice(&tzif.transition_types);
        let mapped_local_records: Vec<LocalTimeRecord> =
            tzif.local_time_types.iter().map(Into::into).collect();
        let types = ZeroVec::alloc_from_slice(&mapped_local_records);
        // TODO: handle this much better.
        let posix = PosixZone::from(&data.posix_time_zone);

        Self {
            transitions,
            transition_types,
            types,
            posix,
        }
    }
}

#[derive(Debug)]
pub enum ZoneInfoDataError {
    Build(ZeroTrieBuildError),
}

#[allow(clippy::expect_used, clippy::unwrap_used, reason = "Datagen only")]
impl ZoneInfoProvider<'_> {
    pub fn build(tzdata: &Path) -> Result<Self, ZoneInfoDataError> {
        let tzdb_source = TzdbDataSource::try_from_rearguard_zoneinfo_dir(tzdata).unwrap();
        let compiled_transitions = ZoneInfoCompiler::new(tzdb_source.data.clone()).build();

        let mut identifiers = BTreeMap::default();
        let mut primary_zones = Vec::default();

        // Create a Map of <ZoneId | Link, ZoneId>, this is used later to index
        let ZoneInfoData { links, zones, .. } = tzdb_source.data;

        for zone_identifier in zones.into_keys() {
            primary_zones.push(zone_identifier.clone());
            identifiers.insert(zone_identifier.clone(), zone_identifier);
        }
        for (link, zone) in links.into_iter() {
            identifiers.insert(link, zone);
        }

        primary_zones.sort();

        let identifier_map: BTreeMap<Vec<u8>, usize> = identifiers
            .into_iter()
            .map(|(id, zoneid)| {
                (
                    id.to_ascii_lowercase().as_bytes().to_vec(),
                    primary_zones.binary_search(&zoneid).unwrap(),
                )
            })
            .collect();

        let tzifs: Vec<ZeroTzif<'_>> = primary_zones
            .into_iter()
            .map(|id| {
                let data = compiled_transitions
                    .data
                    .get(&id)
                    .expect("all zones should be built");
                ZeroTzif::from_transition_data(data)
            })
            .collect();

        let tzifs_zerovec: VarZeroVec<'static, ZeroTzifULE, Index32> = tzifs.as_slice().into();

        let ids = ZeroAsciiIgnoreCaseTrie::try_from(&identifier_map)
            .map_err(ZoneInfoDataError::Build)?
            .convert_store();

        Ok(ZoneInfoProvider {
            ids,
            tzifs: tzifs_zerovec,
        })
    }
}
