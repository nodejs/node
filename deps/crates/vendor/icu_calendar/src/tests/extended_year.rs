// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::preferences::CalendarAlgorithm;
use crate::types::Month;
use crate::Calendar;
use crate::Date;

/// Reference: <https://tc39.es/proposal-intl-era-monthcode/#sec-temporal-calendardatearithmeticyear>
fn extended_epoch<C: Calendar>(c: &C) -> Option<i32> {
    Some(match c.calendar_algorithm()?.as_str() {
        "buddhist" => -543,
        "chinese" => 0,
        "coptic" => 283,
        "dangi" => 0,
        "ethioaa" => -5493,
        "ethiopic" => 7,
        "gregory" => 0,
        "hebrew" => -3761,
        "hijri-civil" => 621,
        "hijri-tbla" => 621,
        "hijri-umalqura" => 621,
        "indian" => 78,
        "japanese" => 0,
        "persian" => 621,
        "roc" => 1911,
        _ => return None,
    })
}

super::test_all_cals!(
    fn extended_year<C: Calendar + Copy>(cal: C) {
        let Some(extended_epoch) = extended_epoch(&cal) else {
            return;
        };

        // Create the first date in the epoch year (extended_year = 0)
        let date_in_epoch_year = Date::try_new(0.into(), Month::new(1), 1, cal).unwrap();
        let iso_date_in_epoch_year = date_in_epoch_year.to_calendar(crate::cal::Iso);
        assert_eq!(
            iso_date_in_epoch_year.year().extended_year(),
            extended_epoch,
            "Extended year for {date_in_epoch_year:?} should be {extended_epoch}"
        );

        // Test that for all calendars except Japanese, the *current* era is
        // the same as that used for the extended year
        // This date can be anything as long as it is vaguely modern.
        //
        // Note that this property is not *required* by the specification, new
        // calendars may have epochs that do not follow this property. This
        // property is strongly suggested by the specification as a rule of thumb
        // to follow where possible.
        let iso_date_in_2025 = Date::try_new_iso(2025, 1, 1).unwrap();
        let date_in_2025 = iso_date_in_2025.to_calendar(cal);

        // The extended year should align with the year in the modern era or related ISO.
        // There is a special case for Japanese since it has a modern era but uses ISO for the extended year.
        if matches!(cal.calendar_algorithm(), Some(CalendarAlgorithm::Japanese)) {
            assert_eq!(
                date_in_2025.year().extended_year(),
                2025,
                "Extended year for {date_in_2025:?} should be 2025"
            );
        } else {
            // Note: This code only works because 2025 is in the modern era for all calendars.
            // These two function calls are not equivalent in general.
            let expected = date_in_2025.year().era_year_or_related_iso();
            assert_eq!(
                date_in_2025.year().extended_year(),
                expected,
                "Extended year for {date_in_2025:?} should be {expected}"
            );
        }
    }
);
