use super::ZonedDateTime;
use crate::{
    builtins::{calendar::CalendarFields, zoned_date_time::ZonedDateTimeFields},
    options::{
        DifferenceSettings, Disambiguation, DisplayCalendar, DisplayOffset, DisplayTimeZone,
        OffsetDisambiguation, Overflow, RoundingIncrement, RoundingMode, RoundingOptions, Unit,
    },
    partial::{PartialTime, PartialZonedDateTime},
    provider::{TimeZoneProvider, TransitionDirection},
    unix_time::EpochNanoseconds,
    Calendar, Duration, MonthCode, TemporalResult, TimeZone, UtcOffset,
};
use alloc::string::ToString;
use timezone_provider::zoneinfo64::ZONEINFO64_RES_FOR_TESTING;
use tinystr::tinystr;

macro_rules! test_all_providers {
    ($(#[cfg_for_fs($cfg_fs:meta)])? $(#[cfg_for_zi64($cfg_zi:meta)])? $provider:ident: $b:block) => {{
        #[cfg(feature = "compiled_data")]
        {
            std::println!("Testing compiled_data:");
            let $provider = &*crate::builtins::TZ_PROVIDER;

            $b
        }

        $(#[cfg($cfg_zi)])? {
            std::println!("Testing ZoneInfo64Provider:");
            let zi_data =
                zoneinfo64::ZoneInfo64::try_from_u32s(&ZONEINFO64_RES_FOR_TESTING).unwrap();
            let zi_provider = timezone_provider::zoneinfo64::ZoneInfo64TzdbProvider::new(zi_data);
            let $provider = &zi_provider;

            $b
        }

        $(#[cfg($cfg_fs)])? #[cfg(feature = "tzdb")] {
            std::println!("Testing FS (note: May fail with bad local tzdb data):");
            let fs = timezone_provider::tzif::FsTzdbProvider::default();
            let $provider = &fs;

            $b
        }
    }};
}

#[test]
fn basic_zdt_test() {
    test_all_providers!(provider: {
        let nov_30_2023_utc = 1_701_308_952_000_000_000i128;

        let zdt = ZonedDateTime::try_new_with_provider(
            nov_30_2023_utc,
            TimeZone::try_from_str_with_provider("UTC", provider).unwrap(),
            Calendar::ISO,
            provider,
        )
        .unwrap();

        assert_eq!(zdt.year(), 2023);
        assert_eq!(zdt.month(), 11);
        assert_eq!(zdt.day(), 30);
        assert_eq!(zdt.hour(), 1);
        assert_eq!(zdt.minute(), 49);
        assert_eq!(zdt.second(), 12);

        let zdt_minus_five = ZonedDateTime::try_new_with_provider(
            nov_30_2023_utc,
            TimeZone::try_from_str_with_provider("America/New_York", provider).unwrap(),
            Calendar::ISO,
            provider,
        )
        .unwrap();

        assert_eq!(zdt_minus_five.year(), 2023);
        assert_eq!(zdt_minus_five.month(), 11);
        assert_eq!(zdt_minus_five.day(), 29);
        assert_eq!(zdt_minus_five.hour(), 20);
        assert_eq!(zdt_minus_five.minute(), 49);
        assert_eq!(zdt_minus_five.second(), 12);

        let zdt_plus_eleven = ZonedDateTime::try_new_with_provider(
            nov_30_2023_utc,
            TimeZone::try_from_str_with_provider("Australia/Sydney", provider).unwrap(),
            Calendar::ISO,
            provider,
        )
        .unwrap();

        assert_eq!(zdt_plus_eleven.year(), 2023);
        assert_eq!(zdt_plus_eleven.month(), 11);
        assert_eq!(zdt_plus_eleven.day(), 30);
        assert_eq!(zdt_plus_eleven.hour(), 12);
        assert_eq!(zdt_plus_eleven.minute(), 49);
        assert_eq!(zdt_plus_eleven.second(), 12);
    })
}

#[test]
// https://tc39.es/proposal-temporal/docs/zoneddatetime.html#round
fn round_with_provider_test() {
    test_all_providers!(provider: {
        let dt = b"1995-12-07T03:24:30.000003500-08:00[America/Los_Angeles]";
        let zdt = ZonedDateTime::from_utf8_with_provider(
            dt,
            Disambiguation::default(),
            OffsetDisambiguation::Use,
            provider,
        )
        .unwrap();

        let result = zdt
            .round_with_provider(
                RoundingOptions {
                    smallest_unit: Some(Unit::Hour),
                    ..Default::default()
                },
                provider,
            )
            .unwrap();
        assert_eq!(
            result.to_string_with_provider(provider).unwrap(),
            "1995-12-07T03:00:00-08:00[America/Los_Angeles]"
        );

        let result = zdt
            .round_with_provider(
                RoundingOptions {
                    smallest_unit: Some(Unit::Minute),
                    increment: Some((RoundingIncrement::try_new(30)).unwrap()),
                    ..Default::default()
                },
                provider,
            )
            .unwrap();
        assert_eq!(
            result.to_string_with_provider(provider).unwrap(),
            "1995-12-07T03:30:00-08:00[America/Los_Angeles]"
        );

        let result = zdt
            .round_with_provider(
                RoundingOptions {
                    smallest_unit: Some(Unit::Minute),
                    increment: Some((RoundingIncrement::try_new(30)).unwrap()),
                    rounding_mode: Some(RoundingMode::Floor),
                    ..Default::default()
                },
                provider,
            )
            .unwrap();
        assert_eq!(
            result.to_string_with_provider(provider).unwrap(),
            "1995-12-07T03:00:00-08:00[America/Los_Angeles]"
        );
    })
}

