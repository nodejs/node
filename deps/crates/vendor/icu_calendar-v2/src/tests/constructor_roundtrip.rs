// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::Calendar;
use crate::Date;
use crate::error::DateFromFieldsError;
use crate::options::{DateFromFieldsOptions, Overflow};
use crate::types::DateFields;
use calendrical_calculations::rata_die::RataDie;

// We could use the `bitflags` crate here, but we are using raw constants
// to avoid pulling in an extra dependency.
type Flags = u32;
const EXTENDED_YEAR: u32 = 1 << 0;
const ERA: u32 = 1 << 1;
const ERA_YEAR: u32 = 1 << 2;
const MONTH: u32 = 1 << 3;
const MONTH_CODE: u32 = 1 << 4;
const ORDINAL_MONTH: u32 = 1 << 5;
const DAY: u32 = 1 << 6;
const MAKE_INCONSISTENT_YEAR: u32 = 1 << 7;
const MAKE_INCONSISTENT_MONTH: u32 = 1 << 8;

fn try_from_fields_helper<C: Calendar + Copy>(
    flags: Flags,
    rd: RataDie,
    options: DateFromFieldsOptions,
    cal: C,
) -> Result<Date<C>, DateFromFieldsError> {
    let date = Date::from_rata_die(rd, cal);
    let mut fields = DateFields::default();

    if flags & EXTENDED_YEAR != 0 {
        fields.extended_year = Some(date.year().extended_year());
    }

    let era_bytes = date.year().era().map(|ey| ey.era);
    if flags & ERA != 0
        && let Some(ref eb) = era_bytes
    {
        fields.era = Some(eb.as_bytes());
    }
    if flags & ERA_YEAR != 0
        && let Some(ey) = date.year().era()
    {
        fields.era_year = Some(ey.year);
    }

    let month_input = date.month().to_input();
    if flags & MONTH != 0 {
        fields.month = Some(month_input);
    }

    let month_code_holder = month_input.code();
    if flags & MONTH_CODE != 0 {
        fields.month_code = Some(month_code_holder.0.as_bytes());
    }
    if flags & ORDINAL_MONTH != 0 {
        fields.ordinal_month = Some(date.month().ordinal);
    }
    if flags & DAY != 0 {
        fields.day = Some(date.day_of_month().0);
    }

    if flags & MAKE_INCONSISTENT_YEAR != 0
        && let Some(ref mut y) = fields.extended_year
    {
        *y += 1;
    }

    if flags & MAKE_INCONSISTENT_MONTH != 0
        && let Some(ref mut m) = fields.ordinal_month
    {
        *m = if *m == 1 { 2 } else { 1 };
    }

    Date::try_from_fields(fields, options, cal)
}

super::test_all_cals!(
    fn check_constructor_roundtrip<C: Calendar + Copy>(cal: C) {
        let rds = crate::tests::get_interesting_rds();

        for rd in rds {
            let date = Date::from_rata_die(rd, cal);

            for overflow in [Overflow::Reject, Overflow::Constrain] {
                let options = DateFromFieldsOptions {
                    overflow: Some(overflow),
                    ..Default::default()
                };

                // Test 1: extended_year + month + day
                let result = try_from_fields_helper(EXTENDED_YEAR | MONTH | DAY, rd, options, cal);
                assert_eq!(
                    result.unwrap().to_rata_die(),
                    rd,
                    "Failed roundtrip for extended_year + month + day at RD {:?}",
                    rd
                );

                // Test 2: extended_year + month_code + day
                let result =
                    try_from_fields_helper(EXTENDED_YEAR | MONTH_CODE | DAY, rd, options, cal);
                assert_eq!(
                    result.unwrap().to_rata_die(),
                    rd,
                    "Failed roundtrip for extended_year + month_code + day at RD {:?}",
                    rd
                );

                // Test 3: extended_year + ordinal_month + day
                let result =
                    try_from_fields_helper(EXTENDED_YEAR | ORDINAL_MONTH | DAY, rd, options, cal);
                assert_eq!(
                    result.unwrap().to_rata_die(),
                    rd,
                    "Failed roundtrip for extended_year + ordinal_month + day at RD {:?}",
                    rd
                );

                // Test 4: era + era_year + month + day (if era exists)
                if date.year().era().is_some() {
                    let result =
                        try_from_fields_helper(ERA | ERA_YEAR | MONTH | DAY, rd, options, cal);
                    assert_eq!(
                        result.unwrap().to_rata_die(),
                        rd,
                        "Failed roundtrip for era + era_year + month + day at RD {:?}",
                        rd
                    );
                }

                // Test 5: Both extended_year and era+era_year
                if date.year().era().is_some() {
                    let result = try_from_fields_helper(
                        EXTENDED_YEAR | ERA | ERA_YEAR | MONTH | DAY,
                        rd,
                        options,
                        cal,
                    );
                    assert_eq!(
                        result.unwrap().to_rata_die(),
                        rd,
                        "Failed roundtrip for both year fields at RD {:?}",
                        rd
                    );
                }

                // Test 6: Date::try_new
                let extended_year = date.year().extended_year();
                let month = date.month().to_input();
                let day = date.day_of_month().0;
                let reconstructed = Date::try_new(extended_year.into(), month, day, cal).unwrap();
                assert_eq!(
                    reconstructed.to_rata_die(),
                    rd,
                    "Failed roundtrip for Date::try_new at RD {:?}",
                    rd
                );

                // Negative Test 1: Inconsistent year
                if date.year().era().is_some() {
                    let result = try_from_fields_helper(
                        EXTENDED_YEAR | ERA | ERA_YEAR | MONTH | DAY | MAKE_INCONSISTENT_YEAR,
                        rd,
                        options,
                        cal,
                    );
                    assert_eq!(
                        result.map(|d| d.to_rata_die()),
                        Err(DateFromFieldsError::InconsistentYear),
                        "Expected InconsistentYear for RD {:?}",
                        rd
                    );
                }

                // Negative Test 2: Inconsistent month
                if date.months_in_year() >= 2 {
                    let result = try_from_fields_helper(
                        EXTENDED_YEAR | MONTH | ORDINAL_MONTH | DAY | MAKE_INCONSISTENT_MONTH,
                        rd,
                        options,
                        cal,
                    );
                    assert_eq!(
                        result.map(|d| d.to_rata_die()),
                        Err(DateFromFieldsError::InconsistentMonth),
                        "Expected InconsistentMonth for RD {:?}",
                        rd
                    );
                }

                // Test insufficient fields: Missing Day
                {
                    let result = try_from_fields_helper(EXTENDED_YEAR | MONTH, rd, options, cal);
                    assert!(
                        result.is_err(),
                        "Expected error for missing day at RD {:?}",
                        rd
                    );
                }

                // Test insufficient fields: Missing Year
                {
                    let result = try_from_fields_helper(MONTH | DAY, rd, options, cal);
                    assert!(
                        result.is_err(),
                        "Expected error for missing year at RD {:?}",
                        rd
                    );
                }
            }
        }
    }
);

// Note: Specific calendar constructors (like Date::try_new_iso, Date::try_new_gregorian, etc.)
// are tested individually in their respective files under src/cal/ to ensure correct coverage
// of their diverse signatures.
