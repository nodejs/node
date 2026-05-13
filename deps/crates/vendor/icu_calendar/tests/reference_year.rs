// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use std::{collections::HashSet, fmt::Debug};

use icu_calendar::{
    cal::*,
    error::DateFromFieldsError,
    options::{DateFromFieldsOptions, MissingFieldsStrategy, Overflow},
    types::{DateFields, MonthCode},
    Calendar, Date, Ref,
};

/// Test that a given calendar produces valid monthdays
///
/// `valid_md_condition`, given (month_number, is_leap, day_number), should return whether or not
/// that combination is ever possible in that calendar
fn test_reference_year_impl<C>(cal: C, valid_md_condition: impl Fn(u8, bool, u8) -> bool)
where
    C: Calendar + Debug,
{
    // Test that all dates in a certain range behave according to Temporal
    let mut month_days_seen = HashSet::new();
    let mut rd = Date::try_new_iso(1972, 12, 31).unwrap().to_rata_die();
    for _ in 1..2000 {
        let date = Date::from_rata_die(rd, Ref(&cal));
        let month_day = (date.month().standard_code, date.day_of_month().0);
        let mut fields = DateFields::default();
        fields.month_code = Some(month_day.0 .0.as_bytes());
        fields.day = Some(month_day.1);
        let mut options = DateFromFieldsOptions::default();
        options.missing_fields_strategy = Some(MissingFieldsStrategy::Ecma);
        let reference_date = Date::try_from_fields(fields, options, Ref(&cal)).unwrap();
        if month_days_seen.contains(&month_day) {
            assert_ne!(date, reference_date, "{cal:?}");
        } else {
            assert_eq!(date, reference_date, "{cal:?}");
            month_days_seen.insert(month_day);
        }
        rd -= 1;
    }
    // Test that all MonthDay values round-trip
    for month_number in 1..=14 {
        for is_leap in [false, true] {
            let mut valid_day_number = 1;
            let is_valid_month = valid_md_condition(month_number, is_leap, valid_day_number);
            for day_number in 1..=32 {
                if valid_md_condition(month_number, is_leap, day_number) {
                    valid_day_number = day_number;
                }
                let mut fields = DateFields::default();
                let mc = match is_leap {
                    false => MonthCode::new_normal(month_number),
                    true => MonthCode::new_leap(month_number),
                };
                fields.month_code = mc.as_ref().map(|m| m.0.as_bytes());
                fields.day = Some(day_number);
                let mut options = DateFromFieldsOptions::default();
                options.overflow = Some(Overflow::Constrain);
                options.missing_fields_strategy = Some(MissingFieldsStrategy::Ecma);
                let reference_date = match Date::try_from_fields(fields, options, Ref(&cal)) {
                    Ok(d) => {
                        assert!(
                            is_valid_month,
                            "try_from_fields passed but should have failed: {fields:?} => {d:?}"
                        );
                        d
                    }
                    Err(DateFromFieldsError::MonthCodeNotInCalendar) => {
                        assert!(
                            !is_valid_month,
                            "try_from_fields failed but should have passed: {fields:?}"
                        );
                        continue;
                    }
                    Err(e) => {
                        panic!("Unexpected error in month day from fields: {e}");
                    }
                };

                // Test round-trip (to valid day number)
                assert_eq!(
                    fields.month_code.unwrap(),
                    reference_date.month().standard_code.0.as_bytes(),
                    "{fields:?} {cal:?}"
                );
                assert_eq!(
                    valid_day_number,
                    reference_date.day_of_month().0,
                    "{fields:?} {cal:?}"
                );

                // Test Overflow::Reject
                options.overflow = Some(Overflow::Reject);
                let reject_result = Date::try_from_fields(fields, options, Ref(&cal));
                if valid_day_number == day_number {
                    assert_eq!(reject_result, Ok(reference_date));
                } else {
                    assert!(matches!(
                        reject_result,
                        Err(DateFromFieldsError::Range { .. })
                    ))
                }

                // Test that ordinal months cause it to fail (even if the month code is still set)
                fields.ordinal_month = Some(month_number);
                let ordinal_result = Date::try_from_fields(fields, options, Ref(&cal));
                assert!(matches!(
                    ordinal_result,
                    Err(DateFromFieldsError::NotEnoughFields)
                ));
            }
        }
    }
}

fn gregorian_md_condition(month_number: u8, is_leap: bool, day_number: u8) -> bool {
    // No leap months
    if is_leap {
        return false;
    }

    match month_number {
        1 | 3 | 5 | 7 | 8 | 10 | 12 => day_number <= 31,
        2 => day_number <= 29,
        4 | 6 | 9 | 11 => day_number <= 30,
        _ => {
            assert!(month_number > 12);
            // No other months
            false
        }
    }
}