#[test]
fn zdt_from_partial() {
    test_all_providers!(provider: {
        let fields = ZonedDateTimeFields {
            calendar_fields: CalendarFields::new()
                .with_year(1970)
                .with_month_code(MonthCode(tinystr!(4, "M01")))
                .with_day(1),
            time: Default::default(),
            offset: None,
        };
        let partial = PartialZonedDateTime {
            fields,
            timezone: Some(TimeZone::utc_with_provider(provider)),
            calendar: Calendar::ISO,
        };

        let result = ZonedDateTime::from_partial_with_provider(partial, None, None, None, provider);
        assert!(result.is_ok());

        // This ensures that the start-of-day branch isn't hit by default time
        let fields = ZonedDateTimeFields {
            calendar_fields: CalendarFields::new()
                .with_year(1970)
                .with_month_code(MonthCode(tinystr!(4, "M01")))
                .with_day(1),
            time: PartialTime::default(),
            offset: Some(UtcOffset::from_minutes(30)),
        };
        let partial = PartialZonedDateTime {
            fields,
            timezone: Some(TimeZone::utc_with_provider(provider)),
            calendar: Calendar::ISO,
        };

        let result = ZonedDateTime::from_partial_with_provider(
            partial,
            None,
            None,
            Some(OffsetDisambiguation::Use),
            provider,
        );
        assert!(result.is_ok());
    })
}

#[test]
fn zdt_from_str() {
    test_all_providers!(provider: {
        let zdt_str = b"1970-01-01T00:00[UTC][u-ca=iso8601]";
        let result = ZonedDateTime::from_utf8_with_provider(
            zdt_str,
            Disambiguation::Compatible,
            OffsetDisambiguation::Reject,
            provider,
        );
        assert!(result.is_ok());
    })
}

#[test]
fn zdt_hours_in_day() {
    test_all_providers!(provider: {
        let result =
            parse_zdt_with_reject("2025-07-04T12:00[UTC][u-ca=iso8601]", provider).unwrap();

        assert_eq!(result.hours_in_day_with_provider(provider).unwrap(), 24.)
    })
}

#[test]
// https://github.com/tc39/test262/blob/d9b10790bc4bb5b3e1aa895f11cbd2d31a5ec743/test/intl402/Temporal/ZonedDateTime/from/dst-skipped-cross-midnight.js
fn dst_skipped_cross_midnight() {
    test_all_providers!(provider: {
        let start_of_day =
            parse_zdt_with_compatible("1919-03-31[America/Toronto]", provider).unwrap();
        let midnight_disambiguated =
            parse_zdt_with_compatible("1919-03-31T00[America/Toronto]", provider).unwrap();

        assert_eq!(
            start_of_day.epoch_nanoseconds(),
            &EpochNanoseconds(-1601753400000000000)
        );
        assert_eq!(
            midnight_disambiguated.epoch_nanoseconds(),
            &EpochNanoseconds(-1601751600000000000)
        );
        let diff = start_of_day
            .instant
            .until(
                &midnight_disambiguated.instant,
                DifferenceSettings {
                    largest_unit: Some(Unit::Hour),
                    smallest_unit: Some(Unit::Nanosecond),
                    ..Default::default()
                },
            )
            .unwrap();
        assert_eq!(diff.years(), 0);
        assert_eq!(diff.months(), 0);
        assert_eq!(diff.weeks(), 0);
        assert_eq!(diff.days(), 0);
        assert_eq!(diff.hours(), 0);
        assert_eq!(diff.minutes(), 30);
        assert_eq!(diff.seconds(), 0);
        assert_eq!(diff.milliseconds(), 0);
        assert_eq!(diff.microseconds(), 0);
        assert_eq!(diff.nanoseconds(), 0);
    });
}

#[test]
fn zdt_offset_match_minutes() {
    // Cases taken from intl402/Temporal/ZonedDateTime/compare/sub-minute-offset

    test_all_providers!(provider: {
        // Rounded mm accepted
        let _ = parse_zdt_with_reject("1970-01-01T00:00-00:45[Africa/Monrovia]", provider).unwrap();
        // unrounded mm::ss accepted
        let _ = parse_zdt_with_reject("1970-01-01T00:00:00-00:44:30[Africa/Monrovia]", provider)
            .unwrap();
        assert!(
            parse_zdt_with_compatible("1970-01-01T00:00:00-00:44:40[Africa/Monrovia]", provider)
                .is_err(),
            "Incorrect unrounded mm::ss rejected"
        );
        assert!(
            parse_zdt_with_compatible("1970-01-01T00:00:00-00:45:00[Africa/Monrovia]", provider)
                .is_err(),
            "Rounded mm::ss rejected"
        );
        assert!(
            parse_zdt_with_compatible("1970-01-01T00:00+00:44:30.123456789[+00:45]", provider)
                .is_err(),
            "Rounding not accepted between ISO offset and timezone"
        );

        assert!(
            ZonedDateTime::from_partial_with_provider(
                PartialZonedDateTime {
                    fields: ZonedDateTimeFields {
                        calendar_fields: CalendarFields::new()
                            .with_year(1970)
                            .with_month_code(MonthCode(tinystr!(4, "M01")))
                            .with_day(1),
                        time: PartialTime::default(),
                        offset: Some(UtcOffset::from_minutes(30)),
                    },
                    timezone: Some(TimeZone::try_from_identifier_str_with_provider("Africa/Monrovia", provider).unwrap()),
                    ..PartialZonedDateTime::default()
                },
                None,
                None,
                None,
                provider
            )
            .is_err(),
            "Rounding not accepted between ISO offset and timezone"
        );
    })
}

