#[cfg(feature = "std")]
use std::path::Path;
#[cfg(feature = "std")]
use zoneinfo_rs::{ZoneInfoCompiler, ZoneInfoData};

#[test]
#[cfg(feature = "std")]
fn posix_string_test() {
    let manifest_dir = Path::new(env!("CARGO_MANIFEST_DIR"));
    let zoneinfo = ZoneInfoData::from_filepath(manifest_dir.join("tests/zoneinfo")).unwrap();

    let mut zic = ZoneInfoCompiler::new(zoneinfo);

    let chicago_posix = zic.get_posix_time_zone("America/Chicago").unwrap();
    assert_eq!(
        chicago_posix.to_string(),
        Ok("CST6CDT,M3.2.0,M11.1.0".into())
    );

    let lord_howe_posix = zic.get_posix_time_zone("Australia/Lord_Howe").unwrap();
    assert_eq!(
        lord_howe_posix.to_string(),
        Ok("<+1030>-10:30<+11>-11,M10.1.0,M4.1.0".into())
    );

    let troll_posix = zic.get_posix_time_zone("Antarctica/Troll").unwrap();
    assert_eq!(
        troll_posix.to_string(),
        Ok("<+00>0<+02>-2,M3.5.0/1,M10.5.0/3".into())
    );

    let dublin_posix = zic.get_posix_time_zone("Europe/Dublin").unwrap();
    assert_eq!(
        dublin_posix.to_string(),
        Ok("IST-1GMT0,M10.5.0,M3.5.0/1".into())
    );

    let minsk_posix = zic.get_posix_time_zone("Europe/Minsk").unwrap();
    assert_eq!(minsk_posix.to_string(), Ok("<+03>-3".into()));

    let moscow_posix = zic.get_posix_time_zone("Europe/Moscow").unwrap();
    assert_eq!(moscow_posix.to_string(), Ok("MSK-3".into()));
}
