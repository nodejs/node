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

/// <https://tc39.es/ecma402/#sec-use-of-iana-time-zone-database>
///
/// This spec text wants us to ensure that all zones fully contained
/// in a region must canonicalize to an entry that is under zone.tab for
/// that region.
///
/// These timezones are mentioned in the packrat entries, HOWEVER the packrat
/// entries map to tzdb's canonical timezones, which doesn't include the fact that
/// we treat all zone.tab entries as canonical. There's no easy way to recover this
/// information. Instead, since there are only three of them, we assert that we have the
/// same three, and hardcode overrides.
///
/// The hardcoded values are taken from <https://github.com/unicode-org/cldr/blob/main/common/bcp47/timezone.xml>
const PACKRAT_OVERRIDES: &[(&str, &str)] = &[
    ("Atlantic/Jan_Mayen", "Arctic/Longyearbyen"),
    ("America/Coral_Harbour", "America/Atikokan"),
    ("Africa/Timbuktu", "Africa/Bamako"),
];

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
        let mut provider = TzdbDataSource::try_from_zoneinfo_directory(tzdata_path)
            .map_err(IanaDataError::Provider)?;

        // This data includes things like Truk/Chuuk which Temporal requires in its tests
        // It also includes the packrat data
        let backzone = ZoneInfoData::from_filepath(tzdata_path.join("backzone")).unwrap();
        provider.data.extend(backzone);

        let packrat_overrides: BTreeMap<_, _> = PACKRAT_OVERRIDES.iter().copied().collect();

        for pack in provider.data.pack_rat {
            assert!(
                packrat_overrides.contains_key(&*pack.0),
                "Found missing packrat entry {}",
                pack.0
            );
        }

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

        // A map from noncanonical identifiers to their canonicalized id
        let mut to_primary_id_map: BTreeMap<usize, usize> = BTreeMap::new();
        // ECMAScript implementations must support an available named time zone with the identifier "UTC", which must be
        // the primary time zone identifier for the UTC time zone. In addition, implementations may support any number of other available named time zones.
        let utc_index = norm_vec.binary_search(&"UTC").unwrap();
        to_primary_id_map.insert(norm_vec.binary_search(&"Etc/UTC").unwrap(), utc_index);
        to_primary_id_map.insert(norm_vec.binary_search(&"Etc/GMT").unwrap(), utc_index);

        let mut all_links: BTreeMap<&str, &str> = provider
            .data
            .links
            .iter()
            .map(|x| (&**x.0, &**x.1))
            .collect();

        // https://tc39.es/ecma402/#sec-use-of-iana-time-zone-database
        // > Any Link name that is present in the “TZ” column of file zone.tab
        // > must be a primary time zone identifier.
        //
        // So we ignore links entries that link from these timezones
        // which results in those timezones considered as primary.
        for tz in provider.data.zone_tab {
            all_links.remove(&*tz.tz);
        }

        // UTC should not map to anything
        all_links.remove("UTC");

        for (link_from, mut link_to) in &all_links {
            // Sometimes links have multiple steps. This happens for Chungking => Chongqing => Shanghai
            while let Some(new_link_to) = all_links.get(link_to) {
                link_to = new_link_to;
            }
            if let Some(overrided) = packrat_overrides.get(link_from) {
                // See comment on PACKRAT_OVERRIDES
                link_to = overrided;
            }
            let link_from = norm_vec.binary_search(link_from).unwrap();
            let index = if *link_to == "Etc/UTC" || *link_to == "Etc/GMT" {
                utc_index
            } else {
                norm_vec.binary_search(link_to).unwrap()
            };
            to_primary_id_map.insert(link_from, index);
        }

        Ok(IanaIdentifierNormalizer {
            version: provider.version.into(),
            available_id_index: ZeroAsciiIgnoreCaseTrie::try_from(&identifier_map)
                .map_err(IanaDataError::Build)?
                .convert_store(),
            non_canonical_identifiers: to_primary_id_map
                .iter()
                .map(|(x, y)| (u32::try_from(*x).unwrap(), u32::try_from(*y).unwrap()))
                .collect(),
            normalized_identifiers: norm_zerovec,
        })
    }
}

// ==== End DataProvider impl ====