// overflow-reject-throws.js
#[test]
fn overflow_reject_throws() {
    test_all_providers!(provider: {
        let zdt = ZonedDateTime::try_new_with_provider(
            217178610123456789,
            TimeZone::utc_with_provider(provider),
            Calendar::default(),
            provider,
        )
        .unwrap();

        let overflow = Overflow::Reject;

        let result_1 = zdt.with_with_provider(
            ZonedDateTimeFields {
                calendar_fields: CalendarFields::new().with_month(29),
                time: Default::default(),
                offset: None,
            },
            None,
            None,
            Some(overflow),
            provider,
        );

        let result_2 = zdt.with_with_provider(
            ZonedDateTimeFields {
                calendar_fields: CalendarFields::new().with_day(31),
                time: Default::default(),
                offset: None,
            },
            None,
            None,
            Some(overflow),
            provider,
        );

        let result_3 = zdt.with_with_provider(
            ZonedDateTimeFields {
                calendar_fields: CalendarFields::new(),
                time: PartialTime {
                    hour: Some(29),
                    ..Default::default()
                },
                offset: None,
            },
            None,
            None,
            Some(overflow),
            provider,
        );

        let result_4 = zdt.with_with_provider(
            ZonedDateTimeFields {
                calendar_fields: CalendarFields::default(),
                time: PartialTime {
                    nanosecond: Some(9000),
                    ..Default::default()
                },
                offset: None,
            },
            None,
            None,
            Some(overflow),
            provider,
        );

        assert!(result_1.is_err());
        assert!(result_2.is_err());
        assert!(result_3.is_err());
        assert!(result_4.is_err());
    })
}

#[test]
fn static_tzdb_zdt_test() {
    test_all_providers!(#[cfg_for_fs(not(target_os = "windows"))] provider: {
        use crate::{Calendar, TimeZone};
        use core::str::FromStr;

        let nov_30_2023_utc = 1_701_308_952_000_000_000i128;

        let zdt = ZonedDateTime::try_new_with_provider(
            nov_30_2023_utc,
            TimeZone::try_from_str_with_provider("UTC", provider).unwrap(),
            Calendar::from_str("iso8601").unwrap(),
            provider,
        )
        .unwrap();

        assert_eq!(zdt.year(), 2023);
        assert_eq!(zdt.month(), 11);
        assert_eq!(zdt.day(), 30);
        assert_eq!(zdt.hour(), 1);
        assert_eq!(zdt.minute(), 49);
        assert_eq!(zdt.second(), 12);

        let zdt_minus_five = ZonedDateTime::try_new_with_provider(
            nov_30_2023_utc,
            TimeZone::try_from_str_with_provider("America/New_York", provider).unwrap(),
            Calendar::from_str("iso8601").unwrap(),
            provider,
        )
        .unwrap();

        assert_eq!(zdt_minus_five.year(), 2023);
        assert_eq!(zdt_minus_five.month(), 11);
        assert_eq!(zdt_minus_five.day(), 29);
        assert_eq!(zdt_minus_five.hour(), 20);
        assert_eq!(zdt_minus_five.minute(), 49);
        assert_eq!(zdt_minus_five.second(), 12);

        let zdt_plus_eleven = ZonedDateTime::try_new_with_provider(
            nov_30_2023_utc,
            TimeZone::try_from_str_with_provider("Australia/Sydney", provider).unwrap(),
            Calendar::from_str("iso8601").unwrap(),
            provider,
        )
        .unwrap();

        assert_eq!(zdt_plus_eleven.year(), 2023);
        assert_eq!(zdt_plus_eleven.month(), 11);
        assert_eq!(zdt_plus_eleven.day(), 30);
        assert_eq!(zdt_plus_eleven.hour(), 12);
        assert_eq!(zdt_plus_eleven.minute(), 49);
        assert_eq!(zdt_plus_eleven.second(), 12);
    })
}

#[test]
fn basic_zdt_add() {
    use crate::{Calendar, Duration, TimeZone};
    test_all_providers!(#[cfg_for_fs(not(target_os = "windows"))] provider: {
        let zdt = ZonedDateTime::try_new_with_provider(
            -560174321098766,
            TimeZone::utc_with_provider(provider),
            Calendar::default(),
            provider,
        )
        .unwrap();
        let d = Duration::new(
            0.into(),
            0.into(),
            0.into(),
            0.into(),
            240.into(),
            0.into(),
            0.into(),
            0.into(),
            0.into(),
            800.into(),
        )
        .unwrap();
        // "1970-01-04T12:23:45.678902034+00:00[UTC]"
        let expected = ZonedDateTime::try_new_with_provider(
            303825678902034,
            TimeZone::utc_with_provider(provider),
            Calendar::default(),
            provider,
        )
        .unwrap();

        let result = zdt.add_with_provider(&d, None, provider).unwrap();
        assert!(result.equals_with_provider(&expected, provider).unwrap());
    })
}

fn parse_zdt_with_reject(
    s: &str,
    provider: &impl TimeZoneProvider,
) -> TemporalResult<ZonedDateTime> {
    ZonedDateTime::from_utf8_with_provider(
        s.as_bytes(),
        Disambiguation::Reject,
        OffsetDisambiguation::Reject,
        provider,
    )
}

fn parse_zdt_with_compatible(
    s: &str,
    provider: &impl TimeZoneProvider,
) -> TemporalResult<ZonedDateTime> {
    ZonedDateTime::from_utf8_with_provider(
        s.as_bytes(),
        Disambiguation::Compatible,
        OffsetDisambiguation::Reject,
        provider,
    )
}

