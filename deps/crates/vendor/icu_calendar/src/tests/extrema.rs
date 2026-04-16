// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::calendar_arithmetic::{CONSTRUCTOR_YEAR_RANGE, GENEROUS_YEAR_RANGE, VALID_RD_RANGE};
use crate::duration::DateDuration;
use crate::error::DateAddError;
use crate::error::DateFromFieldsError;
use crate::options::{
    DateAddOptions, DateDifferenceOptions, DateDurationUnit, DateFromFieldsOptions, Overflow,
};
use crate::types::{DateFields, Month, YearInput};
use crate::Date;
use calendrical_calculations::gregorian::fixed_from_gregorian;
use calendrical_calculations::rata_die::RataDie;

// Minimum and maximum dates allowed in ECMA-262 Temporal.
// https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date#the_epoch_timestamps_and_invalid_date
const MIN_TEMPORAL: RataDie = fixed_from_gregorian(1970, 1, 1).add(-100_000_000);
const MAX_TEMPORAL: RataDie = fixed_from_gregorian(1970, 1, 1).add(100_000_000);

super::test_all_cals!(
    fn check_ecma_extrema<C: Calendar + Copy>(cal: C) {
        // Round-trips
        assert_eq!(
            Date::from_rata_die(MIN_TEMPORAL, cal).to_rata_die(),
            MIN_TEMPORAL
        );
        assert_eq!(
            Date::from_rata_die(MAX_TEMPORAL, cal).to_rata_die(),
            MAX_TEMPORAL
        );
    }
);

super::test_all_cals!(
    fn check_representation_extrema<C: Calendar + Copy>(cal: C) {
        // Round-trips
        assert_eq!(
            Date::from_rata_die(*VALID_RD_RANGE.start(), cal).to_rata_die(),
            *VALID_RD_RANGE.start()
        );
        assert_eq!(
            Date::from_rata_die(*VALID_RD_RANGE.end(), cal).to_rata_die(),
            *VALID_RD_RANGE.end()
        );

        // Saturates
        assert_eq!(
            Date::from_rata_die(*VALID_RD_RANGE.start() - 1, cal).to_rata_die(),
            *VALID_RD_RANGE.start()
        );
        assert_eq!(
            Date::from_rata_die(*VALID_RD_RANGE.end() + 1, cal).to_rata_die(),
            *VALID_RD_RANGE.end()
        );
    }
);

// Add one `unit` to a signed duration (in the direction of its sign)
fn nudge_duration_by_unit(
    mut duration: DateDuration,
    unit: DateDurationUnit,
    value: i32,
) -> DateDuration {
    match unit {
        DateDurationUnit::Years => duration.years = duration.years.saturating_add_signed(value),
        DateDurationUnit::Months => duration.months = duration.months.saturating_add_signed(value),
        DateDurationUnit::Weeks => duration.weeks = duration.weeks.saturating_add_signed(value),
        DateDurationUnit::Days => duration.days = duration.days.saturating_add_signed(value),
    }
    duration
}

const RDS_TO_TEST: &[RataDie] = &[
    *VALID_RD_RANGE.start(),
    VALID_RD_RANGE.start().add(1),
    VALID_RD_RANGE.start().add(5),
    VALID_RD_RANGE.start().add(100),
    VALID_RD_RANGE.start().add(10000),
    RataDie::new(-1000),
    RataDie::new(0),
    RataDie::new(1000),
    VALID_RD_RANGE.end().add(-10000),
    VALID_RD_RANGE.end().add(-100),
    VALID_RD_RANGE.end().add(-5),
    VALID_RD_RANGE.end().add(-1),
    *VALID_RD_RANGE.end(),
];

const CONSTRAIN: DateAddOptions = DateAddOptions {
    overflow: Some(Overflow::Constrain),
};

const REJECT: DateAddOptions = DateAddOptions {
    overflow: Some(Overflow::Reject),
};

