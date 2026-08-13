// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::calendar_arithmetic::{CONSTRUCTOR_YEAR_RANGE, VALID_RD_RANGE};
use crate::*;

// Check rd -> date -> iso -> date -> rd for whole range
super::test_all_cals!(
    #[ignore] // takes about 200 seconds in release-with-assertions
    fn check_round_trip<C: Calendar + Copy>(cal: C) {
        let low = *VALID_RD_RANGE.start();
        let high = *VALID_RD_RANGE.end();
        let mut prev = Date::from_rata_die(low, cal);
        let mut curr = low + 1;
        while curr <= high {
            let date = Date::from_rata_die(curr, cal);
            assert!(prev < date);

            assert_eq!(
                date.to_rata_die(),
                curr,
                "{}",
                cal.as_calendar().debug_name()
            );

            prev = date;
            curr += 1;
        }
    }
);

super::test_all_cals!(
    #[ignore] // takes about 90 seconds in release-with-assertions
    fn check_from_fields<C: Calendar + Copy>(cal: C) {
        let months = (1..19)
            .flat_map(|i| [types::Month::new(i), types::Month::leap(i)].into_iter())
            .collect::<Vec<_>>();
        for year in CONSTRUCTOR_YEAR_RANGE {
            if year % 50000 == 0 {
                println!("{} {year:?}", cal.as_calendar().debug_name());
            }
            for overflow in [options::Overflow::Constrain, options::Overflow::Reject] {
                #![allow(clippy::field_reassign_with_default)] // use public API
                let mut options = options::DateFromFieldsOptions::default();
                options.overflow = Some(overflow);
                for mut fields in months
                    .iter()
                    .map(|&m| {
                        let mut fields = types::DateFields::default();
                        fields.month = Some(m);
                        fields
                    })
                    .chain((1..20).map(|m| {
                        let mut fields = types::DateFields::default();
                        fields.ordinal_month = Some(m);
                        fields
                    }))
                {
                    for day in 1..50 {
                        fields.extended_year = Some(year);
                        fields.day = Some(day);
                        let _ = Date::try_from_fields(fields, options, cal);
                    }
                }
            }
        }
    }
);