#[test]
fn test_pacific_niue() {
    test_all_providers!(provider: {
        // test/intl402/Temporal/ZonedDateTime/compare/sub-minute-offset.js
        // Pacific/Niue on October 15, 1952, where
        // the offset shifted by 20 seconds to a whole-minute boundary.
        //
        // The precise transition is from
        // 1952-10-15T23:59:59-11:19:40[-11:19:40] to 1952-10-15T23:59:40-11:19:00[-11:19:00]
        let ms_pre = -543_069_621_000;
        let zdt =
            parse_zdt_with_reject("1952-10-15T23:59:59-11:19:40[Pacific/Niue]", provider).unwrap();
        assert_eq!(
            zdt.epoch_milliseconds(),
            ms_pre,
            "-11:19:40 is accepted as -11:19:40 in Pacific/Niue edge case"
        );

        let zdt =
            parse_zdt_with_reject("1952-10-15T23:59:59-11:20[Pacific/Niue]", provider).unwrap();
        assert_eq!(
            zdt.epoch_milliseconds(),
            ms_pre,
            "-11:20 matches the first candidate -11:19:40 in the Pacific/Niue edge case"
        );

        let ms_post = -543_069_601_000;

        let zdt =
            parse_zdt_with_reject("1952-10-15T23:59:59-11:20:00[Pacific/Niue]", provider).unwrap();
        assert_eq!(
            zdt.epoch_milliseconds(),
            ms_post,
            "-11:19:40 is accepted as -11:19:40 in Pacific/Niue edge case"
        );

        // Additional tests ensuring that boundary cases are handled

        let zdt =
            parse_zdt_with_reject("1952-10-15T23:59:40-11:20:00[Pacific/Niue]", provider).unwrap();
        assert_eq!(
            zdt.epoch_milliseconds(),
            ms_post - 19_000,
            "Post-transition Niue time allows up to `1952-10-15T23:59:40`"
        );
        let zdt = parse_zdt_with_reject("1952-10-15T23:59:39-11:20:00[Pacific/Niue]", provider);
        assert!(
            zdt.is_err(),
            "Post-transition Niue time does not allow times before `1952-10-15T23:59:40`"
        );

        let zdt =
            parse_zdt_with_reject("1952-10-15T23:59:40-11:19:40[Pacific/Niue]", provider).unwrap();
        assert_eq!(
            zdt.epoch_milliseconds(),
            ms_pre - 19_000,
            "Pre-transition Niue time also allows `1952-10-15T23:59:40`"
        );

        let zdt =
            parse_zdt_with_reject("1952-10-15T23:59:39-11:19:40[Pacific/Niue]", provider).unwrap();
        assert_eq!(
            zdt.epoch_milliseconds(),
            ms_pre - 20_000,
            "Pre-transition Niue time also allows `1952-10-15T23:59:39`"
        );

        // Tests without explicit offset
        let zdt = parse_zdt_with_reject("1952-10-15T23:59:39[Pacific/Niue]", provider).unwrap();
        assert_eq!(
            zdt.epoch_milliseconds(),
            ms_pre - 20_000,
            "Unambiguous before 1952-10-15T23:59:39"
        );

        let zdt = parse_zdt_with_reject("1952-10-16T00:00:00[Pacific/Niue]", provider).unwrap();
        assert_eq!(
            zdt.epoch_milliseconds(),
            ms_post + 1_000,
            "Unambiguous after 1952-10-16T00:00:00"
        );

        let zdt = parse_zdt_with_reject("1952-10-15T23:59:40[Pacific/Niue]", provider);
        assert!(zdt.is_err(), "Ambiguity starts at 1952-10-15T23:59:40");
        let zdt = parse_zdt_with_reject("1952-10-15T23:59:59[Pacific/Niue]", provider);
        assert!(zdt.is_err(), "Ambiguity ends at 1952-10-15T23:59:59");
    })
}

fn total_seconds_for_one_day(s: &str, provider: &impl TimeZoneProvider) -> TemporalResult<f64> {
    Ok(Duration::new(0, 0, 0, 1, 0, 0, 0, 0, 0, 0)
        .unwrap()
        .total_with_provider(
            Unit::Second,
            Some(parse_zdt_with_reject(s, provider).unwrap().into()),
            provider,
        )?
        .as_inner())
}

#[test]
fn test_pacific_niue_duration() {
    test_all_providers!(provider: {
        // Also tests add_to_instant codepaths
        // From intl402/Temporal/Duration/prototype/total/relativeto-sub-minute-offset
        let total =
            total_seconds_for_one_day("1952-10-15T23:59:59-11:19:40[Pacific/Niue]", provider)
                .unwrap();
        assert_eq!(
            total, 86420.,
            "-11:19:40 is accepted as -11:19:40 in Pacific/Niue edge case"
        );

        let total =
            total_seconds_for_one_day("1952-10-15T23:59:59-11:20[Pacific/Niue]", provider).unwrap();
        assert_eq!(
            total, 86420.,
            "-11:20 matches the first candidate -11:19:40 in the Pacific/Niue edge case"
        );

        let total =
            total_seconds_for_one_day("1952-10-15T23:59:59-11:20:00[Pacific/Niue]", provider)
                .unwrap();
        assert_eq!(
            total, 86400.,
            "-11:20:00 is accepted as -11:20:00 in the Pacific/Niue edge case"
        );
    })
}

#[track_caller]
fn assert_tr(
    zdt: &ZonedDateTime,
    direction: TransitionDirection,
    s: &str,
    provider: &impl TimeZoneProvider,
) {
    assert_eq!(
        zdt.get_time_zone_transition_with_provider(direction, provider)
            .unwrap()
            .unwrap()
            .to_string_with_provider(provider)
            .unwrap(),
        s
    );
}

// Modern dates

// Transitions
const DST_2025_03_09: &str = "2025-03-09T03:00:00-07:00[America/Los_Angeles]";
const DST_2026_03_08: &str = "2026-03-08T03:00:00-07:00[America/Los_Angeles]";
const STD_2025_11_02: &str = "2025-11-02T01:00:00-08:00[America/Los_Angeles]";
const STD_2024_11_03: &str = "2024-11-03T01:00:00-08:00[America/Los_Angeles]";

// Non transitions
const IN_DST_2025_07_31: &str = "2025-07-31T00:00:00-07:00[America/Los_Angeles]";
const AFTER_DST_2025_12_31: &str = "2025-12-31T00:00:00-08:00[America/Los_Angeles]";
const BEFORE_DST_2025_01_31: &str = "2025-01-31T00:00:00-08:00[America/Los_Angeles]";

