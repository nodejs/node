#[cfg(feature = "std")]
use serde::{Deserialize, Serialize};
#[cfg(feature = "std")]
use std::{format, fs::read_to_string, path::Path, string::String, vec::Vec};
#[cfg(feature = "std")]
use zoneinfo_rs::{ZoneInfoCompiler, ZoneInfoData};

#[cfg(feature = "std")]
#[derive(Debug, Serialize, Deserialize)]
struct TzifTestData {
    first_record: LocalRecord,
    transitions: Vec<TransitionRecord>,
}

#[cfg(feature = "std")]
#[derive(Debug, Serialize, Deserialize)]
struct TransitionRecord {
    transition_time: i64,
    record: LocalRecord,
}

#[cfg(feature = "std")]
#[derive(Debug, Clone, Serialize, Deserialize)]
struct LocalRecord {
    offset: i64,
    is_dst: bool,
    abbr: String,
}

#[cfg(feature = "std")]
fn test_data_for_id(identifier: &str) {
    let manifest_dir = Path::new(env!("CARGO_MANIFEST_DIR"));
    let test_dir = manifest_dir.join("tests");
    let test_data_dir = test_dir.join("data");

    // Get test data
    let test_json = identifier.replace("/", "-").to_ascii_lowercase();
    let test_data_path = test_data_dir.join(format!("{test_json}.json"));
    let test_data: TzifTestData =
        serde_json::from_str(&read_to_string(test_data_path).unwrap()).unwrap();

    // Compile zoneinfo file.
    let zoneinfo_data = ZoneInfoData::from_filepath(test_dir.join("zoneinfo")).unwrap();
    let mut compiler = ZoneInfoCompiler::new(zoneinfo_data);
    let computed_zoneinfo = compiler.build_zone(identifier);

    assert_eq!(
        computed_zoneinfo.initial_record.offset,
        test_data.first_record.offset
    );
    assert_eq!(
        computed_zoneinfo.initial_record.designation,
        test_data.first_record.abbr
    );

    for (computed, test_data) in computed_zoneinfo
        .transitions
        .iter()
        .zip(test_data.transitions)
    {
        assert_eq!(
            computed.at_time, test_data.transition_time,
            "Transition time are not aligned for {}",
            test_data.transition_time
        );
        assert_eq!(
            computed.offset, test_data.record.offset,
            "Offsets are not aligned for {}",
            test_data.transition_time
        );
        // Test data is currently in rearguard, not vanguard. Would need to add
        // support for rearguard and to test dst for Europe/Dublin
        assert_eq!(
            computed.dst, test_data.record.is_dst,
            "DST flag is not aligned for {}",
            test_data.transition_time
        );
        // When in named rule before any transition has happened,
        // value is initialized to first letter of save == 0
        assert_eq!(
            computed.format, test_data.record.abbr,
            "Designation is not aligned for {}",
            test_data.transition_time
        );
    }
}

#[test]
#[cfg(feature = "std")]
fn test_chicago() {
    test_data_for_id("America/Chicago");
}

#[test]
#[cfg(feature = "std")]
fn test_new_york() {
    test_data_for_id("America/New_York");
}

#[test]
#[cfg(feature = "std")]
fn test_anchorage() {
    test_data_for_id("America/Anchorage");
}

#[test]
#[cfg(feature = "std")]
fn test_sydney() {
    test_data_for_id("Australia/Sydney");
}

#[test]
#[cfg(feature = "std")]
fn test_lord_howe() {
    test_data_for_id("Australia/Lord_Howe");
}

#[test]
#[cfg(feature = "std")]
fn test_troll() {
    test_data_for_id("Antarctica/Troll");
}

// TODO: test_dublin_rearguard
#[test]
#[cfg(feature = "std")]
fn test_dublin() {
    test_data_for_id("Europe/Dublin");
}

#[test]
#[cfg(feature = "std")]
fn test_berlin() {
    test_data_for_id("Europe/Berlin");
}

#[test]
#[cfg(feature = "std")]
fn test_paris() {
    test_data_for_id("Europe/Paris");
}

#[test]
#[cfg(feature = "std")]
fn test_london() {
    test_data_for_id("Europe/London");
}

#[test]
#[cfg(feature = "std")]
fn test_riga() {
    test_data_for_id("Europe/Riga");
}
