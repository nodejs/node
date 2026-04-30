use core::str::FromStr;

use crate::{
    options::{RoundingIncrement, RoundingOptions, ToStringRoundingOptions, Unit},
    parsers::Precision,
    partial::PartialDuration,
    provider::NeverProvider,
};

use super::Duration;

#[test]
fn partial_duration_empty() {
    let err = Duration::from_partial_duration(PartialDuration::default());
    assert!(err.is_err())
}

#[test]
fn partial_duration_values() {
    let mut partial = PartialDuration::default();
    let _ = partial.years.insert(20);
    let result = Duration::from_partial_duration(partial).unwrap();
    assert_eq!(result.years(), 20);
}

#[test]
fn default_duration_string() {
    let duration = Duration::default();

    let options = ToStringRoundingOptions {
        precision: Precision::Auto,
        smallest_unit: None,
        rounding_mode: None,
    };
    let result = duration.as_temporal_string(options).unwrap();
    assert_eq!(&result, "PT0S");

    let options = ToStringRoundingOptions {
        precision: Precision::Digit(0),
        smallest_unit: None,
        rounding_mode: None,
    };
    let result = duration.as_temporal_string(options).unwrap();
    assert_eq!(&result, "PT0S");

    let options = ToStringRoundingOptions {
        precision: Precision::Digit(1),
        smallest_unit: None,
        rounding_mode: None,
    };
    let result = duration.as_temporal_string(options).unwrap();
    assert_eq!(&result, "PT0.0S");

    let options = ToStringRoundingOptions {
        precision: Precision::Digit(3),
        smallest_unit: None,
        rounding_mode: None,
    };
    let result = duration.as_temporal_string(options).unwrap();
    assert_eq!(&result, "PT0.000S");
}

#[test]
fn duration_to_string_auto_precision() {
    let duration = Duration::new(1, 2, 3, 4, 5, 6, 7, 0, 0, 0).unwrap();
    let result = duration
        .as_temporal_string(ToStringRoundingOptions::default())
        .unwrap();
    assert_eq!(&result, "P1Y2M3W4DT5H6M7S");

    let duration = Duration::new(1, 2, 3, 4, 5, 6, 7, 987, 650, 0).unwrap();
    let result = duration
        .as_temporal_string(ToStringRoundingOptions::default())
        .unwrap();
    assert_eq!(&result, "P1Y2M3W4DT5H6M7.98765S");
}

#[test]
fn empty_date_duration() {
    let duration = Duration::from_partial_duration(PartialDuration {
        hours: Some(1.into()),
        ..Default::default()
    })
    .unwrap();
    let result = duration
        .as_temporal_string(ToStringRoundingOptions::default())
        .unwrap();
    assert_eq!(&result, "PT1H");
}

#[test]
fn negative_fields_to_string() {
    let duration = Duration::from_partial_duration(PartialDuration {
        years: Some(-1),
        months: Some(-1),
        weeks: Some(-1),
        days: Some(-1),
        hours: Some(-1),
        minutes: Some(-1),
        seconds: Some(-1),
        milliseconds: Some(-1),
        microseconds: Some(-1),
        nanoseconds: Some(-1),
    })
    .unwrap();
    let result = duration
        .as_temporal_string(ToStringRoundingOptions::default())
        .unwrap();
    assert_eq!(&result, "-P1Y1M1W1DT1H1M1.001001001S");

    let duration = Duration::from_partial_duration(PartialDuration {
        milliseconds: Some(-250),
        ..Default::default()
    })
    .unwrap();
    let result = duration
        .as_temporal_string(ToStringRoundingOptions::default())
        .unwrap();
    assert_eq!(&result, "-PT0.25S");

    let duration = Duration::from_partial_duration(PartialDuration {
        milliseconds: Some(-3500),
        ..Default::default()
    })
    .unwrap();
    let result = duration
        .as_temporal_string(ToStringRoundingOptions::default())
        .unwrap();
    assert_eq!(&result, "-PT3.5S");

    let duration = Duration::from_partial_duration(PartialDuration {
        milliseconds: Some(-3500),
        ..Default::default()
    })
    .unwrap();
    let result = duration
        .as_temporal_string(ToStringRoundingOptions::default())
        .unwrap();
    assert_eq!(&result, "-PT3.5S");

    let duration = Duration::from_partial_duration(PartialDuration {
        weeks: Some(-1),
        days: Some(-1),
        ..Default::default()
    })
    .unwrap();
    let result = duration
        .as_temporal_string(ToStringRoundingOptions::default())
        .unwrap();

    assert_eq!(&result, "-P1W1D");
}