// Transition dates ± 1
const DST_2025_03_09_PLUS_ONE: &str = "2025-03-09T03:00:00.000000001-07:00[America/Los_Angeles]";
const DST_2025_03_09_MINUS_ONE: &str = "2025-03-09T01:59:59.999999999-08:00[America/Los_Angeles]";
const STD_2025_11_02_PLUS_ONE: &str = "2025-11-02T01:00:00.000000001-08:00[America/Los_Angeles]";
const STD_2025_11_02_MINUS_ONE: &str = "2025-11-02T01:59:59.999999999-07:00[America/Los_Angeles]";

// Dates from the tzif data block
// Transitions
const DST_1999_04_04: &str = "1999-04-04T03:00:00-07:00[America/Los_Angeles]";
const DST_2000_04_02: &str = "2000-04-02T03:00:00-07:00[America/Los_Angeles]";
const STD_1999_10_31: &str = "1999-10-31T01:00:00-08:00[America/Los_Angeles]";
const STD_1998_01_31: &str = "1998-10-25T01:00:00-08:00[America/Los_Angeles]";

// Non transitions
const IN_DST_1999_07_31: &str = "1999-07-31T00:00:00-07:00[America/Los_Angeles]";
const AFTER_DST_1999_12_31: &str = "1999-12-31T00:00:00-08:00[America/Los_Angeles]";
const BEFORE_DST_1999_01_31: &str = "1999-01-31T00:00:00-08:00[America/Los_Angeles]";

const LONDON_TRANSITION_1968_02_18: &str = "1968-02-18T03:00:00+01:00[Europe/London]";
const LONDON_TRANSITION_1968_02_18_MINUS_ONE: &str =
    "1968-02-18T01:59:59.999999999+00:00[Europe/London]";

const SAMOA_IDL_CHANGE: &str = "2011-12-31T00:00:00+14:00[Pacific/Apia]";
const SAMOA_IDL_CHANGE_MINUS_ONE: &str = "2011-12-29T23:59:59.999999999-10:00[Pacific/Apia]";

// This date is a fifth Sunday
const LONDON_POSIX_TRANSITION_2019_03_31_MINUS_ONE: &str =
    "2019-03-31T00:59:59.999999999+00:00[Europe/London]";
const LONDON_POSIX_TRANSITION_2019_03_31: &str = "2019-03-31T02:00:00+01:00[Europe/London]";

// This date is a fourth (but last) Sunday
const LONDON_POSIX_TRANSITION_2017_03_26_MINUS_ONE: &str =
    "2017-03-26T00:59:59.999999999+00:00[Europe/London]";
const LONDON_POSIX_TRANSITION_2017_03_26: &str = "2017-03-26T02:00:00+01:00[Europe/London]";

const TROLL_FIRST_TRANSITION: &str = "2005-03-27T03:00:00+02:00[Antarctica/Troll]";

/// Vancouver transitions on the first Sunday in November, which may or may not be
/// before the first Friday in November
const VANCOUVER_FIRST_FRIDAY_IN_NOVEMBER_BEFORE_SUNDAY: &str =
    "2019-11-01T00:00:00-07:00[America/Vancouver]";
const VANCOUVER_FIRST_FRIDAY_IN_NOVEMBER_AFTER_SUNDAY: &str =
    "2019-11-06T00:00:00-08:00[America/Vancouver]";

/// Chile tzdb has a transition on the "first saturday in april", except the transition occurs
/// at 24:00:00, which is, of course, the day after. This is not the same thing as the first Sunday in April
const SANTIAGO_DST_2024: &str = "2024-09-08T01:00:00-03:00[America/Santiago]";
const SANTIAGO_STD_2025_APRIL: &str = "2025-04-05T23:00:00-04:00[America/Santiago]";
const SANTIAGO_STD_2025_APRIL_PLUS_ONE: &str =
    "2025-04-05T23:00:00.000000001-04:00[America/Santiago]";
const SANTIAGO_STD_2025_APRIL_MINUS_ONE: &str =
    "2025-04-05T23:59:59.999999999-03:00[America/Santiago]";
const SANTIAGO_DST_2025_SEPT: &str = "2025-09-07T01:00:00-03:00[America/Santiago]";
const SANTIAGO_DST_2025_SEPT_PLUS_ONE: &str =
    "2025-09-07T01:00:00.000000001-03:00[America/Santiago]";
const SANTIAGO_DST_2025_SEPT_MINUS_ONE: &str =
    "2025-09-06T23:59:59.999999999-04:00[America/Santiago]";
const SANTIAGO_STD_2026: &str = "2026-04-04T23:00:00-04:00[America/Santiago]";

// MUST only contain full strings
// The second boolean is whether these are unambiguous when the offset is removed
// As a rule of thumb, anything around an STD->DST transition
// will be unambiguous, but DST->STD will not
const TO_STRING_TESTCASES: &[(&str, bool)] = &[
    (DST_2025_03_09, true),
    (DST_2026_03_08, true),
    (STD_2025_11_02, false),
    (STD_2024_11_03, false),
    (IN_DST_2025_07_31, true),
    (AFTER_DST_2025_12_31, true),
    (BEFORE_DST_2025_01_31, true),
    (DST_2025_03_09_PLUS_ONE, true),
    (DST_2025_03_09_MINUS_ONE, true),
    (STD_2025_11_02_PLUS_ONE, false),
    (STD_2025_11_02_MINUS_ONE, false),
    (DST_1999_04_04, true),
    (DST_2000_04_02, true),
    (STD_1999_10_31, false),
    (STD_1998_01_31, false),
    (IN_DST_1999_07_31, true),
    (AFTER_DST_1999_12_31, true),
    (BEFORE_DST_1999_01_31, true),
    (LONDON_TRANSITION_1968_02_18, true),
    (LONDON_TRANSITION_1968_02_18_MINUS_ONE, true),
    (SAMOA_IDL_CHANGE, true),
    (SAMOA_IDL_CHANGE_MINUS_ONE, true),
    (LONDON_POSIX_TRANSITION_2019_03_31_MINUS_ONE, true),
    (LONDON_POSIX_TRANSITION_2019_03_31, true),
    (LONDON_POSIX_TRANSITION_2017_03_26_MINUS_ONE, true),
    (LONDON_POSIX_TRANSITION_2017_03_26_MINUS_ONE, true),
    (TROLL_FIRST_TRANSITION, true),
    (VANCOUVER_FIRST_FRIDAY_IN_NOVEMBER_BEFORE_SUNDAY, true),
    (VANCOUVER_FIRST_FRIDAY_IN_NOVEMBER_AFTER_SUNDAY, true),
    (SANTIAGO_DST_2024, true),
    (SANTIAGO_STD_2025_APRIL, false),
    (SANTIAGO_STD_2025_APRIL_PLUS_ONE, false),
    (SANTIAGO_STD_2025_APRIL_MINUS_ONE, false),
    (SANTIAGO_DST_2025_SEPT, true),
    (SANTIAGO_DST_2025_SEPT_PLUS_ONE, true),
    (SANTIAGO_DST_2025_SEPT_MINUS_ONE, true),
    (SANTIAGO_STD_2026, false),
];

