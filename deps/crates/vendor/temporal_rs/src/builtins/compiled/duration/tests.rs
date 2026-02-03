use crate::{
    duration::DateDuration,
    options::{
        OffsetDisambiguation, RelativeTo, RoundingIncrement, RoundingMode, RoundingOptions, Unit,
    },
    partial::PartialDuration,
    Calendar, PlainDate, TimeZone, ZonedDateTime,
};

use core::{num::NonZeroU32, str::FromStr};

use super::Duration;

fn get_round_result(
    test_duration: &Duration,
    relative_to: RelativeTo,
    options: RoundingOptions,
) -> Duration {
    test_duration.round(options, Some(relative_to)).unwrap()
}

fn assert_duration(result: Duration, values: (i64, i64, i64, i64, i64, i64, i64, i64, i128, i128)) {
    assert_eq!(
        (
            result.years(),
            result.months(),
            result.weeks(),
            result.days(),
            result.hours(),
            result.minutes(),
            result.seconds(),
            result.milliseconds(),
            result.microseconds(),
            result.nanoseconds()
        ),
        values
    )
}

// roundingmode-floor.js
#[test]
fn basic_positive_floor_rounding_v2() {
    let test_duration = Duration::new(5, 6, 7, 8, 40, 30, 20, 123, 987, 500).unwrap();
    let forward_date = PlainDate::new(2020, 4, 1, Calendar::default()).unwrap();

    let relative_forward = RelativeTo::PlainDate(forward_date);

    let mut options = RoundingOptions {
        largest_unit: None,
        smallest_unit: None,
        increment: None,
        rounding_mode: Some(RoundingMode::Floor),
    };

    let _ = options.smallest_unit.insert(Unit::Year);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_duration(result, (5, 0, 0, 0, 0, 0, 0, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Month);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_duration(result, (5, 7, 0, 0, 0, 0, 0, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Week);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_duration(result, (5, 7, 3, 0, 0, 0, 0, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Day);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_duration(result, (5, 7, 0, 27, 0, 0, 0, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Hour);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_duration(result, (5, 7, 0, 27, 16, 0, 0, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Minute);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_duration(result, (5, 7, 0, 27, 16, 30, 0, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Second);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_duration(result, (5, 7, 0, 27, 16, 30, 20, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Millisecond);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_duration(result, (5, 7, 0, 27, 16, 30, 20, 123, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Microsecond);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_duration(result, (5, 7, 0, 27, 16, 30, 20, 123, 987, 0));

    let _ = options.smallest_unit.insert(Unit::Nanosecond);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_duration(result, (5, 7, 0, 27, 16, 30, 20, 123, 987, 500));
}

#[test]
fn basic_negative_floor_rounding_v2() {
    // Test setup
    let test_duration = Duration::new(5, 6, 7, 8, 40, 30, 20, 123, 987, 500).unwrap();
    let backward_date = PlainDate::new(2020, 12, 1, Calendar::default()).unwrap();

    let relative_backward = RelativeTo::PlainDate(backward_date);

    let mut options = RoundingOptions {
        largest_unit: None,
        smallest_unit: None,
        increment: None,
        rounding_mode: Some(RoundingMode::Floor),
    };

    let _ = options.smallest_unit.insert(Unit::Year);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_duration(result, (-6, 0, 0, 0, 0, 0, 0, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Month);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_duration(result, (-5, -8, 0, 0, 0, 0, 0, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Week);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_duration(result, (-5, -7, -4, 0, 0, 0, 0, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Day);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_duration(result, (-5, -7, 0, -28, 0, 0, 0, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Hour);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_duration(result, (-5, -7, 0, -27, -17, 0, 0, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Minute);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_duration(result, (-5, -7, 0, -27, -16, -31, 0, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Second);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_duration(result, (-5, -7, 0, -27, -16, -30, -21, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Millisecond);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_duration(result, (-5, -7, 0, -27, -16, -30, -20, -124, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Microsecond);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_duration(result, (-5, -7, 0, -27, -16, -30, -20, -123, -988, 0));

    let _ = options.smallest_unit.insert(Unit::Nanosecond);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_duration(result, (-5, -7, 0, -27, -16, -30, -20, -123, -987, -500));
}

// roundingmode-ceil.js
#[test]
fn basic_positive_ceil_rounding() {
    let test_duration = Duration::new(5, 6, 7, 8, 40, 30, 20, 123, 987, 500).unwrap();
    let forward_date = PlainDate::new(2020, 4, 1, Calendar::from_str("iso8601").unwrap()).unwrap();

    let relative_forward = RelativeTo::PlainDate(forward_date);

    let mut options = RoundingOptions {
        largest_unit: None,
        smallest_unit: None,
        increment: None,
        rounding_mode: Some(RoundingMode::Ceil),
    };

    let _ = options.smallest_unit.insert(Unit::Year);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_duration(result, (6, 0, 0, 0, 0, 0, 0, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Month);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_duration(result, (5, 8, 0, 0, 0, 0, 0, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Week);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_duration(result, (5, 7, 4, 0, 0, 0, 0, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Day);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_duration(result, (5, 7, 0, 28, 0, 0, 0, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Hour);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_duration(result, (5, 7, 0, 27, 17, 0, 0, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Minute);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_duration(result, (5, 7, 0, 27, 16, 31, 0, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Second);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_duration(result, (5, 7, 0, 27, 16, 30, 21, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Millisecond);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_duration(result, (5, 7, 0, 27, 16, 30, 20, 124, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Microsecond);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_duration(result, (5, 7, 0, 27, 16, 30, 20, 123, 988, 0));

    let _ = options.smallest_unit.insert(Unit::Nanosecond);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_duration(result, (5, 7, 0, 27, 16, 30, 20, 123, 987, 500));
}

#[test]
fn basic_negative_ceil_rounding() {
    let test_duration = Duration::new(5, 6, 7, 8, 40, 30, 20, 123, 987, 500).unwrap();
    let backward_date =
        PlainDate::new(2020, 12, 1, Calendar::from_str("iso8601").unwrap()).unwrap();
    let relative_backward = RelativeTo::PlainDate(backward_date);

    let mut options = RoundingOptions {
        largest_unit: None,
        smallest_unit: None,
        increment: None,
        rounding_mode: Some(RoundingMode::Ceil),
    };

    let _ = options.smallest_unit.insert(Unit::Year);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_duration(result, (-5, 0, 0, 0, 0, 0, 0, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Month);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_duration(result, (-5, -7, 0, 0, 0, 0, 0, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Week);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_duration(result, (-5, -7, -3, 0, 0, 0, 0, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Day);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_duration(result, (-5, -7, 0, -27, 0, 0, 0, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Hour);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_duration(result, (-5, -7, 0, -27, -16, 0, 0, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Minute);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_duration(result, (-5, -7, 0, -27, -16, -30, 0, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Second);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_duration(result, (-5, -7, 0, -27, -16, -30, -20, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Millisecond);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_duration(result, (-5, -7, 0, -27, -16, -30, -20, -123, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Microsecond);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_duration(result, (-5, -7, 0, -27, -16, -30, -20, -123, -987, 0));

    let _ = options.smallest_unit.insert(Unit::Nanosecond);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_duration(result, (-5, -7, 0, -27, -16, -30, -20, -123, -987, -500));
}

// roundingmode-expand.js
#[test]
fn basic_positive_expand_rounding() {
    let test_duration = Duration::new(5, 6, 7, 8, 40, 30, 20, 123, 987, 500).unwrap();
    let forward_date = PlainDate::new(2020, 4, 1, Calendar::from_str("iso8601").unwrap()).unwrap();
    let relative_forward = RelativeTo::PlainDate(forward_date);

    let mut options = RoundingOptions {
        largest_unit: None,
        smallest_unit: None,
        increment: None,
        rounding_mode: Some(RoundingMode::Expand),
    };

    let _ = options.smallest_unit.insert(Unit::Year);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_duration(result, (6, 0, 0, 0, 0, 0, 0, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Month);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_duration(result, (5, 8, 0, 0, 0, 0, 0, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Week);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_duration(result, (5, 7, 4, 0, 0, 0, 0, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Day);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_duration(result, (5, 7, 0, 28, 0, 0, 0, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Hour);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_duration(result, (5, 7, 0, 27, 17, 0, 0, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Minute);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_duration(result, (5, 7, 0, 27, 16, 31, 0, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Second);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_duration(result, (5, 7, 0, 27, 16, 30, 21, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Millisecond);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_duration(result, (5, 7, 0, 27, 16, 30, 20, 124, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Microsecond);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_duration(result, (5, 7, 0, 27, 16, 30, 20, 123, 988, 0));

    let _ = options.smallest_unit.insert(Unit::Nanosecond);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_duration(result, (5, 7, 0, 27, 16, 30, 20, 123, 987, 500));
}

#[test]
fn basic_negative_expand_rounding() {
    let test_duration = Duration::new(5, 6, 7, 8, 40, 30, 20, 123, 987, 500).unwrap();

    let backward_date =
        PlainDate::new(2020, 12, 1, Calendar::from_str("iso8601").unwrap()).unwrap();

    let relative_backward = RelativeTo::PlainDate(backward_date);

    let mut options = RoundingOptions {
        largest_unit: None,
        smallest_unit: None,
        increment: None,
        rounding_mode: Some(RoundingMode::Expand),
    };

    let _ = options.smallest_unit.insert(Unit::Year);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_duration(result, (-6, 0, 0, 0, 0, 0, 0, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Month);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_duration(result, (-5, -8, 0, 0, 0, 0, 0, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Week);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_duration(result, (-5, -7, -4, 0, 0, 0, 0, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Day);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_duration(result, (-5, -7, 0, -28, 0, 0, 0, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Hour);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_duration(result, (-5, -7, 0, -27, -17, 0, 0, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Minute);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_duration(result, (-5, -7, 0, -27, -16, -31, 0, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Second);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_duration(result, (-5, -7, 0, -27, -16, -30, -21, 0, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Millisecond);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_duration(result, (-5, -7, 0, -27, -16, -30, -20, -124, 0, 0));

    let _ = options.smallest_unit.insert(Unit::Microsecond);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_duration(result, (-5, -7, 0, -27, -16, -30, -20, -123, -988, 0));

    let _ = options.smallest_unit.insert(Unit::Nanosecond);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_duration(result, (-5, -7, 0, -27, -16, -30, -20, -123, -987, -500));
}

#[test]
fn rounding_to_fractional_day_tests() {
    let twenty_five_hours = Duration::from_hours(25);
    let options = RoundingOptions {
        largest_unit: Some(Unit::Day),
        smallest_unit: None,
        increment: None,
        rounding_mode: Some(RoundingMode::Floor),
    };
    let result = twenty_five_hours.round(options, None).unwrap();
    assert_duration(result, (0, 0, 0, 1, 1, 0, 0, 0, 0, 0));

    let sixty_days = Duration::from(DateDuration::new_unchecked(0, 0, 0, 64));
    let options = RoundingOptions {
        largest_unit: None,
        smallest_unit: Some(Unit::Day),
        increment: Some(RoundingIncrement(NonZeroU32::new(5).unwrap())),
        rounding_mode: Some(RoundingMode::Floor),
    };
    let result = sixty_days.round(options, None).unwrap();
    assert_duration(result, (0, 0, 0, 60, 0, 0, 0, 0, 0, 0));

    let sixty_days = Duration::from(DateDuration::new_unchecked(0, 0, 0, 64));
    let options = RoundingOptions {
        largest_unit: None,
        smallest_unit: Some(Unit::Day),
        increment: Some(RoundingIncrement(NonZeroU32::new(10).unwrap())),
        rounding_mode: Some(RoundingMode::Floor),
    };
    let result = sixty_days.round(options, None).unwrap();
    assert_duration(result, (0, 0, 0, 60, 0, 0, 0, 0, 0, 0));

    let sixty_days = Duration::from(DateDuration::new_unchecked(0, 0, 0, 64));
    let options = RoundingOptions {
        largest_unit: None,
        smallest_unit: Some(Unit::Day),
        increment: Some(RoundingIncrement(NonZeroU32::new(10).unwrap())),
        rounding_mode: Some(RoundingMode::Ceil),
    };
    let result = sixty_days.round(options, None).unwrap();
    assert_duration(result, (0, 0, 0, 70, 0, 0, 0, 0, 0, 0));

    let sixty_days = Duration::from(DateDuration::new_unchecked(0, 0, 0, 1000));
    let options = RoundingOptions {
        largest_unit: None,
        smallest_unit: Some(Unit::Day),
        increment: Some(RoundingIncrement(NonZeroU32::new(1_000_000_000).unwrap())),
        rounding_mode: Some(RoundingMode::Expand),
    };
    let result = sixty_days.round(options, None).unwrap();
    assert_duration(result, (0, 0, 0, 1_000_000_000, 0, 0, 0, 0, 0, 0));
}

// test262/test/built-ins/Temporal/Duration/prototype/round/roundingincrement-non-integer.js
#[test]
fn rounding_increment_non_integer() {
    let test_duration = Duration::from(DateDuration::new(0, 0, 0, 1).unwrap());
    let binding = PlainDate::new(2000, 1, 1, Calendar::from_str("iso8601").unwrap()).unwrap();
    let relative_to = RelativeTo::PlainDate(binding);

    let mut options = RoundingOptions {
        largest_unit: None,
        smallest_unit: Some(Unit::Day),
        increment: None,
        rounding_mode: Some(RoundingMode::Expand),
    };

    let _ = options
        .increment
        .insert(RoundingIncrement::try_from(2.5).unwrap());
    let result = test_duration
        .round(options, Some(relative_to.clone()))
        .unwrap();

    assert_duration(result, (0, 0, 0, 2, 0, 0, 0, 0, 0, 0));

    let _ = options
        .increment
        .insert(RoundingIncrement::try_from(1e9 + 0.5).unwrap());
    let result = test_duration.round(options, Some(relative_to)).unwrap();
    assert_duration(result, (0, 0, 0, 1_000_000_000, 0, 0, 0, 0, 0, 0));
}

#[test]
fn basic_add_duration() {
    let base = Duration::new(0, 0, 0, 1, 0, 5, 0, 0, 0, 0).unwrap();
    let other = Duration::new(0, 0, 0, 2, 0, 5, 0, 0, 0, 0).unwrap();
    let result = base.add(&other).unwrap();
    assert_eq!(result.days(), 3);
    assert_eq!(result.minutes(), 10);

    let other = Duration::new(0, 0, 0, -3, 0, -15, 0, 0, 0, 0).unwrap();
    let result = base.add(&other).unwrap();
    assert_eq!(result.days(), -2);
    assert_eq!(result.minutes(), -10);
}

#[test]
fn basic_subtract_duration() {
    let base = Duration::new(0, 0, 0, 3, 0, 15, 0, 0, 0, 0).unwrap();
    let other = Duration::new(0, 0, 0, 1, 0, 5, 0, 0, 0, 0).unwrap();
    let result = base.subtract(&other).unwrap();
    assert_eq!(result.days(), 2);
    assert_eq!(result.minutes(), 10);

    let other = Duration::new(0, 0, 0, -3, 0, -15, 0, 0, 0, 0).unwrap();
    let result = base.subtract(&other).unwrap();
    assert_eq!(result.days(), 6);
    assert_eq!(result.minutes(), 30);
}

// days-24-hours-relative-to-zoned-date-time.js
#[test]
fn round_relative_to_zoned_datetime() {
    let duration = Duration::from_hours(25);
    let zdt = ZonedDateTime::try_new(
        1_000_000_000_000_000_000,
        TimeZone::try_from_str("+04:30").unwrap(),
        Calendar::default(),
    )
    .unwrap();
    let options = RoundingOptions {
        largest_unit: Some(Unit::Day),
        smallest_unit: None,
        rounding_mode: None,
        increment: None,
    };
    let result = duration
        .round(options, Some(RelativeTo::ZonedDateTime(zdt)))
        .unwrap();
    // Result duration should be: (0, 0, 0, 1, 1, 0, 0, 0, 0, 0)
    assert_eq!(result.days(), 1);
    assert_eq!(result.hours(), 1);
}

#[test]
fn test_duration_total() {
    let d1 = Duration::from_partial_duration(PartialDuration {
        hours: Some(130),
        minutes: Some(20),
        ..Default::default()
    })
    .unwrap();
    assert_eq!(d1.total(Unit::Second, None).unwrap(), 469200.0);

    // How many 24-hour days is 123456789 seconds?
    let d2 = Duration::from_str("PT123456789S").unwrap();
    assert_eq!(d2.total(Unit::Day, None).unwrap(), 1428.8980208333332);

    // Find totals in months, with and without taking DST into account
    let d3 = Duration::from_partial_duration(PartialDuration {
        hours: Some(2756),
        ..Default::default()
    })
    .unwrap();
    let relative_to = ZonedDateTime::from_utf8(
        b"2020-01-01T00:00+01:00[Europe/Rome]",
        Default::default(),
        OffsetDisambiguation::Reject,
    )
    .unwrap();
    assert_eq!(
        d3.total(Unit::Month, Some(RelativeTo::ZonedDateTime(relative_to)))
            .unwrap(),
        3.7958333333333334
    );
    assert_eq!(
        d3.total(
            Unit::Month,
            Some(RelativeTo::PlainDate(
                PlainDate::new(2020, 1, 1, Calendar::default()).unwrap()
            ))
        )
        .unwrap(),
        3.7944444444444443
    );
}

// balance-subseconds.js
#[test]
fn balance_subseconds() {
    // Test positive
    let pos = Duration::from_partial_duration(PartialDuration {
        milliseconds: Some(999),
        microseconds: Some(999999),
        nanoseconds: Some(999999999),
        ..Default::default()
    })
    .unwrap();
    assert_eq!(pos.total(Unit::Second, None).unwrap(), 2.998998999);

    // Test negative
    let neg = Duration::from_partial_duration(PartialDuration {
        milliseconds: Some(-999),
        microseconds: Some(-999999),
        nanoseconds: Some(-999999999),
        ..Default::default()
    })
    .unwrap();
    assert_eq!(neg.total(Unit::Second, None).unwrap(), -2.998998999);
}

// balances-days-up-to-both-years-and-months.js
#[test]
fn balance_days_up_to_both_years_and_months() {
    // Test positive
    let two_years = Duration::from_partial_duration(PartialDuration {
        months: Some(11),
        days: Some(396),
        ..Default::default()
    })
    .unwrap();

    let relative_to = PlainDate::new(2017, 1, 1, Calendar::default()).unwrap();

    assert_eq!(
        two_years
            .total(Unit::Year, Some(RelativeTo::PlainDate(relative_to.clone())))
            .unwrap(),
        2.0
    );

    // Test negative
    let two_years_negative = Duration::from_partial_duration(PartialDuration {
        months: Some(-11),
        days: Some(-396),
        ..Default::default()
    })
    .unwrap();

    assert_eq!(
        two_years_negative
            .total(Unit::Year, Some(RelativeTo::PlainDate(relative_to.clone())))
            .unwrap(),
        -2.0
    );
}

// relativeto-plaindate-add24hourdaystoTimeDuration-out-of-range.js
#[test]
fn add_normalized_time_duration_out_of_range() {
    let duration = Duration::from_partial_duration(PartialDuration {
        years: Some(1),
        seconds: Some(9_007_199_254_740_990_i64),
        ..Default::default()
    })
    .unwrap();

    let relative_to = PlainDate::new(2000, 1, 1, Calendar::default()).unwrap();

    let err = duration.total(Unit::Day, Some(RelativeTo::PlainDate(relative_to)));
    assert!(err.is_err())
}

#[test]
fn test_rounding_boundaries() {
    let relative_to = PlainDate::new(2000, 1, 1, Calendar::default()).unwrap();

    // test year
    let duration = Duration::from_partial_duration(PartialDuration {
        years: Some((u32::MAX - 1) as i64),
        ..Default::default()
    })
    .unwrap();

    let options = RoundingOptions {
        smallest_unit: Some(Unit::Week),
        ..Default::default()
    };
    let err = duration.round(options, Some(RelativeTo::PlainDate(relative_to.clone())));
    assert!(err.is_err());

    // test month
    let duration = Duration::from_partial_duration(PartialDuration {
        months: Some((u32::MAX - 1) as i64),
        ..Default::default()
    })
    .unwrap();

    let options = RoundingOptions {
        smallest_unit: Some(Unit::Week),
        ..Default::default()
    };
    let err = duration.round(options, Some(RelativeTo::PlainDate(relative_to.clone())));
    assert!(err.is_err());

    // test week
    let duration = Duration::from_partial_duration(PartialDuration {
        weeks: Some((u32::MAX - 1) as i64),
        ..Default::default()
    })
    .unwrap();

    let options = RoundingOptions {
        smallest_unit: Some(Unit::Week),
        ..Default::default()
    };
    let err = duration.round(options, Some(RelativeTo::PlainDate(relative_to.clone())));
    assert!(err.is_err());

    // test calendar max
    let duration = Duration::from_partial_duration(PartialDuration {
        years: Some((u32::MAX - 1) as i64),
        months: Some((u32::MAX - 1) as i64),
        weeks: Some((u32::MAX - 1) as i64),
        ..Default::default()
    })
    .unwrap();

    let options = RoundingOptions {
        smallest_unit: Some(Unit::Week),
        ..Default::default()
    };
    let err = duration.round(options, Some(RelativeTo::PlainDate(relative_to.clone())));
    assert!(err.is_err());

    // test days for safety
    let duration = Duration::from_partial_duration(PartialDuration {
        years: Some((u32::MAX - 1) as i64),
        months: Some((u32::MAX - 1) as i64),
        weeks: Some((u32::MAX - 1) as i64),
        days: Some(104_249_991_374),
        ..Default::default()
    })
    .unwrap();

    let options = RoundingOptions {
        smallest_unit: Some(Unit::Week),
        ..Default::default()
    };
    let err = duration.round(options, Some(RelativeTo::PlainDate(relative_to.clone())));
    assert!(err.is_err());

    // test minimum bounds with days for safety
    let min_calendar_value = -((u32::MAX - 1) as i64);
    let duration = Duration::from_partial_duration(PartialDuration {
        years: Some(min_calendar_value),
        months: Some(min_calendar_value),
        weeks: Some(min_calendar_value),
        days: Some(-104_249_991_374),
        ..Default::default()
    })
    .unwrap();

    let options = RoundingOptions {
        smallest_unit: Some(Unit::Week),
        ..Default::default()
    };
    let err = duration.round(options, Some(RelativeTo::PlainDate(relative_to)));
    assert!(err.is_err());
}

#[test]
fn test_duration_compare_boundary() {
    let relative_to = PlainDate::new(2000, 1, 1, Calendar::default()).unwrap();
    let zero = Duration::default();

    let max_days = 2i64.pow(53) / 86_400;
    let max_duration = Duration::new(0, 0, 1, max_days, 0, 0, 0, 0, 0, 0).unwrap();
    let err = zero.compare(
        &max_duration,
        Some(RelativeTo::PlainDate(relative_to.clone())),
    );
    assert!(err.is_err());
    let err = max_duration.compare(&zero, Some(RelativeTo::PlainDate(relative_to.clone())));
    assert!(err.is_err());

    let min_days = -(2i64.pow(53) / 86_400);
    let min_duration = Duration::new(0, 0, -1, min_days, 0, 0, 0, 0, 0, 0).unwrap();
    let err = zero.compare(
        &min_duration,
        Some(RelativeTo::PlainDate(relative_to.clone())),
    );
    assert!(err.is_err());
    let err = min_duration.compare(&zero, Some(RelativeTo::PlainDate(relative_to.clone())));
    assert!(err.is_err());
}

#[test]
fn rounding_cross_boundary() {
    let relative_to = PlainDate::new(2022, 1, 1, Calendar::default()).unwrap();

    let duration = Duration::from(DateDuration::new(1, 11, 0, 24).unwrap());
    let options = RoundingOptions {
        smallest_unit: Some(Unit::Month),
        rounding_mode: Some(RoundingMode::Expand),
        ..Default::default()
    };
    let result = duration
        .round(options, Some(RelativeTo::PlainDate(relative_to)))
        .unwrap();
    assert_duration(result, (2, 0, 0, 0, 0, 0, 0, 0, 0, 0));
}

#[test]
fn rounding_cross_boundary_negative() {
    let relative_to = PlainDate::new(2022, 1, 1, Calendar::default()).unwrap();

    let duration = Duration::from(DateDuration::new(-1, -11, 0, -24).unwrap());
    let options = RoundingOptions {
        smallest_unit: Some(Unit::Month),
        rounding_mode: Some(RoundingMode::Expand),
        ..Default::default()
    };
    let result = duration
        .round(options, Some(RelativeTo::PlainDate(relative_to)))
        .unwrap();
    assert_duration(result, (-2, 0, 0, 0, 0, 0, 0, 0, 0, 0));
}

#[test]
fn rounding_cross_boundary_time_units() {
    let duration = Duration::new(0, 0, 0, 0, 1, 59, 59, 900, 0, 0).unwrap();
    let options = RoundingOptions {
        smallest_unit: Some(Unit::Second),
        rounding_mode: Some(RoundingMode::Expand),
        ..Default::default()
    };
    let result = duration.round(options, None).unwrap();
    assert_duration(result, (0, 0, 0, 0, 2, 0, 0, 0, 0, 0));

    let neg_duration = Duration::new(0, 0, 0, 0, -1, -59, -59, -900, 0, 0).unwrap();
    let result = neg_duration.round(options, None).unwrap();
    assert_duration(result, (0, 0, 0, 0, -2, 0, 0, 0, 0, 0));
}

#[test]
fn nudge_past_end() {
    // built-ins/Temporal/Duration/prototype/round/next-day-out-of-range
    let duration = Duration::default();
    let relative_to = ZonedDateTime::try_new(
        86_40000_00000_00000_00000,
        TimeZone::try_from_str("UTC").unwrap(),
        Default::default(),
    )
    .unwrap();
    let options = RoundingOptions {
        largest_unit: Some(Unit::Day),
        smallest_unit: Some(Unit::Minute),
        increment: None,
        rounding_mode: None,
    };
    // This constructs an endDate of MAX_DATE + 1 day (even though the sign is zero).
    // This must error.
    let rounded = duration.round(options, Some(relative_to.into()));
    assert!(
        rounded.is_err(),
        "Expected rounding to fail, got {rounded:?}"
    );
}

#[test]
fn bubble_smallest_becomes_day() {
    // built-ins/Temporal/Duration/prototype/round/bubble-time-unit
    let duration = Duration::new(0, 0, 0, 0, /* hours = */ 14, 0, 0, 0, 0, 0).unwrap();
    let relative_to = PlainDate::try_new(2025, 6, 14, Default::default()).unwrap();
    let options = RoundingOptions {
        largest_unit: None,
        smallest_unit: Some(Unit::Hour),
        increment: Some(RoundingIncrement::try_new(12).unwrap()),
        rounding_mode: Some(RoundingMode::Ceil),
    };
    // This constructs an endDate of MAX_DATE + 1 day (even though the sign is zero).
    // This must error.
    let rounded = duration.round(options, Some(relative_to.into())).unwrap();
    assert_eq!(
        rounded.hours(),
        24,
        "Expected rounding to fail, got {rounded:?}"
    );
}
