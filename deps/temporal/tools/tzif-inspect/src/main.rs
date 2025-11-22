use std::env;
use std::string::ToString;
use temporal_rs::partial::PartialDuration;
use temporal_rs::{Duration, PlainDate, PlainTime, TimeZone, ZonedDateTime};
use timezone_provider::tzif::Tzif;
use tzif::data::posix::TransitionDay;
use tzif::data::time::Seconds;
use tzif::data::tzif::{StandardWallIndicator, UtLocalIndicator};

macro_rules! format_line(
    ($arr:ident[$i:expr], $($args:expr),*) => {
        let string = stringify!($arr);
        let array = format!("{}[{}]", string, $i);
        format_line!(array, $($args),*)
    };
    ($a:expr, $b:expr, $c: expr, $d: expr) => {
        println!("{:<25} {:<20} {:<5} {}", $a, $b, $c, $d)
    };
    ($a:expr, $b:expr, $c: expr) => {
        println!("{:<25} {:<20} {}", $a, $b, $c)
    };
    ($a:expr, $b:expr) => {
        println!("{:<25} {}", $a, $b)
    };
);

fn seconds_to_zdt_string(s: Seconds, time_zone: TimeZone) -> String {
    ZonedDateTime::try_new_iso(s.0 as i128 * 1_000_000_000, time_zone)
        .unwrap()
        .to_string()
}

fn seconds_to_offset_time(s: Seconds) -> String {
    let is_negative = s.0 < 0;
    let seconds = s.0.abs();
    let partial = PartialDuration::default().with_seconds(seconds);
    let time = PlainTime::default()
        .add(&Duration::from_partial_duration(partial).unwrap())
        .unwrap();
    let string = time.to_ixdtf_string(Default::default()).unwrap();
    if is_negative {
        format!("-{string}")
    } else {
        string
    }
}

fn month(m: u16) -> &'static str {
    match m {
        1 => "Jan",
        2 => "Feb",
        3 => "Mar",
        4 => "Apr",
        5 => "May",
        6 => "Jun",
        7 => "Jul",
        8 => "Aug",
        9 => "Sep",
        10 => "Oct",
        11 => "Nov",
        12 => "Dec",
        _ => unreachable!(),
    }
}

fn format_transition_day(trans_day: TransitionDay) -> String {
    match trans_day {
        TransitionDay::NoLeap(d) | TransitionDay::WithLeap(d) => {
            let no_leap = matches!(trans_day, TransitionDay::NoLeap(_));
            let start_year = if no_leap { 2001 } else { 2004 };
            let start_date = PlainDate::new(start_year, 1, 1, Default::default()).unwrap();
            let date_duration = Duration::new(0, 0, 0, d.into(), 0, 0, 0, 0, 0, 0).unwrap();
            let adjusted = start_date.add(&date_duration, None).unwrap();
            let m = month(adjusted.month().into());
            let day = adjusted.day();
            if no_leap {
                format!("NoLeap({d}): {m} {day}")
            } else {
                format!("WithLeap({d}): {m} {day}")
            }
        }
        TransitionDay::Mwd(m, w, d) => {
            let month = month(m);
            let day = match d {
                0 => "Sun",
                1 => "Mon",
                2 => "Tue",
                3 => "Wed",
                4 => "Thu",
                5 => "Fri",
                6 => "Sat",
                _ => unreachable!(),
            };

            format!("{day} #{w} of {month}")
        }
    }
}
fn main() {
    let tz = env::args().nth(1).expect("Needs one argument");
    let tzif = jiff_tzdb::get(&tz).unwrap();
    let tzif = Tzif::from_bytes(tzif.1).unwrap();
    let time_zone = TimeZone::try_from_identifier_str(&tz).unwrap();
    let utc = TimeZone::try_from_identifier_str("UTC").unwrap();

    let Some(header) = tzif.header2 else {
        println!("No V2 header");
        return;
    };
    let Some(db) = tzif.data_block2 else {
        println!("No V2 data");
        return;
    };

    println!("--------- Header ----------");
    format_line!("version", header.version);
    format_line!("isutcnt", header.isutcnt);
    format_line!("isstdcnt", header.isstdcnt);
    format_line!("leapcnt", header.leapcnt);
    format_line!("timecnt", header.timecnt);
    format_line!("typecnt", header.typecnt);
    format_line!("charcnt", header.charcnt);
    println!("\n--------- Data ----------");
    format_line!("Transition", "Time", "Type", "Time (formatted)");
    for (i, seconds) in db.transition_times.iter().enumerate() {
        format_line!(
            transitions[i],
            seconds.0,
            db.transition_types[i],
            seconds_to_zdt_string(*seconds, time_zone)
        );
    }

    println!();
    for (i, ltt) in db.local_time_type_records.iter().enumerate() {
        format_line!(localtimetype[i], "");
        format_line!("  utcoff", ltt.utoff.0, seconds_to_offset_time(ltt.utoff));
        format_line!("  is_dst", ltt.is_dst);
        format_line!("  idx", ltt.idx);
    }

    if !db.time_zone_designations.is_empty() {
        println!();
    }
    for (i, desig) in db.time_zone_designations.iter().enumerate() {
        format_line!(desginations[i], desig);
    }

    if !db.leap_second_records.is_empty() {
        println!();
    }
    for (i, leap) in db.leap_second_records.iter().enumerate() {
        format_line!(
            leap[i],
            leap.occurrence.0,
            leap.correction,
            seconds_to_zdt_string(leap.occurrence, utc)
        );
    }

    if !db.ut_local_indicators.is_empty() {
        println!();
    }
    for (i, utlocal) in db.ut_local_indicators.iter().enumerate() {
        let utlocal = if *utlocal == UtLocalIndicator::Ut {
            "UT"
        } else {
            "local"
        };
        format_line!(ut_local[i], utlocal);
    }

    if !db.standard_wall_indicators.is_empty() {
        println!();
    }
    for (i, sw) in db.standard_wall_indicators.iter().enumerate() {
        let sw = if *sw == StandardWallIndicator::Standard {
            "standard"
        } else {
            "wall"
        };
        format_line!(standard_wall[i], sw);
    }

    println!("\n--------- Footer ----------");
    if let Some(footer) = tzif.footer {
        let offset = footer.std_info.offset.0;
        format_line!(
            "std_info",
            footer.std_info.name,
            offset,
            // Posix offsets are backwards
            seconds_to_offset_time(Seconds(-offset))
        );
        if let Some(dst_info) = footer.dst_info {
            format_line!("dst_info", "");
            let offset = dst_info.variant_info.offset.0;
            format_line!(
                "  variant_info",
                dst_info.variant_info.name,
                offset,
                // Posix offsets are backwards
                seconds_to_offset_time(Seconds(-offset))
            );
            format_line!(
                "  start_date",
                format_transition_day(dst_info.start_date.day),
                dst_info.start_date.time.0,
                seconds_to_offset_time(dst_info.start_date.time)
            );
            format_line!(
                "  end_date",
                format_transition_day(dst_info.end_date.day),
                dst_info.end_date.time.0,
                seconds_to_offset_time(dst_info.end_date.time)
            );
        }
    }
}