#[test]
fn get_time_zone_transition() {
    // This stops it from wrapping
    use TransitionDirection::*;

    test_all_providers!(provider: {
        // Modern dates that utilize the posix string

        // During DST
        let zdt = parse_zdt_with_reject(IN_DST_2025_07_31, provider).unwrap();
        assert_tr(&zdt, Previous, DST_2025_03_09, provider);
        assert_tr(&zdt, Next, STD_2025_11_02, provider);

        // After DST
        let zdt = parse_zdt_with_reject(AFTER_DST_2025_12_31, provider).unwrap();
        assert_tr(&zdt, Previous, STD_2025_11_02, provider);
        assert_tr(&zdt, Next, DST_2026_03_08, provider);

        // Before DST
        let zdt = parse_zdt_with_reject(BEFORE_DST_2025_01_31, provider).unwrap();
        assert_tr(&zdt, Previous, STD_2024_11_03, provider);
        assert_tr(&zdt, Next, DST_2025_03_09, provider);

        // Boundary test
        // Modern date (On start of DST)
        let zdt = parse_zdt_with_reject(DST_2025_03_09, provider).unwrap();
        assert_tr(&zdt, Previous, STD_2024_11_03, provider);
        assert_tr(&zdt, Next, STD_2025_11_02, provider);
        // Modern date (one ns after DST)
        let zdt = parse_zdt_with_reject(DST_2025_03_09_PLUS_ONE, provider).unwrap();
        assert_tr(&zdt, Previous, DST_2025_03_09, provider);
        assert_tr(&zdt, Next, STD_2025_11_02, provider);
        // Modern date (one ns before DST)
        let zdt = parse_zdt_with_reject(DST_2025_03_09_MINUS_ONE, provider).unwrap();
        assert_tr(&zdt, Previous, STD_2024_11_03, provider);
        assert_tr(&zdt, Next, DST_2025_03_09, provider);

        // Modern date (On start of STD)
        let zdt = parse_zdt_with_reject(STD_2025_11_02, provider).unwrap();
        assert_tr(&zdt, Previous, DST_2025_03_09, provider);
        assert_tr(&zdt, Next, DST_2026_03_08, provider);
        // Modern date (one ns after STD)
        let zdt = parse_zdt_with_reject(STD_2025_11_02_PLUS_ONE, provider).unwrap();
        assert_tr(&zdt, Previous, STD_2025_11_02, provider);
        assert_tr(&zdt, Next, DST_2026_03_08, provider);
        // Modern date (one ns before STD)
        let zdt = parse_zdt_with_reject(STD_2025_11_02_MINUS_ONE, provider).unwrap();
        assert_tr(&zdt, Previous, DST_2025_03_09, provider);
        assert_tr(&zdt, Next, STD_2025_11_02, provider);

        // Old dates using the Tzif data

        // During DST
        let zdt = parse_zdt_with_reject(IN_DST_1999_07_31, provider).unwrap();
        assert_tr(&zdt, Previous, DST_1999_04_04, provider);
        assert_tr(&zdt, Next, STD_1999_10_31, provider);

        // After DST
        let zdt = parse_zdt_with_reject(AFTER_DST_1999_12_31, provider).unwrap();
        assert_tr(&zdt, Previous, STD_1999_10_31, provider);
        assert_tr(&zdt, Next, DST_2000_04_02, provider);

        // Before DST
        let zdt = parse_zdt_with_reject(BEFORE_DST_1999_01_31, provider).unwrap();
        assert_tr(&zdt, Previous, STD_1998_01_31, provider);
        assert_tr(&zdt, Next, DST_1999_04_04, provider);

        // Santiago tests, testing that transitions with the offset = 24:00:00
        // still work
        let zdt = parse_zdt_with_reject(SANTIAGO_DST_2025_SEPT_MINUS_ONE, provider).unwrap();
        assert_tr(&zdt, Previous, SANTIAGO_STD_2025_APRIL, provider);
        assert_tr(&zdt, Next, SANTIAGO_DST_2025_SEPT, provider);
        let zdt = parse_zdt_with_reject(SANTIAGO_DST_2025_SEPT, provider).unwrap();
        assert_tr(&zdt, Previous, SANTIAGO_STD_2025_APRIL, provider);
        assert_tr(&zdt, Next, SANTIAGO_STD_2026, provider);
        let zdt = parse_zdt_with_reject(SANTIAGO_DST_2025_SEPT_PLUS_ONE, provider).unwrap();
        assert_tr(&zdt, Previous, SANTIAGO_DST_2025_SEPT, provider);
        assert_tr(&zdt, Next, SANTIAGO_STD_2026, provider);

        let zdt = parse_zdt_with_reject(SANTIAGO_STD_2025_APRIL_MINUS_ONE, provider).unwrap();
        assert_tr(&zdt, Previous, SANTIAGO_DST_2024, provider);
        assert_tr(&zdt, Next, SANTIAGO_STD_2025_APRIL, provider);
        let zdt = parse_zdt_with_reject(SANTIAGO_STD_2025_APRIL, provider).unwrap();
        assert_tr(&zdt, Previous, SANTIAGO_DST_2024, provider);
        assert_tr(&zdt, Next, SANTIAGO_DST_2025_SEPT, provider);
        let zdt = parse_zdt_with_reject(SANTIAGO_STD_2025_APRIL_PLUS_ONE, provider).unwrap();
        assert_tr(&zdt, Previous, SANTIAGO_STD_2025_APRIL, provider);
        assert_tr(&zdt, Next, SANTIAGO_DST_2025_SEPT, provider);

        // Test case from intl402/Temporal/ZonedDateTime/prototype/getTimeZoneTransition/rule-change-without-offset-transition
        // This ensures we skip "fake" transition entries that do not actually change the offset

        let zdt =
            parse_zdt_with_reject("1970-01-01T01:00:00+01:00[Europe/London]", provider).unwrap();
        assert_tr(&zdt, Previous, LONDON_TRANSITION_1968_02_18, provider);
        let zdt =
            parse_zdt_with_reject("1968-10-01T00:00:00+01:00[Europe/London]", provider).unwrap();
        assert_tr(
            &zdt,
            Next,
            "1971-10-31T02:00:00+00:00[Europe/London]",
            provider,
        );
        let zdt = parse_zdt_with_reject("1967-05-01T00:00:00-10:00[America/Anchorage]", provider)
            .unwrap();
        assert_tr(
            &zdt,
            Previous,
            "1945-09-30T01:00:00-10:00[America/Anchorage]",
            provider,
        );
        let zdt = parse_zdt_with_reject("1967-01-01T00:00:00-10:00[America/Anchorage]", provider)
            .unwrap();
        assert_tr(
            &zdt,
            Next,
            "1969-04-27T03:00:00-09:00[America/Anchorage]",
            provider,
        );
        // These dates are one second after a "fake" transition at the end of the tzif data
        // Ensure that they find a real transition, not the fake one
        let zdt = parse_zdt_with_reject("2020-11-01T00:00:01-07:00[America/Whitehorse]", provider)
            .unwrap();
        assert_tr(
            &zdt,
            Previous,
            "2020-03-08T03:00:00-07:00[America/Whitehorse]",
            provider,
        );

        let zdt =
            parse_zdt_with_reject("1996-05-13T00:00:01+03:00[Europe/Kyiv]", provider).unwrap();
        assert_tr(
            &zdt,
            Previous,
            "1996-03-31T03:00:00+03:00[Europe/Kyiv]",
            provider,
        );

        // This ensures that nanosecond-to-second casting works correctly
        let zdt = parse_zdt_with_reject(LONDON_TRANSITION_1968_02_18_MINUS_ONE, provider).unwrap();
        assert_tr(&zdt, Next, LONDON_TRANSITION_1968_02_18, provider);
        assert_tr(
            &zdt,
            Previous,
            "1967-10-29T02:00:00+00:00[Europe/London]",
            provider,
        );
    })
}

