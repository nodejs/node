// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::options::{DateAddOptions, DateDifferenceOptions, DateDurationUnit};
use crate::types::{DateDuration, RataDie};
use crate::Date;
use insta::assert_snapshot;
use std::fmt::Write;

const UNITS: &[DateDurationUnit] = &[
    DateDurationUnit::Years,
    DateDurationUnit::Months,
    DateDurationUnit::Weeks,
    DateDurationUnit::Days,
];

super::test_all_cals!(
    fn date_add_snapshot<C: Calendar + Copy>(cal: C) {
        let iso_dates = vec![(2023, 1, 1), (2024, 6, 15), (2025, 12, 31)];

        let durations = vec![
            DateDuration {
                years: 0,
                months: 0,
                weeks: 0,
                days: 10,
                is_negative: false,
            },
            DateDuration {
                years: 0,
                months: 2,
                weeks: 0,
                days: 0,
                is_negative: false,
            },
            DateDuration {
                years: 1,
                months: 0,
                weeks: 0,
                days: 0,
                is_negative: false,
            },
            DateDuration {
                years: 1,
                months: 3,
                weeks: 0,
                days: 15,
                is_negative: false,
            },
        ];

        let opts = DateAddOptions::default();
        let mut output = String::new();

        for (y, m, d) in &iso_dates {
            writeln!(&mut output, "{}-{}-{}", y, m, d).unwrap();

            let start_date = Date::try_new_iso(*y, *m, *d)
                .expect("Valid ISO date")
                .to_calendar(cal);

            writeln!(&mut output, "  (start: {:?})", start_date).unwrap();

            for duration in &durations {
                let mut date = start_date;

                date.try_add_with_options(*duration, opts)
                    .expect("Addition should succeed");

                // output logic
                let duration_str = format!(
                    "{}{}{}{}",
                    if duration.years != 0 {
                        format!("{}y ", duration.years)
                    } else {
                        "".into()
                    },
                    if duration.months != 0 {
                        format!("{}m ", duration.months)
                    } else {
                        "".into()
                    },
                    if duration.weeks != 0 {
                        format!("{}w ", duration.weeks)
                    } else {
                        "".into()
                    },
                    if duration.days != 0 {
                        format!("{}d", duration.days)
                    } else {
                        "".into()
                    },
                )
                .trim()
                .to_string();

                writeln!(&mut output, "    +{} → {:?}", duration_str, date).unwrap();
            }

            writeln!(&mut output).unwrap();
        }

        assert_snapshot!(format!("date_add_snapshot_{}", cal.debug_name()), output);
    }
);

super::test_all_cals!(
    fn test_date_until_snapshot<C: Calendar + Copy>(cal: C) {
        let mut output = String::new();
        let rds = [
            -1000, 0, 10000, 25145, 25317, 25340, 25344, 25345, 25355, 25356, 25700,
        ]
        .map(RataDie::new);
        for date1 in rds {
            for date2 in rds {
                writeln!(&mut output, "{date2:?} - {date1:?}").unwrap();
                let date1 = Date::from_rata_die(date1, cal);
                let date2 = Date::from_rata_die(date2, cal);
                writeln!(&mut output, " {date2:?} - {date1:?}").unwrap();
                for unit in UNITS {
                    let options = DateDifferenceOptions {
                        largest_unit: Some(*unit),
                    };
                    let until = date1.try_until_with_options(&date2, options).unwrap();
                    writeln!(&mut output, "  {unit:?}: {until:?}").unwrap();
                }
            }
        }

        assert_snapshot!(format!("date_until_snapshot_{}", cal.debug_name()), output);
    }
);