super::test_all_cals!(
    #[ignore] // slow
    fn check_added_extrema<C: Calendar + Copy>(cal: C) {
        let min_date = Date::from_rata_die(*VALID_RD_RANGE.start(), cal);
        let max_date = Date::from_rata_die(*VALID_RD_RANGE.end(), cal);

        for start_date in RDS_TO_TEST {
            let start_date = Date::from_rata_die(*start_date, cal);

            for unit in [
                DateDurationUnit::Years,
                // This is very very slow right now
                // https://github.com/unicode-org/icu4x/issues/7077
                // DateDurationUnit::Months,
                DateDurationUnit::Weeks,
                DateDurationUnit::Days,
            ] {
                let options = DateDifferenceOptions {
                    largest_unit: Some(unit),
                    ..Default::default()
                };
                let min_duration = start_date
                    .try_until_with_options(&min_date, options)
                    .unwrap();
                let max_duration = start_date
                    .try_until_with_options(&max_date, options)
                    .unwrap();

                // the nudge routine assumes a sign, but zero durations always have positive sign
                // Explicitly set it before nudging
                let mut min_duration_for_nudging = min_duration;
                min_duration_for_nudging.is_negative = true;
                let min_duration_plus_one =
                    nudge_duration_by_unit(min_duration_for_nudging, unit, 1);
                let max_duration_plus_one = nudge_duration_by_unit(max_duration, unit, 1);

                for (date_bound, duration_bound, plus_one, bound) in [
                    (min_date, min_duration, min_duration_plus_one, "min"),
                    (max_date, max_duration, max_duration_plus_one, "max"),
                ] {
                    for overflow in [CONSTRAIN, REJECT] {
                        let added = start_date.try_added_with_options(duration_bound, overflow);
                        if added.is_err() && overflow == REJECT {
                            assert_ne!(added, Err(DateAddError::Overflow), "{start_date:?} + {duration_bound:?} should not produce overflow error in Reject mode");
                        } else {
                            assert_eq!(
                                added,
                                Ok(date_bound),
                                "{start_date:?} + {duration_bound:?} should produce {bound} date"
                            );
                        }

                        let added_plus_one = start_date.try_added_with_options(plus_one, overflow);
                        if overflow == CONSTRAIN {
                            assert_eq!(
                                added_plus_one,
                                Err(DateAddError::Overflow),
                                "{start_date:?} + {plus_one:?} should be out of range"
                            );
                        } else {
                            assert!(
                                added_plus_one.is_err(),
                                "{start_date:?} + {plus_one:?} should be out of range"
                            );
                        }
                    }
                }
            }
        }
    }
);

super::test_all_cals!(
    fn check_generous_duration<C: Calendar + Copy>(cal: C) {
        let generous_max_duration = DateDuration {
            is_negative: false,
            years: crate::calendar_arithmetic::GENEROUS_MAX_YEARS,
            months: crate::calendar_arithmetic::GENEROUS_MAX_MONTHS,
            weeks: 0,
            days: crate::calendar_arithmetic::GENEROUS_MAX_DAYS,
        };

        let mut generous_max_neg_duration = generous_max_duration;
        generous_max_neg_duration.is_negative = true;

        for start_date in RDS_TO_TEST {
            let start_date = Date::from_rata_die(*start_date, cal);

            for generous_duration in [generous_max_duration, generous_max_neg_duration] {
                for overflow in [CONSTRAIN, REJECT] {
                    assert_eq!(start_date.try_added_with_options(generous_duration, overflow), Err(DateAddError::Overflow),
                               "Adding durations from the generously-large range should always fail (but never panic)");
                }
            }
        }
    }
);