#[test]
fn test_to_string_roundtrip() {
    test_all_providers!(provider: {
        for (test, is_unambiguous) in TO_STRING_TESTCASES {
            let zdt = parse_zdt_with_reject(test, provider).expect(test);
            let string = zdt.to_string_with_provider(provider).unwrap();

            assert_eq!(
                *test, &*string,
                "ZonedDateTime {test} round trips on ToString"
            );
            let without_offset = zdt
                .to_ixdtf_string_with_provider(
                    DisplayOffset::Never,
                    DisplayTimeZone::Auto,
                    DisplayCalendar::Never,
                    Default::default(),
                    provider
                )
                .unwrap();
            assert_eq!(
                without_offset[0..19],
                test[0..19],
                "Stringified object should have same date part"
            );

            // These testcases should all also parse unambiguously when the offset is removed.
            if *is_unambiguous {
                let zdt = parse_zdt_with_reject(&without_offset, provider).expect(test);
                let string = zdt.to_string_with_provider(provider).unwrap();
                assert_eq!(
                    *test, &*string,
                    "ZonedDateTime {without_offset} round trips to {test} on ToString"
                );
            }
        }
    });
}

#[test]
fn test_apia() {
    test_all_providers!(provider: {
        // This transition skips an entire day
        // From: 2011-12-29T23:59:59.999999999-10:00[Pacific/Apia]
        // To: 2011-12-31T00:00:00+14:00[Pacific/Apia]
        let zdt = parse_zdt_with_reject(SAMOA_IDL_CHANGE, provider).unwrap();
        let _ = zdt
            .add_with_provider(&Duration::new(0, 0, 0, 1, 1, 0, 0, 0, 0, 0).unwrap(), None, provider)
            .unwrap();

        assert_eq!(zdt.hours_in_day_with_provider(provider).unwrap(), 24.);

        let samoa_before = parse_zdt_with_reject(SAMOA_IDL_CHANGE_MINUS_ONE, provider).unwrap();
        assert_eq!(samoa_before.hours_in_day_with_provider(provider).unwrap(), 24.);
    })
}