#[test]
fn preserve_precision_loss() {
    const MAX_SAFE_INT: i64 = 9_007_199_254_740_991;
    let duration = Duration::from_partial_duration(PartialDuration {
        milliseconds: Some(MAX_SAFE_INT),
        microseconds: Some(MAX_SAFE_INT as i128),
        ..Default::default()
    })
    .unwrap();
    let result = duration
        .as_temporal_string(ToStringRoundingOptions::default())
        .unwrap();

    assert_eq!(&result, "PT9016206453995.731991S");
}

#[test]
fn duration_from_str() {
    let duration = Duration::from_str("PT0.999999999H").unwrap();
    assert_eq!(duration.minutes(), 59);
    assert_eq!(duration.seconds(), 59);
    assert_eq!(duration.milliseconds(), 999);
    assert_eq!(duration.microseconds(), 996);
    assert_eq!(duration.nanoseconds(), 400);

    let duration = Duration::from_str("PT0.000000011H").unwrap();
    assert_eq!(duration.minutes(), 0);
    assert_eq!(duration.seconds(), 0);
    assert_eq!(duration.milliseconds(), 0);
    assert_eq!(duration.microseconds(), 39);
    assert_eq!(duration.nanoseconds(), 600);

    let duration = Duration::from_str("PT0.999999999M").unwrap();
    assert_eq!(duration.seconds(), 59);
    assert_eq!(duration.milliseconds(), 999);
    assert_eq!(duration.microseconds(), 999);
    assert_eq!(duration.nanoseconds(), 940);
}

#[test]
fn duration_max_safe() {
    const MAX_SAFE_INTEGER: i64 = 9007199254740991;

    // From test262 built-ins/Temporal/Duration/prototype/subtract/result-out-of-range-3.js
    assert!(Duration::new(0, 0, 0, 0, 0, 0, 0, 0, 9_007_199_254_740_991_926_258, 0).is_err());

    // https://github.com/tc39/proposal-temporal/issues/3106#issuecomment-2849349391
    let mut options = RoundingOptions {
        increment: Some(RoundingIncrement::ONE),
        largest_unit: Some(Unit::Nanosecond),
        ..Default::default()
    };
    let d = Duration::new(
        0,
        0,
        0,
        0,
        0,
        0,
        /* s = */ MAX_SAFE_INTEGER,
        0,
        0,
        /* ns = */ 463_129_087,
    )
    .unwrap();
    let _ = d
        .round_with_provider(options, None, &NeverProvider::default())
        .expect("Must successfully round");
    let d = Duration::new(
        0,
        0,
        0,
        0,
        0,
        0,
        /* s = */ MAX_SAFE_INTEGER,
        0,
        0,
        /* ns = */ 463_129_088,
    )
    .unwrap();
    assert!(d
        .round_with_provider(options, None, &NeverProvider::default())
        .is_err());

    options.largest_unit = Some(Unit::Microsecond);
    let _ = d
        .round_with_provider(options, None, &NeverProvider::default())
        .expect("Must successfully round");
    let d = Duration::new(
        0,
        0,
        0,
        0,
        0,
        0,
        /* s = */ MAX_SAFE_INTEGER,
        0,
        /* mis = */ 475_712,
        0,
    )
    .unwrap();
    assert!(d
        .round_with_provider(options, None, &NeverProvider::default())
        .is_err());

    options.largest_unit = Some(Unit::Millisecond);
    let _ = d
        .round_with_provider(options, None, &NeverProvider::default())
        .expect("Must successfully round");
}