super::test_all_cals!(
    fn check_from_fields_extrema<C: Calendar + Copy>(cal: C) {
        let min_date = Date::from_rata_die(*VALID_RD_RANGE.start(), cal);
        let max_date = Date::from_rata_die(*VALID_RD_RANGE.end(), cal);

        let first_era = min_date.year().era().map(|e| e.era);
        let last_era = max_date.year().era().map(|e| e.era);

        let constrain = DateFromFieldsOptions {
            overflow: Some(Overflow::Constrain),
            ..Default::default()
        };
        let reject = DateFromFieldsOptions {
            overflow: Some(Overflow::Reject),
            ..Default::default()
        };

        // First we want to test that large values all get range checked
        for era in [first_era, last_era, None] {
            // We want to ensure that the "early" generous year range check
            // AND the RD check both run but return the same errors.
            for year in [
                *GENEROUS_YEAR_RANGE.start() - 1,
                *GENEROUS_YEAR_RANGE.start(),
                *GENEROUS_YEAR_RANGE.start() + 5,
                *GENEROUS_YEAR_RANGE.end() + 1,
                *GENEROUS_YEAR_RANGE.end(),
                *GENEROUS_YEAR_RANGE.end() - 5,
            ] {
                let mut fields = DateFields {
                    day: Some(1),
                    month: Some(Month::new(1)),
                    ..Default::default()
                };

                if let Some(era) = era.as_ref() {
                    fields.era_year = Some(year);
                    fields.era = Some(era.as_bytes());
                } else {
                    fields.extended_year = Some(year);
                }

                let result_constrain = Date::try_from_fields(fields, constrain, cal);
                assert_eq!(
                    result_constrain,
                    Err(DateFromFieldsError::Overflow),
                    "{year}-01-01, era {era:?} should fail to construct (constrain)"
                );

                let result_reject = Date::try_from_fields(fields, reject, cal);
                assert_eq!(
                    result_reject,
                    Err(DateFromFieldsError::Overflow),
                    "{year}-01-01, era {era:?} should fail to construct (reject)"
                );
            }
        }

        // Next we want to check that the range check applies exactly at the VALID_RD_RANGE
        // border.

        let min_day = min_date.day_of_month().0;
        let min_month = min_date.month().ordinal;
        let min_year = min_date.year().extended_year();

        // Check that the lowest date roundtrips
        let min_fields = DateFields {
            day: Some(min_day),
            ordinal_month: Some(min_month),
            extended_year: Some(min_year),
            ..Default::default()
        };
        let min_constrain = Date::try_from_fields(min_fields, constrain, cal);
        assert_eq!(
            min_constrain,
            Ok(min_date),
            "Min date {min_date:?} should roundtrip via {min_fields:?}"
        );

        // Then check that the date before that does not.
        let min_minus_one = if min_day > 1 {
            DateFields {
                day: Some(min_day - 1),
                ..min_fields
            }
        } else if min_month > 1 {
            DateFields {
                day: Some(50), // Should constrain
                ordinal_month: Some(min_month - 1),
                ..min_fields
            }
        } else {
            DateFields {
                day: Some(50), // Should constrain
                ordinal_month: Some(50),
                extended_year: Some(min_year - 1),
                ..Default::default()
            }
        };
        let min_minus_one_constrain = Date::try_from_fields(min_minus_one, constrain, cal);
        assert_eq!(
            min_minus_one_constrain,
            Err(DateFromFieldsError::Overflow),
            "Min date {min_date:?} minus one should fail to construct via {min_minus_one_constrain:?}"
        );

        let max_day = max_date.day_of_month().0;
        let max_month = max_date.month().ordinal;
        let max_year = max_date.year().extended_year();

        // Check that the highest date roundtrips
        let max_fields = DateFields {
            day: Some(max_day),
            ordinal_month: Some(max_month),
            extended_year: Some(max_year),
            ..Default::default()
        };
        let max_constrain = Date::try_from_fields(max_fields, constrain, cal);
        assert_eq!(
            max_constrain,
            Ok(max_date),
            "Max date {max_date:?} should roundtrip via {max_fields:?}"
        );

        // Then check that the date after that does not.
        let max_plus_one = if max_day < min_date.days_in_month() {
            DateFields {
                day: Some(max_day + 1),
                ..max_fields
            }
        } else if max_month < min_date.months_in_year() {
            DateFields {
                day: Some(1),
                ordinal_month: Some(max_month + 1),
                ..max_fields
            }
        } else {
            DateFields {
                day: Some(1),
                ordinal_month: Some(1),
                extended_year: Some(max_year + 1),
                ..Default::default()
            }
        };
        let max_plus_one_constrain = Date::try_from_fields(max_plus_one, constrain, cal);
        assert_eq!(
            max_plus_one_constrain,
            Err(DateFromFieldsError::Overflow),
            "Min date {max_date:?} minus one should fail to construct via {max_plus_one_constrain:?}"
        );
    }
);

super::test_all_cals!(
    fn check_from_codes_extrema<C: Calendar + Copy>(cal: C) {
        // Success
        Date::try_new(
            (*CONSTRUCTOR_YEAR_RANGE.start()).into(),
            Month::new(1),
            1,
            cal,
        )
        .unwrap();
        Date::try_new(
            (*CONSTRUCTOR_YEAR_RANGE.end()).into(),
            Month::new(1),
            1,
            cal,
        )
        .unwrap();

        // Error
        Date::try_new(
            (*CONSTRUCTOR_YEAR_RANGE.start() - 1).into(),
            Month::new(1),
            1,
            cal,
        )
        .unwrap_err();
        Date::try_new(
            (*CONSTRUCTOR_YEAR_RANGE.end() + 1).into(),
            Month::new(1),
            1,
            cal,
        )
        .unwrap_err();

        if let crate::types::YearInfo::Era(y) = Date::try_new(
            (*CONSTRUCTOR_YEAR_RANGE.start()).into(),
            Month::new(1),
            1,
            cal,
        )
        .unwrap()
        .year()
        {
            Date::try_new(
                YearInput::EraYear(&y.era, *CONSTRUCTOR_YEAR_RANGE.start() - 1),
                Month::new(1),
                1,
                cal,
            )
            .unwrap_err();
            Date::try_new(
                YearInput::EraYear(&y.era, *CONSTRUCTOR_YEAR_RANGE.end() + 1),
                Month::new(1),
                1,
                cal,
            )
            .unwrap_err();
        }

        if let crate::types::YearInfo::Era(y) = Date::try_new(
            (*CONSTRUCTOR_YEAR_RANGE.end()).into(),
            Month::new(1),
            1,
            cal,
        )
        .unwrap()
        .year()
        {
            Date::try_new(
                YearInput::EraYear(&y.era, *CONSTRUCTOR_YEAR_RANGE.start() - 1),
                Month::new(1),
                1,
                cal,
            )
            .unwrap_err();
            Date::try_new(
                YearInput::EraYear(&y.era, *CONSTRUCTOR_YEAR_RANGE.end() + 1),
                Month::new(1),
                1,
                cal,
            )
            .unwrap_err();
        }
    }
);

