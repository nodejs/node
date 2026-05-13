// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::error::DateFromFieldsError;
use crate::options::{DateFromFieldsOptions, MissingFieldsStrategy, Overflow};
use crate::types::DateFields;
use crate::Date;

#[test]
fn test_from_fields_not_enough_fields() {
    // Pick a sufficiently complex calendar.
    let calendar = crate::cal::Hebrew::new();

    let big_i32 = Some(i32::MAX);
    let big_u8 = Some(u8::MAX);
    let small_u8 = Some(1);
    let small_i32 = Some(5000);
    let valid_month_code: Option<&[_]> = Some(b"M01");
    let invalid_month_code: Option<&[_]> = Some(b"M99");

    // We want to ensure that most NotEnoughFields cases return NotEnoughFields
    // even when we're providing out-of-range values, so that
    // this produces TypeError in Temporal as opposed to RangeError.

    for overflow in [Overflow::Reject, Overflow::Constrain] {
        for missing_fields in [MissingFieldsStrategy::Reject, MissingFieldsStrategy::Ecma] {
            let options = DateFromFieldsOptions {
                overflow: Some(overflow),
                missing_fields_strategy: Some(missing_fields),
            };

            // No month data always errors
            assert_eq!(
                Date::try_from_fields(
                    DateFields {
                        era: Some(b"hebrew"),
                        era_year: big_i32,
                        extended_year: None,
                        ordinal_month: None,
                        month_code: None,
                        day: small_u8,
                    },
                    options,
                    calendar
                ),
                Err(DateFromFieldsError::NotEnoughFields),
                "Test with {options:?}"
            );
            assert_eq!(
                Date::try_from_fields(
                    DateFields {
                        era: Some(b"hebrew"),
                        era_year: small_i32,
                        extended_year: None,
                        ordinal_month: None,
                        month_code: None,
                        day: big_u8,
                    },
                    options,
                    calendar
                ),
                Err(DateFromFieldsError::NotEnoughFields),
                "Test with {options:?}"
            );
            assert_eq!(
                Date::try_from_fields(
                    DateFields {
                        era: None,
                        era_year: None,
                        extended_year: big_i32,
                        ordinal_month: None,
                        month_code: None,
                        day: small_u8,
                    },
                    options,
                    calendar
                ),
                Err(DateFromFieldsError::NotEnoughFields),
                "Test with {options:?}"
            );

            // Insufficient era-year data always errors
            assert_eq!(
                Date::try_from_fields(
                    DateFields {
                        era: Some(b"hebrew"),
                        era_year: None,
                        extended_year: None,
                        ordinal_month: big_u8,
                        month_code: None,
                        day: small_u8,
                    },
                    options,
                    calendar
                ),
                Err(DateFromFieldsError::NotEnoughFields),
                "Test with {options:?}"
            );
            assert_eq!(
                Date::try_from_fields(
                    DateFields {
                        era: Some(b"hebrew"),
                        era_year: None,
                        extended_year: None,
                        ordinal_month: small_u8,
                        month_code: None,
                        day: big_u8,
                    },
                    options,
                    calendar
                ),
                Err(DateFromFieldsError::NotEnoughFields),
                "Test with {options:?}"
            );

            // No year info errors for ordinal months regardless of missing fields strategy
            assert_eq!(
                Date::try_from_fields(
                    DateFields {
                        era: None,
                        era_year: None,
                        extended_year: None,
                        ordinal_month: small_u8,
                        month_code: None,
                        day: big_u8,
                    },
                    options,
                    calendar
                ),
                Err(DateFromFieldsError::NotEnoughFields),
                "Test with {options:?}"
            );
            assert_eq!(
                Date::try_from_fields(
                    DateFields {
                        era: None,
                        era_year: None,
                        extended_year: None,
                        ordinal_month: big_u8,
                        month_code: None,
                        day: small_u8,
                    },
                    options,
                    calendar
                ),
                Err(DateFromFieldsError::NotEnoughFields),
                "Test with {options:?}"
            );
            if missing_fields != MissingFieldsStrategy::Ecma {
                // No year info errors only when there is no missing fields strategy
                assert_eq!(
                    Date::try_from_fields(
                        DateFields {
                            era: None,
                            era_year: None,
                            extended_year: None,
                            ordinal_month: None,
                            month_code: valid_month_code,
                            day: big_u8,
                        },
                        options,
                        calendar
                    ),
                    Err(DateFromFieldsError::NotEnoughFields),
                    "Test with {options:?}"
                );
                assert_eq!(
                    Date::try_from_fields(
                        DateFields {
                            era: None,
                            era_year: None,
                            extended_year: None,
                            ordinal_month: None,
                            month_code: invalid_month_code,
                            day: small_u8,
                        },
                        options,
                        calendar
                    ),
                    Err(DateFromFieldsError::NotEnoughFields),
                    "Test with {options:?}"
                );

                // No day info errors only when there is no missing field strategy
                assert_eq!(
                    Date::try_from_fields(
                        DateFields {
                            era: None,
                            era_year: None,
                            extended_year: big_i32,
                            ordinal_month: small_u8,
                            month_code: None,
                            day: None,
                        },
                        options,
                        calendar
                    ),
                    Err(DateFromFieldsError::NotEnoughFields),
                    "Test with {options:?}"
                );
                assert_eq!(
                    Date::try_from_fields(
                        DateFields {
                            era: Some(b"hebrew"),
                            era_year: big_i32,
                            extended_year: None,
                            ordinal_month: small_u8,
                            month_code: None,
                            day: None,
                        },
                        options,
                        calendar
                    ),
                    Err(DateFromFieldsError::NotEnoughFields),
                    "Test with {options:?}"
                );
                assert_eq!(
                    Date::try_from_fields(
                        DateFields {
                            era: Some(b"hebrew"),
                            era_year: small_i32,
                            extended_year: None,
                            ordinal_month: big_u8,
                            month_code: None,
                            day: None,
                        },
                        options,
                        calendar
                    ),
                    Err(DateFromFieldsError::NotEnoughFields),
                    "Test with {options:?}"
                );
            }
        }
    }
}
