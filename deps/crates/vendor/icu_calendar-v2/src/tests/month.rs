// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#![allow(clippy::field_reassign_with_default)] // use public API

use crate::error::{DateFromFieldsError, MonthCodeParseError};
use crate::options::{DateFromFieldsOptions, MissingFieldsStrategy, Overflow};
use crate::preferences::CalendarAlgorithm;
use crate::types::{DateFields, Month, MonthCode};
use crate::Date;

static INVALID_SYNTAX: &[&str] = &[
    "M", "M0", "M1", "01L", "L01", "M001", "M110", "MxxL", "m01", "M02l",
];

static NOT_IN_ANY_CALENDAR: &[&str] = &["M00", "M14", "M99", "M13L"];

static CHINESE_ONLY: &[&str] = &[
    "M01L", "M02L", "M03L", "M04L", "M06L", "M07L", "M08L", "M09L", "M10L", "M11L", "M12L",
];

static CHINESE_HEBREW: &[&str] = &["M05L"];

static COPTIC_ONLY: &[&str] = &["M13"];

static UNIVERSAL_MONTH_CODES: &[&str] = &[
    "M01", "M02", "M03", "M04", "M05", "M06", "M07", "M08", "M09", "M10", "M11", "M12",
];

#[test]
fn test_month_parsing() {
    for &code in INVALID_SYNTAX {
        let result = Month::try_from_str(code);
        assert_eq!(
            result.unwrap_err(),
            MonthCodeParseError::InvalidSyntax,
            "Should have failed with InvalidSyntax: {code}"
        );
    }

    let valid_syntax = UNIVERSAL_MONTH_CODES
        .iter()
        .chain(NOT_IN_ANY_CALENDAR.iter())
        .chain(CHINESE_ONLY.iter())
        .chain(CHINESE_HEBREW.iter())
        .chain(COPTIC_ONLY.iter())
        .copied();

    for code in valid_syntax {
        let result = Month::try_from_str(code);
        assert!(result.is_ok(), "Should have succeeded to parse: {code}");
        let month = result.unwrap();
        let number = month.number();
        let is_leap = month.is_leap();

        let expected_number = code[1..3].parse::<u8>().unwrap();
        let expected_leap = code.ends_with('L');

        assert_eq!(number, expected_number, "Wrong number for {code}");
        assert_eq!(is_leap, expected_leap, "Wrong leap status for {code}");

        // Test roundtrip
        assert_eq!(
            month.code(),
            MonthCode(code.parse().unwrap()),
            "Roundtrip failed for {code}"
        );
    }
}

crate::tests::test_all_cals!(
    fn test_month_fields<C: Calendar + Copy>(cal: C) {
        let mut valid_month_codes = UNIVERSAL_MONTH_CODES.to_vec();
        let mut invalid_month_codes = NOT_IN_ANY_CALENDAR.to_vec();

        let cal_alg = cal.calendar_algorithm();

        if matches!(
            cal_alg,
            Some(CalendarAlgorithm::Chinese | CalendarAlgorithm::Dangi)
        ) {
            valid_month_codes.extend_from_slice(CHINESE_ONLY);
        } else {
            invalid_month_codes.extend_from_slice(CHINESE_ONLY);
        }

        if matches!(
            cal_alg,
            Some(CalendarAlgorithm::Chinese | CalendarAlgorithm::Dangi | CalendarAlgorithm::Hebrew)
        ) {
            valid_month_codes.extend_from_slice(CHINESE_HEBREW);
        } else {
            invalid_month_codes.extend_from_slice(CHINESE_HEBREW);
        }

        if matches!(
            cal_alg,
            Some(
                CalendarAlgorithm::Coptic
                    | CalendarAlgorithm::Ethiopic
                    | CalendarAlgorithm::Ethioaa
            )
        ) {
            valid_month_codes.extend_from_slice(COPTIC_ONLY);
        } else {
            invalid_month_codes.extend_from_slice(COPTIC_ONLY);
        }

        // Test with full dates
        for extended_year in -100..100 {
            let options = DateFromFieldsOptions::default();
            let mut fields = DateFields::default();
            fields.extended_year = Some(extended_year);
            fields.day = Some(1);
            for month_code in valid_month_codes.iter() {
                fields.month = Some(Month::try_from_str(month_code).unwrap());
                match Date::try_from_fields(fields, options, cal) {
                    Ok(_) => (),
                    Err(DateFromFieldsError::MonthNotInYear) => (),
                    Err(e) => {
                        panic!(
                            "Should have succeeded, but failed: {extended_year} {month_code} {e:?}"
                        );
                    }
                }
            }
            for month_code in invalid_month_codes.iter() {
                let month = Month::try_from_str(month_code).unwrap();
                fields.month = Some(month);
                let result = Date::try_from_fields(fields, options, cal);
                match result {
                    Err(DateFromFieldsError::MonthNotInCalendar) => (),
                    Ok(_) => {
                        panic!("Should have failed, but succeeded: {extended_year} {month_code}");
                    }
                    Err(e) => {
                        panic!("Failed with wrong error: {extended_year} {month_code} {e:?}");
                    }
                }
            }
        }

        // Test with reference year
        let mut fields = DateFields::default();
        fields.day = Some(1);
        let mut options = DateFromFieldsOptions::default();
        options.missing_fields_strategy = Some(MissingFieldsStrategy::Ecma);
        options.overflow = Some(Overflow::Constrain);
        for month_code in valid_month_codes.iter() {
            fields.month = Some(Month::try_from_str(month_code).unwrap());
            match Date::try_from_fields(fields, options, cal) {
                Ok(_) => (),
                Err(e) => {
                    panic!(
                        "Should have succeeded, but failed: {month_code} {e:?} (reference year)"
                    );
                }
            }
        }
        for month_code in invalid_month_codes.iter() {
            fields.month = Some(Month::try_from_str(month_code).unwrap());
            match Date::try_from_fields(fields, options, cal) {
                Err(DateFromFieldsError::MonthNotInCalendar) => (),
                Ok(_) => {
                    panic!("Should have failed, but succeeded: {month_code} (reference year)");
                }
                Err(e) => {
                    panic!("Failed with wrong error: {month_code} {e:?} (reference year)");
                }
            }
        }
    }
);