fn chinese_md_condition(month_number: u8, _is_leap: bool, day_number: u8) -> bool {
    month_number <= 12 && day_number <= 30
}

fn coptic_md_condition(month_number: u8, is_leap: bool, day_number: u8) -> bool {
    // No leap months
    if is_leap {
        return false;
    }
    match month_number {
        1..=12 => day_number <= 30,
        13 => day_number <= 6,
        _ => false,
    }
}

fn hijri_md_condition(month_number: u8, is_leap: bool, day_number: u8) -> bool {
    // No leap months
    if is_leap {
        return false;
    }
    month_number <= 12 && day_number <= 30
}

fn hijri_tabular_md_condition(month_number: u8, is_leap: bool, day_number: u8) -> bool {
    // No leap months
    if is_leap {
        return false;
    }

    if month_number > 12 {
        return false;
    }

    // Odd months have 30 days, even months have 29, except for M12 in a leap year
    if month_number % 2 == 0 {
        if month_number == 12 {
            day_number <= 30
        } else {
            day_number <= 29
        }
    } else {
        day_number <= 30
    }
}

fn hebrew_md_condition(month_number: u8, is_leap: bool, day_number: u8) -> bool {
    if is_leap {
        return month_number == 5 && day_number <= 30;
    }
    match month_number {
        1 | 2 | 3 | 5 | 7 | 9 | 11 => day_number <= 30,
        // Tevet, Adar, Iyar, Tammuz, Elul
        4 | 6 | 8 | 10 | 12 => day_number <= 29,
        _ => {
            assert!(month_number > 12);
            // No other months
            false
        }
    }
}

#[test]
fn test_reference_year_buddhist() {
    test_reference_year_impl(Buddhist, gregorian_md_condition)
}

#[test]
fn test_reference_year_chinese() {
    test_reference_year_impl(ChineseTraditional::new(), chinese_md_condition)
}

#[test]
fn test_reference_year_coptic() {
    test_reference_year_impl(Coptic, coptic_md_condition)
}

#[test]
fn test_reference_year_korean() {
    test_reference_year_impl(KoreanTraditional::new(), chinese_md_condition)
}

#[test]
fn test_reference_year_ethiopian() {
    test_reference_year_impl(Ethiopian::new(), coptic_md_condition)
}

#[test]
fn test_reference_year_ethiopian_amete_alem() {
    test_reference_year_impl(
        Ethiopian::new_with_era_style(EthiopianEraStyle::AmeteAlem),
        coptic_md_condition,
    )
}

#[test]
fn test_reference_year_gregorian() {
    test_reference_year_impl(Gregorian, gregorian_md_condition)
}

#[test]
fn test_reference_year_julian() {
    test_reference_year_impl(Julian, gregorian_md_condition)
}

#[test]
fn test_reference_year_hebrew() {
    test_reference_year_impl(Hebrew, hebrew_md_condition)
}

#[test]
fn test_reference_year_indian() {
    test_reference_year_impl(Indian, |month_number, is_leap, day_number| {
        if is_leap {
            // No leap months
            return false;
        }
        // First half of the year has long months, second half short
        if month_number <= 6 {
            day_number <= 31
        } else if month_number <= 12 {
            day_number <= 30
        } else {
            // No larger months
            false
        }
    })
}

#[test]
fn test_reference_year_hijri_tabular_type_ii_friday() {
    test_reference_year_impl(
        Hijri::new_tabular(HijriTabularLeapYears::TypeII, HijriTabularEpoch::Friday),
        hijri_tabular_md_condition,
    )
}

#[test]
fn test_reference_year_hijri_tabular_type_ii_thursday() {
    test_reference_year_impl(
        Hijri::new_tabular(HijriTabularLeapYears::TypeII, HijriTabularEpoch::Thursday),
        hijri_tabular_md_condition,
    )
}

#[test]
fn test_reference_year_hijri_umm_al_qura() {
    test_reference_year_impl(Hijri::new_umm_al_qura(), hijri_md_condition)
}

#[test]
fn test_reference_year_iso() {
    test_reference_year_impl(Iso, gregorian_md_condition)
}

#[test]
fn test_reference_year_japanese() {
    test_reference_year_impl(Japanese::new(), gregorian_md_condition)
}

#[test]
fn test_reference_year_japanese_extended() {
    test_reference_year_impl(JapaneseExtended::new(), gregorian_md_condition)
}

#[test]
fn test_reference_year_persian() {
    test_reference_year_impl(Persian, |month_number, is_leap, day_number| {
        if is_leap {
            // No leap months
            return false;
        }
        // First half of the year has long months, second half short
        if month_number <= 6 {
            day_number <= 31
        } else if month_number <= 12 {
            day_number <= 30
        } else {
            // No larger months
            false
        }
    })
}

#[test]
fn test_reference_year_roc() {
    test_reference_year_impl(Roc, gregorian_md_condition)
}
