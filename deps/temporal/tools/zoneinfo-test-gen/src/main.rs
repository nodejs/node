use clap::Parser;
use serde::{Deserialize, Serialize};
use std::{
    fs,
    path::{Path, PathBuf},
};

#[derive(Debug, Serialize, Deserialize)]
struct TzifTestData {
    first_record: LocalRecord,
    transitions: Vec<TransitionRecord>,
}

#[derive(Debug, Serialize, Deserialize)]
struct TransitionRecord {
    transition_time: i64,
    record: LocalRecord,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
struct LocalRecord {
    offset: i64,
    is_dst: bool,
    abbr: String,
}

// Utility function for generating example files
fn generate_test_data(input_dir: PathBuf, output_dir: PathBuf, identifier: &str) {
    let filename = identifier.to_lowercase().replace("/", "-");
    let test_data_path = output_dir.join(format!("{filename}.json"));

    let tzif_path = input_dir.join(identifier);
    std::println!("Parsing tzif from {tzif_path:?}");
    let tzif = tzif::parse_tzif_file(&tzif_path).unwrap();

    let tzif_block_v2 = tzif.data_block2.unwrap();
    let first_record_data = tzif_block_v2.local_time_type_records[0];
    let first_record = LocalRecord {
        offset: first_record_data.utoff.0,
        is_dst: first_record_data.is_dst,
        abbr: tzif_block_v2.time_zone_designations[0].clone(),
    };

    // TODO: There may be a bug in `tzif` around handling of split abbr (EX: IST/GMT)
    let local_records = tzif_block_v2
        .local_time_type_records
        .iter()
        .map(|r| LocalRecord {
            offset: r.utoff.0,
            is_dst: r.is_dst,
            abbr: tzif_block_v2
                .time_zone_designations
                .get(r.idx / 4)
                .cloned()
                .unwrap_or(String::from("unknown")),
        })
        .collect::<Vec<_>>();

    let transitions = tzif_block_v2
        .transition_times
        .iter()
        .zip(tzif_block_v2.transition_types)
        .map(|(time, time_type)| TransitionRecord {
            transition_time: time.0,
            record: local_records[time_type].clone(),
        })
        .collect::<Vec<TransitionRecord>>();

    let tzif_data = TzifTestData {
        first_record,
        transitions,
    };

    std::println!("Writing generated example data to {test_data_path:?}");
    fs::write(
        test_data_path,
        serde_json::to_string_pretty(&tzif_data).unwrap(),
    )
    .unwrap();
}

const UNIX_ZONEINFO: &str = "/usr/share/zoneinfo/";

#[derive(clap::Parser)]
struct Args {
    /// Output directory relative to `CARGO_MANIFEST_DIR`
    #[arg(short, long)]
    output_dir: Option<PathBuf>,

    /// The zoneinfo / tzdata directory relative to `CARGO_MANIFEST_DIR` (defaults to UNIX zoneinfo)
    #[arg(short = 'i', long)]
    zoneinfo_dir: Option<PathBuf>,

    identifier: String,
}

fn main() {
    let args = Args::parse();
    let manifest_dir = Path::new(env!("CARGO_MANIFEST_DIR"));

    // Create input directory
    let zoneinfo_dir = if let Some(dir) = args.zoneinfo_dir {
        manifest_dir.join(dir)
    } else {
        PathBuf::from(UNIX_ZONEINFO)
    };
    // Create output directory
    let out_dir = if let Some(dir) = args.output_dir {
        manifest_dir.join(dir)
    } else {
        manifest_dir.into()
    };

    generate_test_data(zoneinfo_dir, out_dir, "Antarctica/Troll");
}