mod check_convenience_constructors {
    use crate::cal::{
        ChineseTraditional, EthiopianEraStyle, Hijri, HijriTabularEpoch, HijriTabularLeapYears,
        Japanese, KoreanTraditional,
    };

    use super::*;
    #[test]
    fn buddhist() {
        Date::try_new_buddhist(*CONSTRUCTOR_YEAR_RANGE.start() - 1, 1, 1).unwrap_err();
        Date::try_new_buddhist(*CONSTRUCTOR_YEAR_RANGE.end() + 1, 1, 1).unwrap_err();
    }
    #[test]
    #[allow(deprecated)]
    fn chinese_traditional() {
        Date::try_new_chinese_traditional(*CONSTRUCTOR_YEAR_RANGE.start() - 1, Month::new(1), 1)
            .unwrap_err();
        Date::try_new_chinese_traditional(*CONSTRUCTOR_YEAR_RANGE.end() + 1, Month::new(1), 1)
            .unwrap_err();
        #[allow(deprecated)]
        {
            let c = ChineseTraditional::new();
            Date::try_new_chinese_with_calendar(*CONSTRUCTOR_YEAR_RANGE.start() - 1, 1, 1, c)
                .unwrap_err();
            Date::try_new_chinese_with_calendar(*CONSTRUCTOR_YEAR_RANGE.end() + 1, 1, 1, c)
                .unwrap_err();
        }
    }
    #[test]
    fn coptic() {
        Date::try_new_coptic(*CONSTRUCTOR_YEAR_RANGE.start() - 1, 1, 1).unwrap_err();
        Date::try_new_coptic(*CONSTRUCTOR_YEAR_RANGE.end() + 1, 1, 1).unwrap_err();
    }
    #[test]
    fn korean_traditional() {
        Date::try_new_korean_traditional(*CONSTRUCTOR_YEAR_RANGE.start() - 1, Month::new(1), 1)
            .unwrap_err();
        Date::try_new_korean_traditional(*CONSTRUCTOR_YEAR_RANGE.end() + 1, Month::new(1), 1)
            .unwrap_err();
        #[allow(deprecated)]
        {
            let c = KoreanTraditional::new();
            Date::try_new_dangi_with_calendar(*CONSTRUCTOR_YEAR_RANGE.start() - 1, 1, 1, c)
                .unwrap_err();
            Date::try_new_dangi_with_calendar(*CONSTRUCTOR_YEAR_RANGE.end() + 1, 1, 1, c)
                .unwrap_err();
        }
    }
    #[test]
    fn ethiopian() {
        Date::try_new_ethiopian(
            EthiopianEraStyle::AmeteMihret,
            *CONSTRUCTOR_YEAR_RANGE.start() - 1,
            1,
            1,
        )
        .unwrap_err();
        Date::try_new_ethiopian(
            EthiopianEraStyle::AmeteMihret,
            *CONSTRUCTOR_YEAR_RANGE.end() + 1,
            1,
            1,
        )
        .unwrap_err();
    }
    #[test]
    fn ethiopian_amete_alem() {
        Date::try_new_ethiopian(
            EthiopianEraStyle::AmeteAlem,
            *CONSTRUCTOR_YEAR_RANGE.start() - 1,
            1,
            1,
        )
        .unwrap_err();
        Date::try_new_ethiopian(
            EthiopianEraStyle::AmeteAlem,
            *CONSTRUCTOR_YEAR_RANGE.end() + 1,
            1,
            1,
        )
        .unwrap_err();
    }
    #[test]
    fn gregorian() {
        Date::try_new_gregorian(*CONSTRUCTOR_YEAR_RANGE.start() - 1, 1, 1).unwrap_err();
        Date::try_new_gregorian(*CONSTRUCTOR_YEAR_RANGE.end() + 1, 1, 1).unwrap_err();
    }
    #[test]
    fn hebrew() {
        Date::try_new_hebrew_v2(*CONSTRUCTOR_YEAR_RANGE.start() - 1, Month::new(1), 1).unwrap_err();
        Date::try_new_hebrew_v2(*CONSTRUCTOR_YEAR_RANGE.end() + 1, Month::new(1), 1).unwrap_err();
        #[allow(deprecated)]
        {
            Date::try_new_hebrew(*CONSTRUCTOR_YEAR_RANGE.start() - 1, 1, 1).unwrap_err();
            Date::try_new_hebrew(*CONSTRUCTOR_YEAR_RANGE.end() + 1, 1, 1).unwrap_err();
        }
    }
    #[test]
    fn hijri_tabular_friday() {
        let c = Hijri::new_tabular(HijriTabularLeapYears::TypeII, HijriTabularEpoch::Friday);
        Date::try_new_hijri_with_calendar(*CONSTRUCTOR_YEAR_RANGE.start() - 1, 1, 1, c)
            .unwrap_err();
        Date::try_new_hijri_with_calendar(*CONSTRUCTOR_YEAR_RANGE.end() + 1, 1, 1, c).unwrap_err();
    }
    #[test]
    fn hijri_tabular_thursday() {
        let c = Hijri::new_tabular(HijriTabularLeapYears::TypeII, HijriTabularEpoch::Thursday);
        Date::try_new_hijri_with_calendar(*CONSTRUCTOR_YEAR_RANGE.start() - 1, 1, 1, c)
            .unwrap_err();
        Date::try_new_hijri_with_calendar(*CONSTRUCTOR_YEAR_RANGE.end() + 1, 1, 1, c).unwrap_err();
    }
    #[test]
    fn hijri_uaq() {
        let c = Hijri::new_umm_al_qura();
        Date::try_new_hijri_with_calendar(*CONSTRUCTOR_YEAR_RANGE.start() - 1, 1, 1, c)
            .unwrap_err();
        Date::try_new_hijri_with_calendar(*CONSTRUCTOR_YEAR_RANGE.end() + 1, 1, 1, c).unwrap_err();
    }
    #[test]
    fn indian() {
        Date::try_new_indian(*CONSTRUCTOR_YEAR_RANGE.start() - 1, 1, 1).unwrap_err();
        Date::try_new_indian(*CONSTRUCTOR_YEAR_RANGE.end() + 1, 1, 1).unwrap_err();
    }
    #[test]
    fn iso() {
        Date::try_new_iso(*CONSTRUCTOR_YEAR_RANGE.start() - 1, 1, 1).unwrap_err();
        Date::try_new_iso(*CONSTRUCTOR_YEAR_RANGE.end() + 1, 1, 1).unwrap_err();
    }
    #[test]
    fn julian() {
        Date::try_new_julian(*CONSTRUCTOR_YEAR_RANGE.start() - 1, 1, 1).unwrap_err();
        Date::try_new_julian(*CONSTRUCTOR_YEAR_RANGE.end() + 1, 1, 1).unwrap_err();
    }
    #[test]
    fn japanese() {
        let cal = Japanese::new();
        Date::try_new_japanese_with_calendar(
            "reiwa",
            *CONSTRUCTOR_YEAR_RANGE.start() - 1,
            1,
            1,
            cal,
        )
        .unwrap_err();
        Date::try_new_japanese_with_calendar("reiwa", *CONSTRUCTOR_YEAR_RANGE.end() + 1, 1, 1, cal)
            .unwrap_err();
    }
    #[test]
    fn persian() {
        Date::try_new_persian(*CONSTRUCTOR_YEAR_RANGE.start() - 1, 1, 1).unwrap_err();
        Date::try_new_persian(*CONSTRUCTOR_YEAR_RANGE.end() + 1, 1, 1).unwrap_err();
    }
    #[test]
    fn roc() {
        Date::try_new_roc(*CONSTRUCTOR_YEAR_RANGE.start() - 1, 1, 1).unwrap_err();
        Date::try_new_roc(*CONSTRUCTOR_YEAR_RANGE.end() + 1, 1, 1).unwrap_err();
    }
}
