use super::*;
use alloc::string::String;
use alloc::vec::Vec;
use std::{
    borrow::ToOwned,
    collections::{BTreeMap, BTreeSet},
    fs, io,
    path::Path,
};
use zoneinfo_rs::{ZoneInfoData, ZoneInfoError};

#[derive(Debug)]
pub enum TzdbDataSourceError {
    Io(io::Error),
    ZoneInfo(ZoneInfoError),
}

impl From<io::Error> for TzdbDataSourceError {
    fn from(value: io::Error) -> Self {
        Self::Io(value)
    }
}

impl From<ZoneInfoError> for TzdbDataSourceError {
    fn from(value: ZoneInfoError) -> Self {
        Self::ZoneInfo(value)
    }
}

pub struct TzdbDataSource {
    pub version: String,
    pub data: ZoneInfoData,
}

impl TzdbDataSource {
    /// Try to create a tzdb source from a tzdata directory.
    pub fn try_from_zoneinfo_directory(tzdata_path: &Path) -> Result<Self, TzdbDataSourceError> {
        let version_file = tzdata_path.join("version");
        let version = fs::read_to_string(version_file)?.trim().to_owned();
        let data = ZoneInfoData::from_zoneinfo_directory(tzdata_path)?;
        Ok(Self { version, data })
    }

    /// Try to create a tzdb source from a tzdata rearguard.zi
    ///
    /// To generate a rearguard.zi, download tzdata from IANA. Run `make rearguard.zi`
    pub fn try_from_rearguard_zoneinfo_dir(
        tzdata_path: &Path,
    ) -> Result<Self, TzdbDataSourceError> {
        let version_file = tzdata_path.join("version");
        let version = fs::read_to_string(version_file)?.trim().to_owned();
        let rearguard_zoneinfo = tzdata_path.join("rearguard.zi");
        let data = ZoneInfoData::from_filepath(rearguard_zoneinfo)?;
        Ok(Self { version, data })
    }
}

// ==== Begin DataProvider impl ====

#[derive(Debug)]
pub enum IanaDataError {
    Io(io::Error),
    Provider(TzdbDataSourceError),
    Build(zerotrie::ZeroTrieBuildError),
}

#[allow(clippy::expect_used, clippy::unwrap_used, reason = "Datagen only")]
impl IanaIdentifierNormalizer<'_> {
    pub fn build(tzdata_path: &Path) -> Result<Self, IanaDataError> {
        let provider = TzdbDataSource::try_from_zoneinfo_directory(tzdata_path)
            .map_err(IanaDataError::Provider)?;
        let mut all_identifiers = BTreeSet::default();
        for zone_id in provider.data.zones.keys() {
            // Add canonical identifiers.
            let _ = all_identifiers.insert(&**zone_id);
        }

        for link_from in provider.data.links.keys() {
            // Add link / non-canonical identifiers
            let _ = all_identifiers.insert(link_from);
        }
        // Make a sorted list of canonical timezones
        let norm_vec: Vec<&str> = all_identifiers.iter().copied().collect();
        let norm_zerovec: VarZeroVec<'static, str> = norm_vec.as_slice().into();

        let identifier_map: BTreeMap<Vec<u8>, usize> = all_identifiers
            .iter()
            .map(|id| {
                let normalized_id = norm_vec.binary_search(id).unwrap();

                (id.to_ascii_lowercase().as_bytes().to_vec(), normalized_id)
            })
            .collect();

        let mut primary_id_map: BTreeMap<usize, usize> = BTreeMap::new();
        // ECMAScript implementations must support an available named time zone with the identifier "UTC", which must be
        // the primary time zone identifier for the UTC time zone. In addition, implementations may support any number of other available named time zones.
        let utc_index = norm_vec.binary_search(&"UTC").unwrap();
        primary_id_map.insert(norm_vec.binary_search(&"Etc/UTC").unwrap(), utc_index);
        primary_id_map.insert(norm_vec.binary_search(&"Etc/GMT").unwrap(), utc_index);

        for (link_from, link_to) in &provider.data.links {
            if link_from == "UTC" {
                continue;
            }
            let link_from = norm_vec.binary_search(&&**link_from).unwrap();
            let index = if link_to == "Etc/UTC" || link_to == "Etc/GMT" {
                utc_index
            } else {
                norm_vec.binary_search(&&**link_to).unwrap()
            };
            primary_id_map.insert(link_from, index);
        }

        Ok(IanaIdentifierNormalizer {
            version: provider.version.into(),
            available_id_index: ZeroAsciiIgnoreCaseTrie::try_from(&identifier_map)
                .map_err(IanaDataError::Build)?
                .convert_store(),
            non_canonical_identifiers: primary_id_map
                .iter()
                .map(|(x, y)| (u32::try_from(*x).unwrap(), u32::try_from(*y).unwrap()))
                .collect(),
            normalized_identifiers: norm_zerovec,
        })
    }
}

// ==== End DataProvider impl ====