// Temporal/Duration/max.js
#[test]
fn duration_max() {
    let cases = [
        (
            Duration::new(0, 0, 0, 104249991374, 7, 36, 31, 999, 999, 999).unwrap(),
            "max days",
            9007199254740991.999999999,
        ),
        (
            Duration::new(0, 0, 0, 0, 2501999792983, 36, 31, 999, 999, 999).unwrap(),
            "max hours",
            9007199254740991.999999999,
        ),
        (
            Duration::new(0, 0, 0, 0, 0, 150119987579016, 31, 999, 999, 999).unwrap(),
            "max minutes",
            9007199254740991.999999999,
        ),
        (
            Duration::new(0, 0, 0, 0, 0, 0, 9007199254740991, 999, 999, 999).unwrap(),
            "max seconds",
            9007199254740991.999999999,
        ),
        (
            Duration::new(0, 0, 0, -104249991374, -7, -36, -31, -999, -999, -999).unwrap(),
            "min days",
            -9007199254740991.999999999,
        ),
        (
            Duration::new(0, 0, 0, 0, -2501999792983, -36, -31, -999, -999, -999).unwrap(),
            "min hours",
            -9007199254740991.999999999,
        ),
        (
            Duration::new(0, 0, 0, 0, 0, -150119987579016, -31, -999, -999, -999).unwrap(),
            "min minutes",
            -9007199254740991.999999999,
        ),
        (
            Duration::new(0, 0, 0, 0, 0, 0, -9007199254740991, -999, -999, -999).unwrap(),
            "min seconds",
            -9007199254740991.999999999,
        ),
    ];

    for (duration, description, result) in cases {
        assert_eq!(
            duration
                .total_with_provider(Unit::Second, None, &NeverProvider::default())
                .unwrap()
                .0,
            result,
            "{description}"
        );
    }
}

#[test]
fn duration_round_negative() {
    let duration = Duration::new(0, 0, 0, 0, -60, 0, 0, 0, 0, 0).unwrap();
    let result = duration
        .round_with_provider(
            RoundingOptions {
                smallest_unit: Some(Unit::Day),
                ..Default::default()
            },
            None,
            &NeverProvider::default(),
        )
        .unwrap();
    assert_eq!(result.days(), -3);
}

#[test]
#[cfg(feature = "compiled_data")]
fn test_duration_compare() {
    use crate::builtins::FS_TZ_PROVIDER;
    use crate::options::{OffsetDisambiguation, RelativeTo};
    use crate::ZonedDateTime;
    use alloc::string::ToString;
    // TODO(#199): Make this work with Windows
    // This should also ideally use the compiled data APIs and live under builtins/compiled
    if cfg!(not(windows)) {
        let one = Duration::from_partial_duration(PartialDuration {
            hours: Some(79),
            minutes: Some(10),
            ..Default::default()
        })
        .unwrap();
        let two = Duration::from_partial_duration(PartialDuration {
            days: Some(3),
            hours: Some(7),
            seconds: Some(630),
            ..Default::default()
        })
        .unwrap();
        let three = Duration::from_partial_duration(PartialDuration {
            days: Some(3),
            hours: Some(6),
            minutes: Some(50),
            ..Default::default()
        })
        .unwrap();

        let mut arr = [&one, &two, &three];
        arr.sort_by(|a, b| Duration::compare_with_provider(a, b, None, &*FS_TZ_PROVIDER).unwrap());
        assert_eq!(
            arr.map(ToString::to_string),
            [&three, &one, &two].map(ToString::to_string)
        );

        // Sorting relative to a date, taking DST changes into account:
        let zdt = ZonedDateTime::from_utf8_with_provider(
            b"2020-11-01T00:00-07:00[America/Los_Angeles]",
            Default::default(),
            OffsetDisambiguation::Reject,
            &*FS_TZ_PROVIDER,
        )
        .unwrap();
        arr.sort_by(|a, b| {
            Duration::compare_with_provider(
                a,
                b,
                Some(RelativeTo::ZonedDateTime(zdt.clone())),
                &*FS_TZ_PROVIDER,
            )
            .unwrap()
        });
        assert_eq!(
            arr.map(ToString::to_string),
            [&one, &three, &two].map(ToString::to_string)
        )
    }
}
/*
TODO: Uncomment

The below test should fail, but currently doesn't. This has to do with weird
floating point math in IsValidDuration Step 6-8 that defers to C++ std::remquo

Needs further clarification.

#[test]
fn duration_round_out_of_range_norm_conversion() {
    const MAX_SAFE_INT: i64 = 9_007_199_254_740_991;
    let duration = Duration::new(0, 0, 0, 0, 0, 0, MAX_SAFE_INT, 0, 0, 999_999_999).unwrap();
    let err = duration.round_with_provider( RoundingOptions {
        largest_unit: Some(Unit::Nanosecond),
        increment: Some(RoundingIncrement::ONE),
        ..Default::default()
    }, None, &NeverProvider::default());
    assert!(err.is_err())
}
*/