#[test]
fn test_london() {
    test_all_providers!(provider: {
        // Europe/London has an MWD transition where the transitions are on the
        // last sundays of March and October.

        // Test that they correctly compute from nanoseconds
        let zdt = ZonedDateTime::try_new_with_provider(
            1_553_993_999_999_999_999,
            TimeZone::try_from_str_with_provider("Europe/London", provider).unwrap(),
            Calendar::ISO,
            provider,
        )
        .unwrap();
        assert_eq!(
            zdt.to_string_with_provider(provider).unwrap(),
            LONDON_POSIX_TRANSITION_2019_03_31_MINUS_ONE,
        );
        let zdt = ZonedDateTime::try_new_with_provider(
            1_553_994_000_000_000_000,
            TimeZone::try_from_str_with_provider("Europe/London", provider).unwrap(),
            Calendar::ISO,
            provider,
        )
        .unwrap();
        assert_eq!(zdt.to_string_with_provider(provider).unwrap(), LONDON_POSIX_TRANSITION_2019_03_31,);

        // Test that they correctly compute from ZDT strings without explicit offset
        let zdt = parse_zdt_with_reject("2019-03-31T00:59:59.999999999[Europe/London]", provider)
            .unwrap();
        assert_eq!(
            zdt.to_string_with_provider(provider).unwrap(),
            LONDON_POSIX_TRANSITION_2019_03_31_MINUS_ONE
        );

        let zdt =
            parse_zdt_with_reject("2019-03-31T02:00:00+01:00[Europe/London]", provider).unwrap();
        assert_eq!(zdt.to_string_with_provider(provider).unwrap(), LONDON_POSIX_TRANSITION_2019_03_31);

        let zdt = parse_zdt_with_reject("2017-03-26T00:59:59.999999999[Europe/London]", provider)
            .unwrap();
        assert_eq!(
            zdt.to_string_with_provider(provider).unwrap(),
            LONDON_POSIX_TRANSITION_2017_03_26_MINUS_ONE
        );

        let zdt =
            parse_zdt_with_reject("2017-03-26T02:00:00+01:00[Europe/London]", provider).unwrap();
        assert_eq!(zdt.to_string_with_provider(provider).unwrap(), LONDON_POSIX_TRANSITION_2017_03_26);
    })
}

#[test]
fn test_berlin() {
    test_all_providers!(provider: {
        // Need to ensure that when the transition is the last day of the month it still works
        let zdt = parse_zdt_with_reject("2021-03-28T01:00:00Z[Europe/Berlin]", provider).unwrap();
        let prev = zdt
            .get_time_zone_transition_with_provider(TransitionDirection::Previous, provider)
            .unwrap()
            .unwrap();

        assert_eq!(prev.to_string_with_provider(provider).unwrap(), "2020-10-25T02:00:00+01:00[Europe/Berlin]");
    })
}

#[test]
fn test_troll() {
    test_all_providers!(provider: {
        // Antarctica/Troll started DST in 2005, but had no other transitions before that
        let zdt = ZonedDateTime::try_new_with_provider(
            0,
            TimeZone::try_from_str_with_provider("Antarctica/Troll", provider).unwrap(),
            Calendar::ISO,
            provider,
        )
        .unwrap();

        let next = zdt
            .get_time_zone_transition_with_provider(TransitionDirection::Next, provider)
            .unwrap()
            .unwrap();
        assert_eq!(next.to_string_with_provider(provider).unwrap(), TROLL_FIRST_TRANSITION);
    })
}

#[test]
fn test_zdt_until_rounding() {
    test_all_providers!(provider: {
        // Regression test for beyondDaySpan rounding behavior
        let start = parse_zdt_with_reject("2020-01-01T00:00-08:00[-08:00]", provider).unwrap();
        let end = parse_zdt_with_reject("2020-01-03T23:59-08:00[-08:00]", provider).unwrap();
        let difference = start
            .until_with_provider(
                &end,
                DifferenceSettings {
                    largest_unit: Some(Unit::Day),
                    smallest_unit: Some(Unit::Hour),
                    rounding_mode: Some(RoundingMode::HalfExpand),
                    ..Default::default()
                },
                provider
            )
            .unwrap();
        assert_eq!(difference.to_string(), "P3D");
    })
}

#[test]
fn test_toronto_half_hour() {
    test_all_providers!(provider: {
        let zdt =
            parse_zdt_with_reject("1919-03-30T12:00:00-05:00[America/Toronto]", provider).unwrap();
        assert_eq!(zdt.hours_in_day_with_provider(provider).unwrap(), 23.5);
    })
}

#[test]
fn test_round_to_start_of_day() {
    test_all_providers!(provider: {
        // Round up to DST
        let zdt =
            parse_zdt_with_reject("1919-03-30T11:45-05:00[America/Toronto]", provider).unwrap();
        let rounded = zdt
            .round_with_provider(RoundingOptions {
                smallest_unit: Some(Unit::Day),
                ..Default::default()
            }, provider)
            .unwrap();
        let known_rounded =
            parse_zdt_with_reject("1919-03-31T00:30:00-04:00[America/Toronto]", provider).unwrap();

        assert!(
            rounded.equals_with_provider(&known_rounded, provider).unwrap(),
            "Expected {}, found {}",
            known_rounded.to_string_with_provider(provider).unwrap(),
            rounded.to_string_with_provider(provider).unwrap()
        );
        assert_eq!(rounded.get_iso_datetime(), known_rounded.get_iso_datetime());

        // Round down (ensure the offset picked is the correct one)
        // See https://github.com/boa-dev/temporal/pull/520
        let zdt =
            parse_zdt_with_reject("1919-03-30T01:45-05:00[America/Toronto]", provider).unwrap();
        let rounded = zdt
            .round_with_provider(RoundingOptions {
                smallest_unit: Some(Unit::Day),
                ..Default::default()
            }, provider)
            .unwrap();
        let known_rounded =
            parse_zdt_with_reject("1919-03-30T00:00:00-05:00[America/Toronto]", provider).unwrap();

        assert!(
            rounded.equals_with_provider(&known_rounded, provider).unwrap(),
            "Expected {}, found {}",
            known_rounded.to_string_with_provider(provider).unwrap(),
            rounded.to_string_with_provider(provider).unwrap()
        );
        assert_eq!(rounded.get_iso_datetime(), known_rounded.get_iso_datetime());
    })
}
